#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 11: stealth system (Wind Walk, Invisibility, Permanent
// Invisibility, Shadowmeld, Dust reveal) plus Berserk, Summon Bear
// and Battle Roar. Rawcodes validated against retail AbilityData.slk
// (Shadow Meld is Ashm; Summon Bear raises ngz1 at level 1).

// ===== Wind Walk (Blademaster, AOwk) - stealth with backstab =====
class CAbilityWindWalk : public CAbilitySpellBase {
public:
    CAbilityWindWalk(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 30.0f;
    float backstabDamage_ = 40.0f;
};

// ===== Invisibility (Sorceress, Aivs) - stealth without backstab =====
class CAbilityInvisibility : public CAbilitySpellBase {
public:
    CAbilityInvisibility(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 20.0f;
};

// ===== Permanent Invisibility (Apiv) - passive stealth flag =====
class CAbilityPermanentInvisibility : public CAbilitySpellBase {
public:
    CAbilityPermanentInvisibility(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
};

// ===== Shadowmeld (Ashm) - stealth while stationary =====
// Night-only restriction is data-side; movement breaks the meld here.
class CAbilityShadowmeld : public CAbilitySpellBase {
public:
    CAbilityShadowmeld(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    void OnTick(CSimulation& game, CUnit* unit) override;
private:
    float lastX_ = 0.0f;
    float lastY_ = 0.0f;
    bool tracking_ = false;
};

// ===== Dust of Appearance (dust -> AItb) - reveal an area =====
// Revealed units carry the Faerie Fire marker, which IsInvisibleTo
// already honors.
class CAbilityDustOfAppearance : public CAbilitySpellBase {
public:
    CAbilityDustOfAppearance(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float duration_ = 12.0f;
    float radius_ = 300.0f;
};

// ===== Berserk (Absk) - faster attacks, weaker skin =====
class CAbilityBerserk : public CAbilitySpellBase {
public:
    CAbilityBerserk(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 20.0f;
    float attackSpeedMul_ = 1.4f;
    float damageTakenMul_ = 1.25f;
};

// ===== Summon Bear (Beastmaster grizzly, ANsg) - permanent bear =====
class CAbilitySummonBear : public CAbilitySpellBase {
public:
    CAbilitySummonBear(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID bearType_ = War3ID("ngz1");
};

// ===== Battle Roar (Beastmaster, ANbr) - rally nearby allies =====
class CAbilityBattleRoar : public CAbilitySpellBase {
public:
    CAbilityBattleRoar(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float duration_ = 30.0f;
    int bonusDamage_ = 6;
    float radius_ = 650.0f; // ref Area1
};
