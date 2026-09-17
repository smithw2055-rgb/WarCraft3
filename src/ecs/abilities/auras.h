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
    void ApplyAura(CSimulation& game, CUnit* target) override;
    void RemoveAura(CSimulation& game, CUnit* target) override;
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
    void ApplyAura(CSimulation& game, CUnit* target) override;
    void RemoveAura(CSimulation& game, CUnit* target) override;
};

// ===== Command Aura (Orc) - damage bonus =====
class CAbilityCommandAura : public CAbilityAura {
public:
    CAbilityCommandAura(int handleId);
};
