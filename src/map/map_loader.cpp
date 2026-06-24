#include "map_loader.h"
#include "../data/gameplay_constants.h"
#include "../map/w3x_storm.h"
#include "../map/wts.h"
#include "../ecs/destructable.h"
#include <iostream>

MapLoader::MapLoader(MpqManager& mpq, CSimulation& sim, CUnitData& unitData,
                     CDestructableData& destructableData, SceneManager& scene,
                     TexturedTerrain& terrain, TileSet& tileset,
                     W3EMap& mapData, W3iInfo& w3i, WpmData& wpm)
    : mpq_(mpq), sim_(sim), unitData_(unitData),
      destructableData_(destructableData), scene_(scene),
      terrain_(terrain), tileset_(tileset),
      mapData_(mapData), w3i_(w3i), wpm_(wpm)
{
}

void MapLoader::CreateTasks(const std::string& mapPath, int localPlayerIndex) {
    mapPath_ = mapPath;
    tasks_.clear();
    unitsDoo_ = UnitsDooData{};
    unitVisualSeeds_.clear();
    unitLoadResult_ = UnitLoadResult{};
    fatalError_ = false;
    fatalErrorMessage_.clear();

    tasks_.push_back([this]() { DetermineTileset(mapPath_); });
    tasks_.push_back([this]() { LoadWorldEditStrings(mapPath_); });
    tasks_.push_back([this]() { LoadUnitDataTables(); });
    tasks_.push_back([this]() { LoadTerrainData(mapPath_); });
    tasks_.push_back([this]() { LoadPathingData(mapPath_); });
    tasks_.push_back([this]() { BuildTerrain(); });
    tasks_.push_back([this]() { LoadObjectModifications(mapPath_); });
    tasks_.push_back([this, localPlayerIndex]() { CreateSimulation(localPlayerIndex); });
    tasks_.push_back([this]() { LoadDoodadsAndDestructibles(mapPath_); });
    tasks_.push_back([this]() { LoadUnits(mapPath_); });
    tasks_.push_back([this]() { LoadSounds(); });

    startingTaskCount_ = tasks_.size();
    std::cout << "MapLoader: " << startingTaskCount_ << " stages queued" << std::endl;
}

bool MapLoader::Process() {
    if (tasks_.empty()) return true;
    auto task = std::move(tasks_.front());
    tasks_.pop_front();
    const size_t stageIndex = startingTaskCount_ - tasks_.size();
    try {
        task();
    } catch (const std::exception& e) {
        fatalError_ = true;
        fatalErrorMessage_ = e.what();
        tasks_.clear();
        std::cerr << "MapLoader stage " << stageIndex
                  << " failed: " << fatalErrorMessage_ << std::endl;
    }
    return tasks_.empty();
}

float MapLoader::GetCompletionRatio() const {
    if (startingTaskCount_ == 0) return 1.0f;
    return 1.0f - ((float)tasks_.size() / (float)startingTaskCount_);
}

void MapLoader::DetermineTileset(const std::string& mapPath) {
    (void)mapPath;
    std::cout << "  Tileset: " << w3i_.tileset << std::endl;
}

void MapLoader::LoadWorldEditStrings(const std::string& mapPath) {
    // WorldEditStrings loaded as needed; WTS strings parsed later
    std::cout << "  WorldEdit strings prepared" << std::endl;
}

void MapLoader::LoadUnitDataTables() {
    if (mpq_.ArchiveCount() > 0) {
        unitData_ = CUnitData::LoadFromMpq(mpq_, gameplayConstants_);
        destructableData_ = CDestructableData::LoadFromMpq(mpq_);
        sim_.SetUnitData(&unitData_);
        sim_.SetDestructableData(&destructableData_);
        sim_.SetGameplayConstants(gameplayConstants_);
        std::cout << "  UnitData: " << unitData_.GetAllTypes().size() << " types loaded" << std::endl;
    }
}

void MapLoader::LoadTerrainData(const std::string& mapPath) {
    if (W3XLoadTerrain(mapPath, mapData_)) {
        std::cout << "  Terrain: " << mapData_.tileWidth << "x" << mapData_.tileHeight << " tiles" << std::endl;
    } else {
        fatalError_ = true;
        fatalErrorMessage_ = "Terrain W3E failed to load";
        tasks_.clear();
        std::cerr << "  Terrain: failed to load" << std::endl;
    }
}

void MapLoader::LoadPathingData(const std::string& mapPath) {
    bool loaded = WpmLoadFromMpq(mapPath, wpm_);
    if (loaded) {
        std::cout << "  Pathing: " << wpm_.width << "x" << wpm_.height << std::endl;
    } else {
        std::cout << "  Pathing: not found" << std::endl;
    }
    sim_.SetPathingMap(loaded ? &wpm_ : nullptr, mapData_.offsetX, mapData_.offsetY);
}

void MapLoader::BuildTerrain() {
    if (mapData_.tileWidth == 0) return;

    bool hasTextures = false;
    if (mpq_.ArchiveCount() > 0 && tileset_.LoadFromMpq(mpq_, mapData_))
        hasTextures = true;
    else if (mpq_.ArchiveCount() > 0 && tileset_.LoadFromWorldEditData(mpq_, mapData_.tileset, mapData_.groundTilePalette.size()))
        hasTextures = true;

    if (hasTextures) {
        tileset_.LoadCliffTextures(mpq_, mapData_);
        tileset_.LoadCliffModelInfo(mpq_);
        tileset_.LoadWaterInfo(mpq_, mapData_);
        tileset_.LoadWaterTextures(mpq_);
        terrain_.SetMpq(&mpq_);
        terrain_.Build(mapData_, &tileset_);
    }

    terrain_.SetPathing(&wpm_);

    // Set map bounds
    if (w3i_.playableWidth > 0) {
        terrain_.SetMapBounds(w3i_.cameraBounds[0], w3i_.cameraBounds[1],
                              w3i_.cameraBounds[2], w3i_.cameraBounds[3]);
    } else {
        auto center = terrain_.GetCenter();
        float r = terrain_.GetRadius();
        terrain_.SetMapBounds(center.x - r, center.z - r, center.x + r, center.z + r);
    }
    std::cout << "  Terrain built" << std::endl;
}

void MapLoader::LoadObjectModifications(const std::string& mapPath) {
    // Map-specific object modifications (custom unit/ability data in war3map.w3u, etc.)
    // Rebuild unit type data before any simulation units exist, so units.doo and JASS-created
    // units see the same merged object-editor values without invalidating live CUnitType refs.
    if (mpq_.ArchiveCount() > 0) {
        unitData_ = CUnitData::LoadFromMpq(mpq_, gameplayConstants_, mapPath);
        destructableData_ = CDestructableData::LoadFromMpq(mpq_, mapPath);
        sim_.SetUnitData(&unitData_);
        sim_.SetDestructableData(&destructableData_);
        scene_.ApplyMapObjectModifications(mapPath);
    }
    std::cout << "  Object modifications applied" << std::endl;
}

void MapLoader::CreateSimulation(int localPlayerIndex) {
    if (mpq_.ArchiveCount() == 0) return;
    (void)localPlayerIndex;

    // Create players after config() so map-defined slots and start locations are available.
    for (int i = 0; i < 12; i++) {
        CMapControl controller = CMapControl::USER;
        if (configData_) {
            const auto& playerConfig = configData_->playerMapConfig[i];
            const bool active = playerConfig.slotActive
                || (configData_->configuredPlayerCount > 0 && i < configData_->configuredPlayerCount);
            controller = active ? CMapControl::USER : CMapControl::NONE;
        }
        auto player = std::make_unique<CPlayer>(i, "Player " + std::to_string(i + 1), controller);
        player->SetGold(500);
        player->SetLumber(200);
        player->SetFoodCap(10);
        if (configData_) {
            int startIndex = configData_->playerMapConfig[i].startLocationIndex;
            if (startIndex < 0) startIndex = i;
            if (startIndex >= 0 && startIndex < (int)configData_->startLocations.size()) {
                const auto& start = configData_->startLocations[startIndex];
                if (start.defined) {
                    player->SetStartLocation(start.x, start.y);
                }
            }
        }
        sim_.AddPlayer(std::move(player));
    }
    std::cout << "  Simulation: 12 players created" << std::endl;
}

void MapLoader::LoadDoodadsAndDestructibles(const std::string& mapPath) {
    W3dData w3d;
    if (W3dLoadFromMpq(mapPath, w3d)) {
        int doodAdded = 0;
        int simDestructableCreated = 0;
        for (auto& d : w3d.doodads) {
            War3ID doodadId(d.id);
            if (destructableData_.HasDestructable(doodadId)) {
                const auto* dt = destructableData_.GetDestructable(doodadId);
                if (dt) {
                    float maxLife = (float)dt->maxLife;
                    float lifeRatio = std::clamp((float)d.life / 255.0f, 0.0f, 1.0f);
                    float life = maxLife * lifeRatio;
                    if (sim_.CreateDestructable(doodadId, d.pos[0], d.pos[1], life))
                        simDestructableCreated++;
                }
            }
            if (scene_.AddDoodad(d.id, d.pos, d.angle, d.scale, d.variation, d.flags, d.life, d.skinId))
                doodAdded++;
        }
        if (doodAdded > 0) std::cout << "  Doodads: " << doodAdded << " rendered" << std::endl;
        if (simDestructableCreated > 0) std::cout << "  Destructables: " << simDestructableCreated << " in simulation" << std::endl;
    }
}

void MapLoader::LoadUnits(const std::string& mapPath) {
    if (!UnitsDooLoadFromMpq(mapPath, unitsDoo_, &w3i_)) {
        std::cout << "  Units: failed to load units.doo" << std::endl;
        return;
    }
    unitLoadResult_.mapUnitCount = unitsDoo_.units.size();
    for (const auto& u : unitsDoo_.units) {
        War3ID id(u.id);
        CUnit* simUnit = nullptr;
        if (unitData_.HasUnitType(id)) {
            simUnit = sim_.CreateUnit(id, u.player, u.pos[0], u.pos[1], u.angle);
            if (simUnit) {
                unitLoadResult_.simCreated++;
                UnitVisualSeed seed;
                seed.handleId = simUnit->GetHandleId();
                seed.scale[0] = u.scale[0];
                seed.scale[1] = u.scale[1];
                seed.scale[2] = u.scale[2];
                seed.skinId = u.skinId;
                seed.hitpoints = u.hitpoints;
                seed.flags = u.flags;
                unitVisualSeeds_.push_back(seed);
                if (u.hitpoints > 0) simUnit->SetLife(sim_, (float)u.hitpoints);
                if (u.mana > 0) simUnit->SetMana((float)u.mana);
            }
        }
        if (!simUnit) {
            if (scene_.AddUnit(u.id, u.pos, u.angle, u.scale, u.player, u.hitpoints, u.flags, u.skinId))
                unitLoadResult_.visualCreated++;
        }
    }
    std::cout << "  Units: " << unitsDoo_.units.size() << " from map, "
              << unitLoadResult_.simCreated << " simulation units" << std::endl;
}

void MapLoader::LoadSounds() {
    std::cout << "  Sound tables prepared" << std::endl;
}
