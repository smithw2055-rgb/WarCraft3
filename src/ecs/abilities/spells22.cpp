#include "spells22.h"
#include "ecs/ability.h"
#include "ecs/ability_factory.h"

std::unique_ptr<CAbility> CreateSpells22Alias(int handleId, War3ID code) {
    // Phase 22 Stage ZZ2: Delegate alias creation through AbilityFactory::CreateAbility
    // so spells22.cpp has zero cross-spell header dependencies (spells2.h, spells17.h, spells19.h).
    if (code == War3ID("Afod") || code == War3ID("Acht") ||
        code == War3ID("ANin") || code == War3ID("Aams")) {
        return AbilityFactory::CreateAbility(handleId, code);
    }
    return nullptr;
}
