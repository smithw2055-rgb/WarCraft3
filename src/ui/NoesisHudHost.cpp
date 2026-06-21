#include "NoesisHudHost.h"

#include "third_party/sokol/sokol_app.h"
#include <NsCore/DynamicCast.h>
#include <NsCore/Noesis.h>
#include <NsGui/Border.h>
#include <NsGui/Canvas.h>
#include <NsGui/Enums.h>
#include <NsGui/FrameworkElement.h>
#include <NsGui/IRenderer.h>
#include <NsGui/IntegrationAPI.h>
#include <NsGui/TextBlock.h>
#include <NsGui/TextBox.h>
#include <NsGui/Uri.h>
#include <NsGui/UserControl.h>
#include <NsGui/Visual.h>
#include <NsApp/Launcher.h>
#include <NsApp/LocalFontProvider.h>
#include <NsApp/LocalTextureProvider.h>
#include <NsApp/LocalXamlProvider.h>
#include <NsApp/ThemeProviders.h>
#include <NsRender/GLFactory.h>
#include <algorithm>
#include <cstdio>
#include <sstream>

#ifndef NS_LICENSE_NAME
#define NS_LICENSE_NAME ""
#endif

#ifndef NS_LICENSE_KEY
#define NS_LICENSE_KEY ""
#endif

namespace {

int ToLogicalPixels(int physicalPixels, float dpiScale) {
    if (physicalPixels <= 0) return physicalPixels;
    if (dpiScale <= 0.0f) return physicalPixels;
    return std::max(1, (int)(physicalPixels / dpiScale + 0.5f));
}

void InstallLogHandler() {
    static bool installed = false;
    if (installed) return;
    installed = true;

    Noesis::GUI::SetLogHandler([](const char*, uint32_t, uint32_t level, const char*, const char* msg) {
        const char* prefixes[] = {"T", "D", "I", "W", "E"};
        std::fprintf(stderr, "[NOESIS/%s] %s\n", prefixes[level], msg);
    });
}

Noesis::Key MapKey(int sokolKey) {
    switch (sokolKey) {
    case SAPP_KEYCODE_BACKSPACE: return Noesis::Key_Back;
    case SAPP_KEYCODE_TAB: return Noesis::Key_Tab;
    case SAPP_KEYCODE_ENTER: return Noesis::Key_Return;
    case SAPP_KEYCODE_ESCAPE: return Noesis::Key_Escape;
    case SAPP_KEYCODE_SPACE: return Noesis::Key_Space;
    case SAPP_KEYCODE_DELETE: return Noesis::Key_Delete;
    case SAPP_KEYCODE_LEFT: return Noesis::Key_Left;
    case SAPP_KEYCODE_RIGHT: return Noesis::Key_Right;
    case SAPP_KEYCODE_UP: return Noesis::Key_Up;
    case SAPP_KEYCODE_DOWN: return Noesis::Key_Down;
    case SAPP_KEYCODE_HOME: return Noesis::Key_Home;
    case SAPP_KEYCODE_END: return Noesis::Key_End;
    case SAPP_KEYCODE_PAGE_UP: return Noesis::Key_PageUp;
    case SAPP_KEYCODE_PAGE_DOWN: return Noesis::Key_PageDown;
    case SAPP_KEYCODE_INSERT: return Noesis::Key_Insert;
    case SAPP_KEYCODE_F1: return Noesis::Key_F1;
    case SAPP_KEYCODE_F2: return Noesis::Key_F2;
    case SAPP_KEYCODE_F3: return Noesis::Key_F3;
    case SAPP_KEYCODE_F4: return Noesis::Key_F4;
    case SAPP_KEYCODE_F5: return Noesis::Key_F5;
    case SAPP_KEYCODE_F6: return Noesis::Key_F6;
    case SAPP_KEYCODE_F7: return Noesis::Key_F7;
    case SAPP_KEYCODE_F8: return Noesis::Key_F8;
    case SAPP_KEYCODE_F9: return Noesis::Key_F9;
    case SAPP_KEYCODE_F10: return Noesis::Key_F10;
    case SAPP_KEYCODE_F11: return Noesis::Key_F11;
    case SAPP_KEYCODE_F12: return Noesis::Key_F12;
    default: break;
    }

    if (sokolKey >= SAPP_KEYCODE_A && sokolKey <= SAPP_KEYCODE_Z) {
        return static_cast<Noesis::Key>(Noesis::Key_A + (sokolKey - SAPP_KEYCODE_A));
    }
    if (sokolKey >= SAPP_KEYCODE_0 && sokolKey <= SAPP_KEYCODE_9) {
        return static_cast<Noesis::Key>(Noesis::Key_D0 + (sokolKey - SAPP_KEYCODE_0));
    }
    return Noesis::Key_None;
}

template<class T>
T* FindAs(Noesis::FrameworkElement* root, const char* name) {
    return root ? Noesis::DynamicCast<T*>(root->FindName(name)) : nullptr;
}

std::string JoinLines(const std::vector<std::string>& lines) {
    std::ostringstream ss;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) ss << '\n';
        ss << lines[i];
    }
    return ss.str();
}

} // namespace

NoesisHudHost::~NoesisHudHost() {
    Shutdown();
}

bool NoesisHudHost::Initialize(const std::string& assetRoot, int width, int height) {
    if (initialized_) return true;

    assetRoot_ = assetRoot;
    dpiScale_ = sapp_dpi_scale();
    width_ = ToLogicalPixels(width, dpiScale_);
    height_ = ToLogicalPixels(height, dpiScale_);

    InstallLogHandler();
    Noesis::GUI::SetLicense(NS_LICENSE_NAME, NS_LICENSE_KEY);
    Noesis::GUI::Init();
    guiInitialized_ = true;

    NoesisApp::Launcher::RegisterAppComponents();
    Noesis::GUI::SetXamlProvider(Noesis::MakePtr<NoesisApp::LocalXamlProvider>(assetRoot_.c_str()));
    Noesis::GUI::SetTextureProvider(Noesis::MakePtr<NoesisApp::LocalTextureProvider>(assetRoot_.c_str()));
    Noesis::GUI::SetFontProvider(Noesis::MakePtr<NoesisApp::LocalFontProvider>(assetRoot_.c_str()));
    NoesisApp::SetThemeProviders();
    Noesis::GUI::LoadApplicationResources(NoesisApp::Theme::DarkBlue());

    root_ = Noesis::GUI::LoadXaml<Noesis::UserControl>("assets/noesis/Hud.xaml");
    if (!root_) {
        std::fprintf(stderr, "NoesisHudHost: failed to load assets/noesis/Hud.xaml\n");
        return false;
    }

    view_ = Noesis::GUI::CreateView(root_);
    if (!view_) {
        std::fprintf(stderr, "NoesisHudHost: failed to create view\n");
        return false;
    }

    view_->SetFlags(Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD);
    view_->SetSize((uint32_t)width_, (uint32_t)height_);
    view_->SetScale(dpiScale_);

    device_ = NoesisApp::GLFactory::CreateDevice(false);
    if (!device_) {
        std::fprintf(stderr, "NoesisHudHost: failed to create GL render device\n");
        return false;
    }

    view_->GetRenderer()->Init(device_.GetPtr());
    CacheElements();
    initialized_ = true;
    ApplyState();
    return true;
}

void NoesisHudHost::Shutdown() {
    if (initialized_ && view_) {
        view_->GetRenderer()->Shutdown();
    }
    initialized_ = false;
    device_.Reset();
    root_.Reset();
    view_.Reset();

    if (guiInitialized_) {
        Noesis::GUI::Shutdown();
        guiInitialized_ = false;
    }
}

void NoesisHudHost::Resize(int width, int height) {
    if (width <= 0 || height <= 0 || !view_) return;

    float scale = sapp_dpi_scale();
    if (width_ == width && height_ == height && dpiScale_ == scale) return;

    width_ = width;
    height_ = height;
    dpiScale_ = scale;
    view_->SetSize((uint32_t)width_, (uint32_t)height_);
    view_->SetScale(dpiScale_);
}

void NoesisHudHost::Update(double time) {
    if (!initialized_ || !view_) return;
    Resize(ToLogicalPixels(sapp_width(), sapp_dpi_scale()),
           ToLogicalPixels(sapp_height(), sapp_dpi_scale()));
    if (chatInput_ && chatOpen_) {
        chatInputText_ = chatInput_->GetText();
    }
    ApplyState();
    view_->Update(time);
    CacheRects();
}

void NoesisHudHost::Render() {
    if (!initialized_ || !view_) return;

    sg_reset_state_cache();
    auto* renderer = view_->GetRenderer();
    renderer->UpdateRenderTree();
    renderer->RenderOffscreen();
    renderer->Render(false);
    sg_reset_state_cache();
}

bool NoesisHudHost::HandleEvent(const sapp_event* e) {
    if (!initialized_ || !view_ || !e) return false;

    float x = e->mouse_x;
    float y = e->mouse_y;

    switch (e->type) {
    case SAPP_EVENTTYPE_MOUSE_DOWN: {
        Noesis::MouseButton btn = Noesis::MouseButton_Left;
        if (e->mouse_button == SAPP_MOUSEBUTTON_RIGHT) btn = Noesis::MouseButton_Right;
        else if (e->mouse_button == SAPP_MOUSEBUTTON_MIDDLE) btn = Noesis::MouseButton_Middle;
        return view_->MouseButtonDown(x, y, btn);
    }
    case SAPP_EVENTTYPE_MOUSE_UP: {
        Noesis::MouseButton btn = Noesis::MouseButton_Left;
        if (e->mouse_button == SAPP_MOUSEBUTTON_RIGHT) btn = Noesis::MouseButton_Right;
        else if (e->mouse_button == SAPP_MOUSEBUTTON_MIDDLE) btn = Noesis::MouseButton_Middle;
        return view_->MouseButtonUp(x, y, btn);
    }
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        return view_->MouseMove(x, y);
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
        return view_->MouseWheel(x, y, e->scroll_y * 3.5f);
    case SAPP_EVENTTYPE_KEY_DOWN:
        return view_->KeyDown(MapKey(e->key_code));
    case SAPP_EVENTTYPE_KEY_UP:
        return view_->KeyUp(MapKey(e->key_code));
    case SAPP_EVENTTYPE_CHAR:
        return view_->Char(e->char_code);
    case SAPP_EVENTTYPE_RESIZED:
        Resize(e->window_width, e->window_height);
        return true;
    default:
        return false;
    }
}

void NoesisHudHost::SetResources(int gold, int lumber, int foodUsed, int foodCap, const std::string& timeText) {
    gold_ = gold;
    lumber_ = lumber;
    foodUsed_ = foodUsed;
    foodCap_ = foodCap;
    timeValue_ = timeText;
}

void NoesisHudHost::SetSelection(const std::string& name, int level, float hp, float maxHp, float mp, float maxMp,
                                 bool heroVisible, int xp, int maxXp,
                                 int strBase, int strBonus, int agiBase, int agiBonus, int intBase, int intBonus) {
    unitName_ = name;
    unitLevel_ = level;
    hp_ = hp;
    maxHp_ = maxHp;
    mp_ = mp;
    maxMp_ = maxMp;
    heroVisible_ = heroVisible;
    heroXp_ = xp;
    heroMaxXp_ = maxXp;
    heroStrBase_ = strBase;
    heroStrBonus_ = strBonus;
    heroAgiBase_ = agiBase;
    heroAgiBonus_ = agiBonus;
    heroIntBase_ = intBase;
    heroIntBonus_ = intBonus;
}

void NoesisHudHost::SetTooltip(const std::string& text, float screenX, float screenY, bool visible) {
    tooltipValue_ = text;
    tooltipX_ = screenX;
    tooltipY_ = screenY;
    tooltipVisible_ = visible && !text.empty();
}

void NoesisHudHost::SetChatOpen(bool open) {
    chatOpen_ = open;
}

void NoesisHudHost::SetChatInput(const std::string& text) {
    chatInputText_ = text;
    if (chatInput_) {
        chatInput_->SetText(chatInputText_.c_str());
        if (chatOpen_) chatInput_->Focus();
    }
}

std::string NoesisHudHost::GetChatInput() const {
    return chatInput_ ? std::string(chatInput_->GetText()) : chatInputText_;
}

void NoesisHudHost::SetChatMessages(const std::vector<std::string>& messages) {
    chatJoinedText_ = JoinLines(messages);
}

void NoesisHudHost::SetCommandSlot(int index, const std::string& label, bool enabled, bool visible) {
    if (index < 0 || index >= (int)commandStates_.size()) return;
    commandStates_[index].label = label;
    commandStates_[index].enabled = enabled;
    commandStates_[index].visible = visible;
}

int NoesisHudHost::HitCommandButton(float x, float y) const {
    for (int i = 0; i < (int)commandRects_.size(); ++i) {
        if (commandStates_[i].visible && commandStates_[i].enabled && commandRects_[i].Contains(x, y)) {
            return i;
        }
    }
    return -1;
}

ScreenRect NoesisHudHost::GetCommandRect(int index) const {
    if (index < 0 || index >= (int)commandRects_.size()) return {};
    return commandRects_[index];
}

void NoesisHudHost::CacheElements() {
    auto* rootElement = root_.GetPtr();
    goldText_ = FindAs<Noesis::TextBlock>(rootElement, "GoldText");
    lumberText_ = FindAs<Noesis::TextBlock>(rootElement, "LumberText");
    foodText_ = FindAs<Noesis::TextBlock>(rootElement, "FoodText");
    timeText_ = FindAs<Noesis::TextBlock>(rootElement, "TimeText");
    unitNameText_ = FindAs<Noesis::TextBlock>(rootElement, "UnitNameText");
    unitLevelText_ = FindAs<Noesis::TextBlock>(rootElement, "UnitLevelText");
    hpText_ = FindAs<Noesis::TextBlock>(rootElement, "HpText");
    mpText_ = FindAs<Noesis::TextBlock>(rootElement, "MpText");
    heroStatsText_ = FindAs<Noesis::TextBlock>(rootElement, "HeroStatsText");
    hpFill_ = FindAs<Noesis::Border>(rootElement, "HpFill");
    mpFill_ = FindAs<Noesis::Border>(rootElement, "MpFill");
    xpFill_ = FindAs<Noesis::Border>(rootElement, "XpFill");
    tooltipPanel_ = FindAs<Noesis::Border>(rootElement, "TooltipPanel");
    tooltipText_ = FindAs<Noesis::TextBlock>(rootElement, "TooltipText");
    chatPanel_ = FindAs<Noesis::Border>(rootElement, "ChatPanel");
    chatLogText_ = FindAs<Noesis::TextBlock>(rootElement, "ChatLogText");
    chatInput_ = FindAs<Noesis::TextBox>(rootElement, "ChatInputBox");
    minimapViewport_ = FindAs<Noesis::FrameworkElement>(rootElement, "MinimapViewport");
    portraitViewport_ = FindAs<Noesis::FrameworkElement>(rootElement, "PortraitViewport");

    for (int i = 0; i < 12; ++i) {
        char buttonName[32];
        char textName[32];
        std::snprintf(buttonName, sizeof(buttonName), "CommandButton%d", i);
        std::snprintf(textName, sizeof(textName), "CommandText%d", i);
        commandButtons_[i] = FindAs<Noesis::FrameworkElement>(rootElement, buttonName);
        commandLabels_[i] = FindAs<Noesis::TextBlock>(rootElement, textName);
    }
}

void NoesisHudHost::CacheRects() {
    minimapRect_ = GetElementRect(minimapViewport_);
    portraitRect_ = GetElementRect(portraitViewport_);
    for (int i = 0; i < 12; ++i) {
        commandRects_[i] = GetElementRect(commandButtons_[i]);
    }
}

ScreenRect NoesisHudHost::GetElementRect(Noesis::FrameworkElement* element) const {
    if (!element) return {};
    Noesis::Point p = element->PointToScreen(Noesis::Point(0.0f, 0.0f));
    const float scale = dpiScale_ > 0.0f ? dpiScale_ : 1.0f;
    return ScreenRect{
        p.x * scale,
        p.y * scale,
        element->GetActualWidth() * scale,
        element->GetActualHeight() * scale
    };
}

void NoesisHudHost::ApplyState() {
    if (goldText_) goldText_->SetText(std::to_string(gold_).c_str());
    if (lumberText_) lumberText_->SetText(std::to_string(lumber_).c_str());
    if (foodText_) {
        foodText_->SetText((std::to_string(foodUsed_) + " / " + std::to_string(foodCap_)).c_str());
    }
    if (timeText_) timeText_->SetText(timeValue_.c_str());

    if (unitNameText_) unitNameText_->SetText(unitName_.c_str());
    if (unitLevelText_) {
        unitLevelText_->SetText(unitLevel_ > 0 ? ("Lv. " + std::to_string(unitLevel_)).c_str() : "");
    }

    int hpValue = (int)std::max(0.0f, hp_);
    int hpMaxValue = (int)std::max(0.0f, maxHp_);
    int mpValue = (int)std::max(0.0f, mp_);
    int mpMaxValue = (int)std::max(0.0f, maxMp_);
    float hpRatio = maxHp_ > 0.0f ? std::clamp(hp_ / maxHp_, 0.0f, 1.0f) : 0.0f;
    float mpRatio = maxMp_ > 0.0f ? std::clamp(mp_ / maxMp_, 0.0f, 1.0f) : 0.0f;
    float xpRatio = heroMaxXp_ > 0 ? std::clamp((float)heroXp_ / (float)heroMaxXp_, 0.0f, 1.0f) : 0.0f;

    if (hpText_) hpText_->SetText((std::to_string(hpValue) + " / " + std::to_string(hpMaxValue)).c_str());
    if (mpText_) mpText_->SetText((std::to_string(mpValue) + " / " + std::to_string(mpMaxValue)).c_str());
    if (hpFill_) hpFill_->SetWidth(194.0f * hpRatio);
    if (mpFill_) mpFill_->SetWidth(194.0f * mpRatio);
    if (xpFill_) xpFill_->SetWidth(194.0f * xpRatio);

    if (heroStatsText_) {
        std::ostringstream ss;
        if (heroVisible_) {
            ss << "STR " << heroStrBase_;
            if (heroStrBonus_) ss << "+" << heroStrBonus_;
            ss << "    AGI " << heroAgiBase_;
            if (heroAgiBonus_) ss << "+" << heroAgiBonus_;
            ss << "    INT " << heroIntBase_;
            if (heroIntBonus_) ss << "+" << heroIntBonus_;
            heroStatsText_->SetText(ss.str().c_str());
            heroStatsText_->SetVisibility(Noesis::Visibility_Visible);
        } else {
            heroStatsText_->SetText("Unit command console");
            heroStatsText_->SetVisibility(Noesis::Visibility_Visible);
        }
    }

    if (tooltipText_) tooltipText_->SetText(tooltipValue_.c_str());
    if (tooltipPanel_) {
        tooltipPanel_->SetVisibility(tooltipVisible_ ? Noesis::Visibility_Visible : Noesis::Visibility_Collapsed);
        float tx = std::clamp(tooltipX_ + 12.0f, 12.0f, std::max(12.0f, (float)width_ - 280.0f));
        float ty = std::clamp(tooltipY_ - 86.0f, 12.0f, std::max(12.0f, (float)height_ - 120.0f));
        Noesis::Canvas::SetLeft(tooltipPanel_, tx);
        Noesis::Canvas::SetTop(tooltipPanel_, ty);
    }

    if (chatPanel_) {
        chatPanel_->SetVisibility(chatOpen_ ? Noesis::Visibility_Visible : Noesis::Visibility_Collapsed);
    }
    if (chatLogText_) {
        chatLogText_->SetText(chatJoinedText_.c_str());
    }
    if (chatInput_) {
        if (std::string(chatInput_->GetText()) != chatInputText_) {
            chatInput_->SetText(chatInputText_.c_str());
        }
        if (chatOpen_) chatInput_->Focus();
    }

    for (int i = 0; i < 12; ++i) {
        if (commandLabels_[i]) {
            commandLabels_[i]->SetText(commandStates_[i].label.c_str());
        }
        if (commandButtons_[i]) {
            commandButtons_[i]->SetVisibility(commandStates_[i].visible || !commandStates_[i].label.empty()
                ? Noesis::Visibility_Visible
                : Noesis::Visibility_Collapsed);
        }
    }
}
