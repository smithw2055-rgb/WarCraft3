#pragma once
#include "data/combat_types.h"
#include <string>

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

    CGameplayConstants();
    explicit CGameplayConstants(const TxtTable& miscData);
};
