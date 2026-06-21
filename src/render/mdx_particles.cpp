#include "mdx_particles.h"
#include "shader.h"
#include "resource/mpq_storm.h"
#include "renderer.h"
#include "asset/BLP.h"
#include "asset/DDS.h"
#include "asset/stb_image.h"
#include "replaceable_ids.h"
#include <iostream>
#include <cmath>
#include <cstring>
#include <cstdlib>

sg_shader MdxParticleSystem::sharedParticleShader_ = {0};
sg_pipeline MdxParticleSystem::sharedParticlePipeline_ = {0};
sg_buffer MdxParticleSystem::sharedQuadBuffer_ = {0};
sg_buffer MdxParticleSystem::sharedIndexBuffer_ = {0};

static sg_image LoadTex(MpqManager* mpq, const std::string& path) {
    if (!mpq || path.empty()) return {0};
    std::vector<uint8_t> data;
    if (!mpq->ReadFile(path, data)) return {0};

    auto makeImg = [](int w, int h, const void* pixels, size_t size) -> sg_image {
        sg_image_desc d = {};
        d.type = SG_IMAGETYPE_2D; d.width = w; d.height = h;
        d.pixel_format = SG_PIXELFORMAT_RGBA8;
        d.num_mipmaps = 0;
        d.data.mip_levels[0].ptr = pixels;
        d.data.mip_levels[0].size = size;
        sg_image tex = sg_make_image(&d);
        if (sg_query_image_state(tex) != SG_RESOURCESTATE_VALID) tex = {0};
        return tex;
    };

    BLPImage img;
    if (DecodeBLP(data.data(), data.size(), img))
        return makeImg(img.width, img.height, img.pixels.data(), img.pixels.size());

    DdsImage dds;
    if (DecodeDDS(data.data(), data.size(), dds))
        return makeImg(dds.width, dds.height, dds.pixels.data(), (size_t)dds.width * dds.height * 4);

    int w, h, ch;
    stbi_uc* pixels = stbi_load_from_memory(data.data(), (int)data.size(), &w, &h, &ch, STBI_rgb_alpha);
    if (!pixels) return {0};
    sg_image tex = makeImg(w, h, pixels, (size_t)(w * h * 4));
    stbi_image_free(pixels);
    return tex;
}

MdxParticleSystem::MdxParticleSystem()
    : rng_(std::random_device{}()), dist01_(0.0f, 1.0f) {}

MdxParticleSystem::~MdxParticleSystem() { Destroy(); }

void MdxParticleSystem::Destroy() {
    if (!Renderer::IsActive()) { emitters_.clear(); return; }
    for (auto& e : emitters_) {
        if (e.instanceBuffer.id) sg_destroy_buffer(e.instanceBuffer);
        if (e.textureView.id) sg_destroy_view(e.textureView);
        if (e.sampler.id) sg_destroy_sampler(e.sampler);
        if (e.texture.id) sg_destroy_image(e.texture);
    }
    emitters_.clear();
}

void MdxParticleSystem::Init(const MdxModel& model, MpqManager* mpq) {
    Destroy();
    if (model.particleEmitters2.empty()) return;

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
}

Vec3 MdxParticleSystem::RandomDirectionInCone(float latitude, std::mt19937& rng,
                                               std::uniform_real_distribution<float>& dist01) {
    float theta = dist01(rng) * 2.0f * 3.14159265f;
    float phi = dist01(rng) * latitude;
    float sp = std::sin(phi);
    // In Y-up: Y is up, X and Z are horizontal
    return Vec3{std::cos(theta) * sp, std::cos(phi), std::sin(theta) * sp};
}

void MdxParticleSystem::EmitParticle(ParticleEmitterState& state, float dt) {
    const auto& data = *state.emitterData;
    state.emissionAccum += data.emissionRate * dt;
    int count = (int)state.emissionAccum;
    state.emissionAccum -= (float)count;

    for (int i = 0; i < count; i++) {
        if ((int)state.particles.size() >= state.instanceCapacity) break;

        Particle p;

        // Randomize emission position within emitter width/length
        float halfW = data.width * 0.5f;
        float halfL = data.length * 0.5f;
        p.position = Vec3{
            (dist01_(rng_) - 0.5f) * 2.0f * halfW,
            0.0f,
            (dist01_(rng_) - 0.5f) * 2.0f * halfL
        };

        // Velocity: random direction in cone
        Vec3 dir = RandomDirectionInCone(data.latitude * 3.14159265f / 180.0f, rng_, dist01_);

        float speedVar = data.speed * (1.0f + (dist01_(rng_) - 0.5f) * 2.0f * data.variation);
        p.velocity = Vec3{dir.x * speedVar, dir.y * speedVar, dir.z * speedVar};
        p.velocity.y -= data.gravity * 0.5f * dt;

        p.life = data.lifeSpan;
        p.maxLife = data.lifeSpan;
        p.size = data.segmentScaling[0];

        // Random texture cell
        float col = (float)((int)(dist01_(rng_) * data.columns));
        float row = (float)((int)(dist01_(rng_) * data.rows));
        p.texRow = row;
        p.texCol = col;

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
    p.velocity.y -= data.gravity * dt;
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
}

void MdxParticleSystem::Update(float dt, const Vec3& worldOffset) {
    worldOffset_ = worldOffset;
    for (auto& state : emitters_) {
        auto& particles = state.particles;
        const auto& data = *state.emitterData;

        size_t oldCount = particles.size();
        EmitParticle(state, dt);

        // Apply world offset to newly emitted particles
        for (size_t i = oldCount; i < particles.size(); i++) {
            particles[i].position.x += worldOffset_.x;
            particles[i].position.y += worldOffset_.y;
            particles[i].position.z += worldOffset_.z;
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
                    p.texRow, p.texCol
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

        sg_apply_pipeline(sharedParticlePipeline_);
        sg_apply_uniforms(0, sg_rng(vsU));


        sg_bindings bind = {};
        bind.vertex_buffers[0] = sharedQuadBuffer_;
        bind.vertex_buffers[1] = state.instanceBuffer;
        bind.index_buffer = sharedIndexBuffer_;
        bind.views[0] = state.textureView;
        bind.samplers[0] = state.sampler;
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
                vUV = vec2(aTexCoord.x + (aQuad.x + 0.5) * uTexCellW,
                           aTexCoord.y + (aQuad.y + 0.5) * uTexCellH);
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
        // But std140 alignment: vec3 takes 12 bytes in vec4-aligned block
        // Actually in sokol with SG_UNIFORMLAYOUT_NATIVE, it packs sequentially
        // 64 + 3*4 + 3*4 + 4 + 4 = 92 bytes. Round to 96 for alignment.
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

    // ---- Particle pipeline ----
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
    }
}

void MdxParticleSystem::ReleaseSharedParticleResources() {
    if (sharedQuadBuffer_.id) { sg_destroy_buffer(sharedQuadBuffer_); sharedQuadBuffer_ = {0}; }
    if (sharedIndexBuffer_.id) { sg_destroy_buffer(sharedIndexBuffer_); sharedIndexBuffer_ = {0}; }
    if (sharedParticlePipeline_.id) { sg_destroy_pipeline(sharedParticlePipeline_); sharedParticlePipeline_ = {0}; }
    if (sharedParticleShader_.id) { sg_destroy_shader(sharedParticleShader_); sharedParticleShader_ = {0}; }
}
