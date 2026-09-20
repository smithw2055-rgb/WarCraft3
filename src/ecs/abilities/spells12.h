#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 12: detection (true sight, sentry ward, flare) plus burrow,
// cannibalize, carrion beetles and web. Rawcodes validated against
// retail AbilityData.slk (Shade detect is Atru, sentry detect Adet).

// ===== True Sight (Shade, Atru) - passive detector =====
// Sentry Ward's Adet shares this class via alias (cf. Repair pattern).
class CAbilityTrueSight : public CAbilitySpellBase {
public:
    CAbilityTrueSight(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
};

// ===== Sentry Ward (Witch Doctor, Aeye) - detector ward =====
// The ward unit (oeye, ref UnitData) gets its Adet detector attached
// explicitly: data-driven abilities are not auto-granted (ref
// AddDefaultAbilitiesToUnit stub).
class CAbilitySentryWard : public CAbilitySpellBase {
public:
    CAbilitySentryWard(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    War3ID wardType_ = War3ID("oeye");
};

// ===== Flare (Mortar, Afla) - reveal an area =====
class CAbilityFlare : public CAbilitySpellBase {
public:
    CAbilityFlare(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float duration_ = 15.0f;
    float radius_ = 400.0f;
};

// ===== Burrow (Crypt Fiend, Abur) - go underground =====
// Timed-buff approximation of the burrow toggle.
class CAbilityBurrow : public CAbilitySpellBase {
public:
    CAbilityBurrow(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 120.0f;
};

// ===== Cannibalize (Ghoul, Acan) - eat a corpse to heal =====
class CAbilityCannibalize : public CAbilitySpellBase {
public:
    CAbilityCannibalize(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float healAmount_ = 200.0f;
};

// ===== Carrion Beetles (Crypt Lord, AUcb) - beetles from a corpse =====
class CAbilityCarrionBeetles : public CAbilitySpellBase {
public:
    CAbilityCarrionBeetles(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    War3ID beetleType_ = War3ID("ucry"); // Carrion Beetle, ref UnitData
    int count_ = 2;
};

// ===== Web (Crypt Fiend, Aweb) - ground a flyer =====
class CAbilityWeb : public CAbilitySpellBase {
public:
    CAbilityWeb(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 6.0f;
};
