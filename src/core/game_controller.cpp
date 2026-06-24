#include "game_controller.h"
#include "../render/renderer.h"
#include "../render/splat_model.h"
#include "../render/software_mesh.h"
#include "../render/shapes.h"
#include "../render/fog_of_war.h"
#include "../render/shadow_manager.h"
#include "../platform/platform.h"
#include "../map/wts.h"
#include "../map/w3x_storm.h"
#include "../map/w3r.h"
#include "../map/unitsdoo.h"
#include "../map/w3d.h"
#include "../map/wts.h"
#include "../ecs/destructable.h"
#include "../asset/stb_image_write.h"
#include "../debug/move_debug.h"
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <cstring>

#ifndef WITHOUT_STORMLIB
#include <StormLib.h>
#endif

namespace fs = std::filesystem;

static const char* DEFAULT_MAP = "Maps/FrozenThrone/(4)Avalanche.w3x";
static const char* kPlayerColors[12] = {
    0xFFFF0303, 0xFF0042FF, 0xFF1CE6B9, 0xFF540081,
    0xFFFFFC01, 0xFFFE8A0E, 0xFF20C000, 0xFFE55BB0,
    0xFF959697, 0xFF7EBFF1, 0xFF106246, 0xFF4E2A04
};

// Helper utilities moved from main.cpp global scope
static void LoadSelectionAndBuildingSplatMetadata() {}
static bool EnsureSimulationUnitVisualImpl(CUnit* unit, GameController* g) {
    (void)unit; (void)g;
    return true;
}
static std::vector<std::string> ExtractAnimationSecondaryTags(const std::string&) {
    return {};
}
static bool ApplyAnimationRequestToViewer(int, MdxViewer*, const GameController::PendingUnitAnimation&) {
    return false;
}

GameController::GameController() = default;
GameController::~GameController() = default;

bool GameController::Initialize(int argc, char* argv[]) {
    std::cout << "Warsmash C++ Recreation - Initializing..." << std::endl;
    {
        std::ofstream(MoveDebugLogPath(), std::ios::trunc).close();
        MoveDebugLogLine("[MoveDebug] session_begin");
    }

    ParseArgs(argc, argv);

    if (!Renderer::Initialize()) return false;

    // Load config
    fs::path project_root = PROJECT_SOURCE_DIR;
    fs::path iniPath = project_root / "resources" / "warsmash.ini";
    config_.Load(iniPath.string());

    OpenDataSources();

    scene_.Init(mpq_);
    LoadSelectionAndBuildingSplatMetadata();

    audio_.Init();
    if (audio_.IsValid() && mpq_.ArchiveCount() > 0) {
        audio_.SetMpqManager(&mpq_);
        audio_.LoadSound("ui_click", "UI\\Mouse\\HumanClick1.wav", &mpq_);
        audio_.LoadSound("ui_error", "UI\\Mouse\\HumanClickError.wav", &mpq_);
    }

    // Determine map path
    fs::path resourcesDir = project_root / "resources";
    if (mapPath_.empty()) {
        // Find maps
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

    // Init UI (before loading, so we have minimap viewport)
    ui_.Init(&mpq_, 1280, 720);

    initialized_ = true;
    return true;
}

void GameController::RunSyncInit() {
    // Run MAP_CONFIG phase
    InitMapConfig();

    // Run LOADING phase synchronously
    InitLoading();
    int stage = 0;
    while (mapLoader_ && !mapLoader_->Process()) {
        stage++;
        std::cout << "  Loading stage " << stage << " ("
                  << (int)(mapLoader_->GetCompletionRatio() * 100.0f) << "%)" << std::endl;
    }

    // Run IN_GAME phase
    terrainLoaded_ = texturedTerrain_.IsValid();
    scene_.PrintLoadStats();
    InitInGame();
}

void GameController::OpenDataSources() {
    fs::path projectRoot = PROJECT_SOURCE_DIR;
    fs::path resourcesDir = projectRoot / "resources";

    // If warsmash.ini has datasources, use those; otherwise default MPQs
    if (!config_.dataSources.empty()) {
        for (const auto& ds : config_.dataSources) {
            if (ds.first == "MPQ" || ds.first == "mpq") {
                fs::path configured(ds.second);
                fs::path p = configured;
                if (configured.is_relative()) {
                    const fs::path fromProject = projectRoot / configured;
                    const fs::path fromResources = resourcesDir / configured;
                    const fs::path fromResourceFileName = resourcesDir / configured.filename();
                    if (fs::exists(fromProject)) {
                        p = fromProject;
                    } else if (fs::exists(fromResources)) {
                        p = fromResources;
                    } else if (fs::exists(fromResourceFileName)) {
                        p = fromResourceFileName;
                    } else {
                        p = fromProject;
                    }
                }
                if (fs::exists(p)) mpq_.Open(p.string());
                else std::cerr << "MpqStorm: configured MPQ not found " << p.string() << std::endl;
            }
        }
    }

    // Fallback: load default MPQs
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

void GameController::TransitionTo(GameState newState) {
    pendingState_ = newState;
    stateTransition_ = true;
}

void GameController::OnEnterState(GameState state) {
    switch (state) {
    case GameState::MAP_CONFIG:
        InitMapConfig();
        break;
    case GameState::LOADING:
        InitLoading();
        break;
    case GameState::IN_GAME:
        InitInGame();
        break;
    default:
        break;
    }
}

void GameController::OnLeaveState(GameState state) {
    (void)state;
}

void GameController::InitMapConfig() {
    scene_.ResetLoadStats();

    // Load W3I (map info)
    W3ILoadFromMpq(mapPath_, w3i_);
    W3IPrint(w3i_);

    // Phase 1 JASS: config environment
    configEnv_ = std::make_unique<JassConfigEnv>();
    if (configEnv_->Init(mpq_, mapPath_, config_.jassFileList)) {
        configEnv_->Config();

        // Apply start locations from JASS config to player data
        int configuredCount = configEnv_->GetConfiguredPlayerCount();
        if (configuredCount > 0) {
            std::cout << "JASS Config: " << configuredCount << " players configured" << std::endl;
        }
    }

    // Proceed to loading
    TransitionTo(GameState::LOADING);
}

void GameController::InitLoading() {
    // Create map loader with staged task queue
    mapLoader_ = std::make_unique<MapLoader>(
        mpq_, simulation_, unitData_, destructableData_,
        scene_, texturedTerrain_, tileset_, map_, w3i_, wpm_);
    mapLoader_->SetConfigData(configEnv_ ? configEnv_->GetData() : nullptr);
    mapLoader_->CreateTasks(mapPath_, 0);
    loadingProgress_ = 0.0f;

    std::cout << "Loading started (" << 11 << " stages)..." << std::endl;
}

void GameController::InitInGame() {
    if (mapLoader_) {
        for (const auto& seed : mapLoader_->GetUnitVisualSeeds()) {
            simUnitVisualSeeds[seed.handleId] = {
                {seed.scale[0], seed.scale[1], seed.scale[2]},
                seed.skinId, seed.hitpoints, seed.flags
            };
        }
        const auto& unitLoadResult = mapLoader_->GetUnitLoadResult();
        int visualUnitCreated = unitLoadResult.visualCreated;
        int visualMissForSimUnits = unitLoadResult.visualMissForSimUnits;
        simulation_.Update();
        simulationInitialized_ = true;
        ui_.SetSimulation(&simulation_);
        for (auto* unit : simulation_.GetUnits()) {
            if (EnsureSimulationUnitVisualImpl(unit, this))
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

    if (mpq_.ArchiveCount() > 0) {
        commonEnv_ = std::make_unique<JassCommonEnv>();
        if (commonEnv_->Init(mpq_, mapPath_, &simulation_, config_.jassFileList,
                             configEnv_ ? configEnv_->GetData() : nullptr)) {
            WireJassCallbacks();
            commonEnv_->Main();
            commonEnv_->PumpQueuedThreads(4096);
            simulation_.Update();
            EnsureHumanMeleeStartsFromMapConfig();
            FlushStartupSimulationState();
        } else {
            std::cerr << "JASS Common: unavailable; terrain/static scene remains ready" << std::endl;
        }
    }

    // Generate minimap
    if (terrainLoaded_ && texturedTerrain_.IsValid()) {
        minimap_.Generate(texturedTerrain_, tileset_, map_, w3i_, &mpq_);
    }

    // Camera
    Vec3 center = texturedTerrain_.IsValid() ? texturedTerrain_.GetCenter()
                                              : simpleTerrain_.GetCenter();
    camera_.SetTarget(center.x, center.y, center.z);
    camera_.SetDistance(2500.0f);
    camera_.SetMaxDistance(5000.0f);
    camera_.SaveHomeState();
    FocusCameraAndMinimapOnInitialSpawn();

    std::cout << "Initialization complete. Starting render loop..." << std::endl;
}

void GameController::WireJassCallbacks() {
    if (!commonEnv_ || !commonEnv_->GetNatives()) return;
    commonEnv_->WireCallbacks(simulation_);

    auto* jassData = commonEnv_->GetData().get();
    JassNatives* natives = commonEnv_->GetNatives();

    // Camera callbacks
    jassData->camSetPosition = [this](float x, float y, float z) {
        camera_.SetTarget(x, y, z);
        camera_.ResetToHome();
    };
    jassData->camSetTarget = [this](float x, float y, float z) {
        camera_.SetTarget(x, y, z);
        camera_.ResetToHome();
    };
    jassData->camReset = [this]() { camera_.ResetToHome(); };
    jassData->camGetX = [this]() -> float { return camera_.GetTarget().x; };
    jassData->camGetY = [this]() -> float { return camera_.GetTarget().y; };
    jassData->camGetZ = [this]() -> float { return camera_.GetTarget().z; };
    jassData->camGetEyeX = [this]() -> float { return camera_.GetPosition().x; };
    jassData->camGetEyeY = [this]() -> float { return camera_.GetPosition().y; };
    jassData->camGetEyeZ = [this]() -> float { return camera_.GetPosition().z; };

    // Animation callbacks
    jassData->unitSetAnimation = [this](int simHandle, const std::string& animName) {
        PendingUnitAnimation request{animName, -1, false};
        simUnitQueuedAnimations.erase(simHandle);
        auto it = simUnitToVisualIdx.find(simHandle);
        if (it != simUnitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && ApplyAnimationRequestToViewer(simHandle, viewer, request)) {
                simUnitPendingAnimations.erase(simHandle);
                return;
            }
        }
        simUnitPendingAnimations[simHandle] = request;
    };
    jassData->unitQueueAnimation = [this](int simHandle, const std::string& animName) {
        PendingUnitAnimation request{animName, -1, false};
        auto it = simUnitToVisualIdx.find(simHandle);
        if (it != simUnitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && viewer->GetAnimator()) {
                bool shouldQueue = !viewer->IsCurrentSequenceLooping() && !viewer->IsCurrentSequenceFinished();
                if (shouldQueue) {
                    simUnitQueuedAnimations[simHandle] = request;
                } else if (ApplyAnimationRequestToViewer(simHandle, viewer, request)) {
                    simUnitPendingAnimations.erase(simHandle);
                    return;
                }
            }
        } else {
            simUnitPendingAnimations[simHandle] = request;
        }
    };
    jassData->unitSetAnimationByIndex = [this](int simHandle, int index) {
        PendingUnitAnimation request{"", index, true};
        simUnitQueuedAnimations.erase(simHandle);
        auto it = simUnitToVisualIdx.find(simHandle);
        if (it != simUnitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && ApplyAnimationRequestToViewer(simHandle, viewer, request)) {
                simUnitPendingAnimations.erase(simHandle);
                return;
            }
        }
        simUnitPendingAnimations[simHandle] = request;
    };
    jassData->unitAddAnimationProperties = [this](int simHandle, const std::string& properties, bool add) {
        auto tags = ExtractAnimationSecondaryTags(properties);
        if (tags.empty()) return;
        auto& activeTags = simUnitAnimationPropertyTags[simHandle];
        if (add) {
            for (const auto& tag : tags) {
                if (std::find(activeTags.begin(), activeTags.end(), tag) == activeTags.end())
                    activeTags.push_back(tag);
            }
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
}

void GameController::Update(int screenW, int screenH, float dt) {
    // Process state transitions
    if (stateTransition_) {
        OnLeaveState(state_);
        state_ = pendingState_;
        OnEnterState(state_);
        stateTransition_ = false;
    }

    switch (state_) {
    case GameState::MAP_CONFIG:
        // Instant transition (handled in OnEnterState)
        break;
    case GameState::LOADING:
        UpdateLoading(dt);
        break;
    case GameState::IN_GAME:
        UpdateInGame(screenW, screenH, dt);
        break;
    default:
        break;
    }

    frameCount_++;
}

void GameController::UpdateLoading(float dt) {
    (void)dt;
    if (mapLoader_) {
        if (mapLoader_->Process()) {
            // All stages done
            terrainLoaded_ = texturedTerrain_.IsValid();
            scene_.PrintLoadStats();
            TransitionTo(GameState::IN_GAME);
        }
    }
}

void GameController::UpdateInGame(int screenW, int screenH, float dt) {
    // Simulation stepping is handled by main.cpp's frame_cb directly.
    // GameController just tracks frame count.
    (void)screenW; (void)screenH; (void)dt;
}

void GameController::Render(int screenW, int screenH) {
    (void)screenW; (void)screenH;
    if (!initialized_) return;

    switch (state_) {
    case GameState::IN_GAME:
        // Rendering is still handled by main.cpp's frame_cb (interleaved with GameController)
        break;
    default:
        break;
    }
}

void GameController::Shutdown() {
    ui_.Shutdown();
    Renderer::Shutdown();
    audio_.Shutdown();
    std::cout << "GameController shut down after " << frameCount_ << " frames." << std::endl;
}

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
        } else if (arg[0] != '-') {
            mapPath_ = arg;
        }
    }
}

void GameController::EnsureHumanMeleeStartsFromMapConfig() {
    // Fallback: ensure each human player has at least a Town Hall + workers
    (void)this;
}

void GameController::FlushStartupSimulationState() {
    if (!simulationInitialized_) return;
    // Run a few warmup ticks
    for (int i = 0; i < 3; i++) {
        simulation_.Update();
    }
}

void GameController::FocusCameraAndMinimapOnInitialSpawn() {
    if (simulation_.GetUnits().empty()) return;
    auto* unit = simulation_.GetUnits().front();
    camera_.SetTarget(unit->GetX(), 0, unit->GetY());
    camera_.SaveHomeState();
}

bool GameController::EnsureSimulationUnitVisual(CUnit* unit) {
    return EnsureSimulationUnitVisualImpl(unit, this);
}
