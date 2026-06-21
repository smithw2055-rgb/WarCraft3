#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "resource/slk.h"

class CSimulation;
class CUnit;
class CPlayer;

// Upgrade effect type
enum class CUpgradeEffectType : uint8_t {
    ATTACK_DAMAGE,
    ATTACK_DICE,
    ATTACK_RANGE,
    ATTACK_SPEED,
    DEFENSE_BONUS,
    HIT_POINTS,
    HIT_POINTS_PCT,
    HIT_POINT_REGEN,
    MANA_POINTS,
    MANA_POINTS_PCT,
    MANA_REGEN,
    MOVE_SPEED,
    MOVE_SPEED_PCT,
    SIGHT_RADIUS,
    SPELL_LEVEL
};

// ============================================================
// CUpgradeEffect - base class for upgrade effects
// ============================================================
class CUpgradeEffect {
public:
    CUpgradeEffect(CUpgradeEffectType type, War3ID targetUnitType);
    virtual ~CUpgradeEffect() = default;

    CUpgradeEffectType GetType() const { return type_; }
    War3ID GetTargetUnitType() const { return targetUnitType_; }

    // Apply effect at given level
    virtual void Apply(CSimulation& game, CUnit* unit, int level) = 0;
    virtual void Unapply(CSimulation& game, CUnit* unit, int level) = 0;

    // Apply to all units of a player
    void ApplyToPlayer(CSimulation& game, int playerIndex, int level);
    void UnapplyFromPlayer(CSimulation& game, int playerIndex, int level);

protected:
    CUpgradeEffectType type_;
    War3ID targetUnitType_; // War3ID() means all units
};

// ===== Concrete upgrade effects =====

class CUpgradeEffectAttackDamage : public CUpgradeEffect {
public:
    CUpgradeEffectAttackDamage(War3ID target, int baseValue, int perLevel);
    void Apply(CSimulation& game, CUnit* unit, int level) override;
    void Unapply(CSimulation& game, CUnit* unit, int level) override;
private:
    int baseValue_, perLevel_;
};

class CUpgradeEffectDefense : public CUpgradeEffect {
public:
    CUpgradeEffectDefense(War3ID target, int baseValue, int perLevel);
    void Apply(CSimulation& game, CUnit* unit, int level) override;
    void Unapply(CSimulation& game, CUnit* unit, int level) override;
private:
    int baseValue_, perLevel_;
};

class CUpgradeEffectHitPoints : public CUpgradeEffect {
public:
    CUpgradeEffectHitPoints(War3ID target, int baseValue, int perLevel);
    void Apply(CSimulation& game, CUnit* unit, int level) override;
    void Unapply(CSimulation& game, CUnit* unit, int level) override;
private:
    int baseValue_, perLevel_;
};

class CUpgradeEffectMoveSpeed : public CUpgradeEffect {
public:
    CUpgradeEffectMoveSpeed(War3ID target, int baseValue, int perLevel);
    void Apply(CSimulation& game, CUnit* unit, int level) override;
    void Unapply(CSimulation& game, CUnit* unit, int level) override;
private:
    int baseValue_, perLevel_;
};

class CUpgradeEffectAttackRange : public CUpgradeEffect {
public:
    CUpgradeEffectAttackRange(War3ID target, int baseValue, int perLevel);
    void Apply(CSimulation& game, CUnit* unit, int level) override;
    void Unapply(CSimulation& game, CUnit* unit, int level) override;
private:
    int baseValue_, perLevel_;
};
