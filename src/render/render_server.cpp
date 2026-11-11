#include "render/render_server.h"

#include <algorithm>
#include <cmath>

#include "core/job_system.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "engine/simulation_module.h"
#include "render/scene.h"

namespace render {

// ============================================================================
// 1. ShaderMaterial & MaterialPropertyBlock
// ============================================================================

uint64_t ShaderMaterial::ComputePipelineKey() const {
    uint64_t key = 0;
    key |= (static_cast<uint64_t>(IsTransparent() ? 1 : 0) & 0x1ULL) << 63;
    key |= (static_cast<uint64_t>(isOverlay ? 1 : 0) & 0x1ULL) << 62;
    key |= (static_cast<uint64_t>(blendMode) & 0xFULL) << 58;
    key |= (static_cast<uint64_t>(depthState) & 0x3ULL) << 56;
    key |= (static_cast<uint64_t>(cullMode) & 0x3ULL) << 54;
    key |= (static_cast<uint64_t>(isSkinned ? 1 : 0) & 0x1ULL) << 53;
    key |= (static_cast<uint64_t>(shadingFlags & 0x1FFu)) << 44;
    key |= (static_cast<uint64_t>(shaderId & 0xFFFu)) << 32;
    key |= static_cast<uint64_t>(textureId);
    return key;
}

ShaderMaterial ShaderMaterial::FromMdxLayer(int filterMode,
                                            int layerFlags,
                                            uint32_t textureId,
                                            bool isSkinned,
                                            bool isOverlay) {
    ShaderMaterial mat;
    mat.textureId = textureId;
    mat.isSkinned = isSkinned;
    mat.isOverlay = isOverlay;
    mat.shadingFlags = static_cast<uint32_t>(layerFlags);

    switch (filterMode) {
        case 0: mat.blendMode = RenderBlendMode::Opaque; break;
        case 1: mat.blendMode = RenderBlendMode::AlphaTest; break;
        case 2: mat.blendMode = RenderBlendMode::AlphaBlend; break;
        case 3: mat.blendMode = RenderBlendMode::Additive; break;
        case 4: mat.blendMode = RenderBlendMode::AddAlpha; break;
        case 5: mat.blendMode = RenderBlendMode::Modulate; break;
        case 6: mat.blendMode = RenderBlendMode::Modulate2x; break;
        default: mat.blendMode = RenderBlendMode::Opaque; break;
    }

    if ((layerFlags & kShadingNoDepthTest) != 0) {
        mat.depthState = RenderDepthState::Disabled;
    } else if ((layerFlags & kShadingNoDepthSet) != 0 || mat.IsTransparent()) {
        mat.depthState = RenderDepthState::ReadOnly;
    } else {
        mat.depthState = RenderDepthState::ReadWrite;
    }

    if ((layerFlags & kShadingTwoSided) != 0) {
        mat.cullMode = RenderCullMode::None;
    } else {
        mat.cullMode = RenderCullMode::Back;
    }

    mat.shaderId = isSkinned ? 2u : 1u;
    return mat;
}

float MaterialPropertyBlock::GetFloat(const std::string& name, float defaultValue) const {
    auto it = floatProps_.find(name);
    return (it != floatProps_.end()) ? it->second : defaultValue;
}

Vec4 MaterialPropertyBlock::GetVec4(const std::string& name, const Vec4& defaultValue) const {
    auto it = vec4Props_.find(name);
    return (it != vec4Props_.end()) ? it->second : defaultValue;
}

// ============================================================================
// 2. View-Projection Frustum Culling
// ============================================================================

static FrustumPlane MakeNormalizedPlane(float a, float b, float c, float d) {
    FrustumPlane plane;
    float len = std::sqrt(a * a + b * b + c * c);
    if (len > 1e-7f) {
        float invLen = 1.0f / len;
        plane.normal = Vec3(a * invLen, b * invLen, c * invLen);
        plane.d = d * invLen;
    } else {
        plane.normal = Vec3(0.0f, 1.0f, 0.0f);
        plane.d = 0.0f;
    }
    return plane;
}

Frustum Frustum::FromViewProj(const Mat4& vp) {
    // Mat4 is column-major: vp.m[col * 4 + row]
    const float r0x = vp.m[0], r0y = vp.m[4], r0z = vp.m[8],  r0w = vp.m[12];
    const float r1x = vp.m[1], r1y = vp.m[5], r1z = vp.m[9],  r1w = vp.m[13];
    const float r2x = vp.m[2], r2y = vp.m[6], r2z = vp.m[10], r2w = vp.m[14];
    const float r3x = vp.m[3], r3y = vp.m[7], r3z = vp.m[11], r3w = vp.m[15];

    Frustum f;
    f.planes_[kLeft]   = MakeNormalizedPlane(r3x + r0x, r3y + r0y, r3z + r0z, r3w + r0w);
    f.planes_[kRight]  = MakeNormalizedPlane(r3x - r0x, r3y - r0y, r3z - r0z, r3w - r0w);
    f.planes_[kBottom] = MakeNormalizedPlane(r3x + r1x, r3y + r1y, r3z + r1z, r3w + r1w);
    f.planes_[kTop]    = MakeNormalizedPlane(r3x - r1x, r3y - r1y, r3z - r1z, r3w - r1w);
    f.planes_[kNear]   = MakeNormalizedPlane(r3x + r2x, r3y + r2y, r3z + r2z, r3w + r2w);
    f.planes_[kFar]    = MakeNormalizedPlane(r3x - r2x, r3y - r2y, r3z - r2z, r3w - r2w);
    return f;
}

bool Frustum::TestSphere(const Vec3& center, float radius) const {
    for (int i = 0; i < kPlaneCount; ++i) {
        if (planes_[i].SignedDistance(center) < -radius) {
            return false;
        }
    }
    return true;
}

bool Frustum::TestAABB(const Vec3& minCorner, const Vec3& maxCorner) const {
    return ClassifyAABB(minCorner, maxCorner) != Containment::Outside;
}

Frustum::Containment Frustum::ClassifyAABB(const Vec3& minCorner, const Vec3& maxCorner) const {
    bool allInside = true;
    for (int i = 0; i < kPlaneCount; ++i) {
        const FrustumPlane& p = planes_[i];
        const Vec3 positiveVertex(
            p.normal.x >= 0.0f ? maxCorner.x : minCorner.x,
            p.normal.y >= 0.0f ? maxCorner.y : minCorner.y,
            p.normal.z >= 0.0f ? maxCorner.z : minCorner.z
        );
        if (p.SignedDistance(positiveVertex) < 0.0f) {
            return Containment::Outside;
        }
        const Vec3 negativeVertex(
            p.normal.x >= 0.0f ? minCorner.x : maxCorner.x,
            p.normal.y >= 0.0f ? minCorner.y : maxCorner.y,
            p.normal.z >= 0.0f ? minCorner.z : maxCorner.z
        );
        if (p.SignedDistance(negativeVertex) < 0.0f) {
            allInside = false;
        }
    }
    return allInside ? Containment::Inside : Containment::Intersecting;
}

// ============================================================================
// 3. Fixed-Tick Simulation to High-Refresh Render Interpolation
// ============================================================================

float RenderInterpolationBuffer::LerpAngleDeg(float fromDeg, float toDeg, float alpha) {
    float t = std::clamp(alpha, 0.0f, 1.0f);
    float diff = std::fmod(toDeg - fromDeg, 360.0f);
    if (diff > 180.0f) {
        diff -= 360.0f;
    } else if (diff < -180.0f) {
        diff += 360.0f;
    }
    float result = std::fmod(fromDeg + diff * t, 360.0f);
    if (result < 0.0f) {
        result += 360.0f;
    }
    return result;
}

RenderTransformSnapshot RenderInterpolationBuffer::Interpolate(
    const RenderTransformSnapshot& prev,
    const RenderTransformSnapshot& curr,
    float alpha) {
    const float t = std::clamp(alpha, 0.0f, 1.0f);
    RenderTransformSnapshot out = curr;
    out.position = prev.position + (curr.position - prev.position) * t;
    out.facingDeg = LerpAngleDeg(prev.facingDeg, curr.facingDeg, t);
    out.flyHeight = prev.flyHeight + (curr.flyHeight - prev.flyHeight) * t;
    out.scale = prev.scale + (curr.scale - prev.scale) * t;
    out.vertexColor = Vec4(
        prev.vertexColor.x + (curr.vertexColor.x - prev.vertexColor.x) * t,
        prev.vertexColor.y + (curr.vertexColor.y - prev.vertexColor.y) * t,
        prev.vertexColor.z + (curr.vertexColor.z - prev.vertexColor.z) * t,
        prev.vertexColor.w + (curr.vertexColor.w - prev.vertexColor.w) * t
    );
    return out;
}

void RenderInterpolationBuffer::PushEntitySnapshot(int handleId,
                                                   const RenderTransformSnapshot& snapshot) {
    auto it = currSnapshots_.find(handleId);
    if (it != currSnapshots_.end()) {
        prevSnapshots_[handleId] = it->second;
    } else {
        prevSnapshots_[handleId] = snapshot;
    }
    currSnapshots_[handleId] = snapshot;
}

void RenderInterpolationBuffer::CaptureSimulationTick(const CSimulation& sim) {
    // Phase 28: Swap currSnapshots_ into prevSnapshots_ and clear currSnapshots_
    // in place so bucket capacity is reused with zero map reallocation and dead
    // units from 2+ ticks ago are automatically purged from prevSnapshots_.
    std::swap(prevSnapshots_, currSnapshots_);
    currSnapshots_.clear();

    const auto& units = sim.GetUnits();
    for (const CUnit* unit : units) {
        if (!unit) continue;
        const int handleId = unit->GetHandleId();
        RenderTransformSnapshot snap;
        // Warcraft 3 simulation X, Y map onto scene X, Z, while flyHeight maps onto Y
        const auto& vis = unit->VisualComp();
        float modScaleMul = 1.0f;
        uint32_t modTintRgba = 0xFFFFFFFFu;
        if (const auto* simMod = sim.GetSimulationModule()) {
            if (!simMod->OnResolveUnitVisualScaleAndTintForUnit(*unit, modScaleMul, modTintRgba)) {
                simMod->OnResolveUnitVisualScaleAndTint(unit->GetTypeId().value, modScaleMul, modTintRgba);
            }
        }
        const float effectiveScale = vis.scale * modScaleMul;
        const float modR = static_cast<float>((modTintRgba >> 24) & 0xFFu) / 255.0f;
        const float modG = static_cast<float>((modTintRgba >> 16) & 0xFFu) / 255.0f;
        const float modB = static_cast<float>((modTintRgba >> 8) & 0xFFu) / 255.0f;
        const float modA = static_cast<float>(modTintRgba & 0xFFu) / 255.0f;

        snap.flyHeight = unit->GetFlyHeight();
        snap.position = Vec3(unit->GetX(), unit->GetFlyHeight(), unit->GetY());
        snap.facingDeg = unit->GetFacing();
        snap.scale = Vec3(effectiveScale, effectiveScale, effectiveScale);
        snap.vertexColor = Vec4(
            (static_cast<float>(vis.vertexR) / 255.0f) * modR,
            (static_cast<float>(vis.vertexG) / 255.0f) * modG,
            (static_cast<float>(vis.vertexB) / 255.0f) * modB,
            (static_cast<float>(vis.vertexA) / 255.0f) * modA
        );
        snap.playerIndex = unit->GetPlayerIndex();
        snap.unitTypeId = unit->GetTypeId().value;
        snap.visible = !unit->IsHidden() && !unit->IsLoadedAboard();

        if (prevSnapshots_.find(handleId) == prevSnapshots_.end()) {
            prevSnapshots_[handleId] = snap;
        }
        currSnapshots_[handleId] = snap;
    }

    // Prune any units from prevSnapshots_ that no longer exist in currSnapshots_
    for (auto it = prevSnapshots_.begin(); it != prevSnapshots_.end();) {
        if (currSnapshots_.find(it->first) == currSnapshots_.end()) {
            it = prevSnapshots_.erase(it);
        } else {
            ++it;
        }
    }
}

bool RenderInterpolationBuffer::SampleEntity(int handleId,
                                             float alpha,
                                             RenderTransformSnapshot& outSnapshot) const {
    auto currIt = currSnapshots_.find(handleId);
    if (currIt == currSnapshots_.end()) {
        return false;
    }
    auto prevIt = prevSnapshots_.find(handleId);
    if (prevIt == prevSnapshots_.end()) {
        outSnapshot = currIt->second;
        return true;
    }
    outSnapshot = Interpolate(prevIt->second, currIt->second, alpha);
    return true;
}

Mat4 RenderInterpolationBuffer::ComputeInterpolatedWorldMatrix(int handleId, float alpha) const {
    RenderTransformSnapshot snap;
    if (!SampleEntity(handleId, alpha, snap)) {
        return Mat4::Identity();
    }
    return MakeSceneInstanceMatrix(
        snap.position.x,
        snap.position.y,
        snap.position.z,
        snap.facingDeg,
        snap.scale.x,
        snap.scale.y,
        snap.scale.z
    );
}

// ============================================================================
// 4. BonePaletteAtlas Implementation (Stage F1)
// ============================================================================

BonePaletteAtlas::BonePaletteAtlas(int maxBonesPerRow) {
    ResetFrame(maxBonesPerRow);
}

void BonePaletteAtlas::WriteMatrixAt(int row, int boneIndex, const Mat4& m) {
    if (row < 0 || row >= activeRowCount_ || boneIndex < 0 || boneIndex >= maxBonesPerRow_) {
        return;
    }
    const size_t rowStrideFloats = static_cast<size_t>(texWidth_) * 4u;
    const size_t baseOffset = static_cast<size_t>(row) * rowStrideFloats
                            + static_cast<size_t>(boneIndex) * 16u;
    for (int i = 0; i < 16; ++i) {
        texelData_[baseOffset + static_cast<size_t>(i)] = m.m[i];
    }
}

void BonePaletteAtlas::ResetFrame(int maxBonesPerRow) {
    const int clampedBones = std::max(1, maxBonesPerRow);
    const size_t rowStrideFloats = static_cast<size_t>(clampedBones * 4) * 4u;
    if (clampedBones != maxBonesPerRow_ || texelData_.size() < rowStrideFloats) {
        maxBonesPerRow_ = clampedBones;
        texWidth_ = maxBonesPerRow_ * 4;
        activeRowCount_ = 1; // Row 0 is reserved for Identity bind-pose matrices
        texelData_.assign(rowStrideFloats, 0.0f);
        const Mat4 ident = Mat4::Identity();
        for (int b = 0; b < maxBonesPerRow_; ++b) {
            WriteMatrixAt(0, b, ident);
        }
    } else {
        // Phase 28: Row 0 identity palette is already initialized; O(1) reset!
        activeRowCount_ = 1;
        texelData_.resize(rowStrideFloats);
    }
    dirty_ = true;
}

int BonePaletteAtlas::AllocateAndPackRow(const std::vector<Mat4>& boneMatrices) {
    if (boneMatrices.empty()) {
        return 0;
    }
    const int row = activeRowCount_++;
    const size_t rowStrideFloats = static_cast<size_t>(texWidth_) * 4u;
    texelData_.resize(static_cast<size_t>(activeRowCount_) * rowStrideFloats, 0.0f);

    const int count = std::min(static_cast<int>(boneMatrices.size()), maxBonesPerRow_);
    for (int b = 0; b < count; ++b) {
        WriteMatrixAt(row, b, boneMatrices[static_cast<size_t>(b)]);
    }
    const Mat4 ident = Mat4::Identity();
    for (int b = count; b < maxBonesPerRow_; ++b) {
        WriteMatrixAt(row, b, ident);
    }
    dirty_ = true;
    return row;
}

Mat4 BonePaletteAtlas::SampleMatrix(int row, int boneIndex) const {
    if (row < 0 || row >= activeRowCount_ || boneIndex < 0 || boneIndex >= maxBonesPerRow_) {
        return Mat4::Identity();
    }
    const size_t rowStrideFloats = static_cast<size_t>(texWidth_) * 4u;
    const size_t baseOffset = static_cast<size_t>(row) * rowStrideFloats
                            + static_cast<size_t>(boneIndex) * 16u;
    Mat4 out;
    for (int i = 0; i < 16; ++i) {
        out.m[i] = texelData_[baseOffset + static_cast<size_t>(i)];
    }
    return out;
}

// ============================================================================
// 5. RenderServer Batching, Sorting & Execution
// ============================================================================

uint32_t RenderServer::RegisterMaterial(const std::string& name, const ShaderMaterial& material) {
    auto existing = materialIdsByName_.find(name);
    if (existing != materialIdsByName_.end()) {
        materialsById_[existing->second] = material;
        return existing->second;
    }
    const uint32_t handle = nextMaterialHandle_++;
    materialsById_[handle] = material;
    materialIdsByName_[name] = handle;
    return handle;
}

const ShaderMaterial* RenderServer::GetMaterial(uint32_t materialHandle) const {
    auto it = materialsById_.find(materialHandle);
    return (it != materialsById_.end()) ? &it->second : nullptr;
}

const ShaderMaterial* RenderServer::FindMaterialByName(const std::string& name) const {
    auto it = materialIdsByName_.find(name);
    if (it == materialIdsByName_.end()) return nullptr;
    return GetMaterial(it->second);
}

void RenderServer::BeginFrame(const Mat4& view, const Mat4& proj, const Vec3& cameraPos) {
    view_ = view;
    proj_ = proj;
    viewProj_ = proj * view;
    cameraPos_ = cameraPos;
    frustum_ = Frustum::FromViewProj(viewProj_);

    visibleItems_.clear();
    opaqueBatches_.clear();
    transparentBatches_.clear();
    boneAtlas_.ResetFrame();
    stats_ = RenderFrameStats{};
    stats_.boneAtlasRowsAllocated = boneAtlas_.GetActiveRowCount();
}

void RenderServer::SubmitItem(const RenderItem& item) {
    stats_.submittedItems++;
    if (frustumCullingEnabled_) {
        if (!frustum_.TestSphere(item.worldBoundsCenter, item.worldBoundsRadius)) {
            stats_.culledByFrustum++;
            return;
        }
    }
    stats_.visibleItems++;
    visibleItems_.push_back(item);
}

void RenderServer::SubmitItemsParallel(const std::vector<RenderItem>& items,
                                       core::JobSystem* jobSystem,
                                       int minBatchSize) {
    const int count = static_cast<int>(items.size());
    if (count <= 0) {
        return;
    }
    stats_.submittedItems += count;
    stats_.parallelCulledItems += count;

    if (!frustumCullingEnabled_) {
        stats_.visibleItems += count;
        visibleItems_.insert(visibleItems_.end(), items.begin(), items.end());
        return;
    }

    core::JobSystem& js = jobSystem ? *jobSystem : core::JobSystem::Instance();
    std::vector<uint8_t> visibleFlags(static_cast<size_t>(count), 0u);

    js.ParallelForRange(count, std::max(1, minBatchSize), [&](int startIdx, int endIdx) {
        for (int i = startIdx; i < endIdx; ++i) {
            const RenderItem& it = items[static_cast<size_t>(i)];
            if (frustum_.TestSphere(it.worldBoundsCenter, it.worldBoundsRadius)) {
                visibleFlags[static_cast<size_t>(i)] = 1u;
            }
        }
    });

    visibleItems_.reserve(visibleItems_.size() + static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        if (visibleFlags[static_cast<size_t>(i)] != 0u) {
            stats_.visibleItems++;
            visibleItems_.push_back(items[static_cast<size_t>(i)]);
        } else {
            stats_.culledByFrustum++;
        }
    }
}

void RenderServer::SubmitSimulationUnits(const CSimulation& sim,
                                         const RenderInterpolationBuffer& interpBuffer,
                                         float alpha,
                                         float defaultBoundingRadius) {
    for (const CUnit* unit : sim.GetUnits()) {
        if (!unit) continue;
        const int handleId = unit->GetHandleId();
        RenderTransformSnapshot snap;
        if (!interpBuffer.SampleEntity(handleId, alpha, snap)) {
            continue;
        }
        if (!snap.visible || snap.vertexColor.w <= 0.001f) {
            continue;
        }

        RenderItem item;
        item.entityHandle = handleId;
        item.modelId = snap.unitTypeId;
        item.submeshIndex = 0;

        // Automatically select AlphaBlend pipeline when vertex alpha < 1.0 (e.g. Wind Walk / fade)
        const bool translucentUnit = (snap.vertexColor.w < 0.999f);
        item.material = ShaderMaterial::FromMdxLayer(
            translucentUnit ? 2 : 0,
            0,
            snap.unitTypeId,
            true,
            false
        );

        item.properties.teamColorIndex = snap.playerIndex;
        item.properties.vertexTint = snap.vertexColor;
        item.properties.animationFrame = unit->VisualComp().timeScale;

        item.worldMatrix = MakeSceneInstanceMatrix(
            snap.position.x,
            snap.position.y,
            snap.position.z,
            snap.facingDeg,
            snap.scale.x,
            snap.scale.y,
            snap.scale.z
        );
        item.worldBoundsCenter = snap.position;
        item.worldBoundsRadius = defaultBoundingRadius * std::max({snap.scale.x, snap.scale.y, snap.scale.z});

        SubmitItem(item);
    }
}

void RenderServer::BuildAndSortBatches() {
    opaqueBatches_.clear();
    transparentBatches_.clear();

    std::unordered_map<InstanceBatchKey, size_t, InstanceBatchKeyHasher> opaqueMap;
    std::unordered_map<InstanceBatchKey, size_t, InstanceBatchKeyHasher> transparentMap;

    for (const RenderItem& item : visibleItems_) {
        const uint64_t matKey = item.material.ComputePipelineKey();
        const InstanceBatchKey batchKey{item.modelId, item.submeshIndex, matKey};

        const Vec3 diff = item.worldBoundsCenter - cameraPos_;
        const float distSq = Dot(diff, diff);

        InstanceGpuPayload payload;
        payload.worldMatrix = item.worldMatrix;
        payload.vertexTint = item.properties.vertexTint;
        payload.instanceParams = Vec4(
            static_cast<float>(item.properties.teamColorIndex),
            0.0f, // boneAtlasRow populated during hardware instanced batch execution
            item.properties.animationFrame,
            item.properties.emissiveGain
        );

        if (!item.material.IsTransparent()) {
            auto it = opaqueMap.find(batchKey);
            if (it == opaqueMap.end()) {
                size_t idx = opaqueBatches_.size();
                opaqueMap[batchKey] = idx;
                RenderBatchGroup group;
                group.key = batchKey;
                group.material = item.material;
                group.sortDepthSq = distSq;
                group.instances.push_back(payload);
                group.entityHandles.push_back(item.entityHandle);
                opaqueBatches_.push_back(std::move(group));
            } else {
                RenderBatchGroup& group = opaqueBatches_[it->second];
                group.sortDepthSq = std::min(group.sortDepthSq, distSq);
                group.instances.push_back(payload);
                group.entityHandles.push_back(item.entityHandle);
            }
        } else {
            auto it = transparentMap.find(batchKey);
            if (it == transparentMap.end()) {
                size_t idx = transparentBatches_.size();
                transparentMap[batchKey] = idx;
                RenderBatchGroup group;
                group.key = batchKey;
                group.material = item.material;
                group.sortDepthSq = distSq;
                group.instances.push_back(payload);
                group.entityHandles.push_back(item.entityHandle);
                transparentBatches_.push_back(std::move(group));
            } else {
                RenderBatchGroup& group = transparentBatches_[it->second];
                group.sortDepthSq = std::max(group.sortDepthSq, distSq);
                group.instances.push_back(payload);
                group.entityHandles.push_back(item.entityHandle);
            }
        }
    }

    // Opaque: sort primarily by MaterialPipelineKey (to minimize shader/pipeline switches),
    // then front-to-back (nearest depth first to maximize early-Z rejection)
    std::sort(opaqueBatches_.begin(), opaqueBatches_.end(),
              [](const RenderBatchGroup& a, const RenderBatchGroup& b) {
                  if (a.key.materialKey != b.key.materialKey) {
                      return a.key.materialKey < b.key.materialKey;
                  }
                  if (std::abs(a.sortDepthSq - b.sortDepthSq) > 1e-3f) {
                      return a.sortDepthSq < b.sortDepthSq;
                  }
                  return a.key.modelId < b.key.modelId;
              });

    // Transparent: sort strictly back-to-front (furthest depth first) for proper blending,
    // breaking ties by MaterialPipelineKey
    std::sort(transparentBatches_.begin(), transparentBatches_.end(),
              [](const RenderBatchGroup& a, const RenderBatchGroup& b) {
                  if (std::abs(a.sortDepthSq - b.sortDepthSq) > 1e-3f) {
                      return a.sortDepthSq > b.sortDepthSq;
                  }
                  return a.key.materialKey < b.key.materialKey;
              });

    stats_.opaqueBatchCount = static_cast<int>(opaqueBatches_.size());
    stats_.transparentBatchCount = static_cast<int>(transparentBatches_.size());
    stats_.totalDrawCalls = stats_.opaqueBatchCount + stats_.transparentBatchCount;
    stats_.instancedDrawCallsSaved = std::max(0, stats_.visibleItems - stats_.totalDrawCalls);

    uint64_t lastPipelineKey = 0;
    bool firstBatch = true;
    auto trackSwitch = [&](const RenderBatchGroup& batch) {
        if (firstBatch || batch.key.materialKey != lastPipelineKey) {
            stats_.pipelineStateSwitches++;
            lastPipelineKey = batch.key.materialKey;
            firstBatch = false;
        }
    };
    for (const auto& b : opaqueBatches_) trackSwitch(b);
    for (const auto& b : transparentBatches_) trackSwitch(b);
}

void RenderServer::ExecuteBatches(const std::function<void(const RenderBatchGroup&)>& drawCallback) {
    RhiDevice& rhi = RhiDevice::Instance();
    const bool recordHeadlessDraws = rhi.IsHeadlessMockEnabled() && !rhi.IsGpuActive();
    for (const auto& batch : opaqueBatches_) {
        if (drawCallback) {
            drawCallback(batch);
        } else if (recordHeadlessDraws && !batch.instances.empty()) {
            rhi.Draw(0, 36, static_cast<int>(batch.instances.size()));
        }
    }
    for (const auto& batch : transparentBatches_) {
        if (drawCallback) {
            drawCallback(batch);
        } else if (recordHeadlessDraws && !batch.instances.empty()) {
            rhi.Draw(0, 36, static_cast<int>(batch.instances.size()));
        }
    }
    stats_.boneAtlasRowsAllocated = boneAtlas_.GetActiveRowCount();
}

void RenderServer::RecordHardwareInstancedSubmission(int drawCalls, int instanceCount) {
    stats_.hardwareInstancedDrawCalls += std::max(0, drawCalls);
    stats_.hardwareInstancesSubmitted += std::max(0, instanceCount);
    stats_.boneAtlasRowsAllocated = boneAtlas_.GetActiveRowCount();
}


// ============================================================================
// 6. StaticSpatialChunkGrid Implementation (Stage H)
// ============================================================================

void StaticSpatialChunkGrid::Clear(float chunkSize) {
    if (chunkSize > 1.0f) {
        chunkSize_ = chunkSize;
    }
    totalStaticItems_ = 0;
    chunks_.clear();
    chunkIndexByKey_.clear();
    slotByEntityHandle_.clear();
}

void StaticSpatialChunkGrid::InsertStaticItem(const RenderItem& item, bool visible) {
    const int cx = static_cast<int>(std::floor(item.worldBoundsCenter.x / chunkSize_));
    const int cz = static_cast<int>(std::floor(item.worldBoundsCenter.z / chunkSize_));
    const uint64_t key = PackChunkCoord(cx, cz);

    const float r = std::max(1.0f, item.worldBoundsRadius);
    const Vec3 itemMin(
        item.worldBoundsCenter.x - r,
        item.worldBoundsCenter.y - r,
        item.worldBoundsCenter.z - r
    );
    const Vec3 itemMax(
        item.worldBoundsCenter.x + r,
        item.worldBoundsCenter.y + r,
        item.worldBoundsCenter.z + r
    );

    size_t chunkIdx = 0;
    auto it = chunkIndexByKey_.find(key);
    if (it == chunkIndexByKey_.end()) {
        chunkIdx = chunks_.size();
        chunkIndexByKey_[key] = chunkIdx;
        StaticSpatialChunk chunk;
        chunk.chunkX = cx;
        chunk.chunkZ = cz;
        chunk.aabbMin = itemMin;
        chunk.aabbMax = itemMax;
        chunks_.push_back(std::move(chunk));
    } else {
        chunkIdx = it->second;
        StaticSpatialChunk& chunk = chunks_[chunkIdx];
        chunk.aabbMin.x = std::min(chunk.aabbMin.x, itemMin.x);
        chunk.aabbMin.y = std::min(chunk.aabbMin.y, itemMin.y);
        chunk.aabbMin.z = std::min(chunk.aabbMin.z, itemMin.z);
        chunk.aabbMax.x = std::max(chunk.aabbMax.x, itemMax.x);
        chunk.aabbMax.y = std::max(chunk.aabbMax.y, itemMax.y);
        chunk.aabbMax.z = std::max(chunk.aabbMax.z, itemMax.z);
    }

    StaticSpatialChunk& chunk = chunks_[chunkIdx];
    const size_t itemIdx = chunk.items.size();
    chunk.items.push_back(item);
    chunk.itemVisible.push_back(visible ? 1u : 0u);
    slotByEntityHandle_[item.entityHandle] = ItemSlotRef{chunkIdx, itemIdx};
    ++totalStaticItems_;
}

bool StaticSpatialChunkGrid::SetStaticItemVisible(int entityHandle, bool visible) {
    auto it = slotByEntityHandle_.find(entityHandle);
    if (it == slotByEntityHandle_.end()) {
        return false;
    }
    StaticSpatialChunk& chunk = chunks_[it->second.chunkIndex];
    chunk.itemVisible[it->second.itemIndex] = visible ? 1u : 0u;
    return true;
}

bool StaticSpatialChunkGrid::UpdateStaticItemTint(int entityHandle,
                                                  const Vec4& vertexTint,
                                                  bool isTranslucent) {
    auto it = slotByEntityHandle_.find(entityHandle);
    if (it == slotByEntityHandle_.end()) {
        return false;
    }
    StaticSpatialChunk& chunk = chunks_[it->second.chunkIndex];
    RenderItem& item = chunk.items[it->second.itemIndex];
    item.properties.vertexTint = vertexTint;
    if (isTranslucent && !item.material.IsTransparent()) {
        item.material.blendMode = RenderBlendMode::AlphaBlend;
        item.material.depthState = RenderDepthState::ReadOnly;
    }
    return true;
}

void StaticSpatialChunkGrid::GatherVisibleStaticItems(
    const Frustum& frustum,
    bool frustumCullingEnabled,
    std::vector<RenderItem>& outVisibleItems,
    RenderFrameStats& inoutStats) const {
    inoutStats.totalStaticChunks += static_cast<int>(chunks_.size());

    for (const StaticSpatialChunk& chunk : chunks_) {
        int activeInChunk = 0;
        for (size_t i = 0; i < chunk.items.size(); ++i) {
            if (chunk.itemVisible[i] != 0u) {
                ++activeInChunk;
            }
        }
        if (activeInChunk == 0) {
            continue;
        }
        inoutStats.submittedItems += activeInChunk;

        if (!frustumCullingEnabled) {
            inoutStats.visibleChunks++;
            inoutStats.visibleItems += activeInChunk;
            for (size_t i = 0; i < chunk.items.size(); ++i) {
                if (chunk.itemVisible[i] != 0u) {
                    outVisibleItems.push_back(chunk.items[i]);
                }
            }
            continue;
        }

        const Frustum::Containment containment = frustum.ClassifyAABB(chunk.aabbMin, chunk.aabbMax);
        if (containment == Frustum::Containment::Outside) {
            inoutStats.culledChunks++;
            inoutStats.culledByChunkHierarchy += activeInChunk;
            inoutStats.culledByFrustum += activeInChunk;
            continue;
        }

        inoutStats.visibleChunks++;
        if (containment == Frustum::Containment::Inside) {
            inoutStats.fastAcceptedByChunkInside += activeInChunk;
            inoutStats.visibleItems += activeInChunk;
            for (size_t i = 0; i < chunk.items.size(); ++i) {
                if (chunk.itemVisible[i] != 0u) {
                    outVisibleItems.push_back(chunk.items[i]);
                }
            }
        } else {
            // Intersecting boundary chunk: test individual items inside this chunk
            for (size_t i = 0; i < chunk.items.size(); ++i) {
                if (chunk.itemVisible[i] == 0u) {
                    continue;
                }
                const RenderItem& item = chunk.items[i];
                if (frustum.TestSphere(item.worldBoundsCenter, item.worldBoundsRadius)) {
                    inoutStats.visibleItems++;
                    outVisibleItems.push_back(item);
                } else {
                    inoutStats.culledByFrustum++;
                }
            }
        }
    }
}

void RenderServer::SubmitStaticChunkGrid(const StaticSpatialChunkGrid& chunkGrid) {
    chunkGrid.GatherVisibleStaticItems(frustum_, frustumCullingEnabled_, visibleItems_, stats_);
}

} // namespace render
