#include "combat_types.h"
#include <algorithm>
#include <cmath>

static std::string Lower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = (char)std::tolower(c);
    return r;
}

CAttackType CAttackTypeInfo::FromString(const std::string& s) {
    std::string l = Lower(s);
    if (l == "normal") return CAttackType::NORMAL;
    if (l == "pierce") return CAttackType::PIERCE;
    if (l == "siege") return CAttackType::SIEGE;
    if (l == "magic" || l == "spells") return CAttackType::MAGIC;
    if (l == "chaos") return CAttackType::CHAOS;
    if (l == "hero") return CAttackType::HERO;
    if (l == "damagebonus") return CAttackType::DAMAGE_BONUS;
    return CAttackType::NONE;
}

std::string CAttackTypeInfo::ToString(CAttackType t) {
    switch (t) {
        case CAttackType::NORMAL: return "Normal";
        case CAttackType::PIERCE: return "Pierce";
        case CAttackType::SIEGE: return "Siege";
        case CAttackType::MAGIC: return "Magic";
        case CAttackType::CHAOS: return "Chaos";
        case CAttackType::HERO: return "Hero";
        case CAttackType::SPELLS: return "Spells";
        case CAttackType::DAMAGE_BONUS: return "DamageBonus";
        default: return "None";
    }
}

CDefenseType CDefenseTypeInfo::FromString(const std::string& s) {
    std::string l = Lower(s);
    if (l == "light") return CDefenseType::LIGHT;
    if (l == "medium") return CDefenseType::MEDIUM;
    if (l == "heavy") return CDefenseType::HEAVY;
    if (l == "fortified") return CDefenseType::FORTIFIED;
    if (l == "hero") return CDefenseType::HERO;
    if (l == "divine") return CDefenseType::DIVINE;
    if (l == "unarmored") return CDefenseType::UNARMORED;
    return CDefenseType::NONE;
}

std::string CDefenseTypeInfo::ToString(CDefenseType t) {
    switch (t) {
        case CDefenseType::LIGHT: return "Light";
        case CDefenseType::MEDIUM: return "Medium";
        case CDefenseType::HEAVY: return "Heavy";
        case CDefenseType::FORTIFIED: return "Fortified";
        case CDefenseType::HERO: return "Hero";
        case CDefenseType::DIVINE: return "Divine";
        case CDefenseType::UNARMORED: return "Unarmored";
        default: return "None";
    }
}

float CDefenseTypeInfo::CalcDamageReduction(int defense, int attackerLevel, float defenseArmor) {
    (void)attackerLevel;
    // Matches warsmash_ref CUnit.damage: positive armor uses
    // reduction = (defense * k) / (1 + defense * k); negative armor amplifies
    // damage as 2 - (1 - k)^(-defense). k = DefenseArmor (0.06 default).
    if (defense >= 0) {
        float d = (float)defense;
        return (d * defenseArmor) / (1.0f + d * defenseArmor);
    }
    return 1.0f - (2.0f - std::pow(1.0f - defenseArmor, -(float)defense));
}

CRegenType CRegenTypeInfo::FromString(const std::string& s) {
    std::string l = Lower(s);
    if (l == "always") return CRegenType::ALWAYS;
    if (l == "only at night" || l == "night") return CRegenType::ONLY_AT_NIGHT;
    return CRegenType::NONE;
}

CWeaponType CWeaponTypeInfo::FromString(const std::string& s) {
    std::string l = Lower(s);
    if (l == "normal") return CWeaponType::NORMAL;
    if (l == "instant") return CWeaponType::INSTANT;
    if (l == "artillery") return CWeaponType::ARTILLERY;
    if (l == "artillery line") return CWeaponType::ARTILLERY_LINE;
    if (l == "missile") return CWeaponType::MISSILE;
    if (l == "missile bounce") return CWeaponType::MISSILE_BOUNCE;
    if (l == "missile line") return CWeaponType::MISSILE_LINE;
    if (l == "missile splash") return CWeaponType::MISSILE_SPLASH;
    return CWeaponType::NONE;
}

bool CTargetTypeInfo::FromString(const std::string& s, CTargetType& out) {
    const std::string l = Lower(s);
    if (l == "air") { out = CTargetType::AIR; return true; }
    if (l == "dead") { out = CTargetType::DEAD; return true; }
    if (l == "debris" || l == "debr") { out = CTargetType::DEBRIS; return true; }
    if (l == "enemies" || l == "enem" || l == "enemy") { out = CTargetType::ENEMY; return true; }
    if (l == "ground" || l == "grou") { out = CTargetType::GROUND; return true; }
    if (l == "hero") { out = CTargetType::HERO; return true; }
    if (l == "invulnerable" || l == "invu") { out = CTargetType::INVULNERABLE; return true; }
    if (l == "item") { out = CTargetType::ITEM; return true; }
    if (l == "mechanical" || l == "mech") { out = CTargetType::MECHANICAL; return true; }
    if (l == "neutral" || l == "neut") { out = CTargetType::NEUTRAL; return true; }
    if (l == "nonhero" || l == "nonh") { out = CTargetType::NON_HERO; return true; }
    if (l == "nonsapper") { out = CTargetType::NON_SAPPER; return true; }
    if (l == "notself" || l == "nots") { out = CTargetType::NON_SELF; return true; }
    if (l == "organic" || l == "orga") { out = CTargetType::ORGANIC; return true; }
    if (l == "player" || l == "play") { out = CTargetType::PLAYER_UNITS; return true; }
    if (l == "self") { out = CTargetType::SELF; return true; }
    if (l == "structure" || l == "stru") { out = CTargetType::STRUCTURE; return true; }
    if (l == "terrain" || l == "terr") { out = CTargetType::TERRAIN; return true; }
    if (l == "tree") { out = CTargetType::TREE; return true; }
    if (l == "vulnerable" || l == "vuln") { out = CTargetType::VULNERABLE; return true; }
    if (l == "wall") { out = CTargetType::WALL; return true; }
    if (l == "ward") { out = CTargetType::WARD; return true; }
    if (l == "ancient") { out = CTargetType::ANCIENT; return true; }
    if (l == "nonancient") { out = CTargetType::NON_ANCIENT; return true; }
    if (l == "friend" || l == "frie") { out = CTargetType::FRIEND; return true; }
    if (l == "allies" || l == "alli" || l == "ally") { out = CTargetType::ALLY; return true; }
    if (l == "bridge") { out = CTargetType::BRIDGE; return true; }
    if (l == "decoration" || l == "deco") { out = CTargetType::DECORATION; return true; }
    return false;
}
