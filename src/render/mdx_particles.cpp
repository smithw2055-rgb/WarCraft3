#include "mdx_particles.h"
#include "mdx_animator.h"
#include "shader.h"
#include "resource/mpq_storm.h"
#include "renderer.h"
#include "asset/BLP.h"
#include "asset/DDS.h"
#include "asset/MipChain.h"
#include "asset/stb_image.h"
#include "replaceable_ids.h"
#include <iostream>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <algorithm>

sg_shader MdxParticleSystem::sharedParticleShader_ = {0};
sg_pipeline MdxParticleSystem::sharedParticlePipeline_ = {0};
sg_pipeline MdxParticleSystem::sharedAdditivePipeline_ = {0};
sg_pipeline MdxParticleSystem::sharedModulatePipeline_ = {0};
sg_buffer MdxParticleSystem::sharedQuadBuffer_ = {0};
sg_buffer MdxParticleSystem::sharedIndexBuffer_ = {0};

static sg_image LoadTex(MpqManager* mpq, const std::string& path) {
    if (!mpq || path.empty()) return {0};
    std::vector<uint8_t> data;
    if (!mpq->ReadFile(path, data)) return {0};

    // Full mip chain: particle samplers are mipmapped; a bare level 0 is
    // an incomplete texture (black on strict drivers).
    auto makeChained = [](int w, int h, const void* pixels,
                          std::vector<MipLevel> embedded = {}) -> sg_image {
        MipChain chain = BuildMipChain(w, h, (const uint8_t*)pixels, std::move(embedded));
        return MakeChainedImage(chain);
    };
    auto toLevels = [](auto& decoded) {
        std::vector<MipLevel> levels;
        for (size_t i = 1; i < decoded.size(); ++i) {
            MipLevel level;
            level.width = decoded[i].width;
            level.height = decoded[i].height;
            level.pixels = decoded[i].pixels;
            levels.push_back(std::move(level));
        }
        return levels;
    };

    std::vector<BLPImage> blpLevels;
    if (DecodeBLPLevels(data.data(), data.size(), blpLevels) && !blpLevels.empty())
        return makeChained(blpLevels[0].width, blpLevels[0].height,
                           blpLevels[0].pixels.data(), toLevels(blpLevels));

    std::vector<DdsImage> ddsLevels;
    if (DecodeDDSLevels(data.data(), data.size(), ddsLevels) && !ddsLevels.empty())
        return makeChained(ddsLevels[0].width, ddsLevels[0].height,
                           ddsLevels[0].pixels.data(), toLevels(ddsLevels));

    int w, h, ch;
    stbi_uc* pixels = stbi_load_from_memory(data.data(), (int)data.size(), &w, &h, &ch, STBI_rgb_alpha);
    if (!pixels) return {0};
    sg_image tex = makeChained(w, h, pixels);
    stbi_image_free(pixels);
    return tex;
}

static Vec3 TransformPoint(const Mat4& matrix, const Vec3& point) {
    return Vec3{
        matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
        matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
        matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14]
    };
}

static Vec3 TransformDirection(const Mat4& matrix, const Vec3& dir) {
    return Vec3{
        matrix.m[0] * dir.x + matrix.m[4] * dir.y + matrix.m[8] * dir.z,
        matrix.m[1] * dir.x + matrix.m[5] * dir.y + matrix.m[9] * dir.z,
        matrix.m[2] * dir.x + matrix.m[6] * dir.y + matrix.m[10] * dir.z
    };
}

MdxParticleSystem::MdxParticleSystem()
    : rng_(std::random_device{}()), dist01_(0.0f, 1.0f) {}

MdxParticleSystem::~MdxParticleSystem() { Destroy(); }

void MdxParticleSystem::Destroy() {
    if (!Renderer::IsActive()) { emitters_.clear(); popcornOwned_.clear(); return; }
    for (auto& e : emitters_) {
        if (e.instanceBuffer.id) sg_destroy_buffer(e.instanceBuffer);
        if (e.textureView.id) sg_destroy_view(e.textureView);
        if (e.sampler.id) sg_destroy_sampler(e.sampler);
        if (e.texture.id) sg_destroy_image(e.texture);
    }
    emitters_.clear();
    popcornOwned_.clear();
}

void MdxParticleSystem::Init(const MdxModel& model, MpqManager* mpq) {
    Destroy();
    popcornOwned_.clear();
    if (model.particleEmitters2.empty() && model.particleEmittersPopcorn.empty()) return;

    InitSharedParticleResources();
    sg_sampler_desc smp = {};
    smp.min_filter = SG_FILTER_LINEAR;
    smp.mag_filter = SG_FILTER_LINEAR;
    smp.mipmap_filter = SG_FILTER_LINEAR;
    smp.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smp.wrap_v = SG_WRAP_CLAMP_TO_EDGE;

    for (auto& pe : model.particleEmitters2) {
        ParticleEmitterState state;
        state.emitterData = &pe;

        // Load texture
        sg_image texHandle = {0};
        int replaceableId = 0;
        std::string replaceablePath;
        if (pe.textureId >= 0 && pe.textureId < (int)model.textures.size()) {
            auto& texRef = model.textures[pe.textureId];
            if (!texRef.path.empty()) {
                std::string path = texRef.path;
                for (auto& c : path) if (c == '/') c = '\\';
                texHandle = LoadTex(mpq, path);
            } else if (texRef.replaceableId != 0) {
                replaceableId = texRef.replaceableId;
                const char* rp = ReplaceableIds::GetPathString(texRef.replaceableId);
                if (rp && rp[0]) {
                    replaceablePath = rp;
                    std::string fp = "ReplaceableTextures\\" + std::string(rp) + ".blp";
                    texHandle = LoadTex(mpq, fp);
                }
            }
        }
        state.texture = texHandle;
        state.replaceableId = replaceableId;
        state.replaceablePath = replaceablePath;
        if (texHandle.id) {
            sg_view_desc vd = {}; vd.texture.image = texHandle;
            state.textureView = sg_make_view(&vd);
            state.sampler = sg_make_sampler(&smp);
        }

        // Allocate instance buffer
        int maxParticles = 1024;
        state.instanceCapacity = maxParticles;
        sg_buffer_desc ibd = {};
        ibd.usage.vertex_buffer = true;
        ibd.usage.stream_update = true;
        ibd.size = (size_t)maxParticles * sizeof(InstanceData);
        state.instanceBuffer = sg_make_buffer(&ibd);

        emitters_.push_back(state);
    }

    // Popcorn (CORN) fallback: map to billboard Emitter2 states so the
    // effects actually render instead of being parsed-but-invisible.
    for (auto& pop : model.particleEmittersPopcorn) {
        auto synth = std::make_unique<MdxParticleEmitter2>();
        synth->name = pop.name;
        synth->objectId = pop.objectId;
        synth->parentId = pop.parentId;
        synth->flags = (int)pop.flags;
        synth->speed = pop.speed;
        synth->variation = 0.0f;
        synth->latitude = 45.0f;
        synth->gravity = 0.0f;
        synth->lifeSpan = pop.lifeSpan > 0.0f ? pop.lifeSpan : 1.0f;
        synth->emissionRate = pop.emissionRate;
        synth->length = 16.0f;
        synth->width = 16.0f;
        synth->rows = 1;
        synth->columns = 1;
        synth->timeMiddle = 0.5f;
        for (int i = 0; i < 3; i++) {
            synth->segmentColors[i][0] = pop.color[0];
            synth->segmentColors[i][1] = pop.color[1];
            synth->segmentColors[i][2] = pop.color[2];
            synth->segmentScaling[i] = 24.0f;
        }
        const unsigned char a = (unsigned char)std::clamp((int)(pop.alpha * 255.0f), 0, 255);
        synth->segmentAlphas[0] = synth->segmentAlphas[1] = synth->segmentAlphas[2] = a ? a : 255;
        synth->textureId = -1;
        synth->timelines = pop.timelines;

        ParticleEmitterState state;
        state.emitterData = synth.get();
        sg_image texHandle = {0};
        if (!pop.path.empty()) {
            std::string path = pop.path;
            for (auto& c : path) if (c == '/') c = '\\';
            texHandle = LoadTex(mpq, path);
        } else if (pop.replaceableId != 0) {
            const char* rp = ReplaceableIds::GetPathString(pop.replaceableId);
            if (rp && rp[0]) {
                std::string fp = "ReplaceableTextures\\" + std::string(rp) + ".blp";
                texHandle = LoadTex(mpq, fp);
            }
            state.replaceableId = pop.replaceableId;
        }
        state.texture = texHandle;
        if (texHandle.id) {
            sg_view_desc vd = {}; vd.texture.image = texHandle;
            state.textureView = sg_make_view(&vd);
            state.sampler = sg_make_sampler(&smp);
        }
        const int maxParticles = 512;
        state.instanceCapacity = maxParticles;
        sg_buffer_desc ibd = {};
        ibd.usage.vertex_buffer = true;
        ibd.usage.stream_update = true;
        ibd.size = (size_t)maxParticles * sizeof(InstanceData);
        state.instanceBuffer = sg_make_buffer(&ibd);

        popcornOwned_.push_back(std::move(synth));
        // Re-point after push (deque-stable via unique_ptr heap object).
        emitters_.push_back(state);
        emitters_.back().emitterData = popcornOwned_.back().get();
    }
}

Vec3 MdxParticleSystem::RandomDirectionInCone(float latitude, std::mt19937& rng,
                                               std::uniform_real_distribution<float>& dist01) {
    float theta = dist01(rng) * 2.0f * 3.14159265f;
    float phi = dist01(rng) * latitude;
    float sp = std::sin(phi);
    // In Y-up: Y is up, X and Z are horizontal
    return Vec3{std::cos(theta) * sp, std::cos(phi), std::sin(theta) * sp};
}

void MdxParticleSystem::EmitParticle(ParticleEmitterState& state, const MdxParticleEmitter2Sample& sample, int count) {
    const auto& data = *state.emitterData;
    if (count <= 0) {
        return;
    }

    for (int i = 0; i < count; i++) {
        if ((int)state.particles.size() >= state.instanceCapacity) break;

        Particle p;

        // Randomize emission position within emitter width/length
        float halfW = sample.width * 0.5f;
        float halfL = sample.length * 0.5f;
        p.position = Vec3{
            (dist01_(rng_) - 0.5f) * 2.0f * halfW,
            0.0f,
            (dist01_(rng_) - 0.5f) * 2.0f * halfL
        };

        // Velocity: random direction in cone
        Vec3 dir = RandomDirectionInCone(sample.latitude * 3.14159265f / 180.0f, rng_, dist01_);

        float speedVar = sample.speed * (1.0f + (dist01_(rng_) - 0.5f) * 2.0f * sample.variation);
        p.velocity = Vec3{dir.x * speedVar, dir.y * speedVar, dir.z * speedVar};
        p.gravity = sample.gravity;

        p.life = data.lifeSpan;
        p.maxLife = data.lifeSpan;
        p.size = data.segmentScaling[0];

        // Starting texture cell (start of interval or 0)
        const unsigned columns = std::max(1u, data.columns);
        const unsigned rows = std::max(1u, data.rows);
        const unsigned totalCells = rows * columns;
        if (totalCells > 1) {
            unsigned startFrame = (data.headIntervals[0][1] > data.headIntervals[0][0])
                ? data.headIntervals[0][0] : 0u;
            p.texCol = (float)(startFrame % columns);
            p.texRow = (float)(startFrame / columns);
        } else {
            p.texCol = 0.0f;
            p.texRow = 0.0f;
        }

        // Starting color
        p.color = Vec4{data.segmentColors[0][0], data.segmentColors[0][1],
                       data.segmentColors[0][2], (float)data.segmentAlphas[0] / 255.0f};

        state.particles.push_back(p);
    }
}

void MdxParticleSystem::UpdateParticle(Particle& p, float dt, const MdxParticleEmitter2& data) {
    p.life -= dt;
    if (p.life <= 0.0f) return;

    // Apply gravity
    p.velocity.y -= p.gravity * dt;
    // Update position
    p.position.x += p.velocity.x * dt;
    p.position.y += p.velocity.y * dt;
    p.position.z += p.velocity.z * dt;

    // Interpolate color, alpha, size based on normalized age
    float t = 1.0f - p.life / p.maxLife; // 0 at birth, 1 at death
    float mid = data.timeMiddle; // usually 0.5ish
    float t0, t1;
    int idx0, idx1;
    if (t <= mid) {
        if (mid <= 0) { idx0 = 0; idx1 = 2; t0 = 0; t1 = 1; }
        else { idx0 = 0; idx1 = 1; t0 = 0; t1 = mid; }
    } else {
        idx0 = 1; idx1 = 2; t0 = mid; t1 = 1.0f;
    }
    float localT = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0f;

    // Color
    p.color.x = data.segmentColors[idx0][0] + (data.segmentColors[idx1][0] - data.segmentColors[idx0][0]) * localT;
    p.color.y = data.segmentColors[idx0][1] + (data.segmentColors[idx1][1] - data.segmentColors[idx0][1]) * localT;
    p.color.z = data.segmentColors[idx0][2] + (data.segmentColors[idx1][2] - data.segmentColors[idx0][2]) * localT;
    float a0 = (float)data.segmentAlphas[idx0] / 255.0f;
    float a1 = (float)data.segmentAlphas[idx1] / 255.0f;
    p.color.w = a0 + (a1 - a0) * localT;

    // Size
    p.size = data.segmentScaling[idx0] + (data.segmentScaling[idx1] - data.segmentScaling[idx0]) * localT;

    // Advance flipbook frame based on normalized age
    const unsigned columns = std::max(1u, data.columns);
    const unsigned rows = std::max(1u, data.rows);
    const unsigned totalCells = rows * columns;
    if (totalCells > 1) {
        unsigned startFrame = 0;
        unsigned endFrame = totalCells - 1;
        if (data.headIntervals[0][1] > data.headIntervals[0][0]) {
            startFrame = data.headIntervals[0][0];
            endFrame = std::min(data.headIntervals[0][1], totalCells - 1);
        }
        int span = (int)(endFrame - startFrame + 1);
        int frameIdx = (int)startFrame + std::clamp((int)(t * span), 0, span - 1);
        p.texCol = (float)(frameIdx % columns);
        p.texRow = (float)(frameIdx / columns);
    }
}

void MdxParticleSystem::Update(float dt, const Vec3& worldOffset) {
    Update(dt, worldOffset, nullptr, nullptr, 0, 0.0f);
}

void MdxParticleSystem::Update(float dt, const Vec3& worldOffset, const MdxModel* model,
                               const MdxAnimator* animator, uint32_t frame, float elapsedMs) {
    worldOffset_ = worldOffset;
    for (auto& state : emitters_) {
        auto& particles = state.particles;
        const auto& data = *state.emitterData;
        MdxParticleEmitter2Sample sample;
        MdxSampleParticleEmitter2(data, model, frame, elapsedMs, sample,
                                  animator ? animator->GetCurrentSequence() : -1);
        const bool emitterVisible = !animator || animator->IsObjectVisible(data.objectId);

        size_t oldCount = particles.size();
        if (emitterVisible && sample.visibility > 0.0f && sample.emissionRate > 0.0f) {
            if (data.squirt != 0) {
                if (sample.emissionRateKey >= 0 && sample.emissionRateKey != state.lastEmissionKey) {
                    state.emissionAccum += sample.emissionRate;
                }
                state.lastEmissionKey = sample.emissionRateKey;
            } else {
                state.emissionAccum += sample.emissionRate * dt;
            }
        }
        int emitCount = (int)state.emissionAccum;
        state.emissionAccum -= (float)emitCount;
        EmitParticle(state, sample, emitCount);

        Mat4 emitterTransform = Mat4::Identity();
        const bool hasEmitterTransform = animator
            && animator->GetObjectWorldTransformYUp(data.objectId, emitterTransform);
        const bool modelSpace = (data.flags & 0x80000) != 0;

        // Apply emitter node transform and instance transform to newly emitted particles.
        for (size_t i = oldCount; i < particles.size(); i++) {
            if (hasEmitterTransform && !modelSpace) {
                particles[i].position = TransformPoint(emitterTransform, particles[i].position);
                particles[i].velocity = TransformDirection(emitterTransform, particles[i].velocity);
            }
            if (hasInstanceMatrix_) {
                particles[i].position = TransformPoint(instanceMatrix_, particles[i].position);
                particles[i].velocity = TransformDirection(instanceMatrix_, particles[i].velocity);
            } else {
                particles[i].position.x += worldOffset_.x;
                particles[i].position.y += worldOffset_.y;
                particles[i].position.z += worldOffset_.z;
            }
        }

        // Update existing particles
        for (int i = (int)particles.size() - 1; i >= 0; i--) {
            UpdateParticle(particles[i], dt, data);
            if (particles[i].life <= 0.0f) {
                particles[i] = particles.back();
                particles.pop_back();
            }
        }

        // Update instance buffer
        if (!particles.empty() && state.instanceBuffer.id) {
            // Resize buffer if needed
            if ((int)particles.size() > state.instanceCapacity) {
                if (state.instanceBuffer.id) sg_destroy_buffer(state.instanceBuffer);
                state.instanceCapacity = (int)particles.size() + 256;
                sg_buffer_desc ibd = {};
                ibd.usage.vertex_buffer = true;
                ibd.usage.stream_update = true;
                ibd.size = (size_t)state.instanceCapacity * sizeof(InstanceData);
                state.instanceBuffer = sg_make_buffer(&ibd);
            }

            // Fill instance data
            std::vector<InstanceData> instanceData(particles.size());
            for (size_t i = 0; i < particles.size(); i++) {
                auto& p = particles[i];
                instanceData[i] = {
                    p.position.x, p.position.y, p.position.z,
                    p.size,
                    p.color.x, p.color.y, p.color.z, p.color.w,
                    p.texCol, p.texRow
                };
            }
            sg_update_buffer(state.instanceBuffer,
                             sg_range{instanceData.data(), particles.size() * sizeof(InstanceData)});
        }
    }
}

void MdxParticleSystem::Render(const Mat4& view, const Mat4& proj) {
    if (!sharedParticleShader_.id || !sharedParticlePipeline_.id) return;
    if (!sharedQuadBuffer_.id || !sharedIndexBuffer_.id) return;
    if (sg_query_buffer_state(sharedQuadBuffer_) != SG_RESOURCESTATE_VALID) return;
    if (sg_query_buffer_state(sharedIndexBuffer_) != SG_RESOURCESTATE_VALID) return;
    if (sg_query_pipeline_state(sharedParticlePipeline_) != SG_RESOURCESTATE_VALID) return;
    if (sg_query_shader_state(sharedParticleShader_) != SG_RESOURCESTATE_VALID) return;

    // Extract camera right/up from view matrix
    // view is column-major: cols[0]=right, cols[1]=up, cols[2]=forward
    Vec3 camRight = {view.m[0], view.m[1], view.m[2]};
    Vec3 camUp = {view.m[4], view.m[5], view.m[6]};

    Mat4 vp = proj * view;

    // Native layout: mat4(64) + vec4(16) + vec4(16) + float(4) + float(4) = 104
    struct VSBlock {
        float vp[16];
        float camRight[4];
        float camUp[4];
        float texCellW, texCellH;
    };

    for (auto& state : emitters_) {
        if (state.particles.empty()) continue;
        if (!state.textureView.id || !state.sampler.id || !state.instanceBuffer.id) continue;
        if (!state.emitterData || state.emitterData->columns <= 0 || state.emitterData->rows <= 0) continue;
        if (sg_query_buffer_state(state.instanceBuffer) != SG_RESOURCESTATE_VALID) continue;
        if (sg_query_view_state(state.textureView) != SG_RESOURCESTATE_VALID) continue;
        if (sg_query_sampler_state(state.sampler) != SG_RESOURCESTATE_VALID) continue;

        VSBlock vsU = {};
        std::memcpy(vsU.vp, vp.m, 64);
        vsU.camRight[0] = camRight.x; vsU.camRight[1] = camRight.y; vsU.camRight[2] = camRight.z;
        vsU.camUp[0] = camUp.x; vsU.camUp[1] = camUp.y; vsU.camUp[2] = camUp.z;
        const auto& data = *state.emitterData;
        vsU.texCellW = 1.0f / (float)data.columns;
        vsU.texCellH = 1.0f / (float)data.rows;

        sg_pipeline pipeline = sharedParticlePipeline_;
        if (state.emitterData) {
            if (state.emitterData->filterMode == 1) { // Additive (SRC_ALPHA, ONE)
                if (sharedAdditivePipeline_.id) pipeline = sharedAdditivePipeline_;
            } else if (state.emitterData->filterMode == 2) { // Modulate (DST_COLOR, ZERO)
                if (sharedModulatePipeline_.id) pipeline = sharedModulatePipeline_;
            }
        }
        sg_apply_pipeline(pipeline);
        sg_apply_uniforms(0, sg_rng(vsU));

        sg_bindings bind = {};
        bind.vertex_buffers[0] = sharedQuadBuffer_;
        bind.vertex_buffers[1] = state.instanceBuffer;
        bind.index_buffer = sharedIndexBuffer_;
        bind.views[0] = state.textureView;
        bind.samplers[0] = state.sampler;
        if (bind.views[0].id && ((bind.views[0].id & 0xFFFFu) >= 4096u)) {
            std::fprintf(stderr, "MdxParticleSystem suspicious view: 0x%08x\n", bind.views[0].id);
            continue;
        }
        sg_apply_bindings(&bind);

        sg_draw(0, 6, (int)state.particles.size());
    }
}

int MdxParticleSystem::GetParticleCount() const {
    int total = 0;
    for (auto& e : emitters_) total += (int)e.particles.size();
    return total;
}

// ---- Shared particle resources ----

sg_shader MdxParticleSystem::ParticleShader() { return sharedParticleShader_; }
sg_pipeline MdxParticleSystem::ParticlePipeline() { return sharedParticlePipeline_; }

void MdxParticleSystem::InitSharedParticleResources() {
    if (sharedParticleShader_.id) return;

    // ---- Particle shader ----
    {
        const char* vertSrc = R"(
            #version 410 core
            layout(location = 0) in vec2 aQuad;
            layout(location = 1) in vec3 aPos;
            layout(location = 2) in float aSize;
            layout(location = 3) in vec4 aColor;
            layout(location = 4) in vec2 aTexCoord;

            out vec4 vColor;
            out vec2 vUV;

            uniform mat4 uVP;
            uniform vec4 uCamRight;
            uniform vec4 uCamUp;
            uniform float uTexCellW;
            uniform float uTexCellH;

            void main() {
                vec3 worldPos = aPos + uCamRight.xyz * aQuad.x * aSize + uCamUp.xyz * aQuad.y * aSize;
                gl_Position = uVP * vec4(worldPos, 1.0);
                vColor = aColor;
                vUV = vec2((aTexCoord.x + aQuad.x + 0.5) * uTexCellW,
                           (aTexCoord.y + aQuad.y + 0.5) * uTexCellH);
            }
        )";
        const char* fragSrc = R"(
            #version 410 core
            in vec4 vColor;
            in vec2 vUV;
            out vec4 FragColor;
            uniform sampler2D uTexture;
            void main() {
                vec4 texel = texture(uTexture, vUV);
                FragColor = texel * vColor;
                if (FragColor.a < 0.01) discard;
            }
        )";

        sg_shader_desc sd = {};
        sd.vertex_func.source = vertSrc;
        sd.fragment_func.source = fragSrc;

        // VS uniform block: mat4 uVP (64) + vec3 uCamRight (12) + vec3 uCamUp (12) + float TexCellW (4) + float TexCellH (4) = 96
        sd.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
        sd.uniform_blocks[0].size = 104;
        sd.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_NATIVE;
        // uVP (mat4, offset 0, 64 bytes)
        sd.uniform_blocks[0].glsl_uniforms[0].type = SG_UNIFORMTYPE_MAT4;
        sd.uniform_blocks[0].glsl_uniforms[0].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[0].glsl_name = "uVP";
        // uCamRight (vec4, offset 64, 16 bytes)
        sd.uniform_blocks[0].glsl_uniforms[1].type = SG_UNIFORMTYPE_FLOAT4;
        sd.uniform_blocks[0].glsl_uniforms[1].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[1].glsl_name = "uCamRight";
        // uCamUp (vec4, offset 80, 16 bytes)
        sd.uniform_blocks[0].glsl_uniforms[2].type = SG_UNIFORMTYPE_FLOAT4;
        sd.uniform_blocks[0].glsl_uniforms[2].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[2].glsl_name = "uCamUp";
        // uTexCellW (float, offset 96, 4 bytes)
        sd.uniform_blocks[0].glsl_uniforms[3].type = SG_UNIFORMTYPE_FLOAT;
        sd.uniform_blocks[0].glsl_uniforms[3].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[3].glsl_name = "uTexCellW";
        // uTexCellH (float, offset 100, 4 bytes) + pad(8) = 112 aligned
        sd.uniform_blocks[0].glsl_uniforms[4].type = SG_UNIFORMTYPE_FLOAT;
        sd.uniform_blocks[0].glsl_uniforms[4].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[4].glsl_name = "uTexCellH";

        sd.views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT;
        sd.views[0].texture.image_type = SG_IMAGETYPE_2D;
        sd.views[0].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
        sd.samplers[0].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;
        sd.texture_sampler_pairs[0].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.texture_sampler_pairs[0].glsl_name = "uTexture";
        sd.texture_sampler_pairs[0].view_slot = 0;
        sd.texture_sampler_pairs[0].sampler_slot = 0;

        sharedParticleShader_ = sg_make_shader(&sd);
    }

    // ---- Quad geometry ----
    if (!sharedQuadBuffer_.id) {
        float quadVerts[] = {
            -0.5f, -0.5f,
             0.5f, -0.5f,
             0.5f,  0.5f,
            -0.5f,  0.5f,
        };
        sg_buffer_desc vbd = {};
        vbd.usage.vertex_buffer = true;
        vbd.usage.immutable = true;
        vbd.data.ptr = quadVerts;
        vbd.data.size = sizeof(quadVerts);
        sharedQuadBuffer_ = sg_make_buffer(&vbd);
    }

    if (!sharedIndexBuffer_.id) {
        uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };
        sg_buffer_desc ibd = {};
        ibd.usage.index_buffer = true;
        ibd.usage.immutable = true;
        ibd.data.ptr = indices;
        ibd.data.size = sizeof(indices);
        sharedIndexBuffer_ = sg_make_buffer(&ibd);
    }

    // ---- Particle pipelines ----
    if (!sharedParticlePipeline_.id && sharedParticleShader_.id) {
        sg_pipeline_desc pd = {};
        pd.shader = sharedParticleShader_;
        pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
        pd.index_type = SG_INDEXTYPE_UINT16;
        pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
        pd.depth.write_enabled = false;
        pd.cull_mode = SG_CULLMODE_NONE;
        pd.colors[0].blend.enabled = true;
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

        // Buffer 0: quad vertices (per-vertex)
        pd.layout.buffers[0].stride = 8; // 2 floats
        pd.layout.buffers[0].step_func = SG_VERTEXSTEP_PER_VERTEX;
        // Buffer 1: instance data (per-instance)
        pd.layout.buffers[1].stride = sizeof(InstanceData);
        pd.layout.buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE;

        pd.layout.attrs[0] = {0, 0, SG_VERTEXFORMAT_FLOAT2};           // aQuad (buffer 0)
        pd.layout.attrs[1] = {1, 0, SG_VERTEXFORMAT_FLOAT3};           // aPos (buffer 1)
        pd.layout.attrs[2] = {1, 12, SG_VERTEXFORMAT_FLOAT};           // aSize (buffer 1, offset 12)
        pd.layout.attrs[3] = {1, 16, SG_VERTEXFORMAT_FLOAT4};          // aColor (buffer 1, offset 16)
        pd.layout.attrs[4] = {1, 32, SG_VERTEXFORMAT_FLOAT2};          // aTexCoord (buffer 1, offset 32)

        sharedParticlePipeline_ = sg_make_pipeline(&pd);

        // Additive pipeline (SRC_ALPHA, ONE) - standard for WC3 fire/spells/auras
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
        sharedAdditivePipeline_ = sg_make_pipeline(&pd);

        // Modulate pipeline (DST_COLOR, ZERO)
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_DST_COLOR;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ZERO;
        sharedModulatePipeline_ = sg_make_pipeline(&pd);
    }
}

void MdxParticleSystem::ReleaseSharedParticleResources() {
    if (sharedQuadBuffer_.id) { sg_destroy_buffer(sharedQuadBuffer_); sharedQuadBuffer_ = {0}; }
    if (sharedIndexBuffer_.id) { sg_destroy_buffer(sharedIndexBuffer_); sharedIndexBuffer_ = {0}; }
    if (sharedModulatePipeline_.id) { sg_destroy_pipeline(sharedModulatePipeline_); sharedModulatePipeline_ = {0}; }
    if (sharedAdditivePipeline_.id) { sg_destroy_pipeline(sharedAdditivePipeline_); sharedAdditivePipeline_ = {0}; }
    if (sharedParticlePipeline_.id) { sg_destroy_pipeline(sharedParticlePipeline_); sharedParticlePipeline_ = {0}; }
    if (sharedParticleShader_.id) { sg_destroy_shader(sharedParticleShader_); sharedParticleShader_ = {0}; }
}
