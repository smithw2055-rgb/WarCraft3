#pragma once
#include "ecs/ability.h"

// Aura effect type
enum class CAuraEffectType : uint8_t {
    MOVE_SPEED, ATTACK_SPEED, ARMOR, DAMAGE, LIFE_REGEN, MANA_REGEN
};

// Aura bonus definition
struct CAuraBonus {
    CAuraEffectType type = CAuraEffectType::MOVE_SPEED;
    float value = 0;
    bool isPercent = false;
};

// CAbilityAura - passive aura that affects nearby units
class CAbilityAura : public CAbility {
public:
    CAbilityAura(int handleId, War3ID code);

    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    void OnTick(CSimulation& game, CUnit* unit) override;

    // Configuration
    float GetRadius() const { return radius_; }
    void SetRadius(float v) { radius_ = v; }
    bool AffectsAllies() const { return affectsAllies_; }
    void SetAffectsAllies(bool v) { affectsAllies_ = v; }
    bool AffectsSelf() const { return affectsSelf_; }
    void SetAffectsSelf(bool v) { affectsSelf_ = v; }
    bool IsEnemyOnly() const { return enemyOnly_; }
    void SetEnemyOnly(bool v) { enemyOnly_ = v; }

    // Bonus management
    void AddBonus(CAuraBonus bonus) { bonuses_.push_back(bonus); }
    const std::vector<CAuraBonus>& GetBonuses() const { return bonuses_; }

private:
    void ApplyAura(CSimulation& game, CUnit* target);
    void RemoveAura(CSimulation& game, CUnit* target);

    float radius_ = 900.0f;
    bool affectsAllies_ = true;
    bool affectsSelf_ = true;
    bool enemyOnly_ = false;
    std::vector<CAuraBonus> bonuses_;

    // Track which units currently have the aura applied
    std::vector<int> affectedUnitHandles_;
};
