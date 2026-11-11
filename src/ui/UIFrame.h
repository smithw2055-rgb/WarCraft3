#pragma once
#include "UiRenderer.h"
#include "UiTypes.h"
#include "engine/ui/ui_frame_rtti_free.h"
#include <cstdint>
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

    // Stage D1 Modernization: Layout dirty-flag telemetry
    uint64_t GetLayoutPassCount() const { return layoutPassCount_; }
    void IncrementLayoutPassCount() { layoutPassCount_++; }
    void ResetLayoutPassCount() { layoutPassCount_ = 0; }

private:
    int lastLayoutW_ = -1;
    int lastLayoutH_ = -1;
    uint64_t layoutPassCount_ = 0;
};

struct FramePointAssign {
    FramePoint myPoint;
    std::string targetName;
    FramePoint targetPoint;
    float x = 0, y = 0;
};

class UIFrame {
public:
    static constexpr engine::ui::FrameKind kStaticFrameKind = engine::ui::FrameKind::Base;

    UIFrame(const std::string& name,
            UIFrame* parent = nullptr,
            engine::ui::FrameKind leafKind = engine::ui::FrameKind::Base);
    virtual ~UIFrame() = default;

    virtual void Render(UiRenderer& r);
    virtual void PositionBounds(GameUI& gui);

    // Stage D1 Modernization: Godot Control-style SceneTree child management & Z-index
    virtual void AddChild(UIFrame* child);
    virtual void RemoveChild(UIFrame* child);
    const std::vector<UIFrame*>& GetChildren() const { return children_; }

    int GetZOrder() const { return zOrder_; }
    void SetZOrder(int z);

    bool IsLayoutDirty() const { return layoutDirty_; }
    void MarkLayoutDirty();
    void ClearLayoutDirty() { layoutDirty_ = false; }
    uint64_t GetLayoutComputeCount() const { return layoutComputeCount_; }

    void SetAllPoints();
    void SetAllPoints(float inset);
    void SetWidth(float w) {
        if (assignedW_ != w || bounds_.w != w) {
            assignedW_ = w;
            bounds_.w = w;
            MarkLayoutDirty();
        }
    }
    void SetHeight(float h) {
        if (assignedH_ != h || bounds_.h != h) {
            assignedH_ = h;
            bounds_.h = h;
            MarkLayoutDirty();
        }
    }
    float GetWidth() const { return bounds_.w; }
    float GetHeight() const { return bounds_.h; }
    float GetFrameX(FramePoint pt) const;
    float GetFrameY(FramePoint pt) const;
    void SetFrameX(FramePoint pt, float x);
    void SetFrameY(FramePoint pt, float y);
    void AddSetPoint(FramePoint myPt, const std::string& other, FramePoint otherPt, float ox, float oy);
    void AddAnchor(FramePoint pt, float ox, float oy);
    void ClearAllPoints() {
        assignments_.clear();
        MarkLayoutDirty();
    }

    void SetVisible(bool v) { visible_ = v; }
    bool IsVisible() const { return visible_; }
    // FDF frames built without any SetPoint/anchor/size-stretch assignment
    // default to their parent's top-left (ref Java, where position-less
    // children stay at the parent origin instead of the viewport origin).
    // Explicitly placed code frames never set this.
    void SetDefaultToParent(bool v) {
        if (defaultToParent_ != v) {
            defaultToParent_ = v;
            MarkLayoutDirty();
        }
    }

    virtual UIFrame* HitTest(float sx, float sy);
    virtual UIFrame* OnMouseDown(float sx, float sy, int btn);
    virtual UIFrame* OnMouseUp(float sx, float sy, int btn);

    void SetPosition(float x, float y) {
        if (bounds_.x != x || bounds_.y != y) {
            bounds_.x = x;
            bounds_.y = y;
            MarkLayoutDirty();
        }
    }
    void SetSize(float w, float h) {
        if (bounds_.w != w || bounds_.h != h || assignedW_ != w || assignedH_ != h) {
            bounds_.w = w;
            bounds_.h = h;
            assignedW_ = w;
            assignedH_ = h;
            MarkLayoutDirty();
        }
    }
    void SetName(const std::string& n) { name_ = n; }
    const std::string& GetName() const { return name_; }
    UIFrame* GetParent() const { return parent_; }
    void SetParent(UIFrame* p) {
        if (parent_ != p) {
            parent_ = p;
            MarkLayoutDirty();
        }
    }
    UiRect GetBounds() const { return bounds_; }

    uint32_t GetFrameKindMask() const noexcept { return frameKindMask_; }
    template <typename T>
    T* As() noexcept { return engine::ui::DynamicCastFrame<T>(this); }
    template <typename T>
    const T* As() const noexcept { return engine::ui::DynamicCastFrame<const T>(this); }

protected:
    void SortChildrenByZOrderIfNeeded();
    void ComputeSelfPositionBounds(GameUI& gui);

    std::string name_;
    UIFrame* parent_ = nullptr;
    std::vector<UIFrame*> children_;
    int zOrder_ = 0;
    bool childrenZOrderDirty_ = false;
    bool layoutDirty_ = true;
    uint64_t layoutComputeCount_ = 0;
    bool visible_ = true;
    bool defaultToParent_ = false;
    UiRect bounds_;
    float assignedW_ = 0, assignedH_ = 0;
    std::unordered_map<FramePoint, FramePointAssign> assignments_;
    uint32_t frameKindMask_ = engine::ui::ComputeFrameAncestryMask(engine::ui::FrameKind::Base);
};
