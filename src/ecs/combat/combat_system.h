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

    // Ethereal / magic-immune rules (ref default damage-taken listeners).
    // Forwarded from CGameplayConstants by CSimulation::SetGameplayConstants.
    void ConfigureMagicRules(float etherealBonusMagic, float etherealBonusSpells,
                             bool magicImmuneResistsDamage,
                             bool magicImmuneResistsUltimates,
                             bool etherealBonusAlly = false);

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

    // Ground-point splash (ref ARTILLERY firing at a point target): the
    // same ring falloff centered on (x, y) with no primary widget.
    // Artillery shells converted at fire time funnel here on impact.
    // excludeWidget (already-damaged primary) skips one widget so targeted
    // splashes never double-hit it.
    float ApplySplashDamageAtPoint(CSimulation& simulation, CUnit* attacker,
                                  float x, float y,
                                  bool isAttack, bool isRanged,
                                  CAttackType attackType, CDamageType damageType,
                                  float damageBase, float bonusDamage,
                                  int dice, int sidesPerDie,
                                  const CUnitAttack& attack,
                                  const CWidget* excludeWidget = nullptr);

    // Line spill (MLINE/ALINE corridor): full damage to enemy units within
    // spillRadius of the segment from (x0, y0) along (dirX, dirY) for
    // spillDistance. excludeWidget (the pierced primary) is skipped so it
    // is never double-hit. Returns total damage dealt.
    float ApplyLineDamage(CSimulation& simulation, CUnit* attacker,
                          float x0, float y0, float dirX, float dirY,
                          bool isAttack, bool isRanged,
                          CAttackType attackType, CDamageType damageType,
                          float damageBase, float bonusDamage,
                          int dice, int sidesPerDie,
                          float spillDistance, float spillRadius,
                          const CWidget* excludeWidget = nullptr);

    // Armor calc
    static float CalcArmorDamageReduction(int defense, int attackerLevel, float defenseArmor = 0.06f);

    // Evasion
    bool CheckEvasion(CUnit* target, CUnit* attacker);

    // Critical strike
    float ApplyCritical(float damage, float chance, float multiplier);

    // Deterministic combat RNG (same LCG as CSimulation::SimRand01; seeded
    // per game so replays reproduce. std::rand/mt19937 stay banned here).
    float CombatRand01();
    void SetSeed(uint32_t seed) { rngState_ = seed; }
    uint32_t GetRandState() const { return rngState_; }

private:
    float GetConfiguredMultiplier(CAttackType atk, CDefenseType def) const;

    float defenseArmor_ = 0.06f;
    AttackDefenseTable table_;
    float etherealBonusMagic_ = 1.6667f;
    float etherealBonusSpells_ = 1.6667f;
    bool magicImmuneResistsDamage_ = true;
    bool magicImmuneResistsUltimates_ = true;
    bool etherealBonusAlly_ = false;
    uint32_t rngState_ = 0x12345678u;
};
