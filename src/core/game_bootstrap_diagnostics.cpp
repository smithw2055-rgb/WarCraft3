#include "core/game_controller_internal.h"

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
            } else if (ds.first == "CASC" || ds.first == "casc") {
                auto p = path_utils::ResolveConfiguredPath(
                    projectRoot, resourcesDir, ds.second);
                if (fs::exists(p)) mpq_.OpenCasc(p.string());
                else std::cerr << "MpqStorm: configured CASC storage not found " << p.string() << std::endl;
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

void GameController::HandleGlueMapLaunchRequest(const std::string& mapPath, int localPlayerIndex) {
    fs::path projectRoot = PROJECT_SOURCE_DIR;
    fs::path resourcesDir = projectRoot / "resources";

    std::string resolvedPath;
    const bool resolved = ResolveMapPathForLoad(resourcesDir, mpq_, mapPath, resolvedPath);
    std::ostringstream launchLog;
    launchLog << "GlueMapLaunch: requested path='" << mapPath
              << "' localPlayer=" << localPlayerIndex
              << " resolved=" << (resolved ? "yes" : "no");
    if (resolved) {
        launchLog << " resolvedPath='" << resolvedPath << "'";
    }
    MoveDebugLogLine(launchLog.str());

    if (!resolved) {
        return;
    }

    config_.localPlayerIndex = localPlayerIndex;
    // Fresh menu launch: never inherit a stale ChangeLevel snapshot (e.g.
    // the user aborted to the menu mid-campaign).
    carriedHeroes_.clear();
    arrivedViaChangeLevel_ = false;
    if (uiOnlyMode_) {
        pendingGlueMapLaunch_ = true;
        pendingGlueMapLaunchRequestedPath_ = mapPath;
        pendingGlueMapLaunchResolvedPath_ = resolvedPath;
        pendingGlueMapLaunchLocalPlayer_ = localPlayerIndex;
        MoveDebugLogLine("GlueMapLaunch: queued pending UI-only launch");
        return;
    }

    if (mapConfigInitialized_ || loadingInitialized_ || simulationInitialized_) {
        MoveDebugLogLine("GlueMapLaunch: runtime map switch requested after initialization; deferred hot-switch support pending");
        return;
    }

    mapPath_ = resolvedPath;
    if (fs::exists(mapPath_)) {
        mpq_.OpenOnTop(mapPath_);
    }
    TransitionTo(GameState::MAP_CONFIG);
    MoveDebugLogLine("GlueMapLaunch: queued map load path='" + mapPath_ + "'");
}

bool GameController::StartPendingGlueMapLaunch(int screenW, int screenH) {
    if (!pendingGlueMapLaunch_) {
        return false;
    }

    const std::string requestedPath = pendingGlueMapLaunchRequestedPath_;
    const std::string resolvedPath = pendingGlueMapLaunchResolvedPath_;
    const int localPlayerIndex = pendingGlueMapLaunchLocalPlayer_;
    pendingGlueMapLaunch_ = false;
    pendingGlueMapLaunchRequestedPath_.clear();
    pendingGlueMapLaunchResolvedPath_.clear();

    if (resolvedPath.empty() || !fs::exists(resolvedPath)) {
        MoveDebugLogLine("GlueMapLaunch: pending launch aborted; resolved map missing path='" + resolvedPath + "'");
        return false;
    }

    MoveDebugLogLine("GlueMapLaunch: starting runtime map launch requested='" + requestedPath
                     + "' resolved='" + resolvedPath + "'");

    uiOnlyMode_ = false;
    debugUiGlueMenu_ = false;
    mapPath_ = resolvedPath;
    config_.localPlayerIndex = localPlayerIndex;

    if (fs::exists(mapPath_)) {
        mpq_.OpenOnTop(mapPath_);
    }

    if (!debugHudGameMode_) {
        scene_.Init(mpq_);
        splatMeta_.Load(mpq_);
    }

    if (!debugHudGameMode_) {
        audio_.Init();
    }
    if (!debugHudGameMode_ && audio_.IsValid() && mpq_.ArchiveCount() > 0) {
        audio_.SetMpqManager(&mpq_);
        audio_.LoadSound("ui_click", "UI\\Mouse\\HumanClick1.wav", &mpq_);
        audio_.LoadSound("ui_error", "UI\\Mouse\\HumanClickError.wav", &mpq_);
        uiSounds_.LoadFromMpq(mpq_);
        ackBank_.LoadFromMpq(mpq_);
    }

    hideHud_ = false;
    if (!ui_.SwitchHudLayout(&mpq_, screenW, screenH, "console", config_.gameVersion)) {
        hideHud_ = true;
        MoveDebugLogLine("GlueMapLaunch: HUD reload failed; continuing map launch with HUD hidden");
    } else {
        MoveDebugLogLine("GlueMapLaunch: HUD reloaded for gameplay");
        ui_.SetGlueMapLaunchCallback([this](const std::string& path, int localPlayer) {
            HandleGlueMapLaunchRequest(path, localPlayer);
        });
    }

    state_ = GameState::BOOTSTRAP;
    pendingState_ = GameState::BOOTSTRAP;
    stateTransition_ = false;
    mapConfigInitialized_ = false;
    loadingInitialized_ = false;
    loadingFailed_ = false;
    loadingFailureReason_.clear();
    simulationInitialized_ = false;
    terrainLoaded_ = false;
    glueMapLaunchInProgress_ = true;
    TransitionTo(GameState::MAP_CONFIG);
    MoveDebugLogLine("GlueMapLaunch: runtime map launch queued path='" + mapPath_ + "'");
    return true;
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

    if (!debugHudGameMode_) {
        scene_.ResetLoadStats();
    }

    W3ILoadFromMpq(mapPath_, w3i_);
    W3IPrint(w3i_);

    // Map terrain fog (ref useTerrainFog): w3i colors are 0-255 bytes.
    if (w3i_.useTerrainFog) {
        dayNight_.SetMapFog(true,
            w3i_.fogColor[0] / 255.0f, w3i_.fogColor[1] / 255.0f,
            w3i_.fogColor[2] / 255.0f, w3i_.fogDensity);
        dayNight_.SetMapFogRange(w3i_.fogHeight[0], w3i_.fogHeight[1]);
    } else {
        dayNight_.SetMapFog(false, 0.5f, 0.5f, 0.5f, 0.0f);
    }

    configEnv_ = std::make_unique<JassConfigEnv>();
    if (configEnv_->Init(mpq_, mapPath_, config_.jassFileList)) {
        configEnv_->Config();
        int configuredCount = configEnv_->GetConfiguredPlayerCount();
        if (configuredCount > 0) {
            std::cout << "JASS Config: " << configuredCount << " players configured" << std::endl;
        }
        // Campaign maps put the human on a non-zero slot (Human01: Player 1 =
        // MAP_CONTROL_USER). With no explicit --map local player, follow the
        // first USER slot instead of blindly using slot 0 (ref SetPlayerController).
        if (config_.localPlayerIndex < 0) {
            if (auto cfgData = configEnv_->GetData()) {
                for (int i = 0; i < (int)cfgData->playerMapConfig.size(); ++i) {
                    if (cfgData->playerMapConfig[i].controller == 0 /* MAP_CONTROL_USER */) {
                        config_.localPlayerIndex = i;
                        std::cout << "LocalPlayer: auto-selected USER slot " << i << " from config" << std::endl;
                        break;
                    }
                }
            }
        }
        // Single-player slot overrides (CLI --ai/--closed): applied after
        // config() so map-defined starts/teams survive; only the controller
        // and slot-active flag are rewritten (ref SetPlayerController).
        if (auto cfgData = configEnv_->GetData()) {
            for (int slot : config_.aiPlayerSlots) {
                if (slot >= 0 && slot < (int)cfgData->playerMapConfig.size()) {
                    cfgData->playerMapConfig[(size_t)slot].controller = 1; // MAP_CONTROL_COMPUTER
                    cfgData->playerMapConfig[(size_t)slot].slotActive = true;
                    std::cout << "SlotOverride: player " << slot << " -> COMPUTER AI" << std::endl;
                }
            }
            for (int slot : config_.closedPlayerSlots) {
                if (slot >= 0 && slot < (int)cfgData->playerMapConfig.size()) {
                    cfgData->playerMapConfig[(size_t)slot].controller = 5; // MAP_CONTROL_NONE
                    cfgData->playerMapConfig[(size_t)slot].slotActive = false;
                    std::cout << "SlotOverride: player " << slot << " -> CLOSED" << std::endl;
                }
            }
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
        mpq_, simulation_, unitData_, destructableData_, doodadData_,
        itemData_, upgradeData_, abilityData_, scene_, texturedTerrain_, tileset_, map_, w3i_, wpm_);
    mapLoader_->SetConfigData(configEnv_ ? configEnv_->GetData() : nullptr);
    mapLoader_->SetLoadRenderAssets(!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_);
    mapLoader_->CreateTasks(mapPath_, 0);
    if (!hideHud_) {
        ui_.ApplyMapAbilityMods(mapPath_);
    }
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
    if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_ && !texturedTerrain_.IsValid()) {
        loadingFailed_ = true;
        loadingFailureReason_ = "Terrain was not built";
        std::cerr << "InitInGame: " << loadingFailureReason_ << std::endl;
        return;
    }

    if (mapLoader_) {
        for (const auto& seed : mapLoader_->GetUnitVisualSeeds()) {
            presentation_.unitVisualSeeds[seed.handleId] = {
                {seed.scale[0], seed.scale[1], seed.scale[2]},
                seed.skinId, seed.hitpoints, seed.flags
            };
        }
        for (const auto& seed : mapLoader_->GetItemVisualSeeds()) {
            if (seed.handleId >= 0 && seed.visualIndex >= 0) {
                presentation_.itemToVisualIdx[seed.handleId] = seed.visualIndex;
            }
        }
        const auto& unitLoadResult = mapLoader_->GetUnitLoadResult();
        int visualUnitCreated = unitLoadResult.visualCreated;
        int visualMissForSimUnits = unitLoadResult.visualMissForSimUnits;
        simulation_.Update();
        simulationInitialized_ = true;
        ui_.SetSimulation(&simulation_);
        if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_) {
            for (auto* unit : simulation_.GetUnits()) {
                if (EnsureSimulationUnitVisual(unit))
                    visualUnitCreated++;
                else
                    visualMissForSimUnits++;
            }
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

    if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_) {
        minimap_.Generate(texturedTerrain_, tileset_, map_, w3i_, &mpq_);

        Vec3 center = texturedTerrain_.GetCenter();
        camera_.SetTarget(center.x, center.y, center.z);
        camera_.SetDistance(2500.0f);
        camera_.SetMaxDistance(5000.0f);
        camera_.SaveHomeState();
        if (overviewCamera_) {
            camera_.SetTarget(center.x, center.y, center.z);
            camera_.SetMaxDistance(20000.0f);
            camera_.SetDistance(11000.0f);
            camera_.SetAngles(0.8f, 1.35f);
            camera_.SaveHomeState();
            std::cout << "OverviewCamera: map center (" << center.x << ", " << center.y << ", " << center.z << ")" << std::endl;
        }
        // Spawn focus runs after JASS main + melee fallback below (town
        // halls/workers must exist for FindInitialSpawnFocusPosition).
        // Camera stays map-centered until then.
    }

    if (debugHudGameMode_) {
        std::cout << "JASS Common: skipped for HUD game debug" << std::endl;
    } else if (mpq_.ArchiveCount() > 0) {
        commonEnv_ = std::make_unique<JassCommonEnv>();
        if (commonEnv_->Init(mpq_, mapPath_, &simulation_, config_.jassFileList,
                             configEnv_ ? configEnv_->GetData() : nullptr)) {
            // Map-init dedupe window: units.doo placement and the map
            // script's CreateNeutral*/CreatePlayer* overlap; fold exact
            // duplicates instead of doubling creeps/mines (ref
            // CSimulation::SetMapInitPhase).
            simulation_.SetMapInitPhase(true);
            commonEnv_->Main();
            commonEnv_->PumpQueuedThreads(4096);
            jassNatives_ = commonEnv_->ReleaseNatives();
            WireJassCallbacks();
            if (jassNatives_) jassNatives_->PumpQueuedThreads(4096);
            simulation_.Update();
            if (jassNatives_) {
                jassNatives_->UpdateTimers(CSimulation::SIM_STEP_TIME);
                jassNatives_->UpdateRegions();
                jassNatives_->UpdateRescue();
                jassNatives_->UpdateTextTags(CSimulation::SIM_STEP_TIME);
                jassNatives_->PumpQueuedThreads(1024);
            }
            EnsureHumanMeleeStartsFromMapConfig();
            SpawnCarriedHeroes();
            FlushStartupSimulationState();
            EnsureComputerAIStarted();
            simulation_.SetMapInitPhase(false);
            // Real-game opening: focus the local start (town hall) now that
            // JASS main + melee fallback have created the starting units.
            // Previously this ran before spawns existed, silently failed, and
            // left the camera over unexplored map center (all-black fog).
            if (!overviewCamera_ && !debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_) {
                FocusCameraAndMinimapOnInitialSpawn();
            }
        } else {
            std::cerr << "JASS Common: unavailable; terrain/static scene remains ready" << std::endl;
        }
    } else {
        std::cout << "JASS Common: skipped because no resource MPQs are open" << std::endl;
    }

    if (debugSelectFirstUnitForHud_) {
        DebugSelectFirstUnitForHud();
    }

    if (dumpUnsupported_) {
        DumpUnsupportedDiagnostics();
    }

    if (debugGameplaySmoke_) {
        RunGameplaySmoke();
        std::cout << "GameplaySmoke: complete; requesting quit." << std::endl;
        sapp_request_quit();
    }

    if (diagnoseMapOnly_) {
        std::cout << "Diagnose-map complete; requesting quit." << std::endl;
        sapp_request_quit();
    }

    if (glueMapLaunchInProgress_) {
        std::ostringstream done;
        done << "GlueMapLaunch: entered map path='" << mapPath_
             << "' terrain=" << (texturedTerrain_.IsValid() ? "yes" : "no")
             << " units=" << simulation_.GetUnits().size()
             << " players=" << simulation_.GetPlayerCount();
        MoveDebugLogLine(done.str());
        glueMapLaunchInProgress_ = false;
    }

    std::cout << "Initialization complete. Starting render loop..." << std::endl;
}

void GameController::DumpUnsupportedDiagnostics() {
    std::cout << "=== Map Diagnostics ===" << std::endl;
    std::cout << "map=" << mapPath_ << std::endl;
    std::cout << "archives=" << mpq_.ArchiveCount() << std::endl;
    std::cout << "terrain valid=" << (texturedTerrain_.IsValid() ? "yes" : "no")
              << " size=" << map_.tileWidth << "x" << map_.tileHeight
              << " tileset=" << map_.tileset
              << " corners=" << map_.tiles.size() << std::endl;

    if (mapLoader_) {
        const auto& unitLoad = mapLoader_->GetUnitLoadResult();
        std::cout << "map_units total=" << unitLoad.mapUnitCount
                  << " sim_created=" << unitLoad.simCreated
                  << " visual_created=" << unitLoad.visualCreated
                  << " visual_miss_for_sim_units=" << unitLoad.visualMissForSimUnits
                  << " items_registered=" << unitLoad.itemRegistered << std::endl;
    }

    std::cout << "simulation units=" << simulation_.GetUnits().size()
              << " destructables=" << simulation_.GetDestructables().size()
              << " items=" << simulation_.GetItems().size()
              << " players=" << simulation_.GetPlayerCount() << std::endl;

    std::unordered_map<std::string, int> unknownFunctions;
    std::unordered_map<std::string, int> parserErrorCounts;
    int parserErrors = 0;
    int lexerErrors = 0;
    int otherErrors = 0;
    if (configEnv_) {
        AddJassErrorStats(configEnv_->GetVM().GetErrors(), unknownFunctions, parserErrorCounts,
                          parserErrors, lexerErrors, otherErrors);
    }
    if (commonEnv_) {
        AddJassErrorStats(commonEnv_->GetVM().GetErrors(), unknownFunctions, parserErrorCounts,
                          parserErrors, lexerErrors, otherErrors);
    }

    int totalUnknownCalls = 0;
    for (const auto& entry : unknownFunctions) {
        totalUnknownCalls += entry.second;
    }
    std::cout << "jass_unknown_unique=" << unknownFunctions.size()
              << " jass_unknown_calls=" << totalUnknownCalls
              << " parser_errors=" << parserErrors
              << " lexer_errors=" << lexerErrors
              << " other_errors=" << otherErrors << std::endl;
    PrintTopJassUnknowns(unknownFunctions);
    PrintTopJassParserErrors(parserErrorCounts);
    if (configEnv_) {
        PrintJassStaticDiagnosticsForVm("config", configEnv_->GetVM());
    }
    if (commonEnv_) {
        PrintJassStaticDiagnosticsForVm("common", commonEnv_->GetVM());
    }
    std::cout << "=== End Map Diagnostics ===" << std::endl;
}

void GameController::RunSyncInit() {
    if (uiOnlyMode_) {
        return;
    }

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
    if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_ && !terrainLoaded_) {
        loadingFailed_ = true;
        loadingFailureReason_ = "Terrain was not built";
        std::cerr << "RunSyncInit: " << loadingFailureReason_ << std::endl;
        pendingState_ = state_;
        stateTransition_ = false;
        return;
    }
    if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_) {
        scene_.PrintLoadStats();
    }
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
    // Race-correct fallback when the map script did not create starting
    // units (ref Blizzard.j MeleeStartingUnitsHuman/Orc/Undead/NightElf).
    const CPlayer* player = simulation_.GetPlayer(playerIndex);
    const int race = player ? player->GetRace() : 1;
    const War3ID townHallId = RaceTownHallId(race);
    const War3ID workerId = RaceWorkerId(race);
    if (!unitData_.HasUnitType(townHallId) || !unitData_.HasUnitType(workerId)) return;

    constexpr float townHallFacing = 270.0f;
    constexpr float peasantFacing = 90.0f;
    static const float peasantOffsets[][2] = {
        {-220.0f,  120.0f}, {-220.0f,  -20.0f}, {-220.0f, -160.0f},
        {-80.0f,   180.0f}, {-80.0f,  -220.0f},
    };

    if (CountPlayerTownHalls(simulation_, playerIndex) == 0) {
        simulation_.CreateUnit(townHallId, playerIndex, startX, startY, townHallFacing);
        std::cout << "MeleeFallback: created Town Hall for player " << playerIndex << std::endl;
    }
    float thX = startX, thY = startY;
    FindPlayerTownHallPosition(simulation_, playerIndex, thX, thY);
    int existingPeasants = CountPlayerUnitsOfType(simulation_, playerIndex, workerId);
    for (int i = existingPeasants; i < 5; ++i) {
        simulation_.CreateUnit(workerId, playerIndex, thX + peasantOffsets[i][0], thY + peasantOffsets[i][1], peasantFacing);
    }
    if (existingPeasants < 5)
        std::cout << "MeleeFallback: created " << (5 - existingPeasants) << " Peasants for player " << playerIndex << std::endl;
}

void GameController::EnsureHumanMeleeStartsFromMapConfig() {
    if (!jassNatives_ || !simulationInitialized_) return;
    if (IsCampaignMapPath(mapPath_)) {
        std::cout << "MeleeFallback: skipped for campaign map" << std::endl;
        return;
    }
    auto data = jassNatives_->GetData();
    if (!data) return;

    bool createdAny = false;
    int playerLimit = data->configuredPlayerCount > 0
        ? std::min(data->configuredPlayerCount, (int)data->playerMapConfig.size())
        : (int)data->playerMapConfig.size();

    for (int pi = 0; pi < playerLimit; ++pi) {
        const auto& pc = data->playerMapConfig[pi];
        if (pc.startLocationIndex < 0) continue;
        // Inactive/closed slots never get starting units (ref slotActive).
        if (!pc.slotActive) continue;
        const int si = pc.startLocationIndex;
        if (si < 0 || si >= (int)data->startLocations.size()) continue;
        const auto& start = data->startLocations[si];
        if (!start.defined) continue;
        if (auto* player = simulation_.GetPlayer(pi)) player->SetStartLocation(start.x, start.y);

        const int raceForCount = simulation_.GetPlayer(pi) ? simulation_.GetPlayer(pi)->GetRace() : 1;
        const War3ID workerForCount = RaceWorkerId(raceForCount);
        int beforeTH = CountPlayerTownHalls(simulation_, pi);
        int beforePeasants = CountPlayerUnitsOfType(simulation_, pi, workerForCount);
        EnsureHumanMeleeStartForPlayer(pi, start.x, start.y);
        if (CountPlayerTownHalls(simulation_, pi) > beforeTH ||
            CountPlayerUnitsOfType(simulation_, pi, workerForCount) > beforePeasants)
            createdAny = true;
    }
    if (createdAny) simulation_.Update();
}

void GameController::CollectCarriedHeroes() {
    carriedHeroes_.clear();
    arrivedViaChangeLevel_ = false;
    if (!simulationInitialized_) return;
    for (CUnit* unit : simulation_.GetUnits()) {
        CarriedHero carried = SnapshotHero(unit, simulation_);
        if (carried.typeId.value != 0) carriedHeroes_.push_back(std::move(carried));
    }
    arrivedViaChangeLevel_ = true;
    std::cout << "ChangeLevel: carrying " << carriedHeroes_.size() << " heroes" << std::endl;
}

void GameController::SpawnCarriedHeroes() {
    if (!arrivedViaChangeLevel_) return;
    arrivedViaChangeLevel_ = false;
    std::vector<CarriedHero> carried = std::move(carriedHeroes_);
    carriedHeroes_.clear();
    if (carried.empty() || !simulationInitialized_) return;
    auto* data = jassNatives_ ? jassNatives_->GetData().get() : nullptr;
    Vec3 center = texturedTerrain_.IsValid() ? texturedTerrain_.GetCenter() : Vec3(0, 0, 0);
    int spawned = 0;
    for (size_t hi = 0; hi < carried.size(); ++hi) {
        const CarriedHero& ch = carried[hi];
        CPlayer* owner = simulation_.GetPlayer(ch.ownerPlayer);
        if (!owner) continue;
        if (!simulation_.GetUnitData() ||
            !simulation_.GetUnitData()->GetUnitType(ch.typeId)) {
            continue; // hero type absent from the next map's data
        }
        // Start location, else an owned building, else map center, each
        // fanned out so stacked heroes do not overlap.
        float sx = center.x, sy = center.z;
        bool placed = data && FindPlayerStartLocationPosition(data, ch.ownerPlayer, sx, sy);
        if (!placed) {
            for (CUnit* unit : simulation_.GetUnits()) {
                if (!unit || unit->IsDead() || unit->GetPlayerIndex() != ch.ownerPlayer) continue;
                if (unit->GetUnitType() && unit->GetUnitType()->IsBuilding()) {
                    sx = unit->GetX();
                    sy = unit->GetY();
                    placed = true;
                    break;
                }
            }
        }
        const float angle = (float)hi * 2.39996f; // golden angle fan-out
        const float radius = 128.0f + 64.0f * (float)hi;
        sx += std::cos(angle) * radius;
        sy += std::sin(angle) * radius;
        CUnit* hero = RespawnCarriedHero(simulation_, ch, sx, sy);
        if (hero) spawned++;
    }
    if (spawned > 0) simulation_.Update();
    std::cout << "ChangeLevel: respawned " << spawned << "/" << carried.size()
              << " carried heroes" << std::endl;
}


void GameController::MaybeLogAIStatus() {
    if (!std::getenv("WARSMASH_AI_STATUS")) return;
    if (!simulationInitialized_ || !jassNatives_) return;
    const int tick = simulation_.GetGameTurnTick();
    static int lastLoggedTick = -100000;
    if (tick - lastLoggedTick < 200) return; // ~10s sim time
    lastLoggedTick = tick;
    auto data = jassNatives_->GetData();
    for (int pi = 0; pi < simulation_.GetPlayerCount(); ++pi) {
        const CPlayer* player = simulation_.GetPlayer(pi);
        if (!player || player->GetController() != CMapControl::COMPUTER) continue;
        int units = 0, buildings = 0, workers = 0;
        for (const CUnit* u : simulation_.GetUnits()) {
            if (!u || u->IsDead() || u->GetPlayerIndex() != pi) continue;
            ++units;
            const CUnitType* t = u->GetUnitType();
            if (t && t->IsBuilding()) { ++buildings; continue; }
            const War3ID id = u->GetTypeId();
            if (id == War3ID("hpea") || id == War3ID("opeo") ||
                id == War3ID("uaco") || id == War3ID("ewsp") ||
                u->GetAbility(War3ID("Ahar")) != nullptr) {
                ++workers;
            }
        }
        int thinks = -1, waves = -1;
        if (data) {
            auto it = data->aiControllers.find(pi);
            if (it != data->aiControllers.end() && it->second) {
                thinks = it->second->GetThinkCount();
                waves = it->second->GetWaveNumber();
            }
        }
        std::cout << "AIStatus: t=" << tick << " player=" << pi
                  << " race=" << player->GetRace()
                  << " units=" << units << " buildings=" << buildings
                  << " workers=" << workers << " gold=" << player->GetGold()
                  << " lumber=" << player->GetLumber()
                  << " thinks=" << thinks << " waves=" << waves;
        // Probe first worker: ability + behavior, to diagnose harvest stalls.
        for (const CUnit* u : simulation_.GetUnits()) {
            if (!u || u->IsDead() || u->GetPlayerIndex() != pi) continue;
            const War3ID id = u->GetTypeId();
            if (id == War3ID("hpea") || id == War3ID("opeo") ||
                id == War3ID("uaco") || id == War3ID("ewsp")) {
                const bool hasHar = u->GetAbility(War3ID("Ahar")) != nullptr;
                const CBehavior* b = u->GetCurrentBehavior();
                int carried = 0;
                if (const CAbility* har = u->GetAbility(War3ID("Ahar"))) {
                    carried = har->GetCarriedAmount();
                }
                float hallDist = -1.0f;
                for (const CUnit* h : simulation_.GetUnits()) {
                    if (!h || h->IsDead() || h->GetPlayerIndex() != pi) continue;
                    if (h->GetUnitType() && h->GetUnitType()->IsBuilding()) {
                        const float dx = h->GetX() - u->GetX();
                        const float dy = h->GetY() - u->GetY();
                        const float d = std::sqrt(dx * dx + dy * dy);
                        if (hallDist < 0.0f || d < hallDist) hallDist = d;
                    }
                }
                std::cout << " probeWorker=" << id.ToString()
                          << " hasAhar=" << (hasHar ? 1 : 0)
                          << " queued=" << (u->HasQueuedOrders() ? 1 : 0)
                          << " behav=" << (b ? (int)b->GetCategory() : -1)
                          << " carried=" << carried
                          << " pos=(" << (int)u->GetX() << "," << (int)u->GetY() << ")"
                          << " hallDist=" << (int)hallDist
                          << " speed=" << u->GetSpeed()
                          << " turnRate=" << u->GetTurnRate()
                          << " moveType=" << (u->GetUnitType() ? (int)u->GetMovementType() : -1)
                          << " facing=" << (int)u->GetFacing()
                          << " coll=" << u->GetCollisionSize();
                if (auto* pf = simulation_.GetPathfinding()) {
                    std::cout << " steps=";
                    for (int dir = 0; dir < 8; ++dir) {
                        const float ang = dir * 3.14159265f / 4.0f;
                        const float nx = u->GetX() + std::cos(ang) * 24.0f;
                        const float ny = u->GetY() + std::sin(ang) * 24.0f;
                        const bool ok = pf->IsWorldPointPathableForUnit(
                            nx, ny, std::max(0.0f, u->GetCollisionSize()),
                            u->GetMovementType(), const_cast<CUnit*>(u));
                        std::cout << (ok ? '1' : '0');
                    }
                    std::cout << " near=[";
                    int shown = 0;
                    for (const CUnit* o : simulation_.GetUnits()) {
                        if (!o || o == u || o->IsDead()) continue;
                        const float dx = o->GetX() - u->GetX();
                        const float dy = o->GetY() - u->GetY();
                        if (dx * dx + dy * dy > 200.0f * 200.0f) continue;
                        if (shown++ > 0) std::cout << ",";
                        std::cout << o->GetTypeId().ToString() << "p" << o->GetPlayerIndex()
                                  << "@" << (int)std::sqrt(dx * dx + dy * dy)
                                  << "c" << (int)o->GetCollisionSize()
                                  << (o->IsHidden() ? "H" : "");
                    }
                    std::cout << "]";
                }
                break;
            }
        }
        std::cout << std::endl;
    }
}

void GameController::EnsureComputerAIStarted() {
    if (!jassNatives_ || !simulationInitialized_) return;
    auto data = jassNatives_->GetData();
    if (!data) return;
    for (int pi = 0; pi < simulation_.GetPlayerCount(); ++pi) {
        const CPlayer* player = simulation_.GetPlayer(pi);
        if (!player || player->GetController() != CMapControl::COMPUTER) continue;
        if (pi < (int)data->playerMapConfig.size() && !data->playerMapConfig[(size_t)pi].slotActive) continue;
        auto it = data->aiControllers.find(pi);
        if (it != data->aiControllers.end() && it->second) continue;
        auto controller = std::make_shared<MeleeAIController>(&simulation_, pi);
        controller->SetDifficulty(data->playerAI[pi].difficulty);
        auto paused = data->compAIpaused.find(pi);
        if (paused != data->compAIpaused.end() && paused->second) controller->SetEnabled(false);
        data->aiControllers[pi] = std::move(controller);
        std::cout << "ComputerAI: started MeleeAIController for player " << pi << std::endl;
    }
}

void GameController::FlushStartupSimulationState() {
    if (!simulationInitialized_) return;
    if (!diagnoseMapOnly_ && !debugGameplaySmoke_) {
        SyncAndReconcileVisuals();
    }
    if (!jassNatives_) return;
    constexpr int maxWarmup = 4;
    for (int i = 0; i < maxWarmup; ++i) {
        size_t before = simulation_.GetUnits().size();
        simulation_.Update();
        int timers = jassNatives_->UpdateTimers(CSimulation::SIM_STEP_TIME);
        jassNatives_->UpdateRegions();
        jassNatives_->UpdateRescue();
        int pumped = jassNatives_->PumpQueuedThreads(1024);
        if (!diagnoseMapOnly_ && !debugGameplaySmoke_) {
            SyncAndReconcileVisuals();
        }
        if (timers == 0 && pumped == 0 && simulation_.GetUnits().size() == before) break;
    }
}


void GameController::DebugSelectFirstUnitForHud() {
    if (!simulationInitialized_) return;

    CUnit* fallback = nullptr;
    CUnit* fallbackNonGoldMine = nullptr;
    CUnit* preferredWorker = nullptr;
    CUnit* localPlayerBuilding = nullptr;
    CUnit* localPlayerUnit = nullptr;
    const int localPlayer = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;

    auto isGoldMine = [](CUnit* unit) {
        return unit && unit->GetAbility(War3ID("Agld")) != nullptr;
    };

    for (auto* unit : simulation_.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
        if (!fallback) fallback = unit;
        if (!fallbackNonGoldMine && !isGoldMine(unit)) {
            fallbackNonGoldMine = unit;
        }
        if (unit->GetPlayerIndex() == localPlayer) {
            if (!isGoldMine(unit)) {
                if (!preferredWorker && unit->GetTypeId() == War3ID("hpea")) {
                    preferredWorker = unit;
                }
                if (!unit->IsBuilding()) {
                    localPlayerUnit = unit;
                }
                if (!localPlayerBuilding) {
                    localPlayerBuilding = unit;
                }
            }
        }
    }

    CUnit* selected = preferredWorker ? preferredWorker
        : (localPlayerUnit ? localPlayerUnit
        : (localPlayerBuilding ? localPlayerBuilding
        : (fallbackNonGoldMine ? fallbackNonGoldMine : fallback)));
    if (!selected) {
        std::cout << "DebugSelectFirstUnitForHud: no selectable unit found" << std::endl;
        return;
    }

    SetSelectedUnits({selected}, selected->GetHandleId(), true);
    std::cout << "DebugSelectFirstUnitForHud: selected "
              << unit_text_utils::DescribeUnitType(selected->GetTypeId(), unitData_)
              << " handle=" << selected->GetHandleId()
              << " player=" << selected->GetPlayerIndex() << std::endl;
}

void GameController::RunGameplaySmoke() {
    if (!simulationInitialized_) {
        std::cout << "GameplaySmoke: result=fail reason=simulation_not_initialized" << std::endl;
        return;
    }

    const int playerIndex = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
    CUnit* worker = FindGameplaySmokeWorker(simulation_, playerIndex);
    CUnit* dropoff = FindPlayerDropoff(simulation_, playerIndex);
    CUnit* mine = FindNearestGoldMine(simulation_, worker);
    CPlayer* player = simulation_.GetPlayer(playerIndex);
    CAbility* harvest = worker ? worker->GetAbility(War3ID("Ahar")) : nullptr;

    if (!worker || !dropoff || !mine || !player || !harvest) {
        std::cout << "GameplaySmoke: result=fail"
                  << " worker=" << (worker ? "yes" : "no")
                  << " dropoff=" << (dropoff ? "yes" : "no")
                  << " mine=" << (mine ? "yes" : "no")
                  << " player=" << (player ? "yes" : "no")
                  << " harvestAbility=" << (harvest ? "yes" : "no")
                  << std::endl;
        return;
    }

    auto pumpTicks = [&](int maxTicks, const std::function<bool()>& done) {
        for (int i = 0; i < maxTicks; ++i) {
            simulation_.Update();
            if (jassNatives_) {
                jassNatives_->UpdateTimers(CSimulation::SIM_STEP_TIME);
                jassNatives_->UpdateRegions();
                jassNatives_->UpdateRescue();
                jassNatives_->PumpQueuedThreads(128);
            }
            if (done && done()) return i + 1;
        }
        return maxTicks;
    };

    auto issuePendingFromUi = [&](CWidget* targetWidget, float x, float y) {
        const int pendingCmd = ui_.GetPendingCommand();
        const auto pendingMode = ui_.GetPendingCommandMode();
        if (pendingCmd < 0) {
            return false;
        }
        ui_.CompletePendingCommand();
        bool issued = false;
        for (auto* unit : GetSelectedSimulationUnits()) {
            if (!unit) continue;
            if (pendingMode != MainUI::PendingCommandMode::POINT_ONLY && targetWidget) {
                unit->IssueOrder(std::make_unique<COrderTargetWidget>(pendingCmd, targetWidget));
            } else {
                unit->IssueOrder(std::make_unique<COrderTargetPoint>(pendingCmd, x, y));
            }
            issued = true;
        }
        return issued;
    };

    SetSelectedUnits({worker}, worker->GetHandleId(), true);
    const bool uiHarvestInitial = ui_.IsCommandOrderEnabled(OrderIds::HARVEST);

    const float moveStartX = worker->GetX();
    const float moveStartY = worker->GetY();
    worker->ClearOrders();
    const bool uiMoveClicked = ui_.InvokeCommandOrderForDiagnostics(OrderIds::MOVE);
    const bool uiMoveIssued = issuePendingFromUi(nullptr, moveStartX + 256.0f, moveStartY);
    const int moveTicks = pumpTicks(120, [&]() {
        const float dx = worker->GetX() - moveStartX;
        const float dy = worker->GetY() - moveStartY;
        return std::sqrt(dx * dx + dy * dy) > 32.0f;
    });
    const float moveDx = worker->GetX() - moveStartX;
    const float moveDy = worker->GetY() - moveStartY;
    const bool movePass = uiMoveClicked && uiMoveIssued && std::sqrt(moveDx * moveDx + moveDy * moveDy) > 32.0f;
    std::cout << "GameplaySmoke: move=" << (movePass ? "pass" : "fail")
              << " ticks=" << moveTicks
              << " uiClicked=" << (uiMoveClicked ? "yes" : "no")
              << " uiIssued=" << (uiMoveIssued ? "yes" : "no")
              << " start=(" << moveStartX << "," << moveStartY << ")"
              << " end=(" << worker->GetX() << "," << worker->GetY() << ")"
              << std::endl;

    worker->ClearOrders();
    worker->SetHidden(false);
    worker->SetInvulnerable(false);
    harvest->ClearCarried();
    worker->SetPosition(mine->GetX() + 96.0f, mine->GetY());
    const int mineGoldBefore = mine->GetAbility(War3ID("Agld"))
        ? mine->GetAbility(War3ID("Agld"))->GetGold()
        : 0;
    const bool uiHarvestClicked = ui_.InvokeCommandOrderForDiagnostics(OrderIds::HARVEST);
    const bool uiHarvestIssued = issuePendingFromUi(mine, mine->GetX(), mine->GetY());
    const int harvestTicks = pumpTicks(420, [&]() {
        return harvest->GetCarriedAmount() > 0;
    });
    const bool harvestPass = uiHarvestClicked && uiHarvestIssued && harvest->GetCarriedAmount() > 0;
    RefreshSelectionUi(true);
    const bool uiReturnAfterHarvest = ui_.IsCommandOrderEnabled(OrderIds::RETURN_RESOURCES);
    std::cout << "GameplaySmoke: harvest=" << (harvestPass ? "pass" : "fail")
              << " ticks=" << harvestTicks
              << " uiClicked=" << (uiHarvestClicked ? "yes" : "no")
              << " uiIssued=" << (uiHarvestIssued ? "yes" : "no")
              << " carried=" << harvest->GetCarriedAmount()
              << " type=" << static_cast<int>(harvest->GetCarriedResourceType())
              << " mineGoldBefore=" << mineGoldBefore
              << std::endl;

    const int goldBeforeReturn = player->GetGold();
    worker->ClearOrders();
    worker->SetHidden(false);
    worker->SetInvulnerable(false);
    worker->SetPosition(dropoff->GetX() + 96.0f, dropoff->GetY());
    const bool uiReturnClicked = ui_.InvokeCommandOrderForDiagnostics(OrderIds::RETURN_RESOURCES);
    const bool uiReturnIssued = issuePendingFromUi(dropoff, dropoff->GetX(), dropoff->GetY());
    const int returnTicks = pumpTicks(180, [&]() {
        return player->GetGold() > goldBeforeReturn && harvest->GetCarriedAmount() == 0;
    });
    const bool returnPass = uiReturnClicked && uiReturnIssued
        && player->GetGold() > goldBeforeReturn && harvest->GetCarriedAmount() == 0;
    std::cout << "GameplaySmoke: return=" << (returnPass ? "pass" : "fail")
              << " ticks=" << returnTicks
              << " uiClicked=" << (uiReturnClicked ? "yes" : "no")
              << " uiIssued=" << (uiReturnIssued ? "yes" : "no")
              << " goldBefore=" << goldBeforeReturn
              << " goldAfter=" << player->GetGold()
              << " carried=" << harvest->GetCarriedAmount()
              << std::endl;

    CAbility* queueAbility = dropoff->GetAbility(War3ID("Aque"));
    SetSelectedUnits({dropoff}, dropoff->GetHandleId(), true);
    const bool uiTrainBeforeQueue = ui_.IsCommandOrderEnabled((int)War3ID("hpea").value);
    const int peasantsBeforeTrain = CountPlayerUnitsOfType(simulation_, playerIndex, War3ID("hpea"));
    const int goldBeforeTrain = player->GetGold();
    const int lumberBeforeTrain = player->GetLumber();
    const int foodBeforeTrain = player->GetFoodUsed();
    bool trainQueued = false;
    bool trainCompleted = false;
    int trainTicks = 0;
    bool uiTrainClicked = false;
    if (queueAbility && queueAbility->GetTrainedUnits().count(War3ID("hpea")) > 0) {
        uiTrainClicked = ui_.InvokeCommandOrderForDiagnostics((int)War3ID("hpea").value);
        trainTicks = pumpTicks(900, [&]() {
            trainQueued = trainQueued || queueAbility->GetQueueCount() > 0;
            trainCompleted = CountPlayerUnitsOfType(simulation_, playerIndex, War3ID("hpea")) > peasantsBeforeTrain;
            return trainCompleted;
        });
    }
    const int peasantsAfterTrain = CountPlayerUnitsOfType(simulation_, playerIndex, War3ID("hpea"));
    const bool trainPass = queueAbility
        && uiTrainClicked
        && trainQueued
        && trainCompleted
        && peasantsAfterTrain > peasantsBeforeTrain
        && player->GetGold() < goldBeforeTrain
        && player->GetFoodUsed() > foodBeforeTrain;
    std::cout << "GameplaySmoke: train=" << (trainPass ? "pass" : "fail")
              << " ticks=" << trainTicks
              << " queueAbility=" << (queueAbility ? "yes" : "no")
              << " uiClicked=" << (uiTrainClicked ? "yes" : "no")
              << " queued=" << (trainQueued ? "yes" : "no")
              << " peasantsBefore=" << peasantsBeforeTrain
              << " peasantsAfter=" << peasantsAfterTrain
              << " goldBefore=" << goldBeforeTrain
              << " goldAfter=" << player->GetGold()
              << " lumberBefore=" << lumberBeforeTrain
              << " lumberAfter=" << player->GetLumber()
              << " foodBefore=" << foodBeforeTrain
              << " foodAfter=" << player->GetFoodUsed()
              << std::endl;

    const War3ID farmId("hhou");
    float buildX = 0.0f;
    float buildY = 0.0f;
    const bool buildSiteFound = FindGameplaySmokeBuildSite(simulation_, dropoff, farmId, buildX, buildY);
    const auto* farmType = unitData_.GetUnitType(farmId);
    const float farmCollision = farmType ? std::max(0.0f, farmType->GetCollisionSize()) : 64.0f;
    const float workerCollision = std::max(0.0f, worker->GetCollisionSize());
    const float builderOffset = std::min(124.0f, std::max(96.0f, farmCollision + workerCollision + 16.0f));
    const int farmsBeforeBuild = CountPlayerUnitsOfType(simulation_, playerIndex, farmId);
    const int goldBeforeBuild = player->GetGold();
    const int lumberBeforeBuild = player->GetLumber();
    const int foodCapBeforeBuild = player->GetFoodCap();
    bool uiBuildMenuClicked = false;
    bool uiFarmEnabled = false;
    bool uiFarmClicked = false;
    bool uiFarmIssued = false;
    bool buildStarted = false;
    bool buildCompleted = false;
    int buildTicks = 0;
    int builtFarmHandle = -1;

    if (buildSiteFound) {
        worker->ClearOrders();
        worker->SetHidden(false);
        worker->SetInvulnerable(false);
        worker->SetPosition(buildX - builderOffset, buildY);
        SetSelectedUnits({worker}, worker->GetHandleId(), true);
        uiBuildMenuClicked = ui_.InvokeCommandOrderForDiagnostics(OrderIds::HUMAN_BUILD);
        uiFarmEnabled = ui_.IsCommandOrderEnabled((int)farmId.value);
        uiFarmClicked = ui_.InvokeCommandOrderForDiagnostics((int)farmId.value);
        uiFarmIssued = issuePendingFromUi(nullptr, buildX, buildY);
        buildTicks = pumpTicks(1400, [&]() {
            for (auto* unit : simulation_.GetUnits()) {
                if (!unit || unit->GetPlayerIndex() != playerIndex || unit->GetTypeId() != farmId) continue;
                if (unit->GetHandleId() == worker->GetHandleId()) continue;
                if (unit->GetHandleId() != builtFarmHandle && unit->GetHandleId() >= 0) {
                    builtFarmHandle = unit->GetHandleId();
                }
                if (unit->IsConstructing()) {
                    buildStarted = true;
                }
                if (!unit->IsConstructing() && unit->GetHandleId() == builtFarmHandle) {
                    buildCompleted = true;
                    return true;
                }
            }
            return false;
        });
    }

    const int farmsAfterBuild = CountPlayerUnitsOfType(simulation_, playerIndex, farmId);
    const bool buildPass = buildSiteFound
        && uiBuildMenuClicked
        && uiFarmEnabled
        && uiFarmClicked
        && uiFarmIssued
        && buildStarted
        && buildCompleted
        && farmsAfterBuild > farmsBeforeBuild
        && player->GetGold() < goldBeforeBuild
        && player->GetLumber() <= lumberBeforeBuild
        && player->GetFoodCap() > foodCapBeforeBuild;
    std::cout << "GameplaySmoke: build=" << (buildPass ? "pass" : "fail")
              << " ticks=" << buildTicks
              << " siteFound=" << (buildSiteFound ? "yes" : "no")
              << " uiBuildMenuClicked=" << (uiBuildMenuClicked ? "yes" : "no")
              << " farmButton=" << (uiFarmEnabled ? "enabled" : "disabled")
              << " uiFarmClicked=" << (uiFarmClicked ? "yes" : "no")
              << " uiIssued=" << (uiFarmIssued ? "yes" : "no")
              << " started=" << (buildStarted ? "yes" : "no")
              << " completed=" << (buildCompleted ? "yes" : "no")
              << " farmsBefore=" << farmsBeforeBuild
              << " farmsAfter=" << farmsAfterBuild
              << " goldBefore=" << goldBeforeBuild
              << " goldAfter=" << player->GetGold()
              << " lumberBefore=" << lumberBeforeBuild
              << " lumberAfter=" << player->GetLumber()
              << " foodCapBefore=" << foodCapBeforeBuild
              << " foodCapAfter=" << player->GetFoodCap()
              << " site=(" << buildX << "," << buildY << ")"
              << std::endl;

    const int enemyPlayerIndex = (playerIndex == 0) ? 1 : 0;
    CUnit* attackTarget = simulation_.CreateUnit(War3ID("hpea"), enemyPlayerIndex,
                                                 worker->GetX() + 96.0f, worker->GetY(), 180.0f);
    if (attackTarget) {
        simulation_.Update();
        attackTarget->SetLife(simulation_, 6.0f);
    }

    bool uiAttackEnabled = false;
    bool uiAttackClicked = false;
    bool uiAttackIssued = false;
    bool attackDamaged = false;
    bool attackKilled = false;
    bool deadSelectionCleared = false;
    int attackTicks = 0;
    const int attackTargetHandle = attackTarget ? attackTarget->GetHandleId() : -1;
    const float attackTargetLifeBefore = attackTarget ? attackTarget->GetLife() : 0.0f;
    if (attackTarget) {
        worker->ClearOrders();
        worker->SetHidden(false);
        worker->SetInvulnerable(false);
        worker->SetPosition(attackTarget->GetX() - 96.0f, attackTarget->GetY());
        SetSelectedUnits({worker}, worker->GetHandleId(), true);
        uiAttackEnabled = ui_.IsCommandOrderEnabled(OrderIds::ATTACK);
        uiAttackClicked = ui_.InvokeCommandOrderForDiagnostics(OrderIds::ATTACK);
        uiAttackIssued = issuePendingFromUi(attackTarget, attackTarget->GetX(), attackTarget->GetY());
        attackTicks = pumpTicks(240, [&]() {
            CUnit* target = simulation_.GetUnit(attackTargetHandle);
            if (!target) return false;
            attackDamaged = attackDamaged || target->GetLife() < attackTargetLifeBefore;
            attackKilled = target->IsDead();
            return attackKilled;
        });
        CUnit* target = simulation_.GetUnit(attackTargetHandle);
        if (target && target->IsDead()) {
            SetSelectedUnits({target}, target->GetHandleId(), true);
            deadSelectionCleared = selectedUnitHandles_.empty() && selectedUnitHandle_ < 0;
        }
    }

    const bool attackPass = attackTarget
        && uiAttackEnabled
        && uiAttackClicked
        && uiAttackIssued
        && attackDamaged
        && attackKilled
        && deadSelectionCleared;
    std::cout << "GameplaySmoke: attack=" << (attackPass ? "pass" : "fail")
              << " ticks=" << attackTicks
              << " target=" << (attackTarget ? "yes" : "no")
              << " uiAttackButton=" << (uiAttackEnabled ? "enabled" : "disabled")
              << " uiClicked=" << (uiAttackClicked ? "yes" : "no")
              << " uiIssued=" << (uiAttackIssued ? "yes" : "no")
              << " damaged=" << (attackDamaged ? "yes" : "no")
              << " killed=" << (attackKilled ? "yes" : "no")
              << " deadSelectionCleared=" << (deadSelectionCleared ? "yes" : "no")
              << " targetHandle=" << attackTargetHandle
              << " lifeBefore=" << attackTargetLifeBefore
              << " lifeAfter=" << (attackTarget ? attackTarget->GetLife() : 0.0f)
              << std::endl;

    const War3ID dynamicAbilityId("AHds");
    const bool dynamicAbsentBefore = worker->GetAbility(dynamicAbilityId) == nullptr;
    bool dynamicAdded = false;
    bool dynamicLevelSet = false;
    bool dynamicRemoved = false;
    if (dynamicAbsentBefore) {
        auto ability = AbilityFactory::CreateAbility(simulation_.AllocateHandleId(), dynamicAbilityId);
        if (ability) {
            worker->AddAbility(simulation_, std::move(ability));
            if (auto* added = worker->GetAbility(dynamicAbilityId)) {
                dynamicAdded = true;
                added->SetLevel(2);
                dynamicLevelSet = added->GetLevel() == 2;
            }
            dynamicRemoved = worker->RemoveAbility(simulation_, dynamicAbilityId);
        }
    }
    const bool dynamicAbilityPass = dynamicAbsentBefore && dynamicAdded && dynamicLevelSet && dynamicRemoved
        && worker->GetAbility(dynamicAbilityId) == nullptr;
    std::cout << "GameplaySmoke: abilityDynamic=" << (dynamicAbilityPass ? "pass" : "fail")
              << " ability=" << dynamicAbilityId.ToString()
              << " absentBefore=" << (dynamicAbsentBefore ? "yes" : "no")
              << " added=" << (dynamicAdded ? "yes" : "no")
              << " levelSet=" << (dynamicLevelSet ? "yes" : "no")
              << " removed=" << (dynamicRemoved ? "yes" : "no")
              << std::endl;

    std::cout << "GameplaySmoke: ui="
              << ((uiHarvestInitial && uiReturnAfterHarvest && uiTrainBeforeQueue && uiAttackEnabled) ? "pass" : "fail")
              << " harvestButton=" << (uiHarvestInitial ? "enabled" : "disabled")
              << " returnButton=" << (uiReturnAfterHarvest ? "enabled" : "disabled")
              << " trainPeasantButton=" << (uiTrainBeforeQueue ? "enabled" : "disabled")
              << " attackButton=" << (uiAttackEnabled ? "enabled" : "disabled")
              << std::endl;

    std::cout << "GameplaySmoke: result="
              << ((movePass && harvestPass && returnPass && trainPass
                   && buildPass
                   && attackPass
                   && dynamicAbilityPass
                   && uiHarvestInitial && uiReturnAfterHarvest && uiTrainBeforeQueue) ? "pass" : "fail")
              << " worker=" << unit_text_utils::DescribeUnitType(worker->GetTypeId(), unitData_)
              << " workerHandle=" << worker->GetHandleId()
              << " mineHandle=" << mine->GetHandleId()
              << " dropoffHandle=" << dropoff->GetHandleId()
              << std::endl;
}
