#pragma once

#include "MainUI.h"
#include <NsCore/Ptr.h>
#include <NsGui/IView.h>
#include <NsGui/UserControl.h>
#include <NsRender/RenderDevice.h>
#include <array>
#include <string>
#include <vector>

namespace Noesis {
class Border;
class Canvas;
class FrameworkElement;
class TextBlock;
class TextBox;
}

struct sapp_event;

class NoesisHudHost {
public:
    NoesisHudHost() = default;
    ~NoesisHudHost();

    bool Initialize(const std::string& assetRoot, int width, int height);
    void Shutdown();
    void Resize(int width, int height);
    void Update(double time);
    void Render();
    bool HandleEvent(const sapp_event* e);

    void SetResources(int gold, int lumber, int foodUsed, int foodCap, const std::string& timeText);
    void SetSelection(const std::string& name, int level, float hp, float maxHp, float mp, float maxMp,
                      bool heroVisible, int xp, int maxXp,
                      int strBase, int strBonus, int agiBase, int agiBonus, int intBase, int intBonus);
    void SetTooltip(const std::string& text, float screenX, float screenY, bool visible);
    void SetChatOpen(bool open);
    void SetChatInput(const std::string& text);
    std::string GetChatInput() const;
    void SetChatMessages(const std::vector<std::string>& messages);
    void SetCommandSlot(int index, const std::string& label, bool enabled, bool visible);

    ScreenRect GetMinimapRect() const { return minimapRect_; }
    ScreenRect GetPortraitRect() const { return portraitRect_; }
    ScreenRect GetCommandRect(int index) const;
    int HitCommandButton(float x, float y) const;

private:
    void CacheElements();
    void CacheRects();
    void ApplyState();
    ScreenRect GetElementRect(Noesis::FrameworkElement* element) const;

    std::string assetRoot_;

    Noesis::Ptr<Noesis::IView> view_;
    Noesis::Ptr<Noesis::UserControl> root_;
    Noesis::Ptr<Noesis::RenderDevice> device_;

    Noesis::TextBlock* goldText_ = nullptr;
    Noesis::TextBlock* lumberText_ = nullptr;
    Noesis::TextBlock* foodText_ = nullptr;
    Noesis::TextBlock* timeText_ = nullptr;
    Noesis::TextBlock* unitNameText_ = nullptr;
    Noesis::TextBlock* unitLevelText_ = nullptr;
    Noesis::TextBlock* hpText_ = nullptr;
    Noesis::TextBlock* mpText_ = nullptr;
    Noesis::TextBlock* heroStatsText_ = nullptr;
    Noesis::Border* hpFill_ = nullptr;
    Noesis::Border* mpFill_ = nullptr;
    Noesis::Border* xpFill_ = nullptr;
    Noesis::Border* tooltipPanel_ = nullptr;
    Noesis::TextBlock* tooltipText_ = nullptr;
    Noesis::Border* chatPanel_ = nullptr;
    Noesis::TextBlock* chatLogText_ = nullptr;
    Noesis::TextBox* chatInput_ = nullptr;
    Noesis::FrameworkElement* minimapViewport_ = nullptr;
    Noesis::FrameworkElement* portraitViewport_ = nullptr;
    std::array<Noesis::FrameworkElement*, 12> commandButtons_{};
    std::array<Noesis::TextBlock*, 12> commandLabels_{};

    int width_ = 0;
    int height_ = 0;
    float dpiScale_ = 1.0f;
    bool guiInitialized_ = false;
    bool initialized_ = false;

    int gold_ = 0;
    int lumber_ = 0;
    int foodUsed_ = 0;
    int foodCap_ = 0;
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

    bool tooltipVisible_ = false;
    std::string tooltipValue_;
    float tooltipX_ = 0.0f;
    float tooltipY_ = 0.0f;

    bool chatOpen_ = false;
    std::string chatJoinedText_;
    std::string chatInputText_;

    struct CommandState {
        std::string label;
        bool enabled = false;
        bool visible = false;
    };
    std::array<CommandState, 12> commandStates_{};
    std::array<ScreenRect, 12> commandRects_{};

    ScreenRect minimapRect_;
    ScreenRect portraitRect_;
};
