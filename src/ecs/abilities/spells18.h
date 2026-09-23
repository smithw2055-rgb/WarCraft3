#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 18 (abilities): closes the ref-skills gap - Whirlwind (Bladestorm),
// Immolation, Force of Nature, Owl Scout, War Eagle (Hawk), Goblin Factory,
// Kaboom (Sapper explode) and Phoenix Fire. Numbers follow the ref field
// mappings (DataA-I/Area/Duration/UnitID); constructor defaults are
// documented retail reads or guesses where the SLK value is unknown.

struct CAbilityDefFull;

// ===== Whirlwind (Bladestorm, AOww) - spinning AoE =====
// Ref DataA damage per second, Area of effect, 1s damage interval;
// the ref magic-immune/attack-lock side effects need the missing
// StateMod stack and are not modeled.
class CAbilityWhirlwind : public CAbilitySpellBase {
public:
    CAbilityWhirlwind(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damagePerSecond_ = 110.0f; // ref DataA level 1 (documented read)
    float radius_ = 250.0f; // ref Area, documented guess
    float duration_ = 6.0f; // ref Dur1, documented guess
};

// ===== Immolation (Demon Hunter, AEim) - burning mana aura =====
// Ref DataA damage per interval, DataB mana drained per second, DataC
// buffer mana, Area, Dur; the toggle is approximated as a timed buff
// (same treatment as Mana Shield).
class CAbilityImmolation : public CAbilitySpellBase {
public:
    CAbilityImmolation(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damagePerTick_ = 10.0f; // ref DataA level 1 (documented read)
    float manaDrain_ = 7.0f; // ref DataB level 1 (documented read)
    float radius_ = 250.0f; // ref Area, documented guess
    float duration_ = 60.0f; // toggle approximated as timed
};

// ===== Force of Nature (Keeper, AEfn) - trees rise as treants =====
// Ref DataA treant count, UnitID treant type, Area. The retail
// must-target-a-tree check needs destructable targeting data, so the
// summons land around the target point (documented simplification).
class CAbilityForceOfNature : public CAbilitySpellBase {
public:
    CAbilityForceOfNature(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    int count_ = 2; // ref DataA level 1
    War3ID treantType_ = War3ID("etrp"); // ref UnitID (documented read)
    float summonDuration_ = 120.0f; // ref Dur1, documented guess
};

// ===== Owl Scout (Priestess, AEst) - scouting owl =====
// Retail AEst is the owl (Starfall is AEsf); ref extends the water
// elemental summon with the scout order id.
class CAbilityOwlScout : public CAbilitySpellBase {
public:
    CAbilityOwlScout(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID owlType_ = War3ID("nef1"); // ref UnitID (documented read)
    float summonDuration_ = 90.0f; // ref Dur1 level 2 read
};

// ===== War Eagle (Beastmaster hawk, ANsw) - flying scout =====
class CAbilitySummonHawk : public CAbilitySpellBase {
public:
    CAbilitySummonHawk(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID hawkType_ = War3ID("nehw"); // ref UnitID (documented read)
    float summonDuration_ = 70.0f; // summon without timed life in ref
};

// ===== Goblin Factory (Tinker, ANfy) - stationary spawner =====
// Older sibling of the Pocket Factory (ANsy): raises a factory that
// builds explosive clockwerk goblins on an interval.
class CAbilityFactory : public CAbilitySpellBase {
public:
    CAbilityFactory(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID factoryType_ = War3ID("nfac"); // ref UnitID progression
    War3ID goblinType_ = War3ID("ncgb"); // ref DataB progression
    float factoryLife_ = 60.0f;
    float spawnInterval_ = 8.0f;
    float explodeDamage_ = 75.0f;
    float explodeRadius_ = 200.0f;
};

// ===== Kaboom (Goblin Sapper, Asds) - suicidal detonation =====
// Ref DataA full-damage radius, DataB full damage, DataC partial radius,
// DataD partial damage, DataE building factor, DataF explodes-on-death.
class CAbilityKaboom : public CAbilitySpellBase {
public:
    CAbilityKaboom(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
    void OnDeath(CSimulation& game, CUnit* unit) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    void Explode(CSimulation& game, CUnit* caster);
    bool armed_ = false;
    float fullRadius_ = 150.0f; // ref DataA (documented read)
    float fullDamage_ = 300.0f; // ref DataB (documented read)
    float partialRadius_ = 250.0f; // ref DataC (documented read)
    float partialDamage_ = 200.0f; // ref DataD (documented read)
    float buildingFactor_ = 1.5f; // ref DataE (documented read)
    bool explodesOnDeath_ = true; // ref DataF
};

// ===== Phoenix Fire (Apxf) - living artillery aura =====
// Passive on the phoenix: scorches nearby enemies every second (ref
// DataA initial damage, DataB damage per second, Area).
class CAbilityPhoenixFire : public CAbilitySpellBase {
public:
    CAbilityPhoenixFire(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damagePerSecond_ = 10.0f; // ref DataB (documented read)
    float radius_ = 250.0f; // ref Area (documented read)
};
