#pragma once
// FDF-backed HUD host (replaces the former AeroGUI/Noesis XAML host).
//
// Same public contract as the removed NoesisHudHost so MainUI is untouched
// apart from the type name: layout comes from retail FDF
// (UI\FrameDef\FrameDef.toc via FrameBuilder, root "ConsoleUI") with a
// procedural classic-console fallback when no MPQ data is present.
// Rendering goes through UiRenderer + TextRenderer (sokol GL); glue menus
// and the in-game menu are FDF/procedural frames with retail-compatible
// debug action names.
#include "MainUI.h"
#include "ConsoleHUD.h"
#include "FrameBuilder.h"
#include "GlueMenuTypes.h"
#include "TextRenderer.h"
#include "UIFrame.h"
#include <array>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct sapp_event;
class MpqManager;
class Texture;

class FdfHudHost {
public:
    FdfHudHost() = default;
    ~FdfHudHost();

    bool Initialize(const std::string& assetRoot, int width, int height, const std::string& layoutPath,
                    MpqManager* mpq = nullptr, int gameVersion = 0);
    bool ReloadLayout(const std::string& layoutPath, MpqManager* mpq = nullptr, int gameVersion = 0);
    void Shutdown();
    void Resize(int width, int height);
    void Update(double time);
    void RenderOffscreen();
    void Render();
    bool HandleEvent(const sapp_event* e);

    void SetResources(int gold, int lumber, int foodUsed, int foodCap, const std::string& timeText);
    // Upkeep indicator (retail ResourceBarUpkeepText): 0 none, 1 low, 2 high.
    void SetUpkeepLevel(int level);
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
    // Upper buttons open retail dialogs (ref UpperButtonBar.fdf): 0 Quests,
    // 1 Menu, 2 Allies, 3 Chat. Java disables Quests/Allies/Chat, but the
    // retail FDF panels are fully inflated here for display/render parity.
    void HandleUpperButtonAction(int index);
    bool IsQuestDialogOpen() const { return questOpen_; }
    void OpenQuestDialog();
    void CloseQuestDialog();
    void ToggleQuestDialog();
    bool IsChatDialogOpen() const { return chatDialogOpen_; }
    void OpenChatDialog();
    void CloseChatDialog();
    bool IsAllianceDialogOpen() const { return allianceDialogOpen_; }
    void OpenAllianceDialog();
    void CloseAllianceDialog();
    bool AnyModalDialogOpen() const;
    void CloseAllDialogs();
    // Modal dialog click routing: consumes the click when a Quest/Chat/
    // Alliance dialog is open (Accept/Cancel buttons close it).
    bool HandleModalClick(float x, float y);
    // Cinematic / timer / script dialogs (ref MeleeUI cinematicPanel,
    // timerDialogs, scriptDialog): FDF-driven visibility + text binding.
    void ShowCinematic(const std::string& speaker, const std::string& dialogue);
    void HideCinematic();
    // Cinematic mode letterbox (ref SetCinematicMode black bars).
    void SetCinematicMode(bool v) { cinematicMode_ = v; }
    // Cinematic filter overlay (ref CCineFilter): fullscreen tint under the
    // HUD chrome; rgba is 0xAARRGGBB, alpha 0 hides.
    void SetCineFilter(uint32_t rgba, bool visible) {
        cineFilterColor_ = rgba;
        cineFilterVisible_ = visible;
    }
    void ShowTimerDialog(const std::string& title, const std::string& value);
    void HideTimerDialog();
    void ShowScriptDialog(const std::string& text);
    void HideScriptDialog();
    // World-frame messages with Java fade timing (ref showGameMessage /
    // errorMessage expire+fadeout millis).
    void ShowErrorMessage(const std::string& text);
    void ShowGameMessage(const std::string& text, float expireSeconds = 5.0f);
    void ClearGameMessages() { gameMessages_.clear(); }
    void SetPressedMinimapButton(int index);
    void ActivateMinimapButton(int index);
    void SetMultiSelection(const std::vector<MultiSelectUnitInfo>& units, int primaryHandle);
    void SetQueue(const std::vector<HudQueueItem>& items, float progressRatio);
    void SetInventory(const std::vector<HudInventoryItem>& items, int capacity, bool available);
    void SetPortraitIcon(const std::string& iconUri);
    // 3D portrait viewport state (P3-21): model resolved from the selected
    // unit type; the render pass draws it when the portrait pipeline lands.
    void SetPortraitModel(const std::string& modelPath) { portraitModelPath_ = modelPath; }
    const std::string& GetPortraitModel() const { return portraitModelPath_; }
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
    bool IsGlueMenuActive() const { return glueActive_; }
    int HitGlueMenuButton(float x, float y) const;
    void SetHoveredGlueMenuButton(int action);
    void SetPressedGlueMenuButton(int action);
    void HandleGlueMenuAction(int action);
    void OpenGlueMenuDebugPage(const std::string& pageName);
    bool InvokeGlueMenuActionForDebug(const std::string& actionName);
    void SetGlueMenuMaps(const std::vector<War3UI::GlueMapListItem>& maps);
    void SetGlueMenuCampaigns(const std::vector<War3UI::GlueCampaignListItem>& campaigns);
    void SetGlueMapLaunchCallback(War3UI::GlueMapLaunchCallback cb);

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
    // ---- layout (ref MeleeUI.java console construction) ----
    bool LoadRetailConsole();
    bool LoadConsoleTemplates(std::map<std::string, FrameDefinition>& templates);
    void BuildFallbackConsole();
    void BuildDynamicGrids(bool withStaticText);
    void LayoutDynamicGrids();
    void CacheConsoleRects();
    UIFrame* FindFrame(const std::string& name) const;
    UiRect FrameBounds(const std::string& name, const UiRect& fallback) const;
    // Write into a retail TextFrame when present (true); else the caller
    // draws directly at the fallback position.
    bool BindText(const std::string& name, const std::string& value);
    // Set a TextureFrame from the first matching name (true when bound).
    bool BindIcon(std::initializer_list<const char*> names, const std::shared_ptr<Texture>& tex);
    void LayoutFallbackConsole();
    // Release owned frames exactly once. Code-owned console frames live both
    // in ownedFrames_ and in their parent's children_ list (ref
    // ConsoleHUD::ParentTo -> SimpleFrame::AddChild, whose dtor deletes
    // children), so the child links must be detached before the vector
    // destroys its elements to avoid double-free on shutdown/reload.
    void ClearOwnedFrames();
    static std::string NormalizeLayoutPath(const std::string& p);

    // ---- textures ----
    std::shared_ptr<Texture> ResolveIcon(const std::string& uri);
    static std::string IconToMpqPath(const std::string& uri);

    // ---- drawing ----
    void DrawConsoleChrome();
    void DrawTexts();
    void DrawIcons();
    void DrawTooltip();
    void DrawHoverTip();
    void DrawChat();
    void DrawDialogMessages();
    void LayoutTooltipFrame();
    void LayoutDialogFrame(UIFrame* dialog);
    void SetDialogVisible(UIFrame* dialog, bool visible);
    void DrawGameMenu();
    void DrawGlueMenu();
    void DrawButtonList(const std::vector<std::pair<int, std::string>>& items,
                        float x, float y, float w, float rowH,
                        int hovered, int pressed,
                        std::vector<UiRect>& rects);
    void DrawText(float x, float y, const std::string& text, uint32_t color, float size);

    // ---- game menu ----
    enum class GameMenuPanel : uint8_t {        Main, EndGame, ConfirmQuit, SaveGame, LoadGame, Help, Tips,
        Options, OptionsGameplay, OptionsVideo, OptionsSound, OptionsNetwork,
    };
    static constexpr int kGameMenuActionCount = 70;
    static const char* GameMenuActionName(int action);
    static int GameMenuActionForName(const std::string& name);
    void SetGameMenuPanel(GameMenuPanel panel);
    bool IsGameMenuActionEnabled(int action) const;
    std::vector<std::pair<int, std::string>> CurrentGameMenuItems() const;
    mutable std::vector<UiRect> gameMenuRects_;
    // Textured menu chrome (ref EscMenuBackdrop/EscMenuButtonBackdropTemplate
    // in retail FDF): standalone backdrops rendered manually at the
    // procedural panel/row rects, never parented into the frame tree.
    BackdropFrame* menuPanelBackdrop_ = nullptr;
    BackdropFrame* menuButtonBackdrop_ = nullptr;
    BackdropFrame* menuButtonPushedBackdrop_ = nullptr;
    BackdropFrame* MakeMenuBackdrop(const std::string& name, const std::string& bgKey,
                                    const std::string& edgeKey, float cornerFdf,
                                    float bgSizeFdf, float insetFdf);

    // ---- glue menu ----
    enum class GluePage : uint8_t {
        MainMenu, BattleNet, LocalAreaNetwork, SinglePlayer, Campaign,
        MissionSelect, CustomGame, Options,
    };
    static constexpr int kGlueActionCount = 92;
    static const char* GlueActionName(int action);
    static int GlueActionForName(const std::string& name);
    void OpenGluePage(GluePage page);
    bool IsGlueActionVisible(int action) const;
    std::vector<std::pair<int, std::string>> CurrentGlueItems() const;
    void GlueSelectCampaign(int index);
    mutable std::vector<UiRect> glueRects_;

    std::string assetRoot_;
    MpqManager* mpq_ = nullptr;
    int gameVersion_ = 0;

    GameUI gui_;
    FrameBuilder* builder_ = nullptr;
    TextRenderer textRenderer_;
    std::vector<std::unique_ptr<UIFrame>> ownedFrames_;
    SimpleFrame* root_ = nullptr;
    bool usingRetailLayout_ = false;
    bool initialized_ = false;
    int width_ = 0;
    int height_ = 0;
    // Java-faithful console refs (ref MeleeUI consoleUI/resourceBar/
    // upperButtonBar/infoPanel/command/inventory/queue/multi/buff).
    ConsoleHudRefs consoleRefs_;
    std::map<std::string, FrameDefinition> consoleTemplates_;
    std::map<std::string, std::string> consoleDecoratedStrings_;

    std::unordered_map<std::string, std::shared_ptr<Texture>> textures_;

    // ---- mirrored HUD state (same shape as the old host) ----
    int gold_ = 0, lumber_ = 0, foodUsed_ = 0, foodCap_ = 0;
    int upkeepLevel_ = 0; // 0 none, 1 low, 2 high (retail upkeep indicator)
    std::string timeValue_ = "12:00";
    std::string unitName_ = "";
    int unitLevel_ = 0;
    float hp_ = 0, maxHp_ = 0, mp_ = 0, maxMp_ = 0;
    bool heroVisible_ = false;
    int heroXp_ = 0, heroMaxXp_ = 0;
    int heroStrBase_ = 0, heroStrBonus_ = 0, heroAgiBase_ = 0, heroAgiBonus_ = 0;
    int heroIntBase_ = 0, heroIntBonus_ = 0;
    HudInfoStats infoStats_;
    bool tooltipVisible_ = false;
    std::string tooltipValue_;
    HudResourceCosts tooltipCosts_;
    float tooltipX_ = 0, tooltipY_ = 0;
    bool chatOpen_ = false;
    std::string chatInputText_;
    std::vector<std::string> chatMessages_;
    std::array<HudCommandSlot, 12> commandStates_{};
    int hoveredCommandIndex_ = -1, pressedCommandIndex_ = -1;
    std::array<ScreenRect, 12> commandRects_{};
    std::array<ScreenRect, 4> upperButtonRects_{};
    int hoveredUpperButtonIndex_ = -1, pressedUpperButtonIndex_ = -1;
    std::array<ScreenRect, 5> minimapButtonRects_{};
    int pressedMinimapButtonIndex_ = -1;
    std::array<int, 5> minimapButtonModes_{};
    std::array<HudQueueItem, 5> queueStates_{};
    std::array<ScreenRect, 5> queueRects_{};
    float queueProgressRatio_ = 0;
    std::array<HudInventoryItem, 6> inventoryStates_{};
    std::array<ScreenRect, 6> inventoryRects_{};
    bool inventoryAvailable_ = false;
    int inventoryCapacity_ = 0;
    std::string portraitIconUri_;
    std::string portraitModelPath_;
    std::array<HudBuffItem, 8> buffStates_{};
    int buffCount_ = 0;
    std::array<MultiSelectUnitInfo, 12> multiSelectStates_{};
    std::array<bool, 12> multiSelectVisible_{};
    std::array<ScreenRect, 12> multiSelectRects_{};
    int multiSelectPrimaryHandle_ = -1;
    ScreenRect minimapRect_;
    ScreenRect portraitRect_;

    // ---- game menu state ----
    bool gameMenuOpen_ = false;
    // In-game dialog state (Quest/Chat/Alliance/Cinematic/Timer/Script).
    bool questOpen_ = false;
    bool chatDialogOpen_ = false;
    bool allianceDialogOpen_ = false;
    bool cinematicVisible_ = false;
    bool cinematicMode_ = false;
    uint32_t cineFilterColor_ = 0;
    bool cineFilterVisible_ = false;
    bool timerVisible_ = false;
    bool scriptVisible_ = false;
    // Error + game messages with expire/fade (ref MeleeUI
    // WORLD_FRAME_MESSAGE_EXPIRE/FADEOUT_MILLIS).
    struct FadingMessage {
        std::string text;
        double expireAt = 0.0;
        double fadeAt = 0.0;
    };
    FadingMessage errorMessage_;
    std::vector<FadingMessage> gameMessages_;
    double uiClock_ = 0.0;
    GameMenuPanel gameMenuPanel_ = GameMenuPanel::Main;
    int hoveredGameMenuAction_ = -1, pressedGameMenuAction_ = -1;
    std::array<bool, kGameMenuActionCount> gameMenuOptionChecked_{};
    std::array<int, kGameMenuActionCount> gameMenuOptionCycle_{};

    // ---- glue state ----
    bool glueActive_ = false;
    GluePage gluePage_ = GluePage::MainMenu;
    bool glueQuitConfirmOpen_ = false;
    bool glueProfileOpen_ = false;
    int hoveredGlueAction_ = -1, pressedGlueAction_ = -1;
    std::vector<War3UI::GlueMapListItem> glueMaps_;
    std::vector<War3UI::GlueCampaignListItem> glueCampaigns_;
    War3UI::GlueMapLaunchCallback glueMapLaunchCallback_;
    int glueSelectedMap_ = 0, glueMapOffset_ = 0;
    int glueSelectedCampaign_ = 0, glueSelectedMission_ = 0;
    int glueSelectedOptionsTab_ = -1;
    bool glueMapInfoOpen_ = false, glueAdvancedOpen_ = false;
    bool glueTeamsTogether_ = false, glueFixedTeams_ = false, glueFullShare_ = false;
};
