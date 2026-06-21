#include "combat_system.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/widget.h"
#include "ecs/player.h"
#include "audio/AudioSystem.h"
#include <cmath>
#include <random>
#include <algorithm>

static std::mt19937 s_combatRng(std::random_device{}());

static const AttackDefenseTable kTable = {
    { 1.0f, 1.0f, 1.5f, 1.0f, 0.7f, 1.0f, 0.7f, 1.0f },
    { 1.0f, 1.5f, 0.75f, 1.0f, 0.35f, 0.5f, 0.2f, 1.0f },
    { 1.0f, 1.0f, 0.5f, 1.0f, 1.5f, 1.0f, 0.5f, 1.0f },
    { 1.0f, 1.0f, 0.75f, 0.5f, 0.35f, 0.5f, 0.2f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 0.5f, 0.35f, 1.0f, 0.5f, 1.0f },
    { 1.0f, 1.0f, 0.75f, 0.5f, 0.35f, 0.5f, 0.2f, 1.0f }
};

static int DefenseTypeIndex(CDefenseType def) {
    switch (def) {
        case CDefenseType::NONE:      return 0;
        case CDefenseType::LIGHT:     return 1;
        case CDefenseType::MEDIUM:    return 2;
        case CDefenseType::HEAVY:     return 3;
        case CDefenseType::FORTIFIED: return 4;
        case CDefenseType::HERO:      return 5;
        case CDefenseType::DIVINE:    return 6;
        case CDefenseType::UNARMORED: return 7;
        default: return 0;
    }
}

CCombatSystem::CCombatSystem() {}

float CCombatSystem::GetAttackDefenseMultiplier(CAttackType atk, CDefenseType def) {
    int di = DefenseTypeIndex(def);
    if (di < 0 || di > 7) di = 0;
    switch (atk) {
        case CAttackType::NORMAL:       return kTable.normal[di];
        case CAttackType::PIERCE:       return kTable.pierce[di];
        case CAttackType::SIEGE:        return kTable.siege[di];
        case CAttackType::MAGIC:
        case CAttackType::SPELLS:       return kTable.magic[di];
        case CAttackType::CHAOS:        return kTable.chaos[di];
        case CAttackType::HERO:         return kTable.hero[di];
        default: return 1.0f;
    }
}

DamageResult CCombatSystem::CalculateDamage(CUnit* attacker, CWidget* target,
                                              bool isAttack, bool isRanged,
                                              CAttackType attackType, CDamageType damageType,
                                              float damageBase, float bonusDamage,
                                              int dice, int sidesPerDie) {
    DamageResult result;

    if (!target || target->IsDead()) return result;

    if (target->IsInvulnerable()) return result;

    // Check evasion
    if (isAttack && target != attacker) {
        CUnit* targetUnit = dynamic_cast<CUnit*>(target);
        if (targetUnit && CheckEvasion(targetUnit, attacker)) {
            result.isEvaded = true;
            return result;
        }
    }

    // Roll damage
    float rolledDamage = damageBase + bonusDamage;
    if (dice > 0 && sidesPerDie > 0) {
        for (int d = 0; d < dice; d++)
            rolledDamage += (float)((std::rand() % sidesPerDie) + 1);
    }

    // Attack type vs defense type multiplier
    CDefenseType targetDef = CDefenseType::NONE;
    CUnit* targetUnit = dynamic_cast<CUnit*>(target);
    if (targetUnit) {
        int defVal = targetUnit->GetDefense();
        (void)defVal;
    }
    if (isAttack) {
        result.multiplier = GetAttackDefenseMultiplier(attackType, targetDef);
        rolledDamage *= result.multiplier;
    }

    // Armor reduction
    float armorReduction = CalcArmorDamageReduction(targetUnit ? targetUnit->GetDefense() : 0, attacker ? attacker->GetUnitType()->GetLevel() : 0);
    if (armorReduction > 0.95f) armorReduction = 0.95f;
    rolledDamage *= (1.0f - armorReduction);
    result.damageBlocked = rolledDamage * armorReduction;

    if (rolledDamage < 0) rolledDamage = 0;

    result.damageDealt = rolledDamage;
    return result;
}

float CCombatSystem::ApplyDamage(CSimulation& simulation, CUnit* attacker, CWidget* target,
                                   bool isAttack, bool isRanged,
                                   CAttackType attackType, CDamageType damageType,
                                   const std::string& weaponSoundType,
                                   float damageBase, float bonusDamage,
                                   int dice, int sidesPerDie) {
    if (!target || target->IsDead()) return 0;

    auto result = CalculateDamage(attacker, target, isAttack, isRanged,
                                   attackType, damageType,
                                   damageBase, bonusDamage, dice, sidesPerDie);
    if (result.isEvaded || result.damageDealt <= 0) return 0;

    float oldLife = target->GetLife();
    float newLife = oldLife - result.damageDealt;
    target->SetLife(simulation, newLife);

    // Fire damage event
    auto& dmgCb = simulation.GetUnitDamageCallback();
    if (dmgCb) {
        int srcHandle = attacker ? attacker->GetHandleId() : -1;
        dmgCb(target->GetHandleId(), srcHandle, result.damageDealt);
    }

    (void)isRanged;

    // Play weapon sound
    auto* audio = simulation.GetAudioSystem();
    if (audio && !weaponSoundType.empty()) {
        audio->Play3D(weaponSoundType, target->GetX(), target->GetY() + 50.0f, 0, 0.7f);
    }

    return result.damageDealt;
}

float CCombatSystem::CalcArmorDamageReduction(int defense, int attackerLevel) {
    (void)attackerLevel;
    float d = (float)defense;
    return (d * 0.06f) / (1.0f + d * 0.06f);
}

bool CCombatSystem::CheckEvasion(CUnit* target, CUnit* attacker) {
    (void)target;
    (void)attacker;
    return false;
}

float CCombatSystem::ApplyCritical(float damage, float chance, float multiplier) {
    if (chance <= 0) return damage;
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    if (dist(s_combatRng) < chance) {
        return damage * multiplier;
    }
    return damage;
}
