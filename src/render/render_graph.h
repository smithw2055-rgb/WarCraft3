#pragma once
// ============================================================================
// Stage N: Declarative RenderGraph, TerrainChunkGrid Frustum Culling,
//          ParticleRibbonBatcher & MdxSharedMeshRegistry
//          (Inspired by Godot 4 RenderingServer & Unity URP RenderGraph)
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "map/w3e.h"
#include "model/mdx.h"
#include "render/math.h"
#include "render/render_server.h"
#include "resource/resource_server.h"

namespace render {

// ============================================================================
// 1. TerrainChunkGrid — Spatial Partitioning & 3-State Frustum Culling for W3EMap
// ============================================================================

struct TerrainChunk {
    int chunkX = 0;
    int chunkY = 0;
    int startTileX = 0;
    int startTileY = 0;
    int endTileX = 0;
    int endTileY = 0;
    Vec3 aabbMin = Vec3(0.0f, 0.0f, 0.0f);
    Vec3 aabbMax = Vec3(0.0f, 0.0f, 0.0f);
    int triangleCount = 0;
    int waterTileCount = 0;
    int cliffTileCount = 0;
};

struct TerrainCullStats {
    int totalChunks = 0;
    int visibleChunks = 0;
    int culledChunks = 0;
    int fullyInsideChunks = 0;
    int intersectingChunks = 0;
    int totalTriangles = 0;
    int visibleTriangles = 0;
    int culledTriangles = 0;
};

class TerrainChunkGrid {
public:
    static constexpr int kDefaultChunkTiles = 16; // 16x16 tiles = 2048x2048 world units

    TerrainChunkGrid() = default;

    bool BuildFromW3E(const W3EMap& map, int chunkTiles = kDefaultChunkTiles);

    TerrainCullStats CullChunks(const Frustum& frustum,
                                std::vector<int>& outVisibleChunkIndices) const;

    size_t GetChunkCount() const { return chunks_.size(); }
    const std::vector<TerrainChunk>& GetChunks() const { return chunks_; }
    int GetChunkTiles() const { return chunkTiles_; }
    int GetTotalTriangles() const { return totalTriangles_; }

private:
    int chunkTiles_ = kDefaultChunkTiles;
    int totalTriangles_ = 0;
    std::vector<TerrainChunk> chunks_;
};

// ============================================================================
// 2. ParticleRibbonBatcher — Cross-Instance Particle & Ribbon Coalescing
// ============================================================================

struct ParticleSubmission {
    uint32_t textureId = 0;
    RenderBlendMode blendMode = RenderBlendMode::Additive;
    Vec3 worldPos = Vec3(0.0f, 0.0f, 0.0f);
    float size = 16.0f;
    Vec4 color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    float uvCol = 0.0f;
    float uvRow = 0.0f;
};

struct RibbonSegmentSubmission {
    uint32_t emitterInstanceId = 0;
    uint32_t textureId = 0;
    RenderBlendMode blendMode = RenderBlendMode::AlphaBlend;
    Vec3 aboveWorld = Vec3(0.0f, 0.0f, 0.0f);
    Vec3 belowWorld = Vec3(0.0f, 0.0f, 0.0f);
    Vec4 color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    float uCoord = 0.0f;
};

struct CoalescedParticleBatch {
    uint32_t textureId = 0;
    RenderBlendMode blendMode = RenderBlendMode::Additive;
    std::vector<ParticleSubmission> particles;
};

struct CoalescedRibbonBatch {
    uint32_t textureId = 0;
    RenderBlendMode blendMode = RenderBlendMode::AlphaBlend;
    uint32_t stripCount = 0;
    uint32_t quadCount = 0;
    std::vector<RibbonSegmentSubmission> segments;
};

struct ParticleRibbonBatchStats {
    int emitterSubmissions = 0;
    int totalParticlesSubmitted = 0;
    int totalRibbonSegmentsSubmitted = 0;
    int coalescedParticleBatches = 0;
    int coalescedRibbonBatches = 0;
    int totalCoalescedDrawCalls = 0;
    int drawCallsSaved = 0;
};

class ParticleRibbonBatcher {
public:
    ParticleRibbonBatcher() = default;

    void BeginFrame();
    void SubmitEmitterParticles(const std::vector<ParticleSubmission>& particles);
    void SubmitRibbonChain(const std::vector<RibbonSegmentSubmission>& chain);
    void BuildCoalescedBatches();

    const std::vector<CoalescedParticleBatch>& GetParticleBatches() const { return particleBatches_; }
    const std::vector<CoalescedRibbonBatch>& GetRibbonBatches() const { return ribbonBatches_; }
    const ParticleRibbonBatchStats& GetStats() const { return stats_; }

private:
    static uint64_t MakeEffectBatchKey(uint32_t textureId, RenderBlendMode blendMode) {
        return (static_cast<uint64_t>(static_cast<uint8_t>(blendMode)) << 32) |
               static_cast<uint64_t>(textureId);
    }

    std::vector<ParticleSubmission> pendingParticles_;
    std::vector<RibbonSegmentSubmission> pendingRibbons_;
    std::vector<CoalescedParticleBatch> particleBatches_;
    std::vector<CoalescedRibbonBatch> ribbonBatches_;
    ParticleRibbonBatchStats stats_;
};

// ============================================================================
// 3. MdxSharedMeshRegistry — Godot 4 RenderingServer Style Asset/Instance Split
// ============================================================================

struct MdxSharedMeshAsset {
    resource::RID meshRid{};
    std::string canonicalPath;
    uint32_t modelKeyId = 0;
    size_t geosetCount = 0;
    size_t boneCount = 0;
    size_t sequenceCount = 0;
    size_t totalVertices = 0;
    size_t totalTriangles = 0;
    float boundsRadius = 64.0f;
};

struct MdxLightweightInstance {
    int instanceId = 0;
    resource::RID meshRid{};
    Mat4 worldTransform = Mat4::Identity();
    int sequenceIndex = 0;
    float normalizedTime = 0.0f;
    int teamColor = 0;
    Vec4 vertexTint = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    int boneAtlasRow = 0;
    bool visible = true;
};

class MdxSharedMeshRegistry {
public:
    MdxSharedMeshRegistry() = default;

    resource::RID RegisterSharedMesh(const std::string& canonicalPath,
                                     const MdxModel& model);
    const MdxSharedMeshAsset* GetSharedMesh(resource::RID rid) const;

    int CreateInstance(resource::RID meshRid,
                       const Mat4& worldTransform,
                       int teamColor = 0);
    MdxLightweightInstance* GetInstance(int instanceId);
    const MdxLightweightInstance* GetInstance(int instanceId) const;
    bool DestroyInstance(int instanceId);

    size_t SharedMeshCount() const { return meshesByRid_.size(); }
    size_t ActiveInstanceCount() const { return instances_.size(); }

    // Submit all visible lightweight instances directly into a RenderServer
    int SubmitInstancesToRenderServer(RenderServer& renderServer) const;

private:
    uint32_t nextMeshIndex_ = 1;
    int nextInstanceId_ = 1;
    std::unordered_map<std::string, resource::RID> ridByPath_;
    std::unordered_map<resource::RID, MdxSharedMeshAsset, resource::RIDHasher> meshesByRid_;
    std::unordered_map<int, MdxLightweightInstance> instances_;
};

// ============================================================================
// 4. Declarative RenderGraph — Explicit Multi-Pass Frame Pipeline
// ============================================================================

enum class RenderGraphPassId : int {
    ShadowMapPass           = 0,
    TerrainChunkPass        = 100,
    OpaqueInstancedPass     = 200,
    TransparentSortedPass   = 300,
    ParticleRibbonBatchPass = 400,
    PortraitOffscreenPass   = 500,
    UiOverlayPass           = 600
};

struct RenderGraphPassMetrics {
    RenderGraphPassId passId = RenderGraphPassId::ShadowMapPass;
    std::string name;
    bool enabled = true;
    bool executed = false;
    int itemsProcessed = 0;
    int drawCallsSubmitted = 0;
};

struct RenderGraphContext {
    Mat4 view = Mat4::Identity();
    Mat4 proj = Mat4::Identity();
    Vec3 cameraEye = Vec3(0.0f, 0.0f, 0.0f);
    Frustum frustum;
    RenderServer* renderServer = nullptr;
    const TerrainChunkGrid* terrainGrid = nullptr;
    ParticleRibbonBatcher* particleBatcher = nullptr;
    bool hasActivePortraitModel = false;
    int uiOverlayCommandCount = 0;

    TerrainCullStats terrainStats{};
    std::vector<int> visibleTerrainChunks;
};

class RenderGraph {
public:
    using PassExecuteFn = std::function<void(RenderGraphContext&, RenderGraphPassMetrics&)>;

    RenderGraph();

    void RegisterStandardWarcraftPasses();
    void SetPassCallback(RenderGraphPassId passId, PassExecuteFn fn);
    bool SetPassEnabled(RenderGraphPassId passId, bool enabled);
    bool IsPassEnabled(RenderGraphPassId passId) const;

    void ExecuteFrame(RenderGraphContext& ctx);

    const RenderGraphPassMetrics* GetPassMetrics(RenderGraphPassId passId) const;
    const std::vector<RenderGraphPassMetrics>& GetAllPassMetrics() const { return metrics_; }
    int GetTotalFrameDrawCalls() const { return totalFrameDrawCalls_; }
    int GetExecutedPassCount() const { return executedPassCount_; }

private:
    struct PassEntry {
        RenderGraphPassId id = RenderGraphPassId::ShadowMapPass;
        std::string name;
        bool enabled = true;
        PassExecuteFn callback;
    };

    std::vector<PassEntry> passes_;
    std::vector<RenderGraphPassMetrics> metrics_;
    int totalFrameDrawCalls_ = 0;
    int executedPassCount_ = 0;
};

} // namespace render
