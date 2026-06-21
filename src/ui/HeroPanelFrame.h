#pragma once
#include "UIFrame.h"
#include "TextRenderer.h"
#include "../asset/Texture.h"
#include <string>

class HeroPanelFrame : public UIFrame {
public:
    HeroPanelFrame(const std::string& name, UIFrame* parent = nullptr);

    void SetTextRenderer(TextRenderer* tr) { textRenderer_ = tr; }
    void Clear();
    void SetVisible(bool v) { visible_ = v; }

    // Hero data
    void SetHeroName(const std::string& name) { heroName_ = name; }
    void SetHeroLevel(int level) { heroLevel_ = level; }
    void SetHeroIcon(const std::shared_ptr<Texture>& tex) { heroIcon_ = tex; }

    // XP bar
    void SetXp(int current, int max) { xp_ = current; maxXp_ = max; }
    void SetHeroXp(float ratio) { xpRatio_ = ratio; }

    // Attributes
    void SetStrength(int base, int bonus) { strBase_ = base; strBonus_ = bonus; }
    void SetAgility(int base, int bonus) { agiBase_ = base; agiBonus_ = bonus; }
    void SetIntelligence(int base, int bonus) { intBase_ = base; intBonus_ = bonus; }

    void Render(UiRenderer& r) override;

private:
    TextRenderer* textRenderer_ = nullptr;
    std::string heroName_;
    int heroLevel_ = 1;
    int xp_ = 0, maxXp_ = 100;
    float xpRatio_ = 0.0f;
    int strBase_ = 0, strBonus_ = 0;
    int agiBase_ = 0, agiBonus_ = 0;
    int intBase_ = 0, intBonus_ = 0;
    std::shared_ptr<Texture> heroIcon_;
};
