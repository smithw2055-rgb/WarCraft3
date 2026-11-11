#include "widget.h"
#include "csimulation.h"
#include "unit.h"
#include "item.h"
#include "destructable.h"
#include "../audio/AudioSystem.h"
#include <cmath>

CWidget::CWidget(int handleId, float x, float y, float life, engine::world::WidgetKind kind)
    : handleId_(handleId), x_(x), y_(y), life_(life), maxLife_(life), widgetKind_(kind) {}

CUnit* CWidget::AsUnit() noexcept {
    return IsUnit() ? static_cast<CUnit*>(this) : nullptr;
}
const CUnit* CWidget::AsUnit() const noexcept {
    return IsUnit() ? static_cast<const CUnit*>(this) : nullptr;
}
CItem* CWidget::AsItem() noexcept {
    return IsItem() ? static_cast<CItem*>(this) : nullptr;
}
const CItem* CWidget::AsItem() const noexcept {
    return IsItem() ? static_cast<const CItem*>(this) : nullptr;
}
CDestructable* CWidget::AsDestructable() noexcept {
    return IsDestructable() ? static_cast<CDestructable*>(this) : nullptr;
}
const CDestructable* CWidget::AsDestructable() const noexcept {
    return IsDestructable() ? static_cast<const CDestructable*>(this) : nullptr;
}

void CWidget::SetLife(CSimulation& simulation, float life) {
    bool wasAlive = life_ > 0;
    life_ = life;
    if (life_ < 0) life_ = 0;
    if (wasAlive && life_ <= 0) {
        // Unit just died - fire death callback
        auto& cb = simulation.GetUnitDeathCallback();
        if (cb) cb(handleId_, -1, -1);
    }
}

float CWidget::Damage(CSimulation& simulation, CUnit* source,
                      bool isAttack, bool isRanged,
                      CAttackType attackType, CDamageType damageType,
                      const std::string& weaponSoundType,
                      float damage, float bonusDamage) {
    if (life_ <= 0) return 0;
    float totalDamage = damage + bonusDamage;
    float oldLife = life_;
    life_ -= totalDamage;
    if (life_ < 0) life_ = 0;
    // Fire damage event callback
    auto& dmgCb = simulation.GetUnitDamageCallback();
    if (dmgCb) {
        int srcHandle = source ? source->GetHandleId() : -1;
        dmgCb(handleId_, srcHandle, totalDamage);
    }

    (void)isAttack;
    (void)isRanged;
    (void)attackType;
    (void)damageType;

    // Play weapon/impact sound
    auto* audio = simulation.GetAudioSystem();
    if (audio && !weaponSoundType.empty()) {
        audio->Play3D(weaponSoundType, x_, y_ + 50.0f, 0, 0.5f);
    }

    // Death event
    if (oldLife > 0 && life_ <= 0) {
        auto& cb = simulation.GetUnitDeathCallback();
        int srcHandle = source ? source->GetHandleId() : -1;
        if (cb) cb(handleId_, srcHandle, -1);
    }

    return oldLife - life_;
}

double CWidget::DistanceSquared(float tx, float ty) const {
    double dx = tx - x_;
    double dy = ty - y_;
    return dx * dx + dy * dy;
}

double CWidget::Distance(float tx, float ty) const {
    return std::sqrt(DistanceSquared(tx, ty));
}

bool CWidget::CanBeTargetedBy(CSimulation& simulation, CUnit* source,
                               const std::vector<CTargetType>& targetsAllowed) const {
    (void)simulation;
    (void)source;
    (void)targetsAllowed;
    return !IsDead();
}
