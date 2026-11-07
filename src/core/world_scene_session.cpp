#include "core/world_scene_session.h"

#include "ecs/player.h"
#include "ecs/unit.h"

namespace core {

void WorldSceneSession::TeardownCurrentSession(MpqManager& mpq,
                                               CSimulation& sim,
                                               SceneManager& scene,
                                               UnitPresentationSystem& presentation) {
    scene.SetBoundTerrainChunkGrid(nullptr);
    scene.Clear();
    presentation.ClearAll();

    // Remove all active simulation units and clear ECS component pools
    std::vector<CUnit*> toRemove = sim.GetUnits();
    for (CUnit* u : toRemove) {
        if (u) sim.RemoveUnit(u);
    }
    sim.Update();
    sim.Registry().Clear();
    sim.SpatialGrid().Clear();
    sim.InvalidateNavigationCaches();

    // Reclaim unreferenced resources in ResourceServer upon map teardown
    stats_.resourcesPrunedOnTeardown += scene.GetResourceServer().PruneUnusedResources();

    mpq.CloseTop();

    w3e_ = W3EMap{};
    w3i_ = W3iInfo{};
    wpm_ = WpmData{};
    unitsDoo_ = UnitsDooData{};
    doodadsDoo_ = W3dData{};
}

bool WorldSceneSession::LoadMap(MpqManager& mpq,
                                CSimulation& sim,
                                SceneManager& scene,
                                UnitPresentationSystem& presentation,
                                const std::string& mapPath) {
    if (mapPath.empty() || !mpq.OpenOnTop(mapPath)) {
        return false;
    }

    scene.GetResourceServer().SetMpqManager(&mpq);
    currentMapPath_ = mapPath;
    stats_.sessionGeneration++;
    stats_.mapsLoaded++;

    // 1. Load W3E terrain & build TerrainChunkGrid
    std::vector<uint8_t> w3eBytes;
    if (mpq.ReadFile("war3map.w3e", w3eBytes) && !w3eBytes.empty()) {
        if (W3ELoad(w3eBytes.data(), w3eBytes.size(), w3e_)) {
            terrainChunks_.BuildFromW3E(w3e_);
            stats_.terrainChunksBuilt = terrainChunks_.GetChunkCount();
            sim.SetTerrainMap(&w3e_);
            scene.SetBoundTerrainChunkGrid(&terrainChunks_);
        }
    }

    // 2. Load W3I map info & ensure players exist
    if (W3ILoadFromMpq(mapPath, w3i_)) {
        if (sim.GetPlayerCount() == 0) {
            for (int i = 0; i < 12; ++i) {
                sim.AddPlayer(std::make_unique<CPlayer>(i, "Player" + std::to_string(i), CMapControl::USER));
            }
        }
        ApplyW3iTechtreeAvailability(sim, w3i_);
    }

    // 3. Load WPM pathing grid
    if (WpmLoadFromMpq(mapPath, wpm_)) {
        sim.SetPathingMap(&wpm_, w3e_.offsetX, w3e_.offsetY);
    }

    // 4. Load UnitsDoo & DoodadsDoo and populate SceneManager StaticSpatialChunkGrid
    if (UnitsDooLoadFromMpq(mapPath, unitsDoo_, nullptr)) {
        stats_.placedUnitsRead = unitsDoo_.units.size();
    }

    stats_.placedDoodadsRegistered = 0;
    if (W3dLoadFromMpq(mapPath, doodadsDoo_)) {
        for (const DoodadData& d : doodadsDoo_.doodads) {
            const float pos[3] = {d.pos[0], d.pos[2], d.pos[1]};
            const float scale[3] = {d.scale[0], d.scale[1], d.scale[2]};
            scene.AddProceduralInstance(d.id, pos, d.angle, scale, 0, "doodad", 1.0f);
            stats_.placedDoodadsRegistered++;
        }
    }

    presentation.BindSimulationEvents(sim);

    // 5. Respawn any carried human heroes from a prior campaign level
    if (!carriedHeroes_.empty()) {
        float spawnX = 0.0f;
        float spawnY = 0.0f;
        if (!w3i_.players.empty()) {
            spawnX = w3i_.players[0].startX;
            spawnY = w3i_.players[0].startY;
        }
        for (size_t i = 0; i < carriedHeroes_.size(); ++i) {
            if (RespawnCarriedHero(sim, carriedHeroes_[i], spawnX + i * 64.0f, spawnY)) {
                stats_.carriedHeroesRespawned++;
            }
        }
        sim.Update();
        carriedHeroes_.clear();
    }

    return true;
}

bool WorldSceneSession::HotSwitchMap(MpqManager& mpq,
                                     CSimulation& sim,
                                     SceneManager& scene,
                                     UnitPresentationSystem& presentation,
                                     const std::string& nextMapPath,
                                     bool carryHumanHeroes) {
    carriedHeroes_.clear();
    if (carryHumanHeroes) {
        for (CUnit* u : sim.GetUnits()) {
            if (u && IsCarriableHero(u, sim)) {
                CarriedHero snap = SnapshotHero(u, sim);
                if (snap.typeId.value != 0) {
                    carriedHeroes_.push_back(std::move(snap));
                    stats_.carriedHeroesCaptured++;
                }
            }
        }
    }

    TeardownCurrentSession(mpq, sim, scene, presentation);
    stats_.hotSwitchesPerformed++;
    return LoadMap(mpq, sim, scene, presentation, nextMapPath);
}

} // namespace core
