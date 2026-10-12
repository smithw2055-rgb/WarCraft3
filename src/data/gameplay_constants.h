#pragma once
#include "data/combat_types.h"
#include <string>
#include <vector>

class TxtTable;

struct CGameplayConstants {
    float gameDayLength = 24.0f;
    float gameDayHours = 24.0f;
    float dawnTimeGameHours = 6.0f;
    float duskTimeGameHours = 18.0f;
    float maxCollisionRadius = 512.0f;
    float deathTimeDefault = 1.0f;
    float decayTime = 8.0f;
    float boneDecayTime = 8.0f;
    float dissipateTime = 2.0f;
    float structureDecayTime = 8.0f;

    // Armor formula constant (Units\MiscData.txt DefenseArmor, 0.06 default).
    float defenseArmor = 0.06f;

    // Attack-type vs defense-type table (Units\MiscGame.txt DamageBonus*
    // rows, Small,Medium,Large,Fort,Normal,Hero,Divine,None column order).
    // Defaults equal the TFT data so behavior is identical with or without
    // the data files present.
    AttackDefenseTable damageBonus;
    float GetDefenseArmor() const { return defenseArmor; }
    float GetDamageRatioAgainst(CAttackType atk, CDefenseType def) const;

    // Ethereal / magic-immune rules (Units\MiscGame.txt, ref
    // CUnitDefaultEtherealDamageModListener /
    // CUnitDefaultMagicImmuneDamageModListener). TFT retail: ethereal takes
    // ~1.66x from MAGIC and SPELLS; NORMAL-damage (physical) is negated.
    // Magic immunity resists MAGIC damage and non-universal SPELLS.
    float etherealDamageBonusMagic = 1.6667f;
    float etherealDamageBonusSpells = 1.6667f;
    bool etherealDamageBonusAlly = false;
    bool magicImmuneResistsDamage = true;
    bool magicImmuneResistsUltimates = true;
    // Pawn shops pay goldCost * pawnItemRate for pawnable items (ref
    // CGameplayConstants PawnItemRate from Units\MiscGame.txt).
    float pawnItemRate = 0.5f;

    // Hero revive/awaken cost factors (Units\MiscGame.txt, ref
    // CGameplayConstants; TFT retail defaults). Revive: cost * (base +
    // levelFactor * (level - 1)), capped at cost * maxFactor. Lumber uses
    // the GOLD cost as its base (ref bug-compatible: lumber factors are 0).
    float reviveBaseFactor = 0.40f;
    float reviveLevelFactor = 0.10f;
    float reviveBaseLumberFactor = 0.0f;
    float reviveLumberLevelFactor = 0.0f;
    float reviveMaxFactor = 4.0f;
    float heroReviveLifeFactor = 1.0f;
    float heroReviveManaFactor = 0.0f;
    float awakenBaseFactor = 0.80f;
    float awakenLevelFactor = 0.20f;
    float awakenBaseLumberFactor = 0.80f;
    float awakenLumberLevelFactor = 0.20f;
    float awakenMaxFactor = 8.0f;
    float heroAwakenLifeFactor = 0.5f;
    float heroAwakenManaFactor = 0.0f;
    float reviveTimeFactor = 0.5f;
    float reviveMaxTimeFactor = 1e9f;

    int GetHeroReviveGoldCost(int originalGoldCost, int level) const;
    int GetHeroReviveLumberCost(int originalGoldCost, int level) const;
    int GetHeroAwakenGoldCost(int originalGoldCost, int level) const;
    int GetHeroAwakenLumberCost(int originalLumberCost, int level) const;
    // Revive train time (ref getHeroReviveTime): buildTime * level *
    // reviveTimeFactor, capped at buildTime * reviveMaxTimeFactor.
    int GetHeroReviveTime(int originalTime, int level) const;

    // Hero XP (Units\MiscGame.txt NeedHeroXP table + formulas, ref
    // CGameplayConstants.parseTable). Cumulative thresholds indexed by
    // level (level 1 = 0). Defaults match TFT retail.
    int maxHeroLevel = 10;
    std::vector<int> needHeroXpSum;
    int GetXPForLevel(int level) const;
    int GetMaxHeroLevel() const { return maxHeroLevel; }

    // Primary-attribute bonuses (ref strHitPointBonus/intManaBonus/
    // agiDefenseBonus). Defaults match the previous hardcoded values.
    float strHitPointBonus = 25.0f;
    float intManaBonus = 15.0f;
    float agiDefenseBonus = 0.33f;

    CGameplayConstants();
    explicit CGameplayConstants(const TxtTable& miscData);
};
