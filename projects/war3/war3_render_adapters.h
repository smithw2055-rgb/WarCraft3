#pragma once
// ============================================================================
// WarCraft III Project Plugin: War3 Render & Material Layer Adapters
//
// Houses Warcraft III-specific replaceable texture IDs (TeamColor, TeamGlow,
// Cliff, LordaeronTree) and MDX KMTF material layer sampling on top of the
// game-agnostic `engine::render::DiscreteStepTimelineSampler`.
// ============================================================================
#include <algorithm>
#include <cstdint>
#include <vector>

#include "engine/render/standard_render_passes.h"

namespace projects::war3 {

enum class War3ReplaceableId : uint32_t {
    None          = 0,
    TeamColor     = 1,
    TeamGlow      = 2,
    Cliff         = 11,
    LordaeronTree = 31,
    AshenvaleTree = 32,
    BarrensTree   = 33,
    NorthrendTree = 34,
    MushroomTree  = 35
};

struct War3MdxMaterialLayer {
    int32_t baseTextureSlot = 0;
    War3ReplaceableId replaceableId = War3ReplaceableId::None;
    std::vector<engine::render::DiscreteStepKeyframe> kmtfKeys;
};

class War3MdxLayerSampler {
public:
    static int32_t EvaluateTextureSlot(const War3MdxMaterialLayer& layer,
                                       int32_t animTimeMs,
                                       int32_t seqStartMs,
                                       int32_t seqEndMs) noexcept {
        return engine::render::DiscreteStepTimelineSampler::SampleStepInt(
            layer.kmtfKeys.data(),
            layer.kmtfKeys.size(),
            animTimeMs,
            seqStartMs,
            seqEndMs,
            layer.baseTextureSlot);
    }

    static uint32_t ResolveEffectiveTextureHandle(const War3MdxMaterialLayer& layer,
                                                  int32_t animTimeMs,
                                                  int32_t seqStartMs,
                                                  int32_t seqEndMs,
                                                  const std::vector<uint32_t>& modelTextureHandles,
                                                  uint32_t teamColorTexHandle,
                                                  uint32_t teamGlowTexHandle,
                                                  uint32_t destructableTreeTexHandle = 0) noexcept {
        if (layer.replaceableId == War3ReplaceableId::TeamColor && teamColorTexHandle != 0) {
            return teamColorTexHandle;
        }
        if (layer.replaceableId == War3ReplaceableId::TeamGlow && teamGlowTexHandle != 0) {
            return teamGlowTexHandle;
        }
        if (layer.replaceableId >= War3ReplaceableId::LordaeronTree &&
            layer.replaceableId <= War3ReplaceableId::MushroomTree &&
            destructableTreeTexHandle != 0) {
            return destructableTreeTexHandle;
        }
        const int32_t slot = EvaluateTextureSlot(layer, animTimeMs, seqStartMs, seqEndMs);
        if (slot >= 0 && static_cast<size_t>(slot) < modelTextureHandles.size()) {
            return modelTextureHandles[static_cast<size_t>(slot)];
        }
        return 0u;
    }
};

inline void ApplyWar3BlightCircle(engine::render::TerrainOverlayState& overlay,
                                  int cx,
                                  int cy,
                                  int radius,
                                  bool blighted) {
    overlay.SetMaskCircle(cx, cy, radius, blighted);
}

} // namespace projects::war3
