#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 19: Banish (Blood Mage, AHbn) and Anti-Magic Shell (Banshee, Aam2).
// Both attach the buffs introduced for the ethereal / magic-immune damage
// rules (CBuffEthereal / CBuffMagicImmune) through the standard
// AbilityBuilder single-target attach. Constructor numbers are documented
// retail reads where known, guesses otherwise; ApplyEffectData re-reads
// Duration from the ability def so level-ups apply.

struct CAbilityDefFull;

// ===== Banish (AHbn) - ethereal curse =====
// Retail: unit target, the victim goes ethereal (physical immune, bonus
// magic/spell damage taken) for Dur.
class CAbilityBanish : public CAbilitySpellBase {
public:
    CAbilityBanish(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float duration_ = 12.0f; // ref Dur, documented guess
};

// ===== Anti-Magic Shell (Aam2) - magic immunity =====
// Retail: friendly unit target, SPELL/MAGIC immunity for Dur.
class CAbilityAntiMagicShell : public CAbilitySpellBase {
public:
    CAbilityAntiMagicShell(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float duration_ = 60.0f; // ref Dur, documented guess
};
