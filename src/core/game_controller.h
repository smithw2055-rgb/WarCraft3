#pragma once
#include "game_state.h"
#include "warsmash_config.h"
#include "../render/scene.h"
#include "../render/camera.h"
#include "../render/terrain.h"
#include "../render/textured_terrain.h"
#include "../render/minimap.h"
#include "../render/mdx_viewer.h"
#include "../render/daynight.h"
#include "../resource/mpq_storm.h"
#include "../asset/TileSet.h"
#include "../asset/Texture.h"
#include "../map/w3e.h"
#include "../map/w3i.h"
#include "../map/wpm.h"
#include "../ecs/csimulation.h"
#include "../ecs/unit.h"
#include "../ecs/player.h"
#include "../data/unitdata.h"
#include "../data/destructabledata.h"
#include "../data/gameplay_constants.h"
#include "../ui/MainUI.h"
#include "../audio/AudioSystem.h"
#include "../scripting/JassVM.h"
#include "../scripting/JassNatives.h"
#include "../scripting/jass_config_env.h"
#include "../scripting/jass_common_env.h"
#include "../map/map_loader.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

class GameController {
public:
    GameController();
    ~GameController();

    bool Initialize(int argc, char* argv[]);
    void RunSyncInit();  // Run all init stages synchronously (for legacy frame_cb compat)
    void Update(int screenW, int screenH, float dt);
    void Render(int screenW, int screenH);
    void Shutdown();

    GameState GetState() const { return state_; }

    // Accessors for subsystems (used by main.cpp callback wiring)
    SceneManager& GetScene() { return scene_; }
    CSimulation& GetSimulation() { return simulation_; }
    MainUI& GetUI() { return ui_; }
    AudioSystem& GetAudio() { return audio_; }
    TexturedTerrain& GetTexturedTerrain() { return texturedTerrain_; }
    TerrainRenderer& GetSimpleTerrain() { return simpleTerrain_; }
    Minimap& GetMinimap() { return minimap_; }
    OrbitCamera& GetCamera() { return camera_; }
    MdxViewer& GetMdxViewer() { return mdxViewer_; }
    DayNightCycle& GetDayNight() { return dayNight_; }
    TileSet& GetTileset() { return tileset_; }
    MpqManager& GetMpq() { return mpq_; }
    CUnitData& GetUnitData() { return unitData_; }
    CDestructableData& GetDestructableData() { return destructableData_; }
    const W3EMap& GetMapData() const { return map_; }
    const W3iInfo& GetW3i() const { return w3i_; }
    const WpmData& GetWpm() const { return wpm_; }
    JassVM& GetJassVM() { return jass_; }
    JassNatives* GetJassNatives() { return jassNatives_.get(); }
    std::unique_ptr<JassNatives>& GetJassNativesRef() { return jassNatives_; }
    WarsmashConfig& GetConfig() { return config_; }
    const std::string& GetMapPath() const { return mapPath_; }

    bool IsInitialized() const { return initialized_; }
    bool IsRunning() const { return running_; }
    int GetFrameCount() const { return frameCount_; }

    // Unit visual state (moved from global 'g')
    struct UnitVisualSeed {
        float scale[3] = {1.0f, 1.0f, 1.0f};
        uint32_t skinId = 0;
        int hitpoints = 0;
        int flags = 0;
    };

    struct PendingUnitAnimation {
        std::string sequenceName;
        int sequenceIndex = -1;
        bool useSequenceIndex = false;
    };

    enum class UnitVisualAnimationMode : uint8_t {
        NONE, READY, READY_GOLD, READY_LUMBER,
        WALK, WALK_GOLD, WALK_LUMBER, WALK_WORK,
        WORK, SPELL, ATTACK, CONSTRUCT,
        DEAD, CORPSE, BONE_CORPSE, DISSIPATE
    };

    struct ScriptedUnitAnimationState {
        PendingUnitAnimation request;
        bool loop = false;
        bool queueStandAfterCurrent = false;
    };

    // Map for per-unit visual state
    std::unordered_map<int, UnitVisualSeed> simUnitVisualSeeds;
    std::unordered_map<int, PendingUnitAnimation> simUnitPendingAnimations;
    std::unordered_map<int, int> simUnitToVisualIdx;
    std::unordered_map<int, std::pair<float, float>> simUnitVisualLastPositions;
    std::unordered_map<int, bool> simUnitVisualMovingStates;
    std::unordered_map<int, UnitVisualAnimationMode> simUnitVisualAnimationModes;
    std::unordered_map<int, int> simUnitVisualLastAttackAnimationTicks;
    std::unordered_map<int, int> simUnitVisualResourceTags; // CResourceType
    std::unordered_map<int, bool> simUnitVisualConstructionStates;
    std::unordered_map<int, std::vector<std::string>> simUnitVisualPropertyTags;
    std::unordered_map<int, ScriptedUnitAnimationState> simUnitScriptedAnimations;
    std::unordered_map<int, PendingUnitAnimation> simUnitQueuedAnimations;
    std::unordered_map<int, std::vector<std::string>> simUnitAnimationPropertyTags;
    std::unordered_set<int> simUnitVisualRegistrationFailed;

    bool EnsureSimulationUnitVisual(CUnit* unit);

    // Screenshot
    std::string screenshotPath;
    int screenshotFrame = -1;

    // Mouse state
    float mouseDX = 0, mouseDY = 0, mouseScroll = 0;
    bool mouseDown[3] = {};

    bool showMinimap = true;
    bool showPathing = false;
    bool showRegions = false;
    bool wireframe = false;

private:
    void TransitionTo(GameState newState);
    void OnEnterState(GameState state);
    void OnLeaveState(GameState state);

    void InitBootstrap();
    void InitMenu();
    void InitMapConfig();
    void InitLoading();
    void InitInGame();

    void UpdateBootstrap();
    void UpdateMenu();
    void UpdateLoading(float dt);
    void UpdateInGame(int screenW, int screenH, float dt);

    void ParseArgs(int argc, char* argv[]);
    void OpenDataSources();
    void LoadCommonJassFiles(JassVM& vm);
    void ReadMapFile(const std::string& mapPath, const std::string& fileName,
                     std::vector<uint8_t>& out);
    void LoadSelectionAndBuildingSplatMetadata();
    void WireJassCallbacks();
    void EnsureHumanMeleeStartsFromMapConfig();
    void FlushStartupSimulationState();
    void FocusCameraAndMinimapOnInitialSpawn();

    GameState state_ = GameState::BOOTSTRAP;
    GameState pendingState_ = GameState::BOOTSTRAP;
    bool stateTransition_ = false;
    bool initialized_ = false;
    bool running_ = true;
    int frameCount_ = 0;
    float simulationAccumulator_ = 0.0f;
    float lastTime_ = 0.0f;

    WarsmashConfig config_;

    // Rendering
    SceneManager scene_;
    TerrainRenderer simpleTerrain_;
    TexturedTerrain texturedTerrain_;
    bool terrainLoaded_ = false;
    W3EMap map_;
    W3iInfo w3i_;
    TileSet tileset_;
    OrbitCamera camera_;
    Minimap minimap_;
    MdxViewer mdxViewer_;
    DayNightCycle dayNight_;

    // Resources
    MpqManager mpq_;
    std::string mapPath_;

    // Simulation
    CSimulation simulation_;
    CUnitData unitData_;
    CDestructableData destructableData_;
    WpmData wpm_;
    bool simulationInitialized_ = false;

    // Audio
    AudioSystem audio_;

    // UI
    MainUI ui_;

    // JASS
    JassVM jass_;
    std::unique_ptr<JassNatives> jassNatives_;
    std::unique_ptr<JassConfigEnv> configEnv_;
    std::unique_ptr<JassCommonEnv> commonEnv_;

    // Map loading
    std::unique_ptr<MapLoader> mapLoader_;
    float loadingProgress_ = 0.0f;
};
