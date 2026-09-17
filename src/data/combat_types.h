#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

enum class CAttackType : uint8_t {
    NONE, NORMAL, PIERCE, SIEGE, MAGIC, CHAOS, HERO, SPELLS, DAMAGE_BONUS, UNKNOWN
};

enum class CDefenseType : uint8_t {
    NONE, LIGHT, MEDIUM, HEAVY, FORTIFIED, HERO, DIVINE, UNARMORED, UNKNOWN
};

enum class CDamageType : uint8_t {
    UNKNOWN, NORMAL, ENHANCED, FIRE, COLD, LIGHTNING, POISON, DISEASE, DIVINE,
    MAGIC, SONIC, ACID, FORCE, DEATH, MIND, PLANT, MECHANICAL,
    PERCENT, ARMOR_SELF, AOE, SLEEP,
    // Extended JASS damage types (common.j ConvertDamageType 21-26). Like all
    // non-NORMAL types they ignore armor value (ref CUnit.damage) but still
    // apply the attack-vs-defense-type table.
    DEFENSIVE, DEMOLITION, SLOW_POISON, SPIRIT_LINK, SHADOW_STRIKE, UNIVERSAL
};

enum class CRegenType : uint8_t {
    NONE, ALWAYS, ONLY_AT_NIGHT
};

enum class CWeaponType : uint8_t {
    NONE, NORMAL, INSTANT, ARTILLERY, ARTILLERY_LINE, MISSILE, MISSILE_BOUNCE, MISSILE_LINE, MISSILE_SPLASH, UNKNOWN
};

enum class CUpgradeClass : uint8_t {
    ARMOR, ATTACK, ARTILLERY, STEALTH, RANGED, SIGHT, SPEED, HEALTH, MANA,
    DEFENSE_UPGRADE_1, DEFENSE_UPGRADE_2, DEFENSE_UPGRADE_3,
    DEFENSE_UPGRADE_4, DEFENSE_UPGRADE_5, DEFENSE_UPGRADE_6,
    DEFENSE_UPGRADE_7, DEFENSE_UPGRADE_8, DEFENSE_UPGRADE_9,
    DEFENSE_UPGRADE_10, MAX_UNKNOWN
};

enum class CTargetType : uint8_t {
    GROUND, AIR, DEBRIS, DECORATION, WALL, DIGGABLE, ITEM, BRIDGE,
    MECHANICAL, ORGANIC, PEON, TREE, VULNERABLE, IMMUNE, HERO,
    ANCIENT, NON_ANCIENT, STANDARD, NEUTRAL, SELF, ALLY, FRIEND,
    ENEMY, PLAYER_UNITS, STRUCTURE, WARD, INVULNERABLE, NON_HERO,
    NON_BUILDING, NON_SAPPER, NON_SELF, DEAD, TERRAIN
};

struct CAttackTypeInfo {
    static CAttackType FromString(const std::string& s);
    static std::string ToString(CAttackType t);
};

struct CDefenseTypeInfo {
    static CDefenseType FromString(const std::string& s);
    static std::string ToString(CDefenseType t);
    static float CalcDamageReduction(int defense, int attackerLevel, float defenseArmor = 0.06f);
};

// Attack-type vs defense-type multipliers, indexed LIGHT..UNARMORED per
// DefenseTypeIndex() in combat_system.cpp:
// [0]=NONE, [1]=LIGHT, [2]=MEDIUM, [3]=HEAVY, [4]=FORTIFIED,
// [5]=HERO, [6]=DIVINE, [7]=UNARMORED.
struct AttackDefenseTable {
    float normal[8];
    float pierce[8];
    float siege[8];
    float magic[8];
    float chaos[8];
    float hero[8];
    float spells[8];
};

struct CRegenTypeInfo {
    static CRegenType FromString(const std::string& s);
};

struct CWeaponTypeInfo {
    static CWeaponType FromString(const std::string& s);
    // Mirrors ref CWeaponType.ranged: only Normal melee (and None) hit
    // instantly; every other weapon type travels (or is instant-cast).
    static bool IsRanged(CWeaponType t) {
        return t != CWeaponType::NONE && t != CWeaponType::NORMAL &&
               t != CWeaponType::UNKNOWN;
    }
};
