#pragma once
#include "ecs/abilities/ability_aura.h"

// ===== Devotion Aura (Paladin) - increases armor =====
class CAbilityDevotionAura : public CAbilityAura {
public:
    explicit CAbilityDevotionAura(int handleId);
};

// ===== Endurance Aura (Tauren Chieftain) - move speed + attack speed =====
class CAbilityEnduranceAura : public CAbilityAura {
public:
    explicit CAbilityEnduranceAura(int handleId);
};

// ===== Unholy Aura (Death Knight) - move speed + life regen =====
class CAbilityUnholyAura : public CAbilityAura {
public:
    explicit CAbilityUnholyAura(int handleId);
};

// ===== Thorns Aura (Keeper of the Grove) - reflects melee damage =====
class CAbilityThornsAura : public CAbilityAura {
public:
    explicit CAbilityThornsAura(int handleId);
};

// ===== Brilliance Aura (Archmage) - mana regen =====
class CAbilityBrillianceAura : public CAbilityAura {
public:
    explicit CAbilityBrillianceAura(int handleId);
};

// ===== Trueshot Aura (Priestess of the Moon) - ranged damage bonus =====
class CAbilityTrueshotAura : public CAbilityAura {
public:
    explicit CAbilityTrueshotAura(int handleId);
};

// ===== Vampiric Aura (Dreadlord) - life steal on attack =====
class CAbilityVampiricAura : public CAbilityAura {
public:
    explicit CAbilityVampiricAura(int handleId);
};

// ===== Command Aura (Orc) - damage bonus =====
class CAbilityCommandAura : public CAbilityAura {
public:
    explicit CAbilityCommandAura(int handleId);
};

// ===== Regeneration Aura (Aoar, alias Aabr) - life regen =====
class CAbilityRegenerationAura : public CAbilityAura {
public:
    explicit CAbilityRegenerationAura(int handleId);
};

// ===== Mana Regeneration Aura (Aarm) - mana regen =====
class CAbilityManaRegenerationAura : public CAbilityAura {
public:
    explicit CAbilityManaRegenerationAura(int handleId);
};

// ===== Disease Cloud (Abomination) - damage aura =====
class CAbilityDiseaseCloud : public CAbilityAura {
public:
    explicit CAbilityDiseaseCloud(int handleId);
};
