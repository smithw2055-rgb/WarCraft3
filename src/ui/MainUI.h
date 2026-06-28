#pragma once
#include "UIFrame.h"
#include "SimpleFrame.h"
#include "CommandCard.h"
#include "ResourceBarFrame.h"
#include "ChatConsoleFrame.h"
#include "TextRenderer.h"
#include "UiRenderer.h"
#include "MultiSelectionPanel.h"
#include "GlueMenuTypes.h"
#include "../asset/Texture.h"
#include "../resource/mpq_storm.h"
#include "../data/abilitydata.h"
#include "../resource/slk.h"
#include <cstdint>
#include <memory>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

class CUnitData;
class CSimulation;
class CUnit;
class NoesisHudHost;
struct sapp_event;

// In-world unit overlay data (populated by main.cpp, rendered by MainUI)
struct UnitOverlayInfo {
    int handleId = -1;
    float screenX = 0, screenY = 0;
    float footX = 0, footY = 0;
    float hpRatio = 1.0f;
    float mpRatio = 0.0f;
    bool isSelected = false;
    bool isAlive = true;
    std::string name;
    uint32_t playerColor = 0xFF4488FF;
};

struct ScreenRect {
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;

    bool IsValid() const { return w > 0.0f && h > 0.0f; }
    bool Contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

struct HudQueueItem {
    int slot = -1;
    std::string label;
    std::string iconUri;
    bool visible = false;
};

struct HudResourceCosts {
    int gold = 0;
    int lumber = 0;
    int stone = 0;
    int mana = 0;
    int food = 0;
};

struct HudCommandSlot {
    std::string label;
    std::string tooltip;
    std::string iconUri;
    HudResourceCosts resourceCosts;
    float cooldownRemaining = 0.0f;
    float cooldownMax = 0.0f;
    int autocastOrderId = 0;
    int numberOverlay = -1;
    bool enabled = false;
    bool visible = false;
    bool active = false;
    bool autocastActive = false;
};

struct HudInventoryItem {
    int slot = -1;
    std::string label;
    std::string iconUri;
    int charges = 0;
    bool occupied = false;
    bool enabled = false;
    bool visible = false;
};

struct HudInfoStats {
    std::string attackText;
    std::string armorText;
    std::string goldText;
    std::string foodText;
    std::string attackIconKey;
    std::string armorIconKey;
    bool visible = false;
};

struct HudBuffItem {
    std::string name;
    std::string iconUri;
    float remaining = 0.0f;
    float duration = 0.0f;
    int stackCount = 1;
    bool positive = true;
    bool visible = false;
};

class MainUI {
public:
    MainUI();
    ~MainUI();

    bool Init(MpqManager* mpq, int screenW, int screenH,
              const std::string& hudXamlPath = "assets/ui/Hud.xaml", int gameVersion = 0);
    void Shutdown();
    void Update(int screenW, int screenH);
    void RenderNoesisOffscreen();
    void RenderNoesisOnly();
    void Render();
    void SetUnitData(const CUnitData* ud) { unitData_ = ud; }
    bool HandleEvent(const sapp_event* ev);

    bool OnMouseDown(float x, float y, int btn);
    bool OnMouseUp(float x, float y, int btn);
    void OnKeyDown(int key);

    CommandCard* GetCommandCard() { return commandCard_; }
    GameUI& GetGameUI() { return gameUI_; }

    void PopulateCommandCardForUnitType(War3ID unitTypeId);
    void PopulateCommandCardForUnit(CUnit* unit);
    void PopulateCommandCardFromAbilities(const std::vector<War3ID>& abilityIds);
    void PopulateDebugCommandCardForUiOnly();
    void OpenGameMenuForUiOnly();
    void OpenGameMenuPanelForUiOnly(const std::string& panel);
    bool InvokeGameMenuActionForUiOnly(const std::string& actionName);
    bool InvokeUpperButtonForUiOnly(const std::string& buttonName);
    void OpenGlueMenuForUiOnly(const std::string& pageName);
    bool InvokeGlueMenuActionForUiOnly(const std::string& actionName);
    void SetGlueMenuMapsForUiOnly(const std::vector<War3UI::GlueMapListItem>& maps);
    void SetGlueMenuCampaignsForUiOnly(const std::vector<War3UI::GlueCampaignListItem>& campaigns);
    void SetResourcesForUiOnly(int gold, int lumber, int foodUsed, int foodCap, const std::string& timeText);
    void SetSimulation(CSimulation* sim) { simulation_ = sim; }
    void SetSelectedUnit(int handleId);
    void WireCommandCardCallbacks();

    void UpdateInfoPanel(War3ID unitTypeId, const std::string& name, int level,
                         float hp, float maxHp, float mp, float maxMp);
    void ClearInfoPanel();

    // Tooltip
    void ShowTooltip(const std::string& text, float screenX, float screenY,
                     const HudResourceCosts& costs = HudResourceCosts{});
    void HideTooltip();
    void UpdateTooltip(float mouseX, float mouseY);

    // Chat
    ChatConsoleFrame* GetChatConsole() { return chatConsole_; }
    bool IsChatOpen() const;
    void OnCharInput(char c);

    // Hero panel
    void ShowHeroPanel(const std::string& name, int level,
                       int xp, int maxXp,
                       int strBase, int strBonus,
                       int agiBase, int agiBonus,
                       int intBase, int intBonus);
    void HideHeroPanel();

    // In-world unit overlays (health bars, selection indicators, names)
    void SetOverlayData(const std::vector<UnitOverlayInfo>& data) { overlayData_ = &data; }
    void SetMultiSelection(const std::vector<MultiSelectUnitInfo>& units);
    void SetSelectionBox(const ScreenRect& rect);
    void ClearSelectionBox();
    void RenderOverlays(UiRenderer& r);
    int HitQueueSlot(float x, float y) const;

    // Resource bar access for game state updates
    ResourceBarFrame* GetResourceBar() { return resourceBar_; }

    void UpdateHeroPanelFromUnit(War3ID unitTypeId, int handleId);

    // Pending command (for buttons that need a target click)
    enum class PendingCommandMode : uint8_t {
        NONE,
        POINT_OR_WIDGET,
        POINT_ONLY,
    };

    void SetPendingCommand(int orderId, PendingCommandMode mode = PendingCommandMode::POINT_OR_WIDGET) {
        pendingCommandOrderId_ = orderId;
        pendingCommandMode_ = (orderId >= 0) ? mode : PendingCommandMode::NONE;
    }
    int GetPendingCommand() const { return pendingCommandOrderId_; }
    PendingCommandMode GetPendingCommandMode() const { return pendingCommandMode_; }
    void ClearPendingCommand() {
        pendingCommandOrderId_ = -1;
        pendingCommandMode_ = PendingCommandMode::NONE;
    }
    ScreenRect GetMinimapRect() const;
    ScreenRect GetPortraitRect() const;
    int HitMultiSelection(float x, float y) const;
    int HitInventorySlot(float x, float y) const;

private:
    enum class CommandClickMode : uint8_t {
        NONE,
        IMMEDIATE,
        TARGET,
        POINT_ONLY,
        OPEN_MENU,
        CLOSE_MENU,
    };

    struct CommandSlot {
        std::string label;
        std::string tooltip;
        int orderId = -1;
        bool visible = false;
        bool enabled = false;
        std::shared_ptr<Texture> icon;
        std::string noesisIconUri;
        HudResourceCosts resourceCosts;
        float cooldownRemaining = 0.0f;
        float cooldownMax = 0.0f;
        int autocastOrderId = 0;
        int numberOverlay = -1;
        bool active = false;
        bool autocastActive = false;
        CommandClickMode clickMode = CommandClickMode::NONE;
    };

    void ClearCommandSlots();
    void HandleCommandButtonClick(int index);
    void SetCommandSlotAt(int row, int col,
                          int orderId,
                          CommandClickMode clickMode,
                          bool enabled,
                          const std::string& label,
                          const std::shared_ptr<Texture>& icon,
                          const std::string& noesisIconUri,
                          int numberOverlay = -1,
                          const std::string& tooltip = std::string(),
                          const HudResourceCosts& resourceCosts = HudResourceCosts{},
                          float cooldownRemaining = 0.0f,
                          float cooldownMax = 0.0f,
                          int autocastOrderId = 0,
                          bool autocastActive = false);
    int FindNextOpenCommandSlot() const;
    void SubmitChatMessage();
    void BrowseChatHistory(int delta);
    void UpdateQueueState();
    bool HandleQueueSlotClick(int slot);
    void UpdateInventoryState();
    bool HandleInventorySlotClick(int slot);
    void UpdatePortraitIconState();
    void UpdateBuffState();
    std::shared_ptr<Texture> LoadTextureFromMpq(const std::string& path);
    std::string GetNoesisTextureUri(const std::string& path) const;
    std::string ExportNoesisTexture(const std::string& normalizedPath, const std::vector<uint8_t>& data);
    void LoadFont();

    GameUI gameUI_;
    MpqManager* mpq_ = nullptr;
    SimpleFrame* rootFrame_ = nullptr;
    std::unique_ptr<NoesisHudHost> hudHost_;

    // Text renderer
    TextRenderer textRenderer_;

    CommandCard* commandCard_ = nullptr;
    ResourceBarFrame* resourceBar_ = nullptr;
    std::unordered_map<std::string, std::shared_ptr<Texture>> textures_;
    std::unordered_map<std::string, std::string> noesisTextureUris_;

    // Data
    CAbilityData abilityData_;
    SlkTable unitAbilitiesSlk_;
    const CUnitData* unitData_ = nullptr;
    CSimulation* simulation_ = nullptr;
    int selectedUnitHandle_ = -1;

    std::vector<War3ID> GetUnitAbilities(War3ID unitTypeId);

    const std::vector<UnitOverlayInfo>* overlayData_ = nullptr;
    MultiSelectionPanel multiSelectionPanel_;
    std::vector<MultiSelectUnitInfo> multiSelectionUnits_;
    bool selectionBoxVisible_ = false;
    ScreenRect selectionBoxRect_;

    ChatConsoleFrame* chatConsole_ = nullptr;

    // Tooltip state
    float tooltipTimer_ = 0;
    bool tooltipActive_ = false;
    std::string pendingTooltipText_;
    HudResourceCosts pendingTooltipCosts_;
    float pendingTooltipX_ = 0, pendingTooltipY_ = 0;
    static constexpr float kTooltipDelay = 0.4f;

    // Pending command mode
    int pendingCommandOrderId_ = -1;
    PendingCommandMode pendingCommandMode_ = PendingCommandMode::NONE;
    int activeSubmenuOrderId_ = 0;

    // Current HUD state
    std::string selectedName_ = "No unit selected";
    int selectedLevel_ = 0;
    float selectedHp_ = 0.0f;
    float selectedMaxHp_ = 0.0f;
    float selectedMp_ = 0.0f;
    float selectedMaxMp_ = 0.0f;
    bool heroVisible_ = false;
    int heroXp_ = 0;
    int heroMaxXp_ = 0;
    int heroStrBase_ = 0;
    int heroStrBonus_ = 0;
    int heroAgiBase_ = 0;
    int heroAgiBonus_ = 0;
    int heroIntBase_ = 0;
    int heroIntBonus_ = 0;

    int playerGold_ = 0;
    int playerLumber_ = 0;
    int playerFoodUsed_ = 0;
    int playerFoodCap_ = 0;
    std::string resourceTimeText_ = "12:00";

    std::array<CommandSlot, 12> commandSlots_{};
    std::vector<HudQueueItem> queueItems_;
    float queueProgressRatio_ = 0.0f;
    std::vector<HudInventoryItem> inventoryItems_;
    bool inventoryAvailable_ = false;
    int inventoryCapacity_ = 0;
    std::string selectedPortraitIconUri_;
    HudInfoStats selectedInfoStats_;
    std::vector<HudBuffItem> selectedBuffs_;

    bool chatOpen_ = false;
    std::vector<std::string> chatMessages_;
    std::vector<std::string> chatHistory_;
    int chatHistoryIndex_ = 0;
    std::string chatDraft_;
    double uiTime_ = 0.0;
};
