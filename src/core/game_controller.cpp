#include "core/game_controller_internal.h"

GameController::GameController() {
    inputActionMap_.RegisterStandardWarcraftDefaults();
}
GameController::~GameController() = default;

void GameController::ParseArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "--screenshot") == 0 && i + 1 < argc) {
            screenshotPath = argv[++i];
            screenshotFrame = 1;
        } else if (std::strncmp(arg, "--screenshot=", 13) == 0) {
            screenshotPath = arg + 13;
            screenshotFrame = 1;
        } else if (std::strcmp(arg, "--screenshot-frame") == 0 && i + 1 < argc) {
            screenshotFrame = std::max(0, std::atoi(argv[++i]));
        } else if (std::strncmp(arg, "--screenshot-frame=", 19) == 0) {
            screenshotFrame = std::max(0, std::atoi(arg + 19));
        } else if (std::strcmp(arg, "--exit-after-frame") == 0 && i + 1 < argc) {
            exitAfterFrame_ = std::max(0, std::atoi(argv[++i]));
        } else if (std::strncmp(arg, "--exit-after-frame=", 19) == 0) {
            exitAfterFrame_ = std::max(0, std::atoi(arg + 19));
        } else if (std::strcmp(arg, "--ui-only") == 0) {
            uiOnlyMode_ = true;
        } else if (std::strcmp(arg, "--hide-hud") == 0) {
            hideHud_ = true;
        } else if (std::strcmp(arg, "--terrain-only") == 0) {
            terrainOnly_ = true;
            hideHud_ = true;
        } else if (std::strcmp(arg, "--dump-unsupported") == 0) {
            dumpUnsupported_ = true;
        } else if (std::strcmp(arg, "--diagnose-map") == 0) {
            diagnoseMapOnly_ = true;
            dumpUnsupported_ = true;
            if (exitAfterFrame_ < 0) exitAfterFrame_ = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                mapPath_ = argv[++i];
            }
        } else if (std::strncmp(arg, "--diagnose-map=", 15) == 0) {
            diagnoseMapOnly_ = true;
            dumpUnsupported_ = true;
            if (exitAfterFrame_ < 0) exitAfterFrame_ = 1;
            mapPath_ = arg + 15;
        } else if (std::strcmp(arg, "--debug-gameplay-smoke") == 0) {
            debugGameplaySmoke_ = true;
            dumpUnsupported_ = true;
        } else if (std::strcmp(arg, "--overview-camera") == 0) {
            overviewCamera_ = true;
        } else if (std::strcmp(arg, "--scene-kind") == 0 && i + 1 < argc) {
            debugSceneRenderKind_ = argv[++i];
            hideHud_ = true;
        } else if (std::strncmp(arg, "--scene-kind=", 13) == 0) {
            debugSceneRenderKind_ = arg + 13;
            hideHud_ = true;
        } else if (std::strcmp(arg, "--scene-first") == 0 && i + 1 < argc) {
            debugSceneRenderStart_ = 0;
            debugSceneRenderCount_ = std::max(0, std::atoi(argv[++i]));
            hideHud_ = true;
        } else if (std::strncmp(arg, "--scene-first=", 14) == 0) {
            debugSceneRenderStart_ = 0;
            debugSceneRenderCount_ = std::max(0, std::atoi(arg + 14));
            hideHud_ = true;
        } else if (std::strcmp(arg, "--scene-range") == 0 && i + 1 < argc) {
            int start = 0;
            int count = -1;
            std::sscanf(argv[++i], "%d,%d", &start, &count);
            debugSceneRenderStart_ = std::max(0, start);
            debugSceneRenderCount_ = count;
            hideHud_ = true;
        } else if (std::strncmp(arg, "--scene-range=", 14) == 0) {
            int start = 0;
            int count = -1;
            std::sscanf(arg + 14, "%d,%d", &start, &count);
            debugSceneRenderStart_ = std::max(0, start);
            debugSceneRenderCount_ = count;
            hideHud_ = true;
        } else if (std::strcmp(arg, "--debug-hud-game") == 0) {
            debugHudGameMode_ = true;
            debugSelectFirstUnitForHud_ = true;
        } else if (std::strcmp(arg, "--debug-ui-command-card") == 0) {
            debugUiCommandCard_ = true;
        } else if (std::strcmp(arg, "--debug-ui-game-menu") == 0) {
            debugUiGameMenu_ = true;
        } else if (std::strcmp(arg, "--debug-ui-game-menu-panel") == 0 && i + 1 < argc) {
            debugUiGameMenuPanel_ = argv[++i];
            debugUiGameMenu_ = true;
        } else if (std::strncmp(arg, "--debug-ui-game-menu-panel=", 27) == 0) {
            debugUiGameMenuPanel_ = arg + 27;
            debugUiGameMenu_ = true;
        } else if (std::strcmp(arg, "--debug-ui-game-menu-actions") == 0 && i + 1 < argc) {
            AppendCommaSeparatedArgs(debugUiGameMenuActions_, argv[++i]);
            debugUiGameMenu_ = true;
        } else if (std::strncmp(arg, "--debug-ui-game-menu-actions=", 29) == 0) {
            AppendCommaSeparatedArgs(debugUiGameMenuActions_, arg + 29);
            debugUiGameMenu_ = true;
        } else if (std::strcmp(arg, "--debug-ui-glue-menu") == 0) {
            debugUiGlueMenu_ = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                debugUiGlueMenuPage_ = argv[++i];
            }
            if (hudLayoutPath_ == "console") {
                hudLayoutPath_ = "glue";
            }
        } else if (std::strncmp(arg, "--debug-ui-glue-menu=", 21) == 0) {
            debugUiGlueMenu_ = true;
            debugUiGlueMenuPage_ = arg + 21;
            if (hudLayoutPath_ == "console") {
                hudLayoutPath_ = "glue";
            }
        } else if ((std::strcmp(arg, "--debug-ui-glue-action") == 0 ||
                    std::strcmp(arg, "--debug-ui-glue-actions") == 0) && i + 1 < argc) {
            AppendCommaSeparatedArgs(debugUiGlueMenuActions_, argv[++i]);
            debugUiGlueMenu_ = true;
            if (hudLayoutPath_ == "console") {
                hudLayoutPath_ = "glue";
            }
        } else if (std::strncmp(arg, "--debug-ui-glue-action=", 23) == 0) {
            AppendCommaSeparatedArgs(debugUiGlueMenuActions_, arg + 23);
            debugUiGlueMenu_ = true;
            if (hudLayoutPath_ == "console") {
                hudLayoutPath_ = "glue";
            }
        } else if (std::strncmp(arg, "--debug-ui-glue-actions=", 24) == 0) {
            AppendCommaSeparatedArgs(debugUiGlueMenuActions_, arg + 24);
            debugUiGlueMenu_ = true;
            if (hudLayoutPath_ == "console") {
                hudLayoutPath_ = "glue";
            }
        } else if (std::strcmp(arg, "--debug-ui-upper-button") == 0 && i + 1 < argc) {
            debugUiUpperButton_ = argv[++i];
        } else if (std::strncmp(arg, "--debug-ui-upper-button=", 24) == 0) {
            debugUiUpperButton_ = arg + 24;
        } else if (std::strcmp(arg, "--debug-ui-resources") == 0 && i + 1 < argc) {
            if (ParseDebugUiResources(argv[++i], debugUiGold_, debugUiLumber_, debugUiFoodUsed_, debugUiFoodCap_, debugUiTimeText_)) {
                debugUiResources_ = true;
            } else {
                std::cerr << "Invalid --debug-ui-resources value. Expected gold,lumber,foodUsed,foodCap[,time]" << std::endl;
            }
        } else if (std::strncmp(arg, "--debug-ui-resources=", 21) == 0) {
            if (ParseDebugUiResources(arg + 21, debugUiGold_, debugUiLumber_, debugUiFoodUsed_, debugUiFoodCap_, debugUiTimeText_)) {
                debugUiResources_ = true;
            } else {
                std::cerr << "Invalid --debug-ui-resources value. Expected gold,lumber,foodUsed,foodCap[,time]" << std::endl;
            }
        } else if (std::strcmp(arg, "--hud-layout") == 0 && i + 1 < argc) {
            hudLayoutPath_ = argv[++i];
        } else if (std::strncmp(arg, "--hud-layout=", 11) == 0) {
            hudLayoutPath_ = arg + 11;
        } else if (std::strncmp(arg, "--run=", 6) == 0) {
            mapPath_ = arg + 6;
        } else if (std::strcmp(arg, "--run") == 0 && i + 1 < argc) {
            mapPath_ = argv[++i];
        } else if (std::strncmp(arg, "--map=", 6) == 0) {
            mapPath_ = arg + 6;
        } else if (std::strcmp(arg, "--map") == 0 && i + 1 < argc) {
            mapPath_ = argv[++i];
        } else if (std::strcmp(arg, "--debug-select-first-unit") == 0) {
            debugSelectFirstUnitForHud_ = true;
        } else if (std::strncmp(arg, "--ai=", 5) == 0) {
            // Single-player setup: force slots to COMPUTER AI (ref
            // MeleeStartingAI/PickMeleeAI; e.g. --ai=1,2).
            std::vector<std::string> parts;
            AppendCommaSeparatedArgs(parts, arg + 5);
            for (const auto& p : parts) {
                const int slot = std::atoi(p.c_str());
                if (slot >= 0 && slot < 24) config_.aiPlayerSlots.push_back(slot);
            }
        } else if (std::strncmp(arg, "--closed=", 9) == 0) {
            // Force slots closed (NONE controller + inactive; e.g. --closed=3).
            std::vector<std::string> parts;
            AppendCommaSeparatedArgs(parts, arg + 9);
            for (const auto& p : parts) {
                const int slot = std::atoi(p.c_str());
                if (slot >= 0 && slot < 24) config_.closedPlayerSlots.push_back(slot);
            }
        } else if (std::strncmp(arg, "--local-player=", 15) == 0) {
            config_.localPlayerIndex = std::max(0, std::atoi(arg + 15));
        } else if (arg[0] != '-') {
            mapPath_ = arg;
        }
    }
}

bool GameController::Initialize() {
    std::cout << "Warcraft III C++ Recreation - Initializing..." << std::endl;
    {
        std::ofstream(MoveDebugLogPath(), std::ios::trunc).close();
        MoveDebugLogLine("[MoveDebug] session_begin");
    }

    if (!Renderer::Initialize()) return false;

    fs::path project_root = PROJECT_SOURCE_DIR;
    if (uiOnlyMode_) {
        fs::path iniPath = project_root / "resources" / "warsmash.ini";
        config_.Load(iniPath.string());
        std::cout << "UI-only mode: skipping map loading and scene simulation" << std::endl;
        if (debugUiGlueMenu_) {
            OpenDataSources();
        }
        if (!ui_.Init(debugUiGlueMenu_ ? &mpq_ : nullptr, 1280, 720, hudLayoutPath_, config_.gameVersion)) {
            std::cerr << "UI-only mode: MainUI init failed" << std::endl;
            return false;
        }
        ui_.SetGlueMapLaunchCallback([this](const std::string& path, int localPlayer) {
            HandleGlueMapLaunchRequest(path, localPlayer);
        });
        if (debugUiGlueMenu_) {
            std::vector<War3UI::GlueCampaignListItem> campaigns = LoadUiOnlyGlueCampaigns(mpq_, config_.gameVersion);
            ui_.SetGlueMenuCampaignsForUiOnly(campaigns);
            std::vector<War3UI::GlueMapListItem> maps = DiscoverUiOnlyGlueMaps(project_root);
            ui_.SetGlueMenuMapsForUiOnly(maps);
            std::cout << "UI-only mode: discovered " << maps.size()
                      << " custom game maps for glue menu" << std::endl;
        }
        if (debugUiCommandCard_) {
            ui_.PopulateDebugCommandCardForUiOnly();
        }
        if (debugUiResources_) {
            ui_.SetResourcesForUiOnly(debugUiGold_, debugUiLumber_, debugUiFoodUsed_, debugUiFoodCap_, debugUiTimeText_);
        }
        if (debugUiGlueMenu_) {
            ui_.OpenGlueMenuForUiOnly(debugUiGlueMenuPage_);
            for (const std::string& action : debugUiGlueMenuActions_) {
                if (!ui_.InvokeGlueMenuActionForUiOnly(action)) {
                    std::cerr << "UI-only mode: failed to invoke glue menu action '" << action << "'" << std::endl;
                    return false;
                }
            }
        }
        if (!debugUiUpperButton_.empty()) {
            if (!ui_.InvokeUpperButtonForUiOnly(debugUiUpperButton_)) {
                std::cerr << "UI-only mode: failed to invoke upper button '" << debugUiUpperButton_ << "'" << std::endl;
            }
        }
        if (debugUiGameMenu_) {
            if (!debugUiGameMenuPanel_.empty()) {
                ui_.OpenGameMenuPanelForUiOnly(debugUiGameMenuPanel_);
            } else {
                ui_.OpenGameMenuForUiOnly();
            }
            for (const std::string& action : debugUiGameMenuActions_) {
                if (!ui_.InvokeGameMenuActionForUiOnly(action)) {
                    std::cerr << "UI-only mode: failed to invoke game menu action '" << action << "'" << std::endl;
                    return false;
                }
            }
        }
        if (pendingGlueMapLaunch_) {
            if (!StartPendingGlueMapLaunch(1280, 720)) {
                return false;
            }
            initialized_ = true;
            return true;
        }
        state_ = GameState::IN_GAME;
        pendingState_ = GameState::IN_GAME;
        stateTransition_ = false;
        initialized_ = true;
        return true;
    }

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
        std::string resolvedMapPath;
        if (ResolveMapPathForLoad(resourcesDir, mpq_, mapPath_, resolvedMapPath)) {
            mapPath_ = resolvedMapPath;
        }
    }

    std::cout << "\nMap: " << fs::path(mapPath_).filename() << std::endl;

    if (!mapPath_.empty() && fs::exists(mapPath_)) {
        mpq_.OpenOnTop(mapPath_);
    }

    if (debugHudGameMode_) {
        std::cout << "HUD game debug: loading game state from default map and rendering HUD only" << std::endl;
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

    if (!hideHud_) {
        if (!ui_.Init(&mpq_, 1280, 720, hudLayoutPath_, config_.gameVersion)) {
            std::cerr << "GameController: MainUI init failed" << std::endl;
            return false;
        }
        ui_.SetGlueMapLaunchCallback([this](const std::string& path, int localPlayer) {
            HandleGlueMapLaunchRequest(path, localPlayer);
        });
        // Headless/smoke support: open the game menu like the ui-only debug
        // path does, so textured menu chrome can be verified in full mode.
        if (debugUiGameMenu_) {
            if (!debugUiGameMenuPanel_.empty()) {
                ui_.OpenGameMenuPanelForUiOnly(debugUiGameMenuPanel_);
            } else {
                ui_.OpenGameMenuForUiOnly();
            }
        }
        // Upper-button dialogs (quests/allies/chat) for in-game verification,
        // mirroring the ui-only path (ref --debug-ui-upper-button).
        if (!debugUiUpperButton_.empty()) {
            if (!ui_.InvokeUpperButtonForUiOnly(debugUiUpperButton_)) {
                std::cerr << "GameController: failed to invoke upper button '"
                          << debugUiUpperButton_ << "'" << std::endl;
            }
        }
    } else {
        std::cout << "GameController: HUD init skipped (--hide-hud)" << std::endl;
    }
    initialized_ = true;
    return true;
}


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


void GameController::Update(int screenW, int screenH, float dt) {
    profiler_.BeginFrame(static_cast<uint64_t>(frameCount_ + 1));
    WARSMASH_PROFILE_SCOPE(profiler_, "GameController::Update", "Core");
    if (uiOnlyMode_) {
        if (pendingGlueMapLaunch_ && StartPendingGlueMapLaunch(screenW, screenH)) {
            // Continue into the normal state machine in this frame.
        } else {
            frameCount_++;
            return;
        }
    }

    if (uiOnlyMode_) {
        frameCount_++;
        return;
    }

    // Campaign ChangeLevel: JASS queued a map switch (ref ChangeLevel).
    // Resolve like a glue launch and reuse the runtime reload path so
    // campaign missions actually chain instead of stalling on victory.
    if (jassNatives_ && simulationInitialized_) {
        auto jd = jassNatives_->GetData();
        if (jd && !jd->pendingChangeLevel.empty()) {
            const std::string next = jd->pendingChangeLevel;
            jd->pendingChangeLevel.clear();
            fs::path projectRoot = PROJECT_SOURCE_DIR;
            fs::path resourcesDir = projectRoot / "resources";
            std::string resolved;
            if (ResolveMapPathForLoad(resourcesDir, mpq_, next, resolved) && !resolved.empty()) {
                pendingGlueMapLaunch_ = true;
                pendingGlueMapLaunchRequestedPath_ = next;
                pendingGlueMapLaunchResolvedPath_ = resolved;
                pendingGlueMapLaunchLocalPlayer_ = config_.localPlayerIndex;
                MoveDebugLogLine("ChangeLevel: queued campaign map switch to '" + resolved + "'");
                // Snapshot human heroes BEFORE the reload tears the sim down;
                // SpawnCarriedHeroes consumes them once the next map is up.
                CollectCarriedHeroes();
                StartPendingGlueMapLaunch(screenW, screenH);
                frameCount_++;
                return;
            } else {
                MoveDebugLogLine("ChangeLevel: could not resolve next map '" + next + "'");
            }
        }
    }

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
        if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_ && !terrainLoaded_) {
            loadingFailed_ = true;
            loadingFailureReason_ = "Terrain was not built";
            std::cerr << "UpdateLoading: " << loadingFailureReason_ << std::endl;
            return;
        }
        if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_) {
            scene_.PrintLoadStats();
        }
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

    if (jassNatives_ && jassNatives_->GetData()) {
        const auto& noise = jassNatives_->GetData()->sourceNoise;
        camera_.SetNoise(noise.magnitude, noise.velocity, noise.vertOnly, noise.horizOnly);
        auto& rot = jassNatives_->GetData()->rotateMode;
        if (rot.active) {
            if (!camera_.StepRotateAround(rot.x, rot.y, rot.radians, rot.duration, dt)) {
                rot.active = false;
            }
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
    UpdateMidiAmbient(dt);
    SyncMusic();
    if (jassNatives_ && jassNatives_->GetData() && texturedTerrain_.IsValid()) {
        const auto& fog = jassNatives_->GetData()->globalUnitFog;
        if (fog.e > fog.d && fog.d >= 0.0f) {
            float color[3] = {fog.a, fog.b, fog.c};
            if (color[0] > 1.0f || color[1] > 1.0f || color[2] > 1.0f) {
                color[0] /= 255.0f; color[1] /= 255.0f; color[2] /= 255.0f;
            }
            texturedTerrain_.SetFog(color, fog.d, fog.e);
        }
    }
    if (ui_.GetResourceBar()) ui_.GetResourceBar()->SetTimeOfDay(dayNight_.GetTimeOfDay());

    for (auto it = activeEventSplats_.begin(); it != activeEventSplats_.end(); ) {
        it->lifeRemaining -= dt;
        if (it->lifeRemaining <= 0.0f) {
            it = activeEventSplats_.erase(it);
        } else {
            ++it;
        }
    }

    // Simulation step
    StepSimulation(dt);

    // Hovered unit raycast
    if (simulationInitialized_ && !sapp_mouse_locked() && !selectionDragVisible_) {
        const float bottomConsoleY = (float)screenH * 0.73f;
        if (mousePosY_ > 40.0f && mousePosY_ < bottomConsoleY && mousePosX_ >= 0.0f && mousePosX_ <= (float)screenW) {
            float aspect = (screenH > 0) ? (float)screenW / screenH : 1.0f;
            Mat4 view = camera_.GetViewMatrix();
            Mat4 proj = camera_.GetProjectionMatrix(aspect);
            CUnit* hovered = PickSimulationUnitAtScreenPoint(mousePosX_, mousePosY_, view, proj, screenW, screenH, -1, false, false);
            if (hovered && !hovered->IsDead() && !hovered->IsHidden()) {
                hoveredUnitHandle_ = hovered->GetHandleId();
            } else {
                hoveredUnitHandle_ = -1;
            }
        } else {
            hoveredUnitHandle_ = -1;
        }
    } else {
        hoveredUnitHandle_ = -1;
    }

    // After sim step, sync visuals
    if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_) {
        SyncAndReconcileVisuals();
        RefreshRuntimeUnitSplats();
    }
    RefreshSelectionUi(false);
}

void GameController::StepSimulation(float dt) {
    if (!simulationInitialized_) return;

    const float simStep = CSimulation::SIM_STEP_TIME;
    simulationAccumulator_ = std::min(simulationAccumulator_ + dt, simStep * 3.0f);
    bool ranSimulationStep = false;
    while (simulationAccumulator_ >= simStep) {
        {
            WARSMASH_PROFILE_SCOPE(profiler_, "SimulationStep", "ECS");
            simulation_.Update();
        }
        if (jassNatives_) {
            WARSMASH_PROFILE_SCOPE(profiler_, "JassNativesStep", "Scripting");
            jassNatives_->UpdateTimers(simStep);
            jassNatives_->UpdateRegions();
            jassNatives_->UpdateRescue();
            jassNatives_->UpdateAI();
            jassNatives_->PumpQueuedThreads(1024);
        }
        simulationAccumulator_ -= simStep;
        ranSimulationStep = true;
    }

    if (ranSimulationStep) {
        presentation_.CaptureSimulationTick(simulation_);
    }
    presentation_.SetInterpolationAlpha(
        simStep > 0.0f ? std::clamp(simulationAccumulator_ / simStep, 0.0f, 1.0f) : 1.0f);

    if (ranSimulationStep) {
        if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_) {
            SyncAndReconcileVisuals();
        }
        SyncFogOfWar();
        SyncLightnings();
        SyncScriptDecor();
        RefreshSelectionUi(false);
        MaybeLogAIStatus();
    }
}


void GameController::Render(int screenW, int screenH) {
    if (!initialized_) return;
    if (uiOnlyMode_) {
        RenderUiOnly(screenW, screenH);
        return;
    }

    if (debugHudGameMode_) {
        ui_.Update(screenW, screenH);

        const float hudDebugClearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        Renderer::SetClearColor(hudDebugClearColor);
        Renderer::BeginFrame();
        sg_reset_state_cache();
        sg_apply_viewport(0, 0, screenW, screenH, true);
        sg_apply_scissor_rect(0, 0, screenW, screenH, true);
        ui_.RenderHudOnly();
        if (screenshotFrame >= 0) {
            screenshotFrame = std::min(screenshotFrame, frameCount_);
            CaptureScreenshotIfNeeded(screenW, screenH);
        }
        Renderer::EndFrame();
        Platform::ProcessInput();
        if (screenshotFrame < 0 && exitAfterFrame_ >= 0) {
            sapp_request_quit();
            exitAfterFrame_ = -1;
            return;
        }
        MaybeExitAfterFrame();
        return;
    }

    if (state_ != GameState::IN_GAME) {
        if (screenshotFrame >= 0 || exitAfterFrame_ >= 0) {
            ui_.Update(screenW, screenH);

            Renderer::BeginFrame();
            sg_reset_state_cache();
            sg_apply_viewport(0, 0, screenW, screenH, true);
            sg_apply_scissor_rect(0, 0, screenW, screenH, true);
            ui_.RenderHudOnly();
            CaptureScreenshotIfNeeded(screenW, screenH);
            Renderer::EndFrame();
            MaybeExitAfterFrame();
        }
        return;
    }

    float dt = 0.016f;
    float aspect = (screenH > 0) ? (float)screenW / screenH : 1.0f;
    Mat4 view = camera_.GetViewMatrix();
    Mat4 proj = camera_.GetProjectionMatrix(aspect);

    if (!hideHud_) {
        ui_.Update(screenW, screenH);
    }

    Renderer::BeginFrame();
    Renderer::SetWireframe(wireframe);

    if (texturedTerrain_.IsValid()) {
        texturedTerrain_.SetWaterDeforms(simulationInitialized_ && simulation_.GetWaterDeforms());
        texturedTerrain_.Render(view, proj, frameCount_ * 0.016f);
    } else if (simpleTerrain_.IsValid())
        simpleTerrain_.Render(view, proj);

    sg_reset_state_cache();
    sg_apply_viewport(0, 0, screenW, screenH, true);
    sg_apply_scissor_rect(0, 0, screenW, screenH, true);
    if (!terrainOnly_) {
            WARSMASH_PROFILE_SCOPE(profiler_, "SceneRenderAll", "Render");
            if (texturedTerrain_.IsValid()) {
                scene_.SetBoundTerrainChunkGrid(&texturedTerrain_.GetTerrainChunkGrid());
            }
            scene_.SetDebugRenderRange(debugSceneRenderStart_, debugSceneRenderCount_);
            scene_.SetDebugRenderKind(debugSceneRenderKind_);
            SyncWeather(dt);
            scene_.RenderAll(view, proj, dt);
            for (const SceneEventEffect& eventEffect : scene_.GetFrameEventEffects()) {
                if (audio_.IsValid() && !eventEffect.effect.soundPaths.empty()) {
                    audio_.Play3DPath(eventEffect.effect.soundPaths.front(),
                                      eventEffect.x, eventEffect.y, eventEffect.z,
                                      eventEffect.effect.volume);
                }
                if (!eventEffect.effect.texturePath.empty()) {
                    ActiveEventSplat splat;
                    splat.key = "mdx_event_" + std::to_string(nextEventSplatId_++);
                    splat.texturePath = eventEffect.effect.texturePath;
                    splat.x = eventEffect.x;
                    splat.y = eventEffect.z;
                    const float scale = eventEffect.effect.scale > 0.0f ? eventEffect.effect.scale : 1.0f;
                    splat.halfSize = std::max(16.0f, scale * 64.0f);
                    if (eventEffect.hasWorldTransform) {
                        const Vec3 splatScaleCompensation = MdxEventSplatScaleCompensationForType(
                            eventEffect.effect.type, eventEffect.effect.localScale);
                        MdxEventSplatMesh mesh = BuildMdxEventSplatMeshFromTransform(
                            eventEffect.worldTransform,
                            splat.halfSize,
                            8.0f,
                            splatScaleCompensation);
                        splat.meshVertices = std::move(mesh.vertices);
                        splat.meshIndices = std::move(mesh.indices);
                    }
                    splat.lifeRemaining = eventEffect.effect.lifeSpan > 0.0f
                        ? eventEffect.effect.lifeSpan
                        : 0.5f;
                    splat.lifeSpan = splat.lifeRemaining;
                    splat.colors = eventEffect.effect.colors;
                    splat.intervalTimes = eventEffect.effect.intervalTimes;
                    splat.uvIntervals = eventEffect.effect.uvIntervals;
                    splat.intervalCount = eventEffect.effect.intervalCount;
                    splat.columns = eventEffect.effect.columns;
                    splat.rows = eventEffect.effect.rows;
                    splat.blendMode = eventEffect.effect.blendMode;
                    splat.unshaded = eventEffect.effect.unshaded;
                    splat.highPriority = true;
                    activeEventSplats_.push_back(std::move(splat));
                }
                if (!eventEffect.effect.modelPath.empty()) {
                    const float spawnPos[3] = {eventEffect.x, eventEffect.y, eventEffect.z};
                    float spawnScale = eventEffect.effect.scale > 0.0f
                        ? eventEffect.effect.scale
                        : 1.0f;
                    if (!eventEffect.hasWorldTransform && eventEffect.effect.hasLocalTransform) {
                        const Vec3& localScale = eventEffect.effect.localScale;
                        const float nodeScale = (std::fabs(localScale.x)
                            + std::fabs(localScale.y)
                            + std::fabs(localScale.z)) / 3.0f;
                        if (std::isfinite(nodeScale) && nodeScale > 0.0f) {
                            spawnScale *= nodeScale;
                        }
                    }
                    const float fallbackLife = eventEffect.effect.lifeSpan > 0.0f
                        ? eventEffect.effect.lifeSpan
                        : 1.0f;
                    if (eventEffect.hasWorldTransform) {
                        Mat4 spawnTransform = eventEffect.worldTransform;
                        if (spawnScale > 0.0f && std::isfinite(spawnScale)
                            && std::fabs(spawnScale - 1.0f) > 0.0001f) {
                            spawnTransform = spawnTransform
                                * Mat4::Scale(spawnScale, spawnScale, spawnScale);
                        }
                        scene_.AddTransientModelWithSceneMatrix(
                            eventEffect.effect.modelPath, spawnTransform, 0, fallbackLife);
                    } else {
                        scene_.AddTransientModelAtScenePosition(
                            eventEffect.effect.modelPath, spawnPos, 0.0f, spawnScale, 0, fallbackLife);
                    }
                }
            }
    }

    ScreenRect minimapRect = hideHud_ ? ScreenRect{} : ui_.GetMinimapRect();
    if (!hideHud_ && !minimapRect.IsValid()) {
        minimapRect = FallbackMinimapRect(screenW, screenH);
    }
    if (!hideHud_ && minimapRect.IsValid()) {
        minimap_.SetScreenRect(minimapRect.x, minimapRect.y, minimapRect.w, minimapRect.h);
    }

    if (!hideHud_) SyncHudLiveData();

    if (!hideHud_ && showMinimap && minimap_.IsValid()) {
        SyncMinimapLayer();
        sg_reset_state_cache();
        sg_apply_viewport(0, 0, screenW, screenH, true);
        sg_apply_scissor_rect(0, 0, screenW, screenH, true);
        Vec3 cp = camera_.GetPosition();
        minimap_.Render(screenW, screenH, cp.x, cp.z, camera_.GetYaw(), 0, 0, 45.0f, aspect);
        sg_reset_state_cache();
    }

    if (!hideHud_) {
        ui_.SetExternalMinimapVisible(showMinimap && minimap_.IsValid());
        CollectUnitOverlays(screenW, screenH);
        CollectTextTagOverlays(screenW, screenH);
        SyncLeaderboard();
        SyncMultiboard();
        sg_reset_state_cache();
        sg_apply_viewport(0, 0, screenW, screenH, true);
        sg_apply_scissor_rect(0, 0, screenW, screenH, true);
        ui_.Render();

        ui_.RenderHudOnly();
    }
    CaptureScreenshotIfNeeded(screenW, screenH);
    Renderer::EndFrame();
    profiler_.EndFrame();
    Platform::ProcessInput();
    MaybeExitAfterFrame();
}

void GameController::RenderUiOnly(int screenW, int screenH) {
    if (state_ != GameState::IN_GAME) return;

    ui_.Update(screenW, screenH);

    const float glueClearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    Renderer::SetClearColor(glueClearColor);
    Renderer::BeginFrame();
    sg_reset_state_cache();
    sg_apply_viewport(0, 0, screenW, screenH, true);
    sg_apply_scissor_rect(0, 0, screenW, screenH, true);

    sg_reset_state_cache();
    sg_apply_viewport(0, 0, screenW, screenH, true);
    sg_apply_scissor_rect(0, 0, screenW, screenH, true);

    ui_.SetExternalMinimapVisible(false);
    ui_.RenderHudOnly();

    CaptureScreenshotIfNeeded(screenW, screenH);
    Renderer::EndFrame();
    Platform::ProcessInput();
    MaybeExitAfterFrame();
}

bool GameController::LoadUiOnlyGlueBackgroundModel() {
    if (glueBackgroundLoaded_ || mpq_.ArchiveCount() == 0) {
        return glueBackgroundLoaded_;
    }

    std::vector<std::string> candidates;
    const std::string skinModel = ResolveUiOnlySkinField(mpq_, "GlueSpriteLayerBackground", config_.gameVersion);
    AddUiOnlyCandidatePath(candidates, skinModel);
    AddUiOnlyCandidatePath(candidates, UiOnlyMdxPathFromModelField(skinModel));
    AddUiOnlyCandidatePath(candidates, "UI\\Glues\\MainMenu\\MainMenu3D_exp\\MainMenu3D_exp.mdx");
    AddUiOnlyCandidatePath(candidates, "UI\\Glues\\MainMenu\\MainMenu3D\\MainMenu3D.mdx");

    for (const std::string& path : candidates) {
        if (path.empty()) {
            continue;
        }

        std::vector<uint8_t> data;
        if (!mpq_.ReadFile(path, data)) {
            continue;
        }

        MdxModel model;
        if (!MdxLoad(data.data(), data.size(), model)) {
            continue;
        }

        if (!glueBackgroundViewer_.Load(model, &mpq_)) {
            continue;
        }

        glueBackgroundModel_ = std::move(model);
        glueBackgroundLoaded_ = true;
        glueBackgroundPath_ = path;
        glueBackgroundViewer_.PlayStandSequence();
        glueBackgroundCameraSequenceIndex_ = UiOnlySelectStandSequenceIndex(glueBackgroundModel_);
        glueBackgroundCameraElapsedMs_ = 0.0f;
        std::cout << "UI-only mode: loaded glue background model " << glueBackgroundPath_
                  << " cameras=" << glueBackgroundModel_.cameras.size()
                  << " sequences=" << glueBackgroundModel_.sequences.size() << std::endl;
        return true;
    }

    std::cerr << "UI-only mode: failed to load glue background model" << std::endl;
    return false;
}

bool GameController::LoadUiOnlyGlueSpriteLayers() {
    if (glueSpriteLayersLoaded_ || mpq_.ArchiveCount() == 0) {
        return glueSpriteLayersLoaded_;
    }

    auto loadLayer = [&](GlueSpriteLayer& layer,
                         const char* skinField,
                         const char* fallbackMdxPath,
                         const Vec3& position,
                         const Vec3& scale,
                         bool billboardPlane,
                         const char* birthSequence,
                         const char* standSequence) -> bool {
        std::vector<std::string> candidates;
        const std::string skinModel = ResolveUiOnlySkinField(mpq_, skinField, config_.gameVersion);
        AddUiOnlyCandidatePath(candidates, skinModel);
        AddUiOnlyCandidatePath(candidates, UiOnlyMdxPathFromModelField(skinModel));
        AddUiOnlyCandidatePath(candidates, fallbackMdxPath);

        for (const std::string& path : candidates) {
            if (path.empty()) {
                continue;
            }

            std::vector<uint8_t> data;
            if (!mpq_.ReadFile(path, data)) {
                continue;
            }

            MdxModel model;
            if (!MdxLoad(data.data(), data.size(), model)) {
                continue;
            }

            if (!layer.viewer.Load(model, &mpq_)) {
                continue;
            }

            layer.model = std::move(model);
            layer.loaded = true;
            layer.position = position;
            layer.scale = scale;
            layer.billboardPlane = billboardPlane;
            layer.birthSequence = birthSequence ? birthSequence : "";
            layer.standSequence = standSequence ? standSequence : "";
            layer.waitingForStand = !layer.birthSequence.empty()
                && layer.viewer.PlaySequenceByName(layer.birthSequence.c_str(), false, 1.0f, false);
            if (!layer.waitingForStand) {
                if (!layer.standSequence.empty()) {
                    const bool playedStand = layer.viewer.PlaySequenceByName(layer.standSequence.c_str(), true);
                    if (playedStand) {
                        layer.viewer.SetCurrentSequenceFrameByRatio(0.5f);
                    }
                } else {
                    layer.viewer.PlayStandSequence();
                }
            }
            layer.path = path;
            std::cout << "UI-only mode: loaded glue sprite layer " << skinField
                      << " path=" << layer.path
                      << " sequences=" << layer.model.sequences.size()
                      << " geosets=" << layer.model.geosets.size()
                      << " pos=(" << layer.position.x << "," << layer.position.y << "," << layer.position.z << ")"
                      << " scale=(" << layer.scale.x << "," << layer.scale.y << "," << layer.scale.z << ")" << std::endl;
            return true;
        }

        std::cerr << "UI-only mode: failed to load glue sprite layer " << skinField << std::endl;
        return false;
    };

    const bool loadedLeft = loadLayer(glueSpriteTopLeft_,
        "GlueSpriteLayerTopLeft",
        "UI\\Glues\\SpriteLayers\\Expansion\\TopLeftPanel-Expansion.mdx",
        Vec3(0.0f, 0.0f, 0.0f),
        Vec3(1.0f, 1.0f, 1.0f),
        false,
        "",
        "MainMenu Stand");
    const bool loadedRight = loadLayer(glueSpriteTopRight_,
        "GlueSpriteLayerTopRight",
        "UI\\Glues\\SpriteLayers\\Expansion\\TopRightPanel-Expansion.mdx",
        Vec3(0.0f, 0.0f, 0.0f),
        Vec3(1.0f, 1.0f, 1.0f),
        false,
        "",
        "MainMenu Stand");
    const bool loadedLogo = loadLayer(glueMainMenuLogo_,
        "MainMenuLogo",
        "UI\\Glues\\MainMenu\\WarCraftIIILogo_exp\\WarCraftIIILogo_exp.mdx",
        Vec3(0.0f, 0.49f, -0.586f),
        Vec3(1.0f, 1.0f, 1.0f),
        true,
        "",
        "Stand");
    glueSpriteLayersLoaded_ = loadedLeft || loadedRight || loadedLogo;
    return glueSpriteLayersLoaded_;
}

void GameController::RenderUiOnlyGlueBackground(int screenW, int screenH) {
    if (!glueBackgroundLoaded_ || screenW <= 0 || screenH <= 0) {
        return;
    }

    Mat4 view;
    Mat4 proj;
    if (!glueBackgroundModel_.cameras.empty()) {
        const MdxCamera& camera = glueBackgroundModel_.cameras[0];
        glueBackgroundCameraElapsedMs_ += 1000.0f / 60.0f;
        uint32_t cameraFrame = 0;
        if (glueBackgroundCameraSequenceIndex_ >= 0
            && glueBackgroundCameraSequenceIndex_ < (int)glueBackgroundModel_.sequences.size()) {
            const MdxSequence& sequence = glueBackgroundModel_.sequences[(size_t)glueBackgroundCameraSequenceIndex_];
            const float durationMs = MdxSequenceDurationSeconds(glueBackgroundModel_, glueBackgroundCameraSequenceIndex_) * 1000.0f;
            if (durationMs > 0.0f) {
                glueBackgroundCameraElapsedMs_ = std::fmod(glueBackgroundCameraElapsedMs_, durationMs);
            }
            const uint32_t range = sequence.endFrame > sequence.startFrame
                ? sequence.endFrame - sequence.startFrame
                : 0;
            const uint32_t offset = range > 0
                ? std::min(range, (uint32_t)glueBackgroundCameraElapsedMs_)
                : 0;
            cameraFrame = sequence.startFrame + offset;
        }

        MdxCameraSample sampledCamera;
        MdxSampleCamera(camera, &glueBackgroundModel_, cameraFrame,
                        glueBackgroundCameraElapsedMs_, sampledCamera,
                        glueBackgroundCameraSequenceIndex_);
        view = LookAt(UiOnlyMdxPointToYUp(sampledCamera.pos),
                      UiOnlyMdxPointToYUp(sampledCamera.target),
                      Vec3(0.0f, 1.0f, 0.0f));
        const float fovDeg = sampledCamera.fov * 0.6f * 57.2957795f;
        const float nearClip = std::max(1.0f, sampledCamera.nearClip);
        const float farClip = std::max(nearClip + 1.0f, sampledCamera.farClip);
        proj = Perspective(fovDeg, (float)screenW / (float)screenH, nearClip, farClip);
    } else {
        view = LookAt(Vec3(0.0f, 900.0f, 1800.0f), Vec3(0.0f, 400.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
        proj = Perspective(60.0f, (float)screenW / (float)screenH, 8.0f, 70000.0f);
    }

    glueBackgroundViewer_.Update(1.0f / 60.0f, view);
    glueBackgroundViewer_.RenderAt(Vec3(0.0f, 0.0f, 0.0f), view, proj, 1.0f, 0, 0.0f, 1.0f, true);
}

void GameController::RenderUiOnlyGlueSpriteLayers(int screenW, int screenH) {
    if (!glueSpriteLayersLoaded_ || screenW <= 0 || screenH <= 0) {
        return;
    }

    Mat4 proj;
    const float aspect = (float)screenW / (float)screenH;
    const float uiWidth = aspect > (4.0f / 3.0f) ? 0.8f * aspect / (4.0f / 3.0f) : 0.8f;
    const float uiHeight = aspect < (4.0f / 3.0f) ? 0.6f * (4.0f / 3.0f) / aspect : 0.6f;
    const float uiX = (0.8f - uiWidth) * 0.5f;
    const float uiY = (0.6f - uiHeight) * 0.5f;
    proj.SetOrtho(uiX, uiX + uiWidth, uiY, uiY + uiHeight, -1024.0f, 1024.0f);
    Mat4 view = Mat4::Identity();

    auto renderLayer = [&](GlueSpriteLayer& layer) {
        if (!layer.loaded) {
            return;
        }
        layer.viewer.Update(1.0f / 60.0f, view);
        if (layer.waitingForStand && layer.viewer.IsCurrentSequenceFinished()) {
            layer.waitingForStand = false;
            if (!layer.standSequence.empty() && !layer.viewer.PlaySequenceByName(layer.standSequence.c_str(), true)) {
                layer.viewer.PlayStandSequence();
            }
        }
        Mat4 model = Mat4::Identity();
        if (layer.billboardPlane) {
            const float logoScale = 1.35f;
            model.m[0] = 0.0f;
            model.m[5] = logoScale;
            model.m[8] = -logoScale;
            model.m[12] = 0.585f;
            model.m[13] = 0.47f;
        } else {
            model.m[0] = 0.8f / 497.896f;
            model.m[5] = 0.0f;
            model.m[6] = 1.0f;
            model.m[9] = -1.0f;
        }
        layer.viewer.RenderWithModelMatrix(model, view, proj, 0, 1.0f, false);
    };

    renderLayer(glueSpriteTopLeft_);
    renderLayer(glueSpriteTopRight_);
    renderLayer(glueMainMenuLogo_);
}

void GameController::CaptureScreenshotIfNeeded(int screenW, int screenH) {
    if (screenshotFrame < 0 || frameCount_ < screenshotFrame) return;

    if (screenshotPath.empty()) {
        std::cerr << "Screenshot requested without a path" << std::endl;
    } else if (WriteFramebufferPng(screenshotPath, screenW, screenH)) {
        std::cout << "Screenshot saved: " << screenshotPath << std::endl;
    } else {
        std::cerr << "Screenshot failed: " << screenshotPath << std::endl;
    }
    screenshotFrame = -1;
}

bool GameController::WriteFramebufferPng(const std::string& path, int screenW, int screenH) {
    if (path.empty() || screenW <= 0 || screenH <= 0) return false;

    fs::path screenshotFile(path);
    if (screenshotFile.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(screenshotFile.parent_path(), ec);
        if (ec) {
            std::cerr << "Screenshot: failed to create directory '"
                      << screenshotFile.parent_path().string() << "': " << ec.message() << std::endl;
            return false;
        }
    }

    std::vector<unsigned char> pixels((size_t)screenW * (size_t)screenH * 4u);
    std::vector<unsigned char> flipped(pixels.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, screenW, screenH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    const size_t stride = (size_t)screenW * 4u;
    for (int y = 0; y < screenH; ++y) {
        const size_t src = (size_t)(screenH - 1 - y) * stride;
        const size_t dst = (size_t)y * stride;
        std::memcpy(flipped.data() + dst, pixels.data() + src, stride);
    }

    return stbi_write_png(screenshotFile.string().c_str(), screenW, screenH, 4, flipped.data(), (int)stride) != 0;
}

void GameController::MaybeExitAfterFrame() {
    if (exitAfterFrame_ >= 0 && frameCount_ >= exitAfterFrame_) {
        if (debugHudGameMode_ && state_ == GameState::IN_GAME && screenshotFrame >= 0) {
            return;
        }
        std::cout << "Exit-after-frame reached: " << frameCount_ << std::endl;
        sapp_request_quit();
        exitAfterFrame_ = -1;
    }
}

// --- OnEvent ---


void GameController::Shutdown() {
    ui_.Shutdown();
    Renderer::Shutdown();
    audio_.Shutdown();
    std::cout << "GameController shut down after " << frameCount_ << " frames." << std::endl;
}
