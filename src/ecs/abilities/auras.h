#pragma once
#include "ecs/abilities/ability_aura.h"

// ===== Devotion Aura (Paladin) - increases armor =====
class CAbilityDevotionAura : public CAbilityAura {
public:
    CAbilityDevotionAura(int handleId);
};

// ===== Endurance Aura (Tauren Chieftain) - move speed + attack speed =====
class CAbilityEnduranceAura : public CAbilityAura {
public:
    CAbilityEnduranceAura(int handleId);
};

// ===== Unholy Aura (Death Knight) - move speed + life regen =====
class CAbilityUnholyAura : public CAbilityAura {
public:
    CAbilityUnholyAura(int handleId);
};

// ===== Thorns Aura (Keeper of the Grove) - reflects melee damage =====
class CAbilityThornsAura : public CAbilityAura {
public:
    CAbilityThornsAura(int handleId);

protected:
    void ApplyAura(CSimulation& game, CUnit* caster, CUnit* target) override;
    void RemoveAura(CSimulation& game, CUnit* caster, CUnit* target) override;
};

// ===== Brilliance Aura (Archmage) - mana regen =====
class CAbilityBrillianceAura : public CAbilityAura {
public:
    CAbilityBrillianceAura(int handleId);
};

// ===== Trueshot Aura (Priestess of the Moon) - ranged damage bonus =====
class CAbilityTrueshotAura : public CAbilityAura {
public:
    CAbilityTrueshotAura(int handleId);
};

// ===== Vampiric Aura (Dreadlord) - life steal on attack =====
class CAbilityVampiricAura : public CAbilityAura {
public:
    CAbilityVampiricAura(int handleId);

protected:
    void ApplyAura(CSimulation& game, CUnit* caster, CUnit* target) override;
    void RemoveAura(CSimulation& game, CUnit* caster, CUnit* target) override;
};

// ===== Command Aura (Orc) - damage bonus =====
class CAbilityCommandAura : public CAbilityAura {
public:
    CAbilityCommandAura(int handleId);
};

// ===== Regeneration Aura (Aoar, alias Aabr) - life regen =====
// Ref template AURA_STATS / MAXHPGEN from DataA. Level-1 amount is a
// documented guess; the percentage flag in DataB is not read yet.
class CAbilityRegenerationAura : public CAbilityAura {
public:
    CAbilityRegenerationAura(int handleId);
};

// ===== Mana Regeneration Aura (Aarm) - mana regen =====
// Ref template AURA_STATS / MAXMPGEN from DataA.
class CAbilityManaRegenerationAura : public CAbilityAura {
public:
    CAbilityManaRegenerationAura(int handleId);
};

// ===== Disease Cloud (Abomination) - damage aura =====
class CAbilityDiseaseCloud : public CAbilityAura {
public:
    CAbilityDiseaseCloud(int handleId);

protected:
    void ApplyAura(CSimulation& game, CUnit* caster, CUnit* target) override;
    void RemoveAura(CSimulation& game, CUnit* caster, CUnit* target) override;
};
