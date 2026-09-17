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
