#pragma once
// Skeletal ribbon trails (MDX RIBB chunk), mirroring ref RibbonEmitter +
// Ribbon: each emission freezes an above/below vertex pair from the emitter
// node's pivot transformed by its world matrix; segments age out by
// lifeSpan while color/alpha/slot resample live every frame. Rendered as
// camera-independent strips (verts already in world space).
#include "third_party/sokol/sokol_gfx.h"
#include "model/mdx.h"
#include "render/math.h"
#include <deque>
#include <vector>

class MdxAnimator;
class MpqManager;

struct RibbonSegment {
    float above[3] = {0, 0, 0};
    float below[3] = {0, 0, 0};
    float age = 0.0f;
    float life = 1.0f;
    float color[3] = {1, 1, 1};
    float initialAlpha = 1.0f;
    float alpha = 1.0f;
    int slot = 0;
};

struct RibbonEmitterState {
    const MdxRibbonEmitter* emitterData = nullptr;
    float emissionAccum = 0.0f;
    std::deque<RibbonSegment> segments;
    sg_image texture = {0};
    sg_view textureView = {0};
    sg_sampler sampler = {0};
    sg_buffer vertexBuffer = {0};
    sg_buffer indexBuffer = {0};
    int vertexCapacity = 0;
    int indexCapacity = 0;
    int filterMode = 0;
};

class MdxRibbonSystem {
public:
    MdxRibbonSystem();
    ~MdxRibbonSystem();

    struct Vertex {
        float x, y, z;
        float u, v;
        float r, g, b, a;
    };

    void Init(const MdxModel& model, class MpqManager* mpq);
    void Update(float dt, const Vec3& worldOffset = Vec3{0, 0, 0});
    void Update(float dt, const Vec3& worldOffset, const MdxModel* model, const MdxAnimator* animator,
                uint32_t frame, float elapsedMs);
    void Render(const Mat4& view, const Mat4& proj);
    void SetWorldOffset(const Vec3& offset) { worldOffset_ = offset; }
    const Vec3& GetWorldOffset() const { return worldOffset_; }
    void SetInstanceMatrix(const Mat4& m) {
        instanceMatrix_ = m;
        hasInstanceMatrix_ = true;
        worldOffset_ = Vec3{m.m[12], m.m[13], m.m[14]};
    }
    const Mat4& GetInstanceMatrix() const { return instanceMatrix_; }
    void Destroy();

    size_t GetSegmentCount() const;
    size_t GetLiveQuadCount() const;

    static sg_shader RibbonShader();
    static sg_pipeline RibbonPipeline();
    static void InitSharedRibbonResources();
    static void ReleaseSharedRibbonResources();

private:
    static constexpr int kMaxSegmentsPerEmitter = 512;

    void EmitSegment(RibbonEmitterState& state, const MdxModel* model, const MdxAnimator* animator,
                     uint32_t frame, float elapsedMs, int sequenceIndex);
    void SampleEmitter(const MdxRibbonEmitter& emitter, const MdxModel* model,
                       uint32_t frame, float elapsedMs, int sequenceIndex,
                       float& heightAbove, float& heightBelow,
                       float color[3], float& alpha, int& slot, float& visibility) const;
    void UploadEmitter(RibbonEmitterState& state);

    std::vector<RibbonEmitterState> emitters_;
    Vec3 worldOffset_ = {0, 0, 0};
    Mat4 instanceMatrix_ = Mat4::Identity();
    bool hasInstanceMatrix_ = false;

    static sg_shader sharedRibbonShader_;
    static sg_pipeline sharedRibbonPipeline_;
    static sg_pipeline sharedAdditivePipeline_;
    static sg_pipeline sharedModulatePipeline_;
};
