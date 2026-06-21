#include "ability_return_resources.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"

CAbilityReturnResources::CAbilityReturnResources(int handleId)
    : CAbility(handleId, War3ID("Aret"), CAbilityCategory::HARVEST) {
    iconShowing_ = false;
}

void CAbilityReturnResources::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
}
