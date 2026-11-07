#include "render/render_graph.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace render {

bool TerrainChunkGrid::BuildFromW3E(const W3EMap& map, int chunkTiles) {
    chunks_.clear();
    totalTriangles_ = 0;
    chunkTiles_ = std::max(4, chunkTiles);

    if (map.tileWidth <= 1 || map.tileHeight <= 1 || map.tiles.empty()) {
        return false;
    }

    const int numTilesX = map.tileWidth - 1;
    const int numTilesY = map.tileHeight - 1;
    const int chunksX = (numTilesX + chunkTiles_ - 1) / chunkTiles_;
    const int chunksY = (numTilesY + chunkTiles_ - 1) / chunkTiles_;
    constexpr float kTileWorldSize = 128.0f;

    chunks_.reserve(static_cast<size_t>(chunksX * chunksY));

    for (int cy = 0; cy < chunksY; ++cy) {
        for (int cx = 0; cx < chunksX; ++cx) {
            TerrainChunk chunk;
            chunk.chunkX = cx;
            chunk.chunkY = cy;
            chunk.startTileX = cx * chunkTiles_;
            chunk.startTileY = cy * chunkTiles_;
            chunk.endTileX = std::min(numTilesX, chunk.startTileX + chunkTiles_);
            chunk.endTileY = std::min(numTilesY, chunk.startTileY + chunkTiles_);

            const float minWorldX = map.offsetX + static_cast<float>(chunk.startTileX) * kTileWorldSize;
            const float maxWorldX = map.offsetX + static_cast<float>(chunk.endTileX) * kTileWorldSize;
            const float minWorldZ = map.offsetY + static_cast<float>(chunk.startTileY) * kTileWorldSize;
            const float maxWorldZ = map.offsetY + static_cast<float>(chunk.endTileY) * kTileWorldSize;

            float minH =  1e9f;
            float maxH = -1e9f;
            int waterCount = 0;
            int cliffCount = 0;

            for (int ty = chunk.startTileY; ty <= chunk.endTileY; ++ty) {
                for (int tx = chunk.startTileX; tx <= chunk.endTileX; ++tx) {
                    const size_t cornerIdx = static_cast<size_t>(ty) * static_cast<size_t>(map.tileWidth) +
                                             static_cast<size_t>(tx);
                    if (cornerIdx >= map.tiles.size()) continue;
                    const W3ETileCorner& c = map.tiles[cornerIdx];
                    const float gh = c.FinalGroundHeight();
                    minH = std::min(minH, gh);
                    maxH = std::max(maxH, gh);
                    if (c.IsWater()) {
                        const float wh = c.FinalWaterHeight();
                        minH = std::min(minH, wh);
                        maxH = std::max(maxH, wh);
                        if (tx < chunk.endTileX && ty < chunk.endTileY) waterCount++;
                    }
                    if (tx < chunk.endTileX && ty < chunk.endTileY && c.LayerHeight() != 2) {
                        cliffCount++;
                    }
                }
            }

            if (minH > maxH) {
                minH = -256.0f;
                maxH =  512.0f;
            }

            // World convention in SceneManager / RenderServer: X = worldX, Y = height, Z = worldY
            chunk.aabbMin = Vec3(minWorldX, minH - 32.0f, minWorldZ);
            chunk.aabbMax = Vec3(maxWorldX, maxH + 64.0f, maxWorldZ);

            const int tilesInChunk = (chunk.endTileX - chunk.startTileX) *
                                     (chunk.endTileY - chunk.startTileY);
            chunk.triangleCount = tilesInChunk * 2;
            chunk.waterTileCount = waterCount;
            chunk.cliffTileCount = cliffCount;

            totalTriangles_ += chunk.triangleCount;
            chunks_.push_back(chunk);
        }
    }
    return !chunks_.empty();
}

TerrainCullStats TerrainChunkGrid::CullChunks(const Frustum& frustum,
                                              std::vector<int>& outVisibleChunkIndices) const {
    TerrainCullStats stats;
    stats.totalChunks = static_cast<int>(chunks_.size());
    stats.totalTriangles = totalTriangles_;
    outVisibleChunkIndices.clear();
    outVisibleChunkIndices.reserve(chunks_.size());

    for (size_t i = 0; i < chunks_.size(); ++i) {
        const TerrainChunk& c = chunks_[i];
        const Frustum::Containment state = frustum.ClassifyAABB(c.aabbMin, c.aabbMax);
        if (state == Frustum::Containment::Outside) {
            stats.culledChunks++;
            stats.culledTriangles += c.triangleCount;
            continue;
        }
        if (state == Frustum::Containment::Inside) {
            stats.fullyInsideChunks++;
        } else {
            stats.intersectingChunks++;
        }
        stats.visibleChunks++;
        stats.visibleTriangles += c.triangleCount;
        outVisibleChunkIndices.push_back(static_cast<int>(i));
    }
    return stats;
}

void ParticleRibbonBatcher::BeginFrame() {
    pendingParticles_.clear();
    pendingRibbons_.clear();
    particleBatches_.clear();
    ribbonBatches_.clear();
    stats_ = ParticleRibbonBatchStats{};
}

void ParticleRibbonBatcher::SubmitEmitterParticles(const std::vector<ParticleSubmission>& particles) {
    if (particles.empty()) return;
    stats_.emitterSubmissions++;
    stats_.totalParticlesSubmitted += static_cast<int>(particles.size());
    pendingParticles_.insert(pendingParticles_.end(), particles.begin(), particles.end());
}

void ParticleRibbonBatcher::SubmitRibbonChain(const std::vector<RibbonSegmentSubmission>& chain) {
    if (chain.size() < 2) return;
    stats_.emitterSubmissions++;
    stats_.totalRibbonSegmentsSubmitted += static_cast<int>(chain.size());
    pendingRibbons_.insert(pendingRibbons_.end(), chain.begin(), chain.end());
}

void ParticleRibbonBatcher::BuildCoalescedBatches() {
    particleBatches_.clear();
    ribbonBatches_.clear();

    std::unordered_map<uint64_t, size_t> particleBatchIdxByKey;
    for (const auto& p : pendingParticles_) {
        const uint64_t key = MakeEffectBatchKey(p.textureId, p.blendMode);
        auto it = particleBatchIdxByKey.find(key);
        if (it == particleBatchIdxByKey.end()) {
            const size_t idx = particleBatches_.size();
            particleBatchIdxByKey[key] = idx;
            CoalescedParticleBatch batch;
            batch.textureId = p.textureId;
            batch.blendMode = p.blendMode;
            batch.particles.push_back(p);
            particleBatches_.push_back(std::move(batch));
        } else {
            particleBatches_[it->second].particles.push_back(p);
        }
    }

    std::unordered_map<uint64_t, size_t> ribbonBatchIdxByKey;
    std::unordered_map<uint64_t, std::unordered_set<uint32_t>> emittersPerRibbonBatch;
    for (const auto& seg : pendingRibbons_) {
        const uint64_t key = MakeEffectBatchKey(seg.textureId, seg.blendMode);
        auto it = ribbonBatchIdxByKey.find(key);
        if (it == ribbonBatchIdxByKey.end()) {
            const size_t idx = ribbonBatches_.size();
            ribbonBatchIdxByKey[key] = idx;
            CoalescedRibbonBatch batch;
            batch.textureId = seg.textureId;
            batch.blendMode = seg.blendMode;
            batch.segments.push_back(seg);
            ribbonBatches_.push_back(std::move(batch));
        } else {
            ribbonBatches_[it->second].segments.push_back(seg);
        }
        emittersPerRibbonBatch[key].insert(seg.emitterInstanceId);
    }

    for (auto& rb : ribbonBatches_) {
        const uint64_t key = MakeEffectBatchKey(rb.textureId, rb.blendMode);
        rb.stripCount = static_cast<uint32_t>(emittersPerRibbonBatch[key].size());
        rb.quadCount = (rb.segments.size() > rb.stripCount)
            ? static_cast<uint32_t>(rb.segments.size() - rb.stripCount)
            : 0u;
    }

    stats_.coalescedParticleBatches = static_cast<int>(particleBatches_.size());
    stats_.coalescedRibbonBatches = static_cast<int>(ribbonBatches_.size());
    stats_.totalCoalescedDrawCalls = stats_.coalescedParticleBatches + stats_.coalescedRibbonBatches;
    stats_.drawCallsSaved = std::max(0, stats_.emitterSubmissions - stats_.totalCoalescedDrawCalls);
}

resource::RID MdxSharedMeshRegistry::RegisterSharedMesh(const std::string& canonicalPath,
                                                        const MdxModel& model) {
    auto it = ridByPath_.find(canonicalPath);
    if (it != ridByPath_.end()) {
        return it->second;
    }

    const uint32_t meshIdx = nextMeshIndex_++;
    const resource::RID rid(meshIdx, 1u, resource::ResourceKind::MdxModel);

    MdxSharedMeshAsset asset;
    asset.meshRid = rid;
    asset.canonicalPath = canonicalPath;
    asset.modelKeyId = meshIdx;
    asset.geosetCount = model.geosets.size();
    asset.boneCount = model.bones.size();
    asset.sequenceCount = model.sequences.size();
    asset.boundsRadius = std::max(32.0f, model.boundsRadius);
    for (const auto& g : model.geosets) {
        asset.totalVertices += g.vertices.size() / 3;
        asset.totalTriangles += g.indices.size() / 3;
    }

    ridByPath_[canonicalPath] = rid;
    meshesByRid_[rid] = std::move(asset);
    return rid;
}

const MdxSharedMeshAsset* MdxSharedMeshRegistry::GetSharedMesh(resource::RID rid) const {
    auto it = meshesByRid_.find(rid);
    return (it != meshesByRid_.end()) ? &it->second : nullptr;
}

int MdxSharedMeshRegistry::CreateInstance(resource::RID meshRid,
                                          const Mat4& worldTransform,
                                          int teamColor) {
    if (!GetSharedMesh(meshRid)) return -1;
    const int id = nextInstanceId_++;
    MdxLightweightInstance inst;
    inst.instanceId = id;
    inst.meshRid = meshRid;
    inst.worldTransform = worldTransform;
    inst.teamColor = teamColor;
    instances_[id] = inst;
    return id;
}

MdxLightweightInstance* MdxSharedMeshRegistry::GetInstance(int instanceId) {
    auto it = instances_.find(instanceId);
    return (it != instances_.end()) ? &it->second : nullptr;
}

const MdxLightweightInstance* MdxSharedMeshRegistry::GetInstance(int instanceId) const {
    auto it = instances_.find(instanceId);
    return (it != instances_.end()) ? &it->second : nullptr;
}

bool MdxSharedMeshRegistry::DestroyInstance(int instanceId) {
    return instances_.erase(instanceId) > 0;
}

int MdxSharedMeshRegistry::SubmitInstancesToRenderServer(RenderServer& renderServer) const {
    int submitted = 0;
    for (const auto& [instId, inst] : instances_) {
        if (!inst.visible) continue;
        const MdxSharedMeshAsset* asset = GetSharedMesh(inst.meshRid);
        if (!asset) continue;

        RenderItem item;
        item.entityHandle = instId;
        item.modelId = asset->modelKeyId;
        item.submeshIndex = 0;
        const bool isTrans = inst.vertexTint.w < 0.999f;
        item.material = ShaderMaterial::FromMdxLayer(
            isTrans ? 2 : 0,
            0,
            asset->modelKeyId,
            asset->boneCount > 0,
            false);
        item.properties.teamColorIndex = inst.teamColor;
        item.properties.vertexTint = inst.vertexTint;
        item.properties.animationFrame = inst.normalizedTime;
        item.worldMatrix = inst.worldTransform;
        item.worldBoundsCenter = Vec3(
            inst.worldTransform.m[12],
            inst.worldTransform.m[13],
            inst.worldTransform.m[14]);
        item.worldBoundsRadius = asset->boundsRadius;

        renderServer.SubmitItem(item);
        submitted++;
    }
    return submitted;
}

RenderGraph::RenderGraph() {
    RegisterStandardWarcraftPasses();
}

void RenderGraph::RegisterStandardWarcraftPasses() {
    passes_.clear();
    passes_.push_back({
        RenderGraphPassId::ShadowMapPass, "ShadowMapPass", true,
        [](RenderGraphContext& ctx, RenderGraphPassMetrics& m) {
            if (ctx.renderServer) {
                const int opaqueVisible = ctx.renderServer->GetStats().visibleItems;
                m.itemsProcessed = opaqueVisible;
                m.drawCallsSubmitted = ctx.renderServer->GetStats().opaqueBatchCount;
            }
        }
    });
    passes_.push_back({
        RenderGraphPassId::TerrainChunkPass, "TerrainChunkPass", true,
        [](RenderGraphContext& ctx, RenderGraphPassMetrics& m) {
            if (ctx.terrainGrid) {
                ctx.terrainStats = ctx.terrainGrid->CullChunks(ctx.frustum, ctx.visibleTerrainChunks);
                m.itemsProcessed = ctx.terrainStats.visibleChunks;
                // Coalesced instanced terrain chunk draw + water/cliff layer draw
                m.drawCallsSubmitted = (ctx.terrainStats.visibleChunks > 0) ? 2 : 0;
            }
        }
    });
    passes_.push_back({
        RenderGraphPassId::OpaqueInstancedPass, "OpaqueInstancedPass", true,
        [](RenderGraphContext& ctx, RenderGraphPassMetrics& m) {
            if (ctx.renderServer) {
                const auto& batches = ctx.renderServer->GetOpaqueBatches();
                for (const auto& b : batches) m.itemsProcessed += static_cast<int>(b.instances.size());
                m.drawCallsSubmitted = static_cast<int>(batches.size());
            }
        }
    });
    passes_.push_back({
        RenderGraphPassId::TransparentSortedPass, "TransparentSortedPass", true,
        [](RenderGraphContext& ctx, RenderGraphPassMetrics& m) {
            if (ctx.renderServer) {
                const auto& batches = ctx.renderServer->GetTransparentBatches();
                for (const auto& b : batches) m.itemsProcessed += static_cast<int>(b.instances.size());
                m.drawCallsSubmitted = static_cast<int>(batches.size());
            }
        }
    });
    passes_.push_back({
        RenderGraphPassId::ParticleRibbonBatchPass, "ParticleRibbonBatchPass", true,
        [](RenderGraphContext& ctx, RenderGraphPassMetrics& m) {
            if (ctx.particleBatcher) {
                ctx.particleBatcher->BuildCoalescedBatches();
                const auto& ps = ctx.particleBatcher->GetStats();
                m.itemsProcessed = ps.totalParticlesSubmitted + ps.totalRibbonSegmentsSubmitted;
                m.drawCallsSubmitted = ps.totalCoalescedDrawCalls;
            }
        }
    });
    passes_.push_back({
        RenderGraphPassId::PortraitOffscreenPass, "PortraitOffscreenPass", true,
        [](RenderGraphContext& ctx, RenderGraphPassMetrics& m) {
            if (ctx.hasActivePortraitModel) {
                m.itemsProcessed = 1;
                m.drawCallsSubmitted = 1;
            }
        }
    });
    passes_.push_back({
        RenderGraphPassId::UiOverlayPass, "UiOverlayPass", true,
        [](RenderGraphContext& ctx, RenderGraphPassMetrics& m) {
            m.itemsProcessed = ctx.uiOverlayCommandCount;
            m.drawCallsSubmitted = (ctx.uiOverlayCommandCount > 0) ? 1 : 0;
        }
    });
}

void RenderGraph::SetPassCallback(RenderGraphPassId passId, PassExecuteFn fn) {
    for (auto& p : passes_) {
        if (p.id == passId) {
            p.callback = std::move(fn);
            return;
        }
    }
}

bool RenderGraph::SetPassEnabled(RenderGraphPassId passId, bool enabled) {
    for (auto& p : passes_) {
        if (p.id == passId) {
            p.enabled = enabled;
            return true;
        }
    }
    return false;
}

bool RenderGraph::IsPassEnabled(RenderGraphPassId passId) const {
    for (const auto& p : passes_) {
        if (p.id == passId) return p.enabled;
    }
    return false;
}

void RenderGraph::ExecuteFrame(RenderGraphContext& ctx) {
    ctx.frustum = Frustum::FromViewProj(ctx.proj * ctx.view);
    if (ctx.renderServer) {
        ctx.renderServer->BuildAndSortBatches();
    }

    metrics_.clear();
    metrics_.reserve(passes_.size());
    totalFrameDrawCalls_ = 0;
    executedPassCount_ = 0;

    for (const auto& p : passes_) {
        RenderGraphPassMetrics m;
        m.passId = p.id;
        m.name = p.name;
        m.enabled = p.enabled;
        if (p.enabled && p.callback) {
            p.callback(ctx, m);
            m.executed = true;
            executedPassCount_++;
            totalFrameDrawCalls_ += m.drawCallsSubmitted;
        }
        metrics_.push_back(std::move(m));
    }
}

const RenderGraphPassMetrics* RenderGraph::GetPassMetrics(RenderGraphPassId passId) const {
    for (const auto& m : metrics_) {
        if (m.passId == passId) return &m;
    }
    return nullptr;
}

} // namespace render
