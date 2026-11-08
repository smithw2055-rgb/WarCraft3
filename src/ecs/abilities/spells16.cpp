#include "spells16.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "ecs/unit_type.h"
#include "data/abilitydata.h"
#include <cmath>

// ===== Devour =====
CAbilityDevour::CAbilityDevour(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Adev"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(100.0f);
}

void CAbilityDevour::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    ABActionDevourTarget devour(digestDuration_, digestPerTick_);
    devour.Run(game, caster, target, false, 0, 0);
}

void CAbilityDevour::ApplyEffectData(const CAbilityDefFull& def, int level) {
    digestPerTick_ = def.DataFloatAt(level, 0, digestPerTick_);
}

// ===== Devour Magic =====
CAbilityDevourMagic::CAbilityDevourMagic(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Advm"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(6.0f);
    SetManaCost(50);
}

void CAbilityDevourMagic::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    ABActionDevourMagic devour(manaPerBuff_);
    devour.Run(game, caster, target, false, 0, 0);
}

// ===== Cold Arrows =====
CAbilityColdArrows::CAbilityColdArrows(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHca"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

namespace {

class ColdArrowsEffect : public CAttackModifierEffect {
public:
    ColdArrowsEffect(float slowDuration, float slowPercent,
                     float poisonDuration, float poisonPerTick)
        : slowDuration_(slowDuration), slowPercent_(slowPercent),
          poisonDuration_(poisonDuration), poisonPerTick_(poisonPerTick) {}
    void OnAttackLanded(CSimulation& game, CUnit* attacker, CUnit* victim,
                        float damageDealt) override {
        (void)damageDealt;
        if (!victim) return;
        const int caster = attacker ? attacker->GetHandleId() : -1;
        victim->AddBuff(std::make_unique<CBuffSlow>(slowDuration_, slowPercent_, caster), game);
        victim->AddBuff(std::make_unique<CBuffPoison>(poisonDuration_, poisonPerTick_, caster), game);
    }
private:
    float slowDuration_;
    float slowPercent_;
    float poisonDuration_;
    float poisonPerTick_;
};

} // namespace

void CAbilityColdArrows::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->SetAttackModifier(0, GetAlias(),
        std::make_unique<ColdArrowsEffect>(slowDuration_, slowPercent_,
                                           poisonDuration_, poisonPerTick_));
}

void CAbilityColdArrows::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->ClearAttackModifier(0, GetAlias());
}

void CAbilityColdArrows::ApplyEffectData(const CAbilityDefFull& def, int level) {
    poisonPerTick_ = def.DataFloatAt(level, 0, poisonPerTick_);
    slowPercent_ = def.DataFloatAt(level, 1, slowPercent_);
    slowDuration_ = def.DurationAt(level);
    poisonDuration_ = def.DataFloatAt(level, 3, poisonDuration_);
}

// ===== Monsoon =====
CAbilityMonsoon::CAbilityMonsoon(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANmo"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(500.0f);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityMonsoon::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    CastPointDamageWaves(game, caster, x, y, waves_, radius_, damagePerWave_,
                         CAttackType::SPELLS, CDamageType::COLD,
                         ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
}

void CAbilityMonsoon::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damagePerWave_ = def.DataFloatAt(level, 0, damagePerWave_);
    radius_ = def.AreaAt(level);
}

// ===== Absorb Mana =====
CAbilityAbsorbMana::CAbilityAbsorbMana(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aabs"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
}

void CAbilityAbsorbMana::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit || !targetUnit->IsSummoned()) return;
    const float mana = std::min(caster->GetMana() + manaGain_, static_cast<float>(caster->GetMaxMana()));
    ABActionKillTarget kill;
    kill.Run(game, caster, target, false, 0, 0);
    ABActionSetManaTarget refund(mana);
    refund.Run(game, caster, caster, false, 0, 0);
}

// ===== Unholy Frenzy =====
CAbilityUnholyFrenzy::CAbilityUnholyFrenzy(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Auhf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityUnholyFrenzy::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_, attack = attackSpeedMul_]() {
        return std::make_unique<CBuffBerserk>(duration, attack, 1.0f);
    });
}
