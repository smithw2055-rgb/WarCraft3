#include "spells10.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include <cmath>

// ===== Cleaving Attack =====
CAbilityCleavingAttack::CAbilityCleavingAttack(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANca"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

// ===== Searing Arrows =====
CAbilitySearingArrows::CAbilitySearingArrows(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHfa"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

namespace {

class SearingArrowsEffect : public CAttackModifierEffect {
public:
    explicit SearingArrowsEffect(float bonusDamage) : bonusDamage_(bonusDamage) {}
    void OnAttackLanded(CSimulation& game, CUnit* attacker, CUnit* victim,
                        float damageDealt) override {
        (void)damageDealt;
        if (!victim) return;
        victim->Damage(game, attacker, false, false,
                       CAttackType::SPELLS, CDamageType::FIRE,
                       "", bonusDamage_, 0);
    }
private:
    float bonusDamage_;
};

} // namespace

void CAbilitySearingArrows::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->SetAttackModifier(0, War3ID("AHfa"),
        std::make_unique<SearingArrowsEffect>(bonusDamage_));
}

void CAbilitySearingArrows::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->ClearAttackModifier(0, War3ID("AHfa"));
}

// ===== Ensnare =====
CAbilityEnsnare::CAbilityEnsnare(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aens"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(500.0f);
    SetCooldown(8.0f);
    SetManaCost(50);
}

void CAbilityEnsnare::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto a single AbilityBuilder buff attach.
    ABActionAddBuffTarget snare(
        [duration = duration_]() { return std::make_unique<CBuffEnsnare>(duration); });
    snare.Run(game, caster, target, false, 0, 0);
}

// ===== Ancestral Spirit =====
CAbilityAncestralSpirit::CAbilityAncestralSpirit(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aast"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(500.0f);
    SetCooldown(12.0f);
    SetManaCost(75);
}

void CAbilityAncestralSpirit::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto a single AbilityBuilder tauren revive (ownership
    // transfer included).
    ABActionAncestralRevive spirit;
    spirit.Run(game, caster, target, false, 0, 0);
}

// ===== Healing Ward (unit) =====
CAbilityHealingWardUnit::CAbilityHealingWardUnit(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Ahwd"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(500.0f);
    SetCooldown(8.0f);
    SetManaCost(50);
}

void CAbilityHealingWardUnit::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    // Migrated onto a single AbilityBuilder create-and-buff (same shape as
    // the item Healing Ward).
    if (!caster) return;
    ABActionCreateUnitWithBuff ward(
        wardType_, x, y,
        {ABActionCreateUnitWithBuff::BuffFactory(
            [duration = auraDuration_, tick = healPerTick_, radius = radius_]() {
                return std::make_unique<CBuffHealingWard>(duration, tick, radius);
            })});
    ward.Run(game, caster, nullptr, true, x, y);
}

// ===== Unstable Concoction =====
CAbilityUnstableConcoction::CAbilityUnstableConcoction(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Auco"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityUnstableConcoction::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    // Migrated onto an AbilityBuilder iteration (enemies around the caster)
    // plus a self-hit. Same coverage as the loop replaced.
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDamageTarget>(
        damage_, CAttackType::SPELLS, CDamageType::FIRE));
    ABActionForEachInRange blast(radius_, std::move(perUnit));
    blast.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    blast.Run(game, caster, nullptr, false, 0, 0);
    ABActionDamageTarget selfHit(selfDamage_, CAttackType::SPELLS, CDamageType::FIRE);
    selfHit.Run(game, caster, caster, false, 0, 0);
}

// ===== Abolish Magic =====
CAbilityAbolishMagic::CAbilityAbolishMagic(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aadm"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
    SetManaCost(75);
}

void CAbilityAbolishMagic::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    // Migrated onto an AbilityBuilder iteration (dispel per unit in radius;
    // caster excluded via includeCaster=false, matching the loop replaced).
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDispelTarget>(true, true));
    ABActionForEachInRange strip(radius_, std::move(perUnit));
    strip.Run(game, caster, nullptr, true, x, y);
}

// ===== Cluster Rockets =====
CAbilityClusterRockets::CAbilityClusterRockets(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANcs"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(800.0f);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityClusterRockets::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    // Two clusters straddling the target point (ref twin-rocket spread).
    // Migrated onto two AbilityBuilder iterations centered at (x-75,y) and
    // (x+75,y). Same enemy-only FIRE coverage as the loops replaced.
    ABEffectChain westHit;
    westHit.Add(std::make_unique<ABActionDamageTarget>(
        damage_, CAttackType::SPELLS, CDamageType::FIRE));
    ABActionForEachInRange west(radius_, std::move(westHit));
    west.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    west.Run(game, caster, nullptr, true, x - 75.0f, y);
    ABEffectChain eastHit;
    eastHit.Add(std::make_unique<ABActionDamageTarget>(
        damage_, CAttackType::SPELLS, CDamageType::FIRE));
    ABActionForEachInRange east(radius_, std::move(eastHit));
    east.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    east.Run(game, caster, nullptr, true, x + 75.0f, y);
}
