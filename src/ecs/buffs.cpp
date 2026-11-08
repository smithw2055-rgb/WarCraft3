#include "buffs.h"
#include "ecs/unit.h"
#include "ecs/player.h"
#include "ecs/unit_type.h"
#include "ecs/csimulation.h"
#include "engine/simulation_module.h"
#include "data/unitdata.h"
#include <algorithm>
#include <memory>

namespace {

// Phase 18 Stage RR: Shared state-flag restoration & periodic AoE helpers
// (eliminates duplicated buff-scanning loops across DivineShield, StoneForm,
// Cyclone, Ensnare, Burrow, Ethereal, MagicImmune, Whirlwind, Explode,
// Immolation, and PhoenixFire).

void RefreshUnitInvulnerableFromBuffs(CUnit* target, const CBuff* excluding) {
    if (!target) return;
    bool hasOtherInvul = (target->GetAbility(War3ID("Avul")) != nullptr);
    if (!hasOtherInvul) {
        for (const auto& b : target->GetBuffs()) {
            if (!b || b.get() == excluding) continue;
            const War3ID bid = b->GetId();
            if (bid == War3ID("Bdsh") || bid == War3ID("Bstm") || bid == War3ID("Bcyc")) {
                hasOtherInvul = true;
                break;
            }
        }
    }
    if (!hasOtherInvul) {
        target->SetInvulnerable(false);
    }
}

void RefreshUnitRootedFromBuffs(CUnit* target, const CBuff* excluding) {
    if (!target) return;
    bool hasOtherRoot = target->IsBuilding();
    if (!hasOtherRoot) {
        for (const auto& b : target->GetBuffs()) {
            if (!b || b.get() == excluding) continue;
            const War3ID bid = b->GetId();
            if (bid == War3ID("Bbur") || bid == War3ID("Bena") || bid == War3ID("Bstm")) {
                hasOtherRoot = true;
                break;
            }
        }
    }
    if (!hasOtherRoot) {
        target->SetRooted(false);
    }
}

void RefreshUnitInvisibleFromBuffs(CUnit* target, const CBuff* excluding) {
    if (!target) return;
    bool hasOtherInvis = (target->GetAbility(War3ID("Apiv")) != nullptr);
    if (!hasOtherInvis) {
        for (const auto& b : target->GetBuffs()) {
            if (!b || b.get() == excluding) continue;
            const War3ID bid = b->GetId();
            if (bid == War3ID("Bbur") || bid == War3ID("BOwk") || bid == War3ID("Binv")) {
                hasOtherInvis = true;
                break;
            }
        }
    }
    if (!hasOtherInvis) {
        target->SetInvisible(false);
    }
}

void RefreshUnitMagicImmuneFromBuffs(CUnit* target, const CBuff* excluding) {
    if (!target) return;
    bool hasOtherImmune = (target->GetAbility(War3ID("Amim")) != nullptr);
    if (!hasOtherImmune) {
        for (const auto& b : target->GetBuffs()) {
            if (!b || b.get() == excluding) continue;
            if (b->GrantsMagicImmune()) {
                hasOtherImmune = true;
                break;
            }
        }
    }
    if (!hasOtherImmune) {
        target->SetMagicImmune(false);
    }
}

void DamageEnemyUnitsInRadius(CSimulation& game, CUnit* source,
                              float radius, float amount,
                              CDamageType dmgType, bool isRanged = false) {
    if (!source || amount <= 0.0f) return;
    CPlayer* tp = game.GetPlayer(source->GetPlayerIndex());
    const float radiusSq = radius * radius;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == source) continue;
        CPlayer* up = game.GetPlayer(unit->GetPlayerIndex());
        const bool isAlly = (unit->GetPlayerIndex() == source->GetPlayerIndex()) ||
            (tp && up && (tp->HasAlliance(*up, CAllianceType::ALLIED) ||
                          tp->HasAlliance(*up, CAllianceType::PASSIVE)));
        if (isAlly) continue;
        const float dx = unit->GetX() - source->GetX();
        const float dy = unit->GetY() - source->GetY();
        if (dx * dx + dy * dy <= radiusSq) {
            unit->Damage(game, source, false, isRanged,
                         CAttackType::SPELLS, dmgType, "", amount, 0);
        }
    }
}

void NotifySummonedUnit(CSimulation& game, int summonerHandle, CUnit* spawned) {
    if (!spawned) return;
    if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
        summonedCb(summonerHandle, spawned->GetHandleId());
    }
}

} // namespace

// ===== CBuffSlow =====
CBuffSlow::CBuffSlow(float duration, float slowPercent, int casterHandle)
    : CBuffCloneable(War3ID("Bslo"), duration, false), slowPercent_(slowPercent) {
    casterHandleId_ = casterHandle;
}

void CBuffSlow::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->AddSpeedModifier(GetId(), -slowPercent_);
}

void CBuffSlow::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->RemoveSpeedModifier(GetId());
}

// ===== CBuffPoison =====
CBuffPoison::CBuffPoison(float duration, float damagePerTick, int casterHandle)
    : CBuffCloneable(War3ID("Bpoi"), duration, false), damagePerTick_(damagePerTick) {
    casterHandleId_ = casterHandle;
    SetPeriodic(true, 1.0f);
}

void CBuffPoison::OnPeriodic(CSimulation& game, CUnit* target) {
    if (target && !target->IsDead()) {
        target->Damage(game, nullptr, false, false,
                       CAttackType::NORMAL, CDamageType::NORMAL,
                       "", damagePerTick_, 0);
    }
}

// ===== CBuffBloodlust =====
CBuffBloodlust::CBuffBloodlust(float duration, float speedBonus, float attackSpeedBonus)
    : CBuffCloneable(War3ID("Bblo"), duration, true)
    , speedBonus_(speedBonus), attackSpeedBonus_(attackSpeedBonus) {}

void CBuffBloodlust::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    target->AddSpeedModifier(GetId(), speedBonus_);
    target->SetNonStackingAttackSpeed(GetId(), attackSpeedBonus_);
}

void CBuffBloodlust::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    target->RemoveSpeedModifier(GetId());
    target->ClearNonStackingAttackSpeed(GetId());
}

// ===== CBuffDivineShield =====
CBuffDivineShield::CBuffDivineShield(float duration)
    : CBuffCloneable(War3ID("Bdsh"), duration, true) {}

void CBuffDivineShield::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->SetInvulnerable(true);
}

void CBuffDivineShield::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    RefreshUnitInvulnerableFromBuffs(target, this);
}

// ===== CBuffEthereal =====
CBuffEthereal::CBuffEthereal(float duration, War3ID id)
    : CBuffCloneable(id, duration, false) {}

void CBuffEthereal::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->SetEthereal(true);
}

void CBuffEthereal::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    bool hasOtherEth = (target->GetAbility(War3ID("Aetl")) != nullptr);
    if (!hasOtherEth) {
        for (const auto& b : target->GetBuffs()) {
            if (!b || b.get() == this) continue;
            if (b->GrantsEthereal()) {
                hasOtherEth = true;
                break;
            }
        }
    }
    if (!hasOtherEth) target->SetEthereal(false);
}

// ===== CBuffMagicImmune =====
CBuffMagicImmune::CBuffMagicImmune(float duration, War3ID id)
    : CBuffCloneable(id, duration, true) {}

void CBuffMagicImmune::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->SetMagicImmune(true);
}

void CBuffMagicImmune::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    RefreshUnitMagicImmuneFromBuffs(target, this);
}

// ===== CBuffStun =====
CBuffStun::CBuffStun(float duration)
    : CBuffCloneable(War3ID("Bstn"), duration, false) {}

void CBuffStun::OnApply(CSimulation& game, CUnit* target) {
    if (!target) return;
    if (CBehavior* cur = target->GetCurrentBehavior()) {
        cur->End(game);
    }
    target->ClearOrders();
    const float dur = GetRemaining() > 0 ? GetRemaining() : GetDuration();
    std::unique_ptr<CBehavior> stun;
    if (auto* mod = game.GetSimulationModule()) {
        stun.reset(mod->CreateStunBehavior(dur));
    }
    if (!stun) {
        class CBehaviorEngineStun final : public CBehavior {
        public:
            explicit CBehaviorEngineStun(float d) : duration_(d), remaining_(d) {}
            CBehaviorCategory GetCategory() const override { return CBehaviorCategory::STUN; }
            bool IsInterruptable() const override { return false; }
            void Begin(CSimulation&) override { remaining_ = duration_; }
            void End(CSimulation&) override { remaining_ = 0.0f; }
            bool Update(CSimulation& sim) override {
                remaining_ -= sim.GetSimStepTime();
                return remaining_ <= 0.0f;
            }
            void SetDuration(float d) override { duration_ = d; }
            float GetRemaining() const override { return remaining_; }
            void SetRemaining(float r) override { remaining_ = r; }
        private:
            float duration_ = 0.0f;
            float remaining_ = 0.0f;
        };
        stun = std::make_unique<CBehaviorEngineStun>(dur);
    }
    target->SetCurrentBehavior(std::move(stun));
    if (auto* behavior = target->GetCurrentBehavior()) {
        behavior->Begin(game);
    }
}

void CBuffStun::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    if (auto* behavior = target->GetCurrentBehavior()) {
        if (behavior->GetCategory() == CBehaviorCategory::STUN) {
            target->SetCurrentBehavior(nullptr);
        }
    }
}

void CBuffStun::OnRefresh(CSimulation& game, CUnit* target, const CBuff& newBuff) {
    CBuff::OnRefresh(game, target, newBuff);
    if (target) {
        if (auto* beh = target->GetCurrentBehavior()) {
            if (beh->GetCategory() == CBehaviorCategory::STUN) {
                beh->SetRemaining(std::max(beh->GetRemaining(), remaining_));
            }
        }
    }
}

// ===== CBuffSleep =====
CBuffSleep::CBuffSleep(float duration)
    : CBuffCloneable(War3ID("BUsl"), duration, false) { sleep_ = true; }

void CBuffSleep::OnApply(CSimulation& game, CUnit* target) {
    if (!target) return;
    if (CBehavior* cur = target->GetCurrentBehavior()) {
        cur->End(game);
    }
    target->ClearOrders();
    target->SetCurrentBehavior(nullptr);
}

// ===== CBuffRejuvenation =====
CBuffRejuvenation::CBuffRejuvenation(float duration, float healPerTick)
    : CBuffCloneable(War3ID("Brej"), duration, true), healPerTick_(healPerTick) {
    SetPeriodic(true, 1.0f);
}

void CBuffRejuvenation::OnPeriodic(CSimulation& game, CUnit* target) {
    if (target && !target->IsDead()) {
        target->SetLife(game, std::min(target->GetMaxLife(), target->GetLife() + healPerTick_));
    }
}

// ===== CBuffTimedLife =====
CBuffTimedLife::CBuffTimedLife(War3ID alias, float duration)
    : CBuffCloneable(alias, duration, false) { timedLife_ = true; }

void CBuffTimedLife::OnRemove(CSimulation& game, CUnit* target) {
    if (!target || target->IsDead()) return;
    target->SetInvulnerable(false);
    target->Damage(game, nullptr, false, false,
                   CAttackType::CHAOS, CDamageType::NORMAL,
                   "", 1e9f, 0);
}

// ===== CBuffLifeDrain =====
CBuffLifeDrain::CBuffLifeDrain(float duration, float drainPerTick, int casterHandle)
    : CBuffCloneable(War3ID("ANdr"), duration, false), drainPerTick_(drainPerTick) {
    casterHandleId_ = casterHandle;
    SetPeriodic(true, 1.0f);
}

void CBuffLifeDrain::OnPeriodic(CSimulation& game, CUnit* target) {
    if (!target || target->IsDead()) return;
    target->Damage(game, nullptr, false, false,
                   CAttackType::SPELLS, CDamageType::MAGIC,
                   "", drainPerTick_, 0);
    if (CUnit* caster = game.GetUnit(casterHandleId_)) {
        if (!caster->IsDead()) {
            caster->SetLife(game, std::min(caster->GetMaxLife(), caster->GetLife() + drainPerTick_));
        }
    }
}

// ===== CBuffArmor =====
CBuffArmor::CBuffArmor(War3ID id, float duration, int armorDelta, bool positive)
    : CBuffCloneable(id, duration, positive), armorDelta_(armorDelta) {}

void CBuffArmor::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->AddBonusArmor(armorDelta_);
}

void CBuffArmor::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->AddBonusArmor(-armorDelta_);
}

// ===== CBuffDoom =====
CBuffDoom::CBuffDoom(float duration, float damagePerTick, int casterHandle)
    : CBuffCloneable(War3ID("BNdo"), duration, false), damagePerTick_(damagePerTick) {
    casterHandleId_ = casterHandle;
    SetPeriodic(true, 1.0f);
}

void CBuffDoom::OnPeriodic(CSimulation& game, CUnit* target) {
    if (target && !target->IsDead()) {
        target->Damage(game, nullptr, false, false,
                       CAttackType::SPELLS, CDamageType::DEATH,
                       "", damagePerTick_, 0);
    }
}

void CBuffDoom::OnDeath(CSimulation& game, CUnit* target) {
    if (!target) return;
    int owner = target->GetPlayerIndex();
    if (CUnit* caster = game.GetUnit(casterHandleId_)) {
        if (!caster->IsDead()) owner = caster->GetPlayerIndex();
    }
    CUnit* guard = game.CreateUnit(War3ID("nba2"), owner, target->GetX(), target->GetY(), 0);
    NotifySummonedUnit(game, casterHandleId_, guard);
}

// ===== CBuffLightningShield =====
CBuffLightningShield::CBuffLightningShield(float duration, float damagePerTick, float radius)
    : CBuffCloneable(War3ID("Blsh"), duration, true)
    , damagePerTick_(damagePerTick), radius_(radius) {
    SetPeriodic(true, 1.0f);
}

void CBuffLightningShield::OnPeriodic(CSimulation& game, CUnit* target) {
    if (!target || target->IsDead()) return;
    const float radiusSq = radius_ * radius_;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == target) continue;
        const float dx = unit->GetX() - target->GetX();
        const float dy = unit->GetY() - target->GetY();
        if (dx * dx + dy * dy <= radiusSq) {
            unit->Damage(game, target, false, false,
                         CAttackType::SPELLS, CDamageType::LIGHTNING,
                         "", damagePerTick_, 0);
        }
    }
}

// ===== CBuffInnerFire =====
CBuffInnerFire::CBuffInnerFire(float duration, int bonusDamage, int bonusArmor,
                               War3ID buffId)
    : CBuffCloneable(buffId, duration, true)
    , bonusDamage_(bonusDamage), bonusArmor_(bonusArmor) {}

void CBuffInnerFire::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    target->SetNonStackingDamageBonus(GetId(), bonusDamage_);
    target->AddBonusArmor(bonusArmor_);
}

void CBuffInnerFire::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    target->ClearNonStackingDamageBonus(GetId());
    target->AddBonusArmor(-bonusArmor_);
}

// ===== CBuffClarity =====
CBuffClarity::CBuffClarity(float duration, float regenBonus)
    : CBuffCloneable(War3ID("Bclf"), duration, true), regenBonus_(regenBonus) {}

void CBuffClarity::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->AddBonusManaRegen(regenBonus_);
}

void CBuffClarity::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->AddBonusManaRegen(-regenBonus_);
}

// ===== CBuffHealingWard =====
CBuffHealingWard::CBuffHealingWard(float duration, float healPerTick, float radius)
    : CBuffCloneable(War3ID("Bhwd"), duration, true)
    , healPerTick_(healPerTick), radius_(radius) {
    SetPeriodic(true, 1.0f);
}

void CBuffHealingWard::OnPeriodic(CSimulation& game, CUnit* target) {
    if (!target || target->IsDead()) return;
    CPlayer* tp = game.GetPlayer(target->GetPlayerIndex());
    const float radiusSq = radius_ * radius_;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsBuilding()) continue;
        if (const auto* ut = unit->GetUnitType()) {
            if (ut->GetClassifications().count(CUnitClassification::MECHANICAL)) continue;
        }
        CPlayer* up = game.GetPlayer(unit->GetPlayerIndex());
        const bool isAlly = (unit->GetPlayerIndex() == target->GetPlayerIndex()) ||
            (tp && up && (tp->HasAlliance(*up, CAllianceType::ALLIED) ||
                          tp->HasAlliance(*up, CAllianceType::PASSIVE)));
        if (!isAlly) continue;
        const float dx = unit->GetX() - target->GetX();
        const float dy = unit->GetY() - target->GetY();
        if (dx * dx + dy * dy <= radiusSq) {
            unit->SetLife(game, std::min(unit->GetMaxLife(), unit->GetLife() + healPerTick_));
        }
    }
}

// ===== CBuffEnsnare =====
CBuffEnsnare::CBuffEnsnare(float duration)
    : CBuffCloneable(War3ID("Bena"), duration, false) {}

void CBuffEnsnare::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->SetRooted(true);
}

void CBuffEnsnare::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    RefreshUnitRootedFromBuffs(target, this);
}

// ===== CBuffBerserk =====
CBuffBerserk::CBuffBerserk(float duration, float attackSpeedMul, float damageTakenMul)
    : CBuffCloneable(War3ID("Babr"), duration, true)
    , attackSpeedMul_(attackSpeedMul), damageTakenMul_(damageTakenMul) {}

void CBuffBerserk::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->SetNonStackingAttackSpeed(GetId(), attackSpeedMul_ - 1.0f);
}

void CBuffBerserk::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->ClearNonStackingAttackSpeed(GetId());
}

// ===== CBuffBurrow =====
CBuffBurrow::CBuffBurrow(float duration)
    : CBuffCloneable(War3ID("Bbur"), duration, true) {}

void CBuffBurrow::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    target->SetInvisible(true);
    target->SetRooted(true);
}

void CBuffBurrow::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    RefreshUnitInvisibleFromBuffs(target, this);
    RefreshUnitRootedFromBuffs(target, this);
}

// ===== CBuffTransform =====
CBuffTransform::CBuffTransform(War3ID id, float duration, War3ID formId)
    : CBuffCloneable(id, duration, true), formId_(formId) {}

void CBuffTransform::OnApply(CSimulation& game, CUnit* target) {
    if (!target) return;
    if (CUnitData* data = game.GetUnitData()) {
        if (CUnitType* form = data->GetUnitType(formId_)) {
            target->MorphTo(form, formId_);
        }
    }
}

void CBuffTransform::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->Unmorph();
}

// ===== CBuffAvatar =====
CBuffAvatar::CBuffAvatar(float duration, int bonusDamage, int bonusLife)
    : CBuffCloneable(War3ID("BAvt"), duration, true)
    , bonusDamage_(bonusDamage), bonusLife_(bonusLife) {}

void CBuffAvatar::OnApply(CSimulation& game, CUnit* target) {
    if (!target) return;
    target->SetNonStackingDamageBonus(GetId(), bonusDamage_);
    target->AddBonusMaxLife(static_cast<float>(bonusLife_));
    target->SetLife(game, std::min(target->GetMaxLife(), target->GetLife() + static_cast<float>(bonusLife_)));
}

void CBuffAvatar::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    target->ClearNonStackingDamageBonus(GetId());
    target->AddBonusMaxLife(-static_cast<float>(bonusLife_));
}

// ===== CBuffStoneForm =====
CBuffStoneForm::CBuffStoneForm(float duration)
    : CBuffCloneable(War3ID("Bstm"), duration, true) {}

void CBuffStoneForm::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    target->SetInvulnerable(true);
    target->SetRooted(true);
}

void CBuffStoneForm::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    RefreshUnitInvulnerableFromBuffs(target, this);
    RefreshUnitRootedFromBuffs(target, this);
}

// ===== CBuffManaSiphon =====
CBuffManaSiphon::CBuffManaSiphon(float duration, float manaPerTick, int casterHandle)
    : CBuffCloneable(War3ID("Bsip"), duration, false), manaPerTick_(manaPerTick) {
    casterHandleId_ = casterHandle;
    SetPeriodic(true, 1.0f);
}

void CBuffManaSiphon::OnPeriodic(CSimulation& game, CUnit* target) {
    if (!target || target->IsDead()) return;
    const float drained = std::min(manaPerTick_, target->GetMana());
    if (drained <= 0.0f) return;
    target->SetMana(target->GetMana() - drained);
    if (CUnit* caster = game.GetUnit(casterHandleId_)) {
        if (!caster->IsDead()) {
            caster->SetMana(std::min(static_cast<float>(caster->GetMaxMana()), caster->GetMana() + drained));
        }
    }
}

// ===== CBuffSpawner =====
CBuffSpawner::CBuffSpawner(float duration, War3ID spawnType, float interval,
                           float explodeDamage, float explodeRadius)
    : CBuffCloneable(War3ID("Bspy"), duration, true), spawnType_(spawnType)
    , explodeDamage_(explodeDamage), explodeRadius_(explodeRadius) {
    SetPeriodic(true, interval);
}

void CBuffSpawner::OnPeriodic(CSimulation& game, CUnit* target) {
    if (!target || target->IsDead()) return;
    CUnit* spawned = game.CreateUnit(spawnType_, target->GetPlayerIndex(),
                                     target->GetX() + 64.0f, target->GetY(), 0);
    if (!spawned) return;
    if (explodeDamage_ > 0.0f) {
        spawned->AddBuff(std::make_unique<CBuffExplode>(explodeDamage_, explodeRadius_), game);
    }
    NotifySummonedUnit(game, target->GetHandleId(), spawned);
}

// ===== CBuffSplit =====
CBuffSplit::CBuffSplit(War3ID splitType, int count, War3ID childType, int childCount)
    : CBuffCloneable(War3ID("Bspl"), 3600.0f, false)
    , splitType_(splitType), count_(count)
    , childType_(childType), childCount_(childCount) {}

void CBuffSplit::OnDeath(CSimulation& game, CUnit* target) {
    if (!target) return;
    for (int i = 0; i < count_; i++) {
        CUnit* child = game.CreateUnit(splitType_, target->GetPlayerIndex(),
                                       target->GetX() + (i - count_ / 2) * 64.0f,
                                       target->GetY(), 0);
        if (!child) continue;
        if (childCount_ > 0) {
            child->AddBuff(std::make_unique<CBuffSplit>(childType_, childCount_, War3ID(), 0), game);
        }
        NotifySummonedUnit(game, target->GetHandleId(), child);
    }
}

// ===== CBuffExplode =====
CBuffExplode::CBuffExplode(float damage, float radius)
    : CBuffCloneable(War3ID("Bexp"), 3600.0f, false), damage_(damage), radius_(radius) {}

void CBuffExplode::OnDeath(CSimulation& game, CUnit* target) {
    DamageEnemyUnitsInRadius(game, target, radius_, damage_, CDamageType::FIRE, false);
}

// ===== CBuffDisease =====
CBuffDisease::CBuffDisease(War3ID id, float duration, float damagePerTick)
    : CBuffCloneable(id, duration, false), damagePerTick_(damagePerTick) {
    SetPeriodic(true, 1.0f);
}

void CBuffDisease::OnPeriodic(CSimulation& game, CUnit* target) {
    if (target && !target->IsDead()) {
        target->Damage(game, nullptr, false, false,
                       CAttackType::SPELLS, CDamageType::DISEASE,
                       "", damagePerTick_, 0);
    }
}

// ===== CBuffPhoenixRebirth =====
CBuffPhoenixRebirth::CBuffPhoenixRebirth(float eggHatchDelay)
    : CBuffCloneable(War3ID("Bphx"), 3600.0f, true), eggHatchDelay_(eggHatchDelay) {}

void CBuffPhoenixRebirth::OnDeath(CSimulation& game, CUnit* target) {
    if (!target) return;
    CUnit* egg = game.CreateUnit(War3ID("hpxe"), target->GetPlayerIndex(),
                                 target->GetX(), target->GetY(), 0);
    if (!egg) return;
    egg->AddBuff(std::make_unique<CBuffHatchAfter>(eggHatchDelay_, War3ID("hphx"), true), game);
    NotifySummonedUnit(game, target->GetHandleId(), egg);
}

// ===== CBuffHatchAfter =====
CBuffHatchAfter::CBuffHatchAfter(float duration, War3ID hatchType, bool chainRebirth)
    : CBuffCloneable(War3ID("Bhch"), duration, true)
    , hatchType_(hatchType), chainRebirth_(chainRebirth) {}

void CBuffHatchAfter::OnRemove(CSimulation& game, CUnit* target) {
    if (!target || target->IsDead()) return;
    CUnit* hatched = game.CreateUnit(hatchType_, target->GetPlayerIndex(),
                                     target->GetX(), target->GetY(), 0);
    if (!hatched) return;
    if (chainRebirth_) {
        hatched->AddBuff(std::make_unique<CBuffPhoenixRebirth>(duration_), game);
    }
    game.RemoveUnit(target);
    NotifySummonedUnit(game, target->GetHandleId(), hatched);
}

// ===== CBuffDigest =====
CBuffDigest::CBuffDigest(float duration, float damagePerTick)
    : CBuffCloneable(War3ID("Bdig"), duration, false), damagePerTick_(damagePerTick) {
    SetPeriodic(true, 1.0f);
}

void CBuffDigest::OnPeriodic(CSimulation& game, CUnit* target) {
    if (target && !target->IsDead()) {
        target->Damage(game, nullptr, false, false,
                       CAttackType::SPELLS, CDamageType::NORMAL,
                       "", damagePerTick_, 0);
    }
}

// ===== CBuffCyclone =====
CBuffCyclone::CBuffCyclone(float duration)
    : CBuffCloneable(duration) {
    id_ = War3ID("Bcyc");
}

void CBuffCyclone::OnApply(CSimulation& game, CUnit* target) {
    CBuffStun::OnApply(game, target);
    if (target) target->SetInvulnerable(true);
}

void CBuffCyclone::OnRemove(CSimulation& game, CUnit* target) {
    CBuffStun::OnRemove(game, target);
    RefreshUnitInvulnerableFromBuffs(target, this);
}

// ===== CBuffWhirlwind =====
CBuffWhirlwind::CBuffWhirlwind(float duration, float damagePerSecond, float radius)
    : CBuffCloneable(War3ID("BOww"), duration, true)
    , damagePerSecond_(damagePerSecond), radius_(radius) {
    SetPeriodic(true, 1.0f);
}

void CBuffWhirlwind::OnApply(CSimulation&, CUnit* target) {
    if (!target) return;
    target->SetMagicImmune(true);
    target->SetAttacksDisabled(true);
}

void CBuffWhirlwind::OnRemove(CSimulation&, CUnit* target) {
    if (!target) return;
    target->SetAttacksDisabled(false);
    RefreshUnitMagicImmuneFromBuffs(target, this);
}

void CBuffWhirlwind::OnPeriodic(CSimulation& game, CUnit* target) {
    DamageEnemyUnitsInRadius(game, target, radius_, damagePerSecond_, CDamageType::NORMAL, true);
}

// ===== CBuffImmolation =====
CBuffImmolation::CBuffImmolation(float duration, float damagePerTick,
                                 float manaDrainPerSecond, float radius)
    : CBuffCloneable(War3ID("BEim"), duration, true)
    , damagePerTick_(damagePerTick)
    , manaDrainPerSecond_(manaDrainPerSecond), radius_(radius) {
    SetPeriodic(true, 1.0f);
}

void CBuffImmolation::OnPeriodic(CSimulation& game, CUnit* target) {
    if (!target || target->IsDead()) return;
    if (target->GetMana() < manaDrainPerSecond_) {
        SetRemaining(0.0f);
        return;
    }
    target->SetMana(target->GetMana() - manaDrainPerSecond_);
    DamageEnemyUnitsInRadius(game, target, radius_, damagePerTick_, CDamageType::FIRE, false);
}

// ===== CBuffPhoenixFire =====
CBuffPhoenixFire::CBuffPhoenixFire(float duration, float damagePerSecond, float radius,
                                   float initialDamage)
    : CBuffCloneable(War3ID("BPxf"), duration, true)
    , damagePerSecond_(damagePerSecond), radius_(radius), initialDamage_(initialDamage) {
    SetPeriodic(true, 1.0f);
}

void CBuffPhoenixFire::OnApply(CSimulation& game, CUnit* target) {
    DamageEnemyUnitsInRadius(game, target, radius_, initialDamage_, CDamageType::FIRE, false);
}

void CBuffPhoenixFire::OnPeriodic(CSimulation& game, CUnit* target) {
    DamageEnemyUnitsInRadius(game, target, radius_, damagePerSecond_, CDamageType::FIRE, false);
}

// ===== CBuffAuraStat =====
CBuffAuraStat::CBuffAuraStat(War3ID auraCode, std::vector<Bonus> bonuses,
                             int casterHandle)
    : CBuffCloneable(auraCode, 1e9f, true), bonuses_(std::move(bonuses)) {
    casterHandleId_ = casterHandle;
}

void CBuffAuraStat::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    for (const auto& bonus : bonuses_) {
        target->AddAuraStatBonus(bonus.type, bonus.percent, GetId(), bonus.value,
                                 GetCasterHandleId());
    }
}

void CBuffAuraStat::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    for (const auto& bonus : bonuses_) {
        target->RemoveAuraStatBonus(bonus.type, bonus.percent, GetId(),
                                    GetCasterHandleId());
    }
}
