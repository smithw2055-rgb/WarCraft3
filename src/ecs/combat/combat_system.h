#pragma once
#include "data/combat_types.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

struct DamageResult {
    float damageDealt = 0;
    float damageBlocked = 0;
    float multiplier = 1.0f;
    bool isCritical = false;
    bool isEvaded = false;
};

class CSimulation;
class CUnit;
class CWidget;

class CCombatSystem {
public:
    CCombatSystem();

    // Attack-type vs defense-type multiplier table (WC3 standard)
    static float GetAttackDefenseMultiplier(CAttackType atk, CDefenseType def);

    // Full damage calculation
    DamageResult CalculateDamage(CUnit* attacker, CWidget* target,
                                  bool isAttack, bool isRanged,
                                  CAttackType attackType, CDamageType damageType,
                                  float damageBase, float bonusDamage,
                                  int dice, int sidesPerDie);

    // Apply damage to a widget, returns actual damage dealt
    float ApplyDamage(CSimulation& simulation, CUnit* attacker, CWidget* target,
                       bool isAttack, bool isRanged,
                       CAttackType attackType, CDamageType damageType,
                       const std::string& weaponSoundType,
                       float damageBase, float bonusDamage,
                       int dice = 0, int sidesPerDie = 0);

    // Armor calc
    static float CalcArmorDamageReduction(int defense, int attackerLevel);

    // Evasion
    static bool CheckEvasion(CUnit* target, CUnit* attacker);

    // Critical strike
    static float ApplyCritical(float damage, float chance, float multiplier);
};

struct AttackDefenseTable {
    float normal[8];
    float pierce[8];
    float siege[8];
    float magic[8];
    float chaos[8];
    float hero[8];
    float spells[8];
};
