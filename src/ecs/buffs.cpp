#include "buffs.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"

// ===== CBuffSlow =====
CBuffSlow::CBuffSlow(float duration, float slowPercent, int casterHandle)
    : CBuff(War3ID("Bslo"), duration, false), slowPercent_(slowPercent) {
    casterHandleId_ = casterHandle;
}

void CBuffSlow::OnApply(CSimulation& game, CUnit* target) {
    (void)game;
    originalSpeed_ = target->GetSpeed();
    int reduction = (int)(originalSpeed_ * slowPercent_);
    target->SetSpeed(originalSpeed_ - reduction);
}

void CBuffSlow::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    target->SetSpeed(originalSpeed_);
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
    int bonus = (int)(target->GetSpeed() * speedBonus_);
    target->SetSpeed(target->GetSpeed() + bonus);
    // TODO: apply attack speed bonus
}

void CBuffBloodlust::OnRemove(CSimulation& game, CUnit* target) {
    (void)game;
    int bonus = (int)(target->GetSpeed() * speedBonus_ / (1.0f + speedBonus_));
    target->SetSpeed(target->GetSpeed() - bonus);
}

CBuff* CBuffBloodlust::Clone() const {
    auto* copy = new CBuffBloodlust(duration_, speedBonus_, attackSpeedBonus_);
    copy->remaining_ = remaining_;
    return copy;
}

// ===== CBuffThorns =====
CBuffThorns::CBuffThorns(float duration, float reflectPercent)
    : CBuff(War3ID("Bthr"), duration, true), reflectPercent_(reflectPercent) {}

void CBuffThorns::OnApply(CSimulation& game, CUnit* target) {
    (void)game; (void)target;
    // TODO: register damage reflection callback
}

void CBuffThorns::OnRemove(CSimulation& game, CUnit* target) {
    (void)game; (void)target;
    // TODO: unregister damage reflection callback
}

CBuff* CBuffThorns::Clone() const {
    auto* copy = new CBuffThorns(duration_, reflectPercent_);
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
    target->SetInvulnerable(false);
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
    (void)game; (void)target;
    // TODO: disable unit actions
}

void CBuffStun::OnRemove(CSimulation& game, CUnit* target) {
    (void)game; (void)target;
    // TODO: re-enable unit actions
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
