#pragma once
#include "UiRenderer.h"
#include "UiTypes.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

struct UiRect {
    float x = 0, y = 0, w = 0, h = 0;
    bool Contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

class UIFrame; // forward
class GameUI {
public:
    UiRenderer renderer;
    UIFrame* rootFrame = nullptr;
    int screenW = 1280, screenH = 720;

    bool Init(int w, int h);
    void Update(int w, int h);
    void Render();
    void ResolveTarget(const std::string& name);
    // FDF-unit conversion (ref GameUI.convertX/convertY in warsmash_ref):
    // FDF space is 0..0.8 x 0..0.6 with a bottom-left origin.
    float ConvertX(float fdfX) const { return (fdfX / 0.8f) * (float)screenW; }
    float ConvertY(float fdfY) const { return (fdfY / 0.6f) * (float)screenH; }
    // For simple name-based lookup (used by SetPoint resolution)
    std::unordered_map<std::string, UIFrame*> frameRegistry;
};

struct FramePointAssign {
    FramePoint myPoint;
    std::string targetName;
    FramePoint targetPoint;
    float x = 0, y = 0;
};

class UIFrame {
public:
    UIFrame(const std::string& name, UIFrame* parent = nullptr);
    virtual ~UIFrame() = default;

    virtual void Render(UiRenderer& r);
    virtual void PositionBounds(GameUI& gui);

    void SetAllPoints();
    void SetAllPoints(float inset);
    void SetWidth(float w) { assignedW_ = w; bounds_.w = w; }
    void SetHeight(float h) { assignedH_ = h; bounds_.h = h; }
    float GetWidth() const { return bounds_.w; }
    float GetHeight() const { return bounds_.h; }
    float GetFrameX(FramePoint pt) const;
    float GetFrameY(FramePoint pt) const;
    void SetFrameX(FramePoint pt, float x);
    void SetFrameY(FramePoint pt, float y);
    void AddSetPoint(FramePoint myPt, const std::string& other, FramePoint otherPt, float ox, float oy);
    void AddAnchor(FramePoint pt, float ox, float oy);

    void SetVisible(bool v) { visible_ = v; }
    bool IsVisible() const { return visible_; }
    // FDF frames built without any SetPoint/anchor/size-stretch assignment
    // default to their parent's top-left (ref Java, where position-less
    // children stay at the parent origin instead of the viewport origin).
    // Explicitly placed code frames never set this.
    void SetDefaultToParent(bool v) { defaultToParent_ = v; }

    virtual UIFrame* HitTest(float sx, float sy);
    virtual UIFrame* OnMouseDown(float sx, float sy, int btn);
    virtual UIFrame* OnMouseUp(float sx, float sy, int btn);

    void SetPosition(float x, float y) { bounds_.x = x; bounds_.y = y; }
    void SetSize(float w, float h) { bounds_.w = w; bounds_.h = h; assignedW_ = w; assignedH_ = h; }
    void SetName(const std::string& n) { name_ = n; }
    const std::string& GetName() const { return name_; }
    UIFrame* GetParent() const { return parent_; }
    void SetParent(UIFrame* p) { parent_ = p; }
    UiRect GetBounds() const { return bounds_; }

protected:
    std::string name_;
    UIFrame* parent_ = nullptr;
    bool visible_ = true;
    bool defaultToParent_ = false;
    UiRect bounds_;
    float assignedW_ = 0, assignedH_ = 0;
    std::unordered_map<FramePoint, FramePointAssign> assignments_;
};
