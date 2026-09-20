#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 14: spawner system (Pocket Factory), death-split (Lava Spawn),
// soul burn, quilbeast, frost breath, pulverize, forked lightning.
// Disease Cloud lives in auras.h (aura-driven). Numbers validated
// against retail AbilityData.slk.

// ===== Pocket Factory (Tinker, ANsy) - goblin factory =====
// Level-1 numbers from retail (Dur 40 factory life, DataB 12s spawn
// interval, DataA ncgb goblin, DataI nfac factory).
class CAbilityPocketFactory : public CAbilitySpellBase {
public:
    CAbilityPocketFactory(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float factoryLife_ = 40.0f;
    float spawnInterval_ = 12.0f;
    float explodeDamage_ = 50.0f;
    float explodeRadius_ = 150.0f;
};

// ===== Summon Lava Spawn (Firelord, ANlm) - splitting summon =====
// DataI1 nlv1; the spawn splits nlv1 -> 2x nlv2 -> 2x nlv3 (ref tiers).
class CAbilityLavaSpawn : public CAbilitySpellBase {
public:
    CAbilityLavaSpawn(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float summonDuration_ = 60.0f;
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
// DataI1 nqb1; timed like the Feral wolves (documented guess).
class CAbilityQuilbeast : public CAbilitySpellBase {
public:
    CAbilityQuilbeast(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    War3ID beastType_ = War3ID("nqb1");
    float summonDuration_ = 60.0f;
};

// ===== Frost Breath (Frost Wyrm, Afsh) - chilling cone =====
// Cone approximated as target + splash (ref DataA 250 damage band).
class CAbilityFrostBreath : public CAbilitySpellBase {
public:
    CAbilityFrostBreath(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float damage_ = 100.0f;
    float radius_ = 200.0f;
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
private:
    float damage_ = 60.0f;
    int bounces_ = 2;
    float bounceRange_ = 500.0f;
    float damageReduction_ = 0.25f;
};
