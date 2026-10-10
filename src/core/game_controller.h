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
#include "../data/abilitydata.h"
#include "../data/destructabledata.h"
#include "../data/doodaddata.h"
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
#include <array>
#include <string>
#include <vector>
#include <deque>
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
    CDoodadData& GetDoodadData() { return doodadData_; }
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
    std::unordered_map<int, std::deque<animation_utils::PendingUnitAnimation>> simUnitQueuedAnimations;
    std::unordered_map<int, std::vector<std::string>> simUnitAnimationPropertyTags;
    std::unordered_set<int> simUnitVisualRegistrationFailed;
    std::unordered_map<int, int> simItemToVisualIdx;
    // JASS effect id -> scene attached-model index (-1 = point transient
    // already spawned once). Reconciled every frame in SyncJassEffectVisuals.
    std::unordered_map<int, int> effectVisuals_;
    std::unordered_set<int> simItemVisualRegistrationFailed;

    bool EnsureSimulationUnitVisual(CUnit* unit);
    bool EnsureSimulationItemVisual(CItem* item);

    // Screenshot
    std::string screenshotPath;
    int screenshotFrame = -1;

    // Mouse state
    float mouseDX = 0, mouseDY = 0, mouseScroll = 0;
    float mousePosX_ = 0, mousePosY_ = 0;
    int hoveredUnitHandle_ = -1;
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
    // Fog-of-war visibility for rendering (ref black mask/grey fog): buildings
    // and statics show once explored; mobile units show to owner/allies or
    // while visible (with stealth vs detectors). Picking follows visibility.
    bool IsUnitVisibleInFog(const CUnit* unit) const;
    void SyncFogStaticsVisibility();
    void SyncJassEffectVisuals();
    void SyncFogOfWar();
    // Warsmash MeleeUIMinimap parity: fog overlay (black mask / grey fog)
    // plus per-unit dots (player colors, gold/neutral specials, hero pulse
    // with PASSIVE-ally vs enemy tint). Called before the minimap draw.
    void SyncMinimapLayer();
    // HUD live-data push (resource bar, upkeep indicator, timer dialog):
    // mirrors the sim/local-player state into MainUI cached fields so the
    // FDF console shows live values instead of UI-only debug defaults.
    void SyncHudLiveData();
    void SyncLightnings();
    void SyncScriptDecor();
    void SyncWeather(float dt);
    void CollectUnitOverlays(int screenW, int screenH);
    void RefreshRuntimeUnitSplats();
    void AddActiveEventSplatsToTerrain();
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
    bool IssuePendingCommandAtScreenPoint(float screenX, float screenY);

    // --- Init helpers ---
    void OpenDataSources();
    void HandleGlueMapLaunchRequest(const std::string& mapPath, int localPlayerIndex);
    bool StartPendingGlueMapLaunch(int screenW, int screenH);
    void LoadCommonJassFiles(JassVM& vm);
    void ReadMapFile(const std::string& mapPath, const std::string& fileName,
                     std::vector<uint8_t>& out);
    void WireJassCallbacks();
    void DumpUnsupportedDiagnostics();
    void EnsureHumanMeleeStartsFromMapConfig();
    void EnsureHumanMeleeStartForPlayer(int playerIndex, float startX, float startY);
    // Safety net: any COMPUTER + slot-active player without an AI
    // controller (map script never called StartMeleeAI) gets a
    // MeleeAIController so computer slots always play.
    void EnsureComputerAIStarted();
    // Diagnostic (WARSMASH_AI_STATUS=1): per-10s sim status of computer
    // players (race/units/buildings/gold/AI thinks+waves) to validate the
    // single-player game loop in real runs.
    void MaybeLogAIStatus();
    void FlushStartupSimulationState();
    void FocusCameraAndMinimapOnInitialSpawn();
    void DebugSelectFirstUnitForHud();
    void RunGameplaySmoke();

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
    bool debugGameplaySmoke_ = false;
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
    std::string hudLayoutPath_ = "console";
    bool pendingGlueMapLaunch_ = false;
    std::string pendingGlueMapLaunchRequestedPath_;
    std::string pendingGlueMapLaunchResolvedPath_;
    int pendingGlueMapLaunchLocalPlayer_ = 0;
    bool glueMapLaunchInProgress_ = false;
    float simulationAccumulator_ = 0.0f;
    float lastTime_ = 0.0f;

    WarsmashConfig config_;

    // Scripted-fog raster cache (re-uploaded only when CFogSystem changes).
    uint64_t fogVersionSynced_ = 0;
    std::vector<unsigned char> fogGrid_;
    // Minimap fog-overlay cache (same version gate, minimap extents).
    uint64_t minimapFogVersionSynced_ = uint64_t(-1);
    std::vector<uint8_t> minimapFogRgba_;
    std::vector<Minimap::UnitDot> minimapDotsScratch_;
    // Static-instance fog sweep gate (doodads/items hide under black mask).
    uint64_t fogStaticsVersionSynced_ = uint64_t(-1);
    // JASS lightning bolts mirrored into the scene (ids live here).
    bool lightningParamsLoaded_ = false;
    std::unordered_set<int> lightningSynced_;
    // Last seen text-clear sequence (ClearTextMessages drops shown toasts).
    int lastTextClearSeq_ = 0;

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
    int glueBackgroundCameraSequenceIndex_ = -1;
    float glueBackgroundCameraElapsedMs_ = 0.0f;
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
    struct ActiveEventSplat {
        std::string key;
        std::string texturePath;
        float x = 0.0f;
        float y = 0.0f;
        float halfSize = 64.0f;
        float lifeRemaining = 0.5f;
        float lifeSpan = 0.5f;
        std::array<std::array<float, 4>, 3> colors = {{
            {{255.0f, 255.0f, 255.0f, 255.0f}},
            {{255.0f, 255.0f, 255.0f, 255.0f}},
            {{255.0f, 255.0f, 255.0f, 0.0f}}
        }};
        std::array<float, 3> intervalTimes = {{0.0f, 0.0f, 0.0f}};
        std::array<std::array<float, 3>, 2> uvIntervals = {{
            {{0.0f, 0.0f, 0.0f}},
            {{0.0f, 0.0f, 0.0f}}
        }};
        int intervalCount = 0;
        int columns = 1;
        int rows = 1;
        int blendMode = 0;
        bool unshaded = false;
        bool highPriority = true;
        std::vector<SplatModel::MeshVertex> meshVertices;
        std::vector<uint32_t> meshIndices;
    };
    std::vector<ActiveEventSplat> activeEventSplats_;
    int nextEventSplatId_ = 1;

    // Resources
    MpqManager mpq_;
    std::string mapPath_;

    // Simulation
    CSimulation simulation_;
    CUnitData unitData_;
    CDestructableData destructableData_;
    CDoodadData doodadData_;
    CItemData itemData_;
    CUpgradeData upgradeData_;
    CAbilityData abilityData_;
    WpmData wpm_;
    bool simulationInitialized_ = false;
    bool debugSelectFirstUnitForHud_ = false;

    // Audio
    AudioSystem audio_;

    // UI
    MainUI ui_;
    // Owned per-frame overlay cache (MainUI keeps a pointer to it; must
    // outlive the frame — previously a dangling local).
    std::vector<UnitOverlayInfo> overlayCache_;

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
