#include "ability_gold_mine.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include <iostream>

CAbilityGoldMine::CAbilityGoldMine(int handleId)
    : CAbility(handleId, War3ID("Agld"), CAbilityCategory::GOLD_MINE) {
    iconShowing_ = false;
}

void CAbilityGoldMine::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
}

void CAbilityGoldMine::OnTick(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
    // Gold mine passive effects
    // Mining progress is handled by the harvest behavior
}
