#pragma once
#include <string>
#include "data/combat_types.h"

struct CUnitAttack {
    int index = 0;
    CAttackType attackType = CAttackType::NONE;
    CWeaponType weaponType = CWeaponType::NONE;

    int damageBase = 0;
    int damageDice = 0;
    int damageSidesPerDie = 0;
    int damageUpgradeAmount = 0;

    float cooldown = 1.0f;
    float damagePoint = 0.0f;
    float backswingPoint = 0.0f;

    float range = 100.0f;
    float minRange = 0.0f;

    float projectileSpeed = 0.0f;
    std::string projectileArt;

    float areaFull = 0.0f;
    float areaHalf = 0.0f;
    float areaQuarter = 0.0f;
    float damageFactorHalf = 0.5f;
    float damageFactorQuarter = 0.25f;

    int targetCount = 0;
    float damageLossFactor = 0.0f;
    float areaSpread = 0.0f;

    int attacksEnabled = 0;
    float damageSpillDistance = 0.0f;
    float damageSpillRadius = 0.0f;
    std::string weaponSound;
    std::string weaponSoundLoop;

    bool CanTargetAir() const { return true; }
    bool CanTargetGround() const { return true; }
    float GetRange() const { return range; }
};
