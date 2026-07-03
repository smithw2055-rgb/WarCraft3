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
#include "../render/splat_metadata.h"
#include "../ecs/animation_utils.h"
#include "../resource/mpq_storm.h"
#include "../asset/TileSet.h"
#include "../asset/Texture.h"
#include "../map/w3e.h"
#include "../map/w3i.h"
#include "../map/wpm.h"
#include "../ecs/csimulation.h"
#include "../ecs/unit.h"
#include "../ecs/player.h"
#include "../ecs/abilities/ability_harvest.h"
#include "../data/unitdata.h"
#include "../data/destructabledata.h"
#include "../data/itemdata.h"
#include "../data/upgradedata.h"
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
#include <unordered_set>
#include <memory>
#include <functional>

struct sapp_event; // fwd

class GameController {
public:
    GameController();
    ~GameController();

    void ParseArgs(int argc, char* argv[]);  // call from sokol_main; safe before sokol init
    bool Initialize();                       // call from init_cb (depends on sokol context)
    void RunSyncInit();
    void Update(int screenW, int screenH, float dt);
    void Render(int screenW, int screenH);
    void OnEvent(const sapp_event* ev);
    void Shutdown();

    GameState GetState() const { return state_; }
    bool IsInitialized() const { return initialized_; }
    bool IsRunning() const { return running_; }
    int GetFrameCount() const { return frameCount_; }

    // Accessors
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
    SplatMetadataStore& GetSplatMeta() { return splatMeta_; }
    TileSet& GetTileset() { return tileset_; }
    MpqManager& GetMpq() { return mpq_; }
    CUnitData& GetUnitData() { return unitData_; }
    CDestructableData& GetDestructableData() { return destructableData_; }
    CItemData& GetItemData() { return itemData_; }
    const W3EMap& GetMapData() const { return map_; }
    const W3iInfo& GetW3i() const { return w3i_; }
    const WpmData& GetWpm() const { return wpm_; }
    JassVM& GetJassVM() { return jass_; }
    JassNatives* GetJassNatives() { return jassNatives_.get(); }
    std::unique_ptr<JassNatives>& GetJassNativesRef() { return jassNatives_; }
    WarsmashConfig& GetConfig() { return config_; }
    const std::string& GetMapPath() const { return mapPath_; }

    // Selection access (for JASS natives / splat refresh)
    const std::unordered_set<int>& GetSelectedUnitHandles() const { return selectedUnitHandlesSet_; }
    bool IsUnitSelected(int handleId) const;
    std::vector<CUnit*> GetSelectedSimulationUnits() const;

    // Visual seed (distinct from MapLoader::UnitVisualSeed — no handleId)
    struct UnitVisualSeed { float scale[3]={1,1,1}; uint32_t skinId=0; int hitpoints=0; int flags=0; };

    std::unordered_map<int, UnitVisualSeed> simUnitVisualSeeds;
    std::unordered_map<int, animation_utils::PendingUnitAnimation> simUnitPendingAnimations;
    std::unordered_map<int, int> simUnitToVisualIdx;
    std::unordered_map<int, std::pair<float, float>> simUnitVisualLastPositions;
    std::unordered_map<int, bool> simUnitVisualMovingStates;
    std::unordered_map<int, animation_utils::UnitVisualAnimationMode> simUnitVisualAnimationModes;
    std::unordered_map<int, int> simUnitVisualLastAttackAnimationTicks;
    std::unordered_map<int, CResourceType> simUnitVisualResourceTags;
    std::unordered_map<int, bool> simUnitVisualConstructionStates;
    std::unordered_map<int, std::vector<std::string>> simUnitVisualPropertyTags;
    std::unordered_map<int, animation_utils::ScriptedUnitAnimationState> simUnitScriptedAnimations;
    std::unordered_map<int, animation_utils::PendingUnitAnimation> simUnitQueuedAnimations;
    std::unordered_map<int, std::vector<std::string>> simUnitAnimationPropertyTags;
    std::unordered_set<int> simUnitVisualRegistrationFailed;
    std::unordered_map<int, int> simItemToVisualIdx;
    std::unordered_set<int> simItemVisualRegistrationFailed;

    bool EnsureSimulationUnitVisual(CUnit* unit);
    bool EnsureSimulationItemVisual(CItem* item);

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
    // --- State machine ---
    void TransitionTo(GameState newState);
    void OnEnterState(GameState state);
    void OnLeaveState(GameState state);

    // --- Init phases ---
    void InitBootstrap();
    void InitMapConfig();
    void InitLoading();
    void InitInGame();

    // --- Update phases ---
    void UpdateLoading(float dt);
    void UpdateInGame(int screenW, int screenH, float dt);

    // --- Per-frame helpers ---
    void RenderUiOnly(int screenW, int screenH);
    bool LoadUiOnlyGlueBackgroundModel();
    bool LoadUiOnlyGlueSpriteLayers();
    void RenderUiOnlyGlueBackground(int screenW, int screenH);
    void RenderUiOnlyGlueSpriteLayers(int screenW, int screenH);
    void StepSimulation(float dt);
    void SyncAndReconcileVisuals();
    void CollectUnitOverlays(int screenW, int screenH);
    void CaptureScreenshotIfNeeded(int screenW, int screenH);
    bool WriteFramebufferPng(const std::string& path, int screenW, int screenH);
    void MaybeExitAfterFrame();

    // --- Input helpers ---
    void OnMouseDown(const sapp_event* ev);
    void OnMouseUp(const sapp_event* ev);
    void OnMouseMove(const sapp_event* ev);
    void OnMouseScroll(const sapp_event* ev);
    void OnKeyDown(const sapp_event* ev);

    // --- Selection helpers ---
    void SetSelectedUnits(const std::vector<CUnit*>& units, int primaryHandle = -1, bool rebuildCommandCard = true);
    void SelectSingleUnit(CUnit* unit, bool additive);
    void SelectUnitsInRect(const struct ScreenRect& rect, bool additive);
    void RefreshSelectionUi(bool rebuildCommandCard);
    void ClearSelection();

    // --- Unit picking ---
    CUnit* PickSimulationUnitAtScreenPoint(float screenX, float screenY,
                                           const Mat4& view, const Mat4& proj,
                                           int screenW, int screenH,
                                           int ignoreHandle = -1,
                                           bool preferBuildings = true,
                                           bool strictScreenTargeting = false);
    CDestructable* PickSimulationDestructableAtScreenPoint(float screenX, float screenY,
                                                           const Mat4& view, const Mat4& proj,
                                                           int screenW, int screenH);

    // --- Init helpers ---
    void OpenDataSources();
    void LoadCommonJassFiles(JassVM& vm);
    void ReadMapFile(const std::string& mapPath, const std::string& fileName,
                     std::vector<uint8_t>& out);
    void WireJassCallbacks();
    void DumpUnsupportedDiagnostics();
    void EnsureHumanMeleeStartsFromMapConfig();
    void EnsureHumanMeleeStartForPlayer(int playerIndex, float startX, float startY);
    void FlushStartupSimulationState();
    void FocusCameraAndMinimapOnInitialSpawn();
    void RefreshRuntimeUnitSplats();
    void DebugSelectFirstUnitForHud();

    // --- Core state ---
    GameState state_ = GameState::BOOTSTRAP;
    GameState pendingState_ = GameState::BOOTSTRAP;
    bool stateTransition_ = false;
    bool initialized_ = false;
    bool running_ = true;
    int frameCount_ = 0;
    int exitAfterFrame_ = -1;
    bool uiOnlyMode_ = false;
    bool debugHudGameMode_ = false;
    bool hideHud_ = false;
    bool terrainOnly_ = false;
    bool dumpUnsupported_ = false;
    bool diagnoseMapOnly_ = false;
    int debugSceneRenderStart_ = 0;
    int debugSceneRenderCount_ = -1;
    std::string debugSceneRenderKind_;
    bool debugUiCommandCard_ = false;
    bool debugUiGameMenu_ = false;
    bool debugUiResources_ = false;
    int debugUiGold_ = 0;
    int debugUiLumber_ = 0;
    int debugUiFoodUsed_ = 0;
    int debugUiFoodCap_ = 0;
    std::string debugUiTimeText_ = "12:00";
    std::string debugUiUpperButton_;
    std::string debugUiGameMenuPanel_;
    std::vector<std::string> debugUiGameMenuActions_;
    bool debugUiGlueMenu_ = false;
    std::string debugUiGlueMenuPage_;
    std::vector<std::string> debugUiGlueMenuActions_;
    std::string hudXamlPath_ = "assets/ui/Hud.xaml";
    float simulationAccumulator_ = 0.0f;
    float lastTime_ = 0.0f;

    WarsmashConfig config_;

    // Rendering
    SceneManager scene_;
    TerrainRenderer simpleTerrain_;
    TexturedTerrain texturedTerrain_;
    bool terrainLoaded_ = false;
    bool mapConfigInitialized_ = false;
    bool loadingInitialized_ = false;
    bool loadingFailed_ = false;
    bool overviewCamera_ = false;
    std::string loadingFailureReason_;
    W3EMap map_;
    W3iInfo w3i_;
    TileSet tileset_;
    OrbitCamera camera_;
    Minimap minimap_;
    MdxViewer mdxViewer_;
    MdxModel glueBackgroundModel_;
    MdxViewer glueBackgroundViewer_;
    bool glueBackgroundLoaded_ = false;
    std::string glueBackgroundPath_;
    struct GlueSpriteLayer {
        MdxModel model;
        MdxViewer viewer;
        bool loaded = false;
        bool waitingForStand = false;
        Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
        Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);
        bool billboardPlane = false;
        std::string birthSequence;
        std::string standSequence;
        std::string path;
    };
    GlueSpriteLayer glueSpriteTopLeft_;
    GlueSpriteLayer glueSpriteTopRight_;
    GlueSpriteLayer glueMainMenuLogo_;
    bool glueSpriteLayersLoaded_ = false;
    DayNightCycle dayNight_;

    // Splat metadata
    SplatMetadataStore splatMeta_;

    // Resources
    MpqManager mpq_;
    std::string mapPath_;

    // Simulation
    CSimulation simulation_;
    CUnitData unitData_;
    CDestructableData destructableData_;
    CItemData itemData_;
    CUpgradeData upgradeData_;
    WpmData wpm_;
    bool simulationInitialized_ = false;
    bool debugSelectFirstUnitForHud_ = false;

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

    // Selection
    std::vector<int> selectedUnitHandles_;
    std::unordered_set<int> selectedUnitHandlesSet_;
    int selectedUnitHandle_ = -1;
    War3ID selectedUnitTypeId_;
    bool selectionPressActive_ = false;
    bool selectionDragVisible_ = false;
    float selectionStartX_ = 0.0f;
    float selectionStartY_ = 0.0f;
    float selectionCurrentX_ = 0.0f;
    float selectionCurrentY_ = 0.0f;
};
