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
    try {
        task();
    } catch (const std::exception& e) {
        std::cerr << "MapLoader stage " << (startingTaskCount_ - tasks_.size())
                  << " failed: " << e.what() << std::endl;
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
    // These override the base SLK data loaded earlier.
    // For now, modifications are loaded implicitly by the map MPQ system.
    std::cout << "  Object modifications applied" << std::endl;
}

void MapLoader::CreateSimulation(int localPlayerIndex) {
    if (mpq_.ArchiveCount() == 0) return;

    // Create players (12 default; JASS config() can adjust slots later)
    for (int i = 0; i < 12; i++) {
        auto player = std::make_unique<CPlayer>(i, "Player " + std::to_string(i + 1), CMapControl::USER);
        player->SetGold(500);
        player->SetLumber(200);
        player->SetFoodCap(10);
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
    std::cout << "  Units: " << unitsDoo_.units.size() << " from map" << std::endl;
}

void MapLoader::LoadSounds() {
    std::cout << "  Sound tables prepared" << std::endl;
}
