#include "core/game_controller_internal.h"

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

bool GameController::MinimapWorldFromScreen(float screenX, float screenY,
                                             float* worldX, float* worldY) const {
    if (!worldX || !worldY) return false;
    if (hideHud_ || !minimap_.IsValid()) return false;
    if (!minimap_.HitTest(screenX, screenY, sapp_width(), sapp_height())) return false;
    minimap_.ScreenToWorld(screenX, screenY, sapp_width(), sapp_height(), worldX, worldY);
    return true;
}

CUnit* GameController::PickUnitNearWorldPoint(float worldX, float worldY, float radius) const {
    CUnit* best = nullptr;
    float bestDistSq = radius * radius;
    for (auto* unit : simulation_.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
        const float dx = unit->GetX() - worldX;
        const float dy = unit->GetY() - worldY;
        const float d2 = dx * dx + dy * dy;
        if (d2 < bestDistSq) {
            bestDistSq = d2;
            best = unit;
        }
    }
    return best;
}

void GameController::OnMouseDown(const sapp_event* ev) {    if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        const int multiSelectHandle = ui_.HitMultiSelection(ev->mouse_x, ev->mouse_y);
        if (multiSelectHandle >= 0 && selectedUnitHandlesSet_.find(multiSelectHandle) != selectedUnitHandlesSet_.end()) {
            PlayUiSound("Click", 0.5f);
            SetSelectedUnits(GetSelectedSimulationUnits(), multiSelectHandle, true);
            PlaySelectResponse(simulation_.GetUnit(multiSelectHandle));
            std::cout << "MultiSelectIcon: primary handle=" << multiSelectHandle
                      << " count=" << selectedUnitHandles_.size() << std::endl;
            return;
        }
    }

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
        } else if (ui_.GetPendingCommand() >= 0 && IssuePendingCommandAtScreenPoint(ev->mouse_x, ev->mouse_y)) {
            if (auto selected = GetSelectedSimulationUnits(); !selected.empty() && selected.front()) {
                PlayUnitResponse(selected.front(), "Yes");
            }
            return;
        } else {
            if (jassNatives_ && simulationInitialized_) {
                int winW = sapp_width(), winH = sapp_height();
                float aspect = (winH > 0) ? (float)winW / winH : 1.0f;
                Mat4 view = camera_.GetViewMatrix();
                Mat4 proj = camera_.GetProjectionMatrix(aspect);
                auto terrainH = [&](float x, float z) {
                    return texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(x, z) : 0.0f;
                };
                Vec3 wPos = picking::ScreenToWorld(ev->mouse_x, ev->mouse_y, view, proj, winW, winH,
                                                   texturedTerrain_.IsValid(), texturedTerrain_.GetRadius(), terrainH);
                jassNatives_->NotifyTrackableClick(wPos.x, wPos.z);
            }
            if (simulationInitialized_ && !simulation_.GetUnits().empty()) {
                selectionPressActive_ = true;
            }
        }
    } else if (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) {
        bool issuedOrder = false;
        if (simulationInitialized_ && !selectedUnitHandles_.empty()) {
            std::vector<CUnit*> selUnits = GetSelectedSimulationUnits();
            if (!selUnits.empty()) {
                // Minimap first: right-click issues SMART to the world point
                // (retail). 3D raycasts from minimap pixels are meaningless,
                // so widget targeting resolves by proximity instead.
                float mmX = 0.0f, mmY = 0.0f;
                if (MinimapWorldFromScreen(ev->mouse_x, ev->mouse_y, &mmX, &mmY)) {
                    CUnit* nearUnit = PickUnitNearWorldPoint(mmX, mmY, 128.0f);
                    bool mmAttack = false;
                    for (auto* su : selUnits) {
                        if (!su) continue;
                        if (nearUnit && nearUnit != su) {
                            bool targeted = false;
                            if (auto* attackAbility = dynamic_cast<CAbilityAttack*>(
                                    su->GetAbility(War3ID("Aatk")))) {
                                if (attackAbility->CheckCanTarget(simulation_, su, OrderIds::SMART, nearUnit)) {
                                    su->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::SMART, nearUnit));
                                    targeted = true;
                                    mmAttack = true;
                                }
                            }
                            if (!targeted) {
                                su->IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::SMART, mmX, mmY));
                            }
                        } else {
                            su->IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::SMART, mmX, mmY));
                        }
                    }
                    if (!selUnits.empty() && selUnits.front()) {
                        PlayUnitResponse(selUnits.front(), mmAttack ? "YesAttack" : "Yes");
                    }
                    issuedOrder = true;
                }
            }
            if (!issuedOrder && !selUnits.empty()) {
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
                    issuedOrder = IssuePendingCommandAtScreenPoint(ev->mouse_x, ev->mouse_y);
                    if (issuedOrder && !selUnits.empty() && selUnits.front()) {
                        PlayUnitResponse(selUnits.front(), "Yes");
                    }
                } else {
                    auto terrainH = [&](float x, float z) {
                        return texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(x, z) : 0.0f;
                    };
                    Vec3 wPos = picking::ScreenToWorld(ev->mouse_x, ev->mouse_y, view, proj, winW, winH,
                                                       texturedTerrain_.IsValid(), texturedTerrain_.GetRadius(), terrainH);

                    bool worldAttack = false;
                    for (auto* su : selUnits) {
                        if (!su) continue;
                        int orderId = OrderIds::SMART;
                        bool canTargetWidget = false;

                        if (targetWidget) {
                            if (auto* harvestAbility = dynamic_cast<CAbilityHarvest*>(su->GetAbility(War3ID("Ahar")))) {
                                if (harvestAbility->CheckCanTarget(simulation_, su, OrderIds::HARVEST, targetWidget)) {
                                    orderId = OrderIds::HARVEST;
                                    canTargetWidget = true;
                                } else if (targetUnit && harvestAbility->CheckCanTarget(simulation_, su, OrderIds::RETURN_RESOURCES, targetWidget)) {
                                    orderId = OrderIds::RETURN_RESOURCES;
                                    canTargetWidget = true;
                                }
                            }
                            if (!canTargetWidget) {
                                if (auto* repairAbility = dynamic_cast<CAbilityRepair*>(su->GetAbility(War3ID("Arep")))) {
                                    if (repairAbility->CheckCanTarget(simulation_, su, OrderIds::REPAIR, targetWidget)) {
                                        orderId = OrderIds::REPAIR;
                                        canTargetWidget = true;
                                    }
                                }
                            }
                            if (!canTargetWidget) {
                                if (auto* attackAbility = dynamic_cast<CAbilityAttack*>(su->GetAbility(War3ID("Aatk")))) {
                                    if (attackAbility->CheckCanTarget(simulation_, su, OrderIds::SMART, targetWidget)) {
                                        orderId = OrderIds::SMART;
                                        canTargetWidget = true;
                                        if (targetUnit) worldAttack = true;
                                    }
                                }
                            }
                            if (!canTargetWidget && targetUnit) {
                                if (auto* moveAbility = dynamic_cast<CAbilityMove*>(su->GetAbility(War3ID("Amov")))) {
                                    if (moveAbility->CheckCanTarget(simulation_, su, OrderIds::SMART, targetWidget)) {
                                        orderId = OrderIds::SMART;
                                        canTargetWidget = true;
                                    }
                                }
                            }
                        }

                        if (canTargetWidget) {
                            su->IssueOrder(std::make_unique<COrderTargetWidget>(orderId, targetWidget));
                        } else {
                            su->IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::SMART, wPos.x, wPos.z));
                        }
                    }
                    if (!selUnits.empty() && selUnits.front()) {
                        PlayUnitResponse(selUnits.front(), worldAttack ? "YesAttack" : "Yes");
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

bool GameController::IssuePendingCommandAtScreenPoint(float screenX, float screenY) {
    if (!simulationInitialized_ || selectedUnitHandles_.empty()) {
        return false;
    }

    const int pendingCmd = ui_.GetPendingCommand();
    if (pendingCmd < 0) {
        return false;
    }

    std::vector<CUnit*> selUnits = GetSelectedSimulationUnits();
    if (selUnits.empty()) {
        ui_.ClearPendingCommand();
        return false;
    }

    // Minimap routing first: 3D raycasts from minimap pixels are
    // meaningless. Point orders land on the world point; widget-mode orders
    // resolve the nearest live unit (retail minimap targeting).
    float mmX = 0.0f, mmY = 0.0f;
    const bool viaMinimap = MinimapWorldFromScreen(screenX, screenY, &mmX, &mmY);
    CUnit* mmUnit = viaMinimap ? PickUnitNearWorldPoint(mmX, mmY, 128.0f) : nullptr;

    int winW = sapp_width(), winH = sapp_height();
    float aspect = (winH > 0) ? (float)winW / winH : 1.0f;
    Mat4 view = camera_.GetViewMatrix();
    Mat4 proj = camera_.GetProjectionMatrix(aspect);
    CUnit* targetUnit = viaMinimap ? mmUnit
        : PickSimulationUnitAtScreenPoint(screenX, screenY,
            view, proj, winW, winH, selectedUnitHandle_, false, true);
    CDestructable* targetDest = (targetUnit || viaMinimap) ? nullptr
        : PickSimulationDestructableAtScreenPoint(screenX, screenY, view, proj, winW, winH);
    CWidget* targetWidget = targetUnit ? static_cast<CWidget*>(targetUnit)
        : static_cast<CWidget*>(targetDest);

    auto terrainH = [&](float x, float z) {
        return texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(x, z) : 0.0f;
    };
    Vec3 wPos = viaMinimap ? Vec3(mmX, 0.0f, mmY)
        : picking::ScreenToWorld(screenX, screenY, view, proj, winW, winH,
                                 texturedTerrain_.IsValid(), texturedTerrain_.GetRadius(), terrainH);

    const auto pendingMode = ui_.GetPendingCommandMode();
    ui_.CompletePendingCommand();
    for (auto* su : selUnits) {
        if (!su) continue;
        if (pendingMode == MainUI::PendingCommandMode::POINT_ONLY) {
            su->IssueOrder(std::make_unique<COrderTargetPoint>(pendingCmd, wPos.x, wPos.z));
        } else if (targetWidget) {
            bool canTarget = false;
            for (auto* ab : su->GetAbilities()) {
                if (ab && !ab->IsDisabled() && ab->CheckCanTarget(simulation_, su, pendingCmd, targetWidget)) {
                    canTarget = true;
                    break;
                }
            }
            if (canTarget) {
                su->IssueOrder(std::make_unique<COrderTargetWidget>(pendingCmd, targetWidget));
            } else {
                su->IssueOrder(std::make_unique<COrderTargetPoint>(pendingCmd, wPos.x, wPos.z));
            }
        } else {
            su->IssueOrder(std::make_unique<COrderTargetPoint>(pendingCmd, wPos.x, wPos.z));
        }
    }

    RefreshSelectionUi(true);
    std::cout << "PendingCommand: issued order=" << pendingCmd
              << " mode=" << static_cast<int>(pendingMode)
              << " units=" << selUnits.size()
              << " target=" << (targetWidget ? "widget" : "point")
              << std::endl;
    return true;
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
                    PlayUiSound("Click", 0.5f);
                    SelectSingleUnit(bestUnit, additive);
                    PlaySelectResponse(bestUnit);
                    std::cout << "SelectedUnit: "
                              << unit_text_utils::DescribeUnitType(bestUnit->GetTypeId(), unitData_)
                              << " handle=" << bestUnit->GetHandleId()
                              << " count=" << selectedUnitHandles_.size() << std::endl;
                } else if (!additive) {
                    SetSelectedUnits({}, -1);
                    lastResponseHandle_ = -1;
                    responseCount_ = 0;
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
    mousePosX_ = ev->mouse_x;
    mousePosY_ = ev->mouse_y;
    if (!sapp_mouse_locked() && ui_.HandleEvent(ev)) {
        hoveredUnitHandle_ = -1;
        return;
    }
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
    case SAPP_KEYCODE_ESCAPE:
        // Cinematic skip (ref ESC -> EVENT_PLAYER_END_CINEMATIC): only the
        // local player's skip triggers fire, and only in cinematic mode.
        if (jassNatives_ && jassNatives_->GetData() && jassNatives_->GetData()->cinematicMode) {
            const int local = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
            jassNatives_->FireCinematicSkipEvent(local);
        }
        break;    case SAPP_KEYCODE_P:
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
    case SAPP_KEYCODE_F2: {
        const bool enabled = !dayNight_.IsEnabled();
        dayNight_.SetEnabled(enabled);
        // Keep the JASS switch in sync so the per-frame HUD sync (ref
        // EnableDawnDusk owner) does not immediately revert the toggle.
        if (jassNatives_ && jassNatives_->GetData()) {
            jassNatives_->GetData()->dawnDuskEnabled = enabled;
        }
        std::cout << "Day/night: " << (enabled ? "on" : "off") << std::endl;
        break;
    }
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
        ui_.HideHeroPanel(); ui_.ClearPendingCommand(); ui_.SetPortraitModel("");
        return;
    }

    CUnit* primary = nullptr;
    for (auto* u : sel) if (u->GetHandleId() == selectedUnitHandle_) { primary = u; break; }
    if (!primary) primary = sel.front();
    selectedUnitHandle_ = primary->GetHandleId();
    selectedUnitTypeId_ = primary->GetTypeId();
    ui_.SetSelectedUnit(selectedUnitHandle_);
    // 3D portrait binding (ref portraitModel per selected unit): resolve
    // the portrait variant for the viewport state; rendering follows later.
    ui_.SetPortraitModel(scene_.PortraitPathFor(selectedUnitTypeId_.value));
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

        bool visualHasCollisionShapes = false;
        bool visualCollisionHit = false;
        float visualCollisionT = 0.0f;
        if (auto vit = simUnitToVisualIdx.find(unit->GetHandleId()); vit != simUnitToVisualIdx.end()) {
            if (MdxViewer* v = scene_.GetViewer(vit->second)) {
                visualHasCollisionShapes = v->HasCollisionShapes();
                visualCollisionHit = scene_.IntersectInstanceRayWithCollision(vit->second, ray, visualCollisionT);
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
        if (visualCollisionHit) {
            if (visualCollisionT < bestRayT) { bestRayT = visualCollisionT; bestRay = unit; }
            if (!unit->IsBuilding() && visualCollisionT < bestRayNonBuildingT) { bestRayNonBuildingT = visualCollisionT; bestRayNonBuilding = unit; }
        }
        else if (!visualHasCollisionShapes && picking::IntersectSphere(ray, sphereCenter, sphereRadius, hitT)) {
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
    CDestructable *bestScreen = nullptr;
    float bestScreenScore = std::numeric_limits<float>::infinity();

    for (auto* d : simulation_.GetDestructables()) {
        if (!d || d->IsDead()) continue;
        float worldRadius = 56.0f, worldHeight = 140.0f;
        if (auto* dt = destructableData_.GetDestructable(d->GetTypeId())) {
            if (dt->isTree) { worldRadius = 72.0f; worldHeight = 180.0f; }
            else if (dt->isWall) { worldRadius = 64.0f; worldHeight = 96.0f; }
        }
        float gh = texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(d->GetX(), d->GetY()) : 0.0f;
        Vec3 sc(d->GetX(), gh + worldHeight * 0.5f, d->GetY());
        float cx, cy;
        if (!picking::WorldToScreen(sc, view, proj, screenW, screenH, cx, cy)) continue;
        float rpx = 0;
        auto upR = [&](const Vec3& p) { float sx, sy; if (picking::WorldToScreen(p, view, proj, screenW, screenH, sx, sy)) { float dx=sx-cx, dy=sy-cy; rpx=std::max(rpx,std::sqrt(dx*dx+dy*dy)); } };
        upR(Vec3(d->GetX()+worldRadius, gh + worldHeight*0.5f, d->GetY()));
        upR(Vec3(d->GetX(), gh + worldHeight*0.5f, d->GetY()+worldRadius));
        rpx = std::max(rpx, 20.0f);
        float dx = sx - cx, dy = sy - cy, distSq = dx*dx+dy*dy;
        if (distSq > rpx*rpx) continue;
        float score = distSq / std::max(1.0f, rpx*rpx);
        if (score < bestScreenScore) { bestScreenScore = score; bestScreen = d; }
    }
    return bestScreen;
}

// --- Shutdown ---

