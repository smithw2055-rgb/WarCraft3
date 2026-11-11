#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "resource/slk.h"
#include "data/combat_types.h"
#include "engine/world/widget_kind.h"

class CSimulation;
class CUnit;
class CDestructable;
class CDestructable;
class CItem;
class CPlayer;

// Ability target interface (anything that can be targeted by abilities/orders)
class AbilityTarget {
public:
    virtual ~AbilityTarget() = default;
    virtual float GetX() const = 0;
    virtual float GetY() const = 0;
    virtual float GetZ() const = 0;
};

// Widget base class - corresponds to Java's CWidget
class CWidget : public AbilityTarget {
public:
    CWidget(int handleId, float x, float y, float life,
            engine::world::WidgetKind kind = engine::world::WidgetKind::Base);
    virtual ~CWidget() = default;

    int GetHandleId() const { return handleId_; }
    float GetX() const override { return x_; }
    float GetY() const override { return y_; }
    float GetZ() const override { return flyHeight_; }
    float GetLife() const { return life_; }
    float GetMaxLife() const { return maxLife_; }
    void SetMaxLife(float v) { maxLife_ = v; }

    virtual void SetLife(CSimulation& simulation, float life);
    virtual float Damage(CSimulation& simulation, CUnit* source,
                         bool isAttack, bool isRanged,
                         CAttackType attackType, CDamageType damageType,
                         const std::string& weaponSoundType,
                         float damage, float bonusDamage = 0);

    virtual float GetFlyHeight() const { return flyHeight_; }
    virtual float GetImpactZ() const { return 0; }

    bool IsDead() const { return life_ <= 0; }

    double DistanceSquared(float tx, float ty) const;
    double Distance(float tx, float ty) const;

    virtual bool CanBeTargetedBy(CSimulation& simulation, CUnit* source,
                                 const std::vector<CTargetType>& targetsAllowed) const;

    virtual bool IsInvulnerable() const { return false; }

    engine::world::WidgetKind GetWidgetKind() const noexcept { return widgetKind_; }
    bool IsUnit() const noexcept { return widgetKind_ == engine::world::WidgetKind::Unit; }
    bool IsItem() const noexcept { return widgetKind_ == engine::world::WidgetKind::Item; }
    bool IsDestructable() const noexcept { return widgetKind_ == engine::world::WidgetKind::Destructable; }

    CUnit* AsUnit() noexcept;
    const CUnit* AsUnit() const noexcept;
    CItem* AsItem() noexcept;
    const CItem* AsItem() const noexcept;
    CDestructable* AsDestructable() noexcept;
    const CDestructable* AsDestructable() const noexcept;

protected:
    void SetX(float x) { x_ = x; }
    void SetY(float y) { y_ = y; }
    void SetFlyHeight(float h) { flyHeight_ = h; }

    int handleId_;
    float x_;
    float y_;
    float flyHeight_ = 0;
    float life_;
    float maxLife_ = 100;
    engine::world::WidgetKind widgetKind_ = engine::world::WidgetKind::Base;
};
