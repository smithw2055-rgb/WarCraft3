#pragma once

#include <cstdint>

// Matches Java CItemTypeJass ordinal values used by ConvertItemType.
enum class CItemClass : uint8_t {
    PERMANENT,
    CHARGED,
    POWERUP,
    ARTIFACT,
    PURCHASABLE,
    CAMPAIGN,
    MISCELLANEOUS,
    UNKNOWN,
    ANY
};
