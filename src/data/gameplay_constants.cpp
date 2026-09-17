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
}
