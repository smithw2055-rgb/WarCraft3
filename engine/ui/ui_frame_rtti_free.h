#pragma once
// ============================================================================
// Engine UI: RTTI-Free FrameKind Hierarchy Bitmask (O(1) UIFrame::As<T>())
// ============================================================================
#include <cstdint>
#include <type_traits>

namespace engine::ui {

enum class FrameKind : uint32_t {
    Base        = 1u << 0,
    Simple      = 1u << 1,
    Backdrop    = 1u << 2,
    Button      = 1u << 3,
    Text        = 1u << 4,
    Texture     = 1u << 5,
    CheckBox    = 1u << 6,
    EditBox     = 1u << 7,
    ListBox     = 1u << 8,
    Menu        = 1u << 9,
    PopupMenu   = 1u << 10,
    ScrollBar   = 1u << 11,
    Sprite      = 1u << 12,
    StatusBar   = 1u << 13,
    Control     = 1u << 14
};

constexpr uint32_t FrameBit(FrameKind k) noexcept {
    return static_cast<uint32_t>(k);
}

// Compute full inheritance mask so e.g. PopupMenuFrame matches PopupMenu, Button, Simple, and Base!
constexpr uint32_t ComputeFrameAncestryMask(FrameKind leafKind) noexcept {
    switch (leafKind) {
        case FrameKind::Base:
            return FrameBit(FrameKind::Base);
        case FrameKind::Simple:
            return FrameBit(FrameKind::Base) | FrameBit(FrameKind::Simple);
        case FrameKind::Backdrop:
            return FrameBit(FrameKind::Base) | FrameBit(FrameKind::Simple) | FrameBit(FrameKind::Backdrop);
        case FrameKind::Button:
            return FrameBit(FrameKind::Base) | FrameBit(FrameKind::Simple) | FrameBit(FrameKind::Button);
        case FrameKind::CheckBox:
            return FrameBit(FrameKind::Base) | FrameBit(FrameKind::Simple) | FrameBit(FrameKind::Button) | FrameBit(FrameKind::CheckBox);
        case FrameKind::PopupMenu:
            return FrameBit(FrameKind::Base) | FrameBit(FrameKind::Simple) | FrameBit(FrameKind::Button) | FrameBit(FrameKind::PopupMenu);
        case FrameKind::Control:
            return FrameBit(FrameKind::Base) | FrameBit(FrameKind::Simple) | FrameBit(FrameKind::Control);
        case FrameKind::EditBox:
            return FrameBit(FrameKind::Base) | FrameBit(FrameKind::Simple) | FrameBit(FrameKind::EditBox);
        case FrameKind::ListBox:
            return FrameBit(FrameKind::Base) | FrameBit(FrameKind::Simple) | FrameBit(FrameKind::ListBox);
        case FrameKind::Menu:
            return FrameBit(FrameKind::Base) | FrameBit(FrameKind::Simple) | FrameBit(FrameKind::ListBox) | FrameBit(FrameKind::Menu);
        case FrameKind::ScrollBar:
            return FrameBit(FrameKind::Base) | FrameBit(FrameKind::Simple) | FrameBit(FrameKind::ScrollBar);
        case FrameKind::Sprite:
            return FrameBit(FrameKind::Base) | FrameBit(FrameKind::Simple) | FrameBit(FrameKind::Sprite);
        case FrameKind::StatusBar:
            return FrameBit(FrameKind::Base) | FrameBit(FrameKind::Simple) | FrameBit(FrameKind::StatusBar);
        case FrameKind::Text:
            return FrameBit(FrameKind::Base) | FrameBit(FrameKind::Text);
        case FrameKind::Texture:
            return FrameBit(FrameKind::Base) | FrameBit(FrameKind::Texture);
    }
    return FrameBit(FrameKind::Base);
}

template <typename TargetT, typename BaseFrameT>
inline TargetT* DynamicCastFrame(BaseFrameT* frame) noexcept {
    if (!frame) return nullptr;
    using CleanTarget = std::remove_cv_t<TargetT>;
    if ((frame->GetFrameKindMask() & FrameBit(CleanTarget::kStaticFrameKind)) != 0u) {
        return static_cast<TargetT*>(frame);
    }
    return nullptr;
}

} // namespace engine::ui
