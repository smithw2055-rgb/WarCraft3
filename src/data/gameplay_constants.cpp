#include "gameplay_constants.h"
#include "resource/slk.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

// TFT data defaults (Units\MiscGame.txt DamageBonus* rows). Column order in
// the data files: Small,Medium,Large,Fort,Normal,Hero,Divine,None, mapped to
// struct slots NONE,LIGHT,MEDIUM,HEAVY,FORTIFIED,HERO,DIVINE,UNARMORED.
void SetRow(float row[8], float none, float light, float medium, float heavy,
            float fort, float hero, float divine, float unarmored) {
    row[0] = none;
    row[1] = light;
    row[2] = medium;
    row[3] = heavy;
    row[4] = fort;
    row[5] = hero;
    row[6] = divine;
    row[7] = unarmored;
}

void LoadTftDefaults(AttackDefenseTable& t) {
    SetRow(t.normal, 1.0f, 1.0f, 1.5f, 1.0f, 0.7f, 1.0f, 0.05f, 1.0f);
    SetRow(t.pierce, 1.0f, 2.0f, 0.75f, 1.0f, 0.35f, 0.5f, 0.05f, 1.5f);
    SetRow(t.siege, 1.0f, 1.0f, 0.5f, 1.0f, 1.5f, 0.5f, 0.05f, 1.5f);
    SetRow(t.magic, 1.0f, 1.25f, 0.75f, 2.0f, 0.35f, 0.5f, 0.05f, 1.0f);
    SetRow(t.chaos, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    SetRow(t.hero, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 1.0f, 0.05f, 1.0f);
    SetRow(t.spells, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.7f, 0.05f, 1.0f);
}

// Parse one "a,b,c,d,e,f,g,h" row in Small..None column order into a struct
// row in NONE,LIGHT,..,UNARMORED slot order. Returns false (keeping current
// values) unless exactly 8 components parse.
bool ParseBonusRow(const std::string& text, float row[8]) {
    float small[8];
    size_t pos = 0;
    for (int i = 0; i < 8; i++) {
        size_t comma = text.find(',', pos);
        std::string part = (comma == std::string::npos) ? text.substr(pos)
                                                        : text.substr(pos, comma - pos);
        if (part.empty()) return false;
        small[i] = (float)std::atof(part.c_str());
        if (comma == std::string::npos) {
            if (i != 7) return false; // fewer than 8 values
            break;
        }
        pos = comma + 1;
    }
    if (text.find(',', pos) != std::string::npos) return false; // extra values
    row[0] = small[4]; // Normal -> NONE slot
    row[1] = small[0]; // Small -> LIGHT
    row[2] = small[1]; // Medium
    row[3] = small[2]; // Large -> HEAVY
    row[4] = small[3]; // Fort -> FORTIFIED
    row[5] = small[5]; // Hero
    row[6] = small[6]; // Divine
    row[7] = small[7]; // None -> UNARMORED
    return true;
}

} // namespace

float CGameplayConstants::GetDamageRatioAgainst(CAttackType atk, CDefenseType def) const {
    int di = (int)def;
    if (di < 0 || di > 7) di = 0;
    const float* row = damageBonus.normal;
    switch (atk) {
        case CAttackType::NORMAL: row = damageBonus.normal; break;
        case CAttackType::PIERCE: row = damageBonus.pierce; break;
        case CAttackType::SIEGE: row = damageBonus.siege; break;
        case CAttackType::MAGIC: row = damageBonus.magic; break;
        case CAttackType::CHAOS: row = damageBonus.chaos; break;
        case CAttackType::HERO: row = damageBonus.hero; break;
        case CAttackType::SPELLS: row = damageBonus.spells; break;
        default: break;
    }
    return row[di];
}

CGameplayConstants::CGameplayConstants()
    : gameDayLength(24.0f), gameDayHours(24.0f),
      dawnTimeGameHours(6.0f), duskTimeGameHours(18.0f),
      maxCollisionRadius(512.0f), deathTimeDefault(1.0f),
      decayTime(8.0f), boneDecayTime(8.0f),
      dissipateTime(2.0f), structureDecayTime(8.0f) {
    LoadTftDefaults(damageBonus);
    // TFT retail cumulative XP thresholds (levels 1..10).
    needHeroXpSum = {0, 0, 200, 500, 900, 1400, 2000, 2700, 3500, 4400, 5400};
}

int CGameplayConstants::GetXPForLevel(int level) const {
    if (level < 1) level = 1;
    if (!needHeroXpSum.empty() && level < (int)needHeroXpSum.size()) {
        return needHeroXpSum[(size_t)level];
    }
    if (level > 10) level = 10;
    static const int kDefault[] = {0, 0, 200, 500, 900, 1400, 2000, 2700, 3500, 4400, 5400};
    return kDefault[level];
}

CGameplayConstants::CGameplayConstants(const TxtTable& miscData) {
    auto readFloat = [&](const char* key, float def) {
        if (miscData.HasSection("Misc")) {
            return miscData.GetFloat("Misc", key, def);
        }
        return miscData.GetFloat("GameConstants", key, def);
    };

    gameDayLength = readFloat("GameDayLength", 24.0f);
    gameDayHours = readFloat("GameDayHours", 24.0f);
    dawnTimeGameHours = readFloat("DawnTime", readFloat("Dawn", 6.0f));
    duskTimeGameHours = readFloat("DuskTime", readFloat("Dusk", 18.0f));
    maxCollisionRadius = readFloat("MaxCollisionRadius", 512.0f);
    deathTimeDefault = readFloat("DeathTime", 1.0f);
    decayTime = readFloat("DecayTime", 8.0f);
    boneDecayTime = readFloat("BoneDecayTime", 8.0f);
    dissipateTime = readFloat("DissipateTime", 8.0f);
    structureDecayTime = readFloat("StructureDecayTime", 8.0f);
    defenseArmor = readFloat("DefenseArmor", 0.06f);

    // Attack/defense bonus table (Units\MiscGame.txt). Missing rows keep TFT
    // defaults; SPELLS falls back to the MAGIC row like warsmash_ref.
    LoadTftDefaults(damageBonus);
    auto readRow = [&](const char* key, float row[8]) {
        std::string text;
        if (miscData.HasSection("Misc")) {
            text = miscData.GetString("Misc", key, "");
        } else {
            text = miscData.GetString("GameConstants", key, "");
        }
        if (!text.empty()) ParseBonusRow(text, row);
    };
    readRow("DamageBonusNormal", damageBonus.normal);
    readRow("DamageBonusPierce", damageBonus.pierce);
    readRow("DamageBonusSiege", damageBonus.siege);
    readRow("DamageBonusMagic", damageBonus.magic);
    readRow("DamageBonusChaos", damageBonus.chaos);
    readRow("DamageBonusHero", damageBonus.hero);
    std::string spellsText = miscData.HasSection("Misc")
        ? miscData.GetString("Misc", "DamageBonusSpells", "")
        : miscData.GetString("GameConstants", "DamageBonusSpells", "");
    if (!spellsText.empty()) {
        ParseBonusRow(spellsText, damageBonus.spells);
    } else {
        std::memcpy(damageBonus.spells, damageBonus.magic, sizeof(damageBonus.spells));
    }

    // Ethereal bonus pair lives in one comma list (ref: index 3 = magic,
    // index 5 = spells); ally bonus and magic-immune gates are ints.
    auto readMiscStr = [&](const char* key) {
        return miscData.HasSection("Misc")
            ? miscData.GetString("Misc", key, "")
            : miscData.GetString("GameConstants", key, "");
    };
    auto readMiscInt = [&](const char* key, int def) {
        if (miscData.HasSection("Misc")) return miscData.GetInt("Misc", key, def);
        return miscData.GetInt("GameConstants", key, def);
    };
    const std::string etherealText = readMiscStr("EtherealDamageBonus");
    if (!etherealText.empty()) {
        std::vector<std::string> parts;
        size_t pos = 0;
        while (true) {
            size_t comma = etherealText.find(',', pos);
            parts.push_back(comma == std::string::npos
                ? etherealText.substr(pos)
                : etherealText.substr(pos, comma - pos));
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
        if (parts.size() > 3 && !parts[3].empty()) {
            etherealDamageBonusMagic = (float)std::atof(parts[3].c_str());
        }
        if (parts.size() > 5 && !parts[5].empty()) {
            etherealDamageBonusSpells = (float)std::atof(parts[5].c_str());
        }
    }
    etherealDamageBonusAlly = readMiscInt("EtherealDamageBonusAlly", 0) != 0;
    magicImmuneResistsDamage = readMiscInt("MagicImmunesResistDamage", 1) != 0;
    magicImmuneResistsUltimates = readMiscInt("MagicImmunesResistUltimates", 1) != 0;
    pawnItemRate = readFloat("PawnItemRate", 0.5f);
    reviveBaseFactor = readFloat("ReviveBaseFactor", 0.40f);
    reviveLevelFactor = readFloat("ReviveLevelFactor", 0.10f);
    reviveBaseLumberFactor = readFloat("ReviveBaseLumberFactor", 0.0f);
    reviveLumberLevelFactor = readFloat("ReviveLumberLevelFactor", 0.0f);
    reviveMaxFactor = readFloat("ReviveMaxFactor", 4.0f);
    heroReviveLifeFactor = readFloat("HeroReviveLifeFactor", 1.0f);
    heroReviveManaFactor = readFloat("HeroReviveManaFactor", 0.0f);
    awakenBaseFactor = readFloat("AwakenBaseFactor", 0.80f);
    awakenLevelFactor = readFloat("AwakenLevelFactor", 0.20f);
    awakenBaseLumberFactor = readFloat("AwakenBaseLumberFactor", 0.80f);
    awakenLumberLevelFactor = readFloat("AwakenLumberLevelFactor", 0.20f);
    awakenMaxFactor = readFloat("AwakenMaxFactor", 8.0f);
    heroAwakenLifeFactor = readFloat("HeroAwakenLifeFactor", 0.5f);
    heroAwakenManaFactor = readFloat("HeroAwakenManaFactor", 0.0f);

    // Hero XP table (ref parseTable): explicit NeedHeroXP prefix wins,
    // the tail extends by formulaA*prev + formulaB*i + formulaC.
    maxHeroLevel = readMiscInt("MaxHeroLevel", 10);
    if (maxHeroLevel < 1) maxHeroLevel = 1;
    if (maxHeroLevel > 100) maxHeroLevel = 100;
    {
        const std::string tableText = readMiscStr("NeedHeroXP");
        const int formulaA = readMiscInt("NeedHeroXPFormulaA", 0);
        const int formulaB = readMiscInt("NeedHeroXPFormulaB", 0);
        const int formulaC = readMiscInt("NeedHeroXPFormulaC", 0);
        std::vector<int> perLevel((size_t)maxHeroLevel + 1, 0);
        std::vector<std::string> parts;
        if (!tableText.empty()) {
            size_t pos = 0;
            while (true) {
                size_t comma = tableText.find(',', pos);
                parts.push_back(comma == std::string::npos
                    ? tableText.substr(pos)
                    : tableText.substr(pos, comma - pos));
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
        }
        // parts[0] feeds level 2 (level 1 needs 0); extra entries ignored.
        for (int lvl = 2; lvl <= maxHeroLevel; ++lvl) {
            const int pi = lvl - 2;
            if (pi < (int)parts.size() && !parts[(size_t)pi].empty()) {
                perLevel[(size_t)lvl] = std::atoi(parts[(size_t)pi].c_str());
            } else if (lvl - 1 >= 2) {
                perLevel[(size_t)lvl] =
                    formulaA * perLevel[(size_t)lvl - 1] + formulaB * (lvl - 1) + formulaC;
            } else {
                perLevel[(size_t)lvl] = formulaB + formulaC;
            }
        }
        needHeroXpSum.assign((size_t)maxHeroLevel + 1, 0);
        int acc = 0;
        needHeroXpSum[0] = 0;
        if (maxHeroLevel >= 1) needHeroXpSum[1] = 0;
        for (int lvl = 2; lvl <= maxHeroLevel; ++lvl) {
            acc += perLevel[(size_t)lvl];
            needHeroXpSum[(size_t)lvl] = acc;
        }
    }
    strHitPointBonus = readFloat("StrHitPointBonus", 25.0f);
    intManaBonus = readFloat("IntManaBonus", 15.0f);
    agiDefenseBonus = readFloat("AgiDefenseBonus", 0.33f);
    reviveTimeFactor = readFloat("ReviveTimeFactor", 0.5f);
    reviveMaxTimeFactor = readFloat("ReviveMaxTimeFactor", 1e9f);

    // Kill-reward tables (ref parseTable: explicit prefix wins, tail extends
    // by formulaA*prev + formulaB*i + formulaC with 0-based i).
    maxUnitLevel = readMiscInt("MaxUnitLevel", 10);
    if (maxUnitLevel < 1) maxUnitLevel = 1;
    if (maxUnitLevel > 100) maxUnitLevel = 100;
    auto parseRewardTable = [&](const char* tableKey, const char* aKey,
                                const char* bKey, const char* cKey, int size) {
        std::vector<int> result((size_t)size, 0);
        const std::string text = readMiscStr(tableKey);
        std::vector<std::string> parts;
        if (!text.empty()) {
            size_t pos = 0;
            while (true) {
                size_t comma = text.find(',', pos);
                parts.push_back(comma == std::string::npos
                    ? text.substr(pos) : text.substr(pos, comma - pos));
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
        }
        const int formulaA = readMiscInt(aKey, 0);
        const int formulaB = readMiscInt(bKey, 0);
        const int formulaC = readMiscInt(cKey, 0);
        for (int i = 0; i < size; ++i) {
            if (i < (int)parts.size() && !parts[(size_t)i].empty()) {
                result[(size_t)i] = std::atoi(parts[(size_t)i].c_str());
            } else if (i > 0) {
                result[(size_t)i] = formulaA * result[(size_t)i - 1] + formulaB * i + formulaC;
            } else {
                result[(size_t)i] = formulaB * 0 + formulaC;
            }
        }
        return result;
    };
    grantHeroXp = parseRewardTable("GrantHeroXP", "GrantHeroXPFormulaA",
                                   "GrantHeroXPFormulaB", "GrantHeroXPFormulaC", maxHeroLevel);
    grantNormalXp = parseRewardTable("GrantNormalXP", "GrantNormalXPFormulaA",
                                     "GrantNormalXPFormulaB", "GrantNormalXPFormulaC", maxUnitLevel);
    {
        const std::string text = readMiscStr("HeroFactorXP");
        if (!text.empty()) {
            heroFactorXp.clear();
            size_t pos = 0;
            while (true) {
                size_t comma = text.find(',', pos);
                const std::string part = comma == std::string::npos
                    ? text.substr(pos) : text.substr(pos, comma - pos);
                if (!part.empty()) heroFactorXp.push_back(std::atoi(part.c_str()));
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
            if (heroFactorXp.empty()) heroFactorXp = {100};
        }
    }
    heroExpRange = readMiscInt("HeroExpRange", 1000);
    globalExperience = readMiscInt("GlobalExperience", 0) != 0;
    buildingKillsGiveExp = readMiscInt("BuildingKillsGiveExp", 0) != 0;
    buildingAngle = readFloat("BuildingAngle", 0.0f);
}

int CGameplayConstants::GetHeroReviveGoldCost(int originalGoldCost, int level) const {
    const int cost = (int)(originalGoldCost * (reviveBaseFactor + reviveLevelFactor * (level - 1)));
    const int cap = (int)(originalGoldCost * reviveMaxFactor);
    return cost < cap ? cost : cap;
}

int CGameplayConstants::GetHeroReviveLumberCost(int originalGoldCost, int level) const {
    // Ref bug-compatible: lumber revives off the GOLD cost (factors are 0).
    const int cost =
        (int)(originalGoldCost * (reviveBaseLumberFactor + reviveLumberLevelFactor * (level - 1)));
    const int cap = (int)(originalGoldCost * reviveMaxFactor);
    return cost < cap ? cost : cap;
}

int CGameplayConstants::GetHeroAwakenGoldCost(int originalGoldCost, int level) const {
    const int cost = (int)(originalGoldCost * (awakenBaseFactor + awakenLevelFactor * (level - 1)));
    const int cap = (int)(originalGoldCost * awakenMaxFactor);
    return cost < cap ? cost : cap;
}

int CGameplayConstants::GetHeroAwakenLumberCost(int originalLumberCost, int level) const {
    const int cost =
        (int)(originalLumberCost * (awakenBaseLumberFactor + awakenLumberLevelFactor * (level - 1)));
    const int cap = (int)(originalLumberCost * awakenMaxFactor);
    return cost < cap ? cost : cap;
}

int CGameplayConstants::GetHeroReviveTime(int originalTime, int level) const {
    const int t = (int)(originalTime * level * reviveTimeFactor);
    const int cap = (int)(originalTime * reviveMaxTimeFactor);
    return t < cap ? t : cap;
}

namespace {

int TableValue(const std::vector<int>& table, int level) {
    if (level <= 0 || table.empty()) return 0;
    if (level > (int)table.size()) level = (int)table.size();
    return table[(size_t)level - 1];
}

} // namespace

int CGameplayConstants::GetGrantHeroXP(int level) const { return TableValue(grantHeroXp, level); }
int CGameplayConstants::GetGrantNormalXP(int level) const { return TableValue(grantNormalXp, level); }
float CGameplayConstants::GetHeroFactorXp(int level) const {
    return (float)TableValue(heroFactorXp, level) / 100.0f;
}
