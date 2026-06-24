#pragma once
#include "../resource/mpq_storm.h"
#include "../ecs/csimulation.h"
#include "../ecs/player.h"
#include "../data/unitdata.h"
#include "../data/destructabledata.h"
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
#include <string>
#include <deque>
#include <functional>
#include <memory>

class MapLoader {
public:
    using LoadTask = std::function<void()>;

    MapLoader(MpqManager& mpq, CSimulation& sim, CUnitData& unitData,
              CDestructableData& destructableData, SceneManager& scene,
              TexturedTerrain& terrain, TileSet& tileset,
              W3EMap& mapData, W3iInfo& w3i, WpmData& wpm);

    void CreateTasks(const std::string& mapPath, int localPlayerIndex);

    bool Process();
    float GetCompletionRatio() const;

    const std::string& GetMapPath() const { return mapPath_; }

private:
    void LoadUnitDataTables();
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
};
