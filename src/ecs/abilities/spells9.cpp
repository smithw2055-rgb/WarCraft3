#include "spells9.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "data/abilitydata.h"
#include <cmath>

// ===== Orb of Frost =====
CAbilityOrbOfFrost::CAbilityOrbOfFrost(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIob"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

namespace {

class OrbFrostEffect : public CAttackModifierEffect {
public:
    OrbFrostEffect(float duration, float slowPercent)
        : duration_(duration), slowPercent_(slowPercent) {}
    void OnAttackLanded(CSimulation& game, CUnit* attacker, CUnit* victim,
                        float damageDealt) override {
        (void)damageDealt;
        if (!victim) return;
        victim->AddBuff(std::make_unique<CBuffSlow>(duration_, slowPercent_,
                                                    attacker ? attacker->GetHandleId() : -1),
                        game);
    }
private:
    float duration_;
    float slowPercent_;
};

class OrbCorruptionEffect : public CAttackModifierEffect {
public:
    OrbCorruptionEffect(float duration, int armorPenalty)
        : duration_(duration), armorPenalty_(armorPenalty) {}
    void OnAttackLanded(CSimulation& game, CUnit* attacker, CUnit* victim,
                        float damageDealt) override {
        (void)attacker;
        (void)damageDealt;
        if (!victim) return;
        victim->AddBuff(std::make_unique<CBuffArmor>(War3ID("AIcb"), duration_, armorPenalty_, false),
                        game);
    }
private:
    float duration_;
    int armorPenalty_;
};

} // namespace

void CAbilityOrbOfFrost::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->SetAttackModifier(0, War3ID("AIob"),
        std::make_unique<OrbFrostEffect>(duration_, slowPercent_));
}

void CAbilityOrbOfFrost::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->ClearAttackModifier(0, War3ID("AIob"));
}

// ===== Orb of Corruption =====
CAbilityOrbOfCorruption::CAbilityOrbOfCorruption(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIcb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityOrbOfCorruption::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->SetAttackModifier(0, War3ID("AIcb"),
        std::make_unique<OrbCorruptionEffect>(duration_, armorPenalty_));
}

void CAbilityOrbOfCorruption::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->ClearAttackModifier(0, War3ID("AIcb"));
}

// ===== Orb of Fire =====
CAbilityOrbOfFire::CAbilityOrbOfFire(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIfb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityOrbOfFire::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->SetAttackModifier(0, War3ID("AIfb"),
        std::make_unique<CAttackBonusDamageEffect>(bonusDamage_));
}

void CAbilityOrbOfFire::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->ClearAttackModifier(0, War3ID("AIfb"));
}

// ===== Wand of Illusion =====
CAbilityIllusion::CAbilityIllusion(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIil"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
}

void CAbilityIllusion::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    ABActionSummonCopyOfTarget illusion(War3ID("AIil"), duration_);
    illusion.Run(game, caster, target, false, 0, 0);
}

// ===== Wand of Lightning Shield =====
CAbilityLightningShieldWand::CAbilityLightningShieldWand(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIls"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
}

void CAbilityLightningShieldWand::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target,
                     [duration = duration_, tick = damagePerTick_, radius = radius_]() {
                         return std::make_unique<CBuffLightningShield>(duration, tick, radius);
                     });
}

// ===== Scroll of the Beast =====
CAbilityBeastScroll::CAbilityBeastScroll(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIrr"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityBeastScroll::DoEffect(CSimulation& game, CUnit* caster) {
    CastCasterAreaBuff(game, caster, radius_,
                       [duration = duration_, damage = bonusDamage_]() {
                           return std::make_unique<CBuffInnerFire>(duration, damage, 0);
                       });
}

// ===== Healing Wards =====
CAbilityHealingWard::CAbilityHealingWard(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIhw"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(500.0f);
}

void CAbilityHealingWard::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABActionCreateUnitWithBuff ward(
        wardType_, x, y,
        {ABActionCreateUnitWithBuff::BuffFactory(
            [duration = auraDuration_, tick = healPerTick_, radius = radius_]() {
                return std::make_unique<CBuffHealingWard>(duration, tick, radius);
            })});
    ward.Run(game, caster, nullptr, true, x, y);
}
