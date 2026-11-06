#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "render/math.h"

class CSimulation;
class CUnit;

namespace render {

// ============================================================================
// 1. Modern ShaderMaterial & Pipeline State Abstraction
//    (Inspired by Godot 4 RenderingServer & Unity SRP MaterialPropertyBlock)
// ============================================================================

enum class RenderBlendMode : uint8_t {
    Opaque = 0,       // MDX FilterMode 0: None (Opaque)
    AlphaTest = 1,    // MDX FilterMode 1: Transparent (Alpha cut-out >= 0.75)
    AlphaBlend = 2,   // MDX FilterMode 2: Blend (SrcAlpha, OneMinusSrcAlpha)
    Additive = 3,     // MDX FilterMode 3: Additive (SrcAlpha, One)
    AddAlpha = 4,     // MDX FilterMode 4: AddAlpha (SrcAlpha, One)
    Modulate = 5,     // MDX FilterMode 5: Modulate (DstColor, Zero)
    Modulate2x = 6    // MDX FilterMode 6: Modulate 2X (DstColor, SrcColor)
};

enum class RenderDepthState : uint8_t {
    ReadWrite = 0,
    ReadOnly = 1,
    Disabled = 2
};

enum class RenderCullMode : uint8_t {
    Back = 0,
    Front = 1,
    None = 2
};

enum RenderShadingFlags : uint32_t {
    kShadingNone         = 0,
    kShadingUnshaded     = 1u << 0, // MDX 0x1
    kShadingSphereEnvMap = 1u << 1, // MDX 0x2
    kShadingTwoSided     = 1u << 4, // MDX 0x10
    kShadingUnfogged     = 1u << 5, // MDX 0x20
    kShadingNoDepthTest  = 1u << 6, // MDX 0x40
    kShadingNoDepthSet   = 1u << 7  // MDX 0x80
};

struct ShaderMaterial {
    uint32_t shaderId = 1;
    uint32_t textureId = 0;
    RenderBlendMode blendMode = RenderBlendMode::Opaque;
    RenderDepthState depthState = RenderDepthState::ReadWrite;
    RenderCullMode cullMode = RenderCullMode::Back;
    uint32_t shadingFlags = kShadingNone;
    bool isSkinned = false;
    bool isOverlay = false;

    bool IsTransparent() const {
        return blendMode == RenderBlendMode::AlphaBlend ||
               blendMode == RenderBlendMode::Additive ||
               blendMode == RenderBlendMode::AddAlpha ||
               blendMode == RenderBlendMode::Modulate ||
               blendMode == RenderBlendMode::Modulate2x;
    }

    // Deterministic 64-bit key packing pipeline state + shader + primary texture
    uint64_t ComputePipelineKey() const;

    // Factory helper converting Warcraft 3 MDX layer filterMode & flags
    static ShaderMaterial FromMdxLayer(int filterMode,
                                       int layerFlags,
                                       uint32_t textureId,
                                       bool isSkinned = false,
                                       bool isOverlay = false);
};

// Per-instance uniform overrides (Unity MaterialPropertyBlock equivalent)
class MaterialPropertyBlock {
public:
    int teamColorIndex = 0;
    Vec4 vertexTint = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    Vec4 geosetColorAlpha = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    float emissiveGain = 0.0f;
    float animationFrame = 0.0f;

    void SetFloat(const std::string& name, float value) { floatProps_[name] = value; }
    float GetFloat(const std::string& name, float defaultValue = 0.0f) const;

    void SetVec4(const std::string& name, const Vec4& value) { vec4Props_[name] = value; }
    Vec4 GetVec4(const std::string& name, const Vec4& defaultValue = Vec4(0, 0, 0, 0)) const;

    bool HasCustomOverrides() const { return !floatProps_.empty() || !vec4Props_.empty(); }

private:
    std::unordered_map<std::string, float> floatProps_;
    std::unordered_map<std::string, Vec4> vec4Props_;
};

// ============================================================================
// 2. View-Projection Frustum Culling (Gribb-Hartmann 6-Plane Extractor)
// ============================================================================

struct FrustumPlane {
    Vec3 normal = Vec3(0.0f, 1.0f, 0.0f);
    float d = 0.0f;

    float SignedDistance(const Vec3& point) const {
        return normal.x * point.x + normal.y * point.y + normal.z * point.z + d;
    }
};

class Frustum {
public:
    enum PlaneIndex {
        kLeft = 0,
        kRight = 1,
        kBottom = 2,
        kTop = 3,
        kNear = 4,
        kFar = 5,
        kPlaneCount = 6
    };

    static Frustum FromViewProj(const Mat4& viewProj);

    bool TestSphere(const Vec3& center, float radius) const;
    bool TestAABB(const Vec3& minCorner, const Vec3& maxCorner) const;

    const FrustumPlane& GetPlane(int idx) const { return planes_[idx]; }

private:
    FrustumPlane planes_[kPlaneCount];
};

// ============================================================================
// 3. Fixed-Tick Simulation to High-Refresh Render Interpolation
// ============================================================================

struct RenderTransformSnapshot {
    Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
    float facingDeg = 0.0f;
    float flyHeight = 0.0f;
    Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);
    Vec4 vertexColor = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    int playerIndex = 0;
    uint32_t unitTypeId = 0;
    bool visible = true;
};

class RenderInterpolationBuffer {
public:
    // Shortest-arc angle interpolation in degrees ([0, 360))
    static float LerpAngleDeg(float fromDeg, float toDeg, float alpha);

    static RenderTransformSnapshot Interpolate(const RenderTransformSnapshot& prev,
                                               const RenderTransformSnapshot& curr,
                                               float alpha);

    // Captures a full simulation tick from CSimulation. Moves previous current
    // snapshots into prevSnapshots_ and records the latest CUnit state.
    void CaptureSimulationTick(const CSimulation& sim);

    // Manually record a snapshot for a specific entity handle
    void PushEntitySnapshot(int handleId, const RenderTransformSnapshot& snapshot);

    bool SampleEntity(int handleId, float alpha, RenderTransformSnapshot& outSnapshot) const;
    Mat4 ComputeInterpolatedWorldMatrix(int handleId, float alpha) const;

    void Clear() {
        prevSnapshots_.clear();
        currSnapshots_.clear();
    }

    size_t TrackedEntityCount() const { return currSnapshots_.size(); }

private:
    std::unordered_map<int, RenderTransformSnapshot> prevSnapshots_;
    std::unordered_map<int, RenderTransformSnapshot> currSnapshots_;
};

// ============================================================================
// 4. GPU Instancing & Automatic Render Queue Batching (RenderServer)
// ============================================================================

struct InstanceGpuPayload {
    Mat4 worldMatrix = Mat4::Identity();
    Vec4 vertexTint = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    // x = teamColorIndex, y = layerAlpha, z = animFrame, w = emissiveGain
    Vec4 instanceParams = Vec4(0.0f, 1.0f, 0.0f, 0.0f);
};

struct InstanceBatchKey {
    uint32_t modelId = 0;
    uint32_t submeshIndex = 0;
    uint64_t materialKey = 0;

    bool operator==(const InstanceBatchKey& other) const {
        return modelId == other.modelId &&
               submeshIndex == other.submeshIndex &&
               materialKey == other.materialKey;
    }
};

struct InstanceBatchKeyHasher {
    size_t operator()(const InstanceBatchKey& k) const noexcept {
        size_t h1 = std::hash<uint32_t>{}(k.modelId);
        size_t h2 = std::hash<uint32_t>{}(k.submeshIndex);
        size_t h3 = std::hash<uint64_t>{}(k.materialKey);
        return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL) ^ (h3 << 1);
    }
};

struct RenderItem {
    int entityHandle = 0;
    uint32_t modelId = 0;
    uint32_t submeshIndex = 0;
    ShaderMaterial material;
    MaterialPropertyBlock properties;
    Mat4 worldMatrix = Mat4::Identity();
    Vec3 worldBoundsCenter = Vec3(0.0f, 0.0f, 0.0f);
    float worldBoundsRadius = 64.0f;
};

struct RenderBatchGroup {
    InstanceBatchKey key;
    ShaderMaterial material;
    float sortDepthSq = 0.0f;
    std::vector<InstanceGpuPayload> instances;
    std::vector<int> entityHandles;
};

struct RenderFrameStats {
    int submittedItems = 0;
    int culledByFrustum = 0;
    int visibleItems = 0;
    int opaqueBatchCount = 0;
    int transparentBatchCount = 0;
    int totalDrawCalls = 0;
    int instancedDrawCallsSaved = 0;
    int pipelineStateSwitches = 0;
};

class RenderServer {
public:
    RenderServer() = default;
    ~RenderServer() = default;

    // Material registry
    uint32_t RegisterMaterial(const std::string& name, const ShaderMaterial& material);
    const ShaderMaterial* GetMaterial(uint32_t materialHandle) const;
    const ShaderMaterial* FindMaterialByName(const std::string& name) const;

    // Frame lifecycle
    void BeginFrame(const Mat4& view, const Mat4& proj, const Vec3& cameraPos);
    void SetFrustumCullingEnabled(bool enabled) { frustumCullingEnabled_ = enabled; }
    bool IsFrustumCullingEnabled() const { return frustumCullingEnabled_; }

    // Submit single renderable item
    void SubmitItem(const RenderItem& item);

    // Bridge: Extract all active visible CUnits from CSimulation, sample their
    // interpolated transform & UnitVisualComponent state, and submit them
    void SubmitSimulationUnits(const CSimulation& sim,
                               const RenderInterpolationBuffer& interpBuffer,
                               float alpha,
                               float defaultBoundingRadius = 64.0f);

    // Build instanced batches and sort opaque (pipeline + front-to-back) and
    // transparent (back-to-front) queues
    void BuildAndSortBatches();

    // Execute all sorted batches through an optional backend draw callback
    void ExecuteBatches(const std::function<void(const RenderBatchGroup&)>& drawCallback = nullptr);

    const std::vector<RenderBatchGroup>& GetOpaqueBatches() const { return opaqueBatches_; }
    const std::vector<RenderBatchGroup>& GetTransparentBatches() const { return transparentBatches_; }
    const RenderFrameStats& GetStats() const { return stats_; }
    const Frustum& GetFrustum() const { return frustum_; }

private:
    Mat4 view_ = Mat4::Identity();
    Mat4 proj_ = Mat4::Identity();
    Mat4 viewProj_ = Mat4::Identity();
    Vec3 cameraPos_ = Vec3(0.0f, 0.0f, 0.0f);
    Frustum frustum_;
    bool frustumCullingEnabled_ = true;

    std::vector<RenderItem> visibleItems_;
    std::vector<RenderBatchGroup> opaqueBatches_;
    std::vector<RenderBatchGroup> transparentBatches_;
    RenderFrameStats stats_;

    uint32_t nextMaterialHandle_ = 1;
    std::unordered_map<uint32_t, ShaderMaterial> materialsById_;
    std::unordered_map<std::string, uint32_t> materialIdsByName_;
};

} // namespace render
