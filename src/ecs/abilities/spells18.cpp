#include "spells18.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "ecs/abilities/ability_builder.h"
#include "data/abilitydata.h"
#include <cmath>

// ===== Whirlwind =====
CAbilityWhirlwind::CAbilityWhirlwind(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOww"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(8.0f);
    SetManaCost(75);
}

void CAbilityWhirlwind::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto a single AbilityBuilder buff attach.
    if (!caster) return;
    ABActionAddBuffTarget spin(
        [duration = duration_, dps = damagePerSecond_, radius = radius_]() {
            return std::make_unique<CBuffWhirlwind>(duration, dps, radius);
        });
    spin.Run(game, caster, caster, false, 0, 0);
}

void CAbilityWhirlwind::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damagePerSecond_ = def.DataFloatAt(level, 0, damagePerSecond_);
    radius_ = def.AreaAt(level);
    duration_ = def.DurationAt(level);
}

// ===== Immolation =====
CAbilityImmolation::CAbilityImmolation(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEim"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(5.0f);
    SetManaCost(10);
}

void CAbilityImmolation::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto a single AbilityBuilder buff attach.
    if (!caster) return;
    ABActionAddBuffTarget immolate(
        [duration = duration_, tick = damagePerTick_, drain = manaDrain_, radius = radius_]() {
            return std::make_unique<CBuffImmolation>(duration, tick, drain, radius);
        });
    immolate.Run(game, caster, caster, false, 0, 0);
}

void CAbilityImmolation::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damagePerTick_ = def.DataFloatAt(level, 0, damagePerTick_);
    manaDrain_ = def.DataFloatAt(level, 1, manaDrain_);
    radius_ = def.AreaAt(level);
    duration_ = def.DurationAt(level);
}

// ===== Force of Nature =====
CAbilityForceOfNature::CAbilityForceOfNature(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEfn"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(600.0f);
    SetCooldown(11.0f);
    SetManaCost(75);
}

void CAbilityForceOfNature::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    // Migrated onto an AbilityBuilder summon ring (treants on a 64-radius
    // ring, each facing outward).
    if (!caster) return;
    ABActionSummonRing nature(treantType_, War3ID("AEfn"), count_, summonDuration_);
    nature.Run(game, caster, nullptr, true, x, y);
}

void CAbilityForceOfNature::ApplyEffectData(const CAbilityDefFull& def, int level) {
    count_ = def.DataIntAt(level, 0, count_);
    treantType_ = def.UnitIdAt(level, treantType_);
    summonDuration_ = def.DurationAt(level);
}

// ===== Owl Scout =====
CAbilityOwlScout::CAbilityOwlScout(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEst"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(40.0f);
    SetManaCost(25);
}

void CAbilityOwlScout::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto a single AbilityBuilder timed summon beside the caster.
    if (!caster) return;
    ABActionSummon owl(owlType_, War3ID("AEst"), 1, summonDuration_, 0.0f);
    owl.Run(game, caster, nullptr, true, caster->GetX() + 64.0f, caster->GetY());
}

void CAbilityOwlScout::ApplyEffectData(const CAbilityDefFull& def, int level) {
    owlType_ = def.UnitIdAt(level, owlType_);
    summonDuration_ = def.DurationAt(level);
}

// ===== War Eagle =====
CAbilitySummonHawk::CAbilitySummonHawk(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANsw"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(30.0f);
    SetManaCost(50);
}

void CAbilitySummonHawk::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto a single AbilityBuilder timed summon beside the caster.
    if (!caster) return;
    ABActionSummon hawk(hawkType_, War3ID("ANsw"), 1, summonDuration_, 0.0f);
    hawk.Run(game, caster, nullptr, true, caster->GetX() + 64.0f, caster->GetY());
}

void CAbilitySummonHawk::ApplyEffectData(const CAbilityDefFull& def, int level) {
    hawkType_ = def.UnitIdAt(level, hawkType_);
    summonDuration_ = def.DurationAt(level);
}

// ===== Goblin Factory =====
CAbilityFactory::CAbilityFactory(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANfy"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(500.0f);
    SetCooldown(60.0f);
    SetManaCost(150);
}

void CAbilityFactory::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    // Migrated onto a single AbilityBuilder create-and-buff (TimedLife
    // plus Spawner; graceful without unit data like the code replaced).
    if (!caster) return;
    using BF = ABActionCreateUnitWithBuff::BuffFactory;
    ABActionCreateUnitWithBuff factory(
        factoryType_, x, y,
        {BF([life = factoryLife_]() {
             return std::make_unique<CBuffTimedLife>(War3ID("ANfy"), life);
         }),
         BF([life = factoryLife_, goblin = goblinType_, interval = spawnInterval_,
             boom = explodeDamage_, radius = explodeRadius_]() {
             return std::make_unique<CBuffSpawner>(life, goblin, interval, boom, radius);
         })});
    factory.Run(game, caster, nullptr, true, x, y);
}

void CAbilityFactory::ApplyEffectData(const CAbilityDefFull& def, int level) {
    factoryType_ = def.UnitIdAt(level, factoryType_);
    const std::string goblin = def.DataAt(level, 1);
    if (!goblin.empty()) goblinType_ = War3ID::FromString(goblin);
}

// ===== Kaboom =====
CAbilityKaboom::CAbilityKaboom(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Asds"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_BOTH);
    SetCastRange(128.0f);
    SetCooldown(5.0f);
    SetManaCost(0);
}

void CAbilityKaboom::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)target;
    if (!caster) return;
    armed_ = true;
    // Migrated detonation onto an AbilityBuilder kill (blast itself lives
    // in OnDeath/Explode, outside DoEffect's reach).
    ABActionKillTarget boom;
    boom.Run(game, caster, caster, false, 0, 0);
}

void CAbilityKaboom::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    (void)x; (void)y;
    if (!caster) return;
    armed_ = true;
    // Migrated detonation onto an AbilityBuilder kill (see above).
    ABActionKillTarget boom;
    boom.Run(game, caster, caster, false, 0, 0);
}

void CAbilityKaboom::OnDeath(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    if (armed_ || explodesOnDeath_) {
        armed_ = false;
        Explode(game, unit);
    }
}

void CAbilityKaboom::Explode(CSimulation& game, CUnit* caster) {
    const float radius = std::max(partialRadius_, fullRadius_);
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        if (unit->GetPlayerIndex() == caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - caster->GetX();
        const float dy = unit->GetY() - caster->GetY();
        const float dist2 = dx * dx + dy * dy;
        if (dist2 > radius * radius) continue;
        float damage = (dist2 <= fullRadius_ * fullRadius_)
            ? fullDamage_ : partialDamage_;
        if (unit->IsBuilding()) damage *= buildingFactor_;
        unit->Damage(game, caster, false, true,
                     CAttackType::SPELLS, CDamageType::DEMOLITION,
                     "", damage, 0);
    }
}

void CAbilityKaboom::ApplyEffectData(const CAbilityDefFull& def, int level) {
    fullRadius_ = def.DataFloatAt(level, 0, fullRadius_);
    fullDamage_ = def.DataFloatAt(level, 1, fullDamage_);
    partialRadius_ = def.DataFloatAt(level, 2, partialRadius_);
    partialDamage_ = def.DataFloatAt(level, 3, partialDamage_);
    buildingFactor_ = def.DataFloatAt(level, 4, buildingFactor_);
    explodesOnDeath_ = def.DataIntAt(level, 5, explodesOnDeath_ ? 1 : 0) != 0;
}

// ===== Phoenix Fire =====
CAbilityPhoenixFire::CAbilityPhoenixFire(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Apxf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(0.0f);
    SetManaCost(0);
}

void CAbilityPhoenixFire::OnAdd(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    unit->AddBuff(std::make_unique<CBuffPhoenixFire>(
        99999.0f, damagePerSecond_, radius_, initialDamage_), game);
}

void CAbilityPhoenixFire::OnRemove(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    unit->RemoveBuff(War3ID("BPxf"), game);
}

void CAbilityPhoenixFire::ApplyEffectData(const CAbilityDefFull& def, int level) {
    initialDamage_ = def.DataFloatAt(level, 0, initialDamage_);
    damagePerSecond_ = def.DataFloatAt(level, 1, damagePerSecond_);
    radius_ = def.AreaAt(level);
}
