#pragma once
#include "ecs/abilities/ability_aura.h"

// Batch 24: item stat passives from warsmash abilityBehaviors/auras.json
// (PASSIVE_STATS). Numbers are the classic level-1 DataA values.

// Self-only stat buff keyed by the ability code, so it shares the
// non-stacking aura table (strongest bonus per slot wins).
class CAbilitySelfStatPassive : public CAbility {
public:
    CAbilitySelfStatPassive(int handleId, War3ID code, CAuraEffectType type,
                            float value, bool percent);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;

private:
    CAuraEffectType type_;
    float value_;
    bool percent_;
};

// Gloves of Haste (AIas) - +15% attack speed.
class CAbilityItemAttackSpeed : public CAbilitySelfStatPassive {
public:
    explicit CAbilityItemAttackSpeed(int handleId);
};

// Sobi Mask (AIrm) - ref stores DataA as a mana-regen percentage.
// This engine's mana-regen slot is flat, so 0.50 is that field applied
// as mana per second.
class CAbilityItemManaRegen : public CAbilitySelfStatPassive {
public:
    explicit CAbilityItemManaRegen(int handleId);
};

// Boots of Speed (AIms) - +50 move speed, flat.
class CAbilityItemMoveSpeed : public CAbilitySelfStatPassive {
public:
    explicit CAbilityItemMoveSpeed(int handleId);
};

// Mask of Death (AIva) - 15% life steal on the carrier.
class CAbilityItemVampirism : public CAbility {
public:
    explicit CAbilityItemVampirism(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
};
