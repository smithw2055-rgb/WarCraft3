#include "mdx_ribbons.h"
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
#include <algorithm>

namespace {

constexpr uint32_t RibbonTag(char a, char b, char c, char d) {
    return (uint32_t(uint8_t(a)) << 24) | (uint32_t(uint8_t(b)) << 16) |
           (uint32_t(uint8_t(c)) << 8) | uint32_t(uint8_t(d));
}

constexpr uint32_t TAG_KRHA = RibbonTag('K', 'R', 'H', 'A');
constexpr uint32_t TAG_KRHB = RibbonTag('K', 'R', 'H', 'B');
constexpr uint32_t TAG_KRAL = RibbonTag('K', 'R', 'A', 'L');
constexpr uint32_t TAG_KRCO = RibbonTag('K', 'R', 'C', 'O');
constexpr uint32_t TAG_KRTX = RibbonTag('K', 'R', 'T', 'X');
constexpr uint32_t TAG_KRVS = RibbonTag('K', 'R', 'V', 'S');

float FindTimelineValue(const MdxRibbonEmitter& emitter, const MdxModel* model,
                        uint32_t tag, uint32_t frame, float elapsedMs,
                        int elemCount, float def, int sequenceIndex) {
    for (const auto& tl : emitter.timelines) {
        if (tl.tag == tag) {
            float out[4] = {def, def, def, def};
            if (MdxSampleTimelineForModel(tl, model, frame, elapsedMs, out,
                                          elemCount, def, sequenceIndex)) {
                return out[0];
            }
            return def;
        }
    }
    return def;
}

Vec3 TransformPointYUp(const Mat4& matrix, const Vec3& point) {
    return Vec3{
        matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
        matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
        matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14]
    };
}

Vec3 TransformPoint(const Mat4& matrix, const Vec3& point) {
    return Vec3{
        matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
        matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
        matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14]
    };
}

sg_image LoadRibbonTex(MpqManager* mpq, const std::string& path) {
    if (!mpq || path.empty()) return {0};
    std::vector<uint8_t> data;
    if (!mpq->ReadFile(path, data)) return {0};

    // Full mip chain: ribbon samplers are mipmapped; a bare level 0 is an
    // incomplete texture (black on strict drivers).
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

} // namespace

sg_shader MdxRibbonSystem::sharedRibbonShader_ = {0};
sg_pipeline MdxRibbonSystem::sharedRibbonPipeline_ = {0};
sg_pipeline MdxRibbonSystem::sharedAdditivePipeline_ = {0};
sg_pipeline MdxRibbonSystem::sharedModulatePipeline_ = {0};

MdxRibbonSystem::MdxRibbonSystem() {}
MdxRibbonSystem::~MdxRibbonSystem() { Destroy(); }

void MdxRibbonSystem::Destroy() {
    if (!Renderer::IsActive()) {
        emitters_.clear();
        return;
    }
    for (auto& e : emitters_) {
        if (e.vertexBuffer.id) sg_destroy_buffer(e.vertexBuffer);
        if (e.indexBuffer.id) sg_destroy_buffer(e.indexBuffer);
        if (e.textureView.id) sg_destroy_view(e.textureView);
        if (e.sampler.id) sg_destroy_sampler(e.sampler);
        if (e.texture.id) sg_destroy_image(e.texture);
    }
    emitters_.clear();
}

void MdxRibbonSystem::Init(const MdxModel& model, MpqManager* mpq) {
    Destroy();
    const bool glActive = Renderer::IsActive();
    if (glActive) {
        InitSharedRibbonResources();
    }

    sg_sampler_desc smp = {};
    smp.min_filter = SG_FILTER_LINEAR;
    smp.mag_filter = SG_FILTER_LINEAR;
    smp.mipmap_filter = SG_FILTER_LINEAR;
    smp.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smp.wrap_v = SG_WRAP_CLAMP_TO_EDGE;

    for (auto& rb : model.ribbonEmitters) {
        RibbonEmitterState state;
        state.emitterData = &rb;
        if (rb.materialId >= 0 && rb.materialId < (int)model.materials.size()) {
            const auto& mat = model.materials[(size_t)rb.materialId];
            if (!mat.layers.empty()) {
                state.filterMode = mat.layers[0].filterMode;
            }
        }
        if (glActive && mpq && rb.textureSlot >= 0 && rb.textureSlot < (int)model.textures.size()) {
            auto& texRef = model.textures[(size_t)rb.textureSlot];
            sg_image texHandle = {0};
            if (!texRef.path.empty()) {
                std::string path = texRef.path;
                for (auto& c : path) if (c == '/') c = '\\';
                texHandle = LoadRibbonTex(mpq, path);
            } else if (texRef.replaceableId != 0) {
                const char* rp = ReplaceableIds::GetPathString(texRef.replaceableId);
                if (rp && rp[0]) {
                    texHandle = LoadRibbonTex(mpq, "ReplaceableTextures\\" + std::string(rp) + ".blp");
                }
            }
            state.texture = texHandle;
            if (texHandle.id) {
                sg_view_desc vd = {};
                vd.texture.image = texHandle;
                state.textureView = sg_make_view(&vd);
                state.sampler = sg_make_sampler(&smp);
            }
        }
        state.stagingVertices.reserve((size_t)kMaxSegmentsPerEmitter * 4);
        state.stagingIndices.reserve((size_t)kMaxSegmentsPerEmitter * 6);
        emitters_.push_back(std::move(state));
    }
}

void MdxRibbonSystem::SampleEmitter(const MdxRibbonEmitter& emitter, const MdxModel* model,
                                    uint32_t frame, float elapsedMs, int sequenceIndex,
                                    float& heightAbove, float& heightBelow,
                                    float color[3], float& alpha, int& slot, float& visibility) const {
    heightAbove = FindTimelineValue(emitter, model, TAG_KRHA, frame, elapsedMs, 1, emitter.heightAbove, sequenceIndex);
    heightBelow = FindTimelineValue(emitter, model, TAG_KRHB, frame, elapsedMs, 1, emitter.heightBelow, sequenceIndex);
    alpha = FindTimelineValue(emitter, model, TAG_KRAL, frame, elapsedMs, 1, emitter.alpha, sequenceIndex);
    for (int i = 0; i < 3; i++) color[i] = emitter.color[i];
    for (const auto& tl : emitter.timelines) {
        if (tl.tag == TAG_KRCO) {
            float out[4] = {color[0], color[1], color[2], 0};
            if (MdxSampleTimelineForModel(tl, model, frame, elapsedMs, out, 3, 0.0f, sequenceIndex)) {
                color[0] = out[0];
                color[1] = out[1];
                color[2] = out[2];
            }
            break;
        }
    }
    slot = (int)FindTimelineValue(emitter, model, TAG_KRTX, frame, elapsedMs, 1,
                                  (float)emitter.textureSlot, sequenceIndex);
    visibility = FindTimelineValue(emitter, model, TAG_KRVS, frame, elapsedMs, 1, 1.0f, sequenceIndex);
}

void MdxRibbonSystem::EmitSegment(RibbonEmitterState& state, const MdxModel* model,
                                  const MdxAnimator* animator, uint32_t frame, float elapsedMs,
                                  int sequenceIndex) {
    const MdxRibbonEmitter& data = *state.emitterData;
    float heightAbove, heightBelow, alpha, visibility;
    float color[3];
    int slot;
    SampleEmitter(data, model, frame, elapsedMs, sequenceIndex,
                  heightAbove, heightBelow, color, alpha, slot, visibility);

    Vec3 pivot = {0, 0, 0};
    Mat4 world = Mat4::Identity();
    if (animator) {
        if (!animator->GetObjectPivotAndWorldTransformYUp(data.objectId, pivot, world)) {
            return;
        }
    }
    // Local above/below along Y (Y-up node frame), then to world space.
    Vec3 aboveLocal = {pivot.x, pivot.y + heightAbove, pivot.z};
    Vec3 belowLocal = {pivot.x, pivot.y - heightBelow, pivot.z};
    Vec3 above = TransformPointYUp(world, aboveLocal);
    Vec3 below = TransformPointYUp(world, belowLocal);
    if (hasInstanceMatrix_) {
        above = TransformPoint(instanceMatrix_, above);
        below = TransformPoint(instanceMatrix_, below);
    } else {
        above.x += worldOffset_.x;
        above.y += worldOffset_.y;
        above.z += worldOffset_.z;
        below.x += worldOffset_.x;
        below.y += worldOffset_.y;
        below.z += worldOffset_.z;
    }

    RibbonSegment seg;
    seg.above[0] = above.x;
    seg.above[1] = above.y;
    seg.above[2] = above.z;
    seg.below[0] = below.x;
    seg.below[1] = below.y;
    seg.below[2] = below.z;
    seg.age = 0.0f;
    seg.life = data.lifeSpan > 0.0f ? data.lifeSpan : 1.0f;
    seg.color[0] = color[0];
    seg.color[1] = color[1];
    seg.color[2] = color[2];
    seg.initialAlpha = alpha;
    seg.alpha = alpha;
    seg.slot = slot;
    if ((int)state.segments.size() >= kMaxSegmentsPerEmitter) {
        state.segments.pop_front();
    }
    state.segments.push_back(seg);
}

void MdxRibbonSystem::Update(float dt, const Vec3& worldOffset) {
    Update(dt, worldOffset, nullptr, nullptr, 0, 0.0f);
}

void MdxRibbonSystem::Update(float dt, const Vec3& worldOffset, const MdxModel* model,
                             const MdxAnimator* animator, uint32_t frame, float elapsedMs) {
    worldOffset_ = worldOffset;
    const int sequenceIndex = animator ? animator->GetCurrentSequence() : -1;
    for (auto& state : emitters_) {
        const MdxRibbonEmitter& data = *state.emitterData;

        float heightAbove, heightBelow, alpha, visibility;
        float color[3];
        int slot;
        SampleEmitter(data, model, frame, elapsedMs, sequenceIndex,
                      heightAbove, heightBelow, color, alpha, slot, visibility);
        const bool emitterVisible = !animator || animator->IsObjectVisible(data.objectId);
        // Ref emission: accumulate rate*dt capped at 1, at most one
        // segment per update.
        if (emitterVisible && visibility > 0.0f && data.emissionRate > 0.0f) {
            state.emissionAccum = std::min(state.emissionAccum + data.emissionRate * dt, 1.0f);
            if (state.emissionAccum >= 1.0f) {
                state.emissionAccum -= 1.0f;
                EmitSegment(state, model, animator, frame, elapsedMs, sequenceIndex);
            }
        }

        // Stage AA2: Age live segments in-place inside RibbonRingBuffer (0 heap allocations)
        const float gravityDrop = data.gravity * dt;
        state.segments.remove_if_inplace([&](RibbonSegment& seg) -> bool {
            seg.age += dt;
            if (seg.age >= seg.life) {
                return true;
            }
            seg.above[1] -= gravityDrop;
            seg.below[1] -= gravityDrop;
            const float fade = seg.life > 0.0f ? std::clamp(1.0f - (seg.age / seg.life), 0.0f, 1.0f) : 1.0f;
            seg.alpha = seg.initialAlpha * fade;
            return false;
        });

        UploadEmitter(state);
    }
}

void MdxRibbonSystem::UploadEmitter(RibbonEmitterState& state) {
    const size_t quads = state.segments.size() > 1 ? state.segments.size() - 1 : 0;
    if (quads == 0) {
        state.stagingVertices.clear();
        state.stagingIndices.clear();
        return;
    }

    const MdxRibbonEmitter& data = *state.emitterData;
    const unsigned columns = std::max(1u, data.columns);
    const unsigned rows = std::max(1u, data.rows);

    const size_t prevVertCap = state.stagingVertices.capacity();
    const size_t prevIdxCap = state.stagingIndices.capacity();
    auto& verts = state.stagingVertices;
    auto& indices = state.stagingIndices;
    verts.clear();
    indices.clear();

    const size_t count = state.segments.size();
    for (size_t i = 0; i + 1 < count; i++) {
        const RibbonSegment& a = state.segments[i];
        const RibbonSegment& b = state.segments[i + 1];
        const float t0 = count > 1 ? (float)i / (float)(count - 1) : 0.0f;
        const float t1 = count > 1 ? (float)(i + 1) / (float)(count - 1) : 1.0f;
        const int slot = std::max(0, a.slot);
        const float cellU = (float)(slot % (int)columns) / (float)columns;
        const float cellV = (float)(slot / (int)columns) / (float)rows;
        const float cellW = 1.0f / (float)columns;
        const float cellH = 1.0f / (float)rows;
        const uint16_t base = (uint16_t)verts.size();
        verts.push_back({a.above[0], a.above[1], a.above[2], cellU + t0 * cellW, cellV,
                         a.color[0], a.color[1], a.color[2], a.alpha});
        verts.push_back({a.below[0], a.below[1], a.below[2], cellU + t0 * cellW, cellV + cellH,
                         a.color[0], a.color[1], a.color[2], a.alpha});
        verts.push_back({b.above[0], b.above[1], b.above[2], cellU + t1 * cellW, cellV,
                         b.color[0], b.color[1], b.color[2], b.alpha});
        verts.push_back({b.below[0], b.below[1], b.below[2], cellU + t1 * cellW, cellV + cellH,
                         b.color[0], b.color[1], b.color[2], b.alpha});
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 1);
        indices.push_back(base + 3);
        indices.push_back(base + 2);
    }

    if (verts.capacity() == prevVertCap && indices.capacity() == prevIdxCap) {
        ++zeroAllocRibbonUploads_;
    }

    if (!Renderer::IsActive()) return;

    const size_t vertBytes = verts.size() * sizeof(Vertex);
    const size_t indexBytes = indices.size() * sizeof(uint16_t);
    if ((int)verts.size() > state.vertexCapacity || !state.vertexBuffer.id) {
        if (state.vertexBuffer.id) sg_destroy_buffer(state.vertexBuffer);
        state.vertexCapacity = (int)verts.size() + 256;
        sg_buffer_desc vbd = {};
        vbd.usage.vertex_buffer = true;
        vbd.usage.stream_update = true;
        vbd.size = (size_t)state.vertexCapacity * sizeof(Vertex);
        state.vertexBuffer = sg_make_buffer(&vbd);
    }
    if ((int)indices.size() > state.indexCapacity || !state.indexBuffer.id) {
        if (state.indexBuffer.id) sg_destroy_buffer(state.indexBuffer);
        state.indexCapacity = (int)indices.size() + 256;
        sg_buffer_desc ibd = {};
        ibd.usage.index_buffer = true;
        ibd.usage.stream_update = true;
        ibd.size = (size_t)state.indexCapacity * sizeof(uint16_t);
        state.indexBuffer = sg_make_buffer(&ibd);
    }
    if (!state.vertexBuffer.id || !state.indexBuffer.id) return;
    sg_update_buffer(state.vertexBuffer, sg_range{verts.data(), vertBytes});
    sg_update_buffer(state.indexBuffer, sg_range{indices.data(), indexBytes});
}

void MdxRibbonSystem::Render(const Mat4& view, const Mat4& proj) {
    if (!sharedRibbonShader_.id || !sharedRibbonPipeline_.id) return;
    if (sg_query_pipeline_state(sharedRibbonPipeline_) != SG_RESOURCESTATE_VALID) return;
    if (sg_query_shader_state(sharedRibbonShader_) != SG_RESOURCESTATE_VALID) return;

    Mat4 vp = proj * view;
    struct VSBlock {
        float vp[16];
    };
    VSBlock vsU = {};
    std::memcpy(vsU.vp, vp.m, 64);

    for (auto& state : emitters_) {
        const size_t quads = state.segments.size() > 1 ? state.segments.size() - 1 : 0;
        if (quads == 0) continue;
        if (!state.textureView.id || !state.sampler.id) continue;
        if (!state.vertexBuffer.id || !state.indexBuffer.id) continue;
        if (sg_query_buffer_state(state.vertexBuffer) != SG_RESOURCESTATE_VALID) continue;
        if (sg_query_buffer_state(state.indexBuffer) != SG_RESOURCESTATE_VALID) continue;
        if (sg_query_view_state(state.textureView) != SG_RESOURCESTATE_VALID) continue;
        if (sg_query_sampler_state(state.sampler) != SG_RESOURCESTATE_VALID) continue;

        sg_pipeline pipeline = sharedRibbonPipeline_;
        if (state.filterMode == 3 || state.filterMode == 4) { // Additive / AddAlpha
            if (sharedAdditivePipeline_.id) pipeline = sharedAdditivePipeline_;
        } else if (state.filterMode == 5 || state.filterMode == 6) { // Modulate / Modulate 2x
            if (sharedModulatePipeline_.id) pipeline = sharedModulatePipeline_;
        }
        sg_apply_pipeline(pipeline);
        sg_apply_uniforms(0, sg_rng(vsU));

        sg_bindings bind = {};
        bind.vertex_buffers[0] = state.vertexBuffer;
        bind.index_buffer = state.indexBuffer;
        bind.views[0] = state.textureView;
        bind.samplers[0] = state.sampler;
        sg_apply_bindings(&bind);

        sg_draw(0, (int)(quads * 6), 1);
    }
}

size_t MdxRibbonSystem::GetSegmentCount() const {
    size_t total = 0;
    for (const auto& e : emitters_) total += e.segments.size();
    return total;
}

size_t MdxRibbonSystem::GetLiveQuadCount() const {
    size_t total = 0;
    for (const auto& e : emitters_) {
        if (e.segments.size() > 1) total += e.segments.size() - 1;
    }
    return total;
}

sg_shader MdxRibbonSystem::RibbonShader() {
    return sharedRibbonShader_;
}

sg_pipeline MdxRibbonSystem::RibbonPipeline() {
    return sharedRibbonPipeline_;
}

void MdxRibbonSystem::InitSharedRibbonResources() {
    if (sharedRibbonShader_.id) return;

    {
        const char* vertSrc = R"(
            #version 410 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec2 aUV;
            layout(location = 2) in vec4 aColor;

            out vec4 vColor;
            out vec2 vUV;

            uniform mat4 uVP;

            void main() {
                gl_Position = uVP * vec4(aPos, 1.0);
                vColor = aColor;
                vUV = aUV;
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

        sd.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
        sd.uniform_blocks[0].size = 64;
        sd.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_NATIVE;
        sd.uniform_blocks[0].glsl_uniforms[0].type = SG_UNIFORMTYPE_MAT4;
        sd.uniform_blocks[0].glsl_uniforms[0].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[0].glsl_name = "uVP";

        sd.views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT;
        sd.views[0].texture.image_type = SG_IMAGETYPE_2D;
        sd.views[0].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
        sd.samplers[0].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;
        sd.texture_sampler_pairs[0].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.texture_sampler_pairs[0].glsl_name = "uTexture";
        sd.texture_sampler_pairs[0].view_slot = 0;
        sd.texture_sampler_pairs[0].sampler_slot = 0;

        sharedRibbonShader_ = sg_make_shader(&sd);
    }

    if (!sharedRibbonPipeline_.id && sharedRibbonShader_.id) {
        sg_pipeline_desc pd = {};
        pd.shader = sharedRibbonShader_;
        pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
        pd.index_type = SG_INDEXTYPE_UINT16;
        pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
        pd.depth.write_enabled = false;
        pd.cull_mode = SG_CULLMODE_NONE;
        pd.colors[0].blend.enabled = true;
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

        pd.layout.buffers[0].stride = sizeof(Vertex);
        pd.layout.buffers[0].step_func = SG_VERTEXSTEP_PER_VERTEX;

        pd.layout.attrs[0] = {0, 0, SG_VERTEXFORMAT_FLOAT3};
        pd.layout.attrs[1] = {0, 12, SG_VERTEXFORMAT_FLOAT2};
        pd.layout.attrs[2] = {0, 20, SG_VERTEXFORMAT_FLOAT4};

        sharedRibbonPipeline_ = sg_make_pipeline(&pd);

        // Additive pipeline (SRC_ALPHA, ONE) - standard for weapon trails
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
        sharedAdditivePipeline_ = sg_make_pipeline(&pd);

        // Modulate pipeline (DST_COLOR, ZERO)
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_DST_COLOR;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ZERO;
        sharedModulatePipeline_ = sg_make_pipeline(&pd);
    }
}

void MdxRibbonSystem::ReleaseSharedRibbonResources() {
    if (sharedModulatePipeline_.id) {
        sg_destroy_pipeline(sharedModulatePipeline_);
        sharedModulatePipeline_ = {0};
    }
    if (sharedAdditivePipeline_.id) {
        sg_destroy_pipeline(sharedAdditivePipeline_);
        sharedAdditivePipeline_ = {0};
    }
    if (sharedRibbonPipeline_.id) {
        sg_destroy_pipeline(sharedRibbonPipeline_);
        sharedRibbonPipeline_ = {0};
    }
    if (sharedRibbonShader_.id) {
        sg_destroy_shader(sharedRibbonShader_);
        sharedRibbonShader_ = {0};
    }
}
