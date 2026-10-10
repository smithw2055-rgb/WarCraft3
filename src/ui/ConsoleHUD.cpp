// ConsoleHUD: Java MeleeUI console construction ported to the C++ FDF stack.
// Ref: warsmash_ref/core/src/com/etheller/warsmash/viewer5/handlers/w3x/ui/MeleeUI.java
//   (main(): ConsoleUI -> ResourceBar -> UpperButtonBar -> TimeOfDayIndicator ->
//    UnitPortrait -> SmashSimpleInfoPanel -> SimpleInfoPanel{Unit,Building,Item,
//    Destructable}Detail -> queue icons -> multiselect/cargo -> attack/armor/hero
//    wrappers -> SmashSimpleInventoryBar -> inventory cover -> error message ->
//    SmashCommandButtons -> SmashToolTip/HoverTip -> cinematic/cursor/timer).
//
// The renderer works in y-down pixels while FDF/Java use an up-positive
// 0..0.8 x 0..0.6 space. Retail FDF frames are converted + positioned by the
// FDF engine (FrameBuilder/UIFrame, ref GameUI.inflate). The code-owned grids
// below are positioned here with explicit y-down rects mirroring the Java
// anchor math: screenX = ConvertX(fdfX),
// screenY(edge) = H - ConvertY(fdfY) - size.
#include "ConsoleHUD.h"
#include "BackdropFrame.h"
#include "ButtonFrame.h"
#include "ExtendedFrames.h"
#include "FrameBuilder.h"
#include "SimpleFrame.h"
#include "TextFrame.h"
#include "TextureFrame.h"
#include "UIFrame.h"
#include <algorithm>

namespace {

void ParentTo(GameUI& gui, UIFrame* parent, UIFrame* child) {
    if (!parent || !child) return;
    if (auto* s = dynamic_cast<SimpleFrame*>(parent)) s->AddChild(child);
    child->SetParent(parent);
    if (gui.frameRegistry.count(child->GetName()) == 0)
        gui.frameRegistry[child->GetName()] = child;
}

void Place(UIFrame* f, float x, float y, float w, float h) {
    if (!f) return;
    f->SetPosition(x, y);
    f->SetSize(w, h);
}

} // namespace

UIFrame* ConsoleHUD::Find(GameUI& gui, FrameBuilder* builder, const std::string& name) {
    if (builder) {
        if (UIFrame* f = builder->GetFrame(name)) return f;
    }
    auto it = gui.frameRegistry.find(name);
    return it != gui.frameRegistry.end() ? it->second : nullptr;
}

UIFrame* ConsoleHUD::CreateSimpleFrame(GameUI& gui, FrameBuilder* builder,
                                       std::map<std::string, FrameDefinition>& templates,
                                       const std::string& name, UIFrame* parent,
                                       std::vector<UIFrame*>& ownedOut) {
    if (UIFrame* existing = Find(gui, builder, name)) return existing;
    auto it = templates.find(name);
    UIFrame* frame = nullptr;
    if (it != templates.end() && builder) {
        frame = builder->ConstructFrame(it->second, parent ? parent : gui.rootFrame);
        return frame ? frame : Find(gui, builder, name);
    }
    // No template (Smash-only frame without FDF loaded, or headless test):
    // procedural stand-in parented like GameUI.createSimpleFrame fallback.
    auto* simple = new SimpleFrame(name, nullptr);
    ParentTo(gui, parent ? parent : gui.rootFrame, simple);
    ownedOut.push_back(simple);
    return simple;
}

UIFrame* ConsoleHUD::CreateFrame(GameUI& gui, FrameBuilder* builder,
                                 std::map<std::string, FrameDefinition>& templates,
                                 const std::string& name, UIFrame* parent,
                                 std::vector<UIFrame*>& ownedOut) {
    // Same as CreateSimpleFrame for our stack: template inflate when
    // available, otherwise a procedural SimpleFrame.
    return CreateSimpleFrame(gui, builder, templates, name, parent, ownedOut);
}

TextFrame* ConsoleHUD::CreateStringFrame(GameUI& gui, const std::string& name, UIFrame* parent,
                                         float fdfFontSize, int screenH, TextRenderer* textRenderer,
                                         std::vector<UIFrame*>& ownedOut) {
    if (auto it = gui.frameRegistry.find(name); it != gui.frameRegistry.end()) {
        if (auto* t = dynamic_cast<TextFrame*>(it->second)) return t;
    }
    auto* text = new TextFrame(name, nullptr);
    text->SetTextRenderer(textRenderer);
    text->SetFontSize(ConvertY(screenH, fdfFontSize));
    ParentTo(gui, parent, text);
    ownedOut.push_back(text);
    return text;
}

ButtonFrame* ConsoleHUD::CreateButton(GameUI& gui, const std::string& name, UIFrame* parent,
                                      TextRenderer* textRenderer, std::vector<UIFrame*>& ownedOut) {
    if (auto it = gui.frameRegistry.find(name); it != gui.frameRegistry.end()) {
        if (auto* b = dynamic_cast<ButtonFrame*>(it->second)) return b;
    }
    auto* btn = new ButtonFrame(name, nullptr);
    btn->SetTextRenderer(textRenderer);
    ParentTo(gui, parent, btn);
    ownedOut.push_back(btn);
    return btn;
}

bool ConsoleHUD::Build(GameUI& gui, FrameBuilder* builder, TextRenderer* textRenderer,
                       std::map<std::string, FrameDefinition>& templates,
                       SimpleFrame* root, int screenW, int screenH,
                       std::vector<UIFrame*>& ownedOut, ConsoleHudRefs& refs) {
    if (!root) return false;
    UIFrame* rootParent = root;

    // ---- ConsoleUI background (racial theme chrome) ----
    refs.consoleUI = dynamic_cast<SimpleFrame*>(
        CreateSimpleFrame(gui, builder, templates, "ConsoleUI", rootParent, ownedOut));
    if (!refs.consoleUI) return false;
    refs.consoleUI->SetAllPoints();

    // ---- ResourceBarFrame at TOPRIGHT (template has no position) ----
    refs.resourceBar = dynamic_cast<SimpleFrame*>(
        CreateSimpleFrame(gui, builder, templates, "ResourceBarFrame", refs.consoleUI, ownedOut));
    if (refs.resourceBar) {
        refs.resourceBar->AddSetPoint(FramePoint::TOPRIGHT, "ConsoleUI", FramePoint::TOPRIGHT, 0, 0);
    }

    // ---- UpperButtonBarFrame at TOPLEFT ----
    refs.upperButtonBar = dynamic_cast<SimpleFrame*>(
        CreateSimpleFrame(gui, builder, templates, "UpperButtonBarFrame", refs.consoleUI, ownedOut));
    if (refs.upperButtonBar) {
        refs.upperButtonBar->AddSetPoint(FramePoint::TOPLEFT, "ConsoleUI", FramePoint::TOPLEFT, 0, 0);
    }
    // Retail upper buttons come from the UpperButtonBarFrame template itself
    // (ref MeleeUI getFrameByName UpperButtonBar{Quests,Menu,Allies,Chat}Button).
    static const char* kRetailUppers[] = {
        "UpperButtonBarQuestsButton", "UpperButtonBarMenuButton",
        "UpperButtonBarAlliesButton", "UpperButtonBarChatButton",
    };
    for (int i = 0; i < 4; ++i) {
        UIFrame* btn = Find(gui, builder, kRetailUppers[i]);
        static const char* kLabels[4] = {"Quests", "Menu (F10)", "Allies", "Chat"};
        if (!btn && refs.upperButtonBar) {
            // Procedural stand-in only when the retail template is absent.
            static const char* kNames[4] = {"QuestsButton", "MenuButton", "AlliesButton", "ChatButton"};
            ButtonFrame* fallback = CreateButton(gui, kNames[i], refs.upperButtonBar, textRenderer, ownedOut);
            fallback->SetText(kLabels[i]);
            fallback->SetEnabled(true);
            btn = fallback;
        } else if (auto* bf = dynamic_cast<ButtonFrame*>(btn)) {
            if (textRenderer) bf->SetTextRenderer(textRenderer);
            bf->SetText(kLabels[i]);
            bf->SetFontSize(11.0f);
            bf->SetTextColor(0xFFB0C8D8);
            bf->SetEnabled(true);
        }
        refs.upperButtons[i] = btn;
    }

    // ---- TimeOfDayIndicator clock (screen-space, parented to root) ----
    refs.timeIndicator = CreateFrame(gui, builder, templates, "TimeOfDayIndicator", rootParent, ownedOut);

    // ---- UnitPortrait (FDF-anchored; procedural fallback positioned in Layout) ----
    refs.unitPortrait = dynamic_cast<SimpleFrame*>(
        CreateSimpleFrame(gui, builder, templates, "UnitPortrait", refs.consoleUI, ownedOut));
    refs.proceduralPortrait = templates.count("UnitPortrait") == 0;

    // Unit portrait HP/MP texts (centered directly beneath the portrait arch)
    refs.unitPortraitHitPointText = CreateStringFrame(gui, "UnitPortraitHitPointText", rootParent,
                                                      0.011f, screenH, textRenderer, ownedOut);
    if (refs.unitPortraitHitPointText) {
        refs.unitPortraitHitPointText->SetAlign(TextAlign::CENTER);
        refs.unitPortraitHitPointText->SetColor(0xFF00FF00);
        refs.unitPortraitHitPointText->SetDefaultToParent(false);
        refs.unitPortraitHitPointText->SetVisible(false);
    }
    refs.unitPortraitManaPointText = CreateStringFrame(gui, "UnitPortraitManaPointText", rootParent,
                                                       0.011f, screenH, textRenderer, ownedOut);
    if (refs.unitPortraitManaPointText) {
        refs.unitPortraitManaPointText->SetAlign(TextAlign::CENTER);
        refs.unitPortraitManaPointText->SetColor(0xFF60A0FF);
        refs.unitPortraitManaPointText->SetDefaultToParent(false);
        refs.unitPortraitManaPointText->SetVisible(false);
    }

    // ---- SmashSimpleInfoPanel (always code-owned; explicit rect in Layout) ----
    refs.smashSimpleInfoPanel = dynamic_cast<SimpleFrame*>(
        CreateSimpleFrame(gui, builder, templates, "SmashSimpleInfoPanel", rootParent, ownedOut));
    if (!refs.smashSimpleInfoPanel) {
        auto* panel = new SimpleFrame("SmashSimpleInfoPanel", nullptr);
        ParentTo(gui, rootParent, panel);
        ownedOut.push_back(panel);
        refs.smashSimpleInfoPanel = panel;
    }

    // ---- SimpleInfoPanel*Detail children (retail templates) ----
    refs.simpleInfoPanelUnitDetail = dynamic_cast<SimpleFrame*>(
        CreateSimpleFrame(gui, builder, templates, "SimpleInfoPanelUnitDetail",
                          refs.smashSimpleInfoPanel, ownedOut));
    refs.proceduralUnitDetail = templates.count("SimpleInfoPanelUnitDetail") == 0;
    refs.simpleInfoPanelBuildingDetail =
        CreateSimpleFrame(gui, builder, templates, "SimpleInfoPanelBuildingDetail",
                          refs.smashSimpleInfoPanel, ownedOut);
    if (refs.simpleInfoPanelBuildingDetail) refs.simpleInfoPanelBuildingDetail->SetVisible(false);
    refs.simpleInfoPanelItemDetail =
        CreateSimpleFrame(gui, builder, templates, "SimpleInfoPanelItemDetail",
                          refs.smashSimpleInfoPanel, ownedOut);
    if (refs.simpleInfoPanelItemDetail) refs.simpleInfoPanelItemDetail->SetVisible(false);
    refs.simpleInfoPanelDestructableDetail =
        CreateSimpleFrame(gui, builder, templates, "SimpleInfoPanelDestructableDetail",
                          refs.smashSimpleInfoPanel, ownedOut);
    if (refs.simpleInfoPanelDestructableDetail) refs.simpleInfoPanelDestructableDetail->SetVisible(false);

    BuildBuffBar(gui, refs, textRenderer, screenW, screenH, ownedOut);
    BuildQueueAndMultiSelect(gui, refs, textRenderer, screenW, screenH, ownedOut);
    BuildInfoWrappers(gui, builder, templates, refs, textRenderer, screenW, screenH, ownedOut);
    BuildInventoryBar(gui, refs, textRenderer, screenW, screenH, ownedOut);
    BuildCommandGrid(gui, refs, textRenderer, screenW, screenH, ownedOut);

    // ---- SmashConsoleInventoryCover (racial cover over empty slots) ----
    CreateSimpleFrame(gui, builder, templates, "SmashConsoleInventoryCover", rootParent, ownedOut);

    // ---- SmashErrorMessageFrame (explicit rect in Layout; hidden) ----
    refs.errorMessageFrame = CreateStringFrame(gui, "SmashErrorMessageFrame", rootParent,
                                               0.012f, screenH, textRenderer, ownedOut);
    refs.errorMessageFrame->SetVisible(false);

    // ---- SmashToolTip / HoverTip (FDF geometry; hidden until hover) ----
    // Ref MeleeUI tooltipFrame/hovertipFrame + SmashToolTipText/UberTip.
    refs.tooltipFrame = CreateFrame(gui, builder, templates, "SmashToolTip", rootParent, ownedOut);
    if (refs.tooltipFrame) refs.tooltipFrame->SetVisible(false);
    refs.hovertipFrame = CreateFrame(gui, builder, templates, "SmashHoverTip", rootParent, ownedOut);
    if (refs.hovertipFrame) refs.hovertipFrame->SetVisible(false);
    auto bindText = [&](const char* name) -> TextFrame* {
        if (UIFrame* f = Find(gui, builder, name)) {
            if (auto* t = dynamic_cast<TextFrame*>(f)) return t;
        }
        return nullptr;
    };
    refs.tooltipText = bindText("SmashToolTipText");
    refs.uberTipText = bindText("SmashUberTipText");
    refs.hoverTipText = bindText("SmashHoverTipText");

    // ---- Screen-space overlays (cinematic/cursor/timer/script/messages) ----
    // Ref MeleeUI cinematicPanel/cursorFrame/timerDialogs/scriptDialog/
    // gameMessages. Retail templates carry real geometry; hidden by default.
    refs.cinematicPanel = CreateFrame(gui, builder, templates, "CinematicPanel", rootParent, ownedOut);
    if (refs.cinematicPanel) refs.cinematicPanel->SetVisible(false);
    refs.cinematicSpeakerText = bindText("CinematicSpeakerText");
    refs.cinematicDialogueText = bindText("CinematicDialogueText");
    CreateFrame(gui, builder, templates, "SmashCursorFrame", rootParent, ownedOut);
    refs.timerDialog = CreateFrame(gui, builder, templates, "TimerDialog", rootParent, ownedOut);
    if (refs.timerDialog) refs.timerDialog->SetVisible(false);
    refs.timerTitleText = bindText("TimerDialogTitle");
    refs.timerValueText = bindText("TimerDialogValue");
    refs.scriptDialog = CreateFrame(gui, builder, templates, "ScriptDialog", rootParent, ownedOut);
    if (refs.scriptDialog) refs.scriptDialog->SetVisible(false);
    refs.scriptText = bindText("ScriptDialogText");
    CreateFrame(gui, builder, templates, "SmashGameMessageFrame", rootParent, ownedOut);

    // ---- In-game menu / dialog panels (ref FrameDef.toc UI/*.fdf) ----
    // Esc menu (EscMenuBackdrop + EscMenuMainPanel with Main/EndGame/
    // ConfirmQuit/Help/Tips sub-panels), Quest dialog (任务面板),
    // Chat/Alliance dialogs (upper-button targets), Log dialog.
    // All hidden until opened via the upper buttons / Esc key.
    refs.escMenuBackdrop = CreateFrame(gui, builder, templates, "EscMenuBackdrop", rootParent, ownedOut);
    if (refs.escMenuBackdrop) refs.escMenuBackdrop->SetVisible(false);
    refs.escMenuMainPanel =
        CreateFrame(gui, builder, templates, "EscMenuMainPanel", rootParent, ownedOut);
    if (refs.escMenuMainPanel) refs.escMenuMainPanel->SetVisible(false);
    refs.questDialog = CreateFrame(gui, builder, templates, "QuestDialog", rootParent, ownedOut);
    if (refs.questDialog) refs.questDialog->SetVisible(false);
    refs.chatDialog = CreateFrame(gui, builder, templates, "ChatDialog", rootParent, ownedOut);
    if (refs.chatDialog) refs.chatDialog->SetVisible(false);
    refs.allianceDialog =
        CreateFrame(gui, builder, templates, "AllianceDialog", rootParent, ownedOut);
    if (refs.allianceDialog) refs.allianceDialog->SetVisible(false);
    refs.logDialog = CreateFrame(gui, builder, templates, "LogDialog", rootParent, ownedOut);
    if (refs.logDialog) refs.logDialog->SetVisible(false);

    // ---- Minimap: code-owned backdrop at the Java minimap rect ----
    // (Java MeleeUI has no minimap side buttons, so none are created.)
    if (!Find(gui, builder, "Minimap")) {
        auto* mm = new BackdropFrame("Minimap", nullptr);
        ParentTo(gui, rootParent, mm);
        ownedOut.push_back(mm);
        refs.minimap = mm;
    } else {
        refs.minimap = Find(gui, builder, "Minimap");
    }
    // Geometry is applied by Layout (after the FDF tree is positioned).
    return true;
}

void ConsoleHUD::BuildBuffBar(GameUI& gui, ConsoleHudRefs& refs, TextRenderer* textRenderer,
                              int screenW, int screenH, std::vector<UIFrame*>& ownedOut) {
    UIFrame* parent = refs.simpleInfoPanelUnitDetail ? (UIFrame*)refs.simpleInfoPanelUnitDetail
                                                     : (UIFrame*)refs.smashSimpleInfoPanel;
    if (!parent) return;
    // SmashBuffStatusBar "COLON_STATUS" label; explicit rect in Layout.
    TextFrame* status = CreateStringFrame(gui, "SmashBuffStatusBar", parent, 0.01f, screenH,
                                          textRenderer, ownedOut);
    status->SetText("Status:");
    status->SetVisible(false);
    refs.buffStatusBar = status;
    for (int i = 0; i < kBuffMax; ++i) {
        std::string name = "SmashBuffStatusBarIcon" + std::to_string(i);
        refs.buffButtons[i] = CreateButton(gui, name, parent, textRenderer, ownedOut);
        refs.buffButtons[i]->SetVisible(false);
    }
}

void ConsoleHUD::BuildQueueAndMultiSelect(GameUI& gui, ConsoleHudRefs& refs,
                                          TextRenderer* textRenderer, int screenW, int screenH,
                                          std::vector<UIFrame*>& ownedOut) {
    SimpleFrame* panel = refs.smashSimpleInfoPanel;
    if (!panel) return;
    refs.queueButtons[0] = CreateButton(gui, "SmashBuildQueueIcon0", panel, textRenderer, ownedOut);
    for (int i = 1; i < kQueueCount; ++i) {
        refs.queueButtons[i] =
            CreateButton(gui, "SmashBuildQueueIcon" + std::to_string(i), panel, textRenderer, ownedOut);
    }
    refs.queueWorkerButton =
        CreateButton(gui, "SmashBuildQueueWorkerIcon", panel, textRenderer, ownedOut);
    for (int i = 0; i < kMultiSelectMax; ++i) {
        refs.multiSelectButtons[i] = CreateButton(
            gui, "SmashMultiSelectUnitIcon" + std::to_string(i), panel, textRenderer, ownedOut);
        refs.multiSelectButtons[i]->SetVisible(false);
        refs.cargoButtons[i] = CreateButton(
            gui, "SmashCargoUnitIcon" + std::to_string(i), panel, textRenderer, ownedOut);
        refs.cargoButtons[i]->SetVisible(false);
    }
    (void)screenW;
    (void)screenH;
}

void ConsoleHUD::BuildInfoWrappers(GameUI& gui, FrameBuilder* builder,
                                   std::map<std::string, FrameDefinition>& templates,
                                   ConsoleHudRefs& refs, TextRenderer* textRenderer,
                                   int screenW, int screenH, std::vector<UIFrame*>& ownedOut) {
    UIFrame* unitDetail = refs.simpleInfoPanelUnitDetail
                              ? (UIFrame*)refs.simpleInfoPanelUnitDetail
                              : (UIFrame*)refs.smashSimpleInfoPanel;
    if (!unitDetail) return;
    auto ensureWrapper = [&](const std::string& wrapper) -> UIFrame* {
        UIFrame* wrap = Find(gui, builder, wrapper);
        if (!wrap) {
            auto* s = new SimpleFrame(wrapper, nullptr);
            ParentTo(gui, unitDetail, s);
            ownedOut.push_back(s);
            wrap = s;
        }
        return wrap;
    };

    UIFrame* dmg0Wrap = ensureWrapper("SmashSimpleInfoPanelIconDamage0");
    ensureWrapper("SmashSimpleInfoPanelIconDamage1");
    UIFrame* armorWrap = ensureWrapper("SmashSimpleInfoPanelIconArmor");
    UIFrame* heroWrap = ensureWrapper("SmashSimpleInfoPanelIconHero");
    if (heroWrap && templates.count("SimpleInfoPanelIconHero") && builder) {
        bool present = false;
        if (auto* s = dynamic_cast<SimpleFrame*>(heroWrap)) {
            for (UIFrame* c : s->GetChildren()) {
                if (c && c->GetName() == "SimpleInfoPanelIconHero") { present = true; break; }
            }
        }
        if (!present) builder->ConstructFrame(templates.at("SimpleInfoPanelIconHero"), heroWrap);
    }

    // Explicit children for Damage0:
    if (dmg0Wrap) {
        auto* bd = new TextureFrame("Attack1IconBackdrop", nullptr);
        ParentTo(gui, dmg0Wrap, bd);
        gui.frameRegistry["InfoPanelIconBackdrop"] = bd;
        gui.frameRegistry["Attack1IconBackdrop"] = bd;
        ownedOut.push_back(bd);
        refs.attack1IconBackdrop = bd;

        auto* tl = new TextFrame("Attack1IconLabel", nullptr);
        tl->SetTextRenderer(textRenderer);
        tl->SetFontSize(ConvertY(screenH, 0.010f));
        tl->SetText("Damage:");
        tl->SetColor(0xFFFFCC00);
        ParentTo(gui, dmg0Wrap, tl);
        gui.frameRegistry["InfoPanelIconLabel"] = tl;
        gui.frameRegistry["Attack1IconLabel"] = tl;
        ownedOut.push_back(tl);
        refs.attack1IconLabel = tl;

        auto* tv = new TextFrame("Attack1IconValue", nullptr);
        tv->SetTextRenderer(textRenderer);
        tv->SetFontSize(ConvertY(screenH, 0.011f));
        ParentTo(gui, dmg0Wrap, tv);
        gui.frameRegistry["InfoPanelIconValue"] = tv;
        gui.frameRegistry["Attack1IconValue"] = tv;
        ownedOut.push_back(tv);
        refs.attack1IconValue = tv;
    }

    // Explicit children for Armor:
    if (armorWrap) {
        auto* bd = new TextureFrame("SmashArmorIconBackdrop", nullptr);
        ParentTo(gui, armorWrap, bd);
        gui.frameRegistry["SmashArmorIconBackdrop"] = bd;
        gui.frameRegistry["ArmorIconBackdrop"] = bd;
        ownedOut.push_back(bd);
        refs.armorIconBackdrop = bd;

        auto* tl = new TextFrame("SmashArmorIconLabel", nullptr);
        tl->SetTextRenderer(textRenderer);
        tl->SetFontSize(ConvertY(screenH, 0.010f));
        tl->SetText("Armor:");
        tl->SetColor(0xFFFFCC00);
        ParentTo(gui, armorWrap, tl);
        gui.frameRegistry["SmashArmorIconLabel"] = tl;
        gui.frameRegistry["ArmorIconLabel"] = tl;
        ownedOut.push_back(tl);
        refs.armorIconLabel = tl;

        auto* tv = new TextFrame("SmashArmorIconValue", nullptr);
        tv->SetTextRenderer(textRenderer);
        tv->SetFontSize(ConvertY(screenH, 0.011f));
        ParentTo(gui, armorWrap, tv);
        gui.frameRegistry["SmashArmorIconValue"] = tv;
        gui.frameRegistry["ArmorIconValue"] = tv;
        ownedOut.push_back(tv);
        refs.armorIconValue = tv;
    }
    (void)screenW;
    (void)screenH;
}

void ConsoleHUD::BuildInventoryBar(GameUI& gui, ConsoleHudRefs& refs, TextRenderer* textRenderer,
                                  int screenW, int screenH, std::vector<UIFrame*>& ownedOut) {
    SimpleFrame* panel = refs.smashSimpleInfoPanel;
    UIFrame* consoleUI = refs.consoleUI ? (UIFrame*)refs.consoleUI : (UIFrame*)gui.rootFrame;
    auto* bar = new SimpleFrame("SmashSimpleInventoryBar", nullptr);
    ParentTo(gui, panel ? (UIFrame*)panel : consoleUI, bar);
    ownedOut.push_back(bar);
    refs.inventoryBarFrame = bar;

    int index = 0;
    for (int j = 0; j < kInventoryRows; ++j) {
        for (int i = 0; i < kInventoryCols; ++i) {
            refs.inventoryButtons[index] = CreateButton(
                gui, "SmashInventoryButton_" + std::to_string(index), bar, textRenderer, ownedOut);
            index++;
        }
    }
    TextFrame* title = CreateStringFrame(gui, "SmashInventoryText", bar, 0.0109f, screenH,
                                         textRenderer, ownedOut);
    title->SetText("INVENTORY");
    refs.inventoryTitleFrame = title;
    (void)screenW;
}

void ConsoleHUD::BuildCommandGrid(GameUI& gui, ConsoleHudRefs& refs, TextRenderer* textRenderer,
                                  int screenW, int screenH, std::vector<UIFrame*>& ownedOut) {
    UIFrame* rootParent = gui.rootFrame;
    auto* container = new SimpleFrame("SmashCommandButtons", nullptr);
    ParentTo(gui, rootParent, container);
    ownedOut.push_back(container);
    refs.smashCommandButtons = container;
    int index = 0;
    for (int j = 0; j < kCommandRows; ++j) {
        for (int i = 0; i < kCommandCols; ++i) {
            refs.commandButtons[index] = CreateButton(
                gui, "SmashCommandButton_" + std::to_string(index), container, textRenderer, ownedOut);
            index++;
        }
    }
    (void)screenW;
    (void)screenH;
}

void ConsoleHUD::Layout(GameUI& gui, ConsoleHudRefs& refs, int screenW, int screenH) {
    const float W = (float)screenW, H = (float)screenH;

    // ---- Info panel: BOTTOM-centered, 0.180 x 0.120 (width via convertY!) ----
    refs.infoPanelWidth = ConvertY(screenH, 0.180f);
    refs.infoPanelHeight = ConvertY(screenH, 0.120f);
    const float panelW = refs.infoPanelWidth, panelH = refs.infoPanelHeight;
    const float panelX = (W - panelW) * 0.5f, panelY = H - panelH;
    if (refs.smashSimpleInfoPanel) Place(refs.smashSimpleInfoPanel, panelX, panelY, panelW, panelH);
    const float panelBottom = panelY + panelH; // == H

    // ---- Procedural fallbacks without FDF geometry fill sensible rects ----
    if (refs.proceduralUnitDetail && refs.simpleInfoPanelUnitDetail) {
        Place(refs.simpleInfoPanelUnitDetail, panelX, panelY, panelW, panelH);
    }
    if (refs.proceduralPortrait && refs.unitPortrait) {
        const float pw = ConvertX(screenW, 0.0835f), ph = ConvertY(screenH, 0.114f);
        Place(refs.unitPortrait, ConvertX(screenW, 0.211f), H - ph, pw, ph);
    }
    if (refs.unitPortrait) {
        UiRect pr = refs.unitPortrait->GetBounds();
        if (refs.unitPortraitHitPointText) {
            refs.unitPortraitHitPointText->SetDefaultToParent(false);
            Place(refs.unitPortraitHitPointText, pr.x, pr.y + pr.h - ConvertY(screenH, 0.024f), pr.w, ConvertY(screenH, 0.012f));
        }
        if (refs.unitPortraitManaPointText) {
            refs.unitPortraitManaPointText->SetDefaultToParent(false);
            Place(refs.unitPortraitManaPointText, pr.x, pr.y + pr.h - ConvertY(screenH, 0.012f), pr.w, ConvertY(screenH, 0.012f));
        }
    }
    UIFrame* unitDetail = refs.simpleInfoPanelUnitDetail
                              ? (UIFrame*)refs.simpleInfoPanelUnitDetail
                              : (UIFrame*)refs.smashSimpleInfoPanel;
    UiRect detailBounds = unitDetail ? unitDetail->GetBounds() : UiRect{panelX, panelY, panelW, panelH};
    if ((detailBounds.w < 2.0f || detailBounds.h < 2.0f) && unitDetail) {
        // Retail SimpleInfoPanelUnitDetail carries no own size; it fills the
        // info panel (its FDF children position themselves inside it).
        Place(unitDetail, panelX, panelY, panelW, panelH);
        detailBounds = unitDetail->GetBounds();
        // Children were positioned against the old (degenerate) rect.
        if (auto* detailSimple = dynamic_cast<SimpleFrame*>(unitDetail)) {
            detailSimple->PositionBounds(gui);
        }
    }

    // ---- Status-bar widths (ref java buildTimeIndicatorWidth etc.) ----
    if (UIFrame* bar = Find(gui, nullptr, "SimpleBuildTimeIndicator")) {
        bar->SetWidth(ConvertX(screenW, 0.10538f));
        bar->SetHeight(ConvertY(screenH, 0.0103f));
    }
    if (UIFrame* bar = Find(gui, nullptr, "SimpleHeroLevelBar")) bar->SetWidth(panelW);
    if (UIFrame* bar = Find(gui, nullptr, "SimpleProgressIndicator")) bar->SetWidth(panelW);
    if (UIFrame* backdrop = Find(gui, nullptr, "SimpleBuildQueueBackdrop")) {
        backdrop->SetWidth(panelW);
        backdrop->SetHeight(panelW * 0.5f);
    }

    // ---- UpperButtonBar and ResourceBar bounds across the top HUD ----
    const float ubW = ConvertX(screenW, 0.343f);
    const float rbW = ConvertX(screenW, 0.341f);
    const float topH = ConvertY(screenH, 0.052f);
    if (refs.upperButtonBar) {
        Place(refs.upperButtonBar, 0.0f, 0.0f, ubW, topH);
        refs.upperButtonBar->PositionBounds(gui);
    }
    if (refs.resourceBar) {
        Place(refs.resourceBar, W - rbW, 0.0f, rbW, topH);
    }

    // ---- ResourceBar text widths (ref Warsmash MeleeUI: lines 580-582) ----
    UIFrame* upkeepText = Find(gui, nullptr, "ResourceBarUpkeepText");
    float upkeepWidth = upkeepText ? upkeepText->GetWidth() : 0.0f;
    if (upkeepWidth < 2.0f) {
        upkeepWidth = ConvertX(screenW, 0.055f);
    }
    for (const char* name : {"ResourceBarGoldText", "ResourceBarLumberText", "ResourceBarSupplyText"}) {
        if (UIFrame* tf = Find(gui, nullptr, name)) {
            tf->SetWidth(upkeepWidth);
            if (upkeepText && upkeepText->GetHeight() > 2.0f) {
                tf->SetHeight(upkeepText->GetHeight());
            }
        }
    }
    if (refs.resourceBar) {
        refs.resourceBar->PositionBounds(gui);
    }

    // ---- Buff bar: parent BOTTOMLEFT + (0.03, 0.003) up-positive (the
    // Java AnchorDefinition resolves against the detail panel, and the
    // icons chain LEFT->RIGHT off the previous frame) ----
    if (refs.buffStatusBar) {
        const float sw = ConvertX(screenW, 0.035f), sh = ConvertY(screenH, 0.01f);
        const float sx = detailBounds.x + ConvertX(screenW, 0.03f);
        const float sy = detailBounds.y + detailBounds.h - ConvertY(screenH, 0.003f) - sh;
        Place(refs.buffStatusBar, sx, sy, sw, sh);
        const float iw = ConvertX(screenW, 0.015f), ih = ConvertY(screenH, 0.015f);
        float ix = sx + sw + ConvertX(screenW, 0.001f);
        const float iy = sy + (sh - ih) * 0.5f;
        for (int i = 0; i < kBuffMax; ++i) {
            if (refs.buffButtons[i]) {
                Place(refs.buffButtons[i], ix, iy, iw, ih);
                ix += iw + ConvertX(screenW, 0.001f);
            }
        }
    }

    // ---- Queue + multiselect (proportional to the info panel, ref java) ----
    refs.frontQueueIconWidth = (panelW * 38.0f) / 256.0f;
    const float fw = refs.frontQueueIconWidth;
    if (refs.queueButtons[0]) {
        const float cx = panelX + (panelW * (15.0f + 19.0f)) / 256.0f;
        const float cy = panelBottom - (panelW * (66.0f + 19.0f)) / 256.0f;
        Place(refs.queueButtons[0], cx - fw * 0.5f, cy - fw * 0.5f, fw, fw);
    }
    for (int i = 1; i < kQueueCount; ++i) {
        if (!refs.queueButtons[i]) continue;
        const float qw = (panelW * 29.0f) / 256.0f;
        const float cx = panelX + (panelW * (13.0f + 14.5f + (40.0f * (i - 1)))) / 256.0f;
        const float cy = panelBottom - (panelW * (24.0f + 14.5f)) / 256.0f;
        Place(refs.queueButtons[i], cx - qw * 0.5f, cy - qw * 0.5f, qw, qw);
    }
    if (refs.queueWorkerButton && refs.queueButtons[1]) {
        UiRect q1 = refs.queueButtons[1]->GetBounds();
        Place(refs.queueWorkerButton, q1.x, q1.y, fw, fw);
    }
    {
        const int perRow = kMultiSelectMax / 3;
        for (int i = 0; i < kMultiSelectMax; ++i) {
            if (refs.multiSelectButtons[i]) {
                const float mx = panelX + fw * (0.10f + 1.10f * (i % perRow));
                const float my = panelY + fw * (0.75f + 1.5f * (i / perRow));
                Place(refs.multiSelectButtons[i], mx, my, fw, fw);
            }
            if (refs.cargoButtons[i]) {
                const float mx = panelX + fw * (1.20f * (i % perRow));
                const float my = panelY + fw * (0.75f + 1.5f * (i / perRow));
                Place(refs.cargoButtons[i], mx, my, fw, fw);
            }
        }
    }

    // ---- Info wrappers (attack x2 / armor / hero), relative to unit detail ----
    auto placeWrapper = [&](const char* name, float fdfX, float fdfY, float fdfW, float fdfH) {
        UIFrame* wrap = Find(gui, nullptr, name);
        if (!wrap) return;
        Place(wrap, detailBounds.x + ConvertX(screenW, fdfX),
              detailBounds.y - ConvertY(screenH, fdfY),
              ConvertX(screenW, fdfW), ConvertY(screenH, fdfH));
    };
    placeWrapper("SmashSimpleInfoPanelIconDamage0", 0.0f, -0.040f, 0.1f, 0.030125f);
    placeWrapper("SmashSimpleInfoPanelIconDamage1", 0.1f, -0.03925f, 0.1f, 0.030125f);
    placeWrapper("SmashSimpleInfoPanelIconArmor", 0.0f, -0.0705f, 0.1f, 0.030125f);
    placeWrapper("SmashSimpleInfoPanelIconHero", 0.1f, -0.037f, 0.1f, 0.0625f);
    // Damage0 procedural layout
    if (UIFrame* dmgWrap = Find(gui, nullptr, "SmashSimpleInfoPanelIconDamage0")) {
        UiRect dw = dmgWrap->GetBounds();
        const float s = std::min(dw.w, dw.h);
        if (UIFrame* bd = refs.attack1IconBackdrop ? refs.attack1IconBackdrop : Find(gui, nullptr, "InfoPanelIconBackdrop")) {
            Place(bd, dw.x, dw.y, s, s);
        }
        const float tx = dw.x + s + ConvertX(screenW, 0.004f);
        const float tw = std::max(10.0f, dw.w - s - ConvertX(screenW, 0.004f));
        const float lineH = dw.h * 0.48f;
        if (UIFrame* tl = refs.attack1IconLabel ? (UIFrame*)refs.attack1IconLabel : Find(gui, nullptr, "InfoPanelIconLabel")) {
            Place(tl, tx, dw.y, tw, lineH);
        }
        if (UIFrame* tv = refs.attack1IconValue ? (UIFrame*)refs.attack1IconValue : Find(gui, nullptr, "InfoPanelIconValue")) {
            Place(tv, tx, dw.y + lineH, tw, lineH);
        }
    }
    // Armor procedural children mirror the retail icon offsets: backdrop
    // fills the wrapper, label on top right, value below it.
    if (UIFrame* armorWrap = Find(gui, nullptr, "SmashSimpleInfoPanelIconArmor")) {
        UiRect aw = armorWrap->GetBounds();
        const float s = std::min(aw.w, aw.h);
        if (UIFrame* bd = refs.armorIconBackdrop ? refs.armorIconBackdrop : Find(gui, nullptr, "SmashArmorIconBackdrop")) {
            Place(bd, aw.x, aw.y, s, s);
        }
        const float tx = aw.x + s + ConvertX(screenW, 0.004f);
        const float tw = std::max(10.0f, aw.w - s - ConvertX(screenW, 0.004f));
        const float lineH = aw.h * 0.48f;
        if (UIFrame* tl = refs.armorIconLabel ? (UIFrame*)refs.armorIconLabel : Find(gui, nullptr, "SmashArmorIconLabel")) {
            Place(tl, tx, aw.y, tw, lineH);
        }
        if (UIFrame* tv = refs.armorIconValue ? (UIFrame*)refs.armorIconValue : Find(gui, nullptr, "SmashArmorIconValue")) {
            Place(tv, tx, aw.y + lineH, tw, lineH);
        }
    }

    if (UIFrame* nv = Find(gui, nullptr, "SimpleNameValue")) {
        UiRect nb = nv->GetBounds();
        if (nb.y <= detailBounds.y + 1.0f) {
            nv->SetPosition(nb.x, detailBounds.y + ConvertY(screenH, 0.003f));
        }
    }

    // ---- Inventory bar: BOTTOMRIGHT at consoleUI BOTTOMLEFT + (0.591, 0) ----
    if (refs.inventoryBarFrame) {
        const float barW = ConvertX(screenW, 0.079f), barH = ConvertY(screenH, 0.115f);
        const float barX = ConvertX(screenW, 0.591f) - barW, barY = H - barH;
        Place(refs.inventoryBarFrame, barX, barY, barW, barH);
        for (int j = 0; j < kInventoryRows; ++j) {
            for (int i = 0; i < kInventoryCols; ++i) {
                int k = j * kInventoryCols + i;
                if (!refs.inventoryButtons[k]) continue;
                const float s = kInventoryIconWidth;
                Place(refs.inventoryButtons[k], barX + ConvertX(screenW, 0.0037f + (0.04f * i)),
                      barY + ConvertY(screenH, 0.0021f + (0.03815f * j)),
                      ConvertX(screenW, s), ConvertY(screenH, s));
            }
        }
        if (refs.inventoryTitleFrame) {
            const float tw = ConvertX(screenW, 0.071f), th = ConvertX(screenW, 0.01125f);
            Place(refs.inventoryTitleFrame, barX + ConvertX(screenW, 0.004f),
                  barY - ConvertY(screenH, 0.0165625f), tw, th);
        }
    }

    // ---- Command grid: BOTTOMLEFT + (0.6175+0.0434*i, 0.095-0.044*j) ----
    for (int j = 0; j < kCommandRows; ++j) {
        for (int i = 0; i < kCommandCols; ++i) {
            int k = j * kCommandCols + i;
            if (!refs.commandButtons[k]) continue;
            const float s = kCommandIconWidth;
            const float fx = 0.6175f + (0.0434f * i), fy = 0.095f - (0.044f * j);
            Place(refs.commandButtons[k], ConvertX(screenW, fx),
                  H - ConvertY(screenH, fy) - ConvertY(screenH, s),
                  ConvertX(screenW, s), ConvertY(screenH, s));
        }
    }

    // ---- Error message: viewport BOTTOMLEFT + (0.212, 0.182), hidden ----
    if (refs.errorMessageFrame) {
        const float w = ConvertX(screenW, 0.35f), h = ConvertY(screenH, 0.012f);
        Place(refs.errorMessageFrame, ConvertX(screenW, 0.212f), H - ConvertY(screenH, 0.182f) - h,
              w, h);
    }

    // ---- Minimap rect: Java 1600x1200 UI space, bottom-left origin ----
    if (refs.minimap) {
        const float mmX = (18.75f / 1600.0f) * W;
        const float mmW = (278.75f / 1600.0f) * W;
        const float mmH = (276.25f / 1200.0f) * H;
        const float mmBottom = (13.75f / 1200.0f) * H;
        Place(refs.minimap, mmX, H - mmBottom - mmH, mmW, mmH);
    }

    // ---- TimeOfDayIndicator: retail SPRITE placed in the top HUD gap
    // between UpperButtonBar (x ~548) and ResourceBar (x ~734) ----
    if (refs.timeIndicator) {
        const float clockW = ConvertX(screenW, 0.116f);
        const float clockH = ConvertY(screenH, 0.078f);
        const float clockX = (W - clockW) * 0.5f;
        Place(refs.timeIndicator, clockX, 0.0f, clockW, clockH);
    }

    // ---- Procedural upper-button fallbacks (retail FDF absent only) ----
    static const char* kLegacyUppers[4] = {"QuestsButton", "MenuButton", "AlliesButton", "ChatButton"};
    for (int i = 0; i < 4; ++i) {
        UIFrame* b = Find(gui, nullptr, kLegacyUppers[i]);
        if (b) Place(b, W - 320 + i * 80, 28, 76, 22);
    }
    (void)gui;
}
