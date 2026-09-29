#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 23: Chain Dispel (Ache) and Taunt (Mountain Giant, ANta). Codes
// and target shapes verified against Units\AbilityData.slk. Chain Dispel
// reuses the bounce iterator (allies shed debuffs, enemies lose buffs);
// Taunt issues logged ATTACK orders on enemies around the caster.

struct CAbilityDefFull;

// ===== Chain Dispel (Ache) - jumping cleanse =====
// Retail targets: friend/self/enemy single units (ref targs1); no
// dedicated unit order string exists in ref, so it answers the chain
// order shared with the item version (documented guess).
class CAbilityChainDispel : public CAbilitySpellBase {
public:
    CAbilityChainDispel(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    int bounces_ = 4; // ref DataA jump count, documented guess
    float range_ = 500.0f; // jump range, documented guess
};

// ===== Taunt (ANta) - forced attack =====
// No-target blast around the caster: nearby enemies attack it.
class CAbilityTaunt : public CAbilitySpellBase {
public:
    CAbilityTaunt(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float radius_ = 500.0f; // ref Area, documented guess
};
