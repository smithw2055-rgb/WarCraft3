#pragma once
#include "../resource/mpq_storm.h"
#include "../ecs/csimulation.h"
#include "../ecs/player.h"
#include "../data/unitdata.h"
#include "../data/destructabledata.h"
#include "../data/itemdata.h"
#include "../data/upgradedata.h"
#include "../data/gameplay_constants.h"
#include "../map/w3i.h"
#include "../map/wpm.h"
#include "../map/w3e.h"
#include "../map/unitsdoo.h"
#include "../map/w3d.h"
#include "../map/w3r.h"
#include "../render/textured_terrain.h"
#include "../render/scene.h"
#include "../asset/TileSet.h"
#include "../scripting/JassNatives.h"
#include <string>
#include <deque>
#include <functional>
#include <memory>
#include <cstdint>

class MapLoader {
public:
    using LoadTask = std::function<void()>;

    struct UnitVisualSeed {
        int handleId = -1;
        float scale[3] = {1.0f, 1.0f, 1.0f};
        uint32_t skinId = 0;
        int hitpoints = 0;
        int flags = 0;
    };

    struct ItemVisualSeed {
        int handleId = -1;
        int visualIndex = -1;
        float scale[3] = {1.0f, 1.0f, 1.0f};
        uint32_t skinId = 0;
        int flags = 0;
    };

    struct UnitLoadResult {
        int simCreated = 0;
        int visualCreated = 0;
        int visualMissForSimUnits = 0;
        int itemRegistered = 0;
        size_t mapUnitCount = 0;
    };

    MapLoader(MpqManager& mpq, CSimulation& sim, CUnitData& unitData,
              CDestructableData& destructableData, CItemData& itemData,
              CUpgradeData& upgradeData, SceneManager& scene,
              TexturedTerrain& terrain, TileSet& tileset,
              W3EMap& mapData, W3iInfo& w3i, WpmData& wpm);

    void CreateTasks(const std::string& mapPath, int localPlayerIndex);
    void SetConfigData(std::shared_ptr<JassNativesData> configData) { configData_ = std::move(configData); }
    void SetLoadRenderAssets(bool enabled) { loadRenderAssets_ = enabled; }

    bool Process();
    float GetCompletionRatio() const;

    const std::string& GetMapPath() const { return mapPath_; }
    const std::vector<UnitVisualSeed>& GetUnitVisualSeeds() const { return unitVisualSeeds_; }
    const std::vector<ItemVisualSeed>& GetItemVisualSeeds() const { return itemVisualSeeds_; }
    const UnitLoadResult& GetUnitLoadResult() const { return unitLoadResult_; }
    bool HasFatalError() const { return fatalError_; }
    const std::string& GetFatalError() const { return fatalErrorMessage_; }

private:
    void LoadUnitDataTables();
    void LoadGameplayConstants();
    void LoadWorldEditStrings(const std::string& mapPath);
    void DetermineTileset(const std::string& mapPath);
    void LoadTerrainData(const std::string& mapPath);
    void LoadPathingData(const std::string& mapPath);
    void BuildTerrain();
    void LoadObjectModifications(const std::string& mapPath);
    void CreateSimulation(int localPlayerIndex);
    void LoadDoodadsAndDestructibles(const std::string& mapPath);
    void LoadUnits(const std::string& mapPath);
    void LoadSounds();

    MpqManager& mpq_;
    CSimulation& sim_;
    CUnitData& unitData_;
    CDestructableData& destructableData_;
    CItemData& itemData_;
    CUpgradeData& upgradeData_;
    SceneManager& scene_;
    TexturedTerrain& terrain_;
    TileSet& tileset_;
    W3EMap& mapData_;
    W3iInfo& w3i_;
    WpmData& wpm_;

    std::string mapPath_;
    std::deque<LoadTask> tasks_;
    size_t startingTaskCount_ = 0;

    UnitsDooData unitsDoo_;
    CGameplayConstants gameplayConstants_;
    std::shared_ptr<JassNativesData> configData_;
    std::vector<UnitVisualSeed> unitVisualSeeds_;
    std::vector<ItemVisualSeed> itemVisualSeeds_;
    UnitLoadResult unitLoadResult_;
    bool fatalError_ = false;
    std::string fatalErrorMessage_;
    bool loadRenderAssets_ = true;
};
