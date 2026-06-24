#include "game_controller.h"
#include "../render/renderer.h"
#include "../render/picking.h"
#include "../render/unit_selection_geom.h"
#include "../ecs/animation_utils.h"
#include "../ecs/unit_text_utils.h"
#include "../ecs/orders/order.h"
#include "../ecs/widget.h"
#include "../ecs/destructable.h"
#include "../ecs/item.h"
#include "../core/path_utils.h"
#include "../platform/platform.h"
#include "../debug/move_debug.h"
#include "../third_party/sokol/sokol_app.h"
#include "../third_party/sokol/sokol_gfx.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>

#ifndef WITHOUT_STORMLIB
#include <StormLib.h>
#endif

namespace fs = std::filesystem;

static const char* DEFAULT_MAP = "Maps/FrozenThrone/(4)Avalanche.w3x";

// --- Helpers (animation / visual) ---

static bool ApplyAnimationRequestToViewer(int handleId,
                                          MdxViewer* viewer,
                                          const animation_utils::PendingUnitAnimation& request,
                                          GameController* g) {
    if (!viewer || !viewer->GetAnimator()) return false;

    bool loop = false;
    bool queueStandAfterCurrent = true;
    std::string lowerName;
    if (animation_utils::GetRequestedAnimationLowerName(viewer, request, lowerName)) {
        animation_utils::ClassifyScriptedAnimationName(lowerName, loop, queueStandAfterCurrent);
    }

    bool played = false;
    if (request.useSequenceIndex) {
        played = viewer->PlaySequenceByIndex(request.sequenceIndex, loop, 1.0f, queueStandAfterCurrent);
    } else if (!request.sequenceName.empty()) {
        played = viewer->PlaySequenceByName(request.sequenceName.c_str(), loop, 1.0f, queueStandAfterCurrent);
    }

    if (played) {
        g->simUnitScriptedAnimations[handleId] = {request, loop, queueStandAfterCurrent};
    }
    return played;
}

static void ApplyPendingUnitAnimation(int handleId, MdxViewer* viewer, GameController* g) {
    if (!viewer || !viewer->GetAnimator()) return;
    const auto pendingIt = g->simUnitPendingAnimations.find(handleId);
    if (pendingIt == g->simUnitPendingAnimations.end()) return;
    ApplyAnimationRequestToViewer(handleId, viewer, pendingIt->second, g);
    g->simUnitPendingAnimations.erase(pendingIt);
}

namespace {

void PlayResolvedUnitAnimation(CUnit* unit, MdxViewer* viewer, GameController* g) {
    if (!unit || !viewer) return;

    const int handleId = unit->GetHandleId();
    auto& splatMeta = g->GetSplatMeta();
    const auto* scriptTags = &g->simUnitAnimationPropertyTags;

    auto GetSplatMetaFn = [&](const CUnit& u) -> const SplatMetadataStore::UnitRenderSplatMeta* {
        return splatMeta.GetUnitRenderSplatMeta(u);
    };

    CResourceType carriedType = g->simUnitVisualResourceTags.count(handleId)
        ? g->simUnitVisualResourceTags[handleId] : CResourceType::NONE;
    CBehaviorCategory behaviorCategory = unit_text_utils::GetUnitCurrentBehaviorCategory(unit);

    // If script requested a specific animation, use it
    ApplyPendingUnitAnimation(handleId, viewer, g);

    const auto scriptIt = g->simUnitScriptedAnimations.find(handleId);
    if (scriptIt != g->simUnitScriptedAnimations.end()) {
        const auto& state = scriptIt->second;
        bool applied = false;
        if (state.request.useSequenceIndex) {
            applied = viewer->PlaySequenceByIndex(state.request.sequenceIndex, state.loop, 1.0f,
                                                  state.queueStandAfterCurrent);
        } else if (!state.request.sequenceName.empty()) {
            applied = viewer->PlaySequenceByName(state.request.sequenceName.c_str(), state.loop, 1.0f,
                                                  state.queueStandAfterCurrent);
        }
        if (applied) {
            return;
        }
        g->simUnitScriptedAnimations.erase(scriptIt);
    }

    // Death / decay
    auto modeIt = g->simUnitVisualAnimationModes.find(handleId);
    if (modeIt != g->simUnitVisualAnimationModes.end()) {
        auto mode = modeIt->second;
        if (mode == animation_utils::UnitVisualAnimationMode::DEAD ||
            mode == animation_utils::UnitVisualAnimationMode::CORPSE ||
            mode == animation_utils::UnitVisualAnimationMode::BONE_CORPSE ||
            mode == animation_utils::UnitVisualAnimationMode::DISSIPATE) {
            animation_utils::PlayDeathVisualForUnit(unit, viewer, mode, g->GetSimulation());
            return;
        }
    }

    // Spell
    if (auto* spell = animation_utils::GetSpellBehavior(unit)) {
        if (spell->IsCasting() || spell->IsBackswing()) {
            animation_utils::PlaySpellVisualForUnit(unit, viewer, scriptTags);
            return;
        }
    }

    // Building construction (no dedicated method; use stand and set construction flag)
    if (g->simUnitVisualConstructionStates.count(handleId)) {
        viewer->PlayStandSequence();
        return;
    }

    // Determine if moving
    bool isMoving = animation_utils::ComputeUnitVisualIsMoving(unit, g->simUnitVisualLastPositions);
    bool wasMoving = g->simUnitVisualMovingStates.count(handleId) && g->simUnitVisualMovingStates[handleId];
    bool justStopped = !isMoving && wasMoving;

    if (isMoving) {
        bool playedWalk = false;
        bool hasWorkTag = false;
        auto propTags = g->simUnitVisualPropertyTags.count(handleId)
            ? g->simUnitVisualPropertyTags[handleId] : std::vector<std::string>{};
        auto scriptTagsVec = g->simUnitAnimationPropertyTags.count(handleId)
            ? g->simUnitAnimationPropertyTags[handleId] : std::vector<std::string>{};
        for (const auto& t : propTags) if (t == "work") hasWorkTag = true;
        for (const auto& t : scriptTagsVec) if (t == "work") hasWorkTag = true;

        if (hasWorkTag) {
            playedWalk = animation_utils::PlayWalkWorkVisualForUnit(unit, viewer, behaviorCategory, carriedType,
                                                                     splatMeta, scriptTags);
        }
        if (!playedWalk) {
            animation_utils::PlayWalkVisualForUnit(unit, viewer, behaviorCategory, carriedType,
                                                    splatMeta, scriptTags);
        }
    } else {
        bool isWorking = false;
        auto propTags = g->simUnitVisualPropertyTags.count(handleId)
            ? g->simUnitVisualPropertyTags[handleId] : std::vector<std::string>{};
        auto scriptTagsVec = g->simUnitAnimationPropertyTags.count(handleId)
            ? g->simUnitAnimationPropertyTags[handleId] : std::vector<std::string>{};
        for (const auto& t : propTags) if (t == "work") isWorking = true;
        for (const auto& t : scriptTagsVec) if (t == "work") isWorking = true;

        if (isWorking) {
            animation_utils::PlayWorkVisualForUnit(unit, viewer, behaviorCategory, carriedType,
                                                    splatMeta, scriptTags);
        } else {
            animation_utils::PlayReadyVisualForUnit(unit, viewer, behaviorCategory, carriedType,
                                                     splatMeta, scriptTags);
        }
    }

    g->simUnitVisualMovingStates[handleId] = isMoving;
}

void SyncSimulationUnitAnimation(CUnit* unit, MdxViewer* viewer, GameController* g) {
    if (!unit || !viewer) return;
    const int handleId = unit->GetHandleId();

    const auto lastPosIt = g->simUnitVisualLastPositions.find(handleId);
    bool posChanged = false;
    if (lastPosIt != g->simUnitVisualLastPositions.end()) {
        float dx = unit->GetX() - lastPosIt->second.first;
        float dy = unit->GetY() - lastPosIt->second.second;
        posChanged = (dx * dx + dy * dy) > 0.01f;
    } else {
        posChanged = true;
    }

    if (posChanged) {
        g->simUnitVisualLastPositions[handleId] = {unit->GetX(), unit->GetY()};
    }

    // Re-evaluate animation if state changed
    bool isMoving = animation_utils::ComputeUnitVisualIsMoving(unit, g->simUnitVisualLastPositions);
    bool wasMoving = g->simUnitVisualMovingStates.count(handleId) && g->simUnitVisualMovingStates[handleId];

    bool needsResolve = posChanged || (isMoving != wasMoving);
    if (g->simUnitPendingAnimations.count(handleId)) needsResolve = true;
    if (g->simUnitQueuedAnimations.count(handleId)) needsResolve = true;

    if (needsResolve) {
        PlayResolvedUnitAnimation(unit, viewer, g);
    }

    // Handle queued animations
    const auto queuedIt = g->simUnitQueuedAnimations.find(handleId);
    if (queuedIt != g->simUnitQueuedAnimations.end()) {
        if (!viewer->IsCurrentSequenceLooping() && viewer->IsCurrentSequenceFinished()) {
            ApplyAnimationRequestToViewer(handleId, viewer, queuedIt->second, g);
            g->simUnitQueuedAnimations.erase(queuedIt);
        }
    }

    // Sync position/rotation via SceneManager
    float visualHeight = unit_selection_geom::ComputeUnitVisualHeight(
        *unit, g->GetTexturedTerrain().IsValid(),
        [&](float x, float z) { return g->GetTexturedTerrain().GetGroundHeight(x, z); });
    {
        float pos[3] = {unit->GetX(), visualHeight, unit->GetY()};
        auto vit = g->simUnitToVisualIdx.find(handleId);
        if (vit != g->simUnitToVisualIdx.end()) {
            g->GetScene().UpdateInstancePosition(vit->second, pos, unit->GetFacing());
        }
    }
}

} // namespace

// ======================================================================
// GameController implementation
// ======================================================================

GameController::GameController() = default;
GameController::~GameController() = default;

void GameController::ParseArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "--screenshot") == 0 && i + 1 < argc) {
            screenshotPath = argv[++i];
            screenshotFrame = 1;
        } else if (std::strncmp(arg, "--run=", 6) == 0) {
            mapPath_ = arg + 6;
        } else if (std::strcmp(arg, "--run") == 0 && i + 1 < argc) {
            mapPath_ = argv[++i];
        } else if (std::strncmp(arg, "--map=", 6) == 0) {
            mapPath_ = arg + 6;
        } else if (std::strcmp(arg, "--map") == 0 && i + 1 < argc) {
            mapPath_ = argv[++i];
        } else if (arg[0] != '-') {
            mapPath_ = arg;
        }
    }
}

bool GameController::Initialize() {
    std::cout << "Warsmash C++ Recreation - Initializing..." << std::endl;
    {
        std::ofstream(MoveDebugLogPath(), std::ios::trunc).close();
        MoveDebugLogLine("[MoveDebug] session_begin");
    }

    if (!Renderer::Initialize()) return false;

    fs::path project_root = PROJECT_SOURCE_DIR;
    fs::path iniPath = project_root / "resources" / "warsmash.ini";
    config_.Load(iniPath.string());

    OpenDataSources();

    // Determine map path
    fs::path resourcesDir = project_root / "resources";
    if (mapPath_.empty()) {
        std::vector<fs::path> mapFiles;
        if (fs::exists(resourcesDir)) {
            for (const auto& entry : fs::recursive_directory_iterator(resourcesDir)) {
                std::string ext = entry.path().extension().string();
                if (ext == ".w3x" || ext == ".w3m")
                    mapFiles.push_back(entry.path());
            }
        }
        fs::path defaultPath = resourcesDir / DEFAULT_MAP;
        if (fs::exists(defaultPath))
            mapPath_ = defaultPath.string();
        else if (!mapFiles.empty())
            mapPath_ = mapFiles[0].string();
    } else {
        fs::path mapFilePath(mapPath_);
        if (mapFilePath.is_relative()) {
            fs::path resolved = resourcesDir / mapFilePath;
            if (fs::exists(resolved))
                mapPath_ = resolved.string();
        }
    }

    std::cout << "\nMap: " << fs::path(mapPath_).filename() << std::endl;

    if (!mapPath_.empty() && fs::exists(mapPath_)) {
        mpq_.Open(mapPath_);
    }

    scene_.Init(mpq_);
    splatMeta_.Load(mpq_);

    audio_.Init();
    if (audio_.IsValid() && mpq_.ArchiveCount() > 0) {
        audio_.SetMpqManager(&mpq_);
        audio_.LoadSound("ui_click", "UI\\Mouse\\HumanClick1.wav", &mpq_);
        audio_.LoadSound("ui_error", "UI\\Mouse\\HumanClickError.wav", &mpq_);
    }

    ui_.Init(&mpq_, 1280, 720);
    initialized_ = true;
    return true;
}

void GameController::OpenDataSources() {
    fs::path projectRoot = PROJECT_SOURCE_DIR;
    fs::path resourcesDir = projectRoot / "resources";

    if (!config_.dataSources.empty()) {
        for (const auto& ds : config_.dataSources) {
            if (ds.first == "MPQ" || ds.first == "mpq") {
                auto p = path_utils::ResolveConfiguredPath(
                    projectRoot, resourcesDir, ds.second);
                if (fs::exists(p)) mpq_.Open(p.string());
                else std::cerr << "MpqStorm: configured MPQ not found " << p.string() << std::endl;
            }
        }
    }

    if (mpq_.ArchiveCount() == 0) {
        for (const auto& name : {"War3Patch.mpq", "War3xLocal.mpq", "war3x.mpq", "war3.mpq"}) {
            fs::path p = resourcesDir / name;
            if (fs::exists(p)) mpq_.Open(p.string());
        }
    }
}

void GameController::ReadMapFile(const std::string& mapPath, const std::string& fileName,
                                  std::vector<uint8_t>& out) {
#ifndef WITHOUT_STORMLIB
    HANDLE hMpq = nullptr;
    if (!SFileOpenArchive(mapPath.c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) return;
    HANDLE hFile = nullptr;
    if (!SFileOpenFileEx(hMpq, fileName.c_str(), SFILE_OPEN_BASE_FILE, &hFile)) {
        SFileCloseArchive(hMpq); return;
    }
    DWORD sz = SFileGetFileSize(hFile, nullptr);
    out.resize(sz);
    DWORD read = 0;
    SFileReadFile(hFile, out.data(), sz, &read, nullptr);
    SFileCloseFile(hFile);
    SFileCloseArchive(hMpq);
#else
    (void)mapPath; (void)fileName; (void)out;
#endif
}

// --- State machine ---

void GameController::TransitionTo(GameState newState) {
    pendingState_ = newState;
    stateTransition_ = true;
}

void GameController::OnEnterState(GameState state) {
    switch (state) {
    case GameState::MAP_CONFIG: InitMapConfig(); break;
    case GameState::LOADING:    InitLoading();   break;
    case GameState::IN_GAME:    InitInGame();    break;
    default: break;
    }
}

void GameController::OnLeaveState(GameState state) { (void)state; }

// --- Init phases ---

void GameController::InitMapConfig() {
    if (mapConfigInitialized_) {
        std::cout << "InitMapConfig: already initialized; skipping duplicate config phase" << std::endl;
        if (!loadingFailed_ && !loadingInitialized_ && !simulationInitialized_) {
            TransitionTo(GameState::LOADING);
        }
        return;
    }

    if (mapPath_.empty() || !fs::exists(mapPath_)) {
        loadingFailed_ = true;
        loadingFailureReason_ = "Map file not found: " + mapPath_;
        std::cerr << loadingFailureReason_ << std::endl;
        return;
    }

    scene_.ResetLoadStats();

    W3ILoadFromMpq(mapPath_, w3i_);
    W3IPrint(w3i_);

    configEnv_ = std::make_unique<JassConfigEnv>();
    if (configEnv_->Init(mpq_, mapPath_, config_.jassFileList)) {
        configEnv_->Config();
        int configuredCount = configEnv_->GetConfiguredPlayerCount();
        if (configuredCount > 0) {
            std::cout << "JASS Config: " << configuredCount << " players configured" << std::endl;
        }
    }

    mapConfigInitialized_ = true;
    TransitionTo(GameState::LOADING);
}

void GameController::InitLoading() {
    if (simulationInitialized_) {
        std::cout << "InitLoading: simulation already initialized; skipping duplicate loader creation" << std::endl;
        return;
    }
    if (loadingFailed_) {
        std::cerr << "InitLoading: skipped because loading already failed: "
                  << loadingFailureReason_ << std::endl;
        return;
    }
    if (loadingInitialized_) {
        std::cout << "InitLoading: already initialized; skipping duplicate loader creation" << std::endl;
        return;
    }

    mapLoader_ = std::make_unique<MapLoader>(
        mpq_, simulation_, unitData_, destructableData_,
        itemData_, scene_, texturedTerrain_, tileset_, map_, w3i_, wpm_);
    mapLoader_->SetConfigData(configEnv_ ? configEnv_->GetData() : nullptr);
    mapLoader_->CreateTasks(mapPath_, 0);
    loadingProgress_ = 0.0f;
    loadingInitialized_ = true;
    std::cout << "Loading started..." << std::endl;
}

void GameController::InitInGame() {
    if (simulationInitialized_) {
        std::cout << "InitInGame: already initialized; skipping duplicate entry" << std::endl;
        return;
    }

    if (loadingFailed_) {
        std::cerr << "InitInGame: skipped because loading failed: "
                  << loadingFailureReason_ << std::endl;
        return;
    }
    if (!mapLoader_) {
        loadingFailed_ = true;
        loadingFailureReason_ = "MapLoader was not initialized";
        std::cerr << "InitInGame: " << loadingFailureReason_ << std::endl;
        return;
    }
    if (mapLoader_->HasFatalError()) {
        loadingFailed_ = true;
        loadingFailureReason_ = mapLoader_->GetFatalError();
        std::cerr << "InitInGame: map loading failed: "
                  << loadingFailureReason_ << std::endl;
        return;
    }
    if (!texturedTerrain_.IsValid()) {
        loadingFailed_ = true;
        loadingFailureReason_ = "Terrain was not built";
        std::cerr << "InitInGame: " << loadingFailureReason_ << std::endl;
        return;
    }

    if (mapLoader_) {
        for (const auto& seed : mapLoader_->GetUnitVisualSeeds()) {
            simUnitVisualSeeds[seed.handleId] = {
                {seed.scale[0], seed.scale[1], seed.scale[2]},
                seed.skinId, seed.hitpoints, seed.flags
            };
        }
        for (const auto& seed : mapLoader_->GetItemVisualSeeds()) {
            if (seed.handleId >= 0 && seed.visualIndex >= 0) {
                simItemToVisualIdx[seed.handleId] = seed.visualIndex;
            }
        }
        const auto& unitLoadResult = mapLoader_->GetUnitLoadResult();
        int visualUnitCreated = unitLoadResult.visualCreated;
        int visualMissForSimUnits = unitLoadResult.visualMissForSimUnits;
        simulation_.Update();
        simulationInitialized_ = true;
        ui_.SetSimulation(&simulation_);
        for (auto* unit : simulation_.GetUnits()) {
            if (EnsureSimulationUnitVisual(unit))
                visualUnitCreated++;
            else
                visualMissForSimUnits++;
        }
        std::cout << "Simulation: " << simulation_.GetUnits().size() << " units" << std::endl;
        std::cout << "SceneUnitSync: simCreated=" << unitLoadResult.simCreated
                  << " visualCreated=" << visualUnitCreated
                  << " visualMissForSimUnits=" << visualMissForSimUnits << std::endl;

        ui_.SetUnitData(&unitData_);
        if (!simulation_.GetUnits().empty()) {
            War3ID firstType = simulation_.GetUnits()[0]->GetTypeId();
            ui_.PopulateCommandCardForUnitType(firstType);
        }
    }

    terrainLoaded_ = texturedTerrain_.IsValid();

    minimap_.Generate(texturedTerrain_, tileset_, map_, w3i_, &mpq_);

    Vec3 center = texturedTerrain_.GetCenter();
    camera_.SetTarget(center.x, center.y, center.z);
    camera_.SetDistance(2500.0f);
    camera_.SetMaxDistance(5000.0f);
    camera_.SaveHomeState();
    FocusCameraAndMinimapOnInitialSpawn();

    if (mpq_.ArchiveCount() > 0) {
        commonEnv_ = std::make_unique<JassCommonEnv>();
        if (commonEnv_->Init(mpq_, mapPath_, &simulation_, config_.jassFileList,
                             configEnv_ ? configEnv_->GetData() : nullptr)) {
            commonEnv_->Main();
            commonEnv_->PumpQueuedThreads(4096);
            jassNatives_ = commonEnv_->ReleaseNatives();
            WireJassCallbacks();
            if (jassNatives_) jassNatives_->PumpQueuedThreads(4096);
            simulation_.Update();
            EnsureHumanMeleeStartsFromMapConfig();
            FlushStartupSimulationState();
        } else {
            std::cerr << "JASS Common: unavailable; terrain/static scene remains ready" << std::endl;
        }
    } else {
        std::cout << "JASS Common: skipped because no resource MPQs are open" << std::endl;
    }

    std::cout << "Initialization complete. Starting render loop..." << std::endl;
}

void GameController::RunSyncInit() {
    InitMapConfig();
    if (loadingFailed_) {
        pendingState_ = state_;
        stateTransition_ = false;
        return;
    }
    InitLoading();
    if (loadingFailed_) {
        pendingState_ = state_;
        stateTransition_ = false;
        return;
    }
    int stage = 0;
    while (mapLoader_ && !mapLoader_->Process()) {
        stage++;
        std::cout << "  Loading stage " << stage << " ("
                  << (int)(mapLoader_->GetCompletionRatio() * 100.0f) << "%)" << std::endl;
    }
    if (!mapLoader_) {
        loadingFailed_ = true;
        loadingFailureReason_ = "MapLoader was not initialized";
        std::cerr << "RunSyncInit: " << loadingFailureReason_ << std::endl;
        pendingState_ = state_;
        stateTransition_ = false;
        return;
    }
    if (mapLoader_->HasFatalError()) {
        loadingFailed_ = true;
        loadingFailureReason_ = mapLoader_->GetFatalError();
        std::cerr << "RunSyncInit: map loading failed: "
                  << loadingFailureReason_ << std::endl;
        pendingState_ = state_;
        stateTransition_ = false;
        return;
    }
    terrainLoaded_ = texturedTerrain_.IsValid();
    if (!terrainLoaded_) {
        loadingFailed_ = true;
        loadingFailureReason_ = "Terrain was not built";
        std::cerr << "RunSyncInit: " << loadingFailureReason_ << std::endl;
        pendingState_ = state_;
        stateTransition_ = false;
        return;
    }
    scene_.PrintLoadStats();
    InitInGame();
    if (!simulationInitialized_) {
        pendingState_ = state_;
        stateTransition_ = false;
        return;
    }

    state_ = GameState::IN_GAME;
    pendingState_ = GameState::IN_GAME;
    stateTransition_ = false;
}

// --- Wire JASS callbacks ---

void GameController::WireJassCallbacks() {
    if (!jassNatives_) return;

    auto* jassData = jassNatives_->GetData().get();
    if (!jassData) return;

    jassData->camSetPosition = [this](float x, float y, float z) {
        camera_.SetTarget(x, y, z); camera_.ResetToHome();
    };
    jassData->camSetTarget = [this](float x, float y, float z) {
        camera_.SetTarget(x, y, z); camera_.ResetToHome();
    };
    jassData->camReset = [this]() { camera_.ResetToHome(); };
    jassData->camGetX = [this]() -> float { return camera_.GetTarget().x; };
    jassData->camGetY = [this]() -> float { return camera_.GetTarget().y; };
    jassData->camGetZ = [this]() -> float { return camera_.GetTarget().z; };
    jassData->camGetEyeX = [this]() -> float { return camera_.GetPosition().x; };
    jassData->camGetEyeY = [this]() -> float { return camera_.GetPosition().y; };
    jassData->camGetEyeZ = [this]() -> float { return camera_.GetPosition().z; };

    jassData->unitSetAnimation = [this](int simHandle, const std::string& animName) {
        animation_utils::PendingUnitAnimation request{animName, -1, false};
        simUnitQueuedAnimations.erase(simHandle);
        auto it = simUnitToVisualIdx.find(simHandle);
        if (it != simUnitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && ApplyAnimationRequestToViewer(simHandle, viewer, request, this)) {
                simUnitPendingAnimations.erase(simHandle);
                return;
            }
        }
        simUnitPendingAnimations[simHandle] = request;
    };
    jassData->unitQueueAnimation = [this](int simHandle, const std::string& animName) {
        animation_utils::PendingUnitAnimation request{animName, -1, false};
        auto it = simUnitToVisualIdx.find(simHandle);
        if (it != simUnitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && viewer->GetAnimator()) {
                bool shouldQueue = !viewer->IsCurrentSequenceLooping() && !viewer->IsCurrentSequenceFinished();
                if (shouldQueue) {
                    simUnitQueuedAnimations[simHandle] = request;
                } else if (ApplyAnimationRequestToViewer(simHandle, viewer, request, this)) {
                    simUnitPendingAnimations.erase(simHandle);
                    return;
                }
            }
        } else {
            simUnitPendingAnimations[simHandle] = request;
        }
    };
    jassData->unitSetAnimationByIndex = [this](int simHandle, int index) {
        animation_utils::PendingUnitAnimation request{"", index, true};
        simUnitQueuedAnimations.erase(simHandle);
        auto it = simUnitToVisualIdx.find(simHandle);
        if (it != simUnitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && ApplyAnimationRequestToViewer(simHandle, viewer, request, this)) {
                simUnitPendingAnimations.erase(simHandle);
                return;
            }
        }
        simUnitPendingAnimations[simHandle] = request;
    };
    jassData->unitAddAnimationProperties = [this](int simHandle, const std::string& properties, bool add) {
        auto tags = animation_utils::ExtractAnimationSecondaryTags(properties);
        if (tags.empty()) return;
        auto& activeTags = simUnitAnimationPropertyTags[simHandle];
        if (add) {
            for (const auto& tag : tags)
                if (std::find(activeTags.begin(), activeTags.end(), tag) == activeTags.end())
                    activeTags.push_back(tag);
        } else {
            activeTags.erase(
                std::remove_if(activeTags.begin(), activeTags.end(),
                    [&](const std::string& t) { return std::find(tags.begin(), tags.end(), t) != tags.end(); }),
                activeTags.end());
            if (activeTags.empty()) simUnitAnimationPropertyTags.erase(simHandle);
        }
        simUnitVisualAnimationModes.erase(simHandle);
    };
    jassData->unitClearAnimations = [this](int simHandle) {
        simUnitPendingAnimations.erase(simHandle);
        simUnitQueuedAnimations.erase(simHandle);
        simUnitScriptedAnimations.erase(simHandle);
        simUnitVisualAnimationModes.erase(simHandle);
    };

    simulation_.SetUnitDeathCallback(
        [this](int unitHandle, int killerHandle, int) {
            if (jassNatives_) {
                jassNatives_->FireDeathEvent(unitHandle, killerHandle, JASS_EVENT_UNIT_DEATH);
            }
        }
    );

    simulation_.SetUnitDamageCallback(
        [this](int unitHandle, int sourceHandle, float damage) {
            if (jassNatives_) {
                jassNatives_->FireDamageEvent(unitHandle, sourceHandle, damage, JASS_EVENT_UNIT_DAMAGED);
            }
        }
    );

    simulation_.SetUnitOrderCallback(
        [this](int unitHandle, int orderId, int targetHandle, float x, float y) {
            if (jassNatives_) {
                jassNatives_->FireOrderEvent(unitHandle, orderId, targetHandle, x, y);
            }
        }
    );

    simulation_.SetItemEventCallback(
        [this](int unitHandle, int itemHandle, int eventType) {
            if (jassNatives_) {
                jassNatives_->FireItemEvent(unitHandle, itemHandle, eventType);
            }
        }
    );

    simulation_.SetItemRemovedCallback(
        [this](int itemHandle) {
            if (jassNatives_) {
                jassNatives_->ForgetItem(itemHandle);
            }
        }
    );
}

// --- Update ---

void GameController::Update(int screenW, int screenH, float dt) {
    if (stateTransition_) {
        OnLeaveState(state_);
        state_ = pendingState_;
        OnEnterState(state_);
        stateTransition_ = false;
    }

    switch (state_) {
    case GameState::LOADING: UpdateLoading(dt); break;
    case GameState::IN_GAME: UpdateInGame(screenW, screenH, dt); break;
    default: break;
    }

    frameCount_++;
}

void GameController::UpdateLoading(float dt) {
    (void)dt;
    if (loadingFailed_) return;
    if (mapLoader_ && mapLoader_->Process()) {
        if (mapLoader_->HasFatalError()) {
            loadingFailed_ = true;
            loadingFailureReason_ = mapLoader_->GetFatalError();
            std::cerr << "UpdateLoading: map loading failed: "
                      << loadingFailureReason_ << std::endl;
            return;
        }
        terrainLoaded_ = texturedTerrain_.IsValid();
        if (!terrainLoaded_) {
            loadingFailed_ = true;
            loadingFailureReason_ = "Terrain was not built";
            std::cerr << "UpdateLoading: " << loadingFailureReason_ << std::endl;
            return;
        }
        scene_.PrintLoadStats();
        TransitionTo(GameState::IN_GAME);
    }
}

void GameController::UpdateInGame(int screenW, int screenH, float dt) {
    // Camera from mouse state
    if (mouseDown[0] || mouseDown[2]) {
        camera_.OnMouseMove(mouseDX, mouseDY, mouseDown[0], mouseDown[2]);
    }
    if (mouseScroll != 0.0f) {
        camera_.OnMouseWheel(mouseScroll);
    }
    mouseDX = 0; mouseDY = 0; mouseScroll = 0;

    // Keyboard WASD panning
    float panSpeed = std::max(100.0f, camera_.GetPosition().y * 0.5f) * dt;
    float panX = 0, panZ = 0;
    if (Platform::IsKeyDown(SAPP_KEYCODE_W) || Platform::IsKeyDown(SAPP_KEYCODE_UP))    panZ += panSpeed;
    if (Platform::IsKeyDown(SAPP_KEYCODE_S) || Platform::IsKeyDown(SAPP_KEYCODE_DOWN))  panZ -= panSpeed;
    if (Platform::IsKeyDown(SAPP_KEYCODE_A) || Platform::IsKeyDown(SAPP_KEYCODE_LEFT))  panX -= panSpeed;
    if (Platform::IsKeyDown(SAPP_KEYCODE_D) || Platform::IsKeyDown(SAPP_KEYCODE_RIGHT)) panX += panSpeed;
    if (panX != 0 || panZ != 0) camera_.Pan(panX, panZ);

    // Player start position jump (keys 1-4)
    for (int k = SAPP_KEYCODE_1; k <= SAPP_KEYCODE_4; k++) {
        if (Platform::WasKeyPressed(k)) {
            int pi = k - SAPP_KEYCODE_1;
            float fx = 0.25f + (pi % 2) * 0.5f;
            float fy = 0.25f + (pi / 2) * 0.5f;
            float wx = w3i_.cameraBounds[0] + (w3i_.cameraBounds[1] - w3i_.cameraBounds[0]) * fx;
            float wy = w3i_.cameraBounds[2] + (w3i_.cameraBounds[3] - w3i_.cameraBounds[2]) * fy;
            wx *= 128.0f; wy *= 128.0f;
            float gh = texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(wx, wy) : 0.0f;
            camera_.SetTarget(wx, gh + 100.0f, wy);
            camera_.ResetToHome();
        }
    }

    camera_.Update(dt);

    // Audio listener
    if (audio_.IsValid()) {
        Vec3 cp = camera_.GetPosition();
        audio_.SetListenerPosition(cp.x, cp.y, cp.z);
        float yaw = camera_.GetYaw(), pitch = camera_.GetPitch();
        float fx = std::sin(yaw) * std::cos(pitch);
        float fy = std::sin(pitch);
        float fz = std::cos(yaw) * std::cos(pitch);
        audio_.SetListenerOrientation(fx, fy, fz, 0.0f, 1.0f, 0.0f);
        audio_.Update();
    }

    // Day/night
    dayNight_.Update(dt);
    if (texturedTerrain_.IsValid()) dayNight_.Apply(texturedTerrain_);
    if (ui_.GetResourceBar()) ui_.GetResourceBar()->SetTimeOfDay(dayNight_.GetTimeOfDay());

    // Simulation step
    StepSimulation(dt);

    // After sim step, sync visuals
    SyncAndReconcileVisuals();
    RefreshRuntimeUnitSplats();
    RefreshSelectionUi(false);
}

void GameController::StepSimulation(float dt) {
    if (!simulationInitialized_) return;

    const float simStep = CSimulation::SIM_STEP_TIME;
    simulationAccumulator_ = std::min(simulationAccumulator_ + dt, simStep * 3.0f);
    bool ranSimulationStep = false;
    while (simulationAccumulator_ >= simStep) {
        if (jassNatives_) {
            jassNatives_->UpdateTimers(simStep);
            jassNatives_->PumpQueuedThreads(1024);
        }
        simulation_.Update();
        simulationAccumulator_ -= simStep;
        ranSimulationStep = true;
    }

    if (ranSimulationStep) {
        SyncAndReconcileVisuals();
        RefreshSelectionUi(false);
    }
}

void GameController::SyncAndReconcileVisuals() {
    // Sync all sim units to visuals
    for (auto* unit : simulation_.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        int handleId = unit->GetHandleId();

        auto visualIt = simUnitToVisualIdx.find(handleId);
        if (visualIt != simUnitToVisualIdx.end()) {
            if (auto* viewer = scene_.GetViewer(visualIt->second)) {
                SyncSimulationUnitAnimation(unit, viewer, this);
            }
        }
    }

    for (auto* item : simulation_.GetItems()) {
        if (!item || item->IsDead()) continue;
        const int handleId = item->GetHandleId();
        auto visualIt = simItemToVisualIdx.find(handleId);
        if (visualIt == simItemToVisualIdx.end()) {
            EnsureSimulationItemVisual(item);
            visualIt = simItemToVisualIdx.find(handleId);
        }
        if (visualIt != simItemToVisualIdx.end()) {
            float height = texturedTerrain_.IsValid()
                ? texturedTerrain_.GetGroundHeight(item->GetX(), item->GetY())
                : 0.0f;
            float pos[3] = {item->GetX(), height, item->GetY()};
            scene_.UpdateInstancePosition(visualIt->second, pos, 0.0f);
            scene_.SetInstanceVisible(visualIt->second, !item->IsHidden());
        }
    }

    // Reconcile (remove dead, add new)
    std::vector<int> removedHandles;
    for (auto& [handleId, visualIdx] : simUnitToVisualIdx) {
        CUnit* unit = simulation_.GetUnit(handleId);
        if (!unit || unit->IsDead()) {
            removedHandles.push_back(handleId);
        }
    }
    for (int handleId : removedHandles) {
        simUnitToVisualIdx.erase(handleId);
        selectedUnitHandlesSet_.erase(handleId);
        selectedUnitHandles_.erase(
            std::remove(selectedUnitHandles_.begin(), selectedUnitHandles_.end(), handleId),
            selectedUnitHandles_.end());
        if (selectedUnitHandle_ == handleId) selectedUnitHandle_ = -1;
    }

    std::vector<int> removedItemHandles;
    for (auto& [handleId, visualIdx] : simItemToVisualIdx) {
        CItem* item = simulation_.GetItem(handleId);
        if (!item || item->IsDead()) {
            scene_.SetInstanceVisible(visualIdx, false);
            removedItemHandles.push_back(handleId);
        }
    }
    for (int handleId : removedItemHandles) {
        simItemToVisualIdx.erase(handleId);
        simItemVisualRegistrationFailed.erase(handleId);
    }
}

void GameController::CollectUnitOverlays(int screenW, int screenH) {
    static const uint32_t kPlayerColors[12] = {
        0xFFFF0000, 0xFF0042FF, 0xFF1CE6B6, 0xFF540081,
        0xFFFFFC00, 0xFFFE8A0E, 0xFF20C000, 0xFFE55BB0,
        0xFF959697, 0xFF7EBFF1, 0xFF106246, 0xFF4F2B05
    };

    float aspect = (screenH > 0) ? (float)screenW / screenH : 1.0f;
    Mat4 view = camera_.GetViewMatrix();
    Mat4 proj = camera_.GetProjectionMatrix(aspect);

    std::vector<UnitOverlayInfo> overlays;
    if (simulationInitialized_) {
        for (auto* unit : simulation_.GetUnits()) {
            if (unit->IsDead()) continue;

            float gndH = texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(unit->GetX(), unit->GetY()) : 0.0f;

            UnitOverlayInfo info;
            info.handleId = unit->GetHandleId();
            info.isSelected = IsUnitSelected(unit->GetHandleId());
            info.isAlive = !unit->IsDead();
            info.hpRatio = unit->GetMaxLife() > 0 ? unit->GetLife() / unit->GetMaxLife() : 1.0f;
            info.mpRatio = unit->GetMaxMana() > 0 ? unit->GetMana() / (float)unit->GetMaxMana() : 0.0f;

            Vec3 headPos(unit->GetX(), gndH + 140.0f, unit->GetY());
            picking::WorldToScreen(headPos, view, proj, screenW, screenH, info.screenX, info.screenY);

            Vec3 footPos(unit->GetX(), gndH + 10.0f, unit->GetY());
            picking::WorldToScreen(footPos, view, proj, screenW, screenH, info.footX, info.footY);

            auto* unitType = unitData_.GetUnitType(unit->GetTypeId());
            info.name = unitType ? unitType->GetName() : "?";

            int pi = unit->GetPlayerIndex();
            if (pi >= 0 && pi < 12) info.playerColor = kPlayerColors[pi];
            else info.playerColor = 0xFF888888;

            overlays.push_back(info);
        }
    }
    ui_.SetOverlayData(overlays);
}

// --- Render ---

void GameController::Render(int screenW, int screenH) {
    if (!initialized_) return;
    if (state_ != GameState::IN_GAME) return;

    float dt = 0.016f;
    float aspect = (screenH > 0) ? (float)screenW / screenH : 1.0f;
    Mat4 view = camera_.GetViewMatrix();
    Mat4 proj = camera_.GetProjectionMatrix(aspect);

    Renderer::BeginFrame();
    Renderer::SetWireframe(wireframe);

    if (texturedTerrain_.IsValid())
        texturedTerrain_.Render(view, proj, frameCount_ * 0.016f);
    else if (simpleTerrain_.IsValid())
        simpleTerrain_.Render(view, proj);

    sg_reset_state_cache();
    sg_apply_viewport(0, 0, screenW, screenH, true);
    sg_apply_scissor_rect(0, 0, screenW, screenH, true);
    scene_.RenderAll(view, proj, dt);

    ui_.Update(screenW, screenH);
    ScreenRect minimapRect = ui_.GetMinimapRect();
    if (minimapRect.IsValid()) {
        minimap_.SetScreenRect(minimapRect.x, minimapRect.y, minimapRect.w, minimapRect.h);
    }

    CollectUnitOverlays(screenW, screenH);
    ui_.Render();

    if (showMinimap && minimap_.IsValid()) {
        Vec3 cp = camera_.GetPosition();
        minimap_.Render(screenW, screenH, cp.x, cp.z, camera_.GetYaw(), 0, 0, 45.0f, aspect);
    }

    Renderer::EndFrame();
    Platform::ProcessInput();

    if (screenshotFrame >= 0 && frameCount_ >= screenshotFrame) {
        std::cout << "Screenshot triggered but not implemented with sokol_gfx" << std::endl;
        screenshotFrame = -1;
    }
}

// --- OnEvent ---

void GameController::OnEvent(const sapp_event* ev) {
    Platform::HandleEvent(ev);

    switch (ev->type) {
    case SAPP_EVENTTYPE_MOUSE_DOWN: OnMouseDown(ev); break;
    case SAPP_EVENTTYPE_MOUSE_UP:   OnMouseUp(ev);   break;
    case SAPP_EVENTTYPE_MOUSE_MOVE: OnMouseMove(ev); break;
    case SAPP_EVENTTYPE_MOUSE_SCROLL: OnMouseScroll(ev); break;
    case SAPP_EVENTTYPE_CHAR:
    case SAPP_EVENTTYPE_RESIZED:    ui_.HandleEvent(ev); break;
    case SAPP_EVENTTYPE_KEY_DOWN:   OnKeyDown(ev); break;
    default: break;
    }
}

void GameController::OnMouseDown(const sapp_event* ev) {
    if (ui_.HandleEvent(ev)) return;

    int btn = (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) ? 0 :
              (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ? 1 : 2;
    if (ui_.GetGameUI().rootFrame && ui_.OnMouseDown(ev->mouse_x, ev->mouse_y, btn))
        return;

    if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        mouseDown[0] = true;
        selectionPressActive_ = false;
        selectionDragVisible_ = false;
        selectionStartX_ = ev->mouse_x;
        selectionStartY_ = ev->mouse_y;
        selectionCurrentX_ = ev->mouse_x;
        selectionCurrentY_ = ev->mouse_y;
        ui_.ClearSelectionBox();
        if (minimap_.IsValid() && minimap_.HitTest(ev->mouse_x, ev->mouse_y, sapp_width(), sapp_height())) {
            float wx, wy;
            minimap_.ScreenToWorld(ev->mouse_x, ev->mouse_y, sapp_width(), sapp_height(), &wx, &wy);
            float gh = texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(wx, wy) : 0.0f;
            camera_.SetTarget(wx, gh + 100.0f, wy);
        } else if (simulationInitialized_ && !simulation_.GetUnits().empty()) {
            selectionPressActive_ = true;
        }
    } else if (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) {
        bool issuedOrder = false;
        if (simulationInitialized_ && !selectedUnitHandles_.empty()) {
            std::vector<CUnit*> selUnits = GetSelectedSimulationUnits();
            if (!selUnits.empty()) {
                int winW = sapp_width(), winH = sapp_height();
                float aspect = (winH > 0) ? (float)winW / winH : 1.0f;
                Mat4 view = camera_.GetViewMatrix();
                Mat4 proj = camera_.GetProjectionMatrix(aspect);
                CUnit* targetUnit = PickSimulationUnitAtScreenPoint(ev->mouse_x, ev->mouse_y,
                    view, proj, winW, winH, selectedUnitHandle_, false, true);
                CDestructable* targetDest = targetUnit ? nullptr
                    : PickSimulationDestructableAtScreenPoint(ev->mouse_x, ev->mouse_y, view, proj, winW, winH);
                CWidget* targetWidget = targetUnit ? static_cast<CWidget*>(targetUnit)
                    : static_cast<CWidget*>(targetDest);

                int pendingCmd = ui_.GetPendingCommand();
                if (pendingCmd >= 0) {
                    const auto pendingMode = ui_.GetPendingCommandMode();
                    ui_.ClearPendingCommand();
                    auto terrainH = [&](float x, float z) {
                        return texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(x, z) : 0.0f;
                    };
                    Vec3 wPos = picking::ScreenToWorld(ev->mouse_x, ev->mouse_y, view, proj, winW, winH,
                                                       texturedTerrain_.IsValid(), texturedTerrain_.GetRadius(), terrainH);
                    for (auto* su : selUnits) {
                        if (!su) continue;
                        if (pendingMode == MainUI::PendingCommandMode::POINT_ONLY) {
                            su->IssueOrder(std::make_unique<COrderTargetPoint>(pendingCmd, wPos.x, wPos.z));
                        } else if (targetWidget) {
                            su->IssueOrder(std::make_unique<COrderTargetWidget>(pendingCmd, targetWidget));
                        } else {
                            su->IssueOrder(std::make_unique<COrderTargetPoint>(pendingCmd, wPos.x, wPos.z));
                        }
                    }
                    issuedOrder = true;
                } else if (targetWidget) {
                    for (auto* su : selUnits) {
                        if (!su) continue;
                        int orderId = OrderIds::SMART;
                        if (targetDest) {
                            if (auto* harvestAbility = dynamic_cast<CAbilityHarvest*>(su->GetAbility(War3ID("Ahar")))) {
                                if (harvestAbility->CheckCanTarget(simulation_, su, OrderIds::HARVEST, targetWidget)) {
                                    orderId = OrderIds::HARVEST;
                                }
                            }
                        }
                        su->IssueOrder(std::make_unique<COrderTargetWidget>(orderId, targetWidget));
                    }
                    issuedOrder = true;
                } else {
                    auto terrainH = [&](float x, float z) {
                        return texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(x, z) : 0.0f;
                    };
                    Vec3 wPos = picking::ScreenToWorld(ev->mouse_x, ev->mouse_y, view, proj, winW, winH,
                                                       texturedTerrain_.IsValid(), texturedTerrain_.GetRadius(), terrainH);
                    for (auto* su : selUnits) {
                        if (!su) continue;
                        su->IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::SMART, wPos.x, wPos.z));
                    }
                    issuedOrder = true;
                }
            }
        }
        if (!issuedOrder) {
            mouseDown[2] = true;
            sapp_lock_mouse(true);
        }
    } else {
        sapp_lock_mouse(true);
    }
}

void GameController::OnMouseUp(const sapp_event* ev) {
    if (!selectionPressActive_ && ui_.HandleEvent(ev)) return;

    int btn = (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) ? 0 :
              (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ? 1 : 2;
    if (!selectionPressActive_ && ui_.OnMouseUp(ev->mouse_x, ev->mouse_y, btn)) return;

    if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        mouseDown[0] = false;
        if (selectionPressActive_) {
            bool additive = Platform::IsKeyDown(SAPP_KEYCODE_LEFT_SHIFT) || Platform::IsKeyDown(SAPP_KEYCODE_RIGHT_SHIFT);
            ScreenRect rect;
            rect.x = std::min(selectionStartX_, selectionCurrentX_);
            rect.y = std::min(selectionStartY_, selectionCurrentY_);
            rect.w = std::fabs(selectionCurrentX_ - selectionStartX_);
            rect.h = std::fabs(selectionCurrentY_ - selectionStartY_);
            if (selectionDragVisible_ && rect.w >= 4.0f && rect.h >= 4.0f) {
                SelectUnitsInRect(rect, additive);
            } else if (simulationInitialized_ && !simulation_.GetUnits().empty()) {
                int winW = sapp_width(), winH = sapp_height();
                float aspect = (winH > 0) ? (float)winW / winH : 1.0f;
                Mat4 view = camera_.GetViewMatrix();
                Mat4 proj = camera_.GetProjectionMatrix(aspect);
                CUnit* bestUnit = PickSimulationUnitAtScreenPoint(ev->mouse_x, ev->mouse_y,
                    view, proj, winW, winH, -1, false, true);
                if (bestUnit) {
                    if (audio_.IsValid() && frameCount_ > 10) audio_.Play2D("ui_click", 0.5f);
                    SelectSingleUnit(bestUnit, additive);
                    std::cout << "SelectedUnit: "
                              << unit_text_utils::DescribeUnitType(bestUnit->GetTypeId(), unitData_)
                              << " handle=" << bestUnit->GetHandleId()
                              << " count=" << selectedUnitHandles_.size() << std::endl;
                } else if (!additive) {
                    SetSelectedUnits({}, -1);
                }
            }
        }
        selectionPressActive_ = false;
        selectionDragVisible_ = false;
        ui_.ClearSelectionBox();
    } else if (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) {
        mouseDown[2] = false;
    }
    sapp_lock_mouse(false);
}

void GameController::OnMouseMove(const sapp_event* ev) {
    if (!sapp_mouse_locked() && ui_.HandleEvent(ev)) return;
    if (mouseDown[0] && selectionPressActive_ && !sapp_mouse_locked()) {
        selectionCurrentX_ = ev->mouse_x;
        selectionCurrentY_ = ev->mouse_y;
        ScreenRect rect;
        rect.x = std::min(selectionStartX_, selectionCurrentX_);
        rect.y = std::min(selectionStartY_, selectionCurrentY_);
        rect.w = std::fabs(selectionCurrentX_ - selectionStartX_);
        rect.h = std::fabs(selectionCurrentY_ - selectionStartY_);
        if (rect.w >= 4.0f || rect.h >= 4.0f) {
            selectionDragVisible_ = true;
            ui_.SetSelectionBox(rect);
        }
    }
    if (sapp_mouse_locked()) {
        mouseDX += ev->mouse_dx;
        mouseDY += ev->mouse_dy;
    }
}

void GameController::OnMouseScroll(const sapp_event* ev) {
    if (ui_.HandleEvent(ev)) return;
    mouseScroll += ev->scroll_y;
}

void GameController::OnKeyDown(const sapp_event* ev) {
    if (ui_.HandleEvent(ev)) return;
    switch (ev->key_code) {
    case SAPP_KEYCODE_P:
        showPathing = !showPathing;
        texturedTerrain_.SetShowPathing(showPathing);
        break;
    case SAPP_KEYCODE_R:
        showRegions = !showRegions;
        texturedTerrain_.SetShowRegions(showRegions);
        break;
    case SAPP_KEYCODE_F1:
        wireframe = !wireframe;
        break;
    case SAPP_KEYCODE_M:
        showMinimap = !showMinimap;
        break;
    case SAPP_KEYCODE_N: {
        static bool musicOn = false;
        if (!musicOn) {
            bool ok = audio_.PlayMusic("Sound\\Music\\mp3\\MainMenu.mp3", &mpq_, 0.5f, true);
            if (!ok) audio_.PlayMusic("Sound\\Music\\mp3\\Human1.mp3", &mpq_, 0.5f, true);
            if (ok || audio_.IsMusicPlaying()) { std::cout << "Music: on" << std::endl; musicOn = true; }
        } else {
            audio_.StopMusic(); std::cout << "Music: off" << std::endl; musicOn = false;
        }
        break;
    }
    case SAPP_KEYCODE_F2:
        dayNight_.SetEnabled(!dayNight_.IsEnabled());
        std::cout << "Day/night: " << (dayNight_.IsEnabled() ? "on" : "off") << std::endl;
        break;
    case SAPP_KEYCODE_F3: {
        static float speed = 1.0f;
        speed = (speed >= 8.0f) ? 1.0f : speed * 2.0f;
        dayNight_.SetDayLength(480.0f / speed);
        std::cout << "Day/night speed: " << speed << "x" << std::endl;
        break;
    }
    case SAPP_KEYCODE_D: {
        int mode = (texturedTerrain_.DebugMode() + 1) % 5;
        texturedTerrain_.SetDebugMode(mode);
        const char* labels[] = {"normal", "hasTex(R/G)", "UV", "rawTex", "base-texture-only"};
        std::cout << "Debug mode: " << labels[mode] << std::endl;
        break;
    }
    default: break;
    }
}

// --- Selection / picking ---

bool GameController::IsUnitSelected(int handleId) const {
    return selectedUnitHandlesSet_.find(handleId) != selectedUnitHandlesSet_.end();
}

std::vector<CUnit*> GameController::GetSelectedSimulationUnits() const {
    std::vector<CUnit*> units;
    units.reserve(selectedUnitHandles_.size());
    for (int handleId : selectedUnitHandles_) {
        CUnit* unit = simulation_.GetUnit(handleId);
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
        units.push_back(unit);
    }
    return units;
}

// --- Init helpers (melee start, etc.) ---

static bool IsHumanTownHallType(War3ID typeId) {
    return typeId == War3ID("htow") || typeId == War3ID("hkee") || typeId == War3ID("hcas");
}

static int CountPlayerUnitsOfType(CSimulation& sim, int playerIndex, War3ID typeId) {
    int count = 0;
    for (auto* unit : sim.GetUnits())
        if (unit && unit->GetPlayerIndex() == playerIndex && unit->GetTypeId() == typeId) ++count;
    return count;
}

static int CountPlayerTownHalls(CSimulation& sim, int playerIndex) {
    int count = 0;
    for (auto* unit : sim.GetUnits())
        if (unit && unit->GetPlayerIndex() == playerIndex && IsHumanTownHallType(unit->GetTypeId())) ++count;
    return count;
}

static bool FindPlayerTownHallPosition(CSimulation& sim, int playerIndex, float& outX, float& outY) {
    for (auto* unit : sim.GetUnits())
        if (unit && unit->GetPlayerIndex() == playerIndex && IsHumanTownHallType(unit->GetTypeId())) {
            outX = unit->GetX(); outY = unit->GetY(); return true;
        }
    return false;
}

static bool FindPlayerStartLocationPosition(const JassNativesData* data, int playerIndex, float& outX, float& outY) {
    if (!data) return false;
    if (playerIndex < 0 || playerIndex >= (int)data->playerMapConfig.size()) return false;
    const auto& pc = data->playerMapConfig[playerIndex];
    const int si = pc.startLocationIndex;
    if (si < 0 || si >= (int)data->startLocations.size()) return false;
    const auto& start = data->startLocations[si];
    if (!start.defined) return false;
    outX = start.x; outY = start.y; return true;
}

static bool FindInitialSpawnFocusPosition(CSimulation& sim, GameController* g, float& outX, float& outY) {
    if (FindPlayerTownHallPosition(sim, 0, outX, outY)) return true;
    auto* data = g->GetJassNatives() ? g->GetJassNatives()->GetData().get() : nullptr;
    if (FindPlayerStartLocationPosition(data, 0, outX, outY)) return true;
    if (!data) return false;
    int playerLimit = data->configuredPlayerCount > 0
        ? std::min(data->configuredPlayerCount, (int)data->playerMapConfig.size())
        : (int)data->playerMapConfig.size();
    for (int pi = 0; pi < playerLimit; ++pi) {
        if (FindPlayerTownHallPosition(sim, pi, outX, outY)) return true;
        if (FindPlayerStartLocationPosition(data, pi, outX, outY)) return true;
    }
    return false;
}

void GameController::FocusCameraAndMinimapOnInitialSpawn() {
    float focusX = 0, focusY = 0;
    if (!FindInitialSpawnFocusPosition(simulation_, this, focusX, focusY)) return;
    float gh = texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(focusX, focusY) : 0.0f;
    camera_.SetTarget(focusX, gh + 100.0f, focusY);
    camera_.SetDistance(2200.0f);
    camera_.SaveHomeState();
    minimap_.ClearMarkers();
    minimap_.AddMarker(focusX, focusY, 0xFFD54AFFu, 10.0f);
    std::cout << "InitialFocus: spawn at (" << focusX << ", " << focusY << ")" << std::endl;
}

void GameController::EnsureHumanMeleeStartForPlayer(int playerIndex, float startX, float startY) {
    if (!unitData_.HasUnitType(War3ID("htow")) || !unitData_.HasUnitType(War3ID("hpea"))) return;

    constexpr float townHallFacing = 270.0f;
    constexpr float peasantFacing = 90.0f;
    static const float peasantOffsets[][2] = {
        {-220.0f,  120.0f}, {-220.0f,  -20.0f}, {-220.0f, -160.0f},
        {-80.0f,   180.0f}, {-80.0f,  -220.0f},
    };

    if (CountPlayerTownHalls(simulation_, playerIndex) == 0) {
        simulation_.CreateUnit(War3ID("htow"), playerIndex, startX, startY, townHallFacing);
        std::cout << "MeleeFallback: created Town Hall for player " << playerIndex << std::endl;
    }
    float thX = startX, thY = startY;
    FindPlayerTownHallPosition(simulation_, playerIndex, thX, thY);
    int existingPeasants = CountPlayerUnitsOfType(simulation_, playerIndex, War3ID("hpea"));
    for (int i = existingPeasants; i < 5; ++i) {
        simulation_.CreateUnit(War3ID("hpea"), playerIndex, thX + peasantOffsets[i][0], thY + peasantOffsets[i][1], peasantFacing);
    }
    if (existingPeasants < 5)
        std::cout << "MeleeFallback: created " << (5 - existingPeasants) << " Peasants for player " << playerIndex << std::endl;
}

void GameController::EnsureHumanMeleeStartsFromMapConfig() {
    if (!jassNatives_ || !simulationInitialized_) return;
    auto data = jassNatives_->GetData();
    if (!data) return;

    bool createdAny = false;
    int playerLimit = data->configuredPlayerCount > 0
        ? std::min(data->configuredPlayerCount, (int)data->playerMapConfig.size())
        : (int)data->playerMapConfig.size();

    for (int pi = 0; pi < playerLimit; ++pi) {
        const auto& pc = data->playerMapConfig[pi];
        if (pc.startLocationIndex < 0) continue;
        if (!pc.slotActive && CountPlayerTownHalls(simulation_, pi) > 0) continue;
        const int si = pc.startLocationIndex;
        if (si < 0 || si >= (int)data->startLocations.size()) continue;
        const auto& start = data->startLocations[si];
        if (!start.defined) continue;
        if (auto* player = simulation_.GetPlayer(pi)) player->SetStartLocation(start.x, start.y);

        int beforeTH = CountPlayerTownHalls(simulation_, pi);
        int beforePeasants = CountPlayerUnitsOfType(simulation_, pi, War3ID("hpea"));
        EnsureHumanMeleeStartForPlayer(pi, start.x, start.y);
        if (CountPlayerTownHalls(simulation_, pi) > beforeTH ||
            CountPlayerUnitsOfType(simulation_, pi, War3ID("hpea")) > beforePeasants)
            createdAny = true;
    }
    if (createdAny) simulation_.Update();
}

void GameController::FlushStartupSimulationState() {
    if (!simulationInitialized_) return;
    SyncAndReconcileVisuals();
    if (!jassNatives_) return;
    constexpr int maxWarmup = 4;
    for (int i = 0; i < maxWarmup; ++i) {
        size_t before = simulation_.GetUnits().size();
        int timers = jassNatives_->UpdateTimers(CSimulation::SIM_STEP_TIME);
        int pumped = jassNatives_->PumpQueuedThreads(1024);
        simulation_.Update();
        SyncAndReconcileVisuals();
        if (timers == 0 && pumped == 0 && simulation_.GetUnits().size() == before) break;
    }
}

void GameController::RefreshRuntimeUnitSplats() {
    splatMeta_.RefreshRuntimeSplats(texturedTerrain_, simulationInitialized_, simulation_, selectedUnitHandlesSet_);
}

// --- Unit visual ---

bool GameController::EnsureSimulationUnitVisual(CUnit* unit) {
    if (!unit) return false;
    int handleId = unit->GetHandleId();
    if (simUnitToVisualIdx.find(handleId) != simUnitToVisualIdx.end()) return true;
    if (simUnitVisualRegistrationFailed.find(handleId) != simUnitVisualRegistrationFailed.end()) return false;

    auto seedIt = simUnitVisualSeeds.find(handleId);
    float scale[3] = {1, 1, 1};
    uint32_t skinId = 0;
    int hp = 0, flags = 0;
    if (seedIt != simUnitVisualSeeds.end()) {
        scale[0] = seedIt->second.scale[0]; scale[1] = seedIt->second.scale[1]; scale[2] = seedIt->second.scale[2];
        skinId = seedIt->second.skinId; hp = seedIt->second.hitpoints; flags = seedIt->second.flags;
    }

    float pos[3] = {unit->GetX(), unit->GetY(), 0.0f};
    int visualIdx = scene_.AddUnit(unit->GetTypeId().value, pos, unit->GetFacing(),
                                   scale, unit->GetPlayerIndex(), hp, flags, skinId);
    if (visualIdx < 0) {
        simUnitVisualRegistrationFailed.insert(handleId);
        return false;
    }
    simUnitToVisualIdx[handleId] = visualIdx;

    // Apply initial animation
    if (auto* viewer = scene_.GetViewer(visualIdx)) {
        SyncSimulationUnitAnimation(unit, viewer, this);
    }
    return true;
}

bool GameController::EnsureSimulationItemVisual(CItem* item) {
    if (!item) return false;
    const int handleId = item->GetHandleId();
    if (simItemToVisualIdx.find(handleId) != simItemToVisualIdx.end()) return true;
    if (simItemVisualRegistrationFailed.find(handleId) != simItemVisualRegistrationFailed.end()) return false;
    if (item->IsDead()) return false;

    float height = texturedTerrain_.IsValid()
        ? texturedTerrain_.GetGroundHeight(item->GetX(), item->GetY())
        : 0.0f;
    float pos[3] = {item->GetX(), height, item->GetY()};
    float scale[3] = {1.0f, 1.0f, 1.0f};
    const int visualIdx = scene_.AddItem(item->GetTypeId().value, pos, 0.0f, scale, 0, 0);
    if (visualIdx < 0) {
        simItemVisualRegistrationFailed.insert(handleId);
        return false;
    }

    simItemToVisualIdx[handleId] = visualIdx;
    scene_.SetInstanceVisible(visualIdx, !item->IsHidden());
    return true;
}

// --- Selection helpers ---

static bool RectanglesOverlap(const ScreenRect& a, const ScreenRect& b) {
    if (!a.IsValid() || !b.IsValid()) return false;
    return !(a.x + a.w < b.x || b.x + b.w < a.x || a.y + a.h < b.y || b.y + b.h < a.y);
}

static bool ComputeUnitSelectionScreenBounds(const CUnit& unit, const Mat4& view, const Mat4& proj,
                                                int screenW, int screenH, ScreenRect& outRect,
                                                GameController* g) {
    float footX, footY, headX, headY, radiusPixels;
    auto terrainH = [&](float x, float z) {
        return g->GetTexturedTerrain().IsValid() ? g->GetTexturedTerrain().GetGroundHeight(x, z) : 0.0f;
    };
    if (!unit_selection_geom::ComputeUnitSelectionScreenShape(unit, view, proj, screenW, screenH,
                                                               g->GetTexturedTerrain().IsValid(), terrainH,
                                                               footX, footY, headX, headY, radiusPixels))
        return false;
    float minX = std::min(footX, headX) - radiusPixels;
    float maxX = std::max(footX, headX) + radiusPixels;
    float minY = std::min(footY, headY) - radiusPixels;
    float maxY = std::max(footY, headY) + radiusPixels;
    outRect = {minX, minY, maxX - minX, maxY - minY};
    return outRect.IsValid();
}

static std::vector<CUnit*> CollectUnitsInScreenRect(const ScreenRect& rect, const Mat4& view, const Mat4& proj,
                                                    int screenW, int screenH, CSimulation& sim, GameController* g) {
    std::vector<CUnit*> result, buildingFallback;
    if (!rect.IsValid()) return result;
    for (auto* unit : sim.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
        ScreenRect unitRect;
        if (!ComputeUnitSelectionScreenBounds(*unit, view, proj, screenW, screenH, unitRect, g)) continue;
        if (RectanglesOverlap(rect, unitRect)) {
            if (unit->IsBuilding()) buildingFallback.push_back(unit);
            else result.push_back(unit);
        }
    }
    return result.empty() ? buildingFallback : result;
}

void GameController::SetSelectedUnits(const std::vector<CUnit*>& units, int primaryHandle, bool rebuildCommandCard) {
    selectedUnitHandles_.clear();
    selectedUnitHandlesSet_.clear();
    for (auto* unit : units) {
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
        int hid = unit->GetHandleId();
        if (selectedUnitHandlesSet_.insert(hid).second) selectedUnitHandles_.push_back(hid);
    }
    if (selectedUnitHandles_.empty()) selectedUnitHandle_ = -1;
    else if (primaryHandle >= 0 && selectedUnitHandlesSet_.find(primaryHandle) != selectedUnitHandlesSet_.end())
        selectedUnitHandle_ = primaryHandle;
    else selectedUnitHandle_ = selectedUnitHandles_.front();

    RefreshSelectionUi(rebuildCommandCard);
}

void GameController::SelectSingleUnit(CUnit* unit, bool additive) {
    if (!unit || unit->IsDead() || unit->IsHidden()) {
        if (!additive) SetSelectedUnits({}, -1);
        return;
    }
    if (!additive) { SetSelectedUnits({unit}, unit->GetHandleId()); return; }
    std::vector<CUnit*> units = GetSelectedSimulationUnits();
    bool already = false;
    for (auto* u : units) if (u && u->GetHandleId() == unit->GetHandleId()) { already = true; break; }
    if (!already) units.push_back(unit);
    SetSelectedUnits(units, unit->GetHandleId());
}

void GameController::SelectUnitsInRect(const ScreenRect& rect, bool additive) {
    int ww = sapp_width(), wh = sapp_height();
    float aspect = wh > 0 ? (float)ww / wh : 1.0f;
    Mat4 view = camera_.GetViewMatrix();
    Mat4 proj = camera_.GetProjectionMatrix(aspect);
    std::vector<CUnit*> units = CollectUnitsInScreenRect(rect, view, proj, ww, wh, simulation_, this);
    if (additive) {
        auto existing = GetSelectedSimulationUnits();
        existing.insert(existing.end(), units.begin(), units.end());
        SetSelectedUnits(existing);
    } else {
        SetSelectedUnits(units);
    }
}

static std::vector<MultiSelectUnitInfo> BuildMultiSelectionInfo(const std::vector<CUnit*>& units) {
    static const uint32_t pc[12] = {0xFFFF0000,0xFF0042FF,0xFF1CE6B6,0xFF540081,0xFFFFFC00,0xFFFE8A0E,
                                    0xFF20C000,0xFFE55BB0,0xFF959697,0xFF7EBFF1,0xFF106246,0xFF4F2B05};
    std::vector<MultiSelectUnitInfo> info; info.reserve(units.size());
    for (auto* u : units) {
        if (!u) continue;
        MultiSelectUnitInfo item; item.handleId = u->GetHandleId();
        if (auto* ut = u->GetUnitType()) { item.name = ut->GetName(); item.level = ut->GetLevel(); item.isHero = ut->IsHero(); }
        item.hpRatio = u->GetMaxLife() > 0 ? u->GetLife() / u->GetMaxLife() : 1.0f;
        item.mpRatio = u->GetMaxMana() > 0 ? u->GetMana() / (float)u->GetMaxMana() : 0.0f;
        int pi = u->GetPlayerIndex();
        if (pi >= 0 && pi < 12) item.playerColor = pc[pi];
        info.push_back(std::move(item));
    }
    return info;
}

void GameController::RefreshSelectionUi(bool rebuildCommandCard) {
    auto sel = GetSelectedSimulationUnits();
    selectedUnitHandles_.clear(); selectedUnitHandlesSet_.clear();
    for (auto* u : sel) { selectedUnitHandles_.push_back(u->GetHandleId()); selectedUnitHandlesSet_.insert(u->GetHandleId()); }

    if (sel.empty()) {
        selectedUnitHandle_ = -1; selectedUnitTypeId_ = War3ID();
        ui_.SetSelectedUnit(-1); ui_.SetMultiSelection({}); ui_.ClearInfoPanel();
        ui_.HideHeroPanel(); ui_.ClearPendingCommand(); return;
    }

    CUnit* primary = nullptr;
    for (auto* u : sel) if (u->GetHandleId() == selectedUnitHandle_) { primary = u; break; }
    if (!primary) primary = sel.front();
    selectedUnitHandle_ = primary->GetHandleId();
    selectedUnitTypeId_ = primary->GetTypeId();
    ui_.SetSelectedUnit(selectedUnitHandle_);
    (void)rebuildCommandCard;
    ui_.SetUnitData(&unitData_);
    ui_.PopulateCommandCardForUnit(primary);
    ui_.SetMultiSelection(sel.size() > 1 ? BuildMultiSelectionInfo(sel) : std::vector<MultiSelectUnitInfo>{});

    auto* ut = primary->GetUnitType();
    std::string uname = ut ? ut->GetName() : "Unknown";
    int ulvl = ut ? ut->GetLevel() : 1;
    bool isHero = ut && ut->IsHero();
    float hp = primary->GetLife(), maxHp = primary->GetMaxLife();
    float mp = primary->GetMana(), maxMp = (float)primary->GetMaxMana();
    if (isHero) {
        ui_.ShowHeroPanel(uname, std::max(ulvl, 1), 0, 100,
                          ut->GetStartingStrength(), 0, ut->GetStartingAgility(), 0, ut->GetStartingIntelligence(), 0);
    } else {
        ui_.HideHeroPanel();
        ui_.UpdateInfoPanel(selectedUnitTypeId_, uname, ulvl, hp, maxHp, mp, maxMp);
    }
}

// --- Picking ---

CUnit* GameController::PickSimulationUnitAtScreenPoint(float sx, float sy, const Mat4& view, const Mat4& proj,
                                                        int screenW, int screenH, int ignoreHandle,
                                                        bool preferBuildings, bool strictScreenTargeting) {
    picking::PickRay ray = picking::BuildScreenPickRay(sx, sy, view, proj, screenW, screenH);

    CUnit* bestScreen = nullptr, *bestScreenNonBuilding = nullptr, *bestRay = nullptr, *bestRayNonBuilding = nullptr;
    float bestScreenScore = std::numeric_limits<float>::infinity();
    float bestScreenNonBuildingScore = std::numeric_limits<float>::infinity();
    float bestScreenDistSq = std::numeric_limits<float>::infinity();
    float bestScreenNonBuildingDistSq = std::numeric_limits<float>::infinity();
    float bestRayT = std::numeric_limits<float>::infinity();
    float bestRayNonBuildingT = std::numeric_limits<float>::infinity();

    auto terrainH = [&](float x, float z) {
        return texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(x, z) : 0.0f;
    };

    for (auto* unit : simulation_.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
        if (ignoreHandle >= 0 && unit->GetHandleId() == ignoreHandle) continue;

        float sphereRadius = unit_selection_geom::ComputeUnitSelectionWorldRadius(*unit);
        Vec3 sphereCenter(unit->GetX(),
                          unit_selection_geom::ComputeUnitVisualHeight(*unit, texturedTerrain_.IsValid(), terrainH)
                              + unit_selection_geom::ComputeUnitSelectionVisualHeight(*unit) * 0.5f,
                          unit->GetY());

        if (auto vit = simUnitToVisualIdx.find(unit->GetHandleId()); vit != simUnitToVisualIdx.end()) {
            if (MdxViewer* v = scene_.GetViewer(vit->second)) {
                sphereCenter = Vec3(unit->GetX() + v->GetBoundsCenter().x,
                                    unit_selection_geom::ComputeUnitVisualHeight(*unit, texturedTerrain_.IsValid(), terrainH) + v->GetBoundsCenter().y,
                                    unit->GetY() + v->GetBoundsCenter().z);
                float vr = v->GetBoundsRadius();
                if (vr > 1.0f) sphereRadius = std::max(sphereRadius, vr);
            }
        }
        sphereRadius = std::max(sphereRadius, unit->IsBuilding() ? 72.0f : 28.0f);

        float footX, footY, headX, headY, rpx;
        bool hasScreenShape = unit_selection_geom::ComputeUnitSelectionScreenShape(
            *unit, view, proj, screenW, screenH, texturedTerrain_.IsValid(), terrainH,
            footX, footY, headX, headY, rpx);

        float hitT;
        if (picking::IntersectSphere(ray, sphereCenter, sphereRadius, hitT)) {
            if (hitT < bestRayT) { bestRayT = hitT; bestRay = unit; }
            if (!unit->IsBuilding() && hitT < bestRayNonBuildingT) { bestRayNonBuildingT = hitT; bestRayNonBuilding = unit; }
        }

        if (!hasScreenShape) continue;

        float distSq = picking::DistanceToSegmentSquared(sx, sy, footX, footY, headX, headY);
        float hitRPx = strictScreenTargeting ? rpx * (unit->IsBuilding() ? 0.6f : 0.75f) : rpx;
        if (distSq > hitRPx * hitRPx) continue;

        float score = distSq / std::max(1.0f, hitRPx * hitRPx);
        if (preferBuildings && unit->IsBuilding()) score = std::max(0.0f, score - 0.05f);

        if (score < bestScreenScore || (std::abs(score - bestScreenScore) <= 0.03f
            && (preferBuildings && unit->IsBuilding() && bestScreen && !bestScreen->IsBuilding()))) {
            bestScreenScore = score; bestScreenDistSq = distSq; bestScreen = unit;
        }
        if (!unit->IsBuilding() && score < bestScreenNonBuildingScore) {
            bestScreenNonBuildingScore = score; bestScreenNonBuildingDistSq = distSq; bestScreenNonBuilding = unit;
        }
    }

    if (!preferBuildings && bestScreenNonBuilding) return bestScreenNonBuilding;
    if (bestScreen) return bestScreen;
    if (strictScreenTargeting) return nullptr;
    if (!preferBuildings && bestRayNonBuilding) return bestRayNonBuilding;
    return bestRay;
}

CDestructable* GameController::PickSimulationDestructableAtScreenPoint(float sx, float sy, const Mat4& view,
                                                                        const Mat4& proj, int screenW, int screenH) {
    if (!simulationInitialized_) return nullptr;
    picking::PickRay ray = picking::BuildScreenPickRay(sx, sy, view, proj, screenW, screenH);
    CDestructable *bestScreen = nullptr, *bestRay = nullptr;
    float bestScreenScore = std::numeric_limits<float>::infinity(), bestRayT = std::numeric_limits<float>::infinity();

    for (auto* d : simulation_.GetDestructables()) {
        if (!d || d->IsDead()) continue;
        float worldRadius = 56.0f, worldHeight = 140.0f;
        if (auto* dt = destructableData_.GetDestructable(d->GetTypeId())) {
            if (dt->isTree) { worldRadius = 72.0f; worldHeight = 180.0f; }
            else if (dt->isWall) { worldRadius = 64.0f; worldHeight = 96.0f; }
        }
        Vec3 sc(d->GetX(), worldHeight * 0.5f, d->GetY());
        float hitT;
        if (picking::IntersectSphere(ray, sc, worldRadius, hitT) && hitT < bestRayT) { bestRayT = hitT; bestRay = d; }
        float cx, cy;
        if (!picking::WorldToScreen(sc, view, proj, screenW, screenH, cx, cy)) continue;
        float rpx = 0;
        auto upR = [&](const Vec3& p) { float sx, sy; if (picking::WorldToScreen(p, view, proj, screenW, screenH, sx, sy)) { float dx=sx-cx, dy=sy-cy; rpx=std::max(rpx,std::sqrt(dx*dx+dy*dy)); } };
        upR(Vec3(d->GetX()+worldRadius, worldHeight*0.5f, d->GetY()));
        upR(Vec3(d->GetX(), worldHeight*0.5f, d->GetY()+worldRadius));
        rpx = std::max(rpx, 20.0f);
        float dx = sx - cx, dy = sy - cy, distSq = dx*dx+dy*dy;
        if (distSq > rpx*rpx) continue;
        float score = distSq / std::max(1.0f, rpx*rpx);
        if (score < bestScreenScore) { bestScreenScore = score; bestScreen = d; }
    }
    return bestScreen ? bestScreen : bestRay;
}

// --- Shutdown ---

void GameController::Shutdown() {
    ui_.Shutdown();
    Renderer::Shutdown();
    audio_.Shutdown();
    std::cout << "GameController shut down after " << frameCount_ << " frames." << std::endl;
}
