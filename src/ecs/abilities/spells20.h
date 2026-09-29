#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 20: Possession (Banshee, Apos) and Stampede (Beastmaster, ANst).
// Codes verified against Units\AbilityData.slk (earlier batches had two
// wrong codes, ABan->AHbn and AEtr->AEtq, fixed alongside). Possession
// mirrors Charm's ownership branch plus the retail sacrifice: the banshee
// is removed when the victim converts. Stampede tramples a corridor from
// the caster toward the target point (damage + brief slow-down via stun).

struct CAbilityDefFull;

// ===== Possession (Apos) - convert and sacrifice =====
// Retail targets: ground, non-hero, enemy, organic, neutral (ref targs1).
class CAbilityPossession : public CAbilitySpellBase {
public:
    CAbilityPossession(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    int maxLevel_ = 99; // retail has no level cap beyond non-hero (ref targs1)
};

// ===== Stampede (ANst) - trampling corridor =====
// Retail targets: ground, structures, debris, enemy, neutral (ref targs1).
class CAbilityStampede : public CAbilitySpellBase {
public:
    CAbilityStampede(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 120.0f; // ref DataA, documented guess
    float range_ = 800.0f; // corridor length, documented guess
    float width_ = 200.0f; // corridor width, documented guess
    float slowDuration_ = 2.0f; // trample slow-down, documented guess
};
