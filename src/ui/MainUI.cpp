#include "MainUI.h"
#include "render/rhi_device.h"
#include "FdfHudHost.h"
#include "../data/unitdata.h"
#include "../data/upgradedata.h"
#include "../ecs/unit_type.h"
#include "../ecs/orders/order.h"
#include "../ecs/unit.h"
#include "../ecs/ability.h"
#include "../ecs/csimulation.h"
#include "../ecs/player.h"
#include "../ecs/item.h"
#include "../asset/BLP.h"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "third_party/sokol/sokol_app.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace {

constexpr int kCommandCols = 4;
constexpr int kCommandRows = 3;
constexpr int kCommandCount = kCommandCols * kCommandRows;
constexpr int kHudBuffCount = 8;
constexpr const char* kDefaultCommandIconUri = "assets/ui/widgets/console/human/DefaultButton.png";

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

std::string CleanWar3TooltipText(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size();) {
        if (value[i] == '|' && i + 1 < value.size()) {
            const char code = (char)std::tolower((unsigned char)value[i + 1]);
            if (code == 'n') {
                out.push_back('\n');
                i += 2;
                continue;
            }
            if (code == 'r') {
                i += 2;
                continue;
            }
            if (code == 'c' && i + 9 < value.size()) {
                i += 10;
                continue;
            }
        }
        out.push_back(value[i++]);
    }
    return TrimCopy(out);
}

std::string BuildCommandTooltipText(const std::string& title, const std::string& detail) {
    std::string result = title;
    if (!detail.empty()) {
        if (!result.empty()) result += "\n";
        result += detail;
    }
    return result;
}

HudResourceCosts UnitTypeCosts(const CUnitType* unitType) {
    HudResourceCosts costs;
    if (unitType) {
        costs.gold = unitType->GetGoldCost();
        costs.lumber = unitType->GetLumberCost();
        costs.food = unitType->GetFoodUsed();
    }
    return costs;
}

HudResourceCosts UpgradeCosts(const CUpgradeType* upgrade) {
    HudResourceCosts costs;
    if (upgrade) {
        costs.gold = upgrade->goldCost;
        costs.lumber = upgrade->lumberCost;
    }
    return costs;
}

int UpgradeGoldCostAtLevel(const CUpgradeType* upgrade, int currentLevel) {
    if (!upgrade) return 0;
    if (currentLevel >= 0 && currentLevel < (int)upgrade->goldPerLevel.size()) {
        return upgrade->goldPerLevel[currentLevel];
    }
    return upgrade->goldCost;
}

int UpgradeLumberCostAtLevel(const CUpgradeType* upgrade, int currentLevel) {
    if (!upgrade) return 0;
    if (currentLevel >= 0 && currentLevel < (int)upgrade->lumberPerLevel.size()) {
        return upgrade->lumberPerLevel[currentLevel];
    }
    return upgrade->lumberCost;
}

HudResourceCosts UpgradeCostsAtLevel(const CUpgradeType* upgrade, int currentLevel) {
    HudResourceCosts costs;
    if (upgrade) {
        costs.gold = UpgradeGoldCostAtLevel(upgrade, currentLevel);
        costs.lumber = UpgradeLumberCostAtLevel(upgrade, currentLevel);
    }
    return costs;
}

bool PlayerCanAfford(const CPlayer* player, const HudResourceCosts& costs) {
    if (!player) return false;
    if (player->GetGold() < costs.gold) return false;
    if (player->GetLumber() < costs.lumber) return false;
    if (costs.food > 0 && player->GetFoodUsed() + costs.food > player->GetFoodCap()) return false;
    return true;
}

std::string AttackDamageText(const CUnitType* unitType) {
    if (!unitType) return {};
    const auto& attacks = unitType->GetAttacks();
    if (attacks.empty() || unitType->GetAttacksEnabled() <= 0) return {};

    const CUnitAttack& attack = attacks.front();
    int minDamage = attack.damageBase;
    int maxDamage = attack.damageBase;
    if (attack.damageDice > 0 && attack.damageSidesPerDie > 0) {
        minDamage += attack.damageDice;
        maxDamage += attack.damageDice * attack.damageSidesPerDie;
    }
    if (minDamage == maxDamage) {
        return std::to_string(minDamage);
    }
    return std::to_string(minDamage) + " - " + std::to_string(maxDamage);
}

std::string InfoAttackIconKey(const CUnitType* unitType) {
    if (!unitType) return {};
    const auto& attacks = unitType->GetAttacks();
    if (attacks.empty() || unitType->GetAttacksEnabled() <= 0) return {};

    const bool hasWeaponUpgrade = unitType->HasUpgradeClass(CUpgradeClass::ATTACK) ||
        unitType->HasUpgradeClass(CUpgradeClass::RANGED) ||
        unitType->HasUpgradeClass(CUpgradeClass::ARTILLERY);
    const char* suffix = hasWeaponUpgrade ? "" : "Neutral";
    std::string key;
    switch (attacks.front().attackType) {
        case CAttackType::PIERCE: key = "DamagePierce"; break;
        case CAttackType::SIEGE: key = "DamageSiege"; break;
        case CAttackType::MAGIC:
        case CAttackType::SPELLS: key = "DamageMagic"; break;
        case CAttackType::CHAOS: key = "DamageChaos"; break;
        case CAttackType::HERO: key = "DamageHero"; break;
        case CAttackType::NORMAL: key = "DamageNormal"; break;
        default: key = "DamageUnknown"; break;
    }
    return "InfoPanelIcon" + key + suffix;
}

std::string InfoArmorIconKey(const CUnitType* unitType) {
    if (!unitType) return "InfoPanelIconArmorSmallNeutral";
    const bool hasArmorUpgrade = unitType->HasUpgradeClass(CUpgradeClass::ARMOR);
    const char* suffix = hasArmorUpgrade ? "" : "Neutral";
    std::string key;
    switch (unitType->GetDefenseType()) {
        case CDefenseType::LIGHT: key = "ArmorSmall"; break;
        case CDefenseType::MEDIUM: key = "ArmorMedium"; break;
        case CDefenseType::HEAVY: key = "ArmorLarge"; break;
        case CDefenseType::FORTIFIED: key = "ArmorFort"; break;
        case CDefenseType::HERO: key = "ArmorHero"; break;
        case CDefenseType::DIVINE: key = "ArmorDivine"; break;
        case CDefenseType::UNARMORED: key = "ArmorNone"; break;
        default: key = "ArmorUnknown"; break;
    }
    return "InfoPanelIcon" + key + suffix;
}

HudInfoStats BuildInfoStats(const CUnitType* unitType, const CUnit* unit) {
    HudInfoStats stats;
    if (!unitType) return stats;
    stats.attackText = AttackDamageText(unitType);
    stats.armorText = std::to_string(unit ? unit->GetDefense() : unitType->GetDefense());
    stats.goldText = std::to_string(unitType->GetGoldCost());
    stats.foodText = std::to_string(unitType->GetFoodUsed());
    stats.attackIconKey = InfoAttackIconKey(unitType);
    stats.armorIconKey = InfoArmorIconKey(unitType);
    stats.visible = !stats.attackText.empty() || !stats.armorText.empty() ||
        unitType->GetGoldCost() > 0 || unitType->GetFoodUsed() > 0;
    return stats;
}

std::string War3IdToRawString(War3ID id) {
    char s[5] = {
        (char)((id.value >> 24) & 0xFF),
        (char)((id.value >> 16) & 0xFF),
        (char)((id.value >> 8) & 0xFF),
        (char)(id.value & 0xFF),
        0
    };
    return std::string(s);
}

HudBuffItem BuildBuffItem(const CBuff& buff, const CBuffDefFull* def, const std::string& iconUri) {
    HudBuffItem item;
    item.name = def && !def->tooltip.empty() ? def->tooltip : War3IdToRawString(buff.GetId());
    item.iconUri = iconUri;
    item.remaining = buff.GetRemaining();
    item.duration = buff.GetDuration();
    item.stackCount = buff.GetStackCount();
    item.positive = buff.IsPositive();
    item.visible = true;
    return item;
}

bool SameCosts(const HudResourceCosts& lhs, const HudResourceCosts& rhs) {
    return lhs.gold == rhs.gold
        && lhs.lumber == rhs.lumber
        && lhs.stone == rhs.stone
        && lhs.mana == rhs.mana
        && lhs.food == rhs.food;
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

std::string SanitizeCacheFileStem(const std::string& path) {
    std::string result;
    result.reserve(path.size());
    for (unsigned char c : path) {
        if (std::isalnum(c)) {
            result.push_back((char)std::tolower(c));
        } else {
            result.push_back('_');
        }
    }
    if (result.empty()) {
        result = "icon";
    }
    return result;
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

bool MainUI::Init(MpqManager* mpq, int screenW, int screenH,
                  const std::string& hudLayoutPath, int gameVersion) {
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

    hudHost_ = std::make_unique<FdfHudHost>();
    if (!hudHost_->Initialize(PROJECT_SOURCE_DIR, screenW, screenH, hudLayoutPath, mpq_, gameVersion)) {
        std::fprintf(stderr, "MainUI: FDF HUD host init failed\n");
        return false;
    }

    chatMessages_.push_back("Warcraft III HUD online");
    chatHistoryIndex_ = 0;
    ClearCommandSlots();
    return true;
}

bool MainUI::SwitchHudLayout(MpqManager* mpq, int screenW, int screenH,
                              const std::string& hudLayoutPath, int gameVersion) {
    mpq_ = mpq;
    if (mpq_) {
        abilityData_ = CAbilityData::LoadFromMpq(*mpq_);
        std::vector<uint8_t> buf;
        if (mpq_->ReadFile("Units\\UnitAbilities.slk", buf)) {
            unitAbilitiesSlk_.Load(buf.data(), buf.size());
        }
    }
    if (!hudHost_) {
        hudHost_ = std::make_unique<FdfHudHost>();
        return hudHost_->Initialize(PROJECT_SOURCE_DIR, screenW, screenH, hudLayoutPath, mpq_, gameVersion);
    }
    return hudHost_->ReloadLayout(hudLayoutPath, mpq_, gameVersion);
}

void MainUI::ApplyMapAbilityMods(const std::string& mapPath) {
    if (!mpq_ || mapPath.empty()) return;
    abilityData_ = CAbilityData::LoadFromMpq(*mpq_, mapPath);
}

void MainUI::Shutdown() {
    hudHost_.reset();
    textures_.clear();
    textRenderer_.Shutdown();

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

void MainUI::SetPortraitModel(const std::string& modelPath) {
    selectedPortraitModelPath_ = modelPath;
    if (hudHost_) hudHost_->SetPortraitModel(modelPath);
}

void MainUI::SetCommandSlotAt(int row, int col,
                              int orderId,
                              CommandClickMode clickMode,
                              bool enabled,
                              const std::string& label,
                              const std::shared_ptr<Texture>& icon,
                              const std::string& iconUri,
                              int numberOverlay,
                              const std::string& tooltip,
                              const HudResourceCosts& resourceCosts,
                              float cooldownRemaining,
                              float cooldownMax,
                              int autocastOrderId,
                              bool autocastActive) {
    if (row < 0 || row >= kCommandRows || col < 0 || col >= kCommandCols) {
        return;
    }
    CommandSlot& slot = commandSlots_[SlotIndex(row, col)];
    slot.label = label;
    slot.tooltip = tooltip;
    slot.orderId = orderId;
    slot.visible = true;
    slot.enabled = enabled;
    slot.icon = icon;
    slot.iconUri = iconUri;
    slot.resourceCosts = resourceCosts;
    slot.cooldownRemaining = std::max(0.0f, cooldownRemaining);
    slot.cooldownMax = std::max(0.0f, cooldownMax);
    slot.autocastOrderId = autocastOrderId;
    slot.numberOverlay = numberOverlay;
    slot.active = false;
    slot.autocastActive = autocastActive;
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

int MainUI::FindCommandSlotByOrderId(int orderId) const {
    for (int i = 0; i < kCommandCount; ++i) {
        if (commandSlots_[i].visible && commandSlots_[i].orderId == orderId) {
            return i;
        }
    }
    return -1;
}

bool MainUI::IsCommandSlotEnabled(int slotIndex) const {
    if (slotIndex < 0 || slotIndex >= kCommandCount) {
        return false;
    }
    const CommandSlot& slot = commandSlots_[slotIndex];
    return slot.visible && slot.enabled;
}

bool MainUI::IsCommandOrderEnabled(int orderId) const {
    return IsCommandSlotEnabled(FindCommandSlotByOrderId(orderId));
}

bool MainUI::InvokeCommandOrderForDiagnostics(int orderId) {
    const int slot = FindCommandSlotByOrderId(orderId);
    if (!IsCommandSlotEnabled(slot)) {
        return false;
    }
    HandleCommandButtonClick(slot);
    return true;
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
        if (selectedUnitHandle_ >= 0) {
            PopulateCommandCardForUnit(simulation_->GetUnit(selectedUnitHandle_));
        }
    }

    double frameDt = sapp_frame_duration();
    if (frameDt <= 0.0 || frameDt > 0.25) {
        frameDt = 1.0 / 60.0;
    }

    if (!pendingTooltipText_.empty() && !tooltipActive_) {
        tooltipTimer_ += (float)frameDt;
        if (tooltipTimer_ >= kTooltipDelay) {
            tooltipActive_ = true;
        }
    }

    if (!hudHost_) return;

    UpdateQueueState();
    UpdateInventoryState();
    UpdatePortraitIconState();
    UpdateBuffState();

    hudHost_->Resize(screenW, screenH);
    hudHost_->SetResources(playerGold_, playerLumber_, playerFoodUsed_, playerFoodCap_, resourceTimeText_);
    hudHost_->SetUpkeepLevel(upkeepLevel_);
    hudHost_->SetCinematicMode(cinematicMode_);
    hudHost_->SetCineFilter(cineFilterColor_, cineFilterVisible_);
    if (cinematicVisible_) {
        hudHost_->ShowCinematic(cinematicSpeaker_, cinematicDialogue_);
    } else {
        hudHost_->HideCinematic();
    }
    if (timerDialogVisible_) {
        hudHost_->ShowTimerDialog(timerDialogTitle_, timerDialogValue_);
    } else {
        hudHost_->HideTimerDialog();
    }
    hudHost_->SetSelection(selectedName_, selectedLevel_, selectedHp_, selectedMaxHp_,
                           selectedMp_, selectedMaxMp_, heroVisible_,
                           heroXp_, heroMaxXp_,
                           heroStrBase_, heroStrBonus_,
                           heroAgiBase_, heroAgiBonus_,
                           heroIntBase_, heroIntBonus_,
                           selectedInfoStats_);
    hudHost_->SetTooltip(pendingTooltipText_, pendingTooltipX_, pendingTooltipY_,
                         tooltipActive_, pendingTooltipCosts_);
    hudHost_->SetChatOpen(chatOpen_);
    hudHost_->SetChatMessages(chatMessages_);

    for (int i = 0; i < kCommandCount; ++i) {
        HudCommandSlot hudSlot;
        hudSlot.label = commandSlots_[i].label;
        hudSlot.tooltip = commandSlots_[i].tooltip;
        hudSlot.iconUri = commandSlots_[i].iconUri;
        hudSlot.resourceCosts = commandSlots_[i].resourceCosts;
        hudSlot.cooldownRemaining = commandSlots_[i].cooldownRemaining;
        hudSlot.cooldownMax = commandSlots_[i].cooldownMax;
        hudSlot.autocastOrderId = commandSlots_[i].autocastOrderId;
        hudSlot.numberOverlay = commandSlots_[i].numberOverlay;
        hudSlot.enabled = commandSlots_[i].enabled;
        hudSlot.visible = commandSlots_[i].visible;
        hudSlot.active = commandSlots_[i].active
            || (pendingCommandOrderId_ >= 0 && commandSlots_[i].orderId == pendingCommandOrderId_);
        hudSlot.autocastActive = commandSlots_[i].autocastActive;
        hudHost_->SetCommandSlot(i, hudSlot);
    }
    hudHost_->SetMultiSelection(multiSelectionUnits_, selectedUnitHandle_);
    hudHost_->SetQueue(queueItems_, queueProgressRatio_);
    hudHost_->SetInventory(inventoryItems_, inventoryCapacity_, inventoryAvailable_);
    hudHost_->SetPortraitIcon(selectedPortraitIconUri_);
    hudHost_->SetPortraitModel(selectedPortraitModelPath_);
    hudHost_->SetBuffs(selectedBuffs_);

    uiTime_ += frameDt;
    hudHost_->Update(uiTime_);
}

void MainUI::RenderHudOffscreen() {
    if (hudHost_) {
        hudHost_->RenderOffscreen();
        render::RhiDevice::Instance().ResetStateCache();
    }
}

void MainUI::RenderHudOnly() {
    if (!hudHost_) return;
    hudHost_->Render();
    render::RhiDevice::Instance().ResetStateCache();
}

void MainUI::Render() {
    gameUI_.renderer.BeginFrame(gameUI_.screenW, gameUI_.screenH);
    RenderOverlays(gameUI_.renderer);
    gameUI_.renderer.EndFrame();
}

void MainUI::SetMultiSelection(const std::vector<MultiSelectUnitInfo>& units) {
    multiSelectionUnits_ = units;
}

void MainUI::SetSelectionBox(const ScreenRect& rect) {
    selectionBoxRect_ = rect;
    selectionBoxVisible_ = rect.IsValid();
}

void MainUI::ClearSelectionBox() {
    selectionBoxVisible_ = false;
    selectionBoxRect_ = {};
}

void MainUI::UpdateQueueState() {
    queueItems_.clear();
    queueProgressRatio_ = 0.0f;
    if (!simulation_ || selectedUnitHandle_ < 0) return;

    CUnit* unit = simulation_->GetUnit(selectedUnitHandle_);
    if (!unit) return;
    CAbility* queueAbility = unit->GetAbility(War3ID("Aque"));
    if (!queueAbility) return;

    for (int i = 0; i < CAbility::MAX_QUEUE_SLOTS; ++i) {
        const CQueueItemType type = queueAbility->GetQueuedItemType(i);
        if (type == CQueueItemType::NONE) continue;

        const War3ID itemId = queueAbility->GetQueuedItem(i);
        std::string label = itemId.ToString();
        std::string iconPath;
        if (type == CQueueItemType::UNIT || type == CQueueItemType::HERO_REVIVE) {
            if (const auto* unitType = unitData_ ? unitData_->GetUnitType(itemId) : nullptr) {
                label = ShortLabel(unitType->GetName());
                iconPath = unitType->GetIconPath();
            }
        }
        if (type == CQueueItemType::RESEARCH) {
            label = ShortLabel(itemId.ToString());
            const auto* upgradeData = simulation_ ? simulation_->GetUpgradeData() : nullptr;
            if (const auto* upgrade = upgradeData ? upgradeData->GetUpgrade(itemId) : nullptr) {
                label = ShortLabel(upgrade->name.empty() ? itemId.ToString() : upgrade->name);
                iconPath = upgrade->iconPath;
            }
        }
        std::string iconUri;
        if (!iconPath.empty()) {
            std::shared_ptr<Texture> icon = LoadTextureFromMpq(iconPath);
            iconUri = icon ? GetIconUri(iconPath) : kDefaultCommandIconUri;
        }
        queueItems_.push_back(HudQueueItem{i, label, iconUri, true});
    }

    const float buildTime = queueAbility->GetBuildTime();
    if (buildTime > 0.0f && queueAbility->GetQueuedItemType(0) != CQueueItemType::NONE) {
        queueProgressRatio_ = std::clamp(queueAbility->GetProgress() / buildTime, 0.0f, 1.0f);
    }
}

bool MainUI::HandleQueueSlotClick(int slot) {
    if (!simulation_ || selectedUnitHandle_ < 0 || slot < 0) return false;
    CUnit* unit = simulation_->GetUnit(selectedUnitHandle_);
    if (!unit) return false;
    CAbility* queueAbility = unit->GetAbility(War3ID("Aque"));
    if (!queueAbility || queueAbility->GetQueuedItemType(slot) == CQueueItemType::NONE) return false;

    queueAbility->CancelQueueItem(*simulation_, unit, slot);
    UpdateQueueState();
    PopulateCommandCardForUnit(unit);
    std::cout << "QueueIcon: canceled slot=" << slot << " unit=" << selectedUnitHandle_ << std::endl;
    return true;
}

void MainUI::UpdateInventoryState() {
    inventoryItems_.clear();
    inventoryAvailable_ = false;
    inventoryCapacity_ = 0;
    if (!simulation_ || selectedUnitHandle_ < 0) return;

    CUnit* unit = simulation_->GetUnit(selectedUnitHandle_);
    if (!unit) return;
    CAbility* inventory = unit->GetAbility(War3ID("AInv"));
    if (!inventory) return;

    inventoryAvailable_ = true;
    inventoryCapacity_ = std::clamp(inventory->GetItemCapacity(), 0, CAbility::MAX_INVENTORY_SLOTS);
    for (int i = 0; i < CAbility::MAX_INVENTORY_SLOTS; ++i) {
        CItem* item = inventory->GetItem(i);

        HudInventoryItem hudItem;
        hudItem.slot = i;
        hudItem.visible = true;
        hudItem.enabled = i < inventoryCapacity_;
        hudItem.occupied = hudItem.enabled && item != nullptr;
        if (!hudItem.occupied) {
            inventoryItems_.push_back(hudItem);
            continue;
        }

        hudItem.charges = item->GetCharges();

        std::string iconPath = item->GetIcon();
        if (!iconPath.empty()) {
            std::shared_ptr<Texture> icon = LoadTextureFromMpq(iconPath);
            hudItem.iconUri = icon ? GetIconUri(iconPath) : kDefaultCommandIconUri;
        }

        if (hudItem.iconUri.empty()) {
            hudItem.label = ShortLabel(item->GetName().empty() ? item->GetTypeId().ToString() : item->GetName());
        }

        inventoryItems_.push_back(hudItem);
    }
}

bool MainUI::HandleInventorySlotClick(int slot) {
    if (!simulation_ || selectedUnitHandle_ < 0 || slot < 0) return false;
    CUnit* unit = simulation_->GetUnit(selectedUnitHandle_);
    if (!unit) return false;
    CAbility* inventory = unit->GetAbility(War3ID("AInv"));
    if (!inventory || !inventory->GetItem(slot)) return false;

    const bool used = inventory->UseItem(*simulation_, unit, slot);
    if (used) {
        UpdateInventoryState();
        std::cout << "InventoryIcon: used slot=" << slot << " unit=" << selectedUnitHandle_ << std::endl;
    }
    return used;
}

void MainUI::UpdatePortraitIconState() {
    selectedPortraitIconUri_.clear();
    if (!simulation_ || selectedUnitHandle_ < 0) return;

    CUnit* unit = simulation_->GetUnit(selectedUnitHandle_);
    if (!unit) return;
    const CUnitType* unitType = unit->GetUnitType();
    if (!unitType) return;

    const std::string& iconPath = unitType->GetIconPath();
    if (iconPath.empty()) return;
    std::shared_ptr<Texture> icon = LoadTextureFromMpq(iconPath);
    selectedPortraitIconUri_ = icon ? GetIconUri(iconPath) : kDefaultCommandIconUri;
}

void MainUI::UpdateBuffState() {
    selectedBuffs_.clear();
    if (!simulation_ || selectedUnitHandle_ < 0) return;

    CUnit* unit = simulation_->GetUnit(selectedUnitHandle_);
    if (!unit) return;

    const auto& buffs = unit->GetBuffs();
    selectedBuffs_.reserve(std::min(kHudBuffCount, (int)buffs.size()));
    for (const auto& buff : buffs) {
        if (!buff || buff->IsExpired()) {
            continue;
        }
        const CBuffDefFull* def = abilityData_.GetBuff(buff->GetId());
        std::string iconUri;
        if (def && !def->iconArt.empty()) {
            std::shared_ptr<Texture> icon = LoadTextureFromMpq(def->iconArt);
            iconUri = icon ? GetIconUri(def->iconArt) : kDefaultCommandIconUri;
        }
        selectedBuffs_.push_back(BuildBuffItem(*buff, def, iconUri));
        if ((int)selectedBuffs_.size() >= kHudBuffCount) {
            break;
        }
    }
}

bool MainUI::HandleEvent(const sapp_event* ev) {
    if (!ev) return false;

    altDown_ = (ev->modifiers & SAPP_MODIFIER_ALT) != 0;
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && (ev->key_code == SAPP_KEYCODE_LEFT_ALT || ev->key_code == SAPP_KEYCODE_RIGHT_ALT)) {
        altDown_ = true;
    } else if (ev->type == SAPP_EVENTTYPE_KEY_UP && (ev->key_code == SAPP_KEYCODE_LEFT_ALT || ev->key_code == SAPP_KEYCODE_RIGHT_ALT)) {
        altDown_ = false;
    }

    // Retail right-click on an autocastable command button toggles autocast
    // (ref CAutocastAbility setAutoCastOn/Off). Consumed here so the click
    // never falls through to a world SMART order.
    if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN && ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT && hudHost_) {
        int commandIndex = hudHost_->HitCommandButton(ev->mouse_x, ev->mouse_y);
        if (commandIndex >= 0 && ToggleAutocast(commandIndex)) {
            return true;
        }
    }

    if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT && hudHost_) {
        if (hudHost_->IsGlueMenuActive()) {
            int glueAction = hudHost_->HitGlueMenuButton(ev->mouse_x, ev->mouse_y);
            hudHost_->SetPressedGlueMenuButton(glueAction);
            if (glueAction >= 0) {
                hudHost_->HandleGlueMenuAction(glueAction);
            }
            return glueAction >= 0;
        }

        if (hudHost_->IsGameMenuOpen()) {
            int gameMenuAction = hudHost_->HitGameMenuButton(ev->mouse_x, ev->mouse_y);
            hudHost_->SetPressedGameMenuButton(gameMenuAction);
            if (gameMenuAction >= 0) {
                hudHost_->HandleGameMenuAction(gameMenuAction);
            }
            return true;
        }

        int upperButton = hudHost_->HitUpperButton(ev->mouse_x, ev->mouse_y);
        if (upperButton >= 0) {
            hudHost_->SetPressedUpperButton(upperButton);
            hudHost_->SetPressedCommand(-1);
            hudHost_->SetPressedMinimapButton(-1);
            // Retail upper-bar routing (ref UpperButtonBar.fdf): Quests,
            // Menu, Allies, Chat each open their FDF dialog.
            hudHost_->HandleUpperButtonAction(upperButton);
            return true;
        }

        if (hudHost_->HandleModalClick(ev->mouse_x, ev->mouse_y)) {
            return true;
        }

        int minimapButton = hudHost_->HitMinimapButton(ev->mouse_x, ev->mouse_y);
        if (minimapButton >= 0) {
            hudHost_->SetPressedMinimapButton(minimapButton);
            hudHost_->ActivateMinimapButton(minimapButton);
            hudHost_->SetPressedUpperButton(-1);
            hudHost_->SetPressedCommand(-1);
            return true;
        }

        int inventorySlot = hudHost_->HitInventorySlot(ev->mouse_x, ev->mouse_y);
        if (inventorySlot >= 0 && HandleInventorySlotClick(inventorySlot)) {
            hudHost_->SetPressedCommand(-1);
            hudHost_->SetPressedUpperButton(-1);
            hudHost_->SetPressedMinimapButton(-1);
            return true;
        }

        int queueSlot = hudHost_->HitQueueSlot(ev->mouse_x, ev->mouse_y);
        if (queueSlot >= 0 && HandleQueueSlotClick(queueSlot)) {
            hudHost_->SetPressedCommand(-1);
            hudHost_->SetPressedUpperButton(-1);
            hudHost_->SetPressedMinimapButton(-1);
            return true;
        }

        int commandIndex = hudHost_->HitCommandButton(ev->mouse_x, ev->mouse_y);
        if (commandIndex >= 0) {
            hudHost_->SetPressedCommand(commandIndex);
            hudHost_->SetPressedUpperButton(-1);
            hudHost_->SetPressedMinimapButton(-1);
            HandleCommandButtonClick(commandIndex);
            return true;
        }
        hudHost_->SetPressedCommand(-1);
        hudHost_->SetPressedUpperButton(-1);
        hudHost_->SetPressedMinimapButton(-1);
    }

    if (ev->type == SAPP_EVENTTYPE_MOUSE_UP && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT && hudHost_) {
        hudHost_->SetPressedCommand(-1);
        hudHost_->SetPressedUpperButton(-1);
        hudHost_->SetPressedMinimapButton(-1);
        hudHost_->SetPressedGameMenuButton(-1);
        hudHost_->SetPressedGlueMenuButton(-1);
    }

    if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE && hudHost_) {
        if (hudHost_->IsGlueMenuActive()) {
            hudHost_->SetHoveredGlueMenuButton(hudHost_->HitGlueMenuButton(ev->mouse_x, ev->mouse_y));
            return true;
        }

        if (hudHost_->IsGameMenuOpen()) {
            hudHost_->SetHoveredGameMenuButton(hudHost_->HitGameMenuButton(ev->mouse_x, ev->mouse_y));
            return true;
        }

        hudHost_->SetHoveredUpperButton(hudHost_->HitUpperButton(ev->mouse_x, ev->mouse_y));
        int hoveredCommand = -1;
        for (int i = 0; i < kCommandCount; ++i) {
            if (commandSlots_[i].visible && hudHost_->GetCommandRect(i).Contains(ev->mouse_x, ev->mouse_y)) {
                hoveredCommand = i;
                break;
            }
        }
        hudHost_->SetHoveredCommand(hoveredCommand);

        if (hoveredCommand >= 0 && !commandSlots_[hoveredCommand].tooltip.empty()) {
            const CommandSlot& slot = commandSlots_[hoveredCommand];
            ShowTooltip(slot.tooltip, ev->mouse_x, ev->mouse_y, slot.resourceCosts);
        } else {
            int invSlot = hudHost_->HitInventorySlot(ev->mouse_x, ev->mouse_y);
            if (invSlot >= 0 && invSlot < (int)inventoryItems_.size() && inventoryItems_[invSlot].occupied && !inventoryItems_[invSlot].label.empty()) {
                ShowTooltip(inventoryItems_[invSlot].label, ev->mouse_x, ev->mouse_y);
            } else {
                int buffSlot = hudHost_->HitBuffSlot(ev->mouse_x, ev->mouse_y);
                if (buffSlot >= 0 && buffSlot < (int)selectedBuffs_.size() && !selectedBuffs_[buffSlot].name.empty()) {
                    std::string tip = selectedBuffs_[buffSlot].name;
                    if (selectedBuffs_[buffSlot].remaining > 0.0f) {
                        char remBuf[32];
                        std::snprintf(remBuf, sizeof(remBuf), " (%.1fs)", selectedBuffs_[buffSlot].remaining);
                        tip += remBuf;
                    }
                    ShowTooltip(tip, ev->mouse_x, ev->mouse_y);
                } else if (ev->mouse_y >= (float)sapp_height() * 0.73f || ev->mouse_y <= 40.0f) {
                    if (tooltipActive_ || !pendingTooltipText_.empty()) {
                        HideTooltip();
                    }
                }
            }
        }
    }

    switch (ev->type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
        if (!chatOpen_ && (ev->key_code == SAPP_KEYCODE_ESCAPE || ev->key_code == SAPP_KEYCODE_F10)) {
            if (hudHost_) {
                // Modal dialogs close first (Quest/Chat/Alliance), then the
                // Esc menu toggles (ref retail Esc/quest dialog behavior).
                if (hudHost_->IsQuestDialogOpen() || hudHost_->IsChatDialogOpen() ||
                    hudHost_->IsAllianceDialogOpen()) {
                    hudHost_->CloseAllDialogs();
                } else {
                    hudHost_->ToggleGameMenu();
                }
            }
            return true;
        }

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
        if (hudHost_->IsGlueMenuActive()) {
            int glueAction = hudHost_->HitGlueMenuButton(x, y);
            hudHost_->SetPressedGlueMenuButton(glueAction);
            if (glueAction >= 0) {
                hudHost_->HandleGlueMenuAction(glueAction);
            }
            return glueAction >= 0;
        }

        if (hudHost_->IsGameMenuOpen()) {
            int gameMenuAction = hudHost_->HitGameMenuButton(x, y);
            hudHost_->SetPressedGameMenuButton(gameMenuAction);
            if (gameMenuAction >= 0) {
                hudHost_->HandleGameMenuAction(gameMenuAction);
            }
            return true;
        }

        int upperButton = hudHost_->HitUpperButton(x, y);
        if (upperButton >= 0) {
            hudHost_->SetPressedUpperButton(upperButton);
            hudHost_->SetPressedCommand(-1);
            hudHost_->SetPressedMinimapButton(-1);
            hudHost_->HandleUpperButtonAction(upperButton);
            return true;
        }

        if (hudHost_->HandleModalClick(x, y)) {
            return true;
        }

        int minimapButton = hudHost_->HitMinimapButton(x, y);
        if (minimapButton >= 0) {
            hudHost_->SetPressedMinimapButton(minimapButton);
            hudHost_->ActivateMinimapButton(minimapButton);
            hudHost_->SetPressedUpperButton(-1);
            hudHost_->SetPressedCommand(-1);
            return true;
        }

        int inventorySlot = hudHost_->HitInventorySlot(x, y);
        if (inventorySlot >= 0 && HandleInventorySlotClick(inventorySlot)) {
            hudHost_->SetPressedCommand(-1);
            hudHost_->SetPressedUpperButton(-1);
            hudHost_->SetPressedMinimapButton(-1);
            return true;
        }

        int queueSlot = hudHost_->HitQueueSlot(x, y);
        if (queueSlot >= 0 && HandleQueueSlotClick(queueSlot)) {
            hudHost_->SetPressedCommand(-1);
            hudHost_->SetPressedUpperButton(-1);
            hudHost_->SetPressedMinimapButton(-1);
            return true;
        }

        int commandIndex = hudHost_->HitCommandButton(x, y);
        if (commandIndex >= 0) {
            hudHost_->SetPressedCommand(commandIndex);
            hudHost_->SetPressedUpperButton(-1);
            hudHost_->SetPressedMinimapButton(-1);
            HandleCommandButtonClick(commandIndex);
            return true;
        }
        hudHost_->SetPressedCommand(-1);
        hudHost_->SetPressedUpperButton(-1);
        hudHost_->SetPressedMinimapButton(-1);
    }
    return false;
}

bool MainUI::OnMouseUp(float x, float y, int btn) {
    (void)x;
    (void)y;
    if (btn == 0 && hudHost_) {
        hudHost_->SetPressedCommand(-1);
        hudHost_->SetPressedUpperButton(-1);
        hudHost_->SetPressedMinimapButton(-1);
        hudHost_->SetPressedGameMenuButton(-1);
        hudHost_->SetPressedGlueMenuButton(-1);
    }
    return false;
}

void MainUI::OnKeyDown(int key) {
    if (hudHost_ && !chatOpen_ && (key == SAPP_KEYCODE_ESCAPE || key == SAPP_KEYCODE_F10)) {
        if (hudHost_->IsQuestDialogOpen() || hudHost_->IsChatDialogOpen() ||
            hudHost_->IsAllianceDialogOpen()) {
            hudHost_->CloseAllDialogs();
        } else {
            hudHost_->ToggleGameMenu();
        }
    }
}


bool MainUI::ToggleAutocast(int index) {
    if (index < 0 || index >= kCommandCount || !simulation_) return false;
    const CommandSlot& slot = commandSlots_[index];
    if (!slot.visible || slot.orderId < 0) return false;

    auto* unit = simulation_->GetUnit(selectedUnitHandle_);
    if (!unit) return false;

    for (auto* ability : unit->GetAbilities()) {
        CAbility* spell = (ability && ability->IsSpellAbility()) ? ability : nullptr;
        if (!spell || spell->GetBaseOrderId() != slot.orderId) continue;
        CAutocastKind kind = GetAutocastKind(spell->GetAlias());
        if (kind == CAutocastKind::NONE) kind = GetAutocastKind(spell->GetCode());
        if (kind == CAutocastKind::NONE) return false;
        spell->SetAutocastActive(!spell->IsAutocastActive());
        PopulateCommandCardForUnit(unit);
        return true;
    }
    return false;
}

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

void MainUI::ShowTooltip(const std::string& text, float screenX, float screenY, const HudResourceCosts& costs) {
    const bool changed = pendingTooltipText_ != text || !SameCosts(pendingTooltipCosts_, costs);
    pendingTooltipText_ = text;
    pendingTooltipCosts_ = costs;
    pendingTooltipX_ = screenX;
    pendingTooltipY_ = screenY;
    if (changed) {
        tooltipTimer_ = 0;
        tooltipActive_ = false;
    }
}

void MainUI::HideTooltip() {
    tooltipActive_ = false;
    tooltipTimer_ = 0;
    pendingTooltipText_.clear();
    pendingTooltipCosts_ = HudResourceCosts{};
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
        if (!info.isAlive) continue;
        if (!info.isSelected && !altDown_) continue;

        float sx = info.screenX;
        float sy = info.screenY;

        float barW = 46.0f;
        float barH = 5.0f;
        float barX = sx - barW * 0.5f;
        float barY = sy;

        r.DrawQuad(barX, barY, barW, barH, 0xCC000000);

        float hpRatio = std::clamp(info.hpRatio, 0.0f, 1.0f);
        if (hpRatio > 0.01f) {
            uint32_t hpColor;
            if (hpRatio > 0.5f) {
                float t = (hpRatio - 0.5f) * 2.0f;
                uint8_t red = (uint8_t)((1.0f - t) * 255.0f);
                hpColor = 0xFF000000 | (red << 16) | (0xFF << 8);
            } else {
                float t = hpRatio * 2.0f;
                uint8_t green = (uint8_t)(t * 255.0f);
                hpColor = 0xFF000000 | (0xFF << 16) | (green << 8);
            }
            r.DrawQuad(barX + 1, barY + 1, (barW - 2) * hpRatio, barH - 2, hpColor);
        }
    }

    // Floating combat/text-tag layer (ref TextTag update/fade): screen-space
    // text with per-tag color/alpha, centered or left-anchored.
    if (textTagData_ && textRenderer_.IsValid()) {
        for (const auto& tag : *textTagData_) {
            if (tag.text.empty()) continue;
            const float size = std::clamp(tag.fontSize, 8.0f, 32.0f);
            float x = tag.screenX;
            if (tag.centered) {
                x -= textRenderer_.MeasureText(tag.text.c_str(), size) * 0.5f;
            }
            textRenderer_.RenderText(r, x, tag.screenY, tag.text.c_str(), tag.color, size);
        }
    }

    // Leaderboard panel (ref CLeaderboard): top-right table of the
    // currently displayed board, title plus label/value rows.
    if (leaderboard_.visible && textRenderer_.IsValid() &&
        (!leaderboard_.label.empty() || !leaderboard_.items.empty())) {
        const float titleSize = 13.0f;
        const float rowSize = 12.0f;
        const float rowH = 16.0f;
        const float padX = 8.0f;
        const size_t maxRows = std::min(leaderboard_.items.size(), size_t(16));
        float panelW = 0.0f;
        if (!leaderboard_.label.empty()) {
            panelW = std::max(panelW, textRenderer_.MeasureText(leaderboard_.label.c_str(), titleSize));
        }
        for (size_t i = 0; i < maxRows; ++i) {
            const auto& item = leaderboard_.items[i];
            float w = 0.0f;
            if (item.showLabel && !item.label.empty()) {
                w += textRenderer_.MeasureText(item.label.c_str(), rowSize);
            }
            if (item.showValue) {
                const std::string val = std::to_string(item.value);
                w += textRenderer_.MeasureText(val.c_str(), rowSize) + 24.0f;
            }
            panelW = std::max(panelW, w);
        }
        panelW += padX * 2.0f;
        const float panelH = (leaderboard_.label.empty() ? 0.0f : rowH + 4.0f) +
                             (float)maxRows * rowH + 8.0f;
        const float panelX = (float)gameUI_.screenW - panelW - 8.0f;
        const float panelY = 8.0f;
        r.DrawQuad(panelX, panelY, panelW, panelH, 0xAA111122);
        float cy = panelY + 4.0f;
        if (!leaderboard_.label.empty()) {
            textRenderer_.RenderText(r, panelX + padX, cy,
                                     leaderboard_.label.c_str(), 0xFFFFCC66, titleSize);
            cy += rowH + 4.0f;
        }
        for (size_t i = 0; i < maxRows; ++i) {
            const auto& item = leaderboard_.items[i];
            float cx = panelX + padX;
            if (item.showLabel && !item.label.empty()) {
                textRenderer_.RenderText(r, cx, cy, item.label.c_str(), 0xFFFFFFFF, rowSize);
                cx += textRenderer_.MeasureText(item.label.c_str(), rowSize) + 12.0f;
            }
            if (item.showValue) {
                textRenderer_.RenderText(r, cx, cy, std::to_string(item.value).c_str(),
                                         0xFFFFE090, rowSize);
            }
            cy += rowH;
        }
    }

    // Multiboard panel (ref multiboard API): top-left text grid of the
    // currently displayed board; minimized boards show the title only.
    if (multiboard_.visible && textRenderer_.IsValid() &&
        (!multiboard_.title.empty() || multiboard_.rows > 0)) {
        const float titleSize = 13.0f;
        const float cellSize = 12.0f;
        const float rowH = 16.0f;
        const float padX = 8.0f;
        const int rows = std::min(multiboard_.rows, 12);
        const int cols = std::min(multiboard_.cols, 8);
        std::vector<float> colW((size_t)std::max(cols, 1), 0.0f);
        for (int rr = 0; rr < rows; ++rr) {
            for (int cc = 0; cc < cols; ++cc) {
                const HudMultiboardCell& cell =
                    multiboard_.cells[(size_t)rr * (size_t)multiboard_.cols + (size_t)cc];
                if (cell.value.empty() && !cell.showIcon) continue;
                float w = cell.value.empty() ? 0.0f
                    : textRenderer_.MeasureText(cell.value.c_str(), cellSize);
                if (cell.showIcon && !cell.icon.empty()) w += 16.0f;
                colW[(size_t)cc] = std::max(colW[(size_t)cc], w);
            }
        }
        float gridW = 0.0f;
        for (float w : colW) gridW += w + 12.0f;
        float titleW = multiboard_.title.empty() ? 0.0f
            : textRenderer_.MeasureText(multiboard_.title.c_str(), titleSize);
        const float panelW = std::max(gridW, titleW) + padX * 2.0f;
        const float panelH = (multiboard_.title.empty() ? 0.0f : rowH + 4.0f) +
                             (multiboard_.minimized ? 0.0f : (float)rows * rowH) + 8.0f;
        const float panelX = 8.0f;
        const float panelY = 40.0f;
        r.DrawQuad(panelX, panelY, panelW, panelH, 0xAA111122);
        float cy = panelY + 4.0f;
        if (!multiboard_.title.empty()) {
            textRenderer_.RenderText(r, panelX + padX, cy,
                                     multiboard_.title.c_str(), 0xFFFFCC66, titleSize);
            cy += rowH + 4.0f;
        }
        if (!multiboard_.minimized) {
            for (int rr = 0; rr < rows; ++rr) {
                float cx = panelX + padX;
                for (int cc = 0; cc < cols; ++cc) {
                    const HudMultiboardCell& cell =
                        multiboard_.cells[(size_t)rr * (size_t)multiboard_.cols + (size_t)cc];
                    if (cell.showIcon && !cell.icon.empty()) {
                        if (auto icon = LoadTextureFromMpq(cell.icon)) {
                            if (icon->IsValid()) {
                                r.DrawTexturedQuad(cx, cy + 1.0f, 14.0f, 14.0f,
                                                   icon->GetImage());
                                cx += 16.0f;
                            }
                        }
                    }
                    if (!cell.value.empty()) {
                        textRenderer_.RenderText(r, cx, cy, cell.value.c_str(),
                                                 0xFFFFFFFF, cellSize);
                    }
                    cx += colW[(size_t)cc] + 12.0f;
                }
                cy += rowH;
            }
        }
    }
}

void MainUI::UpdateInfoPanel(War3ID unitTypeId, const std::string& name, int level,
                             float hp, float maxHp, float mp, float maxMp) {
    selectedName_ = name;
    selectedLevel_ = level;
    selectedHp_ = hp;
    selectedMaxHp_ = maxHp;
    selectedMp_ = mp;
    selectedMaxMp_ = maxMp;
    const CUnitType* unitType = unitData_ ? unitData_->GetUnitType(unitTypeId) : nullptr;
    const CUnit* unit = (simulation_ && selectedUnitHandle_ >= 0) ? simulation_->GetUnit(selectedUnitHandle_) : nullptr;
    selectedInfoStats_ = BuildInfoStats(unitType, unit);
}

void MainUI::ClearInfoPanel() {
    selectedName_ = "";
    selectedLevel_ = 0;
    selectedHp_ = 0.0f;
    selectedMaxHp_ = 0.0f;
    selectedMp_ = 0.0f;
    selectedMaxMp_ = 0.0f;
    heroVisible_ = false;
    selectedInfoStats_ = HudInfoStats{};
    selectedBuffs_.clear();
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
        HudResourceCosts costs;
        costs.mana = def->manaCost;
        float cooldownRemaining = 0.0f;
        float cooldownMax = 0.0f;
        if (ability && ability->IsSpellAbility()) {
            const CAbility* spell = ability;
            cooldownRemaining = spell->GetCooldownRemaining();
            cooldownMax = spell->GetCooldown();
        }
        const War3ID defaultAutocast = unit->GetUnitType() ? unit->GetUnitType()->GetDefaultAutocastAbility() : War3ID{};
        const bool isAutocast = ability->IsAutocastActive() || (defaultAutocast.value != 0 && defaultAutocast == ability->GetAlias());
        const int autocastOrderId = isAutocast ? orderId : 0;
        SetCommandSlotAt(row, col, orderId, clickMode, !ability->IsDisabled() && enabledOverride,
                         label, icon, icon ? GetIconUri(def->iconArt) : kDefaultCommandIconUri, -1,
                         BuildCommandTooltipText(def->tooltip.empty() ? def->name : def->tooltip, def->uberTip),
                         costs, cooldownRemaining, cooldownMax, autocastOrderId, isAutocast);
        return true;
    };

    const auto addFallbackButton = [&](int row,
                                       int col,
                                       int orderId,
                                       CommandClickMode clickMode,
                                       bool enabled,
                                       const char* label,
                                       const char* iconPath) {
        std::shared_ptr<Texture> icon = iconPath ? LoadTextureFromMpq(iconPath) : nullptr;
        SetCommandSlotAt(row, col, orderId, clickMode, enabled, {},
                         icon, icon ? GetIconUri(iconPath)
                                    : (iconPath ? kDefaultCommandIconUri : std::string()), -1,
                         label ? label : std::string());
        if (!commandSlots_[SlotIndex(row, col)].icon) {
            commandSlots_[SlotIndex(row, col)].label = label ? label : std::string();
        }
    };

    if (activeSubmenuOrderId_ != 0) {
        addFallbackButton(2, 3, 0, CommandClickMode::CLOSE_MENU, true, "Back",
                          "ReplaceableTextures\\CommandButtons\\BTNCancel.blp");

        for (auto* ability : unit->GetAbilities()) {
            CAbility* buildAbility = (ability && ability->GetCategory() == CAbilityCategory::BUILD && !ability->IsRepairAbility()) ? ability : nullptr;
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
                std::string iconPath;
                const auto* structureType = unitData_ ? unitData_->GetUnitType(structureId) : nullptr;
                if (structureType) {
                    label = ShortLabel(structureType->GetName());
                    iconPath = structureType->GetIconPath();
                }
                const CPlayer* player = simulation_ ? simulation_->GetPlayer(unit->GetPlayerIndex()) : nullptr;
                const HudResourceCosts costs = UnitTypeCosts(structureType);
                std::shared_ptr<Texture> icon = iconPath.empty() ? nullptr : LoadTextureFromMpq(iconPath);
                SetCommandSlotAt(row, col, static_cast<int>(structureId.value), CommandClickMode::POINT_ONLY,
                                 !ability->IsDisabled() && PlayerCanAfford(player, costs), icon ? std::string() : label,
                                 icon, iconPath.empty() ? std::string()
                                                        : (icon ? GetIconUri(iconPath) : kDefaultCommandIconUri), -1,
                                 structureType ? structureType->GetName() : structureId.ToString(),
                                 costs);
            }
            return;
        }

        activeSubmenuOrderId_ = 0;
        ClearCommandSlots();
    }

    if (!unit->IsBuilding() && unit->GetMoveBehavior()) {
        addFallbackButton(0, 0, OrderIds::MOVE, CommandClickMode::TARGET, true, "Move",
                          "ReplaceableTextures\\CommandButtons\\BTNMove.blp");
        addFallbackButton(0, 1, OrderIds::STOP, CommandClickMode::IMMEDIATE, true, "Stop",
                          "ReplaceableTextures\\CommandButtons\\BTNStop.blp");
        addFallbackButton(0, 2, OrderIds::HOLD_POSITION, CommandClickMode::IMMEDIATE, true, "Hold",
                          "ReplaceableTextures\\CommandButtons\\BTNHoldPosition.blp");
        addFallbackButton(0, 3, OrderIds::ATTACK, CommandClickMode::TARGET, true, "Attack",
                          "ReplaceableTextures\\CommandButtons\\BTNAttack.blp");
        addFallbackButton(1, 0, OrderIds::PATROL, CommandClickMode::TARGET, true, "Patrol",
                          "ReplaceableTextures\\CommandButtons\\BTNPatrol.blp");
    }

    for (auto* ability : unit->GetAbilities()) {
        if (!ability) {
            continue;
        }

        if (ability->GetCategory() == CAbilityCategory::BUILD && !ability->IsRepairAbility()) {
            CAbility* buildAbility = ability;
            addAbilityButton(buildAbility, buildAbility->GetBaseOrderId(), CommandClickMode::OPEN_MENU);
            continue;
        }
        if (ability->GetCategory() == CAbilityCategory::HARVEST && ability->GetCode() == War3ID("Ahar")) {
            CAbility* harvestAbility = ability;
            const int orderId = harvestAbility->IsCarryingResources()
                ? OrderIds::RETURN_RESOURCES
                : OrderIds::HARVEST;
            addAbilityButton(harvestAbility, orderId, CommandClickMode::TARGET);
            continue;
        }
        if (ability->IsRepairAbility()) {
            CAbility* repairAbility = ability;
            addAbilityButton(repairAbility, static_cast<int>(repairAbility->GetAlias().value), CommandClickMode::TARGET);
            continue;
        }
        if (ability->GetCategory() == CAbilityCategory::RALLY) {
            CAbility* rallyAbility = ability;
            const auto* def = abilityData_.GetAbility(rallyAbility->GetAlias());
            int rCol = (def && def->buttonPosX >= 0 && def->buttonPosX < kCommandCols) ? def->buttonPosX : 3;
            int rRow = (def && def->buttonPosY >= 0 && def->buttonPosY < kCommandRows) ? def->buttonPosY : 0;
            std::string rArt = (def && !def->iconArt.empty()) ? def->iconArt : "ReplaceableTextures\\CommandButtons\\BTNRallyPoint.blp";
            std::shared_ptr<Texture> icon = LoadTextureFromMpq(rArt);
            SetCommandSlotAt(rRow, rCol, OrderIds::SET_RALLY, CommandClickMode::TARGET, true,
                             icon ? std::string() : "Rally", icon, GetIconUri(rArt), -1, "Set Rally Point");
            continue;
        }
        if (ability->IsSpellAbility()) {
            CAbility* spellAbility = ability;
            // Spell buttons carry the retail base order (ref getBaseOrderId)
            // so clicks dispatch through the same order routing as JASS.
            // Passives and item powers have no engine order: no button.
            const int baseOrder = spellAbility->GetBaseOrderId();
            if (baseOrder <= 0) {
                continue;
            }
            CommandClickMode mode = CommandClickMode::IMMEDIATE;
            switch (spellAbility->GetTargetType()) {
            case CSpellTargetType::TARGET_POINT:
                mode = CommandClickMode::POINT_ONLY;
                break;
            case CSpellTargetType::TARGET_UNIT:
            case CSpellTargetType::TARGET_BOTH:
                mode = CommandClickMode::TARGET;
                break;
            case CSpellTargetType::NO_TARGET:
                mode = CommandClickMode::IMMEDIATE;
                break;
            }
            addAbilityButton(spellAbility, baseOrder, mode);
            continue;
        }
        if (ability->GetCategory() == CAbilityCategory::QUEUE) {
            CAbility* queueAbility = ability;
            std::vector<War3ID> trained(queueAbility->GetTrainedUnits().begin(), queueAbility->GetTrainedUnits().end());
            std::sort(trained.begin(), trained.end(),
                      [](const War3ID& lhs, const War3ID& rhs) { return lhs.value < rhs.value; });
            for (const War3ID& trainedId : trained) {
                const auto* trainedType = unitData_ ? unitData_->GetUnitType(trainedId) : nullptr;
                const int nextIndex = FindNextOpenCommandSlot();
                if (nextIndex < 0) {
                    break;
                }
                const int row = nextIndex / kCommandCols;
                const int col = nextIndex % kCommandCols;
                std::string label = trainedId.ToString();
                std::string iconPath;
                if (trainedType) {
                    label = ShortLabel(trainedType->GetName());
                    iconPath = trainedType->GetIconPath();
                }
                const CPlayer* player = simulation_ ? simulation_->GetPlayer(unit->GetPlayerIndex()) : nullptr;
                const HudResourceCosts costs = UnitTypeCosts(trainedType);
                const bool canQueue = queueAbility->GetQueueCount() < CAbility::MAX_QUEUE_SLOTS;
                std::shared_ptr<Texture> icon = iconPath.empty() ? nullptr : LoadTextureFromMpq(iconPath);
                SetCommandSlotAt(row, col, static_cast<int>(trainedId.value), CommandClickMode::IMMEDIATE,
                                 !ability->IsDisabled() && canQueue && PlayerCanAfford(player, costs), icon ? std::string() : label,
                                 icon, iconPath.empty() ? std::string()
                                                        : (icon ? GetIconUri(iconPath) : kDefaultCommandIconUri), -1,
                                 trainedType ? trainedType->GetName() : trainedId.ToString(),
                                 costs);
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
                std::string label = researchId.ToString();
                std::string iconPath;
                const auto* upgradeData = simulation_ ? simulation_->GetUpgradeData() : nullptr;
                const auto* upgrade = upgradeData ? upgradeData->GetUpgrade(researchId) : nullptr;
                if (upgrade) {
                    label = ShortLabel(upgrade->name);
                    iconPath = upgrade->iconPath;
                }
                const CPlayer* player = simulation_ ? simulation_->GetPlayer(unit->GetPlayerIndex()) : nullptr;
                const int currentLevel = player ? player->GetTechLevel(researchId) : 0;
                const HudResourceCosts costs = UpgradeCostsAtLevel(upgrade, currentLevel);
                const bool canQueue = queueAbility->GetQueueCount() < CAbility::MAX_QUEUE_SLOTS;
                const bool notMaxed = !upgrade || currentLevel < upgrade->maxLevel;
                std::shared_ptr<Texture> icon = iconPath.empty() ? nullptr : LoadTextureFromMpq(iconPath);
                SetCommandSlotAt(row, col, static_cast<int>(researchId.value), CommandClickMode::IMMEDIATE,
                                 !ability->IsDisabled() && canQueue && notMaxed && PlayerCanAfford(player, costs), icon ? std::string() : label,
                                 icon, iconPath.empty() ? std::string()
                                                        : (icon ? GetIconUri(iconPath) : kDefaultCommandIconUri), -1,
                                 upgrade ? upgrade->name : researchId.ToString(),
                                 costs);
            }

            if (queueAbility->GetQueueCount() > 0 && !commandSlots_[SlotIndex(2, 3)].visible) {
                addFallbackButton(2, 3, OrderIds::CANCEL_QUEUE, CommandClickMode::IMMEDIATE, true, "Cancel",
                                  "ReplaceableTextures\\CommandButtons\\BTNCancel.blp");
            }
            continue;
        }
        if (ability->GetCategory() == CAbilityCategory::UPGRADE) {
            CAbility* upgradeAbility = ability;
            std::vector<War3ID> upgrades(upgradeAbility->GetUpgradesTo().begin(), upgradeAbility->GetUpgradesTo().end());
            std::sort(upgrades.begin(), upgrades.end(),
                      [](const War3ID& lhs, const War3ID& rhs) { return lhs.value < rhs.value; });
            for (const War3ID& upgradeId : upgrades) {
                const auto* upgradeType = unitData_ ? unitData_->GetUnitType(upgradeId) : nullptr;
                int row = 1;
                int col = 0;
                if (commandSlots_[SlotIndex(row, col)].visible) {
                    const int nextIndex = FindNextOpenCommandSlot();
                    if (nextIndex < 0) {
                        break;
                    }
                    row = nextIndex / kCommandCols;
                    col = nextIndex % kCommandCols;
                }
                std::string label = upgradeId.ToString();
                std::string iconPath;
                if (upgradeType) {
                    label = ShortLabel(upgradeType->GetName());
                    iconPath = upgradeType->GetIconPath();
                }
                const CPlayer* player = simulation_ ? simulation_->GetPlayer(unit->GetPlayerIndex()) : nullptr;
                const HudResourceCosts costs = UnitTypeCosts(upgradeType);
                std::shared_ptr<Texture> icon = iconPath.empty() ? nullptr : LoadTextureFromMpq(iconPath);
                SetCommandSlotAt(row, col, static_cast<int>(upgradeId.value), CommandClickMode::IMMEDIATE,
                                 !ability->IsDisabled() && PlayerCanAfford(player, costs), icon ? std::string() : label,
                                 icon, iconPath.empty() ? std::string()
                                                        : (icon ? GetIconUri(iconPath) : kDefaultCommandIconUri), -1,
                                 upgradeType ? upgradeType->GetName() : upgradeId.ToString(),
                                 costs);
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
        { War3ID("Astp"), 1, 0, "Stop", OrderIds::STOP },
        { War3ID("Ahrl"), 2, 0, "Hold", OrderIds::HOLD_POSITION },
        { War3ID("Aatk"), 3, 0, "Attack", OrderIds::ATTACK },
        { War3ID("Apat"), 0, 1, "Patrol", OrderIds::PATROL },
    };

    for (const auto& fo : fixed) {
        CommandSlot& slot = commandSlots_[SlotIndex(fo.row, fo.col)];
        slot.label.clear();
        slot.orderId = fo.orderId;
        slot.visible = true;
        slot.enabled = true;
        auto* def = abilityData_.GetAbility(fo.abilityId);
        HudResourceCosts costs;
        std::string tooltip = fo.label;
        if (def && !def->iconArt.empty()) {
            slot.icon = LoadTextureFromMpq(def->iconArt);
            slot.iconUri = slot.icon ? GetIconUri(def->iconArt) : kDefaultCommandIconUri;
            costs.mana = def->manaCost;
            tooltip = BuildCommandTooltipText(def->tooltip.empty() ? def->name : def->tooltip, def->uberTip);
        }
        if (!slot.icon) {
            std::string fallbackPath = "ReplaceableTextures\\CommandButtons\\BTN";
            fallbackPath += fo.label;
            fallbackPath += ".blp";
            slot.icon = LoadTextureFromMpq(fallbackPath);
            slot.iconUri = slot.icon ? GetIconUri(fallbackPath) : kDefaultCommandIconUri;
        }
        slot.tooltip = tooltip;
        slot.resourceCosts = costs;
        slot.autocastOrderId = 0;
        slot.active = false;
        slot.autocastActive = false;
    }

    for (const auto& abilId : abilityIds) {
        auto* def = abilityData_.GetAbility(abilId);
        if (!def) continue;
        if (def->buttonPosX < 0 || def->buttonPosY < 0) continue;
        if (def->buttonPosX >= kCommandCols || def->buttonPosY >= kCommandRows) continue;

        CommandSlot& slot = commandSlots_[SlotIndex(def->buttonPosY, def->buttonPosX)];
        if (!slot.label.empty()) continue;

        slot.icon = LoadTextureFromMpq(def->iconArt);
        slot.iconUri = slot.icon ? GetIconUri(def->iconArt) : kDefaultCommandIconUri;
        slot.label = slot.icon ? std::string() : ShortLabel(def->name);
        slot.tooltip = BuildCommandTooltipText(def->tooltip.empty() ? def->name : def->tooltip, def->uberTip);
        slot.resourceCosts = HudResourceCosts{};
        slot.resourceCosts.mana = def->manaCost;
        slot.autocastOrderId = 0;
        slot.active = false;
        slot.autocastActive = false;
        slot.orderId = -1;
        slot.visible = true;
        slot.enabled = false;
    }
}

void MainUI::PopulateDebugCommandCardForUiOnly() {
    ClearCommandSlots();
    playerGold_ = 735;
    playerLumber_ = 420;
    playerFoodUsed_ = 3;
    playerFoodCap_ = 12;
    ShowHeroPanel("UI Debug Hero", 5, 280, 600, 18, 2, 14, 0, 21, 3);
    UpdateInfoPanel(War3ID("hfoo"), "UI Debug Hero", 5, 385.0f, 725.0f, 120.0f, 220.0f);

    SetCommandSlotAt(0, 0, 851986, CommandClickMode::POINT_ONLY, true, "Move", nullptr,
                     kDefaultCommandIconUri, -1, "Move", HudResourceCosts{});
    SetCommandSlotAt(0, 1, 851983, CommandClickMode::POINT_ONLY, true, "Patrol", nullptr,
                     kDefaultCommandIconUri, 1, "Patrol", HudResourceCosts{});
    commandSlots_[SlotIndex(0, 1)].active = true;
    SetCommandSlotAt(0, 2, 851983, CommandClickMode::POINT_ONLY, false, "Attack", nullptr,
                     kDefaultCommandIconUri, -1, "Attack", HudResourceCosts{});
    SetCommandSlotAt(1, 0, 852095, CommandClickMode::IMMEDIATE, true, "Auto", nullptr,
                     kDefaultCommandIconUri, -1, "Autocast", HudResourceCosts{},
                     0.0f, 0.0f, 852096, true);
    SetCommandSlotAt(1, 1, 852066, CommandClickMode::IMMEDIATE, true, "Cooldown", nullptr,
                     kDefaultCommandIconUri, -1, "Cooldown", HudResourceCosts{},
                     8.0f, 15.0f);

    multiSelectionUnits_.clear();
    MultiSelectUnitInfo hero;
    hero.handleId = 1;
    hero.name = "UI Debug Hero";
    hero.hpRatio = 0.62f;
    hero.mpRatio = 0.55f;
    hero.playerColor = 0xFF006CFF;
    hero.level = 5;
    hero.isHero = true;
    multiSelectionUnits_.push_back(hero);

    MultiSelectUnitInfo unit;
    unit.handleId = 2;
    unit.name = "UI Debug Unit";
    unit.hpRatio = 0.86f;
    unit.mpRatio = 0.0f;
    unit.playerColor = 0xFF2FA046;
    unit.isHero = false;
    multiSelectionUnits_.push_back(unit);
}

void MainUI::OpenGameMenuForUiOnly() {
    if (hudHost_) {
        hudHost_->OpenGameMenu();
    }
}

void MainUI::OpenGameMenuPanelForUiOnly(const std::string& panel) {
    if (hudHost_) {
        hudHost_->OpenGameMenuDebugPanel(panel);
    }
}

bool MainUI::InvokeGameMenuActionForUiOnly(const std::string& actionName) {
    return hudHost_ && hudHost_->InvokeGameMenuActionForDebug(actionName);
}

bool MainUI::InvokeUpperButtonForUiOnly(const std::string& buttonName) {
    return hudHost_ && hudHost_->InvokeUpperButtonForDebug(buttonName);
}

void MainUI::OpenGlueMenuForUiOnly(const std::string& pageName) {
    if (hudHost_) {
        hudHost_->OpenGlueMenuDebugPage(pageName);
    }
}

bool MainUI::InvokeGlueMenuActionForUiOnly(const std::string& actionName) {
    return hudHost_ && hudHost_->InvokeGlueMenuActionForDebug(actionName);
}

void MainUI::SetGlueMenuMapsForUiOnly(const std::vector<War3UI::GlueMapListItem>& maps) {
    if (hudHost_) {
        hudHost_->SetGlueMenuMaps(maps);
    }
}

void MainUI::SetGlueMenuCampaignsForUiOnly(const std::vector<War3UI::GlueCampaignListItem>& campaigns) {
    if (hudHost_) {
        hudHost_->SetGlueMenuCampaigns(campaigns);
    }
}

void MainUI::SetGlueMapLaunchCallback(War3UI::GlueMapLaunchCallback cb) {
    if (hudHost_) {
        hudHost_->SetGlueMapLaunchCallback(std::move(cb));
    }
}

void MainUI::SetResourcesForUiOnly(int gold, int lumber, int foodUsed, int foodCap, const std::string& timeText) {
    SetLiveResources(gold, lumber, foodUsed, foodCap, timeText, upkeepLevel_);
    std::cout << "UI-only mode: debug resources gold=" << playerGold_
              << " lumber=" << playerLumber_
              << " food=" << playerFoodUsed_ << "/" << playerFoodCap_
              << " time=" << resourceTimeText_ << std::endl;
}

void MainUI::SetLiveResources(int gold, int lumber, int foodUsed, int foodCap,
                              const std::string& timeText, int upkeepLevel) {
    playerGold_ = gold;
    playerLumber_ = lumber;
    playerFoodUsed_ = foodUsed;
    playerFoodCap_ = foodCap;
    resourceTimeText_ = timeText.empty() ? "12:00" : timeText;
    upkeepLevel_ = upkeepLevel;
}

void MainUI::SetTimerDialog(const std::string& title, const std::string& value, bool visible) {
    timerDialogTitle_ = title;
    timerDialogValue_ = value;
    timerDialogVisible_ = visible;
}

void MainUI::SetCinematicScene(const std::string& speaker, const std::string& dialogue, bool visible) {
    cinematicSpeaker_ = speaker;
    cinematicDialogue_ = dialogue;
    cinematicVisible_ = visible;
}

void MainUI::ShowGameMessage(const std::string& text, float expireSeconds) {
    if (hudHost_) hudHost_->ShowGameMessage(text, expireSeconds);
}

void MainUI::ClearGameMessages() {
    if (hudHost_) hudHost_->ClearGameMessages();
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

void MainUI::SetExternalMinimapVisible(bool visible) {
    if (hudHost_) hudHost_->SetExternalMinimapVisible(visible);
}

ScreenRect MainUI::GetPortraitRect() const {
    return hudHost_ ? hudHost_->GetPortraitRect() : ScreenRect{};
}

int MainUI::HitMultiSelection(float x, float y) const {
    return hudHost_ ? hudHost_->HitMultiSelection(x, y) : -1;
}

int MainUI::HitQueueSlot(float x, float y) const {
    return hudHost_ ? hudHost_->HitQueueSlot(x, y) : -1;
}

int MainUI::HitInventorySlot(float x, float y) const {
    return hudHost_ ? hudHost_->HitInventorySlot(x, y) : -1;
}

int MainUI::HitBuffSlot(float x, float y) const {
    return hudHost_ ? hudHost_->HitBuffSlot(x, y) : -1;
}

std::string MainUI::GetIconUri(const std::string& path) const {
    std::string normalized = NormalizeAssetPath(path);
    if (normalized.empty()) return {};

    auto it = iconTextureUris_.find(normalized);
    return it != iconTextureUris_.end() ? it->second : std::string();
}

std::string MainUI::ExportIconTexture(const std::string& normalizedPath, const std::vector<uint8_t>& data) {
    if (normalizedPath.empty() || data.empty()) return {};

    std::filesystem::path relativePath = std::filesystem::path("assets") / "ui" / "cache" / "icons"
        / (SanitizeCacheFileStem(normalizedPath) + ".png");
    std::filesystem::path absolutePath = std::filesystem::path(PROJECT_SOURCE_DIR) / relativePath;

    std::error_code ec;
    std::filesystem::create_directories(absolutePath.parent_path(), ec);
    if (ec) {
        std::cerr << "MainUI: failed to create Icon cache directory: " << ec.message() << std::endl;
        return {};
    }

    std::string uri = relativePath.generic_string();
    if (std::filesystem::exists(absolutePath, ec) && !ec) {
        return uri;
    }

    std::vector<uint8_t> rgba;
    int width = 0;
    int height = 0;

    if (data.size() >= 4 && (std::memcmp(data.data(), "BLP1", 4) == 0 || std::memcmp(data.data(), "BLP2", 4) == 0)) {
        BLPImage image;
        if (!DecodeBLP(data.data(), data.size(), image)) {
            return {};
        }
        width = image.width;
        height = image.height;
        rgba = std::move(image.pixels);
    } else {
        int channels = 0;
        stbi_uc* pixels = stbi_load_from_memory(data.data(), (int)data.size(), &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels) {
            return {};
        }
        rgba.assign(pixels, pixels + ((size_t)width * (size_t)height * 4));
        stbi_image_free(pixels);
    }

    if (width <= 0 || height <= 0 || rgba.empty()) return {};

    if (!stbi_write_png(absolutePath.string().c_str(), width, height, 4, rgba.data(), width * 4)) {
        std::cerr << "MainUI: failed to write Icon cache: " << absolutePath.string() << std::endl;
        return {};
    }

    return uri;
}

std::shared_ptr<Texture> MainUI::LoadTextureFromMpq(const std::string& path) {
    if (!mpq_ || path.empty()) return nullptr;

    std::string normalized = NormalizeAssetPath(path);
    if (normalized.empty()) return nullptr;

    auto it = textures_.find(normalized);
    if (it != textures_.end()) return it->second;

    resource::ResourceServer& resServer = resource::ResourceServer::Instance();
    if (resServer.GetMpqManager() != mpq_) {
        resServer.SetMpqManager(mpq_);
    }

    // Phase D2: Pack decoded small icons into dynamic 2D TextureAtlasPacker
    if (auto blpRes = render::BlpMipTextureResource::LoadFromVfs(resServer, normalized, true)) {
        if (blpRes->width > 0 && blpRes->width <= 64 &&
            blpRes->height > 0 && blpRes->height <= 64 &&
            !blpRes->GetBasePixels().empty()) {
            iconAtlasPacker_.PackSprite(
                normalized, blpRes->width, blpRes->height, blpRes->GetBasePixels().data(), nullptr);
        }
    }

    std::vector<uint8_t> buf;
    if (!resServer.ReadRawBytes(normalized, buf) && !mpq_->ReadFile(normalized, buf)) return nullptr;

    auto tex = std::make_shared<Texture>();
    if (!tex->LoadFromMemory(buf.data(), buf.size())) return nullptr;

    iconTextureUris_[normalized] = ExportIconTexture(normalized, buf);
    textures_[normalized] = tex;
    return tex;
}
