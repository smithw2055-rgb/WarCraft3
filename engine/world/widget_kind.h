#pragma once
// ============================================================================
// Engine World: WidgetKind — O(1) RTTI-Free Entity Classification Tag
// ============================================================================
#include <cstdint>

namespace engine::world {

enum class WidgetKind : uint8_t {
    Base = 0,
    Unit = 1,
    Item = 2,
    Destructable = 3
};

} // namespace engine::world
