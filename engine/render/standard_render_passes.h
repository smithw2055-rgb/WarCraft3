#pragma once
// ============================================================================
// Engine Render: StandardRenderPassPipeline, Incremental TerrainOverlayState &
//                DiscreteStepTimelineSampler
//
// 100% Game-Agnostic Render Pipeline Components:
// 1. Zero game-specific symbols or Warcraft terminology in `engine/`.
// 2. `TerrainOverlayState` maintains a persistent RGBA8 overlay buffer and
//    updates ONLY the union of dirty rectangles (`shadowDirty_` U `maskDirty_`)
//    with incremental GPU sub-region upload via `IRhiBackend::UpdateTexture2D`.
// 3. `DiscreteStepTimelineSampler` evaluates discrete step keyframes using
//    truncation (`static_cast<int>`) rather than rounding (`lround`), preventing
//    half-frame flipbook/texture-slot jitter on animated material tracks.
// ============================================================================
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "engine/render/rhi_backend.h"

namespace engine::render {

struct GridRect {
    int minX = 0;
    int minY = 0;
    int maxX = -1; // Inclusive; maxX < minX means empty
    int maxY = -1;

    constexpr bool IsEmpty() const noexcept {
        return maxX < minX || maxY < minY;
    }

    constexpr int Width() const noexcept {
        return IsEmpty() ? 0 : (maxX - minX + 1);
    }

    constexpr int Height() const noexcept {
        return IsEmpty() ? 0 : (maxY - minY + 1);
    }

    void Reset() noexcept {
        minX = 0;
        minY = 0;
        maxX = -1;
        maxY = -1;
    }

    void ExpandToInclude(int x, int y) noexcept {
        if (IsEmpty()) {
            minX = maxX = x;
            minY = maxY = y;
        } else {
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }

    void UnionWith(const GridRect& other) noexcept {
        if (other.IsEmpty()) return;
        if (IsEmpty()) {
            *this = other;
        } else {
            minX = std::min(minX, other.minX);
            minY = std::min(minY, other.minY);
            maxX = std::max(maxX, other.maxX);
            maxY = std::max(maxY, other.maxY);
        }
    }

    GridRect Clamped(int gridW, int gridH) const noexcept {
        if (IsEmpty() || gridW <= 0 || gridH <= 0) {
            return GridRect{};
        }
        GridRect out;
        out.minX = std::clamp(minX, 0, gridW - 1);
        out.minY = std::clamp(minY, 0, gridH - 1);
        out.maxX = std::clamp(maxX, 0, gridW - 1);
        out.maxY = std::clamp(maxY, 0, gridH - 1);
        if (out.maxX < out.minX || out.maxY < out.minY) {
            return GridRect{};
        }
        return out;
    }
};

struct ShadowFootprint {
    int id = 0;
    int centerX = 0;
    int centerY = 0;
    int halfExtentX = 2;
    int halfExtentY = 2;
    uint8_t intensity = 180;
};

// Persistent terrain overlay grid (R = shadow intensity, G = Secondary terrain mask,
// B = Fog visibility, A = 255) with O(dirty_area) incremental baking!
class TerrainOverlayState {
public:
    TerrainOverlayState() = default;
    TerrainOverlayState(int width, int height) {
        Resize(width, height);
    }

    void Resize(int width, int height) {
        width_ = std::max(0, width);
        height_ = std::max(0, height);
        const size_t cells = static_cast<size_t>(width_) * static_cast<size_t>(height_);
        secondaryMask_.assign(cells, 0u);
        fogVisibility_.assign(cells, 255u);
        rgbaBuffer_.assign(cells * 4u, 0u);
        for (size_t i = 0; i < cells; ++i) {
            rgbaBuffer_[i * 4u + 2u] = 255u;
            rgbaBuffer_[i * 4u + 3u] = 255u;
        }
        shadows_.clear();
        if (width_ > 0 && height_ > 0) {
            maskDirty_ = GridRect{0, 0, width_ - 1, height_ - 1};
        } else {
            maskDirty_.Reset();
        }
        shadowDirty_.Reset();
    }

    int Width() const { return width_; }
    int Height() const { return height_; }

    void AddShadowFootprint(const ShadowFootprint& fp) {
        shadows_.push_back(fp);
        MarkShadowBoundsDirty(fp);
    }

    bool RemoveShadowFootprint(int id) {
        for (size_t i = 0; i < shadows_.size(); ++i) {
            if (shadows_[i].id == id) {
                MarkShadowBoundsDirty(shadows_[i]);
                shadows_.erase(shadows_.begin() + static_cast<std::ptrdiff_t>(i));
                return true;
            }
        }
        return false;
    }

    void ClearShadowFootprints() {
        for (const auto& s : shadows_) {
            MarkShadowBoundsDirty(s);
        }
        shadows_.clear();
    }

    void SetMaskCircle(int cx, int cy, int radius, bool active) {
        if (width_ <= 0 || height_ <= 0 || radius < 0) return;
        const int x0 = std::max(0, cx - radius);
        const int y0 = std::max(0, cy - radius);
        const int x1 = std::min(width_ - 1, cx + radius);
        const int y1 = std::min(height_ - 1, cy + radius);
        const int r2 = radius * radius;
        const uint8_t target = active ? 255u : 0u;

        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                const int dx = x - cx;
                const int dy = y - cy;
                if (dx * dx + dy * dy <= r2) {
                    const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x);
                    if (secondaryMask_[idx] != target) {
                        secondaryMask_[idx] = target;
                        maskDirty_.ExpandToInclude(x, y);
                    }
                }
            }
        }
    }

    bool IsMaskActive(int x, int y) const {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return false;
        return secondaryMask_[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)] > 0u;
    }

    void SetFogVisibilityCell(int x, int y, uint8_t visibility) {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
        const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x);
        if (fogVisibility_[idx] != visibility) {
            fogVisibility_[idx] = visibility;
            maskDirty_.ExpandToInclude(x, y);
        }
    }

    bool HasDirtyRegions() const {
        return !shadowDirty_.IsEmpty() || !maskDirty_.IsEmpty();
    }

    // Recomputes ONLY the dirty bounding rectangle and writes the updated sub-region
    // into `outSubRectPixels` (tightly packed `rect.Width() * rect.Height() * 4` RGBA8).
    GridRect SyncDirtyOverlayRgba8(std::vector<uint8_t>* outSubRectPixels = nullptr) {
        GridRect dirty = shadowDirty_;
        dirty.UnionWith(maskDirty_);
        dirty = dirty.Clamped(width_, height_);
        shadowDirty_.Reset();
        maskDirty_.Reset();

        if (dirty.IsEmpty()) {
            if (outSubRectPixels) outSubRectPixels->clear();
            return GridRect{};
        }

        // Gather only shadows overlapping `dirty` (further pruning O(N_overlapping))
        std::vector<const ShadowFootprint*> overlappingShadows;
        for (const auto& s : shadows_) {
            const int sx0 = s.centerX - s.halfExtentX;
            const int sy0 = s.centerY - s.halfExtentY;
            const int sx1 = s.centerX + s.halfExtentX;
            const int sy1 = s.centerY + s.halfExtentY;
            if (sx1 >= dirty.minX && sx0 <= dirty.maxX &&
                sy1 >= dirty.minY && sy0 <= dirty.maxY) {
                overlappingShadows.push_back(&s);
            }
        }

        const int subW = dirty.Width();
        const int subH = dirty.Height();
        if (outSubRectPixels) {
            outSubRectPixels->resize(static_cast<size_t>(subW) * static_cast<size_t>(subH) * 4u);
        }

        for (int y = dirty.minY; y <= dirty.maxY; ++y) {
            for (int x = dirty.minX; x <= dirty.maxX; ++x) {
                uint8_t shadowVal = 0u;
                for (const auto* s : overlappingShadows) {
                    if (std::abs(x - s->centerX) <= s->halfExtentX &&
                        std::abs(y - s->centerY) <= s->halfExtentY) {
                        shadowVal = std::max(shadowVal, s->intensity);
                    }
                }
                const size_t cellIdx = static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x);
                const size_t dstOff = cellIdx * 4u;
                rgbaBuffer_[dstOff + 0u] = shadowVal;
                rgbaBuffer_[dstOff + 1u] = secondaryMask_[cellIdx];
                rgbaBuffer_[dstOff + 2u] = fogVisibility_[cellIdx];
                rgbaBuffer_[dstOff + 3u] = 255u;

                if (outSubRectPixels) {
                    const size_t subIdx =
                        (static_cast<size_t>(y - dirty.minY) * static_cast<size_t>(subW) +
                         static_cast<size_t>(x - dirty.minX)) * 4u;
                    (*outSubRectPixels)[subIdx + 0u] = rgbaBuffer_[dstOff + 0u];
                    (*outSubRectPixels)[subIdx + 1u] = rgbaBuffer_[dstOff + 1u];
                    (*outSubRectPixels)[subIdx + 2u] = rgbaBuffer_[dstOff + 2u];
                    (*outSubRectPixels)[subIdx + 3u] = 255u;
                }
            }
        }

        ++incrementalSyncCount_;
        lastSyncedPixels_ = static_cast<size_t>(subW) * static_cast<size_t>(subH);
        return dirty;
    }

    const std::vector<uint8_t>& RgbaBuffer() const { return rgbaBuffer_; }
    uint64_t IncrementalSyncCount() const { return incrementalSyncCount_; }
    size_t LastSyncedPixelCount() const { return lastSyncedPixels_; }

private:
    void MarkShadowBoundsDirty(const ShadowFootprint& fp) {
        shadowDirty_.ExpandToInclude(fp.centerX - fp.halfExtentX, fp.centerY - fp.halfExtentY);
        shadowDirty_.ExpandToInclude(fp.centerX + fp.halfExtentX, fp.centerY + fp.halfExtentY);
    }

    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> secondaryMask_;
    std::vector<uint8_t> fogVisibility_;
    std::vector<uint8_t> rgbaBuffer_;
    std::vector<ShadowFootprint> shadows_;
    GridRect shadowDirty_;
    GridRect maskDirty_;
    uint64_t incrementalSyncCount_ = 0;
    size_t lastSyncedPixels_ = 0;
};

// Generic discrete step keyframe sampler (holds previous keyframe value until exact
// next keyframe time and truncates to integer slot index without half-way rounding).
struct DiscreteStepKeyframe {
    int32_t timeMs = 0;
    int32_t value = 0;
};

class DiscreteStepTimelineSampler {
public:
    static int32_t SampleStepInt(const DiscreteStepKeyframe* keys,
                                 size_t keyCount,
                                 int32_t animTimeMs,
                                 int32_t seqStartMs,
                                 int32_t seqEndMs,
                                 int32_t fallbackValue) noexcept {
        if (!keys || keyCount == 0) {
            return fallbackValue;
        }
        bool foundInRange = false;
        int32_t heldValue = fallbackValue;
        for (size_t i = 0; i < keyCount; ++i) {
            const int32_t t = keys[i].timeMs;
            if (t < seqStartMs || t > seqEndMs) {
                continue;
            }
            if (t <= animTimeMs) {
                heldValue = keys[i].value;
                foundInRange = true;
            } else {
                if (!foundInRange) {
                    heldValue = keys[i].value;
                    foundInRange = true;
                }
                break;
            }
        }
        return foundInRange ? heldValue : fallbackValue;
    }

    static int TruncateFloatTrackToIndex(float sampledScalar) noexcept {
        return static_cast<int>(sampledScalar);
    }
};

enum class StandardPassKind : uint8_t {
    OverlaySync = 0,
    TerrainOpaque = 1,
    StaticPropsOpaque = 2,
    SkinnedUnitsOpaque = 3,
    WaterTranslucent = 4,
    TransparentVfx = 5,
    UiCanvas = 6
};

struct StandardPassContext {
    IRhiBackend* rhi = nullptr;
    TerrainOverlayState* overlay = nullptr;
    uint32_t overlayTextureHandle = 0;
    int viewportWidth = 1280;
    int viewportHeight = 720;
};

class StandardRenderPassPipeline {
public:
    using PassExecuteFn = std::function<void(StandardPassContext& ctx)>;

    struct PassNode {
        StandardPassKind kind = StandardPassKind::TerrainOpaque;
        std::string name;
        bool enabled = true;
        PassExecuteFn execute;
    };

    void AddPass(StandardPassKind kind, std::string name, PassExecuteFn fn) {
        passes_.push_back({kind, std::move(name), true, std::move(fn)});
    }

    void ExecuteFrame(StandardPassContext& ctx) {
        if (ctx.rhi) {
            ctx.rhi->BeginPass(0.08f, 0.10f, 0.14f, 1.0f);
            ctx.rhi->SetViewport(0, 0, ctx.viewportWidth, ctx.viewportHeight);
            ctx.rhi->SetScissor(0, 0, ctx.viewportWidth, ctx.viewportHeight);
        }
        for (auto& p : passes_) {
            if (p.enabled && p.execute) {
                p.execute(ctx);
                ++passesExecuted_;
            }
        }
        if (ctx.rhi) {
            ctx.rhi->EndPass();
        }
    }

    // Uploads ONLY the dirty sub-rectangle to the GPU overlay texture!
    static void ExecuteIncrementalTerrainOverlaySyncPass(StandardPassContext& ctx) {
        if (!ctx.overlay || !ctx.overlay->HasDirtyRegions()) return;
        std::vector<uint8_t> subPixels;
        const GridRect dirty = ctx.overlay->SyncDirtyOverlayRgba8(&subPixels);
        if (!dirty.IsEmpty() && ctx.rhi && ctx.overlayTextureHandle != 0 && !subPixels.empty()) {
            ctx.rhi->UpdateTexture2D(
                ctx.overlayTextureHandle,
                static_cast<uint32_t>(dirty.minX),
                static_cast<uint32_t>(dirty.minY),
                static_cast<uint32_t>(dirty.Width()),
                static_cast<uint32_t>(dirty.Height()),
                subPixels.data(),
                subPixels.size());
        }
    }

    static StandardRenderPassPipeline CreateStandardRtsPipeline() {
        StandardRenderPassPipeline pipe;
        pipe.AddPass(StandardPassKind::OverlaySync, "OverlaySync",
                     &ExecuteIncrementalTerrainOverlaySyncPass);
        pipe.AddPass(StandardPassKind::TerrainOpaque, "TerrainOpaque", [](StandardPassContext&) {});
        pipe.AddPass(StandardPassKind::StaticPropsOpaque, "StaticPropsOpaque", [](StandardPassContext&) {});
        pipe.AddPass(StandardPassKind::SkinnedUnitsOpaque, "SkinnedUnitsOpaque", [](StandardPassContext&) {});
        pipe.AddPass(StandardPassKind::WaterTranslucent, "WaterTranslucent", [](StandardPassContext&) {});
        pipe.AddPass(StandardPassKind::TransparentVfx, "TransparentVfx", [](StandardPassContext&) {});
        pipe.AddPass(StandardPassKind::UiCanvas, "UiCanvas", [](StandardPassContext&) {});
        return pipe;
    }

    size_t PassCount() const { return passes_.size(); }
    const std::vector<PassNode>& Passes() const { return passes_; }
    uint64_t PassesExecutedCount() const { return passesExecuted_; }

private:
    std::vector<PassNode> passes_;
    uint64_t passesExecuted_ = 0;
};

} // namespace engine::render
