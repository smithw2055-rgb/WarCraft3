#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 26: spell shield, consumable hero items, form toggles, invulnerability,
// and the moonstone clock. Numbers are classic level-1 Data fields.

struct CAbilityDefFull;

// Spell Shield (ANss). Blocks one hostile spell, then rearms after cooldown.
class CAbilitySpellShieldPassive : public CAbility {
public:
    explicit CAbilitySpellShieldPassive(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    void OnTick(CSimulation& game, CUnit* unit) override;
    float GetCooldown() const { return cooldown_; }

private:
    void Arm(CSimulation& game, CUnit* unit);
    float cooldown_ = 40.0f;
    float remaining_ = 0.0f;
    bool armed_ = false;
};

// Spell Shield area (ANse) - allies in the area gain a timed shield.
class CAbilitySpellShieldArea : public CAbilitySpellBase {
public:
    CAbilitySpellShieldArea(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float radius_ = 300.0f;
    float duration_ = 15.0f;
};

// Tome-style permanent attributes. DataA agility, DataB intelligence, DataC strength.
class CAbilityItemPermanentStats : public CAbilitySpellBase {
public:
    CAbilityItemPermanentStats(int handleId, War3ID code, int agility, int intelligence, int strength);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    int agility_;
    int intelligence_;
    int strength_;
};

class CAbilityItemPermanentLife : public CAbilitySpellBase {
public:
    explicit CAbilityItemPermanentLife(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    int life_ = 50;
};

class CAbilityItemExperience : public CAbilitySpellBase {
public:
    explicit CAbilityItemExperience(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    int xp_ = 100;
};

class CAbilityItemLevelGain : public CAbilitySpellBase {
public:
    explicit CAbilityItemLevelGain(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    int levels_ = 1;
};

class CAbilityItemFigurine : public CAbilitySpellBase {
public:
    explicit CAbilityItemFigurine(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID summonId_ = War3ID("nwwf");
    int count_ = 1;
    float duration_ = 60.0f;
};

// Invulnerable (Avul) while the ability is present.
class CAbilityPermanentInvulnerable : public CAbility {
public:
    explicit CAbilityPermanentInvulnerable(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
};

// Moonstone (AIct). DataA hour, DataB minute.
class CAbilityChangeTimeOfDay : public CAbilitySpellBase {
public:
    explicit CAbilityChangeTimeOfDay(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    int hour_ = 0;
    int minute_ = 0;
};

// Corporeal / ethereal / avenger form toggles. Alternate unit comes from UnitID.
class CAbilityToggleForm : public CAbilitySpellBase {
public:
    CAbilityToggleForm(int handleId, War3ID code);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID formId_;
};

// Chaos (Acha) morphs on add and restores on remove (uses SetPassiveSpell(true)).
class CAbilityChaos : public CAbilitySpellBase {
public:
    explicit CAbilityChaos(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID formId_;
};
