#include "ecs/abilities/ability_builder.h"
#include "ecs/ability.h"
#include "ecs/behaviors/behavior.h"
#include "ecs/combat/projectile.h"
#include "ecs/csimulation.h"
#include "ecs/fog_state.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "ecs/buffs.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace {

void ResolveCenter(CUnit* caster, CWidget* unitTarget, bool hasPoint,
                   float x, float y, float& outX, float& outY) {
    if (unitTarget) {
        outX = unitTarget->GetX();
        outY = unitTarget->GetY();
    } else if (hasPoint || !caster) {
        outX = x;
        outY = y;
    } else {
        outX = caster->GetX();
        outY = caster->GetY();
    }
}

void HealUnitClamped(CSimulation& game, CUnit* unit, float amount) {
    if (!unit || unit->IsDead() || amount <= 0.0f) return;
    unit->SetLife(game, std::min(unit->GetMaxLife(), unit->GetLife() + amount));
}

void AddManaClamped(CUnit* unit, float amount) {
    if (!unit || unit->IsDead() || amount <= 0.0f) return;
    unit->SetMana(std::min(static_cast<float>(unit->GetMaxMana()), unit->GetMana() + amount));
}

CUnit* CreateUnitForCasterAndNotify(CSimulation& game, CUnit* caster,
                                    War3ID typeId, float x, float y, float facing) {
    if (!caster) return nullptr;
    CUnit* created = game.CreateUnit(typeId, caster->GetPlayerIndex(), x, y, facing);
    if (created) {
        if (const auto& cb = game.GetUnitSummonedCallback()) {
            cb(caster->GetHandleId(), created->GetHandleId());
        }
    }
    return created;
}

bool TransferUnitOwnerToCaster(CSimulation& game, CUnit* caster, CUnit* target, bool fireEvent) {
    if (!caster || !target || target->GetPlayerIndex() == caster->GetPlayerIndex()) return false;
    const int prevOwner = target->GetPlayerIndex();
    if (CPlayer* oldOwner = game.GetPlayer(prevOwner)) {
        oldOwner->RemoveUnit(target);
    }
    target->SetPlayerIndex(caster->GetPlayerIndex());
    if (CPlayer* newOwner = game.GetPlayer(caster->GetPlayerIndex())) {
        newOwner->AddUnit(target);
    }
    if (fireEvent) {
        if (const auto& cb = game.GetOwnerChangedCallback()) {
            cb(target->GetHandleId(), prevOwner);
        }
    }
    return true;
}

} // namespace

ABActionDamageArea::ABActionDamageArea(float radius, float amount,
                                       CAttackType attackType, CDamageType damageType)
    : radius_(radius), amount_(amount), attackType_(attackType), damageType_(damageType) {}

void ABActionDamageArea::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                             bool hasPoint, float x, float y) {
    float cx = 0.0f, cy = 0.0f;
    ResolveCenter(caster, unitTarget, hasPoint, x, y, cx, cy);
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        const float dx = unit->GetX() - cx;
        const float dy = unit->GetY() - cy;
        if (dx * dx + dy * dy <= radius_ * radius_) {
            unit->Damage(game, caster, false, false, attackType_, damageType_, "", amount_, 0);
        }
    }
}

ABActionHealArea::ABActionHealArea(float radius, float amount)
    : radius_(radius), amount_(amount) {}

void ABActionHealArea::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                           bool hasPoint, float x, float y) {
    float cx = 0.0f, cy = 0.0f;
    ResolveCenter(caster, unitTarget, hasPoint, x, y, cx, cy);
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        const float dx = unit->GetX() - cx;
        const float dy = unit->GetY() - cy;
        if (dx * dx + dy * dy <= radius_ * radius_) {
            HealUnitClamped(game, unit, amount_);
        }
    }
}

ABActionApplyBuffArea::ABActionApplyBuffArea(float radius, BuffFactory factory)
    : radius_(radius), factory_(std::move(factory)) {}

void ABActionApplyBuffArea::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                bool hasPoint, float x, float y) {
    if (!factory_) return;
    float cx = 0.0f, cy = 0.0f;
    ResolveCenter(caster, unitTarget, hasPoint, x, y, cx, cy);
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        const float dx = unit->GetX() - cx;
        const float dy = unit->GetY() - cy;
        if (dx * dx + dy * dy <= radius_ * radius_) {
            unit->AddBuff(factory_(), game);
        }
    }
}

ABActionSummon::ABActionSummon(War3ID typeId, War3ID buffAlias, int count,
                               float duration, float spread)
    : typeId_(typeId), buffAlias_(buffAlias), count_(count),
      duration_(duration), spread_(spread) {}

void ABActionSummon::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                         bool hasPoint, float x, float y) {
    float cx = 0.0f, cy = 0.0f;
    ResolveCenter(caster, unitTarget, hasPoint, x, y, cx, cy);
    for (int i = 0; i < count_; i++) {
        const float ox = (i - count_ / 2) * spread_;
        SummonTimedUnit(game, caster, typeId_, buffAlias_,
                        cx + ox, cy + spread_ * 0.5f,
                        caster ? caster->GetFacing() : 0.0f, duration_);
    }
}

void ABEffectChain::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                        bool hasPoint, float x, float y) const {
    for (const auto& action : actions_) {
        if (action) action->Run(game, caster, unitTarget, hasPoint, x, y);
    }
}

AbilityBuilderRegistry& AbilityBuilderRegistry::Instance() {
    static AbilityBuilderRegistry instance;
    return instance;
}

void AbilityBuilderRegistry::Register(War3ID alias, ABEffectChain chain) {
    chains_[alias.value] = std::move(chain);
}

const ABEffectChain* AbilityBuilderRegistry::Find(War3ID alias) const {
    auto it = chains_.find(alias.value);
    return it != chains_.end() ? &it->second : nullptr;
}

void AbilityBuilderRegistry::RegisterDescriptor(ABSpellDescriptor desc) {
    descriptors_[desc.code.value] = std::move(desc);
}

const ABSpellDescriptor* AbilityBuilderRegistry::FindDescriptor(War3ID alias) const {
    auto it = descriptors_.find(alias.value);
    return it != descriptors_.end() ? &it->second : nullptr;
}

bool AbilityBuilderRegistry::Unregister(War3ID alias) {
    bool removed = false;
    if (chains_.erase(alias.value) > 0) removed = true;
    if (descriptors_.erase(alias.value) > 0) removed = true;
    return removed;
}

ABSpellDescriptor ABSpellDescriptor::MakeAreaDamageAndSlow(
    War3ID code, float radius, float defaultDamage, float slowPct, float slowDur,
    float cooldown, int manaCost, CDamageType dmgType) {
    ABSpellDescriptor d;
    d.code = code;
    d.targetType = 0; // NO_TARGET
    d.defaultRange = 0.0f;
    d.defaultCooldown = cooldown;
    d.defaultManaCost = manaCost;
    d.defaultArea = radius;
    d.defaultDuration = slowDur;
    d.defaultData[0] = defaultDamage;
    d.defaultData[1] = slowPct;
    d.onEffect = [dmgType](const ABSpellEffectContext& ctx, CSimulation& game,
                           CUnit* caster, CWidget*, bool hasPoint, float x, float y) {
        if (!caster) return;
        const int casterHandle = caster->GetHandleId();
        const float dmg = ctx.data[0];
        const float slowPercent = ctx.data[1];
        const float dur = ctx.duration > 0.0f ? ctx.duration : 5.0f;
        const float rad = ctx.area > 0.0f ? ctx.area : 250.0f;
        ABEffectChain perUnit;
        perUnit.Add(std::make_unique<ABActionDamageTarget>(dmg, CAttackType::SPELLS, dmgType));
        perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
            [dur, slowPercent, casterHandle]() {
                return std::make_unique<CBuffSlow>(dur, slowPercent, casterHandle);
            }));
        ABActionForEachInRange area(rad, std::move(perUnit));
        area.Run(game, caster, nullptr, hasPoint, x, y);
    };
    return d;
}

ABSpellDescriptor ABSpellDescriptor::MakeMissileDamageAndStun(
    War3ID code, float defaultDamage, float stunDur, float missileSpeed,
    float range, float cooldown, int manaCost, CDamageType dmgType) {
    ABSpellDescriptor d;
    d.code = code;
    d.targetType = 1; // TARGET_UNIT
    d.defaultRange = range;
    d.defaultCooldown = cooldown;
    d.defaultManaCost = manaCost;
    d.defaultDuration = stunDur;
    d.defaultData[0] = defaultDamage;
    d.onEffect = [missileSpeed, dmgType](const ABSpellEffectContext& ctx, CSimulation& game,
                                         CUnit* caster, CWidget* target, bool, float, float) {
        const float dmg = ctx.data[0];
        const float dur = ctx.duration > 0.0f ? ctx.duration : 2.0f;
        ABActionMissileStun bolt(dmg, dur, missileSpeed, CAttackType::SPELLS, dmgType);
        bolt.Run(game, caster, target, false, 0.0f, 0.0f);
    };
    return d;
}

ABSpellDescriptor ABSpellDescriptor::MakeAreaDamageAndStun(
    War3ID code, float radius, float defaultDamage, float stunDur,
    float cooldown, int manaCost, CDamageType dmgType) {
    ABSpellDescriptor d;
    d.code = code;
    d.targetType = 0; // NO_TARGET
    d.defaultRange = 0.0f;
    d.defaultCooldown = cooldown;
    d.defaultManaCost = manaCost;
    d.defaultArea = radius;
    d.defaultDuration = stunDur;
    d.defaultData[0] = defaultDamage;
    d.onEffect = [dmgType](const ABSpellEffectContext& ctx, CSimulation& game,
                           CUnit* caster, CWidget*, bool hasPoint, float x, float y) {
        if (!caster) return;
        const float dmg = ctx.data[0];
        const float dur = ctx.duration > 0.0f ? ctx.duration : 3.0f;
        const float rad = ctx.area > 0.0f ? ctx.area : 250.0f;
        ABEffectChain perUnit;
        perUnit.Add(std::make_unique<ABActionDamageTarget>(dmg, CAttackType::SPELLS, dmgType));
        perUnit.Add(std::make_unique<ABActionStunTarget>(dur));
        ABActionForEachInRange stomp(rad, std::move(perUnit));
        stomp.Run(game, caster, nullptr, hasPoint, x, y);
    };
    return d;
}

ABSpellDescriptor ABSpellDescriptor::MakeSummonTimed(
    War3ID code, War3ID summonUnitType, War3ID buffAlias,
    int defaultCount, float defaultDuration, float cooldown, int manaCost,
    int countDataIdx) {
    ABSpellDescriptor d;
    d.code = code;
    d.targetType = 0; // NO_TARGET
    d.defaultRange = 0.0f;
    d.defaultCooldown = cooldown;
    d.defaultManaCost = manaCost;
    d.defaultDuration = defaultDuration;
    if (countDataIdx >= 0 && countDataIdx < 9) {
        d.defaultData[countDataIdx] = (float)defaultCount;
    }
    d.onEffect = [summonUnitType, buffAlias, defaultCount, countDataIdx](
                     const ABSpellEffectContext& ctx, CSimulation& game,
                     CUnit* caster, CWidget* target, bool hasPoint, float x, float y) {
        int count = defaultCount;
        if (countDataIdx >= 0 && countDataIdx < 9 && ctx.data[countDataIdx] > 0.0f) {
            count = (int)ctx.data[countDataIdx];
        }
        const float dur = ctx.duration > 0.0f ? ctx.duration : 60.0f;
        ABActionSummon summon(summonUnitType, buffAlias, count, dur);
        summon.Run(game, caster, target, hasPoint, x, y);
    };
    return d;
}

ABSpellDescriptor ABSpellDescriptor::MakeTargetHealOrUndeadDamage(
    War3ID code, float defaultHealAmount, float undeadDamageRatio,
    float range, float cooldown, int manaCost, CDamageType dmgType) {
    ABSpellDescriptor d;
    d.code = code;
    d.targetType = 1; // TARGET_UNIT
    d.defaultRange = range;
    d.defaultCooldown = cooldown;
    d.defaultManaCost = manaCost;
    d.defaultData[0] = defaultHealAmount;
    d.onEffect = [undeadDamageRatio, dmgType](
                     const ABSpellEffectContext& ctx, CSimulation& game,
                     CUnit* caster, CWidget* target, bool, float, float) {
        const float amount = ctx.data[0];
        ABEffectChain smite;
        smite.Add(std::make_unique<ABActionDamageTarget>(
            amount * undeadDamageRatio, CAttackType::NORMAL, dmgType, true));
        ABEffectChain mend;
        mend.Add(std::make_unique<ABActionHealTarget>(amount));
        ABActionIf branch(std::make_shared<ABCondTargetUndead>(), std::move(smite), std::move(mend));
        branch.Run(game, caster, target, false, 0.0f, 0.0f);
    };
    return d;
}

ABSpellDescriptor ABSpellDescriptor::MakeSelfInvulnerableBuff(
    War3ID code, float defaultDuration, float cooldown, int manaCost) {
    ABSpellDescriptor d;
    d.code = code;
    d.targetType = 0; // NO_TARGET
    d.defaultRange = 0.0f;
    d.defaultCooldown = cooldown;
    d.defaultManaCost = manaCost;
    d.defaultDuration = defaultDuration;
    d.onEffect = [](const ABSpellEffectContext& ctx, CSimulation& game,
                    CUnit* caster, CWidget*, bool, float, float) {
        if (!caster) return;
        const float dur = ctx.duration > 0.0f ? ctx.duration : 15.0f;
        ABActionAddBuffTarget shield([dur]() { return std::make_unique<CBuffDivineShield>(dur); });
        shield.Run(game, caster, caster, false, 0.0f, 0.0f);
    };
    return d;
}

ABSpellDescriptor ABSpellDescriptor::MakePointWaveAreaDamage(
    War3ID code, int defaultWaves, float defaultDamagePerWave, float defaultRadius,
    float range, float cooldown, int manaCost, CDamageType dmgType) {
    ABSpellDescriptor d;
    d.code = code;
    d.targetType = 2; // TARGET_POINT
    d.defaultRange = range;
    d.defaultCooldown = cooldown;
    d.defaultManaCost = manaCost;
    d.defaultArea = defaultRadius;
    d.defaultData[0] = static_cast<float>(defaultWaves);
    d.defaultData[1] = defaultDamagePerWave;
    d.onEffect = [defaultWaves, defaultDamagePerWave, defaultRadius, dmgType](
                     const ABSpellEffectContext& ctx, CSimulation& game,
                     CUnit* caster, CWidget* target, bool hasPoint, float x, float y) {
        const int waves = std::max(1, ctx.data[0] > 0.0f ? static_cast<int>(ctx.data[0]) : defaultWaves);
        const float dmg = ctx.data[1] > 0.0f ? ctx.data[1] : defaultDamagePerWave;
        const float rad = ctx.area > 0.0f ? ctx.area : defaultRadius;
        float cx = 0.0f, cy = 0.0f;
        ResolveCenter(caster, target, hasPoint, x, y, cx, cy);
        for (int w = 0; w < waves; ++w) {
            ABEffectChain perUnit;
            perUnit.Add(std::make_unique<ABActionDamageTarget>(dmg, CAttackType::SPELLS, dmgType));
            ABActionForEachInRange shard(rad, std::move(perUnit));
            shard.Run(game, caster, nullptr, true, cx, cy);
        }
    };
    return d;
}

ABSpellDescriptor ABSpellDescriptor::MakeTargetChainBounceDamage(
    War3ID code, float defaultInitialDamage, int defaultTargets, float damageDecayPerBounce,
    float bounceRadius, float range, float cooldown, int manaCost, CDamageType dmgType) {
    ABSpellDescriptor d;
    d.code = code;
    d.targetType = 1; // TARGET_UNIT
    d.defaultRange = range;
    d.defaultCooldown = cooldown;
    d.defaultManaCost = manaCost;
    d.defaultArea = bounceRadius;
    d.defaultData[0] = defaultInitialDamage;
    d.defaultData[1] = static_cast<float>(defaultTargets);
    d.defaultData[2] = damageDecayPerBounce;
    d.onEffect = [defaultInitialDamage, defaultTargets, damageDecayPerBounce, bounceRadius, dmgType](
                     const ABSpellEffectContext& ctx, CSimulation& game,
                     CUnit* caster, CWidget* target, bool, float, float) {
        auto* firstUnit = ((target) ? (target)->AsUnit() : nullptr);
        if (!firstUnit || firstUnit->IsDead()) return;

        float curDmg = ctx.data[0] > 0.0f ? ctx.data[0] : defaultInitialDamage;
        const int maxBounces = std::max(1, ctx.data[1] > 0.0f ? static_cast<int>(ctx.data[1]) : defaultTargets);
        const float decay = (ctx.data[2] > 0.0f && ctx.data[2] < 1.0f) ? ctx.data[2] : damageDecayPerBounce;
        const float searchRad = ctx.area > 0.0f ? ctx.area : bounceRadius;
        const float searchRadSq = searchRad * searchRad;

        std::unordered_set<int> hitHandles;
        CUnit* current = firstUnit;
        for (int i = 0; i < maxBounces && current; ++i) {
            hitHandles.insert(current->GetHandleId());
            current->Damage(game, caster, false, false, CAttackType::SPELLS, dmgType, "", curDmg, 0.0f);
            curDmg *= (1.0f - decay);

            // Find nearest unhit living enemy within bounceRadius
            CUnit* nextTarget = nullptr;
            float bestDistSq = searchRadSq;
            for (CUnit* cand : game.GetUnits()) {
                if (!cand || cand->IsDead() || cand == caster) continue;
                if (hitHandles.count(cand->GetHandleId())) continue;
                if (caster && cand->GetPlayerIndex() == caster->GetPlayerIndex()) continue;
                const float dx = cand->GetX() - current->GetX();
                const float dy = cand->GetY() - current->GetY();
                const float d2 = dx * dx + dy * dy;
                if (d2 <= bestDistSq) {
                    bestDistSq = d2;
                    nextTarget = cand;
                }
            }
            current = nextTarget;
        }
    };
    return d;
}

ABSpellDescriptor ABSpellDescriptor::MakeAreaHealAndDispel(
    War3ID code, float defaultRadius, float defaultHealAmount, bool stripNegativeBuffs,
    float range, float cooldown, int manaCost) {
    ABSpellDescriptor d;
    d.code = code;
    d.targetType = range > 0.0f ? 2 : 0; // POINT if ranged, NO_TARGET if self-centered
    d.defaultRange = range;
    d.defaultCooldown = cooldown;
    d.defaultManaCost = manaCost;
    d.defaultArea = defaultRadius;
    d.defaultData[0] = defaultHealAmount;
    d.onEffect = [defaultRadius, defaultHealAmount, stripNegativeBuffs](
                     const ABSpellEffectContext& ctx, CSimulation& game,
                     CUnit* caster, CWidget* target, bool hasPoint, float x, float y) {
        float cx = 0.0f, cy = 0.0f;
        ResolveCenter(caster, target, hasPoint, x, y, cx, cy);
        const float rad = ctx.area > 0.0f ? ctx.area : defaultRadius;
        const float heal = ctx.data[0] > 0.0f ? ctx.data[0] : defaultHealAmount;
        const float rSq = rad * rad;

        for (CUnit* u : game.GetUnits()) {
            if (!u || u->IsDead()) continue;
            if (caster && u->GetPlayerIndex() != caster->GetPlayerIndex()) continue;
            const float dx = u->GetX() - cx;
            const float dy = u->GetY() - cy;
            if (dx * dx + dy * dy <= rSq) {
                if (heal > 0.0f) {
                    u->SetLife(game, std::min(u->GetMaxLife(), u->GetLife() + heal));
                }
                if (stripNegativeBuffs) {
                    u->RemoveBuffsIf(game, false, true, false, true);
                }
            }
        }
    };
    return d;
}

void AbilityBuilderRegistry::RegisterStandardCanonicalDescriptors() {
    // Consolidates standard Warcraft 3 spell archetypes into declarative descriptors:
    RegisterDescriptor(ABSpellDescriptor::MakePointWaveAreaDamage(
        War3ID("AHbz"), 6, 30.0f, 200.0f, 800.0f, 6.0f, 75, CDamageType::COLD));
    RegisterDescriptor(ABSpellDescriptor::MakeTargetHealOrUndeadDamage(
        War3ID("AHhb"), 200.0f, 0.5f, 600.0f, 5.0f, 65, CDamageType::DIVINE));
    RegisterDescriptor(ABSpellDescriptor::MakeSelfInvulnerableBuff(
        War3ID("AHds"), 15.0f, 35.0f, 25));
    RegisterDescriptor(ABSpellDescriptor::MakeAreaDamageAndSlow(
        War3ID("AHtc"), 250.0f, 60.0f, 0.50f, 5.0f, 6.0f, 90, CDamageType::SONIC));
    RegisterDescriptor(ABSpellDescriptor::MakeMissileDamageAndStun(
        War3ID("AHtb"), 100.0f, 5.0f, 800.0f, 600.0f, 9.0f, 75, CDamageType::LIGHTNING));
    RegisterDescriptor(ABSpellDescriptor::MakeAreaDamageAndStun(
        War3ID("AOws"), 250.0f, 25.0f, 3.0f, 6.0f, 90, CDamageType::SONIC));
    RegisterDescriptor(ABSpellDescriptor::MakeTargetChainBounceDamage(
        War3ID("AOcl"), 85.0f, 4, 0.15f, 500.0f, 700.0f, 9.0f, 120, CDamageType::LIGHTNING));
    RegisterDescriptor(ABSpellDescriptor::MakeSummonTimed(
        War3ID("AHwe"), War3ID("hwat"), War3ID("BTLF"), 1, 60.0f, 20.0f, 125, 1));
    RegisterDescriptor(ABSpellDescriptor::MakeAreaHealAndDispel(
        War3ID("AIha"), 500.0f, 150.0f, true, 0.0f, 20.0f, 0));
}

CUnit* SummonTimedUnit(CSimulation& game, CUnit* caster, War3ID typeId,
                       War3ID buffAlias, float x, float y, float facing,
                       float duration) {
    if (!caster) return nullptr;
    CUnit* summoned = game.CreateUnit(typeId, caster->GetPlayerIndex(), x, y, facing);
    if (!summoned) return nullptr;
    summoned->AddBuff(std::make_unique<CBuffTimedLife>(buffAlias, duration), game);
    if (const auto& cb = game.GetUnitSummonedCallback()) {
        cb(caster->GetHandleId(), summoned->GetHandleId());
    }
    return summoned;
}

namespace {


// Shared iteration filter: life, caster/exclude/faction rules.
bool ShouldVisitUnit(CUnit* caster, CUnit* unit, bool includeCaster,
                     const CWidget* exclude, ABFactionFilter factionFilter,
                     ABLifeFilter lifeFilter = ABLifeFilter::LIVING) {
    if (!unit) return false;
    if (lifeFilter == ABLifeFilter::LIVING && unit->IsDead()) return false;
    if (lifeFilter == ABLifeFilter::DEAD && !unit->IsDead()) return false;
    if (unit == caster && !includeCaster) return false;
    if (unit == exclude) return false;
    if (factionFilter != ABFactionFilter::ANY) {
        if (!caster) return false;
        const bool samePlayer = unit->GetPlayerIndex() == caster->GetPlayerIndex();
        if (factionFilter == ABFactionFilter::SAME_PLAYER_AS_CASTER && !samePlayer) {
            return false;
        }
        if (factionFilter == ABFactionFilter::OTHER_PLAYER_THAN_CASTER && samePlayer) {
            return false;
        }
    }
    return true;
}

} // namespace

bool ABCondTargetAlive::Test(CSimulation&, CUnit*, CWidget* unitTarget,
                             bool, float, float) const {
    auto* unit = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    return unit != nullptr && !unit->IsDead();
}

bool ABCondTargetUndead::Test(CSimulation&, CUnit*, CWidget* unitTarget,
                              bool, float, float) const {
    auto* unit = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!unit || unit->IsDead()) return false;
    auto* type = unit->GetUnitType();
    return type != nullptr &&
           type->GetClassifications().count(CUnitClassification::UNDEAD) > 0;
}

bool ABCondTargetAllyOfCaster::Test(CSimulation& game, CUnit* caster,
                                    CWidget* unitTarget,
                                    bool, float, float) const {
    auto* unit = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!caster || !unit) return false;
    auto* player = game.GetPlayer(caster->GetPlayerIndex());
    auto* targetPlayer = game.GetPlayer(unit->GetPlayerIndex());
    if (!player || !targetPlayer) return false;
    return player->HasAlliance(*targetPlayer, CAllianceType::PASSIVE);
}

ABCondAll::ABCondAll(std::vector<std::shared_ptr<ABCondition>> conds)
    : conds_(std::move(conds)) {}

bool ABCondAll::Test(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                     bool hasPoint, float x, float y) const {
    for (const auto& cond : conds_) {
        if (!cond || !cond->Test(game, caster, unitTarget, hasPoint, x, y)) {
            return false;
        }
    }
    return true;
}

ABActionDamageTarget::ABActionDamageTarget(float amount, CAttackType attackType,
                                           CDamageType damageType, bool ranged,
                                           bool isAttack)
    : amount_(amount), attackType_(attackType), damageType_(damageType),
      ranged_(ranged), isAttack_(isAttack) {}

void ABActionDamageTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                               bool, float, float) {
    if (!unitTarget || unitTarget->IsDead()) return;
    // Spell effect reaction (ref CheckUnitForAbilityEffectReaction): a
    // shielded enemy target consumes its shield and takes nothing.
    if (auto* victim = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr)) {
        if (!victim->CheckSpellEffectReaction(game, caster)) return;
    }
    unitTarget->Damage(game, caster, false, ranged_, attackType_, damageType_, "",
                       amount_, 0);
}

ABActionDamageTargetMaxLifePct::ABActionDamageTargetMaxLifePct(
    float fraction, CAttackType attackType, CDamageType damageType)
    : fraction_(fraction), attackType_(attackType), damageType_(damageType) {}

void ABActionDamageTargetMaxLifePct::Run(CSimulation& game, CUnit* caster,
                                         CWidget* unitTarget,
                                         bool, float, float) {
    if (!unitTarget || unitTarget->IsDead()) return;
    unitTarget->Damage(game, caster, false, false, attackType_, damageType_, "",
                       unitTarget->GetMaxLife() * fraction_, 0);
}

ABActionHealTarget::ABActionHealTarget(float amount) : amount_(amount) {}

void ABActionHealTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                             bool, float, float) {
    (void)caster;
    HealUnitClamped(game, ((unitTarget) ? (unitTarget)->AsUnit() : nullptr), amount_);
}

ABActionAddBuffTarget::ABActionAddBuffTarget(BuffFactory factory)
    : factory_(std::move(factory)) {}

void ABActionAddBuffTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                bool, float, float) {
    auto* unit = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!unit || unit->IsDead() || !factory_) return;
    if (!unit->CheckSpellEffectReaction(game, caster)) return;
    unit->AddBuff(factory_(), game);
}

ABActionStunTarget::ABActionStunTarget(float duration) : duration_(duration) {}

void ABActionStunTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                             bool, float, float) {
    auto* unit = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!unit || unit->IsDead()) return;
    if (!unit->CheckSpellEffectReaction(game, caster)) return;
    unit->AddBuff(std::make_unique<CBuffStun>(duration_), game);
}

void ABActionKillTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                             bool, float, float) {
    (void)caster;
    auto* unit = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!unit || unit->IsDead()) return;
    unit->SetLife(game, 0.0f);
}

static bool CanResurrectCorpse(const CUnit* unit) {
    if (!unit || !unit->IsDead()) return false;
    if (unit->IsBoneCorpse()) return false;
    return true;
}

void ABActionResurrectTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                  bool, float, float) {
    (void)caster;
    auto* unit = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!CanResurrectCorpse(unit)) return;
    unit->Revive(game, unit->GetX(), unit->GetY());
}

void ABActionReviveTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                               bool hasPoint, float x, float y) {
    ABActionResurrectTarget{}.Run(game, caster, unitTarget, hasPoint, x, y);
}

ABActionSetLifeTarget::ABActionSetLifeTarget(float life) : life_(life) {}

void ABActionSetLifeTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                bool, float, float) {
    (void)caster;
    auto* unit = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!unit) return;
    unit->SetLife(game, life_);
}

ABActionSetManaTarget::ABActionSetManaTarget(float mana) : mana_(mana) {}

void ABActionSetManaTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                bool, float, float) {
    (void)caster;
    auto* unit = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!unit) return;
    unit->SetMana(mana_);
}

ABActionCreateUnit::ABActionCreateUnit(War3ID typeId, int count, float spread)
    : typeId_(typeId), count_(count), spread_(spread) {}

void ABActionCreateUnit::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                             bool hasPoint, float x, float y) {
    if (!caster) return;
    float cx, cy;
    ResolveCenter(caster, unitTarget, hasPoint, x, y, cx, cy);
    const float facing = caster->GetFacing();
    const float rad = facing * 3.14159265f / 180.0f;
    for (int i = 0; i < count_; i++) {
        const float off = (static_cast<float>(i) - static_cast<float>(count_ - 1) * 0.5f) * spread_;
        game.CreateUnit(typeId_, caster->GetPlayerIndex(),
                        cx + std::cos(rad) * off, cy + std::sin(rad) * off, facing);
    }
}

ABActionTeleportTarget::ABActionTeleportTarget() = default;

ABActionTeleportTarget::ABActionTeleportTarget(float x, float y)
    : hasDest_(true), destX_(x), destY_(y) {}

void ABActionTeleportTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                 bool hasPoint, float x, float y) {
    (void)game;
    (void)caster;
    auto* unit = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    // MassTeleport relocates onto the clicked point; without a point (and
    // no fixed destination) there is nothing to do (matches the .j guard
    // on target location).
    if (!unit || unit->IsDead()) return;
    if (hasDest_) {
        unit->SetPositionAndCheckUnstuck(destX_, destY_, game);
    } else if (hasPoint) {
        unit->SetPositionAndCheckUnstuck(x, y, game);
    }
}

ABActionForEachInRange::ABActionForEachInRange(float radius, ABEffectChain chain,
                                               bool includeCaster, int maxCount)
    : radius_(radius), chain_(std::move(chain)),
      includeCaster_(includeCaster), maxCount_(maxCount) {}

void ABActionForEachInRange::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                 bool hasPoint, float x, float y) {
    if (chain_.Empty()) return;
    float cx, cy;
    ResolveCenter(caster, unitTarget, hasPoint, x, y, cx, cy);
    int visited = 0;
    for (auto* unit : game.GetUnits()) {
        if (!ShouldVisitUnit(caster, unit, includeCaster_, exclude_, factionFilter_,
                             lifeFilter_)) {
            continue;
        }
        const float dx = unit->GetX() - cx;
        const float dy = unit->GetY() - cy;
        if (dx * dx + dy * dy > radius_ * radius_) continue;
        chain_.Run(game, caster, unit, true, unit->GetX(), unit->GetY());
        if (maxCount_ >= 0 && ++visited >= maxCount_) break;
    }
}

ABActionForEachInLine::ABActionForEachInLine(float range, float width,
                                             ABEffectChain chain,
                                             bool includeCaster, int maxCount)
    : range_(range), width_(width), chain_(std::move(chain)),
      includeCaster_(includeCaster), maxCount_(maxCount) {}

void ABActionForEachInLine::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                bool hasPoint, float x, float y) {
    if (chain_.Empty() || !caster) return;
    float tx, ty;
    if (unitTarget) {
        tx = unitTarget->GetX();
        ty = unitTarget->GetY();
    } else if (hasPoint) {
        tx = x;
        ty = y;
    } else {
        return;
    }
    const float dx = tx - caster->GetX();
    const float dy = ty - caster->GetY();
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1.0f) return;
    const float nx = dx / dist, ny = dy / dist;
    int visited = 0;
    for (auto* unit : game.GetUnits()) {
        if (!ShouldVisitUnit(caster, unit, includeCaster_, exclude_, factionFilter_)) {
            continue;
        }
        const float ux = unit->GetX() - caster->GetX();
        const float uy = unit->GetY() - caster->GetY();
        const float proj = ux * nx + uy * ny;
        if (proj < 0 || proj > range_) continue;
        const float perp = std::fabs(ux * ny - uy * nx);
        if (perp > width_ / 2) continue;
        chain_.Run(game, caster, unit, true, unit->GetX(), unit->GetY());
        if (maxCount_ >= 0 && ++visited >= maxCount_) break;
    }
}

ABActionBounce::ABActionBounce(int bounces, float range, ChainFactory factory,
                               bool includeCaster)
    : bounces_(bounces), range_(range), factory_(std::move(factory)),
      includeCaster_(includeCaster) {}

void ABActionBounce::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                         bool hasPoint, float x, float y) {
    if (!factory_ || !unitTarget || unitTarget->IsDead()) return;
    (void)hasPoint;
    (void)x;
    (void)y;
    CWidget* current = unitTarget;
    std::unordered_set<CWidget*> struck;
    for (int i = 0; i < bounces_ && current; i++) {
        ABEffectChain chain = factory_(i);
        chain.Run(game, caster, current, true, current->GetX(), current->GetY());
        struck.insert(current);
        CUnit* next = nullptr;
        float bestDist = range_;
        for (auto* other : game.GetUnits()) {
            if (other == current || struck.count(other) > 0) continue;
            if (!ShouldVisitUnit(caster, other, includeCaster_, nullptr, factionFilter_,
                                 lifeFilter_)) {
                continue;
            }
            const float dx = other->GetX() - current->GetX();
            const float dy = other->GetY() - current->GetY();
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < bestDist) {
                bestDist = dist;
                next = other;
            }
        }
        current = next;
    }
}

ABActionIf::ABActionIf(std::shared_ptr<ABCondition> cond, ABEffectChain thenChain,
                       ABEffectChain elseChain)
    : cond_(std::move(cond)), thenChain_(std::move(thenChain)),
      elseChain_(std::move(elseChain)) {}

ABActionCreateUnitAtTarget::ABActionCreateUnitAtTarget(War3ID typeId, int count,
                                                           float stepX)
    : typeId_(typeId), count_(count), stepX_(stepX) {}

void ABActionCreateUnitAtTarget::Run(CSimulation& game, CUnit* caster,
                                     CWidget* unitTarget,
                                     bool, float, float) {
    if (!caster || !unitTarget) return;
    for (int i = 0; i < count_; i++) {
        CreateUnitForCasterAndNotify(game, caster, typeId_,
                                     unitTarget->GetX() + i * stepX_,
                                     unitTarget->GetY(), 0.0f);
    }
}

ABActionCreateUnitAtPoint::ABActionCreateUnitAtPoint(War3ID typeId, float x, float y,
                                                     float facing)
    : typeId_(typeId), x_(x), y_(y), facing_(facing) {}

void ABActionCreateUnitAtPoint::Run(CSimulation& game, CUnit* caster,
                                    CWidget*, bool, float, float) {
    CreateUnitForCasterAndNotify(game, caster, typeId_, x_, y_, facing_);
}

ABActionSummonRing::ABActionSummonRing(War3ID typeId, War3ID buffAlias, int count,
                                       float duration, float radius)
    : typeId_(typeId), buffAlias_(buffAlias), count_(count),
      duration_(duration), radius_(radius) {}

void ABActionSummonRing::Run(CSimulation& game, CUnit* caster, CWidget*,
                             bool hasPoint, float x, float y) {
    if (!caster || !hasPoint) return;
    for (int i = 0; i < count_; i++) {
        const float angle =
            (count_ > 1) ? (2.0f * 3.14159265f * (float)i / (float)count_) : 0.0f;
        SummonTimedUnit(game, caster, typeId_, buffAlias_,
                        x + radius_ * std::cos(angle), y + radius_ * std::sin(angle),
                        angle, duration_);
    }
}

ABActionManaSteal::ABActionManaSteal(float maxAmount) : maxAmount_(maxAmount) {}

void ABActionManaSteal::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                            bool, float, float) {
    (void)game;
    if (!unitTarget || !caster) return;
    auto* targetUnit = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!targetUnit || targetUnit->IsDead()) return;
    float stolen = maxAmount_;
    if (stolen > targetUnit->GetMana()) stolen = targetUnit->GetMana();
    targetUnit->SetMana(targetUnit->GetMana() - stolen);
    float mana = caster->GetMana() + stolen;
    if (mana > static_cast<float>(caster->GetMaxMana())) {
        mana = static_cast<float>(caster->GetMaxMana());
    }
    caster->SetMana(mana);
}

ABActionHealCaster::ABActionHealCaster(float amount) : amount_(amount) {}

void ABActionHealCaster::Run(CSimulation& game, CUnit* caster, CWidget*,
                             bool, float, float) {
    if (!caster || caster->IsDead()) return;
    float healed = caster->GetLife() + amount_;
    if (healed > caster->GetMaxLife()) healed = caster->GetMaxLife();
    caster->SetLife(game, healed);
}

ABActionAddGoldToCaster::ABActionAddGoldToCaster(int gold) : gold_(gold) {}

void ABActionAddGoldToCaster::Run(CSimulation& game, CUnit* caster, CWidget*,
                                  bool, float, float) {
    if (!caster) return;
    if (CPlayer* owner = game.GetPlayer(caster->GetPlayerIndex())) {
        owner->AddGold(gold_);
    }
}

void ABActionRemoveTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                               bool, float, float) {
    (void)caster;
    auto* unit = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!unit) return;
    game.RemoveUnit(unit);
}

ABActionDevourTarget::ABActionDevourTarget(float digestDuration, float digestPerTick)
    : digestDuration_(digestDuration), digestPerTick_(digestPerTick) {}

void ABActionDevourTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                               bool, float, float) {
    if (!unitTarget || !caster || unitTarget->IsDead() || unitTarget == caster) return;
    auto* victim = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!victim || victim->IsHero()) return;
    if (victim->GetPlayerIndex() == caster->GetPlayerIndex()) return;
    if (!victim->GetUnitType()) return;
    // Ground meat only (ref targs); flyers cannot be swallowed.
    if (victim->GetUnitType()->GetMovementType() == PathingGridMovementType::FLY) return;
    if (!caster->LoadUnit(victim)) return;
    victim->AddBuff(std::make_unique<CBuffDigest>(digestDuration_, digestPerTick_), game);
}

ABActionCreateUnitWithBuff::ABActionCreateUnitWithBuff(War3ID typeId, float x, float y,
                                                       std::vector<BuffFactory> factories)
    : typeId_(typeId), x_(x), y_(y), factories_(std::move(factories)) {}

void ABActionCreateUnitWithBuff::Run(CSimulation& game, CUnit* caster, CWidget*,
                                     bool, float, float) {
    if (!caster) return;
    CUnit* created = game.CreateUnit(typeId_, caster->GetPlayerIndex(), x_, y_, 0);
    if (!created) return;
    for (auto& factory : factories_) {
        if (factory) created->AddBuff(factory(), game);
    }
    if (const auto& cb = game.GetUnitSummonedCallback()) {
        cb(caster->GetHandleId(), created->GetHandleId());
    }
}

void ABActionTransferToCaster::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                   bool, float, float) {
    TransferUnitOwnerToCaster(game, caster, ((unitTarget) ? (unitTarget)->AsUnit() : nullptr), true);
}

ABActionCreateUnitWithAbility::ABActionCreateUnitWithAbility(
    War3ID typeId, float x, float y, AbilityFactory factory)
    : typeId_(typeId), x_(x), y_(y), factory_(std::move(factory)) {}

void ABActionCreateUnitWithAbility::Run(CSimulation& game, CUnit* caster, CWidget*,
                                        bool, float, float) {
    if (!caster || !factory_) return;
    CUnit* created = game.CreateUnit(typeId_, caster->GetPlayerIndex(), x_, y_, 0);
    if (!created) return;
    if (auto ability = factory_(game, created)) {
        created->AddAbility(game, std::move(ability));
    }
    if (const auto& cb = game.GetUnitSummonedCallback()) {
        cb(caster->GetHandleId(), created->GetHandleId());
    }
}

ABActionSummonCopyOfTarget::ABActionSummonCopyOfTarget(War3ID buffAlias, float duration,
                                                       float offsetX, float offsetY)
    : buffAlias_(buffAlias), duration_(duration), offsetX_(offsetX), offsetY_(offsetY) {}

void ABActionSummonCopyOfTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                     bool, float, float) {
    auto* target = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!caster || !target || target->IsDead()) return;
    CUnit* copy = SummonTimedUnit(game, caster, target->GetTypeId(), buffAlias_,
                                  target->GetX() + offsetX_, target->GetY() + offsetY_,
                                  target->GetFacing(), duration_);
    if (copy) copy->SetIsIllusion(true);
}

ABActionAnimateDeadCorpse::ABActionAnimateDeadCorpse(War3ID buffAlias, float duration)
    : buffAlias_(buffAlias), duration_(duration) {}

void ABActionAnimateDeadCorpse::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                    bool, float, float) {
    auto* target = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!caster || !target || !target->IsDead()) return;
    target->Revive(game, target->GetX(), target->GetY());
    target->SetInvulnerable(true);
    TransferUnitOwnerToCaster(game, caster, target, false);
    target->AddBuff(std::make_unique<CBuffTimedLife>(buffAlias_, duration_), game);
    if (const auto& cb = game.GetUnitSummonedCallback()) {
        cb(caster->GetHandleId(), target->GetHandleId());
    }
}

void ABActionAncestralRevive::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                  bool, float, float) {
    auto* target = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!caster || !target || !target->IsDead()) return;
    const War3ID typeId = target->GetTypeId();
    if (typeId != War3ID("otau") && typeId != War3ID("Otch")) return;
    target->Revive(game, target->GetX(), target->GetY());
    TransferUnitOwnerToCaster(game, caster, target, false);
}

ABActionDevourMagic::ABActionDevourMagic(float manaPerBuff) : manaPerBuff_(manaPerBuff) {}

void ABActionDevourMagic::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                              bool, float, float) {
    auto* target = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!target || target->IsDead()) return;
    const size_t before = target->GetBuffs().size();
    target->RemoveBuffsIf(game, true, true, false, true);
    const size_t eaten = before - target->GetBuffs().size();
    if (eaten > 0 && caster && !caster->IsDead()) {
        float mana = caster->GetMana() + manaPerBuff_ * (float)eaten;
        if (mana > (float)caster->GetMaxMana()) mana = (float)caster->GetMaxMana();
        caster->SetMana(mana);
    }
}

ABActionManaBurn::ABActionManaBurn(int maxBurn, CAttackType attackType,
                                   CDamageType damageType)
    : maxBurn_(maxBurn), attackType_(attackType), damageType_(damageType) {}

void ABActionManaBurn::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                           bool, float, float) {
    auto* unit = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!unit || unit->IsDead()) return;
    if (!unit->CheckSpellEffectReaction(game, caster)) return;
    const int burned = std::min(maxBurn_, static_cast<int>(unit->GetMana()));
    unit->SetMana(unit->GetMana() - burned);
    unit->Damage(game, caster, false, false, attackType_, damageType_, "",
                 static_cast<float>(burned), 0);
}

void ABActionIf::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                     bool hasPoint, float x, float y) {
    const bool take = cond_ && cond_->Test(game, caster, unitTarget, hasPoint, x, y);
    (take ? thenChain_ : elseChain_).Run(game, caster, unitTarget, hasPoint, x, y);
}

ABActionBeginChannel::ABActionBeginChannel(int durationTicks, ABEffectChain tick,
                                           ABEffectChain finish)
    : durationTicks_(durationTicks),
      tick_(std::move(tick)),
      finish_(std::move(finish)) {}

void ABActionBeginChannel::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                               bool hasPoint, float x, float y) {
    (void)game;
    if (spent_ || !caster || caster->IsDead() || durationTicks_ < 0) return;
    spent_ = true;
    const int targetHandle = unitTarget ? unitTarget->GetHandleId() : -1;
    auto behavior = CreateChannelBehavior(
        std::move(tick_), std::move(finish_), durationTicks_,
        targetHandle, hasPoint, x, y);
    caster->SetCurrentBehavior(std::move(behavior));
}

ABActionMissileStun::ABActionMissileStun(float damage, float stunDuration, float speed,
                                         CAttackType attackType, CDamageType damageType)
    : damage_(damage), stunDuration_(stunDuration), speed_(speed),
      attackType_(attackType), damageType_(damageType) {}

void ABActionMissileStun::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                              bool, float, float) {
    auto* target = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!caster || !target || target->IsDead()) return;
    // StormBolt.j fidelity: the missile carries the damage AND the stun
    // lands on impact (no instant effect at cast; fizzle when the target
    // dies mid-flight).
    CProjectile missile;
    missile.attackerHandleId = caster->GetHandleId();
    missile.targetHandleId = target->GetHandleId();
    missile.x = caster->GetX();
    missile.y = caster->GetY();
    missile.speed = speed_;
    missile.damage = damage_;
    missile.attackType = attackType_;
    missile.damageType = damageType_;
    missile.isAttack = false;
    missile.isRanged = false;
    missile.checkEffectReaction = true;
    missile.maxTravel = 4096.0f;
    const float stun = stunDuration_;
    missile.onHit = [stun](CSimulation& sim, CUnit*, CWidget* hit) {
        auto* victim = ((hit) ? (hit)->AsUnit() : nullptr);
        if (!victim || victim->IsDead()) return;
        victim->AddBuff(std::make_unique<CBuffStun>(stun), sim);
    };
    game.SpawnProjectile(std::move(missile));
}

ABActionDispelArea::ABActionDispelArea(float radius, bool removePositive,
                                       bool removeNegative)
    : radius_(radius), removePositive_(removePositive),
      removeNegative_(removeNegative) {}

void ABActionDispelArea::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                             bool hasPoint, float x, float y) {
    float cx, cy;
    ResolveCenter(caster, unitTarget, hasPoint, x, y, cx, cy);
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        const float dx = unit->GetX() - cx;
        const float dy = unit->GetY() - cy;
        if (dx * dx + dy * dy > radius_ * radius_) continue;
        // keepTimedLife=true: summons survive dispels (Abolish Magic rule).
        unit->RemoveBuffsIf(game, removePositive_, removeNegative_, false, true);
    }
}

ABActionDispelTarget::ABActionDispelTarget(bool removePositive, bool removeNegative)
    : removePositive_(removePositive), removeNegative_(removeNegative) {}

void ABActionDispelTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                               bool, float, float) {
    (void)caster;
    auto* unit = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!unit || unit->IsDead()) return;
    unit->RemoveBuffsIf(game, removePositive_, removeNegative_, false, true);
}

void ABActionTauntTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                              bool, float, float) {
    (void)caster;
    auto* unit = ((unitTarget) ? (unitTarget)->AsUnit() : nullptr);
    if (!unit || unit->IsDead() || !foe_ || foe_->IsDead()) return;
    game.IssueUnitOrder(unit->GetHandleId(),
                        std::make_unique<COrderTargetWidget>(OrderIds::ATTACK, foe_));
}

ABActionCreateVision::ABActionCreateVision(float radius, float duration)
    : radius_(radius), duration_(duration) {}

void ABActionCreateVision::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                               bool hasPoint, float x, float y) {
    if (!caster || radius_ <= 0.0f) return;
    float cx, cy;
    ResolveCenter(caster, unitTarget, hasPoint, x, y, cx, cy);
    CFogSystem* fog = game.GetFogSystem();
    if (!fog) return;
    const int id = fog->AddModifier(caster->GetPlayerIndex(),
                                    CFogArea::Circle(cx, cy, radius_),
                                    CFogState::VISIBLE);
    if (id < 0) return;
    fog->SetModifierEnabled(id, true);
    if (duration_ > 0.0f) {
        game.AddDelayedAction(duration_, [fog, id]() { fog->RemoveModifier(id); });
    }
}
