#include "combat_system.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/unit_attack.h"
#include "ecs/widget.h"
#include "ecs/player.h"
#include "audio/AudioSystem.h"
#include <cmath>
#include <algorithm>

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

void CCombatSystem::ConfigureMagicRules(float etherealBonusMagic, float etherealBonusSpells,
                                        bool magicImmuneResistsDamage,
                                        bool magicImmuneResistsUltimates,
                                        bool etherealBonusAlly) {
    etherealBonusMagic_ = etherealBonusMagic;
    etherealBonusSpells_ = etherealBonusSpells;
    magicImmuneResistsDamage_ = magicImmuneResistsDamage;
    magicImmuneResistsUltimates_ = magicImmuneResistsUltimates;
    etherealBonusAlly_ = etherealBonusAlly;
}

float CCombatSystem::CombatRand01() {
    // Same Numerical Recipes LCG as CSimulation::SimRand01.
    rngState_ = rngState_ * 1664525u + 1013904223u;
    return (float)(rngState_ >> 16) * (1.0f / 65536.0f);
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

    // Magic immunity (ref CUnitDefaultMagicImmuneDamageModListener): MAGIC
    // attacks fizzle when MagicImmunesResistDamage; SPELLS fizzle unless
    // UNIVERSAL damage with MagicImmunesResistUltimates off. The inner
    // conjunction is && (ref): UNIVERSAL always pierces, and non-universal
    // SPELLS pass while the ultimates gate is off.
    {
        CUnit* targetUnit = dynamic_cast<CUnit*>(target);
        if (targetUnit && targetUnit->IsMagicImmune()) {
            if (attackType == CAttackType::MAGIC && magicImmuneResistsDamage_) {
                return result;
            }
            if (attackType == CAttackType::SPELLS && damageType != CDamageType::NORMAL) {
                if (damageType != CDamageType::UNIVERSAL && magicImmuneResistsUltimates_) {
                    return result;
                }
            }
        }
    }

    // Ethereal bonus multiplier. Physical (NORMAL damage from a
    // non-MAGIC attack) is negated. Allied casters skip the bonus unless
    // EtherealDamageBonusAlly is set.
    float etherealMultiplier = 1.0f;
    bool etherealNegate = false;
    {
        CUnit* targetUnit = dynamic_cast<CUnit*>(target);
        if (targetUnit && targetUnit->IsEthereal()) {
            const bool allied = attacker && attacker->GetPlayerIndex() == targetUnit->GetPlayerIndex();
            const bool grantBonus = !allied || etherealBonusAlly_;
            if (grantBonus && attackType == CAttackType::MAGIC) {
                etherealMultiplier = etherealBonusMagic_;
            } else if (grantBonus && attackType == CAttackType::SPELLS) {
                etherealMultiplier = etherealBonusSpells_;
            }
            if (damageType == CDamageType::NORMAL && attackType != CAttackType::MAGIC) {
                etherealNegate = true;
            }
        }
    }
    if (etherealNegate) return result;

    // Roll damage (bonus damage is part of the base, like ref computeFinalDamage)
    float rolledDamage = damageBase + bonusDamage;
    if (dice > 0 && sidesPerDie > 0) {
        for (int d = 0; d < dice; d++)
            rolledDamage += (float)(1 + (int)(CombatRand01() * (float)sidesPerDie));
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
    rolledDamage *= etherealMultiplier;

    // Critical strike (ref permanent-crit abilities): every source rolls
    // independently; when several proc, the strongest multiplier wins
    // (they never multiply together). Applied after the attack/defense
    // matchup, before armor reduction.
    if (isAttack && attacker && attacker != targetUnit) {
        float bestMultiplier = 1.0f;
        bool procced = false;
        for (const auto& src : attacker->GetCritSources()) {
            if (src.chance > 0.0f && CombatRand01() < src.chance) {
                procced = true;
                if (src.multiplier > bestMultiplier) bestMultiplier = src.multiplier;
            }
        }
        if (procced) {
            rolledDamage *= std::max(1.0f, bestMultiplier);
            result.isCritical = true;
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
    // Uphill accuracy (ref CUnitDefaultAccuracyCheckListener; same gate as
    // CUnit::Damage so splash/JASS paths share the retail miss rule).
    if (simulation.CheckHighlandMiss(attacker, target, isAttack)) {
        simulation.FireCombatText(attacker ? attacker->GetHandleId() : -1, target->GetHandleId(),
                                  CSimulation::CombatTextKind::MISS, 0.0f);
        return 0;
    }

    auto result = CalculateDamage(attacker, target, isAttack, isRanged,
                                   attackType, damageType,
                                   damageBase, bonusDamage, dice, sidesPerDie);
    if (result.isEvaded) {
        simulation.FireCombatText(attacker ? attacker->GetHandleId() : -1, target->GetHandleId(),
                                  CSimulation::CombatTextKind::MISS, 0.0f);
        return 0;
    }
    if (result.damageDealt <= 0) return 0;
    if (result.isCritical) {
        simulation.FireCombatText(attacker ? attacker->GetHandleId() : -1, target->GetHandleId(),
                                  CSimulation::CombatTextKind::CRIT, result.damageDealt);
    }

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
    total += ApplySplashDamageAtPoint(simulation, attacker, target->GetX(), target->GetY(),
                                      isAttack, isRanged, attackType, damageType,
                                      damageBase, bonusDamage, dice, sidesPerDie, attack, target);
    return total;
}

float CCombatSystem::ApplySplashDamageAtPoint(CSimulation& simulation, CUnit* attacker,
                                              float x, float y,
                                              bool isAttack, bool isRanged,
                                              CAttackType attackType, CDamageType damageType,
                                              float damageBase, float bonusDamage,
                                              int dice, int sidesPerDie,
                                              const CUnitAttack& attack,
                                              const CWidget* excludeWidget) {
    if (attack.areaHalf <= 0.0f) return 0.0f;
    float total = 0.0f;
    const float fullSq = attack.areaFull * attack.areaFull;
    const float halfSq = attack.areaHalf * attack.areaHalf;
    const float quarterSq = attack.areaQuarter * attack.areaQuarter;

    CPlayer* ownerPlayer = nullptr;
    if (attacker) {
        ownerPlayer = simulation.GetPlayer(attacker->GetPlayerIndex());
    }
    for (CUnit* other : simulation.GetUnits()) {
        if (!other || other == excludeWidget || other == attacker || other->IsDead()) continue;
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
        const float dx = other->GetX() - x;
        const float dy = other->GetY() - y;
        const float distSq = dx * dx + dy * dy;
        float factor = 0.0f;
        if (distSq <= fullSq) {
            factor = 1.0f; // inside the full-damage area
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

float CCombatSystem::ApplyLineDamage(CSimulation& simulation, CUnit* attacker,
                                     float x0, float y0, float dirX, float dirY,
                                     bool isAttack, bool isRanged,
                                     CAttackType attackType, CDamageType damageType,
                                     float damageBase, float bonusDamage,
                                     int dice, int sidesPerDie,
                                     float spillDistance, float spillRadius,
                                     const CWidget* excludeWidget) {
    if (spillDistance <= 0.0f || spillRadius <= 0.0f) return 0.0f;
    const float len = std::sqrt(dirX * dirX + dirY * dirY);
    if (len <= 0.0f) return 0.0f;
    const float nx = dirX / len;
    const float ny = dirY / len;
    float total = 0.0f;

    CPlayer* ownerPlayer = nullptr;
    if (attacker) {
        ownerPlayer = simulation.GetPlayer(attacker->GetPlayerIndex());
    }
    for (CUnit* other : simulation.GetUnits()) {
        if (!other || other == excludeWidget || other == attacker || other->IsDead()) continue;
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
        // Distance from the spill segment [p0, p0 + dir*spillDistance].
        const float rx = other->GetX() - x0;
        const float ry = other->GetY() - y0;
        float t = rx * nx + ry * ny;
        if (t < 0.0f) t = 0.0f;
        if (t > spillDistance) t = spillDistance;
        const float cx = x0 + nx * t;
        const float cy = y0 + ny * t;
        const float dx = other->GetX() - cx;
        const float dy = other->GetY() - cy;
        if (dx * dx + dy * dy > spillRadius * spillRadius) continue;
        // Spill carries the full attack (no ring falloff).
        total += ApplyDamage(simulation, attacker, other, isAttack, isRanged,
                             attackType, damageType, "",
                             damageBase, bonusDamage, dice, sidesPerDie);
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
    // Ref evasionListeners loop: every source rolls independently; the
    // attack is evaded when any roll succeeds (miss = miss || roll).
    for (const auto& src : target->GetEvasionSources()) {
        if (src.chance <= 0.0f) continue;
        if (src.chance >= 1.0f) return true;
        if (CombatRand01() < src.chance) return true;
    }
    return false;
}

float CCombatSystem::ApplyCritical(float damage, float chance, float multiplier) {
    if (chance <= 0) return damage;
    if (CombatRand01() < chance) {
        return damage * multiplier;
    }
    return damage;
}
