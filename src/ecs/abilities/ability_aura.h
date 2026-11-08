#pragma once
#include "ecs/ability.h"
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

class CBuff;

// CAbilityAura - passive aura that affects nearby units
// (CAuraEffectType and CAuraBonus are defined in ecs/ability.h)
// Phase 20 Stage WW: Supports both stat-bonus auras and periodic buff-refresh
// auras (`SetAuraBuff`) without requiring subclass ApplyAura/RemoveAura overrides,
// and packs boolean flags into compact 1-bit bitfields.
class CAbilityAura : public CAbility {
public:
    using AuraBuffFactory = std::function<std::unique_ptr<CBuff>()>;

    CAbilityAura(int handleId, War3ID code);
    CAbilityAura(int handleId, War3ID code, float radius,
                 bool affectsAllies, bool affectsSelf, bool enemyOnly,
                 std::initializer_list<CAuraBonus> bonuses = {});

    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    void OnTick(CSimulation& game, CUnit* unit) override;

    // Configuration
    float GetRadius() const { return radius_; }
    void SetRadius(float v) { radius_ = v; }
    bool AffectsAllies() const { return affectsAllies_ != 0; }
    void SetAffectsAllies(bool v) { affectsAllies_ = v ? 1u : 0u; }
    bool AffectsSelf() const { return affectsSelf_ != 0; }
    void SetAffectsSelf(bool v) { affectsSelf_ = v ? 1u : 0u; }
    bool IsEnemyOnly() const { return enemyOnly_ != 0; }
    void SetEnemyOnly(bool v) { enemyOnly_ = v ? 1u : 0u; }

    // Bonus & periodic buff management
    void AddBonus(CAuraBonus bonus) { bonuses_.push_back(bonus); }
    const std::vector<CAuraBonus>& GetBonuses() const { return bonuses_; }
    void SetAuraBuff(War3ID buffId, AuraBuffFactory factory) {
        auraBuffId_ = buffId;
        auraBuffFactory_ = std::move(factory);
    }
    War3ID GetAuraBuffId() const { return auraBuffId_; }

protected:
    virtual void ApplyAura(CSimulation& game, CUnit* caster, CUnit* target);
    virtual void RemoveAura(CSimulation& game, CUnit* caster, CUnit* target);

    float radius_ = 900.0f;
    uint8_t affectsAllies_ : 1;
    uint8_t affectsSelf_   : 1;
    uint8_t enemyOnly_     : 1;
    War3ID auraBuffId_  = War3ID(0u);
    AuraBuffFactory auraBuffFactory_;
    std::vector<CAuraBonus> bonuses_;

    // Track which units currently have the aura applied
    std::vector<int> affectedUnitHandles_;
};
