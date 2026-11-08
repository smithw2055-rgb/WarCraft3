#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 14: spawner system (Pocket Factory), death-split (Lava Spawn),
// soul burn, quilbeast, frost breath, pulverize, forked lightning.
// Disease Cloud lives in auras.h (aura-driven). Numbers validated
// against retail AbilityData.slk.

// ===== Pocket Factory (Tinker, ANsy) - goblin factory =====
// Level-1 numbers from retail (Dur 40 factory life, Cool 35, Cost 125,
// DataB ncgb goblin, DataC 12s spawn interval, UnitID nfac factory).
class CAbilityPocketFactory : public CAbilitySpellBase {
public:
    CAbilityPocketFactory(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID factoryType_ = War3ID("nfac"); // ref UnitID progression
    War3ID goblinType_ = War3ID("ncgb"); // ref DataB progression
    float factoryLife_ = 40.0f;
    float spawnInterval_ = 12.0f;
    float explodeDamage_ = 50.0f;
    float explodeRadius_ = 150.0f;
};

// ===== Summon Lava Spawn (Firelord, ANlm) - splitting summon =====
// UnitID1 nlv1; the spawn splits nlv1 -> 2x nlv2 -> 2x nlv3 (ref tiers);
// Dur 70 is the summon lifespan.
class CAbilityLavaSpawn : public CAbilitySpellBase {
public:
    CAbilityLavaSpawn(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID spawnType_ = War3ID("nlv1"); // ref UnitID progression
    War3ID splitType_ = War3ID("nlv2");
    int splitCount_ = 2;
    War3ID childType_ = War3ID("nlv3");
    int childCount_ = 2;
    float summonDuration_ = 70.0f;
};

// ===== Soul Burn (Firelord, ANsb) - burning strike =====
class CAbilitySoulBurn : public CAbilitySpellBase {
public:
    CAbilitySoulBurn(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float damage_ = 100.0f;
    float burnPerTick_ = 10.0f;
    float burnDuration_ = 8.0f;
};

// ===== Summon Quilbeast (Beastmaster, ANsq) - timed beast =====
// UnitID1 nqb1; timed like the Feral wolves (Dur 70, documented guess).
class CAbilityQuilbeast : public CAbilitySpellBase {
public:
    CAbilityQuilbeast(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID beastType_ = War3ID("nqb1"); // ref UnitID progression
    float summonDuration_ = 70.0f;
};

// ===== Frost Breath (Frost Wyrm, Afsh) - chilling cone =====
// Cone approximated as target + splash (DataA 275 damage, Area 100).
class CAbilityFrostBreath : public CAbilitySpellBase {
public:
    CAbilityFrostBreath(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float damage_ = 275.0f;
    float radius_ = 100.0f;
    float slowPercent_ = 0.5f;
    float slowDuration_ = 4.0f;
};

// ===== Pulverize (Mountain Giant, Awar) - chance splash on attack =====
// The proc hook lives in CUnit::Damage next to Cleaving Attack.
class CAbilityPulverize : public CAbilitySpellBase {
public:
    CAbilityPulverize(int handleId);
    float GetChance() const { return chance_; }
    float GetRadius() const { return radius_; }
    float GetProcChance() const override { return chance_; }
    float GetSplashRadius() const override { return radius_; }
    void SetChance(float v) { chance_ = v; }
private:
    float chance_ = 0.25f;
    float radius_ = 200.0f;
};

// ===== Forked Lightning (Sea Witch, ANfl) - short chain lightning =====
class CAbilityForkedLightning : public CAbilitySpellBase {
public:
    CAbilityForkedLightning(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 85.0f; // ref DataA level 1
    int bounces_ = 3; // ref DataB level 1
    float bounceRange_ = 500.0f;
    float damageReduction_ = 0.25f;
};
