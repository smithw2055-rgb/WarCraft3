#pragma once
#include "resource/mpq_storm.h"
#include "resource/slk.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "render/textured_terrain.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct SplatMetadataStore {
    struct UnitRenderSplatMeta {
        float selectionScale = 0.0f;
        float selectionHeight = 0.0f;
        float animationWalkSpeed = 0.0f;
        float animationRunSpeed = 0.0f;
        std::string uberSplatPath;
        float uberSplatScale = 0.0f;
        std::string buildingShadow;
    };

    struct SelectionCircleVisual {
        float size = 0.0f;
        std::string texturePath;
        std::string dottedTexturePath;
    };

    std::unordered_map<uint32_t, UnitRenderSplatMeta> unitRenderSplatMeta;
    std::vector<SelectionCircleVisual> selectionCircleVisuals;
    float selectionCircleScaleFactor = 1.0f;

    // Load metadata from MPQ. No-op when archive count is zero.
    void Load(MpqManager& mpq);

    const UnitRenderSplatMeta* GetUnitRenderSplatMeta(const CUnit& unit) const;

    float ComputeSelectionCircleSizeWorld(const CUnit& unit,
                                          const std::function<float(const CUnit&)>& fallbackRadiusWorld) const;

    std::string PickSelectionCircleTexturePath(float selectionSizeWorld) const;

    // Refresh terrain runtime splats (selection circles + building ubersplats) for one frame.
    // `selectedHandles` provides the set of currently selected unit handle IDs.
    void RefreshRuntimeSplats(TexturedTerrain& terrain,
                              bool simulationInitialized,
                              CSimulation& simulation,
                              const std::unordered_set<int>& selectedHandles) const;
};