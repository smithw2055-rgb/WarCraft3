#include "combat_system.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/unit_attack.h"
#include "ecs/widget.h"
#include "ecs/player.h"
#include "engine/simulation_module.h"
#include "game/war3/war3_game_module.h"
#include "audio/AudioSystem.h"
#include <cmath>
#include <algorithm>

int CCombatSystem::DefenseTypeToIndex(CDefenseType def) {
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

CCombatSystem::CCombatSystem()
    : table_(war3::War3GameModule::GetCanonicalWar3AttackDefenseTable()) {}

void CCombatSystem::SetCombatModule(const engine::ISimulationModule* module, bool pureEngineMode) {
    combatModule_ = module;
    pureEngineMode_ = pureEngineMode;
}

void CCombatSystem::Configure(float defenseArmor, const AttackDefenseTable& table) {
    defenseArmor_ = defenseArmor;
    table_ = table;
    customTableConfigured_ = true;
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

void CCombatSystem::SetTableEntry(CAttackType atk, CDefenseType def, float multiplier) {
    int di = DefenseTypeToIndex(def);
    if (di < 0 || di > 7) return;
    customTableConfigured_ = true;
    switch (atk) {
        case CAttackType::NORMAL: table_.normal[di] = multiplier; break;
        case CAttackType::PIERCE: table_.pierce[di] = multiplier; break;
        case CAttackType::SIEGE:  table_.siege[di]  = multiplier; break;
        case CAttackType::MAGIC:  table_.magic[di]  = multiplier; break;
        case CAttackType::SPELLS: table_.spells[di] = multiplier; break;
        case CAttackType::CHAOS:  table_.chaos[di]  = multiplier; break;
        case CAttackType::HERO:   table_.hero[di]   = multiplier; break;
        default: break;
    }
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
    if (pureEngineMode_ && combatModule_) {
        return combatModule_->GetAttackDefenseMultiplier(static_cast<int>(atk), static_cast<int>(def));
    }
    if (!customTableConfigured_ && combatModule_) {
        return combatModule_->GetAttackDefenseMultiplier(static_cast<int>(atk), static_cast<int>(def));
    }
    int di = DefenseTypeToIndex(def);
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
    const auto& kTable = war3::War3GameModule::GetCanonicalWar3AttackDefenseTable();
    int di = DefenseTypeToIndex(def);
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
                                               int dice, int sidesPerDie,
                                               bool rollMiss) {
    (void)isRanged;
    DamageResult result;

    if (!target || target->IsDead()) return result;
    const bool invulnerable = target->IsInvulnerable();

    // Check evasion (ref evasionListeners: pre-damage, so it runs even vs
    // invulnerable targets for the miss text; splash secondaries skip it
    // via rollMiss=false after the single primary roll).
    if (rollMiss && isAttack && target != attacker) {
        CUnit* targetUnit = ((target) ? (target)->AsUnit() : nullptr);
        if (targetUnit && CheckEvasion(targetUnit, attacker)) {
            result.isEvaded = true;
            return result;
        }
    }
    if (invulnerable) {
        if (isAttack && attacker && attacker != ((target) ? (target)->AsUnit() : nullptr)) {
            for (const auto& src : attacker->GetCritSources()) {
                if (src.chance > 0.0f && CombatRand01() < src.chance) {
                    result.isCritical = true;
                    break;
                }
            }
        }
        return result;
    }

    // Phase 15: Magic immunity & Ethereal rules delegated to active ISimulationModule when bound
    CUnit* targetUnit = ((target) ? (target)->AsUnit() : nullptr);
    if (targetUnit) {
        if (combatModule_) {
            if (combatModule_->ShouldResistMagicDamage(
                    static_cast<int>(attackType),
                    static_cast<int>(damageType),
                    targetUnit->IsMagicImmune(),
                    magicImmuneResistsDamage_,
                    magicImmuneResistsUltimates_)) {
                return result;
            }
        } else if (targetUnit->IsMagicImmune()) {
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

    float etherealMultiplier = 1.0f;
    if (targetUnit) {
        const bool allied = attacker && attacker->GetPlayerIndex() == targetUnit->GetPlayerIndex();
        if (combatModule_) {
            if (!combatModule_->EvaluateEtherealDamageFactor(
                    static_cast<int>(attackType),
                    static_cast<int>(damageType),
                    targetUnit->IsEthereal(),
                    allied,
                    etherealBonusAlly_,
                    etherealBonusMagic_,
                    etherealBonusSpells_,
                    etherealMultiplier)) {
                return result;
            }
        } else if (targetUnit->IsEthereal()) {
            const bool grantBonus = !allied || etherealBonusAlly_;
            if (grantBonus && attackType == CAttackType::MAGIC) {
                etherealMultiplier = etherealBonusMagic_;
            } else if (grantBonus && attackType == CAttackType::SPELLS) {
                etherealMultiplier = etherealBonusSpells_;
            }
            if (damageType == CDamageType::NORMAL && attackType != CAttackType::MAGIC) {
                return result;
            }
        }
    }

    // Roll damage (bonus damage is part of the base, like ref computeFinalDamage)
    float rolledDamage = damageBase + bonusDamage;
    if (dice > 0 && sidesPerDie > 0) {
        for (int d = 0; d < dice; d++)
            rolledDamage += (float)(1 + (int)(CombatRand01() * (float)sidesPerDie));
    }

    // Attack-type vs defense-type multiplier
    CDefenseType targetDef = CDefenseType::NONE;
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
        if (combatModule_) {
            armorReduction = combatModule_->ComputeArmorDamageReduction(targetArmor, 0, defenseArmor_);
        } else {
            armorReduction = CalcArmorDamageReduction(targetArmor, 0, defenseArmor_);
        }
        if (armorReduction > 0.95f) armorReduction = 0.95f;
    }
    result.damageBlocked = rolledDamage * armorReduction;
    rolledDamage *= (1.0f - armorReduction);

    result.damageDealt = rolledDamage;
    return result;
}

bool CCombatSystem::RollAttackMiss(CSimulation& simulation, CUnit* attacker,
                                   CWidget* target, bool isAttack) {
    if (simulation.CheckHighlandMiss(attacker, target, isAttack)) return true;
    if (isAttack && target && target != attacker) {
        if (CUnit* targetUnit = ((target) ? (target)->AsUnit() : nullptr)) {
            if (CheckEvasion(targetUnit, attacker)) return true;
        }
    }
    return false;
}

float CCombatSystem::ApplyDamage(CSimulation& simulation, CUnit* attacker, CWidget* target,
                                   bool isAttack, bool isRanged,
                                   CAttackType attackType, CDamageType damageType,
                                   const std::string& weaponSoundType,
                                   float damageBase, float bonusDamage,
                                   int dice, int sidesPerDie,
                                   float baseMultiplier, bool rollMiss) {
    if (!target || target->IsDead()) return 0;
    if (rollMiss && simulation.CheckHighlandMiss(attacker, target, isAttack)) {
        simulation.FireCombatText(attacker ? attacker->GetHandleId() : -1, target->GetHandleId(),
                                  CSimulation::CombatTextKind::MISS, 0.0f);
        return 0;
    }

    auto result = CalculateDamage(attacker, target, isAttack, isRanged,
                                   attackType, damageType,
                                   damageBase, bonusDamage, dice, sidesPerDie,
                                   rollMiss);
    if (result.isEvaded) {
        simulation.FireCombatText(attacker ? attacker->GetHandleId() : -1, target->GetHandleId(),
                                  CSimulation::CombatTextKind::MISS, 0.0f);
        return 0;
    }
    const float dealt = result.damageDealt * baseMultiplier;
    if (dealt == 0.0f) {
        if (target->IsInvulnerable()) {
            auto& quietCb = simulation.GetUnitDamageCallback();
            if (quietCb) {
                quietCb(target->GetHandleId(),
                        attacker ? attacker->GetHandleId() : -1, 0.0f);
            }
        }
        return 0;
    }
    if (result.isCritical) {
        simulation.FireCombatText(attacker ? attacker->GetHandleId() : -1, target->GetHandleId(),
                                  CSimulation::CombatTextKind::CRIT, dealt);
    }

    const float oldLife = target->GetLife();
    float newLife = oldLife - dealt;
    if (dealt < 0.0f && oldLife <= target->GetMaxLife() && newLife > target->GetMaxLife()) {
        newLife = target->GetMaxLife();
    }
    target->SetLife(simulation, newLife);

    auto& dmgCb = simulation.GetUnitDamageCallback();
    if (dmgCb) {
        int srcHandle = attacker ? attacker->GetHandleId() : -1;
        dmgCb(target->GetHandleId(), srcHandle, dealt);
    }

    (void)isRanged;

    auto* audio = simulation.GetAudioSystem();
    if (audio && !weaponSoundType.empty()) {
        audio->Play3D(weaponSoundType, target->GetX(), target->GetY() + 50.0f, 0, 0.7f);
    }

    return dealt;
}

float CCombatSystem::ApplySplashDamage(CSimulation& simulation, CUnit* attacker, CWidget* target,
                                       bool isAttack, bool isRanged,
                                       CAttackType attackType, CDamageType damageType,
                                       const std::string& weaponSoundType,
                                       float damageBase, float bonusDamage,
                                       int dice, int sidesPerDie,
                                       const CUnitAttack& attack) {
    float baseMultiplier = 1.0f;
    if (target && RollAttackMiss(simulation, attacker, target, isAttack)) {
        if (CWeaponTypeInfo::IsSplashWeapon(attack.weaponType)) {
            baseMultiplier = simulation.GetGameplayConstants().missDamageReduction;
            if (attack.weaponType != CWeaponType::ARTILLERY) {
                simulation.FireCombatText(attacker ? attacker->GetHandleId() : -1,
                                          target->GetHandleId(),
                                          CSimulation::CombatTextKind::MISS, 0.0f);
            }
        } else {
            simulation.FireCombatText(attacker ? attacker->GetHandleId() : -1,
                                      target->GetHandleId(),
                                      CSimulation::CombatTextKind::MISS, 0.0f);
            return 0;
        }
    }
    float total = ApplyDamage(simulation, attacker, target, isAttack, isRanged,
                              attackType, damageType, weaponSoundType,
                              damageBase, bonusDamage, dice, sidesPerDie,
                              baseMultiplier, false);
    if (!target || attack.areaHalf <= 0.0f) {
        return total;
    }
    total += ApplySplashDamageAtPoint(simulation, attacker, target->GetX(), target->GetY(),
                                      isAttack, isRanged, attackType, damageType,
                                      damageBase, bonusDamage, dice, sidesPerDie, attack, target,
                                      baseMultiplier, false);
    return total;
}

float CCombatSystem::ApplySplashDamageAtPoint(CSimulation& simulation, CUnit* attacker,
                                              float x, float y,
                                              bool isAttack, bool isRanged,
                                              CAttackType attackType, CDamageType damageType,
                                              float damageBase, float bonusDamage,
                                              int dice, int sidesPerDie,
                                              const CUnitAttack& attack,
                                              const CWidget* excludeWidget,
                                              float baseMultiplier, bool rollMiss) {
    if (attack.areaHalf <= 0.0f) return 0.0f;
    float total = 0.0f;
    const float fullSq = attack.areaFull * attack.areaFull;
    const float halfSq = attack.areaHalf * attack.areaHalf;
    const float quarterSq = attack.areaQuarter * attack.areaQuarter;

    CPlayer* ownerPlayer = nullptr;
    if (attacker) {
        ownerPlayer = simulation.GetPlayer(attacker->GetPlayerIndex());
    }
    const float maxSplashRadius = std::max({attack.areaFull, attack.areaHalf, attack.areaQuarter});
    std::vector<CUnit*> spatialCandidates;
    const std::vector<CUnit*>* unitList = &simulation.GetUnits();
    if (simulation.GetUnits().size() > 32 && simulation.SpatialGrid().IndexedEntityCount() > 0) {
        spatialCandidates = simulation.QueryUnitsInRadius(x, y, maxSplashRadius + 64.0f);
        unitList = &spatialCandidates;
    }
    for (CUnit* other : *unitList) {
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
        const float dx = other->GetX() - x;
        const float dy = other->GetY() - y;
        const float distSq = dx * dx + dy * dy;
        float factor = 0.0f;
        if (distSq <= fullSq) {
            factor = 1.0f;
        } else if (distSq <= halfSq) {
            factor = attack.damageFactorHalf;
        } else if (attack.areaQuarter > 0.0f && distSq <= quarterSq) {
            factor = attack.damageFactorQuarter;
        } else {
            continue;
        }
        total += ApplyDamage(simulation, attacker, other, isAttack, isRanged,
                             attackType, damageType, "",
                             damageBase * factor, bonusDamage, dice, sidesPerDie,
                             baseMultiplier, rollMiss);
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
    std::vector<CUnit*> lineCandidates;
    const std::vector<CUnit*>* lineUnitList = &simulation.GetUnits();
    if (simulation.GetUnits().size() > 32 && simulation.SpatialGrid().IndexedEntityCount() > 0) {
        const float midX = x0 + nx * (0.5f * spillDistance);
        const float midY = y0 + ny * (0.5f * spillDistance);
        const float boundRadius = 0.5f * spillDistance + spillRadius + 64.0f;
        lineCandidates = simulation.QueryUnitsInRadius(midX, midY, boundRadius);
        lineUnitList = &lineCandidates;
    }
    for (CUnit* other : *lineUnitList) {
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
        total += ApplyDamage(simulation, attacker, other, isAttack, isRanged,
                             attackType, damageType, "",
                             damageBase, bonusDamage, dice, sidesPerDie);
    }
    return total;
}

float CCombatSystem::CalcArmorDamageReduction(int defense, int attackerLevel, float defenseArmor) {
    return CDefenseTypeInfo::CalcDamageReduction(defense, attackerLevel, defenseArmor);
}

bool CCombatSystem::CheckEvasion(CUnit* target, CUnit* attacker) {
    (void)attacker;
    if (!target) return false;
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
