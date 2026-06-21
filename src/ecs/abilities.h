#pragma once
// This file is kept for backward compatibility.
// The new ability system is in ecs/ability.h and ecs/abilities/
#include "ecs/ability.h"
#include <string>

// Legacy ability definition struct (kept for backward compat)
struct CAbilityDef {
    War3ID id;
    CAbilityCategory category;
    std::string name;
    int level = 0;
    bool active = true;
};
