// FDF-backed HUD host: retail FrameDef layout + Java-faithful console HUD.
// Ref: warsmash_ref/core/src/com/etheller/warsmash/viewer5/handlers/w3x/ui/MeleeUI.java
#include "FdfHudHost.h"
#include "render/rhi_device.h"
#include "BackdropFrame.h"
#include "ButtonFrame.h"
#include "ConsoleHUD.h"
#include "ExtendedFrames.h"
#include "SimpleFrame.h"
#include "TextFrame.h"
#include "TextureFrame.h"
#include "../asset/Texture.h"
#include "../asset/BLP.h"
#include "../resource/mpq_storm.h"
#include "third_party/sokol/sokol_app.h"
#include "third_party/sokol/sokol_gfx.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace {

std::string ToLowerNoSpace(std::string s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (c == '_' || c == '-' || c == ' ') continue;
        out.push_back((char)std::tolower(c));
    }
    return out;
}

bool ReadMpqText(MpqManager* mpq, const std::string& path, std::string& out) {
    if (!mpq) return false;
    std::vector<uint8_t> buf;
    if (!mpq->ReadFile(path, buf) || buf.empty()) return false;
    out.assign((const char*)buf.data(), buf.size());
    return true;
}

void HideSubtree(UIFrame* f) {
    if (!f) return;
    f->SetVisible(false);
    if (auto* s = engine::ui::DynamicCastFrame<SimpleFrame>(f)) {
        for (UIFrame* c : s->GetChildren()) HideSubtree(c);
    }
}

// Hide only the dialog root (ref retail modal panels): children keep their
// FDF visibility so reopening the dialog restores the full subtree.
// HideSubtree above is for SmashEscMenu-style containers that must start
// fully hidden; retail dialogs use this root-only variant.
void HideFrame(UIFrame* f) {
    if (f) f->SetVisible(false);
}

} // namespace

const char* FdfHudHost::GameMenuActionName(int action) {
    static constexpr const char* kNames[] = {
        "GameMenuButtonPause", "GameMenuButtonSaveGame", "GameMenuButtonLoadGame",
        "GameMenuButtonOptions", "GameMenuButtonHelp", "GameMenuButtonTips",
        "GameMenuButtonEndGame", "GameMenuButtonReturnToGame", "GameMenuButtonRestart",
        "GameMenuButtonQuitMission", "GameMenuButtonExitProgram", "GameMenuButtonPrevious",
        "GameMenuButtonConfirmExit", "GameMenuButtonCancelConfirm", "GameMenuButtonGameplay",
        "GameMenuButtonVideo", "GameMenuButtonSound", "GameMenuButtonNetwork",
        "GameMenuButtonOptionsBack", "GameMenuButtonOptionsOK", "GameMenuButtonOptionsCancel",
        "GameMenuButtonSaveGameSave", "GameMenuButtonSaveGameDelete", "GameMenuButtonSaveGameCancel",
        "GameMenuButtonLoadGameLoad", "GameMenuButtonLoadGameCancel", "GameMenuButtonOverwriteConfirm",
        "GameMenuButtonOverwriteCancel", "GameMenuButtonDeleteConfirm", "GameMenuButtonDeleteCancel",
        "GameMenuButtonHelpOK", "GameMenuButtonTipsBack", "GameMenuButtonTipsNext",
        "GameMenuButtonTipsOK", "GameMenuOptionGameSpeedSlider", "GameMenuOptionMouseScrollSlider",
        "GameMenuOptionMouseScrollDisable", "GameMenuOptionKeyScrollSlider",
        "GameMenuOptionEnhancedTooltips", "GameMenuOptionSubgroupModifier",
        "GameMenuOptionFormationToggle", "GameMenuOptionHealthBars", "GameMenuOptionGammaSlider",
        "GameMenuOptionParticles", "GameMenuOptionLights", "GameMenuOptionShadows",
        "GameMenuOptionOcclusion", "GameMenuOptionSoundVolume", "GameMenuOptionSoundVolumeSlider",
        "GameMenuOptionMusicVolume", "GameMenuOptionMusicVolumeSlider", "GameMenuOptionAmbientSounds",
        "GameMenuOptionMovementSounds", "GameMenuOptionUnitSounds", "GameMenuOptionSubtitles",
        "GameMenuOptionEnvironmentalEffects", "GameMenuOptionPositionalAudio",
        "GameMenuOptionLowLatency", "GameMenuOptionHighLatency", "GameMenuOptionExtraHighLatency",
        "GameMenuOptionParticlesLow", "GameMenuOptionParticlesMedium", "GameMenuOptionParticlesHigh",
        "GameMenuOptionLightsLow", "GameMenuOptionLightsMedium", "GameMenuOptionLightsHigh",
        "GameMenuOptionShadowsOff", "GameMenuOptionShadowsOn", "GameMenuOptionOcclusionOff",
        "GameMenuOptionOcclusionOn",
    };
    static_assert(sizeof(kNames) / sizeof(kNames[0]) == 70, "game menu action table");
    if (action < 0 || action >= kGameMenuActionCount) return "";
    return kNames[action];
}

int FdfHudHost::GameMenuActionForName(const std::string& name) {
    const std::string key = ToLowerNoSpace(name);
    for (int i = 0; i < kGameMenuActionCount; ++i) {
        if (key == ToLowerNoSpace(GameMenuActionName(i))) return i;
    }
    return -1;
}

const char* FdfHudHost::GlueActionName(int action) {
    static constexpr const char* kNames[] = {
        "SinglePlayerButton", "BattleNetButton", "LocalAreaNetworkButton", "OptionsButton",
        "CreditsButton", "ExitButton", "QuitConfirmOKButton", "QuitConfirmCancelButton",
        "ProfileButton", "LoadSavedButton", "ViewReplayButton", "CustomCampaignButton",
        "CampaignButton", "CustomGameButton", "CancelButton", "BackButton",
        "CampaignPrologueButton", "CampaignHumanButton", "CampaignUndeadButton",
        "CampaignOrcButton", "CampaignNightElfButton", "CampaignMissionOneButton",
        "CampaignMissionTwoButton", "CampaignMission3Button", "CampaignMission4Button",
        "CampaignMission5Button", "CampaignMission6Button", "CampaignMission7Button",
        "CampaignMission8Button", "CampaignMission9Button", "CampaignMission10Button",
        "CampaignMission11Button", "CampaignMission12Button", "MissionBackButton",
        "PlayGameButton", "CustomGameCancelButton", "MapInfoButton", "AdvancedOptionsButton",
        "TeamsTogetherToggleButton", "FixedTeamsToggleButton", "FullSharedControlToggleButton",
        "MapItem0Button", "MapItem1Button", "MapItem2Button", "MapItem3Button",
        "MapItem4Button", "MapItem5Button", "MapItem6Button", "MapItem7Button",
        "MapItem8Button", "MapItem9Button", "MapPageUpButton", "MapPageDownButton",
        "PlayerSlot0NameMenu", "PlayerSlot1NameMenu", "PlayerSlot2NameMenu",
        "PlayerSlot0RaceMenu", "PlayerSlot1RaceMenu", "PlayerSlot2RaceMenu",
        "PlayerSlot3NameMenu", "PlayerSlot4NameMenu", "PlayerSlot5NameMenu",
        "PlayerSlot6NameMenu", "PlayerSlot7NameMenu", "PlayerSlot8NameMenu",
        "PlayerSlot9NameMenu", "PlayerSlot10NameMenu", "PlayerSlot11NameMenu",
        "PlayerSlot3RaceMenu", "PlayerSlot4RaceMenu", "PlayerSlot5RaceMenu",
        "PlayerSlot6RaceMenu", "PlayerSlot7RaceMenu", "PlayerSlot8RaceMenu",
        "PlayerSlot9RaceMenu", "PlayerSlot10RaceMenu", "PlayerSlot11RaceMenu",
        "OptionsGameplayButton", "OptionsVideoButton", "OptionsSoundButton",
        "OptionsNetworkButton", "OptionsBackButton", "OptionsOKButton", "OptionsCancelButton",
        "BattleNetLogonButton", "BattleNetCreateAccountButton", "BattleNetRecoverPasswordButton",
        "QuitBattleNetButton", "LanRefreshButton", "LanCreateGameButton", "LanJoinGameButton",
        "QuitLocalAreaNetworkButton",
    };
    static_assert(sizeof(kNames) / sizeof(kNames[0]) == 92, "glue action table");
    if (action < 0 || action >= kGlueActionCount) return "";
    return kNames[action];
}

int FdfHudHost::GlueActionForName(const std::string& name) {
    const std::string key = ToLowerNoSpace(name);
    for (int i = 0; i < kGlueActionCount; ++i) {
        if (key == ToLowerNoSpace(GlueActionName(i))) return i;
    }
    if (key == "singleplayer") return 0;
    if (key == "battlenet" || key == "bnet" || key == "battle") return 1;
    if (key == "localareanetwork" || key == "lan" || key == "localnetwork") return 2;
    if (key == "options") return 3;
    if (key == "credits") return 4;
    if (key == "exit" || key == "quit") return 5;
    if (key == "quitok" || key == "exitok" || key == "confirmquit" || key == "confirmexit") return 6;
    if (key == "quitcancel" || key == "exitcancel" || key == "cancelquit") return 7;
    return -1;
}

FdfHudHost::~FdfHudHost() {
    Shutdown();
}

std::string FdfHudHost::NormalizeLayoutPath(const std::string& p) {
    std::string out = p;
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

bool FdfHudHost::Initialize(const std::string& assetRoot, int width, int height,
                            const std::string& layoutPath, MpqManager* mpq, int gameVersion) {
    assetRoot_ = assetRoot;
    mpq_ = mpq;
    gameVersion_ = gameVersion;
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    if (!gui_.Init(width_, height_)) {
        std::cerr << "FdfHudHost: GameUI init failed" << std::endl;
        return false;
    }
    textRenderer_.LoadFromMpq(mpq_);
    builder_ = new FrameBuilder(gui_, mpq_, &textRenderer_);
    // Race skin section defaults to Human (ref GameUI.loadSkin); the version
    // selects "<key>_V<gameVersion>" skin variants like retail.
    builder_->SetSkinGameVersion(gameVersion_);

    const std::string norm = NormalizeLayoutPath(layoutPath);
    const bool wantGlue = norm.find("Glue") != std::string::npos || norm.find("glue") != std::string::npos;

    usingRetailLayout_ = LoadRetailConsole();
    if (!usingRetailLayout_) BuildFallbackConsole();
    CacheConsoleRects();

    if (wantGlue) {
        glueActive_ = true;
        OpenGluePage(GluePage::MainMenu);
    }
    initialized_ = true;
    return true;
}

bool FdfHudHost::ReloadLayout(const std::string& layoutPath, MpqManager* mpq, int gameVersion) {
    Shutdown();
    return Initialize(assetRoot_.empty() ? std::string(PROJECT_SOURCE_DIR) : assetRoot_,
                      width_, height_, layoutPath, mpq ? mpq : mpq_, gameVersion);
}

BackdropFrame* FdfHudHost::MakeMenuBackdrop(const std::string& name, const std::string& bgKey,
                                                const std::string& edgeKey, float cornerFdf,
                                                float bgSizeFdf, float insetFdf) {
    if (!mpq_ || !builder_) return nullptr;
    auto* bd = new BackdropFrame(name, nullptr);
    bd->SetTileBackground(true);
    bd->SetCornerSize(gui_.ConvertX(cornerFdf));
    bd->SetBackgroundSize(gui_.ConvertX(bgSizeFdf));
    const float inset = gui_.ConvertX(insetFdf);
    bd->SetBackgroundInsets(inset, inset, inset, inset);
    bd->SetCornerFlags(BackdropFrame::ParseCornerFlags("UL|UR|BL|BR|T|L|B|R"));
    if (auto bg = builder_->LoadSkinTexture(bgKey)) bd->SetBackground(bg);
    if (auto edge = builder_->LoadSkinTexture(edgeKey)) bd->SetEdgeFile(edge);
    ownedFrames_.emplace_back(bd);
    return bd;
}
void FdfHudHost::ClearOwnedFrames() {
    for (auto& f : ownedFrames_) {
        if (auto* s = engine::ui::DynamicCastFrame<SimpleFrame>(f.get())) {
            // Copy: RemoveChild mutates the list.
            const auto kids = s->GetChildren();
            for (UIFrame* k : kids) s->RemoveChild(k);
        }
    }
    ownedFrames_.clear();
    root_ = nullptr;
    menuPanelBackdrop_ = nullptr;
    menuButtonBackdrop_ = nullptr;
    menuButtonPushedBackdrop_ = nullptr;
}

void FdfHudHost::Shutdown() {
    delete builder_;
    builder_ = nullptr;
    ClearOwnedFrames();
    consoleRefs_ = ConsoleHudRefs{};
    consoleTemplates_.clear();
    gui_.frameRegistry.clear();
    gui_.rootFrame = nullptr;
    textures_.clear();
    textRenderer_.Shutdown();
    initialized_ = false;
    usingRetailLayout_ = false;
    glueActive_ = false;
    gameMenuOpen_ = false;
    questOpen_ = false;
    chatDialogOpen_ = false;
    allianceDialogOpen_ = false;
    cinematicVisible_ = false;
    timerVisible_ = false;
    scriptVisible_ = false;
    errorMessage_ = FadingMessage{};
    gameMessages_.clear();
}

void FdfHudHost::Resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    gui_.Update(width_, height_);
    LayoutDynamicGrids();
}

void FdfHudHost::Update(double time) {
    uiClock_ = time;
    // Expire faded messages (ref MeleeUI render(): error + gameMessages).
    if (!errorMessage_.text.empty() && time >= errorMessage_.expireAt) {
        errorMessage_.text.clear();
    }
    gameMessages_.erase(
        std::remove_if(gameMessages_.begin(), gameMessages_.end(),
                       [time](const FadingMessage& m) { return time >= m.expireAt; }),
        gameMessages_.end());
}

bool FdfHudHost::LoadConsoleTemplates(std::map<std::string, FrameDefinition>& templates) {
    templates.clear();
    consoleDecoratedStrings_.clear();
    int files = 0;
    auto trim = [](std::string s) {
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
            s.pop_back();
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '"' || s[i] == '\'')) i++;
        size_t j = s.size();
        while (j > i && (s[j - 1] == '"' || s[j - 1] == '\'' || s[j - 1] == ',' || s[j - 1] == ';')) j--;
        return s.substr(i, j - i);
    };
    const std::string roots[] = {
        std::string(PROJECT_SOURCE_DIR) + "/resources/UI/FrameDef/",
        std::string(PROJECT_SOURCE_DIR) + "/warsmash_ref/resources/UI/FrameDef/",
    };
    auto resolveFile = [&](const std::string& path, std::string& out) -> bool {
        if (path.empty()) return false;
        if (mpq_ && ReadMpqText(mpq_, path, out)) return true;
        std::string norm = path;
        for (char& c : norm) if (c == '/') c = '\\';
        if (mpq_ && ReadMpqText(mpq_, norm, out)) return true;
        for (char& c : norm) if (c == '\\') c = '/';
        if (mpq_ && ReadMpqText(mpq_, norm, out)) return true;

        for (const auto& r : roots) {
            std::string sub = norm;
            size_t slash = sub.find_last_of('/');
            std::string base = slash == std::string::npos ? sub : sub.substr(slash + 1);
            std::ifstream f(r + base);
            if (!f.good()) f.open(r + "SmashUI/" + base);
            if (!f.good()) f.open(r + "UI/" + base);
            if (!f.good()) f.open(r + "Glue/" + base);
            if (f.good()) {
                std::stringstream cs;
                cs << f.rdbuf();
                out = cs.str();
                return true;
            }
        }
        return false;
    };
    auto parseContent = [&](const std::string& content, const std::string& origin) {
        try {
            FdfParser parser(content, resolveFile);
            parser.Parse();
            for (const auto& [name, def] : parser.GetTemplates()) templates[name] = def;
            for (const auto& [key, value] : parser.GetDecoratedStrings()) {
                if (consoleDecoratedStrings_.count(key) == 0) consoleDecoratedStrings_[key] = value;
            }
            files++;
        } catch (const std::exception& e) {
            std::cerr << "FdfHudHost: FDF parse failed for " << origin << ": " << e.what() << std::endl;
        }
    };
    // 0) Shared base templates referenced via IncludeFile in retail FDFs
    std::string escContent;
    if (resolveFile("UI\\FrameDef\\UI\\EscMenuTemplates.fdf", escContent)) {
        parseContent(escContent, "UI\\FrameDef\\UI\\EscMenuTemplates.fdf");
    }
    std::string stdContent;
    if (resolveFile("UI\\FrameDef\\Glue\\StandardTemplates.fdf", stdContent)) {
        parseContent(stdContent, "UI\\FrameDef\\Glue\\StandardTemplates.fdf");
    }
    // 1) Retail FrameDef.toc from MPQ (ref MeleeUI loads FrameDef.toc first).
    if (mpq_) {
        std::string toc;
        if (ReadMpqText(mpq_, "UI\\FrameDef\\FrameDef.toc", toc)) {
            size_t pos = 0;
            while (pos < toc.size()) {
                size_t nl = toc.find('\n', pos);
                std::string line = trim(toc.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos));
                pos = nl == std::string::npos ? toc.size() : nl + 1;
                if (line.empty() || line.rfind("//", 0) == 0) continue;
                std::string content;
                if (!ReadMpqText(mpq_, line, content)) continue;
                parseContent(content, line);
            }
        }
    }
    // 2) SmashFrameDef.toc: warsmash custom console chrome + tooltip/portrait
    // (ref MeleeUI loads SmashFrameDef.toc second, overriding retail).
    for (const auto& root : roots) {
        const std::string tocPath = root + "SmashFrameDef.toc";
        std::ifstream tocFile(tocPath);
        if (!tocFile.good()) continue;
        std::stringstream ss;
        ss << tocFile.rdbuf();
        std::string toc = ss.str();
        size_t pos = 0;
        while (pos < toc.size()) {
            size_t nl = toc.find('\n', pos);
            std::string line = trim(toc.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos));
            pos = nl == std::string::npos ? toc.size() : nl + 1;
            if (line.empty() || line.rfind("//", 0) == 0) continue;
            std::string rel = line;
            std::replace(rel.begin(), rel.end(), '\\', '/');
            // Line looks like UI\FrameDef\SmashUI\X.fdf -> take basename.
            size_t slash = rel.find_last_of('/');
            std::string base = slash == std::string::npos ? rel : rel.substr(slash + 1);
            for (const auto& r : roots) {
                std::ifstream f(r + "SmashUI/" + base);
                if (!f.good()) continue;
                std::stringstream cs;
                cs << f.rdbuf();
                parseContent(cs.str(), base);
                break;
            }
        }
    }
    const int unresolved = ResolveFdfInheritance(templates);
    std::cout << "FdfHudHost: console templates from " << files << " FDF files"
              << " (" << templates.size() << " templates, " << unresolved << " unresolved parents)"
              << std::endl;
    return templates.count("ConsoleUI") > 0;
}

bool FdfHudHost::LoadRetailConsole() {
    if (!builder_) return false;
    std::map<std::string, FrameDefinition> templates;
    const bool hasRetail = LoadConsoleTemplates(templates);
    consoleTemplates_ = templates;
    builder_->SetDecoratedStrings(consoleDecoratedStrings_);
    builder_->SetTemplates(consoleTemplates_);
    if (hasRetail) {
        // Inflate the ConsoleUI chrome first so its inner textures exist,
        // then let ConsoleHUD (ref MeleeUI.main) attach every other console
        // piece with Java-accurate anchors/sizes.
        if (!builder_->BuildFromTemplates(templates, "ConsoleUI")) return false;
    } else if (!mpq_) {
        return false;
    } else {
        // MPQ present but ConsoleUI template missing: fall through to the
        // procedural SmashConsoleUI-equivalent below.
        if (!builder_->BuildFromTemplates(templates, templates.begin()->first)) return false;
    }
    root_ = engine::ui::DynamicCastFrame<SimpleFrame>(builder_->GetFrame("ConsoleUI"));
    if (!root_) {
        // Retail ConsoleUI may be a plain FRAME; wrap lookup fallback.
        if (UIFrame* any = builder_->GetFrame("ConsoleUI")) {
            // Keep rendering through the generic root below.
            (void)any;
            return false;
        }
        return false;
    }
    gui_.rootFrame = root_;
    root_->SetSize((float)width_, (float)height_);

    // Java-faithful console assembly (replaces the old curated-children +
    // fixed-pixel grids): resource bar, upper bar, portrait, info panels,
    // queue/multiselect, inventory, command grid, tooltip, overlays.
    consoleRefs_ = ConsoleHudRefs{};
    std::vector<UIFrame*> codeOwned;
    if (!ConsoleHUD::Build(gui_, builder_, &textRenderer_, consoleTemplates_, root_,
                           width_, height_, codeOwned, consoleRefs_)) {
        return false;
    }
    // Esc menu + in-game dialogs start hidden (ref MeleeUI: EscMenu
    // panels invisible until the menu button opens them; Timer/Script
    // created on demand; Quest/Chat/Alliance hidden until upper buttons).
    // ConsoleHUD::Build already hid the ConsoleHudRefs dialogs; hide any
    // retail frames that were inflated directly by BuildFromTemplates too.
    if (consoleTemplates_.count("SmashEscMenu")) {
        builder_->ConstructFrame(consoleTemplates_.at("SmashEscMenu"), root_);
        if (UIFrame* esc = builder_->GetFrame("SmashEscMenu")) {
            if (consoleTemplates_.count("EscMenuBackdrop"))
                builder_->ConstructFrame(consoleTemplates_.at("EscMenuBackdrop"), esc);
            if (consoleTemplates_.count("EscMenuMainPanel"))
                builder_->ConstructFrame(consoleTemplates_.at("EscMenuMainPanel"), esc);
        }
        HideSubtree(builder_->GetFrame("SmashEscMenu"));
    }
    HideFrame(FindFrame("EscMenuBackdrop"));
    // EscMenuMainPanel visibility is managed by Open/CloseGameMenu (which
    // re-shows the backdrop+panel pair); hide direct-inflate leftovers that
    // ConsoleHUD did not own.
    if (!gameMenuOpen_) {
        if (consoleRefs_.escMenuBackdrop) consoleRefs_.escMenuBackdrop->SetVisible(false);
        if (consoleRefs_.escMenuMainPanel) consoleRefs_.escMenuMainPanel->SetVisible(false);
    }
    HideFrame(FindFrame("TimerDialog"));
    HideFrame(FindFrame("ScriptDialog"));
    HideFrame(FindFrame("QuestDialog"));
    HideFrame(FindFrame("ChatDialog"));
    HideFrame(FindFrame("AllianceDialog"));
    HideFrame(FindFrame("LogDialog"));
    HideFrame(FindFrame("CinematicPanel"));
    std::unordered_set<UIFrame*> seen;
    for (const auto& [name, frame] : builder_->GetAllFrames()) {
        if (frame && seen.insert(frame).second) ownedFrames_.emplace_back(frame);
    }
    for (UIFrame* f : codeOwned) {
        if (f && seen.insert(f).second) ownedFrames_.emplace_back(f);
    }
    // Blizzard placeholder text in retail FDF ("Retarded text" in
    // SimpleInfoPanel action labels): Java overwrites these on selection
    // change; blank them so they never render literally.
    for (const auto& [name, frame] : builder_->GetAllFrames()) {
        if (auto* text = engine::ui::DynamicCastFrame<TextFrame>(frame)) {
            if (text->GetText() == "Retarded text") text->SetText("");
        }
    }
    // Textured game-menu chrome (ref EscMenuBackdrop 0.048/0.256/0.01 and
    // EscMenuButtonBackdropTemplate 0.0125/0.256/0.005 in retail FDF).
    menuPanelBackdrop_ =
        MakeMenuBackdrop("GameMenuBackdrop", "EscMenuBackground", "EscMenuBorder", 0.048f, 0.256f, 0.01f);
    menuButtonBackdrop_ = MakeMenuBackdrop("GameMenuButtonBackdrop", "EscMenuButtonBackground",
                                           "EscMenuButtonBorder", 0.006f, 0.256f, 0.002f);
    menuButtonPushedBackdrop_ =
        MakeMenuBackdrop("GameMenuButtonPushedBackdrop", "EscMenuButtonPushedBackground",
                         "EscMenuButtonBorder", 0.006f, 0.256f, 0.002f);
    if (std::getenv("WARSMASH_DUMP_FRAMES")) {
        std::cout << "FdfHudHost console frames:" << std::endl;
        for (const auto& [name, frame] : builder_->GetAllFrames()) {
            UiRect b = frame ? frame->GetBounds() : UiRect{};
            std::cout << "  frame '" << name << "' " << b.x << "," << b.y << " " << b.w << "x" << b.h
                      << std::endl;
        }
        std::cout << "FdfHudHost code-owned frames:" << std::endl;
        for (const auto& [name, frame] : gui_.frameRegistry) {
            if (name.rfind("Smash", 0) != 0 && name.rfind("Minimap", 0) != 0 &&
                name.rfind("Portrait", 0) != 0 && name.rfind("Quests", 0) != 0 &&
                name.rfind("Menu", 0) != 0 && name.rfind("Allies", 0) != 0 &&
                name.rfind("Chat", 0) != 0)
                continue;
            if (builder_->GetFrame(name)) continue; // already listed above
            UiRect b = frame ? frame->GetBounds() : UiRect{};
            std::cout << "  frame '" << name << "' " << b.x << "," << b.y << " " << b.w << "x" << b.h
                      << std::endl;
        }
    }
    usingRetailLayout_ = true;
    BuildDynamicGrids(false);
    LayoutDynamicGrids();
    if (std::getenv("WARSMASH_DUMP_FRAMES")) {
        std::cout << "FdfHudHost positioned console frames:" << std::endl;
        auto dumpOne = [&](const std::string& name) {
            UIFrame* f = FindFrame(name);
            UiRect b = f ? f->GetBounds() : UiRect{};
            std::string extra;
            if (auto* tf = engine::ui::DynamicCastFrame<TextureFrame>(f)) {
                extra = tf->HasTexture() ? " textured" : " NO-TEXTURE";
            }
            if (auto* tf = engine::ui::DynamicCastFrame<TextFrame>(f)) {
                extra = " text='" + tf->GetText().substr(0, 24) + "'";
            }
            std::cout << "  '" << name << "' " << b.x << "," << b.y << " " << b.w << "x" << b.h
                      << (f ? (f->IsVisible() ? " vis" : " HID") : " MISSING") << extra << std::endl;
        };
        for (const char* n :
             {"ConsoleUI", "ResourceBarFrame", "ResourceBarGoldText", "UpperButtonBarFrame",
              "UpperButtonBarMenuButton", "UnitPortrait", "UnitPortraitModel",
              "UnitPortraitHitPointText", "UnitPortraitManaPointText", "ConsoleTexture01Top",
              "ConsoleTexture01Bottom", "ConsoleTexture02Bottom", "SmashSimpleInfoPanel",
              "SimpleInfoPanelUnitDetail", "SimpleNameValue", "SimpleClassValue", "SmashBuffStatusBar",
              "SmashCommandButton_0", "SmashCommandButton_11", "SmashInventoryButton_0",
              "SmashInventoryButton_5", "SmashInventoryBar", "SmashSimpleInventoryBar",
              "SmashBuildQueueIcon0", "SmashMultiSelectUnitIcon0", "Minimap", "SmashToolTip",
              "SmashConsoleInventoryCover", "SmashErrorMessageFrame", "TimeOfDayIndicator",
              "SmashSimpleInfoPanelIconDamage0", "SmashSimpleInfoPanelIconArmor",
              "SimpleInfoPanelIconDamage", "InfoPanelIconBackdrop", "InfoPanelIconLabel",
              "InfoPanelIconValue", "SmashArmorIconBackdrop", "SmashArmorIconValue"}) {
            dumpOne(n);
        }
        // Anonymous retail chrome (ConsoleUI inner Texture blocks) plus any
        // other large frame that could paint the console bands.
        for (const auto& [name, frame] : gui_.frameRegistry) {
            if (name.rfind("ConsoleUI/@anon", 0) != 0) continue;
            UiRect b = frame ? frame->GetBounds() : UiRect{};
            std::string extra;
            if (auto* tf = engine::ui::DynamicCastFrame<TextureFrame>(frame)) {
                extra = tf->HasTexture() ? " textured" : " NO-TEXTURE";
            }
            std::cout << "  '" << name << "' " << b.x << "," << b.y << " " << b.w << "x" << b.h
                      << (frame ? (frame->IsVisible() ? " vis" : " HID") : " MISSING") << extra
                      << std::endl;
        }
        for (const auto& [name, frame] : gui_.frameRegistry) {
            if (!frame || !frame->IsVisible()) continue;
            UiRect b = frame->GetBounds();
            if (b.w < 200 || b.h < 60) continue;
            if (name.rfind("ConsoleUI/@anon", 0) == 0) continue;
            std::string kind = "frame";
            std::string extra;
            if (engine::ui::DynamicCastFrame<SimpleFrame>(frame)) kind = "simple";
            if (auto* tf = engine::ui::DynamicCastFrame<TextureFrame>(frame)) {
                kind = "texture";
                extra = tf->HasTexture() ? " textured" : " NO-TEXTURE";
            }
            if (engine::ui::DynamicCastFrame<BackdropFrame>(frame)) kind = "backdrop";
            if (engine::ui::DynamicCastFrame<ButtonFrame>(frame)) kind = "button";
            if (engine::ui::DynamicCastFrame<TextFrame>(frame)) kind = "text";
            std::cout << "  BIG '" << name << "' [" << kind << "] " << b.x << "," << b.y << " "
                      << b.w << "x" << b.h << extra << std::endl;
        }
        for (const auto& [name, frame] : gui_.frameRegistry) {
            auto* tf = engine::ui::DynamicCastFrame<TextFrame>(frame);
            if (!tf || !frame->IsVisible() || tf->GetText().empty()) continue;
            UiRect b = frame->GetBounds();
            std::cout << "  TEXT '" << name << "' " << b.x << "," << b.y << " " << b.w << "x" << b.h
                      << " '" << tf->GetText().substr(0, 40) << "'" << std::endl;
        }
        if (std::getenv("WARSMASH_DEBUG_TREE")) {
            auto tree = [&](const std::string& name, auto&& self, int depth) -> void {
                UIFrame* f = FindFrame(name);
                if (!f) {
                    std::cout << std::string(depth * 2, ' ') << name << " MISSING" << std::endl;
                    return;
                }
                UiRect b = f->GetBounds();
                std::cout << std::string(depth * 2, ' ') << name << " " << b.x << "," << b.y << " "
                          << b.w << "x" << b.h << (f->IsVisible() ? "" : " HID") << std::endl;
                if (auto* s = engine::ui::DynamicCastFrame<SimpleFrame>(f)) {
                    for (UIFrame* c : s->GetChildren()) {
                        if (c) self(c->GetName().empty() ? std::string("@noname") : c->GetName(), self,
                                    depth + 1);
                    }
                }
            };
            std::cout << "FdfHudHost Damage0 subtree:" << std::endl;
            tree("SmashSimpleInfoPanelIconDamage0", tree, 0);
        }
    }
    return true;
}

UIFrame* FdfHudHost::FindFrame(const std::string& name) const {
    auto it = gui_.frameRegistry.find(name);
    return it != gui_.frameRegistry.end() ? it->second : nullptr;
}

UiRect FdfHudHost::FrameBounds(const std::string& name, const UiRect& fallback) const {
    UIFrame* f = FindFrame(name);
    if (!f || !f->IsVisible()) return fallback;
    UiRect b = f->GetBounds();
    if (b.w <= 0 || b.h <= 0) return fallback;
    return b;
}

void FdfHudHost::BuildFallbackConsole() {
    // Drop any partial retail inflate so the procedural SmashConsoleUI
    // equivalent starts from a clean registry (ref GameUI.createSimpleFrame).
    if (builder_) builder_->Clear();
    gui_.frameRegistry.clear();
    ClearOwnedFrames();
    auto root = std::make_unique<SimpleFrame>("ConsoleUI", nullptr);
    root_ = root.get();
    ownedFrames_.push_back(std::move(root));
    gui_.rootFrame = root_;
    gui_.frameRegistry["ConsoleUI"] = root_;
    consoleTemplates_.clear();
    consoleRefs_ = ConsoleHudRefs{};
    std::vector<UIFrame*> codeOwned;
    ConsoleHUD::Build(gui_, builder_, &textRenderer_, consoleTemplates_, root_,
                      width_, height_, codeOwned, consoleRefs_);
    for (UIFrame* f : codeOwned) ownedFrames_.emplace_back(f);
    usingRetailLayout_ = false;
    LayoutFallbackConsole();
    CacheConsoleRects();
}

void FdfHudHost::BuildDynamicGrids(bool /*withStaticText*/) {
    // Grids are owned by ConsoleHUD now (ref MeleeUI command/inventory/queue
    // icons built in code). Kept as a hook so LoadRetailConsole/Resize keep
    // their original call order; actual work happens in LayoutDynamicGrids.
}

void FdfHudHost::LayoutFallbackConsole() {
    if (!root_) return;
    root_->SetSize((float)width_, (float)height_);
    root_->PositionBounds(gui_);
    ConsoleHUD::Layout(gui_, consoleRefs_, width_, height_);
    // Second pass: FDF children of explicitly placed parents (info icon
    // frames in code-positioned wrappers) resolve against final rects.
    root_->PositionBounds(gui_);
    CacheConsoleRects();
}

void FdfHudHost::LayoutDynamicGrids() {
    if (!root_) return;
    // FDF tree first (retail geometry for texts/portrait/bars), then the
    // code-owned grids read the positioned FDF bounds (ref MeleeUI order).
    root_->SetSize((float)width_, (float)height_);
    root_->PositionBounds(gui_);
    ConsoleHUD::Layout(gui_, consoleRefs_, width_, height_);
    // Second pass: FDF children of explicitly placed parents (info icon
    // frames in code-positioned wrappers) resolve against final rects.
    root_->PositionBounds(gui_);
    // Open modal dialogs stay centered across resizes (ref Esc/Quest modal
    // behavior); hidden dialogs keep their FDF anchor for the next open.
    if (questOpen_ && consoleRefs_.questDialog) LayoutDialogFrame(consoleRefs_.questDialog);
    if (chatDialogOpen_ && consoleRefs_.chatDialog) LayoutDialogFrame(consoleRefs_.chatDialog);
    if (allianceDialogOpen_ && consoleRefs_.allianceDialog)
        LayoutDialogFrame(consoleRefs_.allianceDialog);
    if (gameMenuOpen_) {
        // DrawGameMenu owns rendering the in-game menu; keep FDF panel hidden.
        if (consoleRefs_.escMenuBackdrop) consoleRefs_.escMenuBackdrop->SetVisible(false);
        if (consoleRefs_.escMenuMainPanel) consoleRefs_.escMenuMainPanel->SetVisible(false);
    }
    root_->PositionBounds(gui_);
    CacheConsoleRects();
}

void FdfHudHost::CacheConsoleRects() {
    auto rectOf = [this](const std::string& name, const ScreenRect& fb) {
        UiRect b = FrameBounds(name, UiRect{fb.x, fb.y, fb.w, fb.h});
        return ScreenRect{b.x, b.y, b.w, b.h};
    };
    auto rectOfFrame = [](UIFrame* f) {
        if (!f || !f->IsVisible()) return ScreenRect{};
        UiRect b = f->GetBounds();
        if (b.w <= 0 || b.h <= 0) return ScreenRect{};
        return ScreenRect{b.x, b.y, b.w, b.h};
    };
    // Minimap/portrait prefer the ConsoleHUD frames (Java minimap rect /
    // UnitPortrait), falling back to legacy names.
    minimapRect_ = consoleRefs_.minimap ? rectOfFrame(consoleRefs_.minimap)
                                        : rectOf("Minimap", ScreenRect{8, height_ - 200.0f, 180, 180});
    UIFrame* portraitFrame = consoleRefs_.unitPortrait
                                 ? (UIFrame*)consoleRefs_.unitPortrait
                                 : FindFrame("PortraitButton");
    portraitRect_ = portraitFrame ? rectOfFrame(portraitFrame)
                                  : rectOf("PortraitButton", ScreenRect{200, height_ - 130.0f, 110, 110});
    // Command grid: SmashCommandButton_<i> (ref MeleeUI commandCard).
    for (int i = 0; i < 12; ++i) {
        std::string smash = "SmashCommandButton_" + std::to_string(i);
        std::string legacy = "CommandButton" + std::to_string(i);
        UiRect b = FrameBounds(smash, UiRect{});
        if (b.w <= 0) b = FrameBounds(legacy, UiRect{});
        commandRects_[i] = ScreenRect{b.x, b.y, b.w, b.h};
        if (consoleRefs_.commandButtons[i]) commandRects_[i] = rectOfFrame(consoleRefs_.commandButtons[i]);
    }
    // Upper buttons: retail UpperButtonBar* names first, then ConsoleHUD refs.
    static const char* kRetailUppers[4] = {"UpperButtonBarQuestsButton", "UpperButtonBarMenuButton",
                                           "UpperButtonBarAlliesButton", "UpperButtonBarChatButton"};
    static const char* kLegacyUppers[4] = {"QuestsButton", "MenuButton", "AlliesButton", "ChatButton"};
    for (int i = 0; i < 4; ++i) {
        UiRect b = FrameBounds(kRetailUppers[i], UiRect{});
        if (b.w <= 0) b = FrameBounds(kLegacyUppers[i], UiRect{});
        upperButtonRects_[i] = ScreenRect{b.x, b.y, b.w, b.h};
        if (consoleRefs_.upperButtons[i]) {
            ScreenRect r = rectOfFrame(consoleRefs_.upperButtons[i]);
            if (r.w > 0) upperButtonRects_[i] = r;
        }
    }
    for (int i = 0; i < 5; ++i) {
        std::string smash = "SmashBuildQueueIcon" + std::to_string(i);
        std::string legacy = "QueueButton" + std::to_string(i);
        UiRect b = FrameBounds(smash, UiRect{});
        if (b.w <= 0) b = FrameBounds(legacy, UiRect{});
        queueRects_[i] = ScreenRect{b.x, b.y, b.w, b.h};
        if (consoleRefs_.queueButtons[i]) queueRects_[i] = rectOfFrame(consoleRefs_.queueButtons[i]);
        minimapButtonRects_[i] = rectOf("MinimapButton" + std::to_string(i), ScreenRect{});
        if (consoleRefs_.minimapButtons[i]) {
            ScreenRect r = rectOfFrame(consoleRefs_.minimapButtons[i]);
            if (r.w > 0) minimapButtonRects_[i] = r;
        }
    }
    for (int i = 0; i < 6; ++i) {
        std::string smash = "SmashInventoryButton_" + std::to_string(i);
        std::string legacy = "InventoryButton" + std::to_string(i);
        UiRect b = FrameBounds(smash, UiRect{});
        if (b.w <= 0) b = FrameBounds(legacy, UiRect{});
        inventoryRects_[i] = ScreenRect{b.x, b.y, b.w, b.h};
        if (consoleRefs_.inventoryButtons[i]) inventoryRects_[i] = rectOfFrame(consoleRefs_.inventoryButtons[i]);
    }
    for (int i = 0; i < 12; ++i) {
        std::string smash = "SmashMultiSelectUnitIcon" + std::to_string(i);
        std::string legacy = "MultiSelectButton" + std::to_string(i);
        UiRect b = FrameBounds(smash, UiRect{});
        if (b.w <= 0) b = FrameBounds(legacy, UiRect{});
        multiSelectRects_[i] = ScreenRect{b.x, b.y, b.w, b.h};
        if (consoleRefs_.multiSelectButtons[i] && multiSelectVisible_[i])
            multiSelectRects_[i] = rectOfFrame(consoleRefs_.multiSelectButtons[i]);
    }
}

std::string FdfHudHost::IconToMpqPath(const std::string& uri) {
    if (uri.empty()) return {};
    // Disk cache exports from the old pipeline resolve directly.
    if (uri.rfind("assets/", 0) == 0) return uri;
    std::string p = uri;
    std::replace(p.begin(), p.end(), '/', '\\');
    return p;
}

std::shared_ptr<Texture> FdfHudHost::ResolveIcon(const std::string& uri) {
    if (uri.empty()) return nullptr;
    auto it = textures_.find(uri);
    if (it != textures_.end()) return it->second;
    std::shared_ptr<Texture> tex;
    const std::string diskPath = std::string(PROJECT_SOURCE_DIR) + "/" + uri;
    // 1) On-disk PNG cache (exported by MainUI icon pipeline).
    {
        FILE* f = std::fopen(diskPath.c_str(), "rb");
        if (f) {
            std::fseek(f, 0, SEEK_END);
            long size = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            if (size > 0 && size < 16 * 1024 * 1024) {
                std::vector<uint8_t> buf((size_t)size);
                if (std::fread(buf.data(), 1, buf.size(), f) == buf.size()) {
                    tex = std::make_shared<Texture>();
                    if (!tex->LoadFromMemory(buf.data(), buf.size())) tex.reset();
                }
            }
            std::fclose(f);
        }
    }
    // 2) Skin-aware MPQ load (ref GameUI.loadTexture + trySkinField): skin
    // keys (GoldIcon, BTN paths, ...) resolve, plain paths pass through.
    if (!tex && mpq_ && builder_) {
        tex = builder_->LoadSkinTexture(uri);
    }
    // 3) Legacy straight MPQ read for paths the skin loader skips.
    if (!tex && mpq_) {
        std::vector<uint8_t> buf;
        if (mpq_->ReadFile(IconToMpqPath(uri), buf) && !buf.empty()) {
            tex = std::make_shared<Texture>();
            if (!tex->LoadFromMemory(buf.data(), buf.size())) tex.reset();
        }
    }
    if (tex) textures_[uri] = tex;
    return tex;
}

bool FdfHudHost::BindText(const std::string& name, const std::string& value) {
    UIFrame* f = FindFrame(name);
    auto* text = engine::ui::DynamicCastFrame<TextFrame>(f);
    if (!text) return false;
    text->SetText(value);
    return true;
}

bool FdfHudHost::BindIcon(std::initializer_list<const char*> names,
                          const std::shared_ptr<Texture>& tex) {
    if (!tex) return false;
    bool bound = false;
    for (const char* name : names) {
        UIFrame* f = FindFrame(name);
        if (!f) continue;
        if (auto* tf = engine::ui::DynamicCastFrame<TextureFrame>(f)) {
            tf->SetTexture(tex);
            bound = true;
        } else if (auto* bf = engine::ui::DynamicCastFrame<BackdropFrame>(f)) {
            bf->SetBackground(tex);
            bound = true;
        }
    }
    return bound;
}

void FdfHudHost::SetResources(int gold, int lumber, int foodUsed, int foodCap,
                              const std::string& timeText) {
    gold_ = gold; lumber_ = lumber; foodUsed_ = foodUsed; foodCap_ = foodCap;
    timeValue_ = timeText;
    // Java parity: live values go INTO the retail frames (goldChanged() etc.)
    // so the FDF layout (not procedural fallback positions) shows them.
    BindText("ResourceBarGoldText", std::to_string(gold_));
    BindText("ResourceBarLumberText", std::to_string(lumber_));
    BindText("ResourceBarSupplyText", std::to_string(foodUsed_) + "/" + std::to_string(foodCap_));
}

void FdfHudHost::SetUpkeepLevel(int level) {
    upkeepLevel_ = level;
    // Retail upkeep indicator text (ref melee upkeep display): green while
    // untaxed, yellow at low upkeep, red at high upkeep.
    const char* text = "No Upkeep";
    uint32_t color = 0xFF00FF00u;
    if (level >= 2) {
        text = "High Upkeep";
        color = 0xFFFF0000u;
    } else if (level == 1) {
        text = "Low Upkeep";
        color = 0xFFFFFF00u;
    }
    if (!BindText("ResourceBarUpkeepText", text)) return;
    if (UIFrame* f = FindFrame("ResourceBarUpkeepText")) {
        if (auto* tf = engine::ui::DynamicCastFrame<TextFrame>(f)) tf->SetColor(color);
    }
}

void FdfHudHost::SetSelection(const std::string& name, int level, float hp, float maxHp,
                              float mp, float maxMp, bool heroVisible, int xp, int maxXp,
                              int strBase, int strBonus, int agiBase, int agiBonus,
                              int intBase, int intBonus, const HudInfoStats& infoStats) {
    unitName_ = name; unitLevel_ = level;
    hp_ = hp; maxHp_ = maxHp; mp_ = mp; maxMp_ = maxMp;
    heroVisible_ = heroVisible;
    heroXp_ = xp; heroMaxXp_ = maxXp;
    heroStrBase_ = strBase; heroStrBonus_ = strBonus;
    heroAgiBase_ = agiBase; heroAgiBonus_ = agiBonus;
    heroIntBase_ = intBase; heroIntBonus_ = intBonus;
    infoStats_ = infoStats;
    const bool hasSelection = !name.empty();
    BindText("SimpleNameValue", name);
    if (consoleRefs_.simpleInfoPanelUnitDetail) {
        consoleRefs_.simpleInfoPanelUnitDetail->SetVisible(hasSelection);
    }
    if (consoleRefs_.buffStatusBar) {
        consoleRefs_.buffStatusBar->SetVisible(hasSelection && buffCount_ > 0);
    }
    char statBuf[64];
    if (maxHp_ > 0) {
        std::snprintf(statBuf, sizeof(statBuf), "%.0f / %.0f", hp_, maxHp_);
        BindText("UnitPortraitHitPointText", statBuf);
        if (auto* tf = engine::ui::DynamicCastFrame<TextFrame>(FindFrame("UnitPortraitHitPointText"))) {
            tf->SetVisible(true);
        }
    } else {
        BindText("UnitPortraitHitPointText", "");
        if (UIFrame* f = FindFrame("UnitPortraitHitPointText")) f->SetVisible(false);
    }
    if (maxMp_ > 0) {
        std::snprintf(statBuf, sizeof(statBuf), "%.0f / %.0f", mp_, maxMp_);
        BindText("UnitPortraitManaPointText", statBuf);
        if (auto* tf = engine::ui::DynamicCastFrame<TextFrame>(FindFrame("UnitPortraitManaPointText"))) {
            tf->SetVisible(true);
        }
    } else {
        BindText("UnitPortraitManaPointText", "");
        if (UIFrame* f = FindFrame("UnitPortraitManaPointText")) f->SetVisible(false);
    }
    // Hero attribute block only for heroes (retail SimpleInfoPanelIconHero).
    if (UIFrame* hero = FindFrame("SmashSimpleInfoPanelIconHero")) {
        hero->SetVisible(hasSelection && heroVisible_);
    }
    if (heroVisible_ && hasSelection) {
        char attrBuf[32];
        std::snprintf(attrBuf, sizeof(attrBuf), "%d", heroStrBase_ + heroStrBonus_);
        BindText("InfoPanelIconHeroStrengthValue", attrBuf);
        std::snprintf(attrBuf, sizeof(attrBuf), "%d", heroAgiBase_ + heroAgiBonus_);
        BindText("InfoPanelIconHeroAgilityValue", attrBuf);
        std::snprintf(attrBuf, sizeof(attrBuf), "%d", heroIntBase_ + heroIntBonus_);
        BindText("InfoPanelIconHeroIntellectValue", attrBuf);
        if (auto* xp = engine::ui::DynamicCastFrame<StatusBarFrame>(FindFrame("SimpleHeroLevelBar"))) {
            xp->SetMax(heroMaxXp_ > 0 ? (float)heroMaxXp_ : 1.0f);
            xp->SetValue((float)heroXp_);
        }
    }
    // Attack1 block (the complete retail icon children live here): damage
    // value + type backdrop. Damage1 has no second-attack data, hide it.
    if (UIFrame* dmg1 = FindFrame("SmashSimpleInfoPanelIconDamage1")) {
        dmg1->SetVisible(false);
    }
    const bool showStats = hasSelection && (infoStats_.visible || heroVisible_);
    const bool hasAttack = !infoStats_.attackText.empty();
    const bool hasArmor = !infoStats_.armorText.empty();

    if (UIFrame* hero = FindFrame("SmashSimpleInfoPanelIconHero")) hero->SetVisible(hasSelection && heroVisible_);
    if (UIFrame* hero2 = FindFrame("SimpleInfoPanelIconHero")) hero2->SetVisible(hasSelection && heroVisible_);

    if (UIFrame* dmg0 = FindFrame("SmashSimpleInfoPanelIconDamage0")) dmg0->SetVisible(showStats && hasAttack);
    if (UIFrame* dmgR = FindFrame("SimpleInfoPanelIconDamage")) dmgR->SetVisible(showStats && hasAttack);

    if (UIFrame* armor = FindFrame("SmashSimpleInfoPanelIconArmor")) {
        armor->SetVisible(showStats && hasArmor);
        // If unit has no attack (e.g. Town Hall), move armor up to top slot
        UIFrame* unitDetail = consoleRefs_.simpleInfoPanelUnitDetail
                                  ? (UIFrame*)consoleRefs_.simpleInfoPanelUnitDetail
                                  : (UIFrame*)consoleRefs_.smashSimpleInfoPanel;
        if (unitDetail) {
            UiRect db = unitDetail->GetBounds();
            if (db.w > 0) {
                float fdfY = hasAttack ? -0.0705f : -0.040f;
                armor->SetPosition(db.x + ConsoleHUD::ConvertX(width_, 0.0f),
                                   db.y - ConsoleHUD::ConvertY(height_, fdfY));
                armor->SetSize(ConsoleHUD::ConvertX(width_, 0.1f), ConsoleHUD::ConvertY(height_, 0.030125f));

                UiRect aw = armor->GetBounds();
                const float s = std::min(aw.w, aw.h);
                if (UIFrame* bd = consoleRefs_.armorIconBackdrop ? consoleRefs_.armorIconBackdrop : FindFrame("SmashArmorIconBackdrop")) {
                    bd->SetPosition(aw.x, aw.y);
                    bd->SetSize(s, s);
                }
                const float tx = aw.x + s + ConsoleHUD::ConvertX(width_, 0.004f);
                const float tw = std::max(10.0f, aw.w - s - ConsoleHUD::ConvertX(width_, 0.004f));
                const float lineH = aw.h * 0.48f;
                if (UIFrame* tl = consoleRefs_.armorIconLabel ? (UIFrame*)consoleRefs_.armorIconLabel : FindFrame("SmashArmorIconLabel")) {
                    tl->SetPosition(tx, aw.y);
                    tl->SetSize(tw, lineH);
                }
                if (UIFrame* tv = consoleRefs_.armorIconValue ? (UIFrame*)consoleRefs_.armorIconValue : FindFrame("SmashArmorIconValue")) {
                    tv->SetPosition(tx, aw.y + lineH);
                    tv->SetSize(tw, lineH);
                }
            }
        }
    }
    if (UIFrame* armorR = FindFrame("SimpleInfoPanelIconArmor")) armorR->SetVisible(showStats && hasArmor);

    if (showStats) {
        if (hasAttack) {
            if (consoleRefs_.attack1IconLabel) consoleRefs_.attack1IconLabel->SetText("Damage:");
            else BindText("InfoPanelIconLabel", "Damage:");

            if (consoleRefs_.attack1IconValue) consoleRefs_.attack1IconValue->SetText(infoStats_.attackText);
            else BindText("InfoPanelIconValue", infoStats_.attackText);
            BindText("InfoPanelIconLevel", "");

            if (!infoStats_.attackIconKey.empty() && builder_) {
                auto tex = builder_->LoadSkinTexture(infoStats_.attackIconKey);
                if (auto* tf = engine::ui::DynamicCastFrame<TextureFrame>(consoleRefs_.attack1IconBackdrop)) {
                    tf->SetTexture(tex);
                }
                BindIcon({"InfoPanelIconBackdrop", "Attack1IconBackdrop"}, tex);
            }
        }
        if (hasArmor) {
            if (consoleRefs_.armorIconLabel) consoleRefs_.armorIconLabel->SetText("Armor:");
            else BindText("SmashArmorIconLabel", "Armor:");

            if (consoleRefs_.armorIconValue) consoleRefs_.armorIconValue->SetText(infoStats_.armorText);
            else BindText("SmashArmorIconValue", infoStats_.armorText);

            if (!infoStats_.armorIconKey.empty() && builder_) {
                auto tex = builder_->LoadSkinTexture(infoStats_.armorIconKey);
                if (auto* tf = engine::ui::DynamicCastFrame<TextureFrame>(consoleRefs_.armorIconBackdrop)) {
                    tf->SetTexture(tex);
                }
                BindIcon({"SmashArmorIconBackdrop", "ArmorIconBackdrop"}, tex);
            }
        }
    }
    if (std::getenv("WARSMASH_DEBUG_SELECTION")) {
        std::cout << "FdfHudHost::SetSelection name='" << name << "' showStats=" << showStats
                  << " hero=" << heroVisible_ << " attack='" << infoStats_.attackText << "' armor='"
                  << infoStats_.armorText << "' hp=" << hp_ << "/" << maxHp_ << std::endl;
    }
}

void FdfHudHost::SetTooltip(const std::string& text, float screenX, float screenY, bool visible,
                            const HudResourceCosts& costs) {
    tooltipValue_ = text; tooltipX_ = screenX; tooltipY_ = screenY;
    tooltipVisible_ = visible; tooltipCosts_ = costs;
}

void FdfHudHost::SetChatOpen(bool open) { chatOpen_ = open; }
void FdfHudHost::SetChatInput(const std::string& text) { chatInputText_ = text; }
std::string FdfHudHost::GetChatInput() const { return chatInputText_; }
void FdfHudHost::SetChatMessages(const std::vector<std::string>& messages) { chatMessages_ = messages; }

void FdfHudHost::SetCommandSlot(int index, const HudCommandSlot& slot) {
    if (index >= 0 && index < 12) commandStates_[index] = slot;
}
void FdfHudHost::SetHoveredCommand(int index) { hoveredCommandIndex_ = index; }
void FdfHudHost::SetPressedCommand(int index) { pressedCommandIndex_ = index; }
void FdfHudHost::SetHoveredUpperButton(int index) { hoveredUpperButtonIndex_ = index; }
void FdfHudHost::SetPressedUpperButton(int index) { pressedUpperButtonIndex_ = index; }
void FdfHudHost::SetPressedMinimapButton(int index) { pressedMinimapButtonIndex_ = index; }
void FdfHudHost::ActivateMinimapButton(int index) {
    if (index >= 0 && index < 5) minimapButtonModes_[index] = 1 - minimapButtonModes_[index];
}

void FdfHudHost::SetMultiSelection(const std::vector<MultiSelectUnitInfo>& units, int primaryHandle) {
    multiSelectPrimaryHandle_ = primaryHandle;
    for (int i = 0; i < 12; ++i) {
        multiSelectVisible_[i] = i < (int)units.size();
        if (multiSelectVisible_[i]) multiSelectStates_[i] = units[i];
        if (consoleRefs_.multiSelectButtons[i]) {
            consoleRefs_.multiSelectButtons[i]->SetVisible(multiSelectVisible_[i]);
        }
    }
}

void FdfHudHost::SetQueue(const std::vector<HudQueueItem>& items, float progressRatio) {
    queueProgressRatio_ = progressRatio;
    for (int i = 0; i < 5; ++i) {
        if (i < (int)items.size()) queueStates_[i] = items[i];
        else queueStates_[i] = HudQueueItem{};
        if (consoleRefs_.queueButtons[i]) {
            consoleRefs_.queueButtons[i]->SetVisible(queueStates_[i].visible);
        }
    }
    if (consoleRefs_.queueWorkerButton) {
        consoleRefs_.queueWorkerButton->SetVisible(queueStates_[1].visible);
    }
}

void FdfHudHost::SetInventory(const std::vector<HudInventoryItem>& items, int capacity, bool available) {
    inventoryCapacity_ = capacity;
    inventoryAvailable_ = available;
    for (int i = 0; i < 6; ++i) {
        if (i < (int)items.size()) inventoryStates_[i] = items[i];
        else inventoryStates_[i] = HudInventoryItem{};
    }
}

void FdfHudHost::SetPortraitIcon(const std::string& iconUri) { portraitIconUri_ = iconUri; }

void FdfHudHost::SetBuffs(const std::vector<HudBuffItem>& buffs) {
    buffCount_ = std::min<int>((int)buffs.size(), 10);
    for (int i = 0; i < 10; ++i) {
        buffStates_[i] = i < (int)buffs.size() ? buffs[i] : HudBuffItem{};
    }
    // Empty buff slots stay hidden (ref Java, which only shows live icons).
    for (int i = 0; i < 10; ++i) {
        if (consoleRefs_.buffButtons[i]) consoleRefs_.buffButtons[i]->SetVisible(i < buffCount_);
    }
    if (consoleRefs_.buffStatusBar) consoleRefs_.buffStatusBar->SetVisible(!unitName_.empty() && buffCount_ > 0);
}

void FdfHudHost::DrawText(float x, float y, const std::string& text, uint32_t color, float size) {
    if (text.empty() || !textRenderer_.IsValid()) return;
    textRenderer_.RenderText(gui_.renderer, x, y, text.c_str(), color, size);
}

void FdfHudHost::DrawConsoleChrome() {
    UiRenderer& r = gui_.renderer;
    const float W = (float)width_, H = (float)height_;
    // Retail FDF chrome owns the console background (ConsoleUI textures);
    // the procedural backdrops below are the MPQ-less fallback only.
    if (!usingRetailLayout_) {
        // Bottom console backdrop.
        const float stripH = H * 0.27f;
        r.DrawQuad(0, H - stripH, W, stripH, 0xCC101018);
        r.DrawQuad(0, H - stripH, W, 2, 0xFF6A5A2A);
        // Top resource bar backdrop.
        r.DrawQuad(W - 328, 0, 328, 52, 0xCC101018);
    }
    const ScreenRect& mm = minimapRect_;
    if (mm.w > 0 && mm.h > 0) {
        r.DrawQuad(mm.x - 2, mm.y - 2, mm.w + 4, 2, 0xFF6A5A2A);
        r.DrawQuad(mm.x - 2, mm.y + mm.h, mm.w + 4, 2, 0xFF6A5A2A);
        r.DrawQuad(mm.x - 2, mm.y, 2, mm.h, 0xFF6A5A2A);
        r.DrawQuad(mm.x + mm.w, mm.y, 2, mm.h, 0xFF6A5A2A);
    }
    const ScreenRect& pr = portraitRect_;
    if (!usingRetailLayout_) {
        r.DrawQuad(pr.x - 2, pr.y - 2, pr.w + 4, pr.h + 4, 0xFF6A5A2A);
    }
    // Portrait icon.
    if (auto icon = ResolveIcon(portraitIconUri_)) {
        if (icon->IsValid()) r.DrawTexturedQuad(pr.x, pr.y, pr.w, pr.h, icon->GetImage());
    }
    // Command buttons.
    for (int i = 0; i < 12; ++i) {
        const auto& st = commandStates_[i];
        const ScreenRect& rc = commandRects_[i];
        if (rc.w <= 0) continue;
        if (!st.visible) {
            r.DrawQuad(rc.x, rc.y, rc.w, rc.h, 0x66181820);
            continue;
        }
        uint32_t bg = 0xFF2A2A38;
        if (i == pressedCommandIndex_) bg = 0xFF1A1A26;
        else if (i == hoveredCommandIndex_) bg = 0xFF3E3E55;
        r.DrawQuad(rc.x, rc.y, rc.w, rc.h, bg);
        if (auto icon = ResolveIcon(st.iconUri)) {
            if (icon->IsValid()) {
                uint32_t tint = st.enabled ? 0xFFFFFFFF : 0xFF777777;
                r.DrawTexturedQuad(rc.x + 2, rc.y + 2, rc.w - 4, rc.h - 4, icon->GetImage(),
                                   0, 0, 1, 1, tint);
            }
        }
        if (st.active) r.DrawQuad(rc.x, rc.y, rc.w, 3, 0xFF40C040);
        if (st.autocastActive) {
            const uint32_t goldBright = 0xFFFFF040;
            // Base border outline around command slot
            r.DrawQuad(rc.x - 1, rc.y - 1, rc.w + 2, 1, 0xFF806010);
            r.DrawQuad(rc.x - 1, rc.y + rc.h, rc.w + 2, 1, 0xFF806010);
            r.DrawQuad(rc.x - 1, rc.y - 1, 1, rc.h + 2, 0xFF806010);
            r.DrawQuad(rc.x + rc.w, rc.y - 1, 1, rc.h + 2, 0xFF806010);

            // 4 animated circulating beads rotating clockwise around the perimeter (Warcraft 3 retail effect)
            const float peri = (rc.w + rc.h) * 2.0f;
            const float speed = 45.0f;
            float baseOffset = std::fmod((float)(uiClock_ * speed), peri);

            auto drawBeadAtDistance = [&](float dist) {
                while (dist < 0.0f) dist += peri;
                dist = std::fmod(dist, peri);
                float bx = rc.x, by = rc.y;
                if (dist < rc.w) {
                    bx = rc.x + dist;
                    by = rc.y - 1;
                } else if (dist < rc.w + rc.h) {
                    bx = rc.x + rc.w;
                    by = rc.y + (dist - rc.w);
                } else if (dist < rc.w * 2.0f + rc.h) {
                    bx = rc.x + rc.w - (dist - (rc.w + rc.h));
                    by = rc.y + rc.h;
                } else {
                    bx = rc.x - 1;
                    by = rc.y + rc.h - (dist - (rc.w * 2.0f + rc.h));
                }
                r.DrawQuad(bx - 2, by - 2, 5, 5, goldBright);
            };

            for (int b = 0; b < 4; ++b) {
                drawBeadAtDistance(baseOffset + (float)b * (peri * 0.25f));
            }
        }
        if (st.cooldownMax > 0 && st.cooldownRemaining > 0) {
            float f = std::min(1.0f, st.cooldownRemaining / st.cooldownMax);
            r.DrawCooldownSweep(rc.x, rc.y, rc.w, rc.h, f, 0xBB000000);
        }
        if (st.numberOverlay >= 0)
            DrawText(rc.x + 4, rc.y + rc.h - 16, std::to_string(st.numberOverlay), 0xFFFFFF00, 12);
        if (!st.enabled) r.DrawQuad(rc.x, rc.y, rc.w, rc.h, 0x66000000);
    }
    // Upper buttons (procedural fallback only when retail FDF is absent).
    if (!usingRetailLayout_) {
        static const char* upperLabels[4] = {"Quests", "Menu (F10)", "Allies", "Chat"};
        for (int i = 0; i < 4; ++i) {
            const ScreenRect& rc = upperButtonRects_[i];
            if (rc.w <= 0) continue;
            uint32_t bg = (i == pressedUpperButtonIndex_) ? 0xFF1A1A26
                : (i == hoveredUpperButtonIndex_) ? 0xFF3E3E55 : 0xFF2A2A38;
            r.DrawQuad(rc.x, rc.y, rc.w, rc.h, bg);
            float tw = textRenderer_.IsValid() ? textRenderer_.MeasureText(upperLabels[i], 12) : (float)std::strlen(upperLabels[i]) * 7.0f;
            float tx = rc.x + (rc.w - tw) * 0.5f;
            DrawText(tx, rc.y + 4, upperLabels[i], 0xFFFFCC66, 12);
        }
    } else {
        // TimeOfDayIndicator central arch backdrop and day/night dial
        const float clockW = (0.116f / 0.8f) * W;
        const float clockH = (0.078f / 0.6f) * H;
        const float clockX = (W - clockW) * 0.5f;

        auto archTex = ResolveIcon("ui\\Console\\Human\\HumanUITile-TimeIndicatorFrame.blp");
        auto dialTex = ResolveIcon("Textures\\HumanUITile-TimeIndicator.blp");

        int hour = 12, minute = 0;
        if (timeValue_.size() >= 5 && timeValue_[2] == ':') {
            hour = std::atoi(timeValue_.substr(0, 2).c_str());
            minute = std::atoi(timeValue_.substr(3, 2).c_str());
        } else if (timeValue_.size() >= 2) {
            hour = std::atoi(timeValue_.substr(0, 2).c_str());
        }
        float todNorm = (hour + minute / 60.0f) / 24.0f;
        float centerU = std::fmod(todNorm + 0.5f, 1.0f);

        if (archTex && archTex->IsValid()) {
            r.DrawQuad(clockX + clockW * 0.38f, clockH * 0.12f, clockW * 0.24f, clockH * 0.44f, 0xFF0A0E1A);
            if (dialTex && dialTex->IsValid()) {
                const float uSpan = 0.24f;
                const float u0 = centerU - uSpan * 0.5f;
                const float u1 = centerU + uSpan * 0.5f;
                r.DrawTexturedQuad(clockX + clockW * 0.37f, clockH * 0.10f, clockW * 0.26f, clockH * 0.48f,
                                   dialTex->GetImage(), u0, 0.05f, u1, 0.95f);
            }
            r.DrawTexturedQuad(clockX, 0.0f, clockW, clockH, archTex->GetImage(), 0.0f, 0.0f, 1.0f, 1.0f);
        } else {
            r.DrawQuad(clockX, 0.0f, clockW, clockH * 0.72f, 0xEE101018);
            r.DrawQuad(clockX, 0.0f, clockW, 2.0f, 0xFF6A5A2A);
            r.DrawQuad(clockX, clockH * 0.72f - 2.0f, clockW, 2.0f, 0xFF3A3A4A);
            r.DrawQuad(clockX, 0.0f, 2.0f, clockH * 0.72f, 0xFF3A3A4A);
            r.DrawQuad(clockX + clockW - 2.0f, 0.0f, 2.0f, clockH * 0.72f, 0xFF3A3A4A);
        }
    }
    // Minimap buttons.
    for (int i = 0; i < 5; ++i) {
        const ScreenRect& rc = minimapButtonRects_[i];
        if (rc.w <= 0) continue;
        uint32_t bg = (i == pressedMinimapButtonIndex_) ? 0xFF1A1A26
            : (minimapButtonModes_[i] ? 0xFF3E5A3E : 0xFF2A2A38);
        r.DrawQuad(rc.x, rc.y, rc.w, rc.h, bg);
    }
    // Queue / inventory / buffs / multiselect occupancy.
    for (int i = 0; i < 5; ++i) {
        const ScreenRect& rc = queueRects_[i];
        if (rc.w <= 0 || !queueStates_[i].visible) continue;
        r.DrawQuad(rc.x, rc.y, rc.w, rc.h, 0xFF2A2A38);
        if (auto icon = ResolveIcon(queueStates_[i].iconUri)) {
            if (icon->IsValid()) r.DrawTexturedQuad(rc.x + 1, rc.y + 1, rc.w - 2, rc.h - 2, icon->GetImage());
        }
    }
    if (queueProgressRatio_ > 0 && queueRects_[0].w > 0) {
        const ScreenRect& rc = queueRects_[0];
        r.DrawQuad(rc.x, rc.y + rc.h - 4, rc.w * queueProgressRatio_, 4, 0xFF40C040);
    }
    if (inventoryAvailable_) {
        for (int i = 0; i < 6 && i < inventoryCapacity_; ++i) {
            const ScreenRect& rc = inventoryRects_[i];
            if (rc.w <= 0) continue;
            r.DrawQuad(rc.x, rc.y, rc.w, rc.h, 0xFF2A2A38);
            const auto& it = inventoryStates_[i];
            if (it.occupied) {
                if (auto icon = ResolveIcon(it.iconUri)) {
                    if (icon->IsValid())
                        r.DrawTexturedQuad(rc.x + 1, rc.y + 1, rc.w - 2, rc.h - 2, icon->GetImage());
                }
                if (it.charges > 1) DrawText(rc.x + 4, rc.y + rc.h - 15, std::to_string(it.charges), 0xFFFFFFFF, 11);
            }
        }
    }
    for (int i = 0; i < buffCount_ && i < (int)buffStates_.size(); ++i) {
        UiRect b{};
        if (i < (int)consoleRefs_.buffButtons.size() && consoleRefs_.buffButtons[i]) {
            b = consoleRefs_.buffButtons[i]->GetBounds();
        }
        if (b.w <= 0) {
            char name[32];
            std::snprintf(name, sizeof(name), "SmashBuffStatusBarIcon%d", i);
            b = FrameBounds(name, UiRect{});
            if (b.w <= 0) {
                std::snprintf(name, sizeof(name), "BuffButton%d", i);
                b = FrameBounds(name, UiRect{});
            }
        }
        if (b.w <= 0) continue;
        r.DrawQuad(b.x, b.y, b.w, b.h, buffStates_[i].positive ? 0xFF2A4A2A : 0xFF4A2A2A);
        if (auto icon = ResolveIcon(buffStates_[i].iconUri)) {
            if (icon->IsValid()) r.DrawTexturedQuad(b.x + 1, b.y + 1, b.w - 2, b.h - 2, icon->GetImage());
        }
    }
    for (int i = 0; i < 12; ++i) {
        if (!multiSelectVisible_[i]) continue;
        const ScreenRect& rc = multiSelectRects_[i];
        if (rc.w <= 0) continue;
        const auto& ms = multiSelectStates_[i];
        r.DrawQuad(rc.x, rc.y, rc.w, rc.h, ms.handleId == multiSelectPrimaryHandle_ ? 0xFF5A7A5A : 0xFF2A2A38);
        if (auto icon = ResolveIcon(ms.icon)) {
            if (icon->IsValid()) {
                r.DrawTexturedQuad(rc.x + 1, rc.y + 1, rc.w - 2, rc.h - 2, icon->GetImage());
            }
        }
        float barH = 3.0f;
        float barY = rc.y + rc.h - (ms.mpRatio > 0.0f ? (barH * 2.0f + 2.0f) : (barH + 1.0f));
        r.DrawQuad(rc.x + 1, barY, rc.w - 2, barH, 0xFF104010);
        r.DrawQuad(rc.x + 1, barY, (rc.w - 2) * std::clamp(ms.hpRatio, 0.0f, 1.0f), barH, 0xFF30D030);
        if (ms.mpRatio > 0.0f) {
            float mpBarY = rc.y + rc.h - barH - 1.0f;
            r.DrawQuad(rc.x + 1, mpBarY, rc.w - 2, barH, 0xFF102050);
            r.DrawQuad(rc.x + 1, mpBarY, (rc.w - 2) * std::clamp(ms.mpRatio, 0.0f, 1.0f), barH, 0xFF3080F0);
        }
    }
}

void FdfHudHost::DrawTexts() {
    char buf[128];
    if (!usingRetailLayout_) {
        // Small resource icons ahead of the numbers (procedural fallback).
        auto drawResource = [&](const char* frame, const char* skinKey, float x,
                                const std::string& text, uint32_t color) {
            if (auto icon = ResolveIcon(skinKey)) {
                if (icon->IsValid()) gui_.renderer.DrawTexturedQuad(x, 4, 16, 16, icon->GetImage());
            }
            if (!BindText(frame, text)) DrawText(x + 19.0f, 6, text, color, 13);
        };
        std::snprintf(buf, sizeof(buf), "%d", gold_);
        drawResource("ResourceBarGoldText", "ToolTipGoldIcon", width_ - 336.0f, buf, 0xFFFFCC66);
        std::snprintf(buf, sizeof(buf), "%d", lumber_);
        drawResource("ResourceBarLumberText", "ToolTipLumberIcon", width_ - 242.0f, buf, 0xFF66CC66);
        std::snprintf(buf, sizeof(buf), "%d/%d", foodUsed_, foodCap_);
        drawResource("ResourceBarSupplyText", "ToolTipSupplyIcon", width_ - 148.0f, buf, 0xFFFFFFFF);
        if (!BindText("TimeIndicatorText", timeValue_)) {
            DrawText(width_ - 316.0f, 30, timeValue_, 0xFFFFE080, 12);
        }
    } else {
        // Retail FDF layout renders ResourceBar frames directly.
        // TimeOfDay text centered under the top center clock arch
        if (!BindText("TimeIndicatorText", timeValue_)) {
            const float W = (float)width_;
            const float cx = W * 0.5f;
            const float fSize = 11.0f;
            float tw = textRenderer_.IsValid() ? textRenderer_.MeasureText(timeValue_.c_str(), fSize) : (float)timeValue_.size() * 6.5f;
            const float clockH = (0.078f / 0.6f) * (float)height_;
            DrawText(cx - tw * 0.5f + 1.0f, clockH * 0.74f + 1.0f, timeValue_, 0xFF000000, fSize);
            DrawText(cx - tw * 0.5f, clockH * 0.74f, timeValue_, 0xFF00D7FF, fSize);
        }
    }

    if (AnyModalDialogOpen()) return;

    if (unitName_.empty()) {
        BindText("SimpleNameValue", "");
        return;
    }

    const float nx = minimapRect_.x + minimapRect_.w + 64 + portraitRect_.w + 8;
    if (!BindText("SimpleNameValue", unitName_)) {
        DrawText(nx, height_ - height_ * 0.27f + 6, unitName_, 0xFFFFFFFF, 14);
    }
    // Portrait HP/MP text under the portrait arch (ref Screenshots 4 & 5)
    if (portraitRect_.w > 0 && portraitRect_.h > 0) {
        if (maxHp_ > 0) {
            std::snprintf(buf, sizeof(buf), "%.0f / %.0f", hp_, maxHp_);
            const float fSize = 11.0f;
            float tw = textRenderer_.IsValid() ? textRenderer_.MeasureText(buf, fSize) : (float)std::strlen(buf) * 6.5f;
            float tx = portraitRect_.x + (portraitRect_.w - tw) * 0.5f;
            float ty = (maxMp_ > 0) ? (portraitRect_.y + portraitRect_.h - 22.0f) : (portraitRect_.y + portraitRect_.h - 16.0f);
            DrawText(tx, ty, buf, 0xFF00FF00, fSize);
        }
        if (maxMp_ > 0) {
            std::snprintf(buf, sizeof(buf), "%.0f / %.0f", mp_, maxMp_);
            const float fSize = 11.0f;
            float tw = textRenderer_.IsValid() ? textRenderer_.MeasureText(buf, fSize) : (float)std::strlen(buf) * 6.5f;
            float tx = portraitRect_.x + (portraitRect_.w - tw) * 0.5f;
            float ty = portraitRect_.y + portraitRect_.h - 10.0f;
            DrawText(tx, ty, buf, 0xFF60A0FF, fSize);
        }
    } else {
        if (unitLevel_ > 0)
            DrawText(nx, height_ - height_ * 0.27f + 24, "Level " + std::to_string(unitLevel_),
                     0xFFCCCCCC, 12);
        if (maxHp_ > 0) {
            std::snprintf(buf, sizeof(buf), "%.0f / %.0f", hp_, maxHp_);
            DrawText(nx, height_ - height_ * 0.27f + 42, buf, 0xFF60E060, 12);
        }
        if (maxMp_ > 0) {
            std::snprintf(buf, sizeof(buf), "%.0f / %.0f", mp_, maxMp_);
            DrawText(nx, height_ - height_ * 0.27f + 58, buf, 0xFF6080E0, 12);
        }
    }
    // Hero/attack values live in the retail icon blocks when present
    // (bound in SetSelection); procedural draws are the fallback only.
    const bool hasInfoBlocks = FindFrame("SmashSimpleInfoPanelIconHero") != nullptr ||
                               FindFrame("SimpleInfoPanelIconHero") != nullptr;
    if (heroVisible_ && !hasInfoBlocks) {
        std::snprintf(buf, sizeof(buf), "STR %d  AGI %d  INT %d",
                      heroStrBase_ + heroStrBonus_, heroAgiBase_ + heroAgiBonus_,
                      heroIntBase_ + heroIntBonus_);
        DrawText(nx, height_ - height_ * 0.27f + 76, buf, 0xFFFFCC66, 12);
    }
    if (infoStats_.visible && !hasInfoBlocks) {
        if (!infoStats_.attackText.empty()) DrawText(nx, height_ - height_ * 0.27f + 94, infoStats_.attackText, 0xFFFFFFFF, 12);
        if (!infoStats_.armorText.empty()) DrawText(nx + 130, height_ - height_ * 0.27f + 94, infoStats_.armorText, 0xFFFFFFFF, 12);
    }
}

void FdfHudHost::SetDialogVisible(UIFrame* dialog, bool visible) {
    if (!dialog) return;
    dialog->SetVisible(visible);
    if (auto* s = engine::ui::DynamicCastFrame<SimpleFrame>(dialog)) {
        // Children keep their own FDF visibility (e.g. sub-panels stay
        // hidden); only the dialog root toggles. Ensure positioned bounds
        // are fresh when shown.
        if (visible && root_) root_->PositionBounds(gui_);
        (void)s;
    }
}

void FdfHudHost::LayoutDialogFrame(UIFrame* dialog) {
    if (!dialog || !root_) return;
    UiRect b = dialog->GetBounds();
    if (b.w <= 0 || b.h <= 0) return;
    // Center retail dialogs on screen (ref EscMenu/QuestDialog behavior:
    // modal panels centered over the viewport).
    // FDF frames without SetPoint snap to the parent origin on every
    // PositionBounds (defaultToParent_); explicit centering must opt out or
    // the next Render() snaps the dialog back to 0,0.
    dialog->SetDefaultToParent(false);
    dialog->SetPosition(((float)width_ - b.w) * 0.5f, ((float)height_ - b.h) * 0.5f);
    // Children anchor to the dialog root, so re-resolve after centering.
    root_->PositionBounds(gui_);
}

static std::string WrapTooltipText(const std::string& input, float maxWidth, const TextRenderer* tr, float fontSize) {
    if (input.empty()) return "";
    std::string result;
    std::string currentLine;
    float currentLineWidth = 0.0f;
    size_t i = 0;
    while (i < input.size()) {
        if (input[i] == '\n') {
            result += currentLine;
            result += '\n';
            currentLine.clear();
            currentLineWidth = 0.0f;
            i++;
            continue;
        }
        size_t nextSpace = input.find_first_of(" \t\n", i);
        if (nextSpace == std::string::npos) nextSpace = input.size();
        std::string word = input.substr(i, nextSpace - i);
        float wordW = tr && tr->IsValid() ? tr->MeasureText(word.c_str(), fontSize) : ((float)word.size() * 7.5f);
        float spaceW = tr && tr->IsValid() ? tr->MeasureText(" ", fontSize) : 7.0f;

        if (!currentLine.empty()) {
            if (currentLineWidth + spaceW + wordW > maxWidth) {
                result += currentLine;
                result += '\n';
                currentLine = word;
                currentLineWidth = wordW;
            } else {
                currentLine += " ";
                currentLine += word;
                currentLineWidth += spaceW + wordW;
            }
        } else {
            currentLine = word;
            currentLineWidth = wordW;
        }
        i = nextSpace;
        while (i < input.size() && (input[i] == ' ' || input[i] == '\t')) {
            i++;
        }
    }
    if (!currentLine.empty()) {
        result += currentLine;
    }
    return result;
}

void FdfHudHost::LayoutTooltipFrame() {
    UIFrame* tip = consoleRefs_.tooltipFrame
        ? consoleRefs_.tooltipFrame
        : FindFrame("SmashToolTip");
    if (!tip) return;
    const bool show = tooltipVisible_ && !tooltipValue_.empty();
    tip->SetVisible(show);
    if (!show) return;
    std::string title = tooltipValue_;
    std::string uber;
    if (auto nl = tooltipValue_.find('\n'); nl != std::string::npos) {
        title = tooltipValue_.substr(0, nl);
        uber = tooltipValue_.substr(nl + 1);
    }
    const bool hasCosts = tooltipCosts_.gold > 0 || tooltipCosts_.lumber > 0 ||
        tooltipCosts_.food > 0 || tooltipCosts_.mana > 0;
    const float maxTextW = hasCosts ? 280.0f : 260.0f;
    std::string wrappedUber = WrapTooltipText(uber, maxTextW, textRenderer_.IsValid() ? &textRenderer_ : nullptr, 12.0f);
    if (consoleRefs_.tooltipText) consoleRefs_.tooltipText->SetText(title);
    else BindText("SmashToolTipText", title);
    if (consoleRefs_.uberTipText) consoleRefs_.uberTipText->SetText(wrappedUber);
    else BindText("SmashUberTipText", wrappedUber);

    int lines = 1 + (int)std::count(title.begin(), title.end(), '\n');
    if (!wrappedUber.empty()) {
        lines += 1 + (int)std::count(wrappedUber.begin(), wrappedUber.end(), '\n');
    }
    float titleW = textRenderer_.IsValid() ? textRenderer_.MeasureText(title.c_str(), 13.0f) : ((float)title.size() * 8.0f);
    float w = std::max(hasCosts ? 220.0f : 120.0f, std::min(340.0f, std::max(titleW + 24.0f, maxTextW + 24.0f)));
    float h = 14.0f + (float)lines * 16.0f + (hasCosts ? 24.0f : 8.0f);
    float x = std::min(std::max(4.0f, tooltipX_ + 16.0f), (float)width_ - w - 4.0f);
    float y = std::min(std::max(4.0f, tooltipY_ - h - 12.0f), (float)height_ - h - 4.0f);
    // Same snap-back guard as LayoutDialogFrame: SmashToolTip has no FDF
    // SetPoint, so explicit cursor placement must opt out of
    // defaultToParent_ or PositionBounds snaps it to the console origin.
    tip->SetDefaultToParent(false);
    tip->SetPosition(x, y);
    tip->SetSize(w, h);
    if (root_) root_->PositionBounds(gui_);
}

void FdfHudHost::DrawTooltip() {
    LayoutTooltipFrame();
    // When the retail SmashToolTip frame is visible it renders through the
    // FDF tree (root_->Render) with real backdrop/text/resource styling, so
    // the procedural quad below is the MPQ-less fallback only.
    UIFrame* tip = consoleRefs_.tooltipFrame
        ? consoleRefs_.tooltipFrame
        : FindFrame("SmashToolTip");
    if (tip && tip->IsVisible()) {
        // Cost icons still draw procedurally at the tooltip rect (retail
        // SmashToolTipIconResource rows are per-cost templates inflated on
        // demand in Java; our static FDF has one row template).
        UiRect b = tip->GetBounds();
        if (b.w > 0 && tooltipVisible_) {
            float cx = b.x + 8;
            const float cy = b.y + b.h - 20;
            auto drawCost = [&](const char* skinKey, int value) {
                if (value <= 0) return;
                if (auto icon = ResolveIcon(skinKey)) {
                    if (icon->IsValid())
                        gui_.renderer.DrawTexturedQuad(cx, cy, 14, 14, icon->GetImage());
                }
                DrawText(cx + 16.0f, cy + 1, std::to_string(value), 0xFFFCDE12, 11);
                cx += 64.0f;
            };
            drawCost("ToolTipGoldIcon", tooltipCosts_.gold);
            drawCost("ToolTipLumberIcon", tooltipCosts_.lumber);
            drawCost("ToolTipSupplyIcon", tooltipCosts_.food);
            drawCost("ToolTipManaIcon", tooltipCosts_.mana);
        }
        return;
    }
    if (!tooltipVisible_ || tooltipValue_.empty()) return;
    std::string title = tooltipValue_;
    std::string uber;
    if (auto nl = tooltipValue_.find('\n'); nl != std::string::npos) {
        title = tooltipValue_.substr(0, nl);
        uber = tooltipValue_.substr(nl + 1);
    }
    std::string wrappedUber = WrapTooltipText(uber, 280.0f, textRenderer_.IsValid() ? &textRenderer_ : nullptr, 12.0f);
    std::string fullText = title;
    if (!wrappedUber.empty()) fullText += "\n" + wrappedUber;
    int lines = 1 + (int)std::count(fullText.begin(), fullText.end(), '\n');
    const bool hasCosts = tooltipCosts_.gold > 0 || tooltipCosts_.lumber > 0 ||
        tooltipCosts_.food > 0 || tooltipCosts_.mana > 0;
    float w = 296.0f;
    float h = 16.0f + (float)lines * 16.0f + (hasCosts ? 24.0f : 8.0f);
    float x = std::min(std::max(4.0f, tooltipX_ + 16.0f), (float)width_ - w - 4.0f);
    float y = std::min(std::max(4.0f, tooltipY_ - h - 12.0f), (float)height_ - h - 4.0f);
    UiRect b = {x, y, w, h};
    gui_.renderer.DrawQuad(b.x, b.y, b.w, b.h, 0xEE101018);
    gui_.renderer.DrawQuad(b.x, b.y, b.w, 1, 0xFF6A5A2A);
    DrawText(b.x + 8, b.y + 8, fullText, 0xFFFFE080, 13);
    // Ability cost row (ref SmashToolTipIconResource frames): icon + value
    // per nonzero cost, laid out left to right under the text.
    float cx = b.x + 8;
    const float cy = b.y + b.h - 24;
    auto drawCost = [&](const char* skinKey, int value) {
        if (value <= 0) return;
        if (auto icon = ResolveIcon(skinKey)) {
            if (icon->IsValid()) gui_.renderer.DrawTexturedQuad(cx, cy, 14, 14, icon->GetImage());
        }
        DrawText(cx + 16.0f, cy + 1, std::to_string(value), 0xFFFCDE12, 11);
        cx += 64.0f;
    };
    drawCost("ToolTipGoldIcon", tooltipCosts_.gold);
    drawCost("ToolTipLumberIcon", tooltipCosts_.lumber);
    drawCost("ToolTipSupplyIcon", tooltipCosts_.food);
    drawCost("ToolTipManaIcon", tooltipCosts_.mana);
}

void FdfHudHost::DrawHoverTip() {
    // World-frame hover tip (ref MeleeUI getWorldFrameHoverTipText):
    // reserved for unit hover; currently no hover unit is tracked, so the
    // retail SmashHoverTip frame stays hidden. Kept for render parity.
    UIFrame* hover = consoleRefs_.hovertipFrame
        ? consoleRefs_.hovertipFrame
        : FindFrame("SmashHoverTip");
    if (hover) hover->SetVisible(false);
}

void FdfHudHost::DrawDialogMessages() {
    // Error message (ref MeleeUI errorMessageFrame with expire+fadeout):
    // binds into the retail SmashErrorMessageFrame when present, with a
    // procedural centered fallback so MPQ-less runs still show it.
    const bool hasErrorFrame = consoleRefs_.errorMessageFrame != nullptr ||
        FindFrame("SmashErrorMessageFrame") != nullptr;
    if (!errorMessage_.text.empty() && uiClock_ < errorMessage_.expireAt) {
        float alpha = 1.0f;
        if (uiClock_ > errorMessage_.fadeAt) {
            const double span = errorMessage_.expireAt - errorMessage_.fadeAt;
            alpha = span > 0 ? (float)((errorMessage_.expireAt - uiClock_) / span) : 0.0f;
        }
        const uint32_t col =
            ((uint32_t)(std::clamp(alpha, 0.0f, 1.0f) * 255.0f) << 24) | 0x00FF4444;
        if (consoleRefs_.errorMessageFrame) {
            consoleRefs_.errorMessageFrame->SetText(errorMessage_.text);
            consoleRefs_.errorMessageFrame->SetVisible(true);
        } else {
            BindText("SmashErrorMessageFrame", errorMessage_.text);
            if (UIFrame* f = FindFrame("SmashErrorMessageFrame")) f->SetVisible(true);
        }
        if (!hasErrorFrame) {
            DrawText((float)width_ * 0.5f - 180.0f, (float)height_ * 0.72f,
                     errorMessage_.text, col, 14);
        }
    } else {
        if (consoleRefs_.errorMessageFrame) consoleRefs_.errorMessageFrame->SetVisible(false);
        else if (UIFrame* f = FindFrame("SmashErrorMessageFrame")) f->SetVisible(false);
    }
    // Game messages (ref MeleeUI gameMessages deque, centered stack with
    // expire+fade). Procedural draw; retail has no static template for the
    // per-message StringFrames Java creates on demand.
    float gy = (float)height_ * 0.32f;
    for (const auto& m : gameMessages_) {
        if (uiClock_ >= m.expireAt) continue;
        float alpha = 1.0f;
        if (uiClock_ > m.fadeAt) {
            const double span = m.expireAt - m.fadeAt;
            alpha = span > 0 ? (float)((m.expireAt - uiClock_) / span) : 0.0f;
        }
        const uint32_t col =
            ((uint32_t)(std::clamp(alpha, 0.0f, 1.0f) * 255.0f) << 24) | 0x00FFE080;
        DrawText((float)width_ * 0.5f - (float)m.text.size() * 3.5f, gy, m.text, col, 14);
        gy += 20.0f;
    }
}

void FdfHudHost::DrawChat() {
    if (!chatOpen_ && chatMessages_.empty()) return;
    const float stripY = height_ - height_ * 0.27f;
    float y = stripY - 20;
    const int shown = std::min<int>((int)chatMessages_.size(), 6);
    for (int i = (int)chatMessages_.size() - shown; i < (int)chatMessages_.size(); ++i) {
        DrawText(12, y - 18, chatMessages_[i], 0xFFFFFFFF, 12);
        y -= 18;
    }
    if (chatOpen_) {
        gui_.renderer.DrawQuad(8, stripY - 24, 420, 20, 0xCC000000);
        DrawText(12, stripY - 22, "> " + chatInputText_, 0xFFFFFF00, 12);
    }
}

void FdfHudHost::RenderOffscreen() { Render(); }

void FdfHudHost::Render() {
    if (!initialized_) return;
    gui_.renderer.BeginFrame(width_, height_);
    // Cinematic filter tint under the HUD chrome (ref CCineFilter overlay).
    if (cineFilterVisible_ && !glueActive_ && (cineFilterColor_ >> 24) != 0) {
        gui_.renderer.DrawQuad(0.0f, 0.0f, (float)width_, (float)height_, cineFilterColor_);
    }
    if (!glueActive_) {
        // Solid black backing for console HUD cutouts (minimap, portrait arch, info panel, inventory, command grid)
        // so the 3D scene doesn't show through retail console texture cutouts, while respecting the
        // battlements crenellations on the center console wall.
        const float W = (float)width_, H = (float)height_;
        const float leftTowerW = gui_.ConvertX(0.208f);
        const float rightTowerX = gui_.ConvertX(0.612f);
        const float sideH = gui_.ConvertY(0.176f);
        const float centerH = gui_.ConvertY(0.125f);

        // Draw an opaque backing quad minus a rectangular hole, so an
        // externally rendered surface (the live minimap) drawn earlier isn't
        // covered.
        auto drawBackingMinusHole = [&](float x, float y, float w, float h,
                                        const ScreenRect& hole) {
            if (hole.w <= 0.0f || hole.h <= 0.0f) {
                gui_.renderer.DrawQuad(x, y, w, h, 0xFF000000);
                return;
            }
            const float hx0 = std::max(x, hole.x);
            const float hy0 = std::max(y, hole.y);
            const float hx1 = std::min(x + w, hole.x + hole.w);
            const float hy1 = std::min(y + h, hole.y + hole.h);
            if (hx1 <= hx0 || hy1 <= hy0) {
                gui_.renderer.DrawQuad(x, y, w, h, 0xFF000000);
                return;
            }
            if (hy0 > y) gui_.renderer.DrawQuad(x, y, w, hy0 - y, 0xFF000000);
            if (hy1 < y + h) gui_.renderer.DrawQuad(x, hy1, w, (y + h) - hy1, 0xFF000000);
            if (hx0 > x) gui_.renderer.DrawQuad(x, hy0, hx0 - x, hy1 - hy0, 0xFF000000);
            if (hx1 < x + w) gui_.renderer.DrawQuad(hx1, hy0, (x + w) - hx1, hy1 - hy0, 0xFF000000);
        };

        // Left tower / minimap backing
        drawBackingMinusHole(0.0f, H - sideH, leftTowerW, sideH,
                             externalMinimapVisible_ ? minimapRect_ : ScreenRect{});
        // Center wall cutouts backing (portrait arch, info panel, inventory below battlements/beam)
        gui_.renderer.DrawQuad(leftTowerW, H - centerH, rightTowerX - leftTowerW, centerH, 0xFF000000);
        // Right tower / command buttons backing
        gui_.renderer.DrawQuad(rightTowerX, H - sideH, W - rightTowerX, sideH, 0xFF000000);
    }
    // Cinematic letterbox bars (ref SetCinematicMode): solid bands over the
    // scene, under dialogs/modals drawn later.
    if (cinematicMode_ && !glueActive_) {
        const float W = (float)width_, H = (float)height_;
        const float barH = H * 0.08f;
        gui_.renderer.DrawQuad(0.0f, 0.0f, W, barH, 0xFF000000);
        gui_.renderer.DrawQuad(0.0f, H - barH, W, barH, 0xFF000000);
    }
    std::vector<UIFrame*> modalsToDefer;
    if (!glueActive_) {
        for (UIFrame* m : {
                 consoleRefs_.questDialog ? consoleRefs_.questDialog : FindFrame("QuestDialog"),
                 consoleRefs_.chatDialog ? consoleRefs_.chatDialog : FindFrame("ChatDialog"),
                 consoleRefs_.allianceDialog ? consoleRefs_.allianceDialog : FindFrame("AllianceDialog")
             }) {
            if (m && m->IsVisible()) {
                modalsToDefer.push_back(m);
            }
        }
    }

    if (root_ && root_->IsVisible() && !glueActive_) {
        if (gameMenuOpen_) {
            if (consoleRefs_.escMenuBackdrop) consoleRefs_.escMenuBackdrop->SetVisible(false);
            if (consoleRefs_.escMenuMainPanel) consoleRefs_.escMenuMainPanel->SetVisible(false);
            if (UIFrame* sm = FindFrame("SmashEscMenu")) sm->SetVisible(false);
        }
        root_->PositionBounds(gui_);

        for (UIFrame* m : modalsToDefer) m->SetVisible(false);
        root_->Render(gui_.renderer);
        for (UIFrame* m : modalsToDefer) m->SetVisible(true);
    }
    if (!glueActive_) {
        // Console chrome/texts render under modal Esc/Quest dialogs (ref MeleeUI render order).
        DrawConsoleChrome();
        DrawTexts();
        DrawTooltip();
        DrawHoverTip();
        DrawChat();
        DrawDialogMessages();

        if (!modalsToDefer.empty()) {
            gui_.renderer.DrawQuad(0.0f, 0.0f, (float)width_, (float)height_, 0x99000000);
            for (UIFrame* m : modalsToDefer) {
                m->Render(gui_.renderer);
            }
        }
    }
    if (gameMenuOpen_) DrawGameMenu();
    if (glueActive_) DrawGlueMenu();
    gui_.renderer.EndFrame();
    render::RhiDevice::Instance().ResetStateCache();
}

bool FdfHudHost::HandleEvent(const sapp_event* /*e*/) { return false; }

ScreenRect FdfHudHost::GetCommandRect(int index) const {
    if (index < 0 || index >= 12) return ScreenRect{};
    return commandRects_[index];
}

int FdfHudHost::HitCommandButton(float x, float y) const {
    for (int i = 0; i < 12; ++i) {
        if (commandStates_[i].visible && commandRects_[i].Contains(x, y)) return i;
    }
    return -1;
}

int FdfHudHost::HitUpperButton(float x, float y) const {
    for (int i = 0; i < 4; ++i) {
        if (upperButtonRects_[i].Contains(x, y)) return i;
    }
    return -1;
}

int FdfHudHost::HitMinimapButton(float x, float y) const {
    for (int i = 0; i < 5; ++i) {
        if (minimapButtonRects_[i].Contains(x, y)) return i;
    }
    return -1;
}

int FdfHudHost::HitMultiSelection(float x, float y) const {
    for (int i = 0; i < 12; ++i) {
        if (multiSelectVisible_[i] && multiSelectRects_[i].Contains(x, y)) return i;
    }
    return -1;
}

int FdfHudHost::HitQueueSlot(float x, float y) const {
    for (int i = 0; i < 5; ++i) {
        if (queueStates_[i].visible && queueRects_[i].Contains(x, y)) return i;
    }
    return -1;
}

int FdfHudHost::HitInventorySlot(float x, float y) const {
    if (!inventoryAvailable_) return -1;
    for (int i = 0; i < 6 && i < inventoryCapacity_; ++i) {
        if (inventoryStates_[i].occupied && inventoryRects_[i].Contains(x, y)) return i;
    }
    return -1;
}

int FdfHudHost::HitBuffSlot(float x, float y) const {
    for (int i = 0; i < buffCount_ && i < (int)buffStates_.size(); ++i) {
        if (i < (int)consoleRefs_.buffButtons.size() && consoleRefs_.buffButtons[i] && consoleRefs_.buffButtons[i]->IsVisible()) {
            UiRect b = consoleRefs_.buffButtons[i]->GetBounds();
            if (b.Contains(x, y)) return i;
        }
    }
    return -1;
}

// ---- game menu ----

void FdfHudHost::OpenGameMenu() {
    CloseQuestDialog();
    CloseChatDialog();
    CloseAllianceDialog();
    gameMenuOpen_ = true;
    gameMenuPanel_ = GameMenuPanel::Main;
    hoveredGameMenuAction_ = pressedGameMenuAction_ = -1;
    if (consoleRefs_.escMenuBackdrop) consoleRefs_.escMenuBackdrop->SetVisible(false);
    else if (UIFrame* bd = FindFrame("EscMenuBackdrop")) bd->SetVisible(false);
    if (consoleRefs_.escMenuMainPanel) consoleRefs_.escMenuMainPanel->SetVisible(false);
    else if (UIFrame* mp = FindFrame("EscMenuMainPanel")) mp->SetVisible(false);
    if (UIFrame* sm = FindFrame("SmashEscMenu")) sm->SetVisible(false);
}

void FdfHudHost::OpenGameMenuDebugPanel(const std::string& panel) {
    const std::string key = ToLowerNoSpace(panel);
    GameMenuPanel p = GameMenuPanel::Main;
    if (key == "endgame" || key == "end") p = GameMenuPanel::EndGame;
    else if (key == "confirmquit" || key == "quitconfirm" || key == "confirm") p = GameMenuPanel::ConfirmQuit;
    else if (key == "savegame" || key == "save") p = GameMenuPanel::SaveGame;
    else if (key == "loadgame" || key == "load") p = GameMenuPanel::LoadGame;
    else if (key == "help") p = GameMenuPanel::Help;
    else if (key == "tips") p = GameMenuPanel::Tips;
    else if (key == "options") p = GameMenuPanel::Options;
    else if (key == "gameplay") p = GameMenuPanel::OptionsGameplay;
    else if (key == "video") p = GameMenuPanel::OptionsVideo;
    else if (key == "sound") p = GameMenuPanel::OptionsSound;
    else if (key == "network") p = GameMenuPanel::OptionsNetwork;
    gameMenuOpen_ = true;
    gameMenuPanel_ = p;
    hoveredGameMenuAction_ = pressedGameMenuAction_ = -1;
    if (consoleRefs_.escMenuBackdrop) consoleRefs_.escMenuBackdrop->SetVisible(false);
    else if (UIFrame* bd = FindFrame("EscMenuBackdrop")) bd->SetVisible(false);
    if (consoleRefs_.escMenuMainPanel) consoleRefs_.escMenuMainPanel->SetVisible(false);
    else if (UIFrame* mp = FindFrame("EscMenuMainPanel")) mp->SetVisible(false);
    if (UIFrame* sm = FindFrame("SmashEscMenu")) sm->SetVisible(false);
}

void FdfHudHost::CloseGameMenu() {
    gameMenuOpen_ = false;
    hoveredGameMenuAction_ = pressedGameMenuAction_ = -1;
    if (consoleRefs_.escMenuBackdrop) SetDialogVisible(consoleRefs_.escMenuBackdrop, false);
    else if (UIFrame* bd = FindFrame("EscMenuBackdrop")) SetDialogVisible(bd, false);
    if (consoleRefs_.escMenuMainPanel) SetDialogVisible(consoleRefs_.escMenuMainPanel, false);
    else if (UIFrame* mp = FindFrame("EscMenuMainPanel")) SetDialogVisible(mp, false);
}

// ---- upper-button dialogs: Quests / Menu / Allies / Chat ----

void FdfHudHost::HandleUpperButtonAction(int index) {
    // Ref UpperButtonBar.fdf order: Quests, Menu, Allies, Chat.
    switch (index) {
    case 0: ToggleQuestDialog(); break;
    case 1: OpenGameMenu(); break;
    case 2:
        if (allianceDialogOpen_) CloseAllianceDialog();
        else OpenAllianceDialog();
        break;
    case 3:
        if (chatDialogOpen_) CloseChatDialog();
        else OpenChatDialog();
        break;
    default: break;
    }
}

void FdfHudHost::OpenQuestDialog() {
    CloseGameMenu();
    CloseChatDialog();
    CloseAllianceDialog();
    questOpen_ = true;
    UIFrame* d = consoleRefs_.questDialog ? consoleRefs_.questDialog : FindFrame("QuestDialog");
    SetDialogVisible(d, true);
    LayoutDialogFrame(d);
}

void FdfHudHost::CloseQuestDialog() {
    questOpen_ = false;
    UIFrame* d = consoleRefs_.questDialog ? consoleRefs_.questDialog : FindFrame("QuestDialog");
    SetDialogVisible(d, false);
}

void FdfHudHost::ToggleQuestDialog() {
    if (questOpen_) CloseQuestDialog();
    else OpenQuestDialog();
}

void FdfHudHost::OpenChatDialog() {
    CloseGameMenu();
    CloseQuestDialog();
    CloseAllianceDialog();
    chatDialogOpen_ = true;
    UIFrame* d = consoleRefs_.chatDialog ? consoleRefs_.chatDialog : FindFrame("ChatDialog");
    SetDialogVisible(d, true);
    LayoutDialogFrame(d);
}

void FdfHudHost::CloseChatDialog() {
    chatDialogOpen_ = false;
    UIFrame* d = consoleRefs_.chatDialog ? consoleRefs_.chatDialog : FindFrame("ChatDialog");
    SetDialogVisible(d, false);
}

void FdfHudHost::OpenAllianceDialog() {
    CloseGameMenu();
    CloseQuestDialog();
    CloseChatDialog();
    allianceDialogOpen_ = true;
    UIFrame* d =
        consoleRefs_.allianceDialog ? consoleRefs_.allianceDialog : FindFrame("AllianceDialog");
    SetDialogVisible(d, true);
    LayoutDialogFrame(d);
}

void FdfHudHost::CloseAllianceDialog() {
    allianceDialogOpen_ = false;
    UIFrame* d =
        consoleRefs_.allianceDialog ? consoleRefs_.allianceDialog : FindFrame("AllianceDialog");
    SetDialogVisible(d, false);
}

bool FdfHudHost::AnyModalDialogOpen() const {
    return gameMenuOpen_ || questOpen_ || chatDialogOpen_ || allianceDialogOpen_;
}

bool FdfHudHost::HandleModalClick(float x, float y) {
    // Close buttons (retail Accept/Cancel) dismiss the dialog; any other
    // click inside the modal panel is consumed so world/command input behind
    // it never fires (ref modal Esc/Quest behavior).
    auto inside = [](UIFrame* f, float px, float py) {
        if (!f || !f->IsVisible()) return false;
        UiRect b = f->GetBounds();
        return b.w > 0 && b.h > 0 && px >= b.x && px <= b.x + b.w && py >= b.y &&
            py <= b.y + b.h;
    };
    auto buttonInside = [this, inside](const char* name, float px, float py) {
        UIFrame* b = FindFrame(name);
        return inside(b, px, py);
    };
    if (questOpen_) {
        UIFrame* d = consoleRefs_.questDialog ? consoleRefs_.questDialog : FindFrame("QuestDialog");
        if (buttonInside("QuestAcceptButton", x, y)) {
            CloseQuestDialog();
            return true;
        }
        if (inside(d, x, y) && d) {
            d->OnMouseDown(x, y, 0);
        }
        return true;
    }
    if (chatDialogOpen_) {
        UIFrame* d = consoleRefs_.chatDialog ? consoleRefs_.chatDialog : FindFrame("ChatDialog");
        if (buttonInside("ChatAcceptButton", x, y)) {
            CloseChatDialog();
            return true;
        }
        if (inside(d, x, y) && d) {
            d->OnMouseDown(x, y, 0);
        }
        return true;
    }
    if (allianceDialogOpen_) {
        UIFrame* d =
            consoleRefs_.allianceDialog ? consoleRefs_.allianceDialog : FindFrame("AllianceDialog");
        if (buttonInside("AllianceAcceptButton", x, y) ||
            buttonInside("AllianceCancelButton", x, y)) {
            CloseAllianceDialog();
            return true;
        }
        if (inside(d, x, y) && d) {
            d->OnMouseDown(x, y, 0);
        }
        return true;
    }
    return false;
}

void FdfHudHost::CloseAllDialogs() {
    CloseGameMenu();
    CloseQuestDialog();
    CloseChatDialog();
    CloseAllianceDialog();
}

void FdfHudHost::ShowCinematic(const std::string& speaker, const std::string& dialogue) {
    cinematicVisible_ = true;
    UIFrame* p = consoleRefs_.cinematicPanel ? consoleRefs_.cinematicPanel : FindFrame("CinematicPanel");
    SetDialogVisible(p, true);
    if (consoleRefs_.cinematicSpeakerText) consoleRefs_.cinematicSpeakerText->SetText(speaker);
    else BindText("CinematicSpeakerText", speaker);
    if (consoleRefs_.cinematicDialogueText) consoleRefs_.cinematicDialogueText->SetText(dialogue);
    else BindText("CinematicDialogueText", dialogue);
}

void FdfHudHost::HideCinematic() {
    cinematicVisible_ = false;
    UIFrame* p = consoleRefs_.cinematicPanel ? consoleRefs_.cinematicPanel : FindFrame("CinematicPanel");
    SetDialogVisible(p, false);
}

void FdfHudHost::ShowTimerDialog(const std::string& title, const std::string& value) {
    timerVisible_ = true;
    UIFrame* d = consoleRefs_.timerDialog ? consoleRefs_.timerDialog : FindFrame("TimerDialog");
    SetDialogVisible(d, true);
    if (consoleRefs_.timerTitleText) consoleRefs_.timerTitleText->SetText(title);
    else BindText("TimerDialogTitle", title);
    if (consoleRefs_.timerValueText) consoleRefs_.timerValueText->SetText(value);
    else BindText("TimerDialogValue", value);
}

void FdfHudHost::HideTimerDialog() {
    timerVisible_ = false;
    UIFrame* d = consoleRefs_.timerDialog ? consoleRefs_.timerDialog : FindFrame("TimerDialog");
    SetDialogVisible(d, false);
}

void FdfHudHost::ShowScriptDialog(const std::string& text) {
    scriptVisible_ = true;
    UIFrame* d = consoleRefs_.scriptDialog ? consoleRefs_.scriptDialog : FindFrame("ScriptDialog");
    SetDialogVisible(d, true);
    if (consoleRefs_.scriptText) consoleRefs_.scriptText->SetText(text);
    else BindText("ScriptDialogText", text);
}

void FdfHudHost::HideScriptDialog() {
    scriptVisible_ = false;
    UIFrame* d = consoleRefs_.scriptDialog ? consoleRefs_.scriptDialog : FindFrame("ScriptDialog");
    SetDialogVisible(d, false);
}

void FdfHudHost::ShowErrorMessage(const std::string& text) {
    // Ref MeleeUI: 2.0s expire, 1.5s fade start (WORLD_FRAME constants).
    errorMessage_.text = text;
    errorMessage_.expireAt = uiClock_ + 2.0;
    errorMessage_.fadeAt = uiClock_ + 1.5;
}

void FdfHudHost::ShowGameMessage(const std::string& text, float expireSeconds) {
    FadingMessage m;
    m.text = text;
    m.expireAt = uiClock_ + (expireSeconds > 0 ? expireSeconds : 5.0f);
    m.fadeAt = uiClock_ + (expireSeconds > 0 ? expireSeconds * 0.9 : 4.5);
    gameMessages_.push_back(m);
    while (gameMessages_.size() > 5) gameMessages_.erase(gameMessages_.begin());
}

void FdfHudHost::ToggleGameMenu() {
    if (gameMenuOpen_) CloseGameMenu();
    else OpenGameMenu();
}

bool FdfHudHost::IsGameMenuActionEnabled(int action) const {
    if (gameMenuPanel_ == GameMenuPanel::Main) {
        const std::string name = GameMenuActionName(action);
        return name == "GameMenuButtonEndGame" || name == "GameMenuButtonReturnToGame";
    }
    return true;
}

std::vector<std::pair<int, std::string>> FdfHudHost::CurrentGameMenuItems() const {
    auto item = [](int a) { return std::make_pair(a, GameMenuActionName(a)); };
    switch (gameMenuPanel_) {
    case GameMenuPanel::Main:
        return {item(0), item(1), item(2), item(3), item(4), item(5), item(6), item(7)};
    case GameMenuPanel::EndGame:
        return {item(8), item(9), item(10), item(11)};
    case GameMenuPanel::ConfirmQuit:
        return {item(12), item(13)};
    case GameMenuPanel::SaveGame:
        return {item(21), item(22), item(23)};
    case GameMenuPanel::LoadGame:
        return {item(24), item(25)};
    case GameMenuPanel::Help:
        return {item(30)};
    case GameMenuPanel::Tips:
        return {item(31), item(32), item(33)};
    case GameMenuPanel::Options:
        return {item(14), item(15), item(16), item(17), item(18), item(11)};
    case GameMenuPanel::OptionsGameplay:
        return {item(34), item(35), item(36), item(37), item(38), item(39), item(40), item(41), item(19), item(20)};
    case GameMenuPanel::OptionsVideo:
        return {item(42), item(43), item(44), item(45), item(46), item(19), item(20)};
    case GameMenuPanel::OptionsSound:
        return {item(47), item(48), item(49), item(50), item(51), item(52), item(53), item(54), item(19), item(20)};
    case GameMenuPanel::OptionsNetwork:
        return {item(55), item(56), item(57), item(58), item(59), item(60), item(61), item(19), item(20)};
    }
    return {};
}

void FdfHudHost::SetGameMenuPanel(GameMenuPanel panel) {
    gameMenuPanel_ = panel;
    hoveredGameMenuAction_ = pressedGameMenuAction_ = -1;
}

void FdfHudHost::HandleGameMenuAction(int action) {
    if (action < 0 || action >= kGameMenuActionCount) return;
    const std::string name = GameMenuActionName(action);
    std::cout << "FdfHudHost: game menu action '" << name << "'" << std::endl;
    if (name == "GameMenuButtonReturnToGame" || name == "GameMenuButtonPrevious" ||
        name == "GameMenuButtonCancelConfirm" || name == "GameMenuButtonSaveGameCancel" ||
        name == "GameMenuButtonLoadGameCancel" || name == "GameMenuButtonHelpOK" ||
        name == "GameMenuButtonTipsOK" || name == "GameMenuButtonOptionsOK" ||
        name == "GameMenuButtonOptionsCancel" || name == "GameMenuButtonDeleteCancel" ||
        name == "GameMenuButtonOverwriteCancel") {
        if (gameMenuPanel_ == GameMenuPanel::Main || name == "GameMenuButtonReturnToGame") CloseGameMenu();
        else if (gameMenuPanel_ == GameMenuPanel::OptionsGameplay ||
                 gameMenuPanel_ == GameMenuPanel::OptionsVideo ||
                 gameMenuPanel_ == GameMenuPanel::OptionsSound ||
                 gameMenuPanel_ == GameMenuPanel::OptionsNetwork)
            SetGameMenuPanel(GameMenuPanel::Options);
        else SetGameMenuPanel(GameMenuPanel::Main);
        return;
    }
    if (name == "GameMenuButtonQuitMission" || name == "GameMenuButtonExitProgram" ||
        name == "GameMenuButtonConfirmExit") {
        std::cout << "FdfHudHost: exit requested (application exit suppressed in host)" << std::endl;
        CloseGameMenu();
        return;
    }
    if (name == "GameMenuButtonPause") { CloseGameMenu(); return; }
    if (name == "GameMenuButtonEndGame") { SetGameMenuPanel(GameMenuPanel::EndGame); return; }
    if (name == "GameMenuButtonOptions") { SetGameMenuPanel(GameMenuPanel::Options); return; }
    if (name == "GameMenuButtonHelp") { SetGameMenuPanel(GameMenuPanel::Help); return; }
    if (name == "GameMenuButtonTips") { SetGameMenuPanel(GameMenuPanel::Tips); return; }
    if (name == "GameMenuButtonSaveGame") { SetGameMenuPanel(GameMenuPanel::SaveGame); return; }
    if (name == "GameMenuButtonLoadGame") { SetGameMenuPanel(GameMenuPanel::LoadGame); return; }
    if (name == "GameMenuButtonGameplay") { SetGameMenuPanel(GameMenuPanel::OptionsGameplay); return; }
    if (name == "GameMenuButtonVideo") { SetGameMenuPanel(GameMenuPanel::OptionsVideo); return; }
    if (name == "GameMenuButtonSound") { SetGameMenuPanel(GameMenuPanel::OptionsSound); return; }
    if (name == "GameMenuButtonNetwork") { SetGameMenuPanel(GameMenuPanel::OptionsNetwork); return; }
    if (name == "GameMenuButtonRestart") { CloseGameMenu(); return; }
    // Option toggles cycle checked state.
    if (name.rfind("GameMenuOption", 0) == 0 && name.find("Slider") == std::string::npos) {
        gameMenuOptionChecked_[action] = !gameMenuOptionChecked_[action];
        gameMenuOptionCycle_[action]++;
    }
}

int FdfHudHost::HitGameMenuButton(float x, float y) const {
    if (!gameMenuOpen_) return -1;
    for (size_t i = 0; i < gameMenuRects_.size(); ++i) {
        const UiRect& rc = gameMenuRects_[i];
        if (rc.Contains(x, y)) {
            auto items = CurrentGameMenuItems();
            if (i < items.size()) {
                int action = items[i].first;
                if (!IsGameMenuActionEnabled(action)) return -1;
                return action;
            }
        }
    }
    return -1;
}

void FdfHudHost::SetHoveredGameMenuButton(int action) { hoveredGameMenuAction_ = action; }
void FdfHudHost::SetPressedGameMenuButton(int action) { pressedGameMenuAction_ = action; }

bool FdfHudHost::InvokeGameMenuActionForDebug(const std::string& actionName) {
    int action = GameMenuActionForName(actionName);
    if (action < 0 && actionName.rfind("GameMenu", 0) == 0 && actionName.rfind("GameMenuButton", 0) != 0)
        action = GameMenuActionForName("GameMenuButton" + actionName.substr(8));
    if (action < 0 || !IsGameMenuActionEnabled(action)) return false;
    if (!gameMenuOpen_) OpenGameMenu();
    SetPressedGameMenuButton(action);
    HandleGameMenuAction(action);
    SetPressedGameMenuButton(-1);
    return true;
}

bool FdfHudHost::InvokeUpperButtonForDebug(const std::string& buttonName) {
    std::string key = ToLowerNoSpace(buttonName);
    const std::string prefix = "upperbuttonbar";
    if (key.rfind(prefix, 0) == 0) key.erase(0, prefix.size());
    if (key.size() >= 6 && key.compare(key.size() - 6, 6, "button") == 0) key.erase(key.size() - 6);
    if (key == "quest") key = "quests";
    static const char* names[4] = {"quests", "menu", "allies", "chat"};
    for (int i = 0; i < 4; ++i) {
        if (key == names[i]) {
            SetPressedUpperButton(i);
            HandleUpperButtonAction(i);
            SetPressedUpperButton(-1);
            return true;
        }
    }
    return false;
}

namespace {
std::string FormatGameMenuLabel(const std::string& raw) {
    static const std::unordered_map<std::string, std::string> kMap = {
        {"GameMenuButtonPause", "Pause Game"},
        {"GameMenuButtonSaveGame", "Save Game"},
        {"GameMenuButtonLoadGame", "Load Game"},
        {"GameMenuButtonOptions", "Options"},
        {"GameMenuButtonHelp", "Help"},
        {"GameMenuButtonTips", "Tips"},
        {"GameMenuButtonEndGame", "End Game"},
        {"GameMenuButtonReturnToGame", "Return to Game"},
        {"GameMenuButtonRestart", "Restart"},
        {"GameMenuButtonQuitMission", "Quit Mission"},
        {"GameMenuButtonExitProgram", "Exit Program"},
        {"GameMenuButtonPrevious", "Previous"},
        {"GameMenuButtonConfirmExit", "Confirm Exit"},
        {"GameMenuButtonCancelConfirm", "Cancel"},
        {"GameMenuButtonGameplay", "Gameplay"},
        {"GameMenuButtonVideo", "Video"},
        {"GameMenuButtonSound", "Sound"},
        {"GameMenuButtonNetwork", "Network"},
        {"GameMenuButtonOptionsBack", "Back"},
        {"GameMenuButtonOptionsOK", "OK"},
        {"GameMenuButtonOptionsCancel", "Cancel"},
    };
    auto it = kMap.find(raw);
    if (it != kMap.end()) return it->second;
    std::string s = raw;
    if (s.rfind("GameMenuButton", 0) == 0) s = s.substr(14);
    else if (s.rfind("GameMenuOption", 0) == 0) s = s.substr(14);
    else if (s.size() > 6 && s.compare(s.size() - 6, 6, "Button") == 0) s = s.substr(0, s.size() - 6);
    return s;
}
} // namespace

void FdfHudHost::DrawButtonList(const std::vector<std::pair<int, std::string>>& items,
                                float x, float y, float w, float rowH,
                                int hovered, int pressed,
                                std::vector<UiRect>& rects) {
    UiRenderer& r = gui_.renderer;
    const bool textured = menuButtonBackdrop_ != nullptr;
    for (size_t i = 0; i < items.size(); ++i) {
        float iy = y + (float)i * (rowH + 4);
        int action = items[i].first;
        bool enabled = IsGameMenuActionEnabled(action);
        if (textured) {
            BackdropFrame* bd = (action == pressed && menuButtonPushedBackdrop_)
                                    ? menuButtonPushedBackdrop_
                                    : menuButtonBackdrop_;
            bd->SetPosition(x, iy);
            bd->SetSize(w, rowH);
            bd->Render(r);
            if (enabled && action == hovered && action != pressed) {
                r.DrawQuad(x, iy, w, rowH, 0x33FFFFFF);
            }
        } else {
            uint32_t bg = 0xFF2A2A38;
            if (!enabled) bg = 0xFF1F1F2A;
            else if (action == pressed) bg = 0xFF1A1A26;
            else if (action == hovered) bg = 0xFF3E3E55;
            r.DrawQuad(x, iy, w, rowH, bg);
            r.DrawQuad(x, iy, w, 1, 0xFF6A5A2A);
        }
        std::string label = FormatGameMenuLabel(items[i].second);
        uint32_t textColor = enabled ? ((action == hovered) ? 0xFFFFFFFF : 0xFFFFCC00) : 0xFF808080;
        float tw = textRenderer_.IsValid() ? textRenderer_.MeasureText(label.c_str(), 14) : (float)label.size() * 7.5f;
        DrawText(x + (w - tw) * 0.5f, iy + (rowH - 14) * 0.5f, label, textColor, 14);
        if (i < rects.size()) rects[i] = UiRect{x, iy, w, rowH};
    }
}

void FdfHudHost::DrawGameMenu() {
    UiRenderer& r = gui_.renderer;
    r.DrawQuad(0, 0, (float)width_, (float)height_, 0x99000000);
    const float pw = 340, ph = 370;
    const float px = (width_ - pw) * 0.5f, py = (height_ - ph) * 0.5f;
    if (menuPanelBackdrop_) {
        // Retail EscMenu backdrop + border instead of flat quads.
        menuPanelBackdrop_->SetPosition(px - 2, py - 2);
        menuPanelBackdrop_->SetSize(pw + 4, ph + 4);
        menuPanelBackdrop_->Render(r);
    } else {
        r.DrawQuad(px - 2, py - 2, pw + 4, ph + 4, 0xFF6A5A2A);
        r.DrawQuad(px, py, pw, ph, 0xFF14141C);
    }
    float titleW = textRenderer_.IsValid() ? textRenderer_.MeasureText("Game Menu", 16) : 9.0f * 8.5f;
    DrawText(px + (pw - titleW) * 0.5f, py + 36, "Game Menu", 0xFFFFCC66, 16);

    auto items = CurrentGameMenuItems();
    gameMenuRects_.assign(items.size(), UiRect{});

    if (gameMenuPanel_ == GameMenuPanel::Main) {
        // Faithful WC3 EscMenuMainPanel layout (ref Screenshot 3):
        // Pause Game, Save Game, Load Game, Options (full-width, disabled)
        // Help and Tips (half-width side-by-side, disabled)
        // End Game (full-width, enabled)
        // [gap]
        // Return to Game (full-width, enabled)
        const float bw = pw - 40.0f;
        const float bh = 30.0f;
        const float halfW = (bw - 6.0f) * 0.5f;
        float curY = py + 72.0f;

        auto drawBtn = [&](int idx, float bx, float by, float w, float h, const std::string& label) {
            if (idx >= (int)items.size()) return;
            int action = items[idx].first;
            bool enabled = IsGameMenuActionEnabled(action);
            gameMenuRects_[idx] = UiRect{bx, by, w, h};
            if (menuButtonBackdrop_) {
                BackdropFrame* bd = (action == pressedGameMenuAction_ && menuButtonPushedBackdrop_)
                                        ? menuButtonPushedBackdrop_
                                        : menuButtonBackdrop_;
                bd->SetPosition(bx, by);
                bd->SetSize(w, h);
                bd->Render(r);
                if (enabled && action == hoveredGameMenuAction_ && action != pressedGameMenuAction_) {
                    r.DrawQuad(bx, by, w, h, 0x33FFFFFF);
                }
            } else {
                uint32_t bg = 0xFF2A2A38;
                if (!enabled) bg = 0xFF1F1F2A;
                else if (action == pressedGameMenuAction_) bg = 0xFF1A1A26;
                else if (action == hoveredGameMenuAction_) bg = 0xFF3E3E55;
                r.DrawQuad(bx, by, w, h, bg);
                r.DrawQuad(bx, by, w, 1, 0xFF6A5A2A);
            }
            uint32_t textColor = enabled ? ((action == hoveredGameMenuAction_) ? 0xFFFFFFFF : 0xFFFFCC00) : 0xFF808080;
            float tw = textRenderer_.IsValid() ? textRenderer_.MeasureText(label.c_str(), 14) : (float)label.size() * 7.5f;
            DrawText(bx + (w - tw) * 0.5f, by + (h - 14) * 0.5f, label, textColor, 14);
        };

        // 0: Pause Game (disabled)
        drawBtn(0, px + 20, curY, bw, bh, "Pause Game");
        curY += bh + 6.0f;
        // 1: Save Game (disabled)
        drawBtn(1, px + 20, curY, bw, bh, "Save Game");
        curY += bh + 6.0f;
        // 2: Load Game (disabled)
        drawBtn(2, px + 20, curY, bw, bh, "Load Game");
        curY += bh + 6.0f;
        // 3: Options (disabled)
        drawBtn(3, px + 20, curY, bw, bh, "Options");
        curY += bh + 6.0f;
        // 4 & 5: Help & Tips side-by-side (disabled)
        drawBtn(4, px + 20, curY, halfW, bh, "Help");
        drawBtn(5, px + 20 + halfW + 6.0f, curY, halfW, bh, "Tips");
        curY += bh + 6.0f;
        // 6: End Game (enabled)
        drawBtn(6, px + 20, curY, bw, bh, "End Game");
        curY += bh + 24.0f; // Gap before Return to Game
        // 7: Return to Game (enabled)
        drawBtn(7, px + 20, curY, bw, bh, "Return to Game");
    } else {
        DrawButtonList(items, px + 20, py + 76, pw - 40, 30,
                       hoveredGameMenuAction_, pressedGameMenuAction_, gameMenuRects_);
    }
}

void FdfHudHost::OpenGluePage(GluePage page) {
    glueActive_ = true;
    gluePage_ = page;
    glueQuitConfirmOpen_ = false;
    hoveredGlueAction_ = pressedGlueAction_ = -1;
}

void FdfHudHost::OpenGlueMenuDebugPage(const std::string& pageName) {
    const std::string key = ToLowerNoSpace(pageName);
    glueQuitConfirmOpen_ = false;
    glueProfileOpen_ = false;
    if (key.empty() || key == "main" || key == "mainmenu" || key == "home") OpenGluePage(GluePage::MainMenu);
    else if (key == "quitconfirm" || key == "exitconfirm") {
        OpenGluePage(GluePage::MainMenu);
        glueQuitConfirmOpen_ = true;
    } else if (key == "battlenet" || key == "bnet") OpenGluePage(GluePage::BattleNet);
    else if (key == "localareanetwork" || key == "lan") OpenGluePage(GluePage::LocalAreaNetwork);
    else if (key == "singleplayer") OpenGluePage(GluePage::SinglePlayer);
    else if (key == "profile") {
        OpenGluePage(GluePage::SinglePlayer);
        glueProfileOpen_ = true;
    } else if (key == "campaign") OpenGluePage(GluePage::Campaign);
    else if (key == "missionselect" || key == "mission") OpenGluePage(GluePage::MissionSelect);
    else if (key == "customgame" || key == "skirmish") OpenGluePage(GluePage::CustomGame);
    else if (key == "options") OpenGluePage(GluePage::Options);
    else OpenGluePage(GluePage::MainMenu);
}

void FdfHudHost::GlueSelectCampaign(int index) {
    if (!glueCampaigns_.empty()) index = std::clamp(index, 0, (int)glueCampaigns_.size() - 1);
    glueSelectedCampaign_ = std::max(0, index);
    glueSelectedMission_ = 0;
    gluePage_ = GluePage::MissionSelect;
}

bool FdfHudHost::IsGlueActionVisible(int action) const {
    if (action < 0 || action >= kGlueActionCount) return false;
    if (glueQuitConfirmOpen_) return action == 6 || action == 7;
    switch (gluePage_) {
    case GluePage::MainMenu:
        return action <= 5;
    case GluePage::SinglePlayer:
        return (action >= 8 && action <= 13) || action == 14;
    case GluePage::Campaign:
        if (!glueCampaigns_.empty()) return action == 15;
        return action >= 16 && action <= 20;
    case GluePage::MissionSelect:
        if (action == 33) return true;
        if (action < 21 || action > 32) return false;
        if (!glueCampaigns_.empty() && glueSelectedCampaign_ < (int)glueCampaigns_.size())
            return action - 21 < (int)glueCampaigns_[glueSelectedCampaign_].missions.size();
        return true;
    case GluePage::CustomGame:
        if (action >= 41 && action <= 50) {
            const int row = action - 41;
            return row < (int)glueMaps_.size() - glueMapOffset_ && row >= 0;
        }
        if (action == 51) return glueMapOffset_ > 0;
        if (action == 52) return glueMapOffset_ + 10 < (int)glueMaps_.size();
        if (action >= 38 && action <= 40) return glueAdvancedOpen_;
        return action == 34 || action == 35 || action == 36 || action == 37;
    case GluePage::Options:
        if (glueSelectedOptionsTab_ < 0) return action >= 77 && action <= 81;
        return action == 82 || action == 83;
    case GluePage::BattleNet:
        return action >= 84 && action <= 87;
    case GluePage::LocalAreaNetwork:
        return action >= 88 && action <= 91;
    }
    return false;
}

std::vector<std::pair<int, std::string>> FdfHudHost::CurrentGlueItems() const {
    std::vector<std::pair<int, std::string>> items;
    for (int a = 0; a < kGlueActionCount; ++a) {
        if (!IsGlueActionVisible(a)) continue;
        // Slot name/race rows are display-only.
        if (a >= 53 && a <= 76) continue;
        items.emplace_back(a, GlueActionName(a));
    }
    return items;
}

void FdfHudHost::HandleGlueMenuAction(int action) {
    if (!IsGlueActionVisible(action)) return;
    std::cout << "FdfHudHost: glue action '" << GlueActionName(action) << "'" << std::endl;
    switch (action) {
    case 0: OpenGluePage(GluePage::SinglePlayer); glueProfileOpen_ = false; break;
    case 1: OpenGluePage(GluePage::BattleNet); break;
    case 2: OpenGluePage(GluePage::LocalAreaNetwork); break;
    case 3: OpenGluePage(GluePage::Options); glueSelectedOptionsTab_ = -1; break;
    case 4: std::cout << "FdfHudHost: Credits requested (page pending)" << std::endl; break;
    case 5: glueQuitConfirmOpen_ = true; break;
    case 6:
        glueQuitConfirmOpen_ = false;
        std::cout << "FdfHudHost: Exit confirmed (application exit suppressed)" << std::endl;
        break;
    case 7: glueQuitConfirmOpen_ = false; break;
    case 8: glueProfileOpen_ = true; break;
    case 9: std::cout << "FdfHudHost: Load Saved Game requested (page pending)" << std::endl; break;
    case 10: std::cout << "FdfHudHost: View Replay requested (page pending)" << std::endl; break;
    case 11: std::cout << "FdfHudHost: Custom Campaign requested (page pending)" << std::endl; break;
    case 12: OpenGluePage(GluePage::Campaign); break;
    case 13:
        OpenGluePage(GluePage::CustomGame);
        glueMapInfoOpen_ = glueAdvancedOpen_ = false;
        break;
    case 14:
        glueQuitConfirmOpen_ = false;
        if (glueProfileOpen_) glueProfileOpen_ = false;
        else OpenGluePage(GluePage::MainMenu);
        break;
    case 15: OpenGluePage(GluePage::SinglePlayer); glueProfileOpen_ = false; break;
    case 16: case 17: case 18: case 19: case 20:
        GlueSelectCampaign(action - 16);
        break;
    case 33: OpenGluePage(GluePage::Campaign); break;
    case 34: {
        std::string path;
        if (glueSelectedMap_ >= 0 && glueSelectedMap_ < (int)glueMaps_.size())
            path = glueMaps_[glueSelectedMap_].path;
        std::cout << "FdfHudHost: Play Game map='" << path << "'" << std::endl;
        if (glueMapLaunchCallback_ && !path.empty()) glueMapLaunchCallback_(path, 0);
        break;
    }
    case 35: OpenGluePage(GluePage::SinglePlayer); break;
    case 36:
        glueMapInfoOpen_ = !glueMapInfoOpen_;
        if (glueMapInfoOpen_) glueAdvancedOpen_ = false;
        break;
    case 37:
        glueAdvancedOpen_ = !glueAdvancedOpen_;
        if (glueAdvancedOpen_) glueMapInfoOpen_ = false;
        break;
    case 38: glueTeamsTogether_ = !glueTeamsTogether_; break;
    case 39: glueFixedTeams_ = !glueFixedTeams_; break;
    case 40: glueFullShare_ = !glueFullShare_; break;
    case 51: glueMapOffset_ = std::max(0, glueMapOffset_ - 10); break;
    case 52: glueMapOffset_ = std::max(0, glueMapOffset_ + 10); break;
    case 77: case 78: case 79: case 80:
        glueSelectedOptionsTab_ = action - 77;
        break;
    case 81: OpenGluePage(GluePage::MainMenu); break;
    case 82: case 83: glueSelectedOptionsTab_ = -1; break;
    case 87: OpenGluePage(GluePage::MainMenu); break;
    case 91: OpenGluePage(GluePage::MainMenu); break;
    default:
        if (action >= 21 && action <= 32) {
            glueSelectedMission_ = action - 21;
            std::string path = "Maps\\Campaign\\Prologue01.w3m";
            if (glueSelectedCampaign_ < (int)glueCampaigns_.size()) {
                const auto& missions = glueCampaigns_[glueSelectedCampaign_].missions;
                if (glueSelectedMission_ < (int)missions.size() && !missions[glueSelectedMission_].mapPath.empty())
                    path = missions[glueSelectedMission_].mapPath;
            }
            std::cout << "FdfHudHost: campaign mission map='" << path << "'" << std::endl;
            if (glueMapLaunchCallback_) glueMapLaunchCallback_(path, 0);
        } else if (action >= 41 && action <= 50) {
            glueSelectedMap_ = std::min((int)glueMaps_.size() - 1, glueMapOffset_ + (action - 41));
        } else if (action == 84 || action == 85 || action == 86 || action == 88 || action == 89 ||
                   action == 90) {
            std::cout << "FdfHudHost: '" << GlueActionName(action) << "' not available offline" << std::endl;
        }
        break;
    }
}

int FdfHudHost::HitGlueMenuButton(float x, float y) const {
    if (!glueActive_) return -1;
    auto items = CurrentGlueItems();
    for (size_t i = 0; i < items.size() && i < glueRects_.size(); ++i) {
        if (glueRects_[i].Contains(x, y)) return items[i].first;
    }
    return -1;
}

void FdfHudHost::SetHoveredGlueMenuButton(int action) { hoveredGlueAction_ = action; }
void FdfHudHost::SetPressedGlueMenuButton(int action) { pressedGlueAction_ = action; }

bool FdfHudHost::InvokeGlueMenuActionForDebug(const std::string& actionName) {
    if (!glueActive_) OpenGluePage(GluePage::MainMenu);
    const int action = GlueActionForName(actionName);
    if (action == -2) {
        OpenGluePage(GluePage::MainMenu);
        return true;
    }
    if (action < 0) return false;
    HandleGlueMenuAction(action);
    return true;
}

void FdfHudHost::SetGlueMenuMaps(const std::vector<War3UI::GlueMapListItem>& maps) {
    glueMaps_ = maps;
    glueSelectedMap_ = 0;
    glueMapOffset_ = 0;
}

void FdfHudHost::SetGlueMenuCampaigns(const std::vector<War3UI::GlueCampaignListItem>& campaigns) {
    glueCampaigns_ = campaigns;
    glueSelectedCampaign_ = 0;
    glueSelectedMission_ = 0;
}

void FdfHudHost::SetGlueMapLaunchCallback(War3UI::GlueMapLaunchCallback cb) {
    glueMapLaunchCallback_ = std::move(cb);
}

void FdfHudHost::DrawGlueMenu() {
    UiRenderer& r = gui_.renderer;
    r.DrawQuad(0, 0, (float)width_, (float)height_, 0xFF0A0C14);
    const float pw = 420;
    const float px = (width_ - pw) * 0.5f, py = height_ * 0.12f;
    const char* titles[] = {"Warcraft III", "Battle.net", "Local Area Network", "Single Player",
                            "Campaign", "Mission Select", "Custom Game", "Options"};
    DrawText(px + 16, py, titles[(int)gluePage_], 0xFFFFCC66, 22);
    auto items = CurrentGlueItems();
    glueRects_.assign(items.size(), UiRect{});
    // Campaign page with data lists campaigns; custom game lists maps inline below.
    if (gluePage_ == GluePage::Campaign && !glueCampaigns_.empty()) {
        items.clear();
        for (size_t i = 0; i < glueCampaigns_.size() && i < 5; ++i)
            items.emplace_back(16 + (int)i, glueCampaigns_[i].name.empty() ? glueCampaigns_[i].id
                                                                           : glueCampaigns_[i].name);
        items.emplace_back(15, GlueActionName(15));
        glueRects_.assign(items.size(), UiRect{});
    }
    DrawButtonList(items, px, py + 44, pw, 30, hoveredGlueAction_, pressedGlueAction_, glueRects_);
    float infoY = py + 44 + items.size() * 34 + 12;
    if (gluePage_ == GluePage::CustomGame && !glueMaps_.empty()) {
        const int sel = std::clamp(glueSelectedMap_, 0, (int)glueMaps_.size() - 1);
        const auto& m = glueMaps_[sel];
        DrawText(px, infoY, m.displayName.empty() ? m.fileName : m.displayName, 0xFFFFFFFF, 14);
        if (!m.description.empty()) DrawText(px, infoY + 22, m.description.substr(0, 80), 0xFFCCCCCC, 12);
        if (!m.recommendedPlayers.empty())
            DrawText(px, infoY + 42, "Players: " + m.recommendedPlayers, 0xFFCCCCCC, 12);
    }
    if (gluePage_ == GluePage::MissionSelect && glueSelectedCampaign_ < (int)glueCampaigns_.size()) {
        const auto& c = glueCampaigns_[glueSelectedCampaign_];
        DrawText(px, infoY, c.name.empty() ? c.id : c.name, 0xFFFFFFFF, 14);
    }
    if (glueProfileOpen_) DrawText(px, infoY, "Profile: <default>", 0xFFCCCCCC, 12);
    if (glueQuitConfirmOpen_) {
        r.DrawQuad(0, 0, (float)width_, (float)height_, 0x99000000);
        DrawText(width_ * 0.5f - 90, height_ * 0.4f, "Exit the game?", 0xFFFFFFFF, 18);
    }
}
