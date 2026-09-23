#include "lightning.h"
#include "render/shader.h"
#include "resource/slk.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {

// Flicker regen interval for persistent JASS bolts (retail Duration is
// the fade-out lifespan of transient bolts, not used here).
constexpr float kFlickerInterval = 0.08f;
constexpr int kMaxSegments = 64;

uint32_t HashCode(const std::string& code) {    uint32_t h = 2166136261u;
    for (char c : code) {
        h ^= (uint8_t)c;
        h *= 16777619u;
    }
    return h ? h : 1u;
}

} // namespace

void EnsureLightningResources();

int LightningSegmentCount(float length, float avgSegLen) {
    if (!(length > 0.0f) || !(avgSegLen > 0.0f)) return 1;
    return std::clamp((int)std::ceil(length / avgSegLen), 1, kMaxSegments);
}

uint32_t LightningRandNext(uint32_t& state) {
    // xorshift32; zero state would stick, reseed defensively.
    if (state == 0) state = 0x9E3779B9u;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

float LightningRand01(uint32_t& state) {
    return (LightningRandNext(state) >> 8) * (1.0f / 16777216.0f);
}

void GenerateBoltPoints(const LightningPoint& p1, const LightningPoint& p2,
                        int segments, float noiseScale, uint32_t& rngState,
                        std::vector<LightningPoint>& out) {
    const int n = std::clamp(segments, 1, kMaxSegments);
    const float dx = p2.x - p1.x;
    const float dy = p2.y - p1.y;
    const float dz = p2.z - p1.z;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    out.clear();
    out.reserve((size_t)n + 1);
    for (int i = 0; i <= n; i++) {
        const float t = (float)i / n;
        // Sin envelope: ends pinned, jitter peaks mid-bolt.
        const float amp = noiseScale * length * std::sin(3.14159265f * t);
        const float ox = (LightningRand01(rngState) * 2.0f - 1.0f) * amp;
        const float oy = (LightningRand01(rngState) * 2.0f - 1.0f) * amp;
        const float oz = (LightningRand01(rngState) * 2.0f - 1.0f) * amp;
        LightningPoint p;
        p.x = p1.x + dx * t + ox;
        p.y = p1.y + dy * t + oy;
        p.z = p1.z + dz * t + oz;
        out.push_back(p);
    }
    // Pin the exact endpoints (float math must not drift them).
    if (!out.empty()) {
        out.front() = p1;
        out.back() = p2;
    }
}

LightningBoltParams DefaultBoltParams(const std::string& code) {
    static const std::unordered_map<std::string, LightningBoltParams> kTable = {
        {"CLPB", {100, 50, 0.05f, 2}},   {"CLSB", {100, 30, 0.05f, 2}},
        {"MBUR", {100, 45, 0.04f, 2}},   {"CHIM", {100, 45, 0.04f, 2}},
        {"AFOD", {100, 50, 0.05f, 2}},   {"HWPB", {100, 50, 0.0001f, 2}},
        {"HWSB", {100, 40, 0.0001f, 2}}, {"MFPB", {100, 30, 0.05f, 2}},
        {"DRAB", {50, 40, 0.01f, 2}},    {"DRAL", {50, 40, 0.01f, 2}},
        {"DRAM", {50, 40, 0.01f, 2}},    {"FORK", {100, 50, 0.05f, 2}},
        {"SPLK", {100, 45, 0.04f, 2}},   {"LEAS", {50, 80, 0.01f, 2}},
    };
    const auto it = kTable.find(code);
    if (it != kTable.end()) return it->second;
    return kTable.at("CLPB");
}

LightningSystem::LightningSystem() = default;

LightningSystem::~LightningSystem() {
    if (resourcesReady_) {
        if (vertexBuffer_.id) sg_destroy_buffer(vertexBuffer_);
        if (indexBuffer_.id) sg_destroy_buffer(indexBuffer_);
    }
}

void LightningSystem::SetBolt(int id, float x1, float y1, float z1,
                              float x2, float y2, float z2,
                              float r, float g, float b, float a,
                              const std::string& code) {
    Bolt& bolt = bolts_[id];
    const bool fresh = (bolt.id != id);
    bolt.id = id;
    bolt.x1 = x1; bolt.y1 = y1; bolt.z1 = z1;
    bolt.x2 = x2; bolt.y2 = y2; bolt.z2 = z2;
    bolt.r = r; bolt.g = g; bolt.b = b; bolt.a = a;
    bolt.code = code;
    const auto it = paramOverrides_.find(code);
    bolt.params = (it != paramOverrides_.end()) ? it->second : DefaultBoltParams(code);
    if (fresh) {
        bolt.rngState = HashCode(code) ^ (uint32_t)(id * 2654435761u);
        bolt.flickerLeft = 0.0f;
        Regenerate(bolt);
    }
}

bool LightningSystem::RemoveBolt(int id) {
    return bolts_.erase(id) > 0;
}

bool LightningSystem::HasBolt(int id) const {
    return bolts_.find(id) != bolts_.end();
}

const LightningSystem::Bolt* LightningSystem::GetBolt(int id) const {
    const auto it = bolts_.find(id);
    return it != bolts_.end() ? &it->second : nullptr;
}

void LightningSystem::Clear() {
    bolts_.clear();
}

void LightningSystem::SetBoltVisible(int id, bool visible) {
    auto it = bolts_.find(id);
    if (it != bolts_.end()) it->second.visible = visible;
}

bool LightningSystem::LoadParamsFromSlk(const uint8_t* data, size_t size) {
    if (!data || size == 0) return false;
    SlkTable table;
    if (!table.Load(data, size)) return false;
    const int nameCol = table.FindColumn("Name");
    const int segCol = table.FindColumn("AvgSegLen");
    const int widthCol = table.FindColumn("Width");
    const int noiseCol = table.FindColumn("NoiseScale");
    const int durCol = table.FindColumn("Duration");
    if (nameCol < 0) return false;
    bool any = false;
    for (int row = 0; row < table.Rows(); row++) {
        const std::string name = table.GetString(row, nameCol);
        if (name.empty()) continue;
        LightningBoltParams params = DefaultBoltParams(name);
        if (segCol >= 0) {
            const std::string v = table.GetString(row, segCol);
            if (!v.empty()) params.avgSegLen = std::max(1.0f, (float)std::atof(v.c_str()));
        }
        if (widthCol >= 0) {
            const std::string v = table.GetString(row, widthCol);
            if (!v.empty()) params.width = std::max(1.0f, (float)std::atof(v.c_str()));
        }
        if (noiseCol >= 0) {
            const std::string v = table.GetString(row, noiseCol);
            if (!v.empty()) params.noiseScale = std::max(0.0f, (float)std::atof(v.c_str()));
        }
        if (durCol >= 0) {
            const std::string v = table.GetString(row, durCol);
            if (!v.empty()) params.duration = std::max(0.1f, (float)std::atof(v.c_str()));
        }
        paramOverrides_[name] = params;
        any = true;
    }
    return any;
}

void LightningSystem::Regenerate(Bolt& bolt) {
    LightningPoint p1{bolt.x1, bolt.y1, bolt.z1};
    LightningPoint p2{bolt.x2, bolt.y2, bolt.z2};
    const float dx = p2.x - p1.x;
    const float dy = p2.y - p1.y;
    const float dz = p2.z - p1.z;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    const int segments = LightningSegmentCount(length, bolt.params.avgSegLen);
    GenerateBoltPoints(p1, p2, segments, bolt.params.noiseScale, bolt.rngState, bolt.points);
}

void LightningSystem::Update(float dt) {
    if (dt <= 0) return;
    for (auto& [id, bolt] : bolts_) {
        (void)id;
        bolt.flickerLeft -= dt;
        if (bolt.flickerLeft <= 0.0f) {
            bolt.flickerLeft = kFlickerInterval;
            Regenerate(bolt);
        }
    }
}

struct LightningVertex {
    float x, y, z;
    float r, g, b, a;
};

void LightningSystem::EnsureResources() {
    if (resourcesReady_) return;
    resourcesReady_ = true;
    // Buffers are created on first upload (capacities start at 0).
}

void LightningSystem::Render(const Mat4& view, const Mat4& proj) {
    if (!sg_isvalid()) return;
    EnsureResources();
    EnsureLightningResources();
    if (LightningSharedShader().id == 0 || LightningSharedPipeline().id == 0) return;

    // Camera forward in world space (negated view column 2).
    float fwd[3] = {-(view.m[8]), -(view.m[9]), -(view.m[10])};
    const float fwdLen = std::sqrt(fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]);
    if (fwdLen > 1e-6f) {
        fwd[0] /= fwdLen; fwd[1] /= fwdLen; fwd[2] /= fwdLen;
    } else {
        fwd[0] = 0; fwd[1] = 0; fwd[2] = 1;
    }
    const float camRight[3] = {view.m[0], view.m[1], view.m[2]};

    std::vector<LightningVertex> verts;
    std::vector<uint32_t> indices;
    for (const auto& [id, bolt] : bolts_) {
        (void)id;
        if (!bolt.visible || bolt.points.size() < 2) continue;
        const float dx = bolt.x2 - bolt.x1;
        const float dy = bolt.y2 - bolt.y1;
        const float dz = bolt.z2 - bolt.z1;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!(length > 0.0f)) continue;
        const float halfW = std::min(length * 0.5f, bolt.params.width);
        const size_t base = verts.size();
        const size_t n = bolt.points.size();
        for (size_t i = 0; i < n; i++) {
            const LightningPoint& p = bolt.points[i];
            const LightningPoint& q = bolt.points[std::min(i + 1, n - 1)];
            const LightningPoint& o = bolt.points[i > 0 ? i - 1 : 0];
            float dir[3] = {q.x - o.x, q.y - o.y, q.z - o.z};
            const float dirLen = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
            float side[3];
            if (dirLen > 1e-6f) {
                dir[0] /= dirLen; dir[1] /= dirLen; dir[2] /= dirLen;
                side[0] = dir[1] * fwd[2] - dir[2] * fwd[1];
                side[1] = dir[2] * fwd[0] - dir[0] * fwd[2];
                side[2] = dir[0] * fwd[1] - dir[1] * fwd[0];
                const float sideLen = std::sqrt(side[0] * side[0] + side[1] * side[1] + side[2] * side[2]);
                if (sideLen < 1e-4f) {
                    side[0] = camRight[0]; side[1] = camRight[1]; side[2] = camRight[2];
                } else {
                    side[0] /= sideLen; side[1] /= sideLen; side[2] /= sideLen;
                }
            } else {
                side[0] = camRight[0]; side[1] = camRight[1]; side[2] = camRight[2];
            }
            // Alpha fades at the end caps (tapered-quad look, ref batch).
            const float t = (n > 1) ? (float)i / (float)(n - 1) : 0.0f;
            const float capFade = std::sin(3.14159265f * std::clamp(t, 0.0f, 1.0f));
            const float alpha = bolt.a * (0.25f + 0.75f * capFade);
            verts.push_back({p.x + side[0] * halfW, p.y + side[1] * halfW, p.z + side[2] * halfW,
                             bolt.r, bolt.g, bolt.b, 0.0f});
            verts.push_back({p.x - side[0] * halfW, p.y - side[1] * halfW, p.z - side[2] * halfW,
                             bolt.r, bolt.g, bolt.b, 0.0f});
            // Center spine carries the color (additive glow core).
            verts.push_back({p.x, p.y, p.z, bolt.r, bolt.g, bolt.b, alpha});
            if (i + 1 < n) {
                const uint32_t v0 = (uint32_t)(base + i * 3);
                indices.push_back(v0 + 0); indices.push_back(v0 + 2); indices.push_back(v0 + 3);
                indices.push_back(v0 + 0); indices.push_back(v0 + 3); indices.push_back(v0 + 1);
                indices.push_back(v0 + 1); indices.push_back(v0 + 3); indices.push_back(v0 + 4);
                indices.push_back(v0 + 1); indices.push_back(v0 + 4); indices.push_back(v0 + 2);
            }
        }
    }
    if (verts.empty()) return;

    const size_t vertBytes = verts.size() * sizeof(LightningVertex);
    const size_t indexBytes = indices.size() * sizeof(uint32_t);
    if ((int)verts.size() > vertexCapacity_ || !vertexBuffer_.id) {
        if (vertexBuffer_.id) sg_destroy_buffer(vertexBuffer_);
        vertexCapacity_ = (int)verts.size() + 256;
        sg_buffer_desc vbd = {};
        vbd.usage.vertex_buffer = true;
        vbd.usage.stream_update = true;
        vbd.size = (size_t)vertexCapacity_ * sizeof(LightningVertex);
        vertexBuffer_ = sg_make_buffer(&vbd);
    }
    if ((int)indices.size() > indexCapacity_ || !indexBuffer_.id) {
        if (indexBuffer_.id) sg_destroy_buffer(indexBuffer_);
        indexCapacity_ = (int)indices.size() + 256;
        sg_buffer_desc ibd = {};
        ibd.usage.index_buffer = true;
        ibd.usage.stream_update = true;
        ibd.size = (size_t)indexCapacity_ * sizeof(uint32_t);
        indexBuffer_ = sg_make_buffer(&ibd);
    }
    if (!vertexBuffer_.id || !indexBuffer_.id) return;
    sg_update_buffer(vertexBuffer_, sg_range{verts.data(), vertBytes});
    sg_update_buffer(indexBuffer_, sg_range{indices.data(), indexBytes});

    Mat4 vp = proj * view;
    struct VSBlock {
        float vp[16];
    };
    VSBlock vsU = {};
    std::memcpy(vsU.vp, vp.m, 64);
    if (sg_query_pipeline_state(LightningSharedPipeline()) != SG_RESOURCESTATE_VALID) return;
    sg_apply_pipeline(LightningSharedPipeline());
    sg_apply_uniforms(0, sg_rng(vsU));
    sg_bindings bind = {};
    bind.vertex_buffers[0] = vertexBuffer_;
    bind.index_buffer = indexBuffer_;
    sg_apply_bindings(&bind);
    sg_draw(0, (int)indices.size(), 1);
}

static sg_shader g_lightningShader = {0};
static sg_pipeline g_lightningPipeline = {0};

void EnsureLightningResources() {
    if (g_lightningShader.id) return;
    {
        // Plain vertex-color glow (no texture): the edge fade is baked
        // into vertex alpha, additive blend like ref LightningEffectBatch
        // (SRC_ALPHA, ONE), no depth test, no depth write.
        const char* vertSrc = R"(
            #version 410 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec4 aColor;

            out vec4 vColor;

            uniform mat4 uVP;

            void main() {
                gl_Position = uVP * vec4(aPos, 1.0);
                vColor = aColor;
            }
        )";
        const char* fragSrc = R"(
            #version 410 core
            in vec4 vColor;
            out vec4 FragColor;
            void main() {
                FragColor = vColor;
                if (FragColor.a < 0.004) discard;
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

        g_lightningShader = sg_make_shader(&sd);
    }

    if (!g_lightningPipeline.id && g_lightningShader.id) {
        sg_pipeline_desc pd = {};
        pd.shader = g_lightningShader;
        pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
        pd.index_type = SG_INDEXTYPE_UINT32;
        pd.depth.compare = SG_COMPAREFUNC_ALWAYS;
        pd.depth.write_enabled = false;
        pd.cull_mode = SG_CULLMODE_NONE;
        pd.colors[0].blend.enabled = true;
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
        pd.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE;

        pd.layout.buffers[0].stride = sizeof(LightningVertex);
        pd.layout.buffers[0].step_func = SG_VERTEXSTEP_PER_VERTEX;

        pd.layout.attrs[0] = {0, 0, SG_VERTEXFORMAT_FLOAT3};
        pd.layout.attrs[1] = {0, 12, SG_VERTEXFORMAT_FLOAT4};

        g_lightningPipeline = sg_make_pipeline(&pd);
    }
}

sg_shader LightningSharedShader() {
    return g_lightningShader;
}

sg_pipeline LightningSharedPipeline() {
    return g_lightningPipeline;
}
