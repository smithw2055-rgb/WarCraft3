#include "combat_system.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/unit_attack.h"
#include "ecs/widget.h"
#include "ecs/player.h"
#include "audio/AudioSystem.h"
#include <cmath>
#include <random>
#include <algorithm>

static std::mt19937 s_combatRng(std::random_device{}());

// Attack-type vs defense-type multipliers, indexed by DefenseTypeIndex()
// below: NONE, LIGHT, MEDIUM, HEAVY, FORTIFIED, HERO, DIVINE, UNARMORED.
// Default values are the TFT data (Units\MiscGame.txt DamageBonus* rows,
// Small,Medium,Large,Fort,Normal,Hero,Divine,None column order); the same
// values are produced by CGameplayConstants when the data files are present,
// and CCombatSystem::Configure() replaces this table with the loaded one
// (e.g. when a map overrides constants via war3mapMisc.txt).
static const AttackDefenseTable kTable = {
    // Normal: 100/150/100/70/100/100/5/100
    { 1.0f, 1.0f, 1.5f, 1.0f, 0.7f, 1.0f, 0.05f, 1.0f },
    // Pierce: 200/75/100/35/50/5/150
    { 1.0f, 2.0f, 0.75f, 1.0f, 0.35f, 0.5f, 0.05f, 1.5f },
    // Siege: 100/50/100/150/50/5/150
    { 1.0f, 1.0f, 0.5f, 1.0f, 1.5f, 0.5f, 0.05f, 1.5f },
    // Magic: 125/75/200/35/50/5/100
    { 1.0f, 1.25f, 0.75f, 2.0f, 0.35f, 0.5f, 0.05f, 1.0f },
    // Chaos: full damage to everything
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    // Hero: reduced vs Fortified only
    { 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 1.0f, 0.05f, 1.0f },
    // Spells: 100% except Hero 70%, Divine 5%
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.7f, 0.05f, 1.0f }
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

CCombatSystem::CCombatSystem() : table_(kTable) {}

void CCombatSystem::Configure(float defenseArmor, const AttackDefenseTable& table) {
    defenseArmor_ = defenseArmor;
    table_ = table;
}

static float LookupRow(const float row[8], int di) {
    if (di < 0 || di > 7) di = 0;
    return row[di];
}

float CCombatSystem::GetConfiguredMultiplier(CAttackType atk, CDefenseType def) const {
    int di = DefenseTypeIndex(def);
    switch (atk) {
        case CAttackType::NORMAL:       return LookupRow(table_.normal, di);
        case CAttackType::PIERCE:       return LookupRow(table_.pierce, di);
        case CAttackType::SIEGE:        return LookupRow(table_.siege, di);
        case CAttackType::MAGIC:        return LookupRow(table_.magic, di);
        case CAttackType::SPELLS:       return LookupRow(table_.spells, di);
        case CAttackType::CHAOS:        return LookupRow(table_.chaos, di);
        case CAttackType::HERO:         return LookupRow(table_.hero, di);
        default: return 1.0f;
    }
}

float CCombatSystem::GetAttackDefenseMultiplier(CAttackType atk, CDefenseType def) {
    int di = DefenseTypeIndex(def);
    if (di < 0 || di > 7) di = 0;
    switch (atk) {
        case CAttackType::NORMAL:       return kTable.normal[di];
        case CAttackType::PIERCE:       return kTable.pierce[di];
        case CAttackType::SIEGE:        return kTable.siege[di];
        case CAttackType::MAGIC:        return kTable.magic[di];
        case CAttackType::SPELLS:       return kTable.spells[di];
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
    (void)attacker; // reserved for future evasion/level hooks
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

    // Roll damage (bonus damage is part of the base, like ref computeFinalDamage)
    float rolledDamage = damageBase + bonusDamage;
    if (dice > 0 && sidesPerDie > 0) {
        for (int d = 0; d < dice; d++)
            rolledDamage += (float)((std::rand() % sidesPerDie) + 1);
    }

    // Attack-type vs defense-type multiplier, always applied (ref CUnit.damage).
    // Destructables/items have no armor type; they use NONE (1.0x everywhere).
    CDefenseType targetDef = CDefenseType::NONE;
    CUnit* targetUnit = dynamic_cast<CUnit*>(target);
    int targetArmor = 0;
    if (targetUnit) {
        targetDef = targetUnit->GetDefenseType();
        targetArmor = targetUnit->GetTotalDefense();
    }
    result.multiplier = GetConfiguredMultiplier(attackType, targetDef);
    rolledDamage *= result.multiplier;

    // Critical strike (ref permanent-crit abilities): multiplies the damage
    // after the attack/defense matchup, before armor reduction.
    if (isAttack && attacker && attacker != targetUnit) {
        const float chance = attacker->GetCritChance();
        if (chance > 0.0f) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(s_combatRng) < chance) {
                rolledDamage *= std::max(1.0f, attacker->GetCritMultiplier());
                result.isCritical = true;
            }
        }
    }

    // Armor value only matters for Normal damage type (ref: everything else,
    // including spells and enhanced physical, ignores armor value).
    float armorReduction = 0.0f;
    if (damageType == CDamageType::NORMAL) {
        armorReduction = CalcArmorDamageReduction(targetArmor, 0, defenseArmor_);
        if (armorReduction > 0.95f) armorReduction = 0.95f;
    }
    result.damageBlocked = rolledDamage * armorReduction;
    rolledDamage *= (1.0f - armorReduction);

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

float CCombatSystem::ApplySplashDamage(CSimulation& simulation, CUnit* attacker, CWidget* target,
                                       bool isAttack, bool isRanged,
                                       CAttackType attackType, CDamageType damageType,
                                       const std::string& weaponSoundType,
                                       float damageBase, float bonusDamage,
                                       int dice, int sidesPerDie,
                                       const CUnitAttack& attack) {
    float total = ApplyDamage(simulation, attacker, target, isAttack, isRanged,
                              attackType, damageType, weaponSoundType,
                              damageBase, bonusDamage, dice, sidesPerDie);
    if (!target || attack.areaHalf <= 0.0f) {
        return total;
    }
    const float cx = target->GetX();
    const float cy = target->GetY();
    const float fullSq = attack.areaFull * attack.areaFull;
    const float halfSq = attack.areaHalf * attack.areaHalf;
    const float quarterSq = attack.areaQuarter * attack.areaQuarter;

    CPlayer* ownerPlayer = nullptr;
    if (attacker) {
        ownerPlayer = simulation.GetPlayer(attacker->GetPlayerIndex());
    }
    for (CUnit* other : simulation.GetUnits()) {
        if (!other || other == target || other == attacker || other->IsDead()) continue;
        // Allies (and the attacker itself) are immune to friendly splash.
        bool isAlly = false;
        if (attacker) {
            if (ownerPlayer) {
                if (CPlayer* otherPlayer = simulation.GetPlayer(other->GetPlayerIndex())) {
                    isAlly = ownerPlayer->HasAlliance(*otherPlayer, CAllianceType::ALLIED);
                }
            } else {
                isAlly = other->GetPlayerIndex() == attacker->GetPlayerIndex();
            }
        }
        if (isAlly) continue;
        const float dx = other->GetX() - cx;
        const float dy = other->GetY() - cy;
        const float distSq = dx * dx + dy * dy;
        float factor = 0.0f;
        if (distSq <= fullSq) {
            factor = 1.0f; // inside the full-damage area alongside the primary
        } else if (distSq <= halfSq) {
            factor = attack.damageFactorHalf;
        } else if (attack.areaQuarter > 0.0f && distSq <= quarterSq) {
            factor = attack.damageFactorQuarter;
        } else {
            continue;
        }
        // Ring falloff scales the base; instance bonuses apply in full.
        total += ApplyDamage(simulation, attacker, other, isAttack, isRanged,
                             attackType, damageType, "",
                             damageBase * factor, bonusDamage, dice, sidesPerDie);
    }
    return total;
}

float CCombatSystem::CalcArmorDamageReduction(int defense, int attackerLevel, float defenseArmor) {
    // Single source of truth lives in CDefenseTypeInfo (ref armor formula
    // including the negative-armor branch); kept here for API compatibility.
    return CDefenseTypeInfo::CalcDamageReduction(defense, attackerLevel, defenseArmor);
}

bool CCombatSystem::CheckEvasion(CUnit* target, CUnit* attacker) {
    (void)attacker;
    if (!target) return false;
    const float chance = target->GetEvasionChance();
    if (chance <= 0.0f) return false;
    if (chance >= 1.0f) return true;
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(s_combatRng) < chance;
}

float CCombatSystem::ApplyCritical(float damage, float chance, float multiplier) {
    if (chance <= 0) return damage;
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    if (dist(s_combatRng) < chance) {
        return damage * multiplier;
    }
    return damage;
}
