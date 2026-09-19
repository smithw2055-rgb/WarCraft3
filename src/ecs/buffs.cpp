#include "buffs.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/behaviors/behavior_stun.h"
#include <memory>

// ===== CBuffSlow =====
CBuffSlow::CBuffSlow(float duration, float slowPercent, int casterHandle)
    : CBuff(War3ID("Bslo"), duration, false), slowPercent_(slowPercent) {
    casterHandleId_ = casterHandle;
}

void CBuffSlow::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    // Store the delta (not the original speed) so concurrent speed buffs
    // applied in between are not clobbered on remove.
    appliedReduction_ = (int)(target->GetSpeed() * slowPercent_);
    target->SetSpeed(target->GetSpeed() - appliedReduction_);
}

void CBuffSlow::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    target->SetSpeed(target->GetSpeed() + appliedReduction_);
    appliedReduction_ = 0;
}

CBuff* CBuffSlow::Clone() const {
    auto* copy = new CBuffSlow(duration_, slowPercent_, casterHandleId_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffPoison =====
CBuffPoison::CBuffPoison(float duration, float damagePerTick, int casterHandle)
    : CBuff(War3ID("Bpoi"), duration, false), damagePerTick_(damagePerTick) {
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

CBuff* CBuffPoison::Clone() const {
    auto* copy = new CBuffPoison(duration_, damagePerTick_, casterHandleId_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffBloodlust =====
CBuffBloodlust::CBuffBloodlust(float duration, float speedBonus, float attackSpeedBonus)
    : CBuff(War3ID("Bblo"), duration, true)
    , speedBonus_(speedBonus), attackSpeedBonus_(attackSpeedBonus) {}

void CBuffBloodlust::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    appliedSpeedBonus_ = (int)(target->GetSpeed() * speedBonus_);
    target->SetSpeed(target->GetSpeed() + appliedSpeedBonus_);
    target->MultiplyAttackSpeed(1.0f + attackSpeedBonus_);
}

void CBuffBloodlust::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    target->SetSpeed(target->GetSpeed() - appliedSpeedBonus_);
    appliedSpeedBonus_ = 0;
    target->DivideAttackSpeed(1.0f + attackSpeedBonus_);
}

CBuff* CBuffBloodlust::Clone() const {
    auto* copy = new CBuffBloodlust(duration_, speedBonus_, attackSpeedBonus_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffThorns =====
CBuffThorns::CBuffThorns(float duration, float reflectPercent, War3ID id)
    : CBuff(id, duration, true), reflectPercent_(reflectPercent) {}

void CBuffThorns::OnApply(CSimulation& game, CUnit* target) {
    (void)game; (void)target;
    // Reflection is handled in CUnit::Damage via HasBuff(Bthr); no registration needed.
}

void CBuffThorns::OnRemove(CSimulation& game, CUnit* target) {
    (void)game; (void)target;
    // See OnApply: buff presence is the only state.
}

CBuff* CBuffThorns::Clone() const {
    auto* copy = new CBuffThorns(duration_, reflectPercent_, GetId());
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffLifesteal =====
CBuffLifesteal::CBuffLifesteal(float duration, float stealPercent)
    : CBuff(War3ID("BUav"), duration, true), stealPercent_(stealPercent) {}

void CBuffLifesteal::OnApply(CSimulation& game, CUnit* target) {
    (void)game; (void)target;
    // Steal is applied in CUnit::Damage via HasBuff(BUav); no registration needed.
}

void CBuffLifesteal::OnRemove(CSimulation& game, CUnit* target) {
    (void)game; (void)target;
    // See OnApply: buff presence is the only state.
}

CBuff* CBuffLifesteal::Clone() const {
    auto* copy = new CBuffLifesteal(duration_, stealPercent_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffDivineShield =====
CBuffDivineShield::CBuffDivineShield(float duration)
    : CBuff(War3ID("Bdsh"), duration, true) {}

void CBuffDivineShield::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    target->SetInvulnerable(true);
}

void CBuffDivineShield::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    // Don't strip invulnerability granted by another active shield:
    // OnRemove runs before the buff is erased, so self is still counted.
    int shieldCount = 0;
    for (const auto& b : target->GetBuffs()) {
        if (b && b->GetId() == War3ID("Bdsh")) ++shieldCount;
    }
    if (shieldCount <= 1) {
        target->SetInvulnerable(false);
    }
}

CBuff* CBuffDivineShield::Clone() const {
    auto* copy = new CBuffDivineShield(duration_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffStun =====
CBuffStun::CBuffStun(float duration)
    : CBuff(War3ID("Bstn"), duration, false) {}

void CBuffStun::OnApply(CSimulation& game, CUnit* target) {
    if (!target) return;
    target->ClearOrders();
    auto stun = std::make_unique<CBehaviorStun>();
    stun->SetDuration(GetRemaining() > 0 ? GetRemaining() : GetDuration());
    target->SetCurrentBehavior(std::move(stun));
    if (auto* behavior = target->GetCurrentBehavior()) {
        behavior->Begin(game);
    }
}

void CBuffStun::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    // Only clear if the unit is still stunned (behavior may already have
    // expired and been cleaned up by CUnit::Update).
    if (auto* behavior = target->GetCurrentBehavior()) {
        if (behavior->GetCategory() == CBehaviorCategory::STUN) {
            target->SetCurrentBehavior(nullptr);
        }
    }
}

CBuff* CBuffStun::Clone() const {
    auto* copy = new CBuffStun(duration_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffSleep =====
// Canonical buff id BUsl (ref Units\AbilityBuffData.slk, Dreadlord Sleep).
CBuffSleep::CBuffSleep(float duration)
    : CBuff(War3ID("BUsl"), duration, false) {}

void CBuffSleep::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    // Sleep interrupts orders like stun, but any damage wakes the unit
    // (handled in CUnit::Damage). No stun behavior: the unit simply idles.
    target->ClearOrders();
    target->SetCurrentBehavior(nullptr);
}

void CBuffSleep::OnRemove(CSimulation& game, CUnit* target) {
    (void)game; (void)target;
}

CBuff* CBuffSleep::Clone() const {
    auto* copy = new CBuffSleep(duration_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffSilence =====
// Canonical buff id BNsi (ref Units\AbilityBuffData.slk, Dark Ranger Silence).
CBuffSilence::CBuffSilence(float duration)
    : CBuff(War3ID("BNsi"), duration, false) {}

CBuff* CBuffSilence::Clone() const {
    auto* copy = new CBuffSilence(duration_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffHex =====
// Canonical buff id BOhx (ref Units\AbilityBuffData.slk, Shadow Hunter Hex).
CBuffHex::CBuffHex(float duration)
    : CBuff(War3ID("BOhx"), duration, false) {}

CBuff* CBuffHex::Clone() const {
    auto* copy = new CBuffHex(duration_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffRejuvenation =====
CBuffRejuvenation::CBuffRejuvenation(float duration, float healPerTick)
    : CBuff(War3ID("Brej"), duration, true), healPerTick_(healPerTick) {
    SetPeriodic(true, 1.0f);
}

void CBuffRejuvenation::OnPeriodic(CSimulation& game, CUnit* target) {
    if (target && !target->IsDead()) {
        float newLife = target->GetLife() + healPerTick_;
        if (newLife > target->GetMaxLife()) newLife = target->GetMaxLife();
        target->SetLife(game, newLife);
    }
}

CBuff* CBuffRejuvenation::Clone() const {
    auto* copy = new CBuffRejuvenation(duration_, healPerTick_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffTimedLife =====
CBuffTimedLife::CBuffTimedLife(War3ID alias, float duration)
    : CBuff(alias, duration, false) {}

void CBuffTimedLife::OnRemove(CSimulation& game, CUnit* target) {
    if (!target || target->IsDead()) return;
    // Summoned unit dissipates when its timed life runs out (ref
    // CBuffTimedLife.onBuffRemove -> unit.kill). Lethal chaos damage keeps
    // death callbacks and corpse flow consistent with normal kills.
    // Dissipation is not damage: raised (invulnerable) summons such as
    // Animate Dead still expire, so drop invulnerability first.
    target->SetInvulnerable(false);
    target->Damage(game, nullptr, false, false,
                   CAttackType::CHAOS, CDamageType::NORMAL,
                   "", 1e9f, 0);
}

CBuff* CBuffTimedLife::Clone() const {
    auto* copy = new CBuffTimedLife(GetId(), duration_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffLifeDrain =====
CBuffLifeDrain::CBuffLifeDrain(float duration, float drainPerTick, int casterHandle)
    : CBuff(War3ID("ANdr"), duration, false), drainPerTick_(drainPerTick) {
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
            float newLife = caster->GetLife() + drainPerTick_;
            if (newLife > caster->GetMaxLife()) newLife = caster->GetMaxLife();
            caster->SetLife(game, newLife);
        }
    }
}

CBuff* CBuffLifeDrain::Clone() const {
    auto* copy = new CBuffLifeDrain(duration_, drainPerTick_, casterHandleId_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffArmor =====
CBuffArmor::CBuffArmor(War3ID id, float duration, int armorDelta, bool positive)
    : CBuff(id, duration, positive), armorDelta_(armorDelta) {}

void CBuffArmor::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->AddBonusArmor(armorDelta_);
}

void CBuffArmor::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->AddBonusArmor(-armorDelta_);
}

CBuff* CBuffArmor::Clone() const {
    auto* copy = new CBuffArmor(GetId(), duration_, armorDelta_, IsPositive());
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffDoom =====
CBuffDoom::CBuffDoom(float duration, float damagePerTick, int casterHandle)
    : CBuff(War3ID("BNdo"), duration, false), damagePerTick_(damagePerTick) {
    casterHandleId_ = casterHandle;
    SetPeriodic(true, 1.0f);
}

void CBuffDoom::OnPeriodic(CSimulation& game, CUnit* target) {
    if (!target || target->IsDead()) return;
    target->Damage(game, nullptr, false, false,
                   CAttackType::SPELLS, CDamageType::DEATH,
                   "", damagePerTick_, 0);
}

void CBuffDoom::OnDeath(CSimulation& game, CUnit* target) {
    if (!target) return;
    // A unit that dies while doomed rises as a Doom Guard for the
    // caster's side (ref Doom on-death spawn). Expiry on a living
    // target spawns nothing, so this lives on OnDeath, not OnRemove.
    int owner = target->GetPlayerIndex();
    if (CUnit* caster = game.GetUnit(casterHandleId_)) {
        if (!caster->IsDead()) owner = caster->GetPlayerIndex();
    }
    CUnit* guard = game.CreateUnit(War3ID("nba2"), owner, target->GetX(), target->GetY(), 0);
    if (guard) {
        if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
            summonedCb(casterHandleId_, guard->GetHandleId());
        }
    }
}

CBuff* CBuffDoom::Clone() const {
    auto* copy = new CBuffDoom(duration_, damagePerTick_, casterHandleId_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffLightningShield =====
CBuffLightningShield::CBuffLightningShield(float duration, float damagePerTick, float radius)
    : CBuff(War3ID("Blsh"), duration, true)
    , damagePerTick_(damagePerTick), radius_(radius) {
    SetPeriodic(true, 1.0f);
}

void CBuffLightningShield::OnPeriodic(CSimulation& game, CUnit* target) {
    if (!target || target->IsDead()) return;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == target) continue;
        if (unit->GetPlayerIndex() == target->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - target->GetX();
        const float dy = unit->GetY() - target->GetY();
        if (dx * dx + dy * dy <= radius_ * radius_) {
            unit->Damage(game, target, false, false,
                         CAttackType::SPELLS, CDamageType::LIGHTNING,
                         "", damagePerTick_, 0);
        }
    }
}

CBuff* CBuffLightningShield::Clone() const {
    auto* copy = new CBuffLightningShield(duration_, damagePerTick_, radius_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffPolymorph =====
CBuffPolymorph::CBuffPolymorph(float duration)
    : CBuffHex(duration) {
    id_ = War3ID("Bply");
}

CBuff* CBuffPolymorph::Clone() const {
    auto* copy = new CBuffPolymorph(duration_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffInnerFire =====
CBuffInnerFire::CBuffInnerFire(float duration, int bonusDamage, int bonusArmor)
    : CBuff(War3ID("Binf"), duration, true)
    , bonusDamage_(bonusDamage), bonusArmor_(bonusArmor) {}

void CBuffInnerFire::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    target->AddBonusDamage(bonusDamage_);
    target->AddBonusArmor(bonusArmor_);
}

void CBuffInnerFire::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
    target->AddBonusDamage(-bonusDamage_);
    target->AddBonusArmor(-bonusArmor_);
}

CBuff* CBuffInnerFire::Clone() const {
    auto* copy = new CBuffInnerFire(duration_, bonusDamage_, bonusArmor_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffDefend =====
CBuffDefend::CBuffDefend(float duration)
    : CBuff(War3ID("Bdef"), duration, true) {}

CBuff* CBuffDefend::Clone() const {
    auto* copy = new CBuffDefend(duration_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffClarity =====
CBuffClarity::CBuffClarity(float duration, float regenBonus)
    : CBuff(War3ID("Bclf"), duration, true), regenBonus_(regenBonus) {}

void CBuffClarity::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->AddBonusManaRegen(regenBonus_);
}

void CBuffClarity::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    if (target) target->AddBonusManaRegen(-regenBonus_);
}

CBuff* CBuffClarity::Clone() const {
    auto* copy = new CBuffClarity(duration_, regenBonus_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffHealingWard =====
CBuffHealingWard::CBuffHealingWard(float duration, float healPerTick, float radius)
    : CBuff(War3ID("Bhwd"), duration, true)
    , healPerTick_(healPerTick), radius_(radius) {
    SetPeriodic(true, 1.0f);
}

void CBuffHealingWard::OnPeriodic(CSimulation& game, CUnit* target) {
    if (!target || target->IsDead()) return;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        if (unit->GetPlayerIndex() != target->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - target->GetX();
        const float dy = unit->GetY() - target->GetY();
        if (dx * dx + dy * dy <= radius_ * radius_) {
            float life = unit->GetLife() + healPerTick_;
            if (life > unit->GetMaxLife()) life = unit->GetMaxLife();
            unit->SetLife(game, life);
        }
    }
}

CBuff* CBuffHealingWard::Clone() const {
    auto* copy = new CBuffHealingWard(duration_, healPerTick_, radius_);
    copy->remaining_ = remaining_;
    return copy;
}
