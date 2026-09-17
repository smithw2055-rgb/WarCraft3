#pragma once
#include "data/combat_types.h"
#include "ecs/fwd.h"
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

    // Override the default TFT table / DefenseArmor with data-loaded values
    // (CGameplayConstants from Units\MiscGame.txt). Until Configure() is
    // called the compiled-in defaults are used.
    void Configure(float defenseArmor, const AttackDefenseTable& table);

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

    // Splash damage (ref missile-splash / artillery): full damage to the
    // primary target plus falloff damage to other enemy units in the
    // areaHalf/areaQuarter rings. Returns total damage dealt.
    float ApplySplashDamage(CSimulation& simulation, CUnit* attacker, CWidget* target,
                            bool isAttack, bool isRanged,
                            CAttackType attackType, CDamageType damageType,
                            const std::string& weaponSoundType,
                            float damageBase, float bonusDamage,
                            int dice, int sidesPerDie,
                            const CUnitAttack& attack);

    // Armor calc
    static float CalcArmorDamageReduction(int defense, int attackerLevel, float defenseArmor = 0.06f);

    // Evasion
    static bool CheckEvasion(CUnit* target, CUnit* attacker);

    // Critical strike
    static float ApplyCritical(float damage, float chance, float multiplier);

private:
    float GetConfiguredMultiplier(CAttackType atk, CDefenseType def) const;

    float defenseArmor_ = 0.06f;
    AttackDefenseTable table_;
};
