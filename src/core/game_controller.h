#pragma once
enum class GameState : int {
    BOOTSTRAP,
    MENU,
    MAP_CONFIG,
    LOADING,
    IN_GAME,
    EXIT_GAME,
};
#include "warsmash_config.h"
#include "unit_presentation_system.h"
#include "world_scene_session.h"
#include "input_action_map.h"
#include "engine_profiler.h"
#include "../engine/engine_kernel.h"
#include "../render/render_graph.h"
#include "../ui/ui_data_binder.h"
#include "../render/scene.h"
#include "../render/camera.h"
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
#include "../data/unitdata.h"
#include "../data/abilitydata.h"
#include "../data/destructabledata.h"
#include "../data/doodaddata.h"
#include "../data/itemdata.h"
#include "../data/upgradedata.h"
#include "../data/gameplay_constants.h"
#include "../ui/MainUI.h"
#include "../audio/AudioSystem.h"
#include "../audio/KeyedSounds.h"
#include "../audio/AckSoundBank.h"
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

#include "../ecs/hero_carry.h"

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
    engine::EngineKernel& GetEngineKernel() { return engineKernel_; }
    const engine::EngineKernel& GetEngineKernel() const { return engineKernel_; }
    SceneManager& GetScene() { return scene_; }
    CSimulation& GetSimulation() { return simulation_; }
    MainUI& GetUI() { return ui_; }
    AudioSystem& GetAudio() { return audio_; }
    TexturedTerrain& GetTexturedTerrain() { return texturedTerrain_; }
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
    core::WorldSceneSession& GetSceneSession() { return sceneSession_; }
    const core::WorldSceneSession& GetSceneSession() const { return sceneSession_; }
    core::InputActionMap& GetInputActionMap() { return inputActionMap_; }
    const core::InputActionMap& GetInputActionMap() const { return inputActionMap_; }
    core::CommandBufferPipeline& GetCommandPipeline() { return commandPipeline_; }
    const core::CommandBufferPipeline& GetCommandPipeline() const { return commandPipeline_; }
    core::EngineProfiler& GetProfiler() { return profiler_; }
    const core::EngineProfiler& GetProfiler() const { return profiler_; }
    render::RenderGraph& GetRenderGraph() { return scene_.GetRenderGraph(); }
    const render::RenderGraph& GetRenderGraph() const { return scene_.GetRenderGraph(); }
    ui::UIDataBinder& GetUIDataBinder() { return ui_.GetDataBinder(); }
    const ui::UIDataBinder& GetUIDataBinder() const { return ui_.GetDataBinder(); }
    ReplayLog& GetReplayLog() { return replayLog_; }
    const ReplayLog& GetReplayLog() const { return replayLog_; }

    // Selection access (for JASS natives / splat refresh)
    const std::unordered_set<int>& GetSelectedUnitHandles() const { return selectedUnitHandlesSet_; }
    bool IsUnitSelected(int handleId) const;
    std::vector<CUnit*> GetSelectedSimulationUnits() const;

    // Phase 5 Stage B Modernization: Dedicated UnitPresentationSystem subsystem
    // owns all unit/item/effect visual state and subscribes to SimEventBus signals.
    using UnitVisualSeed = UnitPresentationSystem::UnitVisualSeed;
    UnitPresentationSystem& GetPresentation() { return presentation_; }
    const UnitPresentationSystem& GetPresentation() const { return presentation_; }

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
    void CollectTextTagOverlays(int screenW, int screenH);
    void SyncLeaderboard();
    void SyncMultiboard();
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
    // Minimap command routing (retail: right-click issues SMART, targeted
    // abilities resolve through the minimap too). Returns false when the
    // screen point is outside the minimap.
    bool MinimapWorldFromScreen(float screenX, float screenY, float* worldX, float* worldY) const;
    // Nearest live, visible unit to a world point within radius (2D).
    // Minimap-target approximation: clicks resolve to sub-pixel units.
    CUnit* PickUnitNearWorldPoint(float worldX, float worldY, float radius) const;

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
    // Campaign carry-over: snapshot human heroes before a ChangeLevel reload
    // and respawn them after the next map initializes (level/XP/skills/
    // items). Stale snapshots are dropped, never replayed twice.
    void CollectCarriedHeroes();
    void SpawnCarriedHeroes();
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
    // ChangeLevel hero carry-over (see CarriedHero): filled on the way out,
    // consumed once on the way in. Fresh launches never inherit stale data.
    std::vector<CarriedHero> carriedHeroes_;
    bool arrivedViaChangeLevel_ = false;
    float lastTime_ = 0.0f;
    // MIDI ambient re-evaluation throttle (day/night + env switches).
    float ambientCheckAcc_ = 0.0f;
    void UpdateMidiAmbient(float dt);
    // Map-music bridge: JASS music state (PlayMusic/StopMusic/volume)
    // drives AudioSystem. Change-gated so tracks never restart.
    void SyncMusic();
    std::string musicSyncedTrack_;
    bool musicSyncedPlaying_ = false;
    float musicSyncedVolume_ = -1.0f;

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

    // Phase 12 Unified Engine Foundation Kernel & Modernized Subsystems
    engine::EngineKernel engineKernel_;
    core::WorldSceneSession sceneSession_;
    core::InputActionMap inputActionMap_;
    core::CommandBufferPipeline commandPipeline_;
    core::EngineProfiler& profiler_ = core::EngineProfiler::Instance();
    ReplayLog replayLog_;

    // Presentation & Rendering
    UnitPresentationSystem presentation_;
    SceneManager scene_;
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
    KeyedSounds uiSounds_;
    AckSoundBank ackBank_;
    // Play a keyed UI sound (UISounds.slk) with fallback to the preloaded
    // ui_click/ui_error samples when the table is missing.
    int PlayUiSound(const std::string& key, float volume = 0.5f);
    // Unit voice response (What/Yes/YesAttack) at the unit's position.
    // No-op when the bank, set, or backend is missing.
    void PlayUnitResponse(CUnit* unit, const char* kind);
    // Selection voice with Pissed escalation (ref UnitSound Pissed rows):
    // consecutive selections of the same unit rotate to the annoyed set.
    void PlaySelectResponse(CUnit* unit);

    // UI
    MainUI ui_;
    // Owned per-frame overlay cache (MainUI keeps a pointer to it; must
    // outlive the frame — previously a dangling local).
    std::vector<UnitOverlayInfo> overlayCache_;
    std::vector<TextTagOverlayInfo> textTagCache_;

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
    // Consecutive-selection counter for Pissed escalation.
    int lastResponseHandle_ = -1;
    int responseCount_ = 0;
    War3ID selectedUnitTypeId_;
    bool selectionPressActive_ = false;
    bool selectionDragVisible_ = false;
    float selectionStartX_ = 0.0f;
    float selectionStartY_ = 0.0f;
    float selectionCurrentX_ = 0.0f;
    float selectionCurrentY_ = 0.0f;
};
