#pragma once
#include "ecs/ability.h"

// CAbilityGenericDoNothing - fallback for unknown/data-only ability rawcodes.
// Ref: Java CAbilityGenericDoNothing (AbstractGenericAliasedAbility).
// Rejects all orders as non-active, consumes queue checks, never starts
// a behavior (returns nullptr so unit polls next order).
class CAbilityGenericDoNothing : public CAbility {
public:
    CAbilityGenericDoNothing(int handleId, War3ID code)
        : CAbility(handleId, code, CAbilityCategory::SPELL) {
        SetPhysical(true);
        SetPermanent(false);
        SetIconShowing(true);
        SetGenericPlaceholder(true);
    }

    bool CheckCanUse(CSimulation&, CUnit*, int) const override { return false; }
    bool CheckBeforeQueue(CSimulation&, CUnit*, int, CWidget*) override { return false; }
    bool CheckBeforeQueue(CSimulation&, CUnit*, int, float, float) override { return false; }
    bool CheckBeforeQueueNoTarget(CSimulation&, CUnit*, int) override { return false; }
};
