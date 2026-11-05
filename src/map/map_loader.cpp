#include "map_loader.h"
#include "../data/gameplay_constants.h"
#include "../resource/slk.h"
#include "../map/w3x_storm.h"
#include "../map/wts.h"
#include "../ecs/destructable.h"
#include "../ecs/abilities/ability_inventory.h"
#include <cmath>
#include <iostream>

namespace {

static uint32_t FourCC(const char* text) {
    return (uint32_t(uint8_t(text[0])) << 24)
         | (uint32_t(uint8_t(text[1])) << 16)
         | (uint32_t(uint8_t(text[2])) << 8)
         | uint32_t(uint8_t(text[3]));
}

static bool IsKnownNonRenderableMapUnit(uint32_t id) {
    return id == FourCC("sloc");
}

} // namespace

MapLoader::MapLoader(MpqManager& mpq, CSimulation& sim, CUnitData& unitData,
                     CDestructableData& destructableData, CDoodadData& doodadData, CItemData& itemData,
                     CUpgradeData& upgradeData, CAbilityData& abilityData, SceneManager& scene,
                     TexturedTerrain& terrain, TileSet& tileset,
                     W3EMap& mapData, W3iInfo& w3i, WpmData& wpm)
    : mpq_(mpq), sim_(sim), unitData_(unitData),
      destructableData_(destructableData), doodadData_(doodadData), itemData_(itemData),
      upgradeData_(upgradeData), abilityData_(abilityData), scene_(scene),
      terrain_(terrain), tileset_(tileset),
      mapData_(mapData), w3i_(w3i), wpm_(wpm)
{
}

void MapLoader::CreateTasks(const std::string& mapPath, int localPlayerIndex) {
    mapPath_ = mapPath;
    tasks_.clear();
    unitsDoo_ = UnitsDooData{};
    unitVisualSeeds_.clear();
    itemVisualSeeds_.clear();
    unitLoadResult_ = UnitLoadResult{};
    fatalError_ = false;
    fatalErrorMessage_.clear();

    tasks_.push_back([this]() { DetermineTileset(mapPath_); });
    tasks_.push_back([this]() { LoadWorldEditStrings(mapPath_); });
    tasks_.push_back([this]() { LoadGameplayConstants(); });
    tasks_.push_back([this]() { LoadUnitDataTables(); });
    tasks_.push_back([this]() { LoadTerrainData(mapPath_); });
    tasks_.push_back([this]() { LoadPathingData(mapPath_); });
    if (loadRenderAssets_) {
        tasks_.push_back([this]() { BuildTerrain(); });
    }
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

void MapLoader::LoadGameplayConstants() {
    // Misc gameplay data (armor formula + attack/defense bonus table), merged
    // like warsmash_ref: Units\MiscData.txt first, Units\MiscGame.txt wins on
    // conflicts. Missing files keep compiled-in TFT defaults.
    if (mpq_.ArchiveCount() == 0) return;
    TxtTable misc;
    bool loaded = false;
    std::vector<uint8_t> bytes;
    if (mpq_.ReadFile("Units\\MiscData.txt", bytes) && !bytes.empty()) {
        loaded = misc.Load(bytes.data(), bytes.size()) || loaded;
    }
    if (mpq_.ReadFile("Units\\MiscGame.txt", bytes) && !bytes.empty()) {
        loaded = misc.Load(bytes.data(), bytes.size()) || loaded;
    }
    if (loaded) {
        gameplayConstants_ = CGameplayConstants(misc);
        sim_.SetGameplayConstants(gameplayConstants_);
        std::cout << "  GameplayConstants: loaded (DefenseArmor="
                  << gameplayConstants_.GetDefenseArmor() << ")" << std::endl;
    }
}

void MapLoader::LoadUnitDataTables() {
    if (mpq_.ArchiveCount() > 0) {
        unitData_ = CUnitData::LoadFromMpq(mpq_, gameplayConstants_);
        destructableData_ = CDestructableData::LoadFromMpq(mpq_);
        doodadData_ = CDoodadData::LoadFromMpq(mpq_);
        itemData_ = CItemData::LoadFromMpq(mpq_);
        upgradeData_ = CUpgradeData::LoadFromMpq(mpq_);
        abilityData_ = CAbilityData::LoadFromMpq(mpq_);
        sim_.SetUnitData(&unitData_);
        sim_.SetDestructableData(&destructableData_);
        sim_.SetDoodadData(&doodadData_);
        sim_.SetItemData(&itemData_);
        sim_.SetUpgradeData(&upgradeData_);
        sim_.SetAbilityData(&abilityData_);
        sim_.SetGameplayConstants(gameplayConstants_);
        std::cout << "  UnitData: " << unitData_.GetAllTypes().size() << " types loaded" << std::endl;
    }
}

void MapLoader::LoadTerrainData(const std::string& mapPath) {
    if (W3XLoadTerrain(mapPath, mapData_)) {
        std::cout << "  Terrain: " << mapData_.tileWidth << "x" << mapData_.tileHeight << " tiles" << std::endl;
        // Publish to the sim for JASS terrain queries (GetTerrainType etc.).
        sim_.SetTerrainMap(&mapData_);
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
        doodadData_ = CDoodadData::LoadFromMpq(mpq_, mapPath);
        itemData_ = CItemData::LoadFromMpq(mpq_, mapPath);
        upgradeData_ = CUpgradeData::LoadFromMpq(mpq_, mapPath);
        abilityData_ = CAbilityData::LoadFromMpq(mpq_, mapPath);
        sim_.SetUnitData(&unitData_);
        sim_.SetDestructableData(&destructableData_);
        sim_.SetDoodadData(&doodadData_);
        sim_.SetItemData(&itemData_);
        sim_.SetUpgradeData(&upgradeData_);
        sim_.SetAbilityData(&abilityData_);
        if (loadRenderAssets_) {
            scene_.ApplyMapObjectModifications(mapPath);
        }
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
            // common.j ordinals: 0 USER, 1 COMPUTER, 2 RESCUABLE, 3 NEUTRAL,
            // 4 CREEP, 5 NONE (note: C++ enum order differs, map explicitly).
            // -1 = config never called SetPlayerController for this slot.
            switch (playerConfig.controller) {
                case 0: controller = CMapControl::USER; break;
                case 1: controller = CMapControl::COMPUTER; break;
                case 2: controller = CMapControl::RESCUABLE; break;
                case 3: controller = CMapControl::NEUTRAL; break;
                case 4: controller = CMapControl::CREEP; break;
                case 5: controller = CMapControl::NONE; break;
                default: {
                    const bool active = playerConfig.slotActive
                        || (configData_->configuredPlayerCount > 0 && i < configData_->configuredPlayerCount);
                    controller = active ? CMapControl::USER : CMapControl::NONE;
                    break;
                }
            }
        }
        auto player = std::make_unique<CPlayer>(i, "Player " + std::to_string(i + 1), controller);
        player->SetGold(500);
        player->SetLumber(200);
        player->SetFoodCap(10);
        // Creep bounties (ref neutralAggressive GIVES_BOUNTY): kills of
        // neutral/creep-owned units pay their gold/lumber bounty tables.
        if (controller == CMapControl::NEUTRAL || controller == CMapControl::CREEP) {
            player->SetGivesBounty(true);
        }
        if (configData_) {
            const int cfgColor = configData_->playerMapConfig[i].color;
            if (cfgColor >= 0) player->SetColor(cfgColor);
            // Race for JASS GetPlayerRace / Blizzard.j MeleeStartingUnits
            // (ref RACE_HUMAN=1..RACE_NIGHTELF=4; preferences are bitmasks:
            // HUMAN=1, ORC=2, NIGHTELF=4, UNDEAD=8, RANDOM=32).
            const int pref = configData_->playerMapConfig[i].racePreference;
            int race = 0;
            if (pref == 1) race = 1;        // human
            else if (pref == 2) race = 2;   // orc
            else if (pref == 8) race = 3;   // undead
            else if (pref == 4) race = 4;   // night elf
            else {
                // RANDOM / unset: deterministic roll so every slot still
                // gets race-correct melee starting units.
                race = 1 + (int)(sim_.SimRand01() * 4.0f) % 4;
            }
            player->SetRace(race);
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
    // w3i techtree availability baseline (tech bans, upgrade
    // unavailable/available/researched); map script may override later.
    ApplyW3iTechtreeAvailability(sim_, w3i_);
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
                    // DOO angles are radians (ref facingRadians); WPM stamps
                    // take degrees (ref toDegrees for pathing instances).
                    const int rotDeg = (int)std::round(d.angle * 180.0f / 3.14159265358979323846f);
                    // Death drops (ref Doodad.itemTable + itemSets): explicit
                    // sets plus the referenced w3i global item table, rolled
                    // per item on the killing blow (mirrors unit dropSets).
                    if (CDestructable* dest = sim_.CreateDestructable(doodadId, d.pos[0], d.pos[1], life, rotDeg)) {
                        simDestructableCreated++;
                        std::vector<CDestructable::DeathDropItem> drops;
                        for (const auto& set : d.itemSets) {
                            for (const auto& entry : set.items) {
                                if (entry.id == 0 || entry.chance <= 0) continue;
                                drops.push_back({War3ID(entry.id), entry.chance});
                            }
                        }
                        if (w3d.version > 7 && d.itemTable >= 0) {
                            std::vector<W3iDropItem> tableDrops;
                            if (CollectRandomItemTableDrops(w3i_, d.itemTable, tableDrops)) {
                                for (const auto& entry : tableDrops) {
                                    drops.push_back({entry.itemId, entry.chancePercent});
                                }
                            }
                        }
                        if (!drops.empty()) dest->SetDeathDrops(std::move(drops));
                    }
                }
            }
            // DOO stores object angles in radians; the scene takes degrees.
            const float doodadAngleDegrees = d.angle * 180.0f / 3.14159265358979323846f;
            if (loadRenderAssets_ &&
                scene_.AddDoodad(d.id, d.pos, doodadAngleDegrees, d.scale, d.variation, d.flags, d.life, d.skinId))
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
        // Random-table spawn (ref randomFlag==2): roll the unit type from the
        // chance table with the deterministic sim RNG.
        if (u.randomFlag == 2 && !u.randomTable.empty()) {
            float roll = sim_.SimRand01() * 100.0f;
            float accum = 0.0f;
            for (const auto& entry : u.randomTable) {
                accum += (float)entry.chance;
                if (roll < accum) {
                    id = War3ID(entry.id);
                    break;
                }
            }
        }
        // Global random-group spawn (ref randomFlag==1): unit group + column
        // from the w3i global unit tables, same chance roll. Unresolvable
        // groups keep the fixed id (previous behavior).
        if (u.randomFlag == 1) {
            War3ID resolved;
            if (ResolveRandomGroupUnit(w3i_, u.randomGroup, u.randomPosition,
                                       sim_.SimRand01(), resolved) &&
                unitData_.HasUnitType(resolved)) {
                id = resolved;
            }
        }
        // units.doo stores facing in radians; simulation facing and the
        // scene both use degrees.
        const float unitAngleDegrees = u.angle * 180.0f / 3.14159265358979323846f;
        CUnit* simUnit = nullptr;
        if (unitData_.HasUnitType(id)) {
            simUnit = sim_.CreateUnit(id, u.player, u.pos[0], u.pos[1], unitAngleDegrees);
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
                if (u.heroLevel > 0) simUnit->SetHeroLevel(u.heroLevel);
                if (u.targetAcquisition >= 0.0f) simUnit->SetAcquireRange(u.targetAcquisition);
                // Preplaced ability levels and autocast flags from the doodad.
                for (const auto& mod : u.modifiedAbilities) {
                    if (CAbility* ability = simUnit->GetAbility(War3ID(mod.id))) {
                        if (mod.level > 0) ability->SetLevel(mod.level);
                        if (mod.autocastActive != 0) ability->SetAutocastActive(true);
                    }
                }
                if (CUnitType* placedType = simUnit->GetUnitType()) {
                    const War3ID autocast = placedType->GetDefaultAutocastAbility();
                    if (autocast.value != 0) {
                        if (CAbility* ability = simUnit->GetAbility(autocast)) {
                            ability->SetAutocastActive(true);
                        }
                    }
                }
                // Preplaced inventory (heroes, shops): create + pick up.
                if (!u.inventory.empty()) {
                    if (auto* inv = dynamic_cast<CAbilityInventory*>(
                            simUnit->GetAbility(War3ID("AInv")))) {
                        for (const auto& entry : u.inventory) {
                            War3ID itemId(entry.id);
                            if (!itemData_.HasItem(itemId)) continue;
                            if (CItem* item = sim_.CreateItem(
                                    itemId, u.pos[0], u.pos[1])) {
                                inv->PickUpItem(sim_, simUnit, item, entry.slot);
                            }
                        }
                    }
                }
                // Death drops (ref units.doo dropSets + w3i droppedItemTable):
                // explicit sets plus the referenced global item table's
                // sets, rolled per item at the death tick.
                {
                    std::vector<CUnit::DeathDropItem> drops;
                    for (const auto& set : u.dropSets) {
                        for (const auto& entry : set.items) {
                            if (entry.id == 0 || entry.chance <= 0) continue;
                            drops.push_back({War3ID(entry.id), entry.chance});
                        }
                    }
                    // Table index only exists on version > 7 (older maps
                    // leave the default 0, which must not resolve table 0).
                    if (unitsDoo_.version > 7 && u.droppedItemTable >= 0) {
                        std::vector<W3iDropItem> tableDrops;                        if (CollectRandomItemTableDrops(w3i_, u.droppedItemTable,
                                                        tableDrops)) {
                            for (const auto& entry : tableDrops) {
                                drops.push_back({entry.itemId, entry.chancePercent});
                            }
                        }
                    }
                    if (!drops.empty()) simUnit->SetDeathDrops(std::move(drops));
                }
            }
        }
        if (!simUnit) {
            if (!loadRenderAssets_) {
                continue;
            }
            if (IsKnownNonRenderableMapUnit(u.id)) {
                continue;
            }
            if (scene_.HasItemPath(u.id, u.skinId)) {
                int itemHandleId = -1;
                if (configData_ && itemData_.HasItem(id)) {
                    itemHandleId = configData_->CreateItem(&sim_, id, u.pos[0], u.pos[1]);
                    if (itemHandleId >= 0) {
                        unitLoadResult_.itemRegistered++;
                    }
                }
                const int visualIndex = scene_.AddItem(u.id, u.pos, unitAngleDegrees, u.scale, u.flags, u.skinId);
                if (visualIndex >= 0) {
                    unitLoadResult_.visualCreated++;
                    if (itemHandleId >= 0) {
                        ItemVisualSeed seed;
                        seed.handleId = itemHandleId;
                        seed.visualIndex = visualIndex;
                        seed.scale[0] = u.scale[0];
                        seed.scale[1] = u.scale[1];
                        seed.scale[2] = u.scale[2];
                        seed.skinId = u.skinId;
                        seed.flags = u.flags;
                        itemVisualSeeds_.push_back(seed);
                    }
                }
            } else if (scene_.AddUnit(u.id, u.pos, unitAngleDegrees, u.scale, u.player, u.hitpoints, u.flags, u.skinId) >= 0) {
                unitLoadResult_.visualCreated++;
            }
        }
    }
    std::cout << "  Units: " << unitsDoo_.units.size() << " from map, "
              << unitLoadResult_.simCreated << " simulation units, "
              << unitLoadResult_.itemRegistered << " JASS items" << std::endl;
}

void MapLoader::LoadSounds() {
    std::cout << "  Sound tables prepared" << std::endl;
}
