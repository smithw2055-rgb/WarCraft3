#pragma once
// ConsoleHUD: FDF-driven in-game console, faithful port of warsmash_ref
// MeleeUI.java console construction (UI\\FrameDef\\FrameDef.toc +
// UI\\FrameDef\\SmashFrameDef.toc, then code-owned command/inventory/
// queue/multiselect/buff grids).
//
// Coordinate convention mirrors GameUI.convertX/convertY:
//   px = (fdfX / 0.8) * screenW, py = (fdfY / 0.6) * screenH
// All offsets passed to AddSetPoint/AddAnchor are already converted to
// pixels, exactly like the Java call sites.
#include <array>
#include <map>
#include <string>
#include <vector>

class GameUI;
class FrameBuilder;
class TextRenderer;
class UIFrame;
class SimpleFrame;
class TextFrame;
class ButtonFrame;
class FrameDefinition;

struct ConsoleHudRefs {
    SimpleFrame* consoleUI = nullptr;
    SimpleFrame* resourceBar = nullptr;
    SimpleFrame* upperButtonBar = nullptr;
    SimpleFrame* smashSimpleInfoPanel = nullptr;
    SimpleFrame* simpleInfoPanelUnitDetail = nullptr;
    UIFrame* simpleInfoPanelBuildingDetail = nullptr;
    UIFrame* simpleInfoPanelItemDetail = nullptr;
    UIFrame* simpleInfoPanelDestructableDetail = nullptr;
    SimpleFrame* inventoryBarFrame = nullptr;
    SimpleFrame* smashCommandButtons = nullptr;
    UIFrame* tooltipFrame = nullptr;
    UIFrame* hovertipFrame = nullptr;
    TextFrame* tooltipText = nullptr;
    TextFrame* uberTipText = nullptr;
    TextFrame* hoverTipText = nullptr;
    UIFrame* timeIndicator = nullptr;
    // Retail in-game dialogs (ref MeleeUI Esc/Quest/Chat/Alliance/Timer/
    // Script/Cinematic panels from FrameDef.toc). Inflated when the template
    // exists, hidden until opened. Geometry comes from FDF; centering is
    // applied on open by FdfHudHost.
    UIFrame* questDialog = nullptr;
    UIFrame* escMenuBackdrop = nullptr;
    UIFrame* escMenuMainPanel = nullptr;
    UIFrame* chatDialog = nullptr;
    UIFrame* allianceDialog = nullptr;
    UIFrame* cinematicPanel = nullptr;
    UIFrame* timerDialog = nullptr;
    UIFrame* scriptDialog = nullptr;
    UIFrame* logDialog = nullptr;
    TextFrame* cinematicSpeakerText = nullptr;
    TextFrame* cinematicDialogueText = nullptr;
    TextFrame* timerTitleText = nullptr;
    TextFrame* timerValueText = nullptr;
    TextFrame* scriptText = nullptr;
    SimpleFrame* unitPortrait = nullptr;
    TextFrame* unitPortraitHitPointText = nullptr;
    TextFrame* unitPortraitManaPointText = nullptr;
    TextFrame* errorMessageFrame = nullptr;
    TextFrame* inventoryTitleFrame = nullptr;

    UIFrame* attack1IconBackdrop = nullptr;
    TextFrame* attack1IconLabel = nullptr;
    TextFrame* attack1IconValue = nullptr;
    UIFrame* armorIconBackdrop = nullptr;
    TextFrame* armorIconLabel = nullptr;
    TextFrame* armorIconValue = nullptr;

    // Code-owned grids (ref MeleeUI.commandCard/inventoryIcons/
    // queueIconFrames/selectedUnitFrames/cargoUnitFrames/buffBarIcons).
    std::array<UIFrame*, 12> commandButtons{};
    std::array<UIFrame*, 6> inventoryButtons{};
    std::array<UIFrame*, 5> queueButtons{};
    UIFrame* queueWorkerButton = nullptr;
    std::array<UIFrame*, 10> buffButtons{};
    UIFrame* buffStatusBar = nullptr;
    std::array<UIFrame*, 12> multiSelectButtons{};
    std::array<UIFrame*, 12> cargoButtons{};
    std::array<UIFrame*, 4> upperButtons{};
    UIFrame* minimap = nullptr;
    std::array<UIFrame*, 5> minimapButtons{};

    float frontQueueIconWidth = 0.0f;
    float infoPanelWidth = 0.0f;
    float infoPanelHeight = 0.0f;
    // True when the retail template is absent and Layout must fill an
    // explicit rect (ref Java FDF geometry for these two frames).
    bool proceduralUnitDetail = false;
    bool proceduralPortrait = false;
};

class ConsoleHUD {
public:
    static constexpr float kCommandIconWidth = 0.039f;
    static constexpr float kInventoryIconWidth = 0.03125f;
    static constexpr int kCommandCols = 4;
    static constexpr int kCommandRows = 3;
    static constexpr int kInventoryCols = 2;
    static constexpr int kInventoryRows = 3;
    static constexpr int kQueueCount = 5;
    static constexpr int kBuffMax = 10;
    static constexpr int kMultiSelectMax = 12;

    static float ConvertX(int screenW, float fdfX) { return (fdfX / 0.8f) * (float)screenW; }
    static float ConvertY(int screenH, float fdfY) { return (fdfY / 0.6f) * (float)screenH; }

    // Build the full console under `root`. `templates` is the merged
    // retail + Smash template map. Frames inflated from FDF keep their
    // retail layout; code-owned grids get Java-accurate anchors/sizes.
    // Newly allocated code-owned frames are appended to `ownedOut`
    // (caller wraps them in unique_ptr); FDF-inflated frames stay owned
    // by the caller via FrameBuilder exactly as before.
    static bool Build(GameUI& gui, FrameBuilder* builder, TextRenderer* textRenderer,
                      std::map<std::string, FrameDefinition>& templates,
                      SimpleFrame* root, int screenW, int screenH,
                      std::vector<UIFrame*>& ownedOut, ConsoleHudRefs& refs);

    // Re-apply pixel sizes after a resize (anchors are in pixels, so the
    // code-owned grids must be re-laid out; FDF frames reposition via
    // PositionBounds by the caller).
    static void Layout(GameUI& gui, ConsoleHudRefs& refs, int screenW, int screenH);

private:
    static UIFrame* CreateSimpleFrame(GameUI& gui, FrameBuilder* builder,
                                      std::map<std::string, FrameDefinition>& templates,
                                      const std::string& name, UIFrame* parent,
                                      std::vector<UIFrame*>& ownedOut);
    static UIFrame* CreateFrame(GameUI& gui, FrameBuilder* builder,
                                std::map<std::string, FrameDefinition>& templates,
                                const std::string& name, UIFrame* parent,
                                std::vector<UIFrame*>& ownedOut);
    static TextFrame* CreateStringFrame(GameUI& gui, const std::string& name, UIFrame* parent,
                                        float fdfFontSize, int screenH, TextRenderer* textRenderer,
                                        std::vector<UIFrame*>& ownedOut);
    static ButtonFrame* CreateButton(GameUI& gui, const std::string& name, UIFrame* parent,
                                     TextRenderer* textRenderer, std::vector<UIFrame*>& ownedOut);
    static UIFrame* Find(GameUI& gui, FrameBuilder* builder, const std::string& name);
    static void BuildCommandGrid(GameUI& gui, ConsoleHudRefs& refs, TextRenderer* textRenderer,
                                 int screenW, int screenH, std::vector<UIFrame*>& ownedOut);
    static void BuildInventoryBar(GameUI& gui, ConsoleHudRefs& refs, TextRenderer* textRenderer,
                                  int screenW, int screenH, std::vector<UIFrame*>& ownedOut);
    static void BuildQueueAndMultiSelect(GameUI& gui, ConsoleHudRefs& refs, TextRenderer* textRenderer,
                                         int screenW, int screenH, std::vector<UIFrame*>& ownedOut);
    static void BuildBuffBar(GameUI& gui, ConsoleHudRefs& refs, TextRenderer* textRenderer,
                             int screenW, int screenH, std::vector<UIFrame*>& ownedOut);
    static void BuildInfoWrappers(GameUI& gui, FrameBuilder* builder,
                                  std::map<std::string, FrameDefinition>& templates,
                                  ConsoleHudRefs& refs, TextRenderer* textRenderer,
                                  int screenW, int screenH, std::vector<UIFrame*>& ownedOut);
};
