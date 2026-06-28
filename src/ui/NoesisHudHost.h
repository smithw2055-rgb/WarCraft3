#pragma once

#include "MainUI.h"
#include "GlueMenuControl.h"
#include <NsCore/Ptr.h>
#include <NsGui/IView.h>
#include <NsGui/UserControl.h>
#include <NsRender/RenderDevice.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Noesis {
class Border;
class Canvas;
class FrameworkElement;
class Image;
class TextBlock;
class TextBox;
}

namespace War3UI {
class MdxSceneControl;
}

struct sapp_event;
class MpqManager;

class NoesisHudHost {
public:
    NoesisHudHost() = default;
    ~NoesisHudHost();

    bool Initialize(const std::string& assetRoot, int width, int height, const std::string& xamlPath,
                    MpqManager* mpq = nullptr, int gameVersion = 0);
    void Shutdown();
    void Resize(int width, int height);
    void Update(double time);
    void RenderOffscreen();
    void Render();
    bool HandleEvent(const sapp_event* e);

    void SetResources(int gold, int lumber, int foodUsed, int foodCap, const std::string& timeText);
    void SetSelection(const std::string& name, int level, float hp, float maxHp, float mp, float maxMp,
                      bool heroVisible, int xp, int maxXp,
                      int strBase, int strBonus, int agiBase, int agiBonus, int intBase, int intBonus,
                      const HudInfoStats& infoStats = HudInfoStats{});
    void SetTooltip(const std::string& text, float screenX, float screenY, bool visible,
                    const HudResourceCosts& costs = HudResourceCosts{});
    void SetChatOpen(bool open);
    void SetChatInput(const std::string& text);
    std::string GetChatInput() const;
    void SetChatMessages(const std::vector<std::string>& messages);
    void SetCommandSlot(int index, const HudCommandSlot& slot);
    void SetHoveredCommand(int index);
    void SetPressedCommand(int index);
    void SetHoveredUpperButton(int index);
    void SetPressedUpperButton(int index);
    void SetPressedMinimapButton(int index);
    void ActivateMinimapButton(int index);
    void SetMultiSelection(const std::vector<MultiSelectUnitInfo>& units, int primaryHandle);
    void SetQueue(const std::vector<HudQueueItem>& items, float progressRatio);
    void SetInventory(const std::vector<HudInventoryItem>& items, int capacity, bool available);
    void SetPortraitIcon(const std::string& iconUri);
    void SetBuffs(const std::vector<HudBuffItem>& buffs);
    void OpenGameMenu();
    void OpenGameMenuDebugPanel(const std::string& panel);
    void CloseGameMenu();
    void ToggleGameMenu();
    bool IsGameMenuOpen() const { return gameMenuOpen_; }
    int HitGameMenuButton(float x, float y) const;
    void SetHoveredGameMenuButton(int action);
    void SetPressedGameMenuButton(int action);
    void HandleGameMenuAction(int action);
    bool InvokeGameMenuActionForDebug(const std::string& actionName);
    bool InvokeUpperButtonForDebug(const std::string& buttonName);
    bool IsGlueMenuActive() const { return glueMenuControl_ != nullptr; }
    int HitGlueMenuButton(float x, float y) const;
    void SetHoveredGlueMenuButton(int action);
    void SetPressedGlueMenuButton(int action);
    void HandleGlueMenuAction(int action);
    void OpenGlueMenuDebugPage(const std::string& pageName);
    bool InvokeGlueMenuActionForDebug(const std::string& actionName);
    void SetGlueMenuMaps(const std::vector<War3UI::GlueMapListItem>& maps);
    void SetGlueMenuCampaigns(const std::vector<War3UI::GlueCampaignListItem>& campaigns);

    ScreenRect GetMinimapRect() const { return minimapRect_; }
    ScreenRect GetPortraitRect() const { return portraitRect_; }
    ScreenRect GetCommandRect(int index) const;
    int HitCommandButton(float x, float y) const;
    int HitUpperButton(float x, float y) const;
    int HitMinimapButton(float x, float y) const;
    int HitMultiSelection(float x, float y) const;
    int HitQueueSlot(float x, float y) const;
    int HitInventorySlot(float x, float y) const;

private:
    void CacheElements();
    void LogMissingElements() const;
    void CacheRects();
    void ApplyState();
    void ApplyLayoutMetrics();
    ScreenRect GetElementRect(Noesis::FrameworkElement* element) const;

    enum class GameMenuPanel : uint8_t {
        Main,
        EndGame,
        ConfirmQuit,
        SaveGame,
        LoadGame,
        SaveOverwriteConfirm,
        SaveDeleteConfirm,
        Help,
        Tips,
        Options,
        OptionsGameplay,
        OptionsVideo,
        OptionsSound,
        OptionsNetwork,
    };

    static constexpr int kGameMenuActionCount = 70;
    bool IsGameMenuActionEnabled(int action) const;
    void SetGameMenuPanel(GameMenuPanel panel);

    std::string assetRoot_;

    Noesis::Ptr<Noesis::IView> view_;
    Noesis::Ptr<Noesis::UserControl> root_;
    Noesis::Ptr<Noesis::RenderDevice> device_;

    Noesis::TextBlock* goldText_ = nullptr;
    Noesis::TextBlock* lumberText_ = nullptr;
    Noesis::TextBlock* foodText_ = nullptr;
    Noesis::TextBlock* upkeepText_ = nullptr;
    Noesis::TextBlock* timeText_ = nullptr;
    Noesis::FrameworkElement* timeOfDayIndicator_ = nullptr;
    Noesis::TextBlock* unitNameText_ = nullptr;
    Noesis::TextBlock* unitLevelText_ = nullptr;
    Noesis::TextBlock* hpText_ = nullptr;
    Noesis::TextBlock* mpText_ = nullptr;
    Noesis::TextBlock* heroStatsText_ = nullptr;
    Noesis::FrameworkElement* heroStatsPanel_ = nullptr;
    Noesis::TextBlock* heroStrengthText_ = nullptr;
    Noesis::TextBlock* heroAgilityText_ = nullptr;
    Noesis::TextBlock* heroIntelligenceText_ = nullptr;
    Noesis::FrameworkElement* infoStatsPanel_ = nullptr;
    std::array<Noesis::Image*, 16> infoAttackIcons_{};
    std::array<Noesis::Image*, 18> infoArmorIcons_{};
    Noesis::TextBlock* infoAttackText_ = nullptr;
    Noesis::TextBlock* infoArmorText_ = nullptr;
    Noesis::TextBlock* infoGoldText_ = nullptr;
    Noesis::TextBlock* infoFoodText_ = nullptr;
    Noesis::FrameworkElement* buffStatusBar_ = nullptr;
    Noesis::TextBlock* buffStatusBarLabel_ = nullptr;
    std::array<Noesis::Border*, 8> buffSlots_{};
    std::array<Noesis::Image*, 8> buffIcons_{};
    std::array<Noesis::TextBlock*, 8> buffLabels_{};
    std::array<Noesis::TextBlock*, 8> buffDurations_{};
    std::array<Noesis::Border*, 8> buffStackBacks_{};
    std::array<Noesis::TextBlock*, 8> buffStacks_{};
    Noesis::Border* hpFill_ = nullptr;
    Noesis::Border* mpFill_ = nullptr;
    Noesis::Border* xpFill_ = nullptr;
    Noesis::Border* tooltipPanel_ = nullptr;
    Noesis::TextBlock* tooltipText_ = nullptr;
    Noesis::FrameworkElement* tooltipResourceRow_ = nullptr;
    std::array<Noesis::FrameworkElement*, 5> tooltipCostGroups_{};
    std::array<Noesis::TextBlock*, 5> tooltipCostTexts_{};
    Noesis::Border* chatPanel_ = nullptr;
    Noesis::TextBlock* chatLogText_ = nullptr;
    Noesis::TextBox* chatInput_ = nullptr;
    Noesis::FrameworkElement* minimapViewport_ = nullptr;
    Noesis::FrameworkElement* minimapContentViewport_ = nullptr;
    std::array<Noesis::FrameworkElement*, 5> minimapButtons_{};
    std::array<Noesis::Image*, 5> minimapButtonPushed_{};
    std::array<Noesis::Image*, 5> minimapButtonDisabled_{};
    std::array<Noesis::Image*, 5> minimapButtonInactive_{};
    std::array<Noesis::Image*, 5> minimapButtonInactivePushed_{};
    std::array<Noesis::Image*, 5> minimapButtonInactiveDisabled_{};
    std::array<Noesis::Image*, 5> minimapButtonOff_{};
    std::array<Noesis::Image*, 5> minimapButtonOffPushed_{};
    std::array<Noesis::Image*, 5> minimapButtonOffDisabled_{};
    Noesis::FrameworkElement* portraitViewport_ = nullptr;
    Noesis::FrameworkElement* portraitIcon_ = nullptr;
    Noesis::Image* portraitImage_ = nullptr;
    Noesis::FrameworkElement* consolePanel_ = nullptr;
    Noesis::FrameworkElement* upperButtonBar_ = nullptr;
    std::array<Noesis::FrameworkElement*, 4> upperButtons_{};
    std::array<Noesis::Image*, 4> upperButtonPushed_{};
    std::array<Noesis::Image*, 4> upperButtonHighlights_{};
    std::array<Noesis::TextBlock*, 4> upperButtonLabels_{};
    Noesis::FrameworkElement* commandColumn_ = nullptr;
    Noesis::FrameworkElement* inventoryCover_ = nullptr;
    Noesis::FrameworkElement* inventoryPanel_ = nullptr;
    Noesis::FrameworkElement* inventoryGrid_ = nullptr;
    Noesis::FrameworkElement* commandCardPanel_ = nullptr;
    Noesis::FrameworkElement* commandCardGrid_ = nullptr;
    std::array<Noesis::FrameworkElement*, 12> commandButtons_{};
    std::array<Noesis::Image*, 12> commandIcons_{};
    std::array<Noesis::TextBlock*, 12> commandLabels_{};
    std::array<Noesis::Border*, 12> commandDisabledBacks_{};
    std::array<Noesis::Border*, 12> commandHoverHighlights_{};
    std::array<Noesis::Border*, 12> commandPressedBorders_{};
    std::array<Noesis::Border*, 12> commandActiveHighlights_{};
    std::array<Noesis::Border*, 12> commandCooldownOverlays_{};
    std::array<Noesis::TextBlock*, 12> commandCooldownLabels_{};
    std::array<Noesis::Border*, 12> commandNumberBacks_{};
    std::array<Noesis::TextBlock*, 12> commandNumberLabels_{};
    std::array<Noesis::Border*, 12> commandAutocastOverlays_{};
    std::array<Noesis::Border*, 12> multiSelectSlots_{};
    std::array<Noesis::Border*, 12> multiSelectColors_{};
    std::array<Noesis::Border*, 12> multiSelectFocusHighlights_{};
    std::array<Noesis::Border*, 12> multiSelectHeroGlows_{};
    std::array<Noesis::Border*, 12> multiSelectHpFills_{};
    std::array<Noesis::Border*, 12> multiSelectMpFills_{};
    std::array<Noesis::TextBlock*, 12> multiSelectLevelLabels_{};
    Noesis::Border* queueBackdrop_ = nullptr;
    std::array<Noesis::Border*, 5> queueSlots_{};
    std::array<Noesis::Image*, 5> queueIcons_{};
    std::array<Noesis::TextBlock*, 5> queueLabels_{};
    Noesis::FrameworkElement* queueProgressTrack_ = nullptr;
    Noesis::Border* queueProgressFill_ = nullptr;
    std::array<Noesis::Border*, 6> inventorySlots_{};
    std::array<Noesis::Border*, 6> inventoryDisabledBacks_{};
    std::array<Noesis::Image*, 6> inventoryIcons_{};
    std::array<Noesis::TextBlock*, 6> inventoryLabels_{};
    std::array<Noesis::Border*, 6> inventoryChargeBacks_{};
    std::array<Noesis::TextBlock*, 6> inventoryChargeLabels_{};
    Noesis::FrameworkElement* gameMenuRoot_ = nullptr;
    Noesis::FrameworkElement* gameMenuBackdrop_ = nullptr;
    Noesis::FrameworkElement* gameMenuMainPanel_ = nullptr;
    Noesis::FrameworkElement* gameMenuEndGamePanel_ = nullptr;
    Noesis::FrameworkElement* gameMenuConfirmQuitPanel_ = nullptr;
    Noesis::FrameworkElement* gameMenuSaveGamePanel_ = nullptr;
    Noesis::FrameworkElement* gameMenuLoadGamePanel_ = nullptr;
    Noesis::FrameworkElement* gameMenuOverwriteConfirmPanel_ = nullptr;
    Noesis::FrameworkElement* gameMenuDeleteConfirmPanel_ = nullptr;
    Noesis::FrameworkElement* gameMenuHelpPanel_ = nullptr;
    Noesis::FrameworkElement* gameMenuTipsPanel_ = nullptr;
    Noesis::FrameworkElement* gameMenuOptionsPanel_ = nullptr;
    Noesis::FrameworkElement* gameMenuOptionsListPanel_ = nullptr;
    Noesis::FrameworkElement* gameMenuGameplayPanel_ = nullptr;
    Noesis::FrameworkElement* gameMenuVideoPanel_ = nullptr;
    Noesis::FrameworkElement* gameMenuSoundPanel_ = nullptr;
    Noesis::FrameworkElement* gameMenuNetworkPanel_ = nullptr;
    Noesis::FrameworkElement* gameMenuOptionsBottomPanel_ = nullptr;
    Noesis::TextBlock* gameMenuPauseText_ = nullptr;
    std::array<Noesis::FrameworkElement*, kGameMenuActionCount> gameMenuButtons_{};
    std::array<Noesis::FrameworkElement*, kGameMenuActionCount> gameMenuButtonHighlights_{};
    std::array<Noesis::FrameworkElement*, kGameMenuActionCount> gameMenuButtonPressed_{};
    std::array<Noesis::FrameworkElement*, kGameMenuActionCount> gameMenuOptionDisabled_{};
    std::array<Noesis::FrameworkElement*, kGameMenuActionCount> gameMenuOptionChecked_{};
    std::array<Noesis::FrameworkElement*, kGameMenuActionCount> gameMenuOptionDisabledChecked_{};
    std::array<Noesis::FrameworkElement*, kGameMenuActionCount> gameMenuOptionSliderThumbs_{};
    std::array<Noesis::FrameworkElement*, kGameMenuActionCount> gameMenuOptionPopupMenus_{};
    std::array<Noesis::TextBlock*, kGameMenuActionCount> gameMenuOptionValueTexts_{};
    War3UI::GlueMenuControl* glueMenuControl_ = nullptr;
    War3UI::MdxSceneControl* glueMdxBackground_ = nullptr;

    int width_ = 0;
    int height_ = 0;
    int physicalWidth_ = 0;
    int physicalHeight_ = 0;
    float dpiScale_ = 1.0f;
    bool guiInitialized_ = false;
    bool initialized_ = false;
    bool layoutLogged_ = false;
    bool renderTreeReady_ = false;
    bool hasLastUpdateTime_ = false;
    double lastUpdateTime_ = 0.0;

    int gold_ = 0;
    int lumber_ = 0;
    int foodUsed_ = 0;
    int foodCap_ = 0;
    std::string upkeepValue_ = "Upkeep NYI";
    std::string timeValue_ = "12:00";

    std::string unitName_ = "No unit selected";
    int unitLevel_ = 0;
    float hp_ = 0.0f;
    float maxHp_ = 0.0f;
    float mp_ = 0.0f;
    float maxMp_ = 0.0f;
    bool heroVisible_ = false;
    int heroXp_ = 0;
    int heroMaxXp_ = 0;
    int heroStrBase_ = 0;
    int heroStrBonus_ = 0;
    int heroAgiBase_ = 0;
    int heroAgiBonus_ = 0;
    int heroIntBase_ = 0;
    int heroIntBonus_ = 0;
    HudInfoStats infoStats_;

    bool tooltipVisible_ = false;
    std::string tooltipValue_;
    HudResourceCosts tooltipCosts_;
    float tooltipX_ = 0.0f;
    float tooltipY_ = 0.0f;

    bool chatOpen_ = false;
    std::string chatJoinedText_;
    std::string chatInputText_;

    struct CommandState {
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
    std::array<CommandState, 12> commandStates_{};
    std::array<std::string, 12> commandAppliedIconUris_{};
    std::array<ScreenRect, 12> commandRects_{};
    int hoveredCommandIndex_ = -1;
    int pressedCommandIndex_ = -1;
    std::array<ScreenRect, 4> upperButtonRects_{};
    int hoveredUpperButtonIndex_ = -1;
    int pressedUpperButtonIndex_ = -1;
    std::array<ScreenRect, 5> minimapButtonRects_{};
    int pressedMinimapButtonIndex_ = -1;
    std::array<int, 5> minimapButtonModes_{};
    std::array<ScreenRect, kGameMenuActionCount> gameMenuButtonRects_{};
    bool gameMenuOpen_ = false;
    GameMenuPanel gameMenuPanel_ = GameMenuPanel::Main;
    bool gameMenuPaused_ = false;
    bool gameMenuOptionStateInitialized_ = false;
    std::array<bool, kGameMenuActionCount> gameMenuOptionCheckedState_{};
    std::array<int, kGameMenuActionCount> gameMenuOptionCycleIndex_{};
    int gameMenuOptionPopupOpenAction_ = -1;
    int hoveredGameMenuAction_ = -1;
    int pressedGameMenuAction_ = -1;
    std::array<MultiSelectUnitInfo, 12> multiSelectStates_{};
    std::array<bool, 12> multiSelectVisible_{};
    std::array<ScreenRect, 12> multiSelectRects_{};
    int multiSelectPrimaryHandle_ = -1;

    std::array<HudQueueItem, 5> queueStates_{};
    std::array<ScreenRect, 5> queueRects_{};
    std::array<std::string, 5> queueAppliedIconUris_{};
    float queueProgressRatio_ = 0.0f;

    std::array<HudInventoryItem, 6> inventoryStates_{};
    std::array<ScreenRect, 6> inventoryRects_{};
    std::array<std::string, 6> inventoryAppliedIconUris_{};
    bool inventoryAvailable_ = false;
    int inventoryCapacity_ = 0;
    std::string portraitIconUri_;
    std::string portraitAppliedIconUri_;
    std::array<HudBuffItem, 8> buffStates_{};
    std::array<std::string, 8> buffAppliedIconUris_{};
    int buffCount_ = 0;

    ScreenRect minimapRect_;
    ScreenRect portraitRect_;
};
