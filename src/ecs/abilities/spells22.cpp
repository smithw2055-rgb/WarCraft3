#include "spells22.h"
#include "ecs/abilities/spells2.h"
#include "ecs/abilities/spells17.h"
#include "ecs/abilities/spells19.h"

std::unique_ptr<CAbility> CreateSpells22Alias(int handleId, War3ID code) {
    // Afod/Acht share the ANfd/ANht effects with their own retail orders;
    // the widened alias match in CheckCanUse routes them.
    if (code == War3ID("Afod")) {
        auto ability = std::make_unique<CAbilityFingerOfDeath>(handleId);
        ability->SetAlias(code);
        return ability;
    }
    if (code == War3ID("Acht")) {
        auto ability = std::make_unique<CAbilityHowlOfTerror>(handleId);
        ability->SetAlias(code);
        return ability;
    }
    // Generic Inferno (ANin) and base Anti-Magic Shell (Aams) variants are
    // grouped here with the other alias-only retail codes; their base
    // classes live in spells2 (Inferno) and spells19 (Anti-Magic Shell).
    if (code == War3ID("ANin")) {
        auto ability = std::make_unique<CAbilityInferno>(handleId);
        ability->SetAlias(code);
        return ability;
    }
    if (code == War3ID("Aams")) {
        auto ability = std::make_unique<CAbilityAntiMagicShell>(handleId);
        ability->SetAlias(code);
        return ability;
    }
    return nullptr;
}
