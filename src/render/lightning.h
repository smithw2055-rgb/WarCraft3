#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "render/math.h"
#include "third_party/sokol/sokol_gfx.h"

// Lightning bolt visuals (JASS AddLightning*). Scene coordinates are
// Y-up: JASS (x, y, z-height) maps to scene (x, z, y).

struct LightningBoltParams {
    float avgSegLen = 100.0f;
    float width = 50.0f; // half-width in world units (ref min(len/2, width))
    float noiseScale = 0.05f;
    float duration = 2.0f;
};

struct LightningPoint {
    float x = 0, y = 0, z = 0;
};

// Segment count for a bolt of the given length (ref AvgSegLen).
int LightningSegmentCount(float length, float avgSegLen);
// xorshift32 step; LightningRand01 returns [0, 1).
uint32_t LightningRandNext(uint32_t& state);
float LightningRand01(uint32_t& state);
// Midpoint-jittered polyline with exactly segments+1 points; endpoints
// exact, interior offsets bounded by noiseScale * length (times a sin
// envelope peaking mid-bolt).
void GenerateBoltPoints(const LightningPoint& p1, const LightningPoint& p2,
                        int segments, float noiseScale, uint32_t& rngState,
                        std::vector<LightningPoint>& out);
// Built-in retail table (Splats\LightningData.slk values); unknown codes
// fall back to the CLPB row.
LightningBoltParams DefaultBoltParams(const std::string& code);

class LightningSystem {
public:
    struct Bolt {
        int id = 0;
        // Scene coords (Y-up): x, y=height, z.
        float x1 = 0, y1 = 0, z1 = 0, x2 = 0, y2 = 0, z2 = 0;
        float r = 1, g = 1, b = 1, a = 1;
        std::string code;
        LightningBoltParams params;
        uint32_t rngState = 0;
        float flickerLeft = 0;
        bool visible = true;
        std::vector<LightningPoint> points;
    };

    LightningSystem();
    ~LightningSystem();

    // Endpoints in SCENE coords (caller converts JASS x,y,z-height).
    void SetBolt(int id, float x1, float y1, float z1,
                 float x2, float y2, float z2,
                 float r, float g, float b, float a,
                 const std::string& code);
    bool RemoveBolt(int id);
    bool HasBolt(int id) const;
    const Bolt* GetBolt(int id) const;
    size_t BoltCount() const { return bolts_.size(); }
    void Clear();
    void SetBoltVisible(int id, bool visible);

    // Map override for Splats\LightningData.slk (custom maps may replace
    // the retail table); false when nothing usable was parsed.
    bool LoadParamsFromSlk(const uint8_t* data, size_t size);

    void Update(float dt);
    // No-op without a GL context (sg_isvalid), so logic stays testable.
    void Render(const Mat4& view, const Mat4& proj);

private:
    void EnsureResources();
    void Regenerate(Bolt& bolt);

    std::unordered_map<int, Bolt> bolts_;
    std::unordered_map<std::string, LightningBoltParams> paramOverrides_;

    sg_buffer vertexBuffer_ = {0};
    sg_buffer indexBuffer_ = {0};
    int vertexCapacity_ = 0;
    int indexCapacity_ = 0;
    bool resourcesReady_ = false;
};

sg_shader LightningSharedShader();
sg_pipeline LightningSharedPipeline();
