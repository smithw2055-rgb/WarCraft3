#include "ability_revive_hero.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include <iostream>

CAbilityReviveHero::CAbilityReviveHero(int handleId)
    : CAbility(handleId, War3ID("Arev"), CAbilityCategory::HERO) {
    iconShowing_ = true;
}

bool CAbilityReviveHero::CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const {
    (void)game; (void)unit;
    if (IsDisabled()) return false;
    // The orderId encodes the hero handleId
    return orderId > 0;
}

CBehavior* CAbilityReviveHero::BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    // The orderId is the hero's handleId
    int heroHandle = orderId;
    auto* hero = game.GetUnit(heroHandle);
    if (!hero || !hero->IsDead()) {
        std::cout << "Cannot revive: hero not found or not dead" << std::endl;
        return nullptr;
    }

    // Find the queue ability on the building
    CAbility* queueAbility = caster->GetAbility(War3ID("Aque"));
    if (queueAbility) {
        queueAbility->QueueReviveHero(game, caster, hero);
    }

    return nullptr; // no behavior needed
}
