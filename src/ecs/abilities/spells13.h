#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 13: alternate-form morphs (Metamorphosis, Bear/Raven/Robo),
// Stone Form, Avatar, Siphon Mana, Big Bad Voodoo. Morph targets and
// level-1 numbers validated against retail AbilityData.slk (DataA1
// alternate-form ids, Dur/Cool/Cost columns).

// ===== Metamorphosis (Demon Hunter, AEme) - demon form =====
// Timed approximation (60s); alternate form Edem (ref DataA1).
class CAbilityMetamorphosis : public CAbilitySpellBase {
public:
    CAbilityMetamorphosis(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 60.0f;
};

// ===== Bear Form (Druid of the Claw, Abrf) - bear form =====
// Toggle approximated as a timed morph; form edcm (ref UnitData).
class CAbilityBearForm : public CAbilitySpellBase {
public:
    CAbilityBearForm(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 60.0f;
};

// ===== Raven Form (Druid of the Talon, Arav) - storm crow form =====
// Form edot (ref DataA1); same toggle approximation.
class CAbilityRavenForm : public CAbilitySpellBase {
public:
    CAbilityRavenForm(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 60.0f;
};

// ===== Robo-Goblin (Tinker, ANrg) - robo form =====
// Form Ntin (ref DataA1); same toggle approximation.
class CAbilityRoboGoblin : public CAbilitySpellBase {
public:
    CAbilityRoboGoblin(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 60.0f;
};

// ===== Stone Form (Gargoyle, Astn) - living statue =====
class CAbilityStoneForm : public CAbilitySpellBase {
public:
    CAbilityStoneForm(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 30.0f;
};

// ===== Avatar (Mountain King, AHav) - god-rage =====
// Level-1 numbers from retail (Dur 60, DataB 500 bonus life,
// DataC 20 bonus damage).
class CAbilityAvatar : public CAbilitySpellBase {
public:
    CAbilityAvatar(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 60.0f;
    int bonusDamage_ = 20;
    int bonusLife_ = 500;
};

// ===== Siphon Mana (Blood Mage, AHdr) - mana drain channel =====
// Level-1 numbers from retail (Dur 6, DataB 15 per tick).
class CAbilitySiphonMana : public CAbilitySpellBase {
public:
    CAbilitySiphonMana(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 6.0f;
    float manaPerTick_ = 15.0f;
};

// ===== Big Bad Voodoo (Witch Doctor, AOvd) - mass invulnerability =====
// The channel is folded into instant shields (ref Dur 30, Area 800).
class CAbilityBigBadVoodoo : public CAbilitySpellBase {
public:
    CAbilityBigBadVoodoo(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 30.0f;
    float radius_ = 800.0f;
};
