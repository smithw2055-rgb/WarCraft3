#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 21: Stasis Trap (Asta) and Tornado (Sea Witch, ANto). Both ride the
// CSimulation timed-hazard list instead of ward/summon unit types (no fake
// type ids): Stasis is a stationary mine that stuns enemy ground on
// proximity and is consumed; Tornadoes are wandering hazards that toss
// nearby enemies (Cyclone buff + spell damage) every second. Codes verified
// against Units\AbilityData.slk.

struct CAbilityDefFull;

// ===== Stasis Trap (Asta) - proximity stun mine =====
// Retail targets: ground, neutral, enemy (ref targs1).
class CAbilityStasisTrap : public CAbilitySpellBase {
public:
    CAbilityStasisTrap(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float radius_ = 200.0f; // trigger/stun radius, documented guess
    float stunDuration_ = 8.0f; // ref stun length, documented guess
    float lifetime_ = 120.0f; // trap lifetime seconds, documented guess
};

// ===== Tornado (ANto) - wandering tossers =====
class CAbilityTornado : public CAbilitySpellBase {
public:
    CAbilityTornado(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    int count_ = 3; // tornadoes per cast, documented guess
    float radius_ = 150.0f; // toss radius, documented guess
    float damage_ = 30.0f; // spell damage per toss tick, documented guess
    float duration_ = 20.0f; // wander lifetime seconds, documented guess
    float speed_ = 150.0f; // wander speed, documented guess
};
