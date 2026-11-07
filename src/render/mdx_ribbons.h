#pragma once
// Skeletal ribbon trails (MDX RIBB chunk), mirroring ref RibbonEmitter +
// Ribbon: each emission freezes an above/below vertex pair from the emitter
// node's pivot transformed by its world matrix; segments age out by
// lifeSpan while color/alpha/slot resample live every frame. Rendered as
// camera-independent strips (verts already in world space).
#include "third_party/sokol/sokol_gfx.h"
#include "model/mdx.h"
#include "render/math.h"
#include <array>
#include <cstdint>
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

struct RibbonVertex {
    float x, y, z;
    float u, v;
    float r, g, b, a;
};

// ============================================================
// Stage AA2: Fixed-capacity zero-allocation ring buffer for
// ribbon trail segments (replaces std::deque<RibbonSegment>).
// ============================================================
class RibbonRingBuffer {
public:
    static constexpr size_t kMaxCapacity = 512;

    size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }
    void clear() {
        head_ = 0;
        count_ = 0;
    }

    RibbonSegment& operator[](size_t i) {
        return storage_[(head_ + i) % kMaxCapacity];
    }
    const RibbonSegment& operator[](size_t i) const {
        return storage_[(head_ + i) % kMaxCapacity];
    }

    void pop_front() {
        if (count_ > 0) {
            head_ = (head_ + 1) % kMaxCapacity;
            --count_;
        }
    }

    void push_back(const RibbonSegment& seg) {
        if (count_ == kMaxCapacity) {
            storage_[head_] = seg;
            head_ = (head_ + 1) % kMaxCapacity;
        } else {
            storage_[(head_ + count_) % kMaxCapacity] = seg;
            ++count_;
        }
    }

    // Compacts live segments in-place (preserving chronological order) without heap allocation
    template <typename Pred>
    void remove_if_inplace(Pred&& shouldRemove) {
        size_t writeIdx = 0;
        for (size_t readIdx = 0; readIdx < count_; ++readIdx) {
            RibbonSegment& seg = (*this)[readIdx];
            if (!shouldRemove(seg)) {
                if (writeIdx != readIdx) {
                    (*this)[writeIdx] = seg;
                }
                ++writeIdx;
            }
        }
        count_ = writeIdx;
        if (count_ == 0) {
            head_ = 0;
        }
    }

private:
    std::array<RibbonSegment, kMaxCapacity> storage_{};
    size_t head_ = 0;
    size_t count_ = 0;
};

struct RibbonEmitterState {
    const MdxRibbonEmitter* emitterData = nullptr;
    float emissionAccum = 0.0f;
    RibbonRingBuffer segments;
    // Stage AA2: Persistent pre-reserved staging buffers (0 per-frame heap allocations)
    std::vector<RibbonVertex> stagingVertices;
    std::vector<uint16_t> stagingIndices;
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
    using Vertex = RibbonVertex;

    MdxRibbonSystem();
    ~MdxRibbonSystem();

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
    uint64_t GetZeroAllocRibbonUploadCount() const { return zeroAllocRibbonUploads_; }

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
    uint64_t zeroAllocRibbonUploads_ = 0;

    static sg_shader sharedRibbonShader_;
    static sg_pipeline sharedRibbonPipeline_;
    static sg_pipeline sharedAdditivePipeline_;
    static sg_pipeline sharedModulatePipeline_;
};
