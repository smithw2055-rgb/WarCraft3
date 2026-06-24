#include "MainUI.h"
#include "NoesisHudHost.h"
#include "../data/unitdata.h"
#include "../ecs/unit_type.h"
#include "../ecs/orders/order.h"
#include "../ecs/unit.h"
#include "../ecs/csimulation.h"
#include "../ecs/player.h"
#include "../ecs/abilities/ability_attack.h"
#include "../ecs/abilities/ability_build.h"
#include "../ecs/abilities/ability_harvest.h"
#include "../ecs/abilities/ability_move.h"
#include "../ecs/abilities/ability_queue.h"
#include "../ecs/abilities/ability_rally.h"
#include "../ecs/abilities/ability_repair.h"
#include "../ecs/abilities/ability_upgrade.h"
#include "third_party/sokol/sokol_app.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace {

constexpr int kCommandCols = 4;
constexpr int kCommandRows = 3;
constexpr int kCommandCount = kCommandCols * kCommandRows;

std::string TrimCopy(const std::string& value) {
    size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string ShortLabel(const std::string& value) {
    std::string trimmed = TrimCopy(value);
    if (trimmed.empty()) return {};

    size_t space = trimmed.find(' ');
    if (space != std::string::npos) {
        trimmed = trimmed.substr(0, space);
    }
    if (trimmed.size() > 10) {
        trimmed.resize(10);
    }
    return trimmed;
}

std::string NormalizeAssetPath(const std::string& value) {
    std::string path = TrimCopy(value);
    if (path.empty()) return {};

    size_t separator = path.find_first_of(",;");
    if (separator != std::string::npos) {
        path = TrimCopy(path.substr(0, separator));
    }

    std::replace(path.begin(), path.end(), '/', '\\');
    if (path.size() >= 4) {
        std::string tail = path.substr(path.size() - 4);
        std::transform(tail.begin(), tail.end(), tail.begin(), [](unsigned char c) {
            return (char)std::tolower(c);
        });
        if (tail == ".blp" || tail == ".dds" || tail == ".tga") {
            return path;
        }
    }

    path += ".blp";
    return path;
}

int SlotIndex(int row, int col) {
    return row * kCommandCols + col;
}

} // namespace

MainUI::MainUI() {}
MainUI::~MainUI() {
    Shutdown();
}

void MainUI::LoadFont() {
    bool fontOk = textRenderer_.LoadFromMpq(mpq_, nullptr, 24.0f);
    if (!fontOk) {
        std::cerr << "MainUI: no font loaded, overlay text will not render" << std::endl;
    }
}

bool MainUI::Init(MpqManager* mpq, int screenW, int screenH) {
    mpq_ = mpq;
    if (!gameUI_.Init(screenW, screenH)) {
        std::fprintf(stderr, "MainUI: GameUI init failed\n");
        return false;
    }

    LoadFont();

    if (mpq_) {
        abilityData_ = CAbilityData::LoadFromMpq(*mpq_);

        std::vector<uint8_t> buf;
        if (mpq_->ReadFile("Units\\UnitAbilities.slk", buf)) {
            unitAbilitiesSlk_.Load(buf.data(), buf.size());
            std::cout << "UnitAbilities.slk: " << unitAbilitiesSlk_.Rows()
                      << " rows, " << unitAbilitiesSlk_.Cols() << " cols" << std::endl;
        }
    }

    rootFrame_ = new SimpleFrame("Root");
    rootFrame_->SetWidth((float)screenW);
    rootFrame_->SetHeight((float)screenH);
    gameUI_.rootFrame = rootFrame_;
    gameUI_.frameRegistry["Root"] = rootFrame_;

    hudHost_ = std::make_unique<NoesisHudHost>();
    if (!hudHost_->Initialize(PROJECT_SOURCE_DIR, screenW, screenH)) {
        std::fprintf(stderr, "MainUI: Noesis HUD host init failed\n");
        return false;
    }

    chatMessages_.push_back("Warcraft III HUD online");
    chatHistoryIndex_ = 0;
    ClearCommandSlots();
    multiSelectionPanel_.Init(screenW, screenH);
    return true;
}

void MainUI::Shutdown() {
    hudHost_.reset();
    textures_.clear();
    textRenderer_.Shutdown();
    commandCard_ = nullptr;
    resourceBar_ = nullptr;
    chatConsole_ = nullptr;

    if (rootFrame_) {
        gameUI_.frameRegistry.erase("Root");
        if (gameUI_.rootFrame == rootFrame_) {
            gameUI_.rootFrame = nullptr;
        }
        delete rootFrame_;
        rootFrame_ = nullptr;
    }
}

void MainUI::ClearCommandSlots() {
    for (auto& slot : commandSlots_) {
        slot = CommandSlot{};
    }
}

void MainUI::SetSelectedUnit(int handleId) {
    if (selectedUnitHandle_ != handleId) {
        activeSubmenuOrderId_ = 0;
        ClearPendingCommand();
    }
    selectedUnitHandle_ = handleId;
    if (handleId < 0) {
        ClearCommandSlots();
    }
}

void MainUI::SetCommandSlotAt(int row, int col,
                              int orderId,
                              CommandClickMode clickMode,
                              bool enabled,
                              const std::string& label,
                              const std::shared_ptr<Texture>& icon) {
    if (row < 0 || row >= kCommandRows || col < 0 || col >= kCommandCols) {
        return;
    }
    CommandSlot& slot = commandSlots_[SlotIndex(row, col)];
    slot.label = label;
    slot.orderId = orderId;
    slot.visible = true;
    slot.enabled = enabled;
    slot.icon = icon;
    slot.clickMode = clickMode;
}

int MainUI::FindNextOpenCommandSlot() const {
    for (int i = 0; i < kCommandCount; ++i) {
        if (!commandSlots_[i].visible) {
            return i;
        }
    }
    return -1;
}

void MainUI::Update(int screenW, int screenH) {
    gameUI_.Update(screenW, screenH);
    if (rootFrame_) {
        rootFrame_->SetSize((float)screenW, (float)screenH);
    }

    if (simulation_) {
        if (auto* player = simulation_->GetPlayer(0)) {
            playerGold_ = player->GetGold();
            playerLumber_ = player->GetLumber();
            playerFoodUsed_ = player->GetFoodUsed();
            playerFoodCap_ = player->GetFoodCap();
        }
    }

    if (!pendingTooltipText_.empty() && !tooltipActive_) {
        tooltipTimer_ += 0.016f;
        if (tooltipTimer_ >= kTooltipDelay) {
            tooltipActive_ = true;
        }
    }

    if (!hudHost_) return;

    hudHost_->Resize(screenW, screenH);
    hudHost_->SetResources(playerGold_, playerLumber_, playerFoodUsed_, playerFoodCap_, "12:00");
    hudHost_->SetSelection(selectedName_, selectedLevel_, selectedHp_, selectedMaxHp_,
                           selectedMp_, selectedMaxMp_, heroVisible_,
                           heroXp_, heroMaxXp_,
                           heroStrBase_, heroStrBonus_,
                           heroAgiBase_, heroAgiBonus_,
                           heroIntBase_, heroIntBonus_);
    hudHost_->SetTooltip(pendingTooltipText_, pendingTooltipX_, pendingTooltipY_, tooltipActive_);
    hudHost_->SetChatOpen(chatOpen_);
    hudHost_->SetChatMessages(chatMessages_);

    for (int i = 0; i < kCommandCount; ++i) {
        hudHost_->SetCommandSlot(i, commandSlots_[i].label, commandSlots_[i].enabled, commandSlots_[i].visible);
    }

    uiTime_ += 0.016;
    hudHost_->Update(uiTime_);
}

void MainUI::Render() {
    gameUI_.renderer.BeginFrame(gameUI_.screenW, gameUI_.screenH);
    RenderOverlays(gameUI_.renderer);
    gameUI_.renderer.EndFrame();

    if (hudHost_) {
        hudHost_->Render();
        sg_reset_state_cache();
    }

    gameUI_.renderer.BeginFrame(gameUI_.screenW, gameUI_.screenH);
    multiSelectionPanel_.Render(gameUI_.renderer, gameUI_.screenW, gameUI_.screenH);
    RenderCommandIcons(gameUI_.renderer);
    gameUI_.renderer.EndFrame();
}

void MainUI::SetMultiSelection(const std::vector<MultiSelectUnitInfo>& units) {
    multiSelectionPanel_.SetSelection(units);
}

void MainUI::SetSelectionBox(const ScreenRect& rect) {
    selectionBoxRect_ = rect;
    selectionBoxVisible_ = rect.IsValid();
}

void MainUI::ClearSelectionBox() {
    selectionBoxVisible_ = false;
    selectionBoxRect_ = {};
}

void MainUI::RenderCommandIcons(UiRenderer& r) {
    if (!hudHost_) return;

    for (int i = 0; i < kCommandCount; ++i) {
        const CommandSlot& slot = commandSlots_[i];
        if (!slot.visible || !slot.icon || !slot.icon->IsValid()) continue;

        ScreenRect rect = hudHost_->GetCommandRect(i);
        if (!rect.IsValid()) continue;

        float inset = 7.0f;
        float drawX = rect.x + inset;
        float drawY = rect.y + inset;
        float drawW = std::max(0.0f, rect.w - inset * 2.0f);
        float drawH = std::max(0.0f, rect.h - inset * 2.0f);
        if (drawW <= 0.0f || drawH <= 0.0f) continue;

        uint32_t tint = slot.enabled ? 0xFFFFFFFF : 0x88A0A0A0;
        r.DrawTexturedQuad(drawX, drawY, drawW, drawH, slot.icon->GetImage(),
                           0.0f, 0.0f, 1.0f, 1.0f, tint);
    }
}

bool MainUI::HandleEvent(const sapp_event* ev) {
    if (!ev) return false;

    if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT && hudHost_) {
        int commandIndex = hudHost_->HitCommandButton(ev->mouse_x, ev->mouse_y);
        if (commandIndex >= 0) {
            HandleCommandButtonClick(commandIndex);
            return true;
        }
    }

    switch (ev->type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
        if (ev->key_code == SAPP_KEYCODE_ENTER && !chatOpen_) {
            chatOpen_ = true;
            chatDraft_.clear();
            chatHistoryIndex_ = (int)chatHistory_.size();
            if (hudHost_) {
                hudHost_->SetChatInput("");
                hudHost_->SetChatOpen(true);
            }
            return true;
        }

        if (chatOpen_) {
            if (ev->key_code == SAPP_KEYCODE_ENTER) {
                SubmitChatMessage();
                return true;
            }
            if (ev->key_code == SAPP_KEYCODE_ESCAPE) {
                chatOpen_ = false;
                chatDraft_.clear();
                if (hudHost_) hudHost_->SetChatOpen(false);
                return true;
            }
            if (ev->key_code == SAPP_KEYCODE_UP) {
                BrowseChatHistory(-1);
                return true;
            }
            if (ev->key_code == SAPP_KEYCODE_DOWN) {
                BrowseChatHistory(1);
                return true;
            }
        }
        break;
    default:
        break;
    }

    return hudHost_ ? hudHost_->HandleEvent(ev) : false;
}

bool MainUI::OnMouseDown(float x, float y, int btn) {
    if (btn == 0 && hudHost_) {
        int commandIndex = hudHost_->HitCommandButton(x, y);
        if (commandIndex >= 0) {
            HandleCommandButtonClick(commandIndex);
            return true;
        }
    }
    return false;
}

bool MainUI::OnMouseUp(float x, float y, int btn) {
    (void)x;
    (void)y;
    (void)btn;
    return false;
}

void MainUI::OnKeyDown(int /*key*/) {}

void MainUI::WireCommandCardCallbacks() {}

void MainUI::HandleCommandButtonClick(int index) {
    if (index < 0 || index >= kCommandCount) return;
    const CommandSlot& slot = commandSlots_[index];
    if (!slot.visible || !slot.enabled || slot.orderId < 0 || !simulation_) return;

    auto* unit = simulation_->GetUnit(selectedUnitHandle_);
    if (!unit) return;

    switch (slot.clickMode) {
    case CommandClickMode::IMMEDIATE:
        if (slot.orderId == OrderIds::STOP) {
            unit->ClearOrders();
        }
        unit->IssueOrder(std::make_unique<COrderNoTarget>(slot.orderId));
        break;
    case CommandClickMode::TARGET:
        SetPendingCommand(slot.orderId, PendingCommandMode::POINT_OR_WIDGET);
        break;
    case CommandClickMode::POINT_ONLY:
        SetPendingCommand(slot.orderId, PendingCommandMode::POINT_ONLY);
        break;
    case CommandClickMode::OPEN_MENU:
        activeSubmenuOrderId_ = slot.orderId;
        PopulateCommandCardForUnit(unit);
        break;
    case CommandClickMode::CLOSE_MENU:
        activeSubmenuOrderId_ = 0;
        ClearPendingCommand();
        PopulateCommandCardForUnit(unit);
        break;
    default:
        break;
    }
}

void MainUI::ShowTooltip(const std::string& text, float screenX, float screenY) {
    pendingTooltipText_ = text;
    pendingTooltipX_ = screenX;
    pendingTooltipY_ = screenY;
    tooltipTimer_ = 0;
    tooltipActive_ = false;
}

void MainUI::HideTooltip() {
    tooltipActive_ = false;
    tooltipTimer_ = 0;
    pendingTooltipText_.clear();
}

void MainUI::UpdateTooltip(float mouseX, float mouseY) {
    pendingTooltipX_ = mouseX;
    pendingTooltipY_ = mouseY;
}

bool MainUI::IsChatOpen() const {
    return chatOpen_;
}

void MainUI::OnCharInput(char c) {
    if (!chatOpen_ || !hudHost_) return;
    chatDraft_ = hudHost_->GetChatInput();
    chatDraft_.push_back(c);
    hudHost_->SetChatInput(chatDraft_);
}

void MainUI::SubmitChatMessage() {
    if (!hudHost_) return;

    std::string text = TrimCopy(hudHost_->GetChatInput());
    if (!text.empty()) {
        chatMessages_.push_back("Player 1: " + text);
        if (chatMessages_.size() > 8) {
            chatMessages_.erase(chatMessages_.begin());
        }
        chatHistory_.push_back(text);
    }

    chatHistoryIndex_ = (int)chatHistory_.size();
    chatDraft_.clear();
    hudHost_->SetChatInput("");
    chatOpen_ = false;
    hudHost_->SetChatOpen(false);
}

void MainUI::BrowseChatHistory(int delta) {
    if (!hudHost_ || chatHistory_.empty()) return;

    if (delta < 0) {
        chatHistoryIndex_ = std::max(0, chatHistoryIndex_ - 1);
    } else {
        chatHistoryIndex_ = std::min((int)chatHistory_.size(), chatHistoryIndex_ + 1);
    }

    if (chatHistoryIndex_ >= 0 && chatHistoryIndex_ < (int)chatHistory_.size()) {
        hudHost_->SetChatInput(chatHistory_[chatHistoryIndex_]);
    } else {
        hudHost_->SetChatInput(chatDraft_);
    }
}

void MainUI::ShowHeroPanel(const std::string& name, int level,
                           int xp, int maxXp,
                           int strBase, int strBonus,
                           int agiBase, int agiBonus,
                           int intBase, int intBonus) {
    selectedName_ = name;
    selectedLevel_ = level;
    heroVisible_ = true;
    heroXp_ = xp;
    heroMaxXp_ = maxXp;
    heroStrBase_ = strBase;
    heroStrBonus_ = strBonus;
    heroAgiBase_ = agiBase;
    heroAgiBonus_ = agiBonus;
    heroIntBase_ = intBase;
    heroIntBonus_ = intBonus;
}

void MainUI::HideHeroPanel() {
    heroVisible_ = false;
}

void MainUI::UpdateHeroPanelFromUnit(War3ID unitTypeId, int handleId) {
    (void)unitTypeId;
    (void)handleId;
}

void MainUI::RenderOverlays(UiRenderer& r) {
    if (selectionBoxVisible_) {
        const float x0 = selectionBoxRect_.x;
        const float y0 = selectionBoxRect_.y;
        const float x1 = selectionBoxRect_.x + selectionBoxRect_.w;
        const float y1 = selectionBoxRect_.y + selectionBoxRect_.h;
        r.DrawQuad(x0, y0, selectionBoxRect_.w, selectionBoxRect_.h, 0x2233FF66);
        r.DrawQuad(x0, y0, selectionBoxRect_.w, 1.0f, 0xCC66FF99);
        r.DrawQuad(x0, y1 - 1.0f, selectionBoxRect_.w, 1.0f, 0xCC66FF99);
        r.DrawQuad(x0, y0, 1.0f, selectionBoxRect_.h, 0xCC66FF99);
        r.DrawQuad(x1 - 1.0f, y0, 1.0f, selectionBoxRect_.h, 0xCC66FF99);
    }

    if (!overlayData_) return;

    for (const auto& info : *overlayData_) {
        if (!info.isAlive || !info.isSelected) continue;

        float sx = info.screenX;
        float sy = info.screenY;

        float ringSize = 8.0f;
        uint32_t selColor = 0x6600FF66;
        float fx = info.footX, fy = info.footY;
        r.DrawQuad(fx - ringSize, fy - 1, ringSize * 2, 2, selColor);
        r.DrawQuad(fx - 1, fy - ringSize, 2, ringSize * 2, selColor);
        uint32_t innerColor = 0xCC33FF88;
        float innerSize = ringSize * 0.4f;
        r.DrawQuad(fx - innerSize, fy - 1, innerSize * 2, 1, innerColor);
        r.DrawQuad(fx - 1, fy - innerSize, 1, innerSize * 2, innerColor);

        float barW = 48.0f;
        float barH = 6.0f;
        float barX = sx - barW * 0.5f;
        float barY = sy;

        r.DrawQuad(barX, barY, barW, barH, 0xCC111111);

        float hpRatio = std::clamp(info.hpRatio, 0.0f, 1.0f);
        if (hpRatio > 0.01f) {
            uint32_t hpColor = 0xFF1FD655;
            r.DrawQuad(barX + 1, barY + 1, (barW - 2) * hpRatio, barH - 2, hpColor);
        }

        float mpRatio = std::clamp(info.mpRatio, 0.0f, 1.0f);
        if (mpRatio > 0.01f) {
            float mpBarY = barY - barH - 1;
            r.DrawQuad(barX, mpBarY, barW, barH, 0xCC111111);
            r.DrawQuad(barX + 1, mpBarY + 1, (barW - 2) * mpRatio, barH - 2, 0xFF4488FF);
        }

        if (textRenderer_.IsValid() && !info.name.empty()) {
            float nameSize = 11.0f;
            float nameW = textRenderer_.MeasureText(info.name.c_str(), nameSize);
            float nameX = sx - nameW * 0.5f;
            float nameY = barY + barH + 1;
            textRenderer_.RenderText(r, nameX, nameY, info.name.c_str(), 0xFFCCCCCC, nameSize);
        }
    }
}

void MainUI::UpdateInfoPanel(War3ID /*unitTypeId*/, const std::string& name, int level,
                             float hp, float maxHp, float mp, float maxMp) {
    selectedName_ = name;
    selectedLevel_ = level;
    selectedHp_ = hp;
    selectedMaxHp_ = maxHp;
    selectedMp_ = mp;
    selectedMaxMp_ = maxMp;
}

void MainUI::ClearInfoPanel() {
    selectedName_ = "No unit selected";
    selectedLevel_ = 0;
    selectedHp_ = 0.0f;
    selectedMaxHp_ = 0.0f;
    selectedMp_ = 0.0f;
    selectedMaxMp_ = 0.0f;
    heroVisible_ = false;
}

void MainUI::PopulateCommandCardForUnitType(War3ID unitTypeId) {
    auto abilList = GetUnitAbilities(unitTypeId);

    if (abilList.empty() && unitData_) {
        auto* unitType = unitData_->GetUnitType(unitTypeId);
        if (unitType) {
            auto& lst = unitType->GetAbilityList();
            abilList.assign(lst.begin(), lst.end());
        }
    }

    char idStr[5] = {
        (char)((unitTypeId.value >> 24) & 0xFF),
        (char)((unitTypeId.value >> 16) & 0xFF),
        (char)((unitTypeId.value >> 8) & 0xFF),
        (char)(unitTypeId.value & 0xFF),
        0
    };

    std::cout << "CommandCard: " << idStr << " has " << abilList.size() << " abilities";
    for (auto& a : abilList) {
        auto* def = abilityData_.GetAbility(a);
        if (def) std::cout << " " << def->name;
    }
    std::cout << std::endl;

    PopulateCommandCardFromAbilities(abilList);
}

void MainUI::PopulateCommandCardForUnit(CUnit* unit) {
    ClearCommandSlots();
    if (!unit) {
        return;
    }

    const auto addAbilityButton = [&](const CAbility* ability,
                                      int orderId,
                                      CommandClickMode clickMode,
                                      bool enabledOverride = true) -> bool {
        if (!ability) {
            return false;
        }
        const auto* def = abilityData_.GetAbility(ability->GetAlias());
        if (!def) {
            return false;
        }

        int row = def->buttonPosY;
        int col = def->buttonPosX;
        if ((row < 0) || (row >= kCommandRows) || (col < 0) || (col >= kCommandCols)
            || commandSlots_[SlotIndex(row, col)].visible) {
            const int nextIndex = FindNextOpenCommandSlot();
            if (nextIndex < 0) {
                return false;
            }
            row = nextIndex / kCommandCols;
            col = nextIndex % kCommandCols;
        }

        std::shared_ptr<Texture> icon = LoadTextureFromMpq(def->iconArt);
        std::string label = icon ? std::string() : ShortLabel(def->name.empty() ? def->tooltip : def->name);
        SetCommandSlotAt(row, col, orderId, clickMode, !ability->IsDisabled() && enabledOverride, label, icon);
        return true;
    };

    const auto addFallbackButton = [&](int row,
                                       int col,
                                       int orderId,
                                       CommandClickMode clickMode,
                                       bool enabled,
                                       const char* label,
                                       const char* iconPath) {
        SetCommandSlotAt(row, col, orderId, clickMode, enabled, {},
                         iconPath ? LoadTextureFromMpq(iconPath) : nullptr);
        if (!commandSlots_[SlotIndex(row, col)].icon) {
            commandSlots_[SlotIndex(row, col)].label = label ? label : std::string();
        }
    };

    if (activeSubmenuOrderId_ != 0) {
        addFallbackButton(2, 3, 0, CommandClickMode::CLOSE_MENU, true, "Back",
                          "ReplaceableTextures\\CommandButtons\\BTNCancel.blp");

        for (auto* ability : unit->GetAbilities()) {
            auto* buildAbility = dynamic_cast<CAbilityBuild*>(ability);
            if (!buildAbility || buildAbility->GetBaseOrderId() != activeSubmenuOrderId_) {
                continue;
            }

            std::vector<War3ID> structures(buildAbility->GetBuildableStructures().begin(),
                                           buildAbility->GetBuildableStructures().end());
            if (structures.empty()) {
                structures = unit->GetUnitType()->GetStructuresBuilt();
            }
            std::sort(structures.begin(), structures.end(),
                      [](const War3ID& lhs, const War3ID& rhs) { return lhs.value < rhs.value; });
            structures.erase(std::unique(structures.begin(), structures.end()), structures.end());

            for (const War3ID& structureId : structures) {
                const int nextIndex = FindNextOpenCommandSlot();
                if (nextIndex < 0) {
                    break;
                }
                const int row = nextIndex / kCommandCols;
                const int col = nextIndex % kCommandCols;
                std::string label = structureId.ToString();
                if (const auto* structureType = unitData_ ? unitData_->GetUnitType(structureId) : nullptr) {
                    label = ShortLabel(structureType->GetName());
                }
                SetCommandSlotAt(row, col, static_cast<int>(structureId.value), CommandClickMode::POINT_ONLY,
                                 !ability->IsDisabled(), label, nullptr);
            }
            return;
        }

        activeSubmenuOrderId_ = 0;
        ClearCommandSlots();
    }

    if (!unit->IsBuilding() && unit->GetMoveBehavior()) {
        addFallbackButton(0, 0, OrderIds::MOVE, CommandClickMode::TARGET, true, "Move",
                          "ReplaceableTextures\\CommandButtons\\BTNMove.blp");
        addFallbackButton(0, 1, OrderIds::PATROL, CommandClickMode::TARGET, true, "Patrol",
                          "ReplaceableTextures\\CommandButtons\\BTNPatrol.blp");
        addFallbackButton(0, 2, OrderIds::ATTACK, CommandClickMode::TARGET, true, "Attack",
                          "ReplaceableTextures\\CommandButtons\\BTNAttack.blp");
        addFallbackButton(0, 3, OrderIds::HOLD_POSITION, CommandClickMode::IMMEDIATE, true, "Hold",
                          "ReplaceableTextures\\CommandButtons\\BTNHoldPosition.blp");
        addFallbackButton(2, 3, OrderIds::STOP, CommandClickMode::IMMEDIATE, true, "Stop",
                          "ReplaceableTextures\\CommandButtons\\BTNStop.blp");
    }

    for (auto* ability : unit->GetAbilities()) {
        if (!ability) {
            continue;
        }

        if (auto* buildAbility = dynamic_cast<CAbilityBuild*>(ability)) {
            addAbilityButton(buildAbility, buildAbility->GetBaseOrderId(), CommandClickMode::OPEN_MENU);
            continue;
        }
        if (auto* harvestAbility = dynamic_cast<CAbilityHarvest*>(ability)) {
            const int orderId = harvestAbility->IsCarryingResources()
                ? OrderIds::RETURN_RESOURCES
                : OrderIds::HARVEST;
            addAbilityButton(harvestAbility, orderId, CommandClickMode::TARGET);
            continue;
        }
        if (auto* repairAbility = dynamic_cast<CAbilityRepair*>(ability)) {
            addAbilityButton(repairAbility, static_cast<int>(repairAbility->GetAlias().value), CommandClickMode::TARGET);
            continue;
        }
        if (auto* rallyAbility = dynamic_cast<CAbilityRally*>(ability)) {
            addAbilityButton(rallyAbility, OrderIds::SET_RALLY, CommandClickMode::TARGET);
            continue;
        }
        if (auto* queueAbility = dynamic_cast<CAbilityQueue*>(ability)) {
            std::vector<War3ID> trained(queueAbility->GetTrainedUnits().begin(), queueAbility->GetTrainedUnits().end());
            std::sort(trained.begin(), trained.end(),
                      [](const War3ID& lhs, const War3ID& rhs) { return lhs.value < rhs.value; });
            for (const War3ID& trainedId : trained) {
                const int nextIndex = FindNextOpenCommandSlot();
                if (nextIndex < 0) {
                    break;
                }
                const int row = nextIndex / kCommandCols;
                const int col = nextIndex % kCommandCols;
                std::string label = trainedId.ToString();
                if (const auto* trainedType = unitData_ ? unitData_->GetUnitType(trainedId) : nullptr) {
                    label = ShortLabel(trainedType->GetName());
                }
                SetCommandSlotAt(row, col, static_cast<int>(trainedId.value), CommandClickMode::IMMEDIATE,
                                 !ability->IsDisabled(), label, nullptr);
            }

            std::vector<War3ID> researches(queueAbility->GetResearches().begin(), queueAbility->GetResearches().end());
            std::sort(researches.begin(), researches.end(),
                      [](const War3ID& lhs, const War3ID& rhs) { return lhs.value < rhs.value; });
            for (const War3ID& researchId : researches) {
                const int nextIndex = FindNextOpenCommandSlot();
                if (nextIndex < 0) {
                    break;
                }
                const int row = nextIndex / kCommandCols;
                const int col = nextIndex % kCommandCols;
                SetCommandSlotAt(row, col, static_cast<int>(researchId.value), CommandClickMode::IMMEDIATE,
                                 !ability->IsDisabled(), researchId.ToString(), nullptr);
            }

            if (queueAbility->GetQueueCount() > 0 && !commandSlots_[SlotIndex(2, 3)].visible) {
                addFallbackButton(2, 3, OrderIds::CANCEL_QUEUE, CommandClickMode::IMMEDIATE, true, "Cancel",
                                  "ReplaceableTextures\\CommandButtons\\BTNCancel.blp");
            }
            continue;
        }
        if (auto* upgradeAbility = dynamic_cast<CAbilityUpgrade*>(ability)) {
            std::vector<War3ID> upgrades(upgradeAbility->GetUpgradesTo().begin(), upgradeAbility->GetUpgradesTo().end());
            std::sort(upgrades.begin(), upgrades.end(),
                      [](const War3ID& lhs, const War3ID& rhs) { return lhs.value < rhs.value; });
            for (const War3ID& upgradeId : upgrades) {
                const int nextIndex = FindNextOpenCommandSlot();
                if (nextIndex < 0) {
                    break;
                }
                const int row = nextIndex / kCommandCols;
                const int col = nextIndex % kCommandCols;
                std::string label = upgradeId.ToString();
                if (const auto* upgradeType = unitData_ ? unitData_->GetUnitType(upgradeId) : nullptr) {
                    label = ShortLabel(upgradeType->GetName());
                }
                SetCommandSlotAt(row, col, static_cast<int>(upgradeId.value), CommandClickMode::IMMEDIATE,
                                 !ability->IsDisabled(), label, nullptr);
            }
            continue;
        }
    }
}

void MainUI::PopulateCommandCardFromAbilities(const std::vector<War3ID>& abilityIds) {
    ClearCommandSlots();

    struct FixedOrder {
        War3ID abilityId;
        int col;
        int row;
        const char* label;
        int orderId;
    };

    const FixedOrder fixed[] = {
        { War3ID("Amov"), 0, 0, "Move", OrderIds::MOVE },
        { War3ID("Apat"), 1, 0, "Patrol", OrderIds::PATROL },
        { War3ID("Aatk"), 2, 0, "Attack", OrderIds::ATTACK },
        { War3ID("Ahrl"), 3, 0, "Hold", OrderIds::HOLD_POSITION },
        { War3ID("Astp"), 3, 2, "Stop", OrderIds::STOP },
    };

    for (const auto& fo : fixed) {
        CommandSlot& slot = commandSlots_[SlotIndex(fo.row, fo.col)];
        slot.label.clear();
        slot.orderId = fo.orderId;
        slot.visible = true;
        slot.enabled = true;
        auto* def = abilityData_.GetAbility(fo.abilityId);
        if (def && !def->iconArt.empty()) {
            slot.icon = LoadTextureFromMpq(def->iconArt);
        }
        if (!slot.icon) {
            std::string fallbackPath = "ReplaceableTextures\\CommandButtons\\BTN";
            fallbackPath += fo.label;
            fallbackPath += ".blp";
            slot.icon = LoadTextureFromMpq(fallbackPath);
        }
    }

    for (const auto& abilId : abilityIds) {
        auto* def = abilityData_.GetAbility(abilId);
        if (!def) continue;
        if (def->buttonPosX < 0 || def->buttonPosY < 0) continue;
        if (def->buttonPosX >= kCommandCols || def->buttonPosY >= kCommandRows) continue;

        CommandSlot& slot = commandSlots_[SlotIndex(def->buttonPosY, def->buttonPosX)];
        if (!slot.label.empty()) continue;

        slot.icon = LoadTextureFromMpq(def->iconArt);
        slot.label = slot.icon ? std::string() : ShortLabel(def->name);
        slot.orderId = -1;
        slot.visible = true;
        slot.enabled = false;
    }
}

std::vector<War3ID> MainUI::GetUnitAbilities(War3ID unitTypeId) {
    std::vector<War3ID> result;
    if (unitAbilitiesSlk_.Cols() == 0) return result;

    char buf[5] = {};
    buf[0] = (char)((unitTypeId.value >> 24) & 0xFF);
    buf[1] = (char)((unitTypeId.value >> 16) & 0xFF);
    buf[2] = (char)((unitTypeId.value >> 8) & 0xFF);
    buf[3] = (char)(unitTypeId.value & 0xFF);
    std::string idStr(buf);

    int colAbilList = unitAbilitiesSlk_.FindColumn("abilList");
    if (colAbilList < 0) return result;

    int row = unitAbilitiesSlk_.FindRow(idStr, 0);
    if (row < 0) return result;

    std::string abilStr = unitAbilitiesSlk_.GetString(row, colAbilList, "");
    if (abilStr.empty()) return result;

    size_t pos = 0;
    while (pos < abilStr.size()) {
        size_t comma = abilStr.find(',', pos);
        std::string code = abilStr.substr(pos, comma - pos);
        code.erase(0, code.find_first_not_of(" \t"));
        code.erase(code.find_last_not_of(" \t") + 1);
        if (code.size() == 4) {
            result.push_back(War3ID::FromString(code));
        }
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return result;
}

ScreenRect MainUI::GetMinimapRect() const {
    return hudHost_ ? hudHost_->GetMinimapRect() : ScreenRect{};
}

ScreenRect MainUI::GetPortraitRect() const {
    return hudHost_ ? hudHost_->GetPortraitRect() : ScreenRect{};
}

std::shared_ptr<Texture> MainUI::LoadTextureFromMpq(const std::string& path) {
    if (!mpq_ || path.empty()) return nullptr;

    std::string normalized = NormalizeAssetPath(path);
    if (normalized.empty()) return nullptr;

    auto it = textures_.find(normalized);
    if (it != textures_.end()) return it->second;

    std::vector<uint8_t> buf;
    if (!mpq_->ReadFile(normalized, buf)) return nullptr;

    auto tex = std::make_shared<Texture>();
    if (!tex->LoadFromMemory(buf.data(), buf.size())) return nullptr;

    textures_[normalized] = tex;
    return tex;
}
