#pragma once
// ============================================================================
// Stage O1: WorldSceneSession — Hot Map Switching & Scene Lifecycle Manager
//           (Inspired by Godot SceneTree::change_scene_to_file & Unity SceneManager)
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/unit_presentation_system.h"
#include "ecs/csimulation.h"
#include "ecs/hero_carry.h"
#include "map/unitsdoo.h"
#include "map/w3d.h"
#include "map/w3e.h"
#include "map/w3i.h"
#include "map/wpm.h"
#include "render/render_graph.h"
#include "render/scene.h"
#include "resource/mpq_storm.h"

namespace core {

struct WorldSceneSessionStats {
    uint32_t sessionGeneration = 0;
    uint32_t mapsLoaded = 0;
    uint32_t hotSwitchesPerformed = 0;
    size_t placedDoodadsRegistered = 0;
    size_t placedUnitsRead = 0;
    size_t terrainChunksBuilt = 0;
    size_t carriedHeroesCaptured = 0;
    size_t carriedHeroesRespawned = 0;
};

class WorldSceneSession {
public:
    WorldSceneSession() = default;

    // Load a map archive (.w3x / .w3m) into the active session, populating
    // terrain chunks, pathing grid, static doodad spatial chunks, and players.
    bool LoadMap(MpqManager& mpq,
                 CSimulation& sim,
                 SceneManager& scene,
                 UnitPresentationSystem& presentation,
                 const std::string& mapPath);

    // Perform a runtime hot map switch (e.g., ChangeLevel campaign transition
    // or main-menu map launch), optionally carrying over human heroes and
    // cleanly tearing down previous scene/presentation/ECS resources.
    bool HotSwitchMap(MpqManager& mpq,
                      CSimulation& sim,
                      SceneManager& scene,
                      UnitPresentationSystem& presentation,
                      const std::string& nextMapPath,
                      bool carryHumanHeroes = true);

    void TeardownCurrentSession(MpqManager& mpq,
                                CSimulation& sim,
                                SceneManager& scene,
                                UnitPresentationSystem& presentation);

    const std::string& GetCurrentMapPath() const { return currentMapPath_; }
    const W3EMap& GetW3E() const { return w3e_; }
    const W3iInfo& GetW3I() const { return w3i_; }
    const WpmData& GetWpm() const { return wpm_; }
    const UnitsDooData& GetUnitsDoo() const { return unitsDoo_; }
    const W3dData& GetDoodadsDoo() const { return doodadsDoo_; }
    const render::TerrainChunkGrid& GetTerrainChunkGrid() const { return terrainChunks_; }
    const std::vector<CarriedHero>& GetCarriedHeroes() const { return carriedHeroes_; }
    const WorldSceneSessionStats& GetStats() const { return stats_; }

private:
    std::string currentMapPath_;
    W3EMap w3e_{};
    W3iInfo w3i_{};
    WpmData wpm_{};
    UnitsDooData unitsDoo_{};
    W3dData doodadsDoo_{};
    render::TerrainChunkGrid terrainChunks_;
    std::vector<CarriedHero> carriedHeroes_;
    WorldSceneSessionStats stats_{};
};

} // namespace core
