#include "unitdata.h"
#include "gameplay_constants.h"
#include "ecs/unit_type.h"
#include "ecs/unit.h"
#include "ecs/handle_id.h"
#include "map/object_mods.h"
#include "resource/mpq_storm.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <set>
#include <unordered_map>

namespace {

static std::string TrimCopy(std::string value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

static std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return value;
}

static bool TryGetTxtValue(const TxtTable& tbl, const std::string& section, const std::string& key,
                           std::string& outValue) {
    const auto values = tbl.GetSection(section);
    const auto it = values.find(key);
    if (it != values.end()) {
        outValue = it->second;
        return true;
    }

    const std::string targetKey = ToLowerCopy(key);
    for (const auto& [candidateKey, candidateValue] : values) {
        if (ToLowerCopy(candidateKey) == targetKey) {
            outValue = candidateValue;
            return true;
        }
    }
    return false;
}

struct SlkAccessor {
    const SlkTable* table = nullptr;
    int idColumn = -1;
    std::unordered_map<std::string, int> columns;
    std::unordered_map<std::string, int> rowsById;

    SlkAccessor() = default;

    SlkAccessor(const SlkTable& slk, std::initializer_list<const char*> idColumnCandidates) {
        Init(slk, idColumnCandidates);
    }

    void Init(const SlkTable& slk, std::initializer_list<const char*> idColumnCandidates) {
        table = &slk;
        columns.clear();
        rowsById.clear();
        idColumn = -1;

        for (int i = 0; i < slk.Cols(); ++i) {
            columns[slk.colNames_[i]] = i;
        }

        for (const char* candidate : idColumnCandidates) {
            auto it = columns.find(candidate);
            if (it != columns.end()) {
                idColumn = it->second;
                break;
            }
        }
        if (idColumn < 0 && slk.Cols() > 0) {
            idColumn = 0;
        }

        if (idColumn < 0) {
            return;
        }

        for (int row = 0; row < slk.Rows(); ++row) {
            const std::string id = TrimCopy(slk.GetString(row, idColumn));
            if (!id.empty()) {
                rowsById[id] = row;
            }
        }
    }

    bool TryGetValue(const std::string& id, const std::string& key, std::string& outValue) const {
        if (!table) {
            return false;
        }
        const auto rowIt = rowsById.find(id);
        if (rowIt == rowsById.end()) {
            return false;
        }
        const auto colIt = columns.find(key);
        if (colIt == columns.end()) {
            return false;
        }
        outValue = table->GetString(rowIt->second, colIt->second);
        return !outValue.empty();
    }
};

static std::string GetMergedString(const TxtTable& txt, const SlkAccessor& slk,
                                   const std::string& section, const std::string& key,
                                   const std::string& def = "") {
    std::string value;
    if (TryGetTxtValue(txt, section, key, value)) {
        return value;
    }
    if (slk.TryGetValue(section, key, value)) {
        return value;
    }
    return def;
}

static int GetMergedInt(const TxtTable& txt, const SlkAccessor& slk,
                        const std::string& section, const std::string& key, int def = 0) {
    const std::string value = GetMergedString(txt, slk, section, key);
    if (value.empty()) {
        return def;
    }
    return std::atoi(value.c_str());
}

static float GetMergedFloat(const TxtTable& txt, const SlkAccessor& slk,
                            const std::string& section, const std::string& key, float def = 0.0f) {
    const std::string value = GetMergedString(txt, slk, section, key);
    if (value.empty()) {
        return def;
    }
    return std::strtof(value.c_str(), nullptr);
}

static void CollectAllIds(const TxtTable& txt, const SlkAccessor& slk, std::set<std::string>& outIds) {
    for (const auto& section : txt.Sections()) {
        if (section.size() == 4) {
            outIds.insert(section);
        }
    }
    for (const auto& [id, row] : slk.rowsById) {
        (void)row;
        if (id.size() == 4) {
            outIds.insert(id);
        }
    }
}

static TxtTable* SelectUnitOverlayTable(const std::string& slkName,
                                        TxtTable& unitDataTxt,
                                        TxtTable& unitAbilitiesTxt,
                                        TxtTable& unitBalanceTxt,
                                        TxtTable& unitWeaponsTxt,
                                        TxtTable& unitUITxt) {
    const std::string lower = ToLowerCopy(slkName);
    if (lower.find("unitabilities") != std::string::npos) return &unitAbilitiesTxt;
    if (lower.find("unitbalance") != std::string::npos) return &unitBalanceTxt;
    if (lower.find("unitweapons") != std::string::npos) return &unitWeaponsTxt;
    if (lower.find("unitui") != std::string::npos) return &unitUITxt;
    if (lower.find("unitdata") != std::string::npos) return &unitDataTxt;
    return nullptr;
}

static std::string FormatObjectModificationValue(const ObjectModification& mod) {
    switch (mod.varType) {
    case ObjectModification::INT:
    case ObjectModification::BOOLEAN:
        return std::to_string(mod.intValue);
    case ObjectModification::STRING:
        return mod.stringValue;
    case ObjectModification::REAL:
    case ObjectModification::UNREAL:
    default: {
        std::ostringstream ss;
        ss << mod.realValue;
        return ss.str();
    }
    }
}

static bool ResolveObjectModField(const SlkAccessor& meta, const ObjectModification& mod,
                                  std::string& outSlk, std::string& outField) {
    const std::string metaId = mod.id.ToString();
    if (!meta.TryGetValue(metaId, "slk", outSlk) || outSlk.empty()) return false;
    if (!meta.TryGetValue(metaId, "field", outField) || outField.empty()) return false;

    int data = 0;
    std::string dataValue;
    if (meta.TryGetValue(metaId, "data", dataValue) && !dataValue.empty()) {
        data = std::atoi(dataValue.c_str());
    }
    if (data > 0) {
        outField.push_back(static_cast<char>('A' + data - 1));
    }

    int index = -1;
    std::string indexValue;
    if (meta.TryGetValue(metaId, "index", indexValue) && !indexValue.empty()) {
        index = std::atoi(indexValue.c_str());
    }

    int repeat = 0;
    std::string repeatValue;
    if (meta.TryGetValue(metaId, "repeat", repeatValue) && !repeatValue.empty()) {
        repeat = std::atoi(repeatValue.c_str());
    }

    std::string appendIndexMode;
    meta.TryGetValue(metaId, "appendIndex", appendIndexMode);
    int level = mod.level;
    if (repeat > 0) {
        if (appendIndexMode == "1") {
            const int upgradeExtensionLevel = level - 1;
            if (upgradeExtensionLevel > 0) {
                outField += std::to_string(upgradeExtensionLevel);
            }
        } else if (appendIndexMode == "0") {
            // Indexed list storage is collapsed into one comma field in this C++ data path.
        } else if (index == -1 || repeat >= 10) {
            if (level == 0) level = 1;
            if (repeat >= 10 && level >= 0 && level < 100) {
                if (level < 10) outField.push_back('0');
                outField += std::to_string(level);
            } else {
                outField += std::to_string(level);
            }
        }
    }
    return true;
}

static void ApplyMapUnitObjectMods(const MpqManager& mpq, const std::string& mapPath,
                                   TxtTable& unitDataTxt,
                                   TxtTable& unitAbilitiesTxt,
                                   TxtTable& unitBalanceTxt,
                                   TxtTable& unitWeaponsTxt,
                                   TxtTable& unitUITxt) {
    if (mapPath.empty()) return;

    ObjectModificationSet mods;
    if (!LoadWar3MapUnitMods(mapPath, mods)) {
        std::cout << "ObjectMods: no war3map.w3u unit modifications" << std::endl;
        return;
    }

    SlkTable unitMetaSlk;
    std::vector<uint8_t> metaBuf;
    if (!mpq.ReadFile("Units\\UnitMetaData.slk", metaBuf) ||
        !unitMetaSlk.Load(metaBuf.data(), metaBuf.size())) {
        std::cerr << "ObjectMods: UnitMetaData.slk unavailable; unit modifications skipped" << std::endl;
        return;
    }

    const SlkAccessor meta(unitMetaSlk, {"ID"});
    int applied = 0;
    int unknown = 0;
    for (const auto& entry : mods.original) {
        const std::string section = entry.oldId.ToString();
        if (section.size() != 4) continue;
        for (const auto& mod : entry.modifications) {
            std::string slkName;
            std::string fieldName;
            if (!ResolveObjectModField(meta, mod, slkName, fieldName)) {
                unknown++;
                continue;
            }
            TxtTable* target = SelectUnitOverlayTable(
                slkName, unitDataTxt, unitAbilitiesTxt, unitBalanceTxt, unitWeaponsTxt, unitUITxt);
            if (!target) {
                unknown++;
                continue;
            }
            target->SetString(section, fieldName, FormatObjectModificationValue(mod));
            applied++;
        }
    }

    std::cout << "ObjectMods: war3map.w3u original=" << mods.original.size()
              << " custom=" << mods.custom.size()
              << " applied=" << applied
              << " unknown=" << unknown;
    if (!mods.custom.empty()) {
        std::cout << " customSkipped=" << mods.custom.size();
    }
    std::cout << std::endl;
}

static void ParseWar3IdList(const std::string& str, std::vector<War3ID>& out) {
    if (str.empty()) {
        return;
    }
    size_t pos = 0;
    while (pos < str.size()) {
        const size_t comma = str.find(',', pos);
        std::string code = TrimCopy(str.substr(pos, comma - pos));
        if (code.size() == 4) {
            out.push_back(War3ID::FromString(code));
        }
        if (comma == std::string::npos) {
            break;
        }
        pos = comma + 1;
    }
}

static void ParseStringList(const std::string& str, std::vector<std::string>& out) {
    if (str.empty()) {
        return;
    }
    size_t pos = 0;
    while (pos < str.size()) {
        const size_t comma = str.find(',', pos);
        std::string value = TrimCopy(str.substr(pos, comma - pos));
        if (!value.empty()) {
            out.push_back(std::move(value));
        }
        if (comma == std::string::npos) {
            break;
        }
        pos = comma + 1;
    }
}

static void ParseAnimationTagList(const std::string& str, std::vector<std::string>& out) {
    std::vector<std::string> parsed;
    ParseStringList(str, parsed);
    for (std::string& value : parsed) {
        value = ToLowerCopy(TrimCopy(value));
        if (value.empty()) {
            continue;
        }
        if (std::find(out.begin(), out.end(), value) == out.end()) {
            out.push_back(std::move(value));
        }
    }
}

} // namespace

CUnitData::CUnitData(const CGameplayConstants& gameplayConstants,
                     const SlkTable& unitDataSlk, const TxtTable& unitDataTxt,
                     const SlkTable& unitAbilitiesSlk, const TxtTable& unitAbilitiesTxt,
                     const SlkTable& unitBalanceSlk, const TxtTable& unitBalanceTxt,
                     const SlkTable& unitWeaponsSlk, const TxtTable& unitWeaponsTxt,
                     const SlkTable& unitUISlk, const TxtTable& unitUITxt) {
    (void)gameplayConstants;
    const SlkAccessor unitData(unitDataSlk, {"unitID", "unitDataID"});
    const SlkAccessor unitAbilities(unitAbilitiesSlk, {"unitAbilID", "unitAbilitiesID", "unitID"});
    const SlkAccessor unitBalance(unitBalanceSlk, {"unitBalanceID", "unitID"});
    const SlkAccessor unitWeapons(unitWeaponsSlk, {"unitWeapID", "unitWeaponsID", "unitID"});
    const SlkAccessor unitUI(unitUISlk, {"unitUIID", "unitID"});

    std::set<std::string> allIDs;
    CollectAllIds(unitDataTxt, unitData, allIDs);
    CollectAllIds(unitAbilitiesTxt, unitAbilities, allIDs);
    CollectAllIds(unitBalanceTxt, unitBalance, allIDs);
    CollectAllIds(unitWeaponsTxt, unitWeapons, allIDs);

    for (const auto& sid : allIDs) {
        if (sid.size() != 4) {
            continue;
        }

        const War3ID id = War3ID::FromString(sid);

        std::string name = GetMergedString(unitDataTxt, unitData, sid, "Name", sid);
        std::string legacyName = GetMergedString(unitDataTxt, unitData, sid, "LegacyName");
        if (legacyName.empty()) {
            legacyName = name;
        }

        const int maxLife = GetMergedInt(unitBalanceTxt, unitBalance, sid, "HP", 100);
        const float lifeRegen = GetMergedFloat(unitBalanceTxt, unitBalance, sid, "regenHP", 0.0f);
        const float manaRegen = GetMergedFloat(unitBalanceTxt, unitBalance, sid, "regenMana", 0.0f);
        const std::string regenTypeStr = GetMergedString(unitBalanceTxt, unitBalance, sid, "regenType");
        const CRegenType lifeRegenType = CRegenTypeInfo::FromString(regenTypeStr);

        const int manaInitial = GetMergedInt(unitBalanceTxt, unitBalance, sid, "mana0", 0);
        const int manaMaximum = GetMergedInt(unitBalanceTxt, unitBalance, sid, "manaN", 0);
        const int speed = GetMergedInt(unitBalanceTxt, unitBalance, sid, "spd", 270);
        const int defense = GetMergedInt(unitBalanceTxt, unitBalance, sid, "def", 0);
        const int defenseUpgradeBonus = GetMergedInt(unitBalanceTxt, unitBalance, sid, "defUp", 0);
        const int sightDay = GetMergedInt(unitBalanceTxt, unitBalance, sid, "sightDay", 1200);
        const int sightNight = GetMergedInt(unitBalanceTxt, unitBalance, sid, "sightNight", 800);
        bool isBldg = GetMergedInt(unitBalanceTxt, unitBalance, sid, "isbldg",
                                   GetMergedInt(unitDataTxt, unitData, sid, "isbldg", 0)) != 0;
        const std::string moveTypeStr = GetMergedString(unitBalanceTxt, unitBalance, sid, "moveType", "foot");
        const float defaultFlyingHeight = GetMergedFloat(unitBalanceTxt, unitBalance, sid, "flyingHeight", 0.0f);
        const float collisionSize = GetMergedFloat(unitBalanceTxt, unitBalance, sid, "collision", 32.0f);
        const float propWindow = GetMergedFloat(unitBalanceTxt, unitBalance, sid, "propWin", 1.0f);
        const float turnRate = GetMergedFloat(unitBalanceTxt, unitBalance, sid, "turnRate", 0.6f);
        const float impactZ = GetMergedFloat(unitBalanceTxt, unitBalance, sid, "ImpactZ", 0.0f);
        const float deathTime = GetMergedFloat(unitBalanceTxt, unitBalance, sid, "deathTime", 1.0f);
        const bool canRaise = GetMergedInt(unitBalanceTxt, unitBalance, sid, "raise", 0) != 0;
        const bool canDecay = GetMergedInt(unitBalanceTxt, unitBalance, sid, "decay", 1) != 0;
        const float defaultAcq = GetMergedFloat(unitBalanceTxt, unitBalance, sid, "acqRange", 0.0f);
        const float minAtkRange = GetMergedFloat(unitBalanceTxt, unitBalance, sid, "minAtkRange", 0.0f);
        const std::string defTypeStr = GetMergedString(unitBalanceTxt, unitBalance, sid, "defType", "none");
        const CDefenseType defenseType = CDefenseTypeInfo::FromString(defTypeStr);
        const std::string armorType = GetMergedString(unitBalanceTxt, unitBalance, sid, "armorType");

        const int goldCost = GetMergedInt(unitBalanceTxt, unitBalance, sid, "goldcost", 0);
        const int lumberCost = GetMergedInt(unitBalanceTxt, unitBalance, sid, "lumbercost", 0);
        const int foodUsed = GetMergedInt(unitBalanceTxt, unitBalance, sid, "foodUsed", 0);
        const int foodMade = GetMergedInt(unitBalanceTxt, unitBalance, sid, "foodMade", 0);
        const int buildTime = GetMergedInt(unitBalanceTxt, unitBalance, sid, "buildTime", 0);
        const int level = GetMergedInt(unitBalanceTxt, unitBalance, sid, "level", 0);
        const bool hero = GetMergedInt(unitBalanceTxt, unitBalance, sid, "isHero", 0) != 0;

        const int strength = GetMergedInt(unitBalanceTxt, unitBalance, sid, "STR", 0);
        const float strengthPerLevel = GetMergedFloat(unitBalanceTxt, unitBalance, sid, "STRplus", 0.0f);
        const int agility = GetMergedInt(unitBalanceTxt, unitBalance, sid, "AGI", 0);
        const float agilityPerLevel = GetMergedFloat(unitBalanceTxt, unitBalance, sid, "AGIplus", 0.0f);
        const int intelligence = GetMergedInt(unitBalanceTxt, unitBalance, sid, "INT", 0);
        const float intPerLevel = GetMergedFloat(unitBalanceTxt, unitBalance, sid, "INTplus", 0.0f);
        const std::string primAttrStr = GetMergedString(unitBalanceTxt, unitBalance, sid, "primary", "AGI");
        CPrimaryAttribute primaryAttr = CPrimaryAttribute::AGILITY;
        if (primAttrStr == "STR") {
            primaryAttr = CPrimaryAttribute::STRENGTH;
        } else if (primAttrStr == "INT") {
            primaryAttr = CPrimaryAttribute::INTELLIGENCE;
        }

        const bool canFlee = GetMergedInt(unitBalanceTxt, unitBalance, sid, "canFlee", 0) != 0;
        const int priority = GetMergedInt(unitBalanceTxt, unitBalance, sid, "prio", 0);
        const int goldBountyBase = GetMergedInt(unitBalanceTxt, unitBalance, sid, "bountyGoldBase", 0);
        const int goldBountyDice = GetMergedInt(unitBalanceTxt, unitBalance, sid, "bountyGoldDice", 0);
        const int goldBountySides = GetMergedInt(unitBalanceTxt, unitBalance, sid, "bountyGoldSides", 0);
        const int lumberBountyBase = GetMergedInt(unitBalanceTxt, unitBalance, sid, "bountyLumberBase", 0);
        const int lumberBountyDice = GetMergedInt(unitBalanceTxt, unitBalance, sid, "bountyLumberDice", 0);
        const int lumberBountySides = GetMergedInt(unitBalanceTxt, unitBalance, sid, "bountyLumberSides", 0);

        std::vector<War3ID> abilityIDs;
        const std::string rawAbilityList = GetMergedString(unitAbilitiesTxt, unitAbilities, sid, "abilList");
        ParseWar3IdList(rawAbilityList, abilityIDs);

        std::vector<War3ID> heroAbilityIDs;
        std::string heroAbilStr = GetMergedString(unitAbilitiesTxt, unitAbilities, sid, "heroAbilList");
        if (heroAbilStr.empty()) {
            heroAbilStr = GetMergedString(unitAbilitiesTxt, unitAbilities, sid, "heroabilList");
        }
        ParseWar3IdList(heroAbilStr, heroAbilityIDs);

        PathingGridMovementType movementType = PathingGridMovementType::FOOT;
        std::string mt = moveTypeStr;
        std::transform(mt.begin(), mt.end(), mt.begin(), [](unsigned char c) {
            return (char)std::tolower(c);
        });
        if (mt.find("fly") != std::string::npos) {
            movementType = PathingGridMovementType::FLY;
        } else if (mt.find("hover") != std::string::npos) {
            movementType = PathingGridMovementType::HOVER;
        } else if (mt.find("float") != std::string::npos) {
            movementType = PathingGridMovementType::FLOAT;
        } else if (mt.find("amph") != std::string::npos) {
            movementType = PathingGridMovementType::AMPHIBIOUS;
        }

        std::vector<CUnitAttack> attacks;
        const int attacksEnabled = GetMergedInt(unitBalanceTxt, unitBalance, sid, "weapsOn", 1);
        for (int ai = 1; ai <= 2; ++ai) {
            const std::string suffix = std::to_string(ai);
            const std::string atkTypeStr = GetMergedString(unitWeaponsTxt, unitWeapons, sid, "atkType" + suffix);
            if (atkTypeStr.empty()) {
                continue;
            }

            CUnitAttack atk;
            atk.index = ai - 1;
            atk.attackType = CAttackTypeInfo::FromString(atkTypeStr);
            atk.weaponType = CWeaponTypeInfo::FromString(
                GetMergedString(unitWeaponsTxt, unitWeapons, sid, "weapType" + suffix));
            atk.damageBase = GetMergedInt(unitWeaponsTxt, unitWeapons, sid, "dmgplus" + suffix, 0);
            atk.damageDice = GetMergedInt(unitWeaponsTxt, unitWeapons, sid, "dice" + suffix, 0);
            atk.damageSidesPerDie = GetMergedInt(unitWeaponsTxt, unitWeapons, sid, "sides" + suffix, 0);
            atk.damageUpgradeAmount = GetMergedInt(unitWeaponsTxt, unitWeapons, sid, "dmgUp" + suffix, 0);
            atk.cooldown = GetMergedFloat(unitWeaponsTxt, unitWeapons, sid, "cool" + suffix, 1.0f);
            atk.damagePoint = GetMergedFloat(unitWeaponsTxt, unitWeapons, sid, "dmgpt" + suffix, 0.0f);
            atk.backswingPoint = GetMergedFloat(unitWeaponsTxt, unitWeapons, sid, "backSw" + suffix, 0.0f);
            atk.range = GetMergedFloat(unitWeaponsTxt, unitWeapons, sid, "range" + suffix, 100.0f);
            atk.minRange = GetMergedFloat(unitWeaponsTxt, unitWeapons, sid, "minRange" + suffix, 0.0f);
            atk.projectileSpeed = GetMergedFloat(unitWeaponsTxt, unitWeapons, sid, "projSpd" + suffix, 0.0f);
            atk.projectileArt = GetMergedString(unitWeaponsTxt, unitWeapons, sid, "projArt" + suffix);
            atk.areaFull = GetMergedFloat(unitWeaponsTxt, unitWeapons, sid, "Farea" + suffix, 0.0f);
            atk.areaHalf = GetMergedFloat(unitWeaponsTxt, unitWeapons, sid, "Harea" + suffix, 0.0f);
            atk.areaQuarter = GetMergedFloat(unitWeaponsTxt, unitWeapons, sid, "Qarea" + suffix, 0.0f);
            atk.targetCount = GetMergedInt(unitWeaponsTxt, unitWeapons, sid, "targCount" + suffix, 0);
            atk.areaSpread = GetMergedFloat(unitWeaponsTxt, unitWeapons, sid, "areaSpread" + suffix, 0.0f);
            atk.weaponSound = GetMergedString(unitWeaponsTxt, unitWeapons, sid, "weapSnd" + suffix);
            attacks.push_back(atk);
        }

        std::vector<std::string> heroNames;
        ParseStringList(GetMergedString(unitDataTxt, unitData, sid, "Propernames"), heroNames);
        const int properNameCount = GetMergedInt(unitBalanceTxt, unitBalance, sid, "nameCount",
                                                 (int)heroNames.size());

        std::vector<std::string> requiredAnimationTags;
        ParseAnimationTagList(GetMergedString(unitUITxt, unitUI, sid, "animProps"), requiredAnimationTags);
        ParseAnimationTagList(GetMergedString(unitDataTxt, unitData, sid, "animProps"), requiredAnimationTags);

        std::vector<War3ID> buildsList;
        std::vector<War3ID> trainsList;
        std::vector<War3ID> researchesList;
        std::vector<War3ID> upgradesToList;
        std::vector<War3ID> itemsSoldList;
        const std::string rawBuilds = GetMergedString(unitBalanceTxt, unitBalance, sid, "builds");
        const std::string rawTrain = GetMergedString(unitBalanceTxt, unitBalance, sid, "train");
        const std::string rawTrains = GetMergedString(unitBalanceTxt, unitBalance, sid, "trains");
        const std::string rawTrainList = GetMergedString(unitBalanceTxt, unitBalance, sid, "trainList");
        const std::string rawResearches = GetMergedString(unitBalanceTxt, unitBalance, sid, "researches");
        const std::string rawUpgradesTo = GetMergedString(unitBalanceTxt, unitBalance, sid, "Upgrade",
            GetMergedString(unitDataTxt, unitData, sid, "Upgrade"));
        const std::string rawRace = GetMergedString(unitBalanceTxt, unitBalance, sid, "race",
            GetMergedString(unitDataTxt, unitData, sid, "race"));
        ParseWar3IdList(rawBuilds, buildsList);
        ParseWar3IdList(rawTrain, trainsList);
        if (trainsList.empty()) {
            ParseWar3IdList(rawTrains, trainsList);
        }
        if (trainsList.empty()) {
            ParseWar3IdList(rawTrainList, trainsList);
        }
        ParseWar3IdList(rawResearches, researchesList);
        ParseWar3IdList(rawUpgradesTo, upgradesToList);
        ParseWar3IdList(GetMergedString(unitBalanceTxt, unitBalance, sid, "soldItems"), itemsSoldList);
        if (itemsSoldList.empty()) {
            ParseWar3IdList(GetMergedString(unitBalanceTxt, unitBalance, sid, "sellitems"), itemsSoldList);
        }

        auto hasAbility = [&](War3ID abilityId) {
            return std::find(abilityIDs.begin(), abilityIDs.end(), abilityId) != abilityIDs.end();
        };
        if (!isBldg) {
            if (hasAbility(War3ID("Aque")) || hasAbility(War3ID("ARal")) ||
                hasAbility(War3ID("Aret")) || hasAbility(War3ID("Agld")) ||
                hasAbility(War3ID("Aroo")) || hasAbility(War3ID("Aro1")) ||
                !trainsList.empty() || !researchesList.empty() || !itemsSoldList.empty() ||
                (speed <= 0 && collisionSize >= 48.0f)) {
                isBldg = true;
            }
        }

        War3ID defaultAutocast;
        const std::string autoStr = GetMergedString(unitAbilitiesTxt, unitAbilities, sid, "DefaultActiveAbility");
        if (autoStr.size() == 4) {
            defaultAutocast = War3ID::FromString(autoStr);
        }

        std::unordered_set<CUnitClassification> emptyClass;
        std::unordered_set<CTargetType> emptyTargeted;
        std::vector<War3ID> emptyVec;
        std::unordered_map<CUpgradeClass, War3ID> emptyUpgradeMap;
        CUnitRace defaultRace;
        defaultRace.name = ToLowerCopy(rawRace);
        std::unordered_set<CBuildingPathingType> emptyPathTypes;
        std::vector<CUnitTypeRequirement> emptyReqs;
        std::vector<std::vector<CUnitTypeRequirement>> emptyTiers;

        auto type = std::make_unique<CUnitType>(
            name, legacyName, id, maxLife, lifeRegen, manaRegen, lifeRegenType,
            manaInitial, manaMaximum, speed, defense, defaultAutocast,
            abilityIDs, isBldg, movementType, defaultFlyingHeight, collisionSize,
            requiredAnimationTags,
            emptyClass,
            attacks, attacksEnabled, armorType, canRaise, canDecay,
            defenseType, impactZ, deathTime,
            emptyTargeted,
            defaultAcq, minAtkRange,
            buildsList, trainsList, researchesList,
            emptyVec, emptyUpgradeMap, upgradesToList, itemsSoldList, emptyVec, defaultRace,
            goldCost, lumberCost, foodUsed, foodMade, buildTime,
            goldCost / 2, lumberCost / 2, buildTime / 2,
            emptyPathTypes, emptyPathTypes, propWindow, turnRate,
            emptyReqs, emptyTiers, level, hero,
            strength, strengthPerLevel, agility, agilityPerLevel,
            intelligence, intPerLevel, primaryAttr,
            heroAbilityIDs, heroNames, properNameCount,
            canFlee, priority, false, 0, 0.0f, 0.0f,
            false, false, defenseUpgradeBonus,
            sightDay, sightNight, false,
            goldBountyBase, goldBountyDice, goldBountySides,
            lumberBountyBase, lumberBountyDice, lumberBountySides,
            false);

        types_[id] = std::move(type);

    }

    std::cout << "CUnitData: loaded " << types_.size() << " unit types" << std::endl;
}

CUnitData CUnitData::LoadFromMpq(const MpqManager& mpq,
                                 const CGameplayConstants& gameplayConstants) {
    return LoadFromMpq(mpq, gameplayConstants, std::string());
}

CUnitData CUnitData::LoadFromMpq(const MpqManager& mpq,
                                 const CGameplayConstants& gameplayConstants,
                                 const std::string& mapPath) {
    SlkTable unitDataSlk;
    SlkTable unitAbilitiesSlk;
    SlkTable unitBalanceSlk;
    SlkTable unitWeaponsSlk;
    SlkTable unitUISlk;
    TxtTable unitDataTxt;
    TxtTable unitAbilitiesTxt;
    TxtTable unitBalanceTxt;
    TxtTable unitWeaponsTxt;
    TxtTable unitUITxt;

    auto loadSlk = [&](SlkTable& tbl, const std::string& path) {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile(path, buf)) {
            tbl.Load(buf.data(), buf.size());
        }
    };
    auto loadTxt = [&](TxtTable& tbl, const std::string& path) {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile(path, buf)) {
            tbl.Load(buf.data(), buf.size());
        }
    };
    auto loadOverlayEverywhere = [&](const std::string& path) {
        loadTxt(unitDataTxt, path);
        loadTxt(unitAbilitiesTxt, path);
        loadTxt(unitBalanceTxt, path);
        loadTxt(unitWeaponsTxt, path);
        loadTxt(unitUITxt, path);
    };

    loadSlk(unitDataSlk, "Units\\UnitData.slk");
    loadSlk(unitAbilitiesSlk, "Units\\UnitAbilities.slk");
    loadSlk(unitBalanceSlk, "Units\\UnitBalance.slk");
    loadSlk(unitWeaponsSlk, "Units\\UnitWeapons.slk");
    loadSlk(unitUISlk, "Units\\UnitUI.slk");

    const std::vector<std::string> overlayFiles = {
        "Units\\CampaignUnitFunc.txt",
        "Units\\CampaignUnitStrings.txt",
        "Units\\HumanUnitFunc.txt",
        "Units\\HumanUnitStrings.txt",
        "Units\\NeutralUnitFunc.txt",
        "Units\\NeutralUnitStrings.txt",
        "Units\\NightElfUnitFunc.txt",
        "Units\\NightElfUnitStrings.txt",
        "Units\\OrcUnitFunc.txt",
        "Units\\OrcUnitStrings.txt",
        "Units\\UndeadUnitFunc.txt",
        "Units\\UndeadUnitStrings.txt",
        "Units\\UnitSkin.txt",
        "Units\\UnitWeaponsFunc.txt",
        "Units\\UnitWeaponsSkin.txt",
        "war3mapSkin.txt"
    };
    for (const auto& file : overlayFiles) {
        loadOverlayEverywhere(file);
    }
    ApplyMapUnitObjectMods(mpq, mapPath,
                           unitDataTxt, unitAbilitiesTxt, unitBalanceTxt,
                           unitWeaponsTxt, unitUITxt);

    return CUnitData(gameplayConstants,
                     unitDataSlk, unitDataTxt,
                     unitAbilitiesSlk, unitAbilitiesTxt,
                     unitBalanceSlk, unitBalanceTxt,
                     unitWeaponsSlk, unitWeaponsTxt,
                     unitUISlk, unitUITxt);
}

CUnitType* CUnitData::GetUnitType(War3ID typeId) const {
    auto it = types_.find(typeId);
    return (it != types_.end()) ? it->second.get() : nullptr;
}

bool CUnitData::HasUnitType(War3ID typeId) const {
    return types_.find(typeId) != types_.end();
}

CUnit* CUnitData::CreateUnit(CSimulation& simulation, int playerIndex,
                             War3ID typeId, float x, float y, float facing,
                             HandleIdAllocator& handleIdAllocator,
                             const std::vector<uint8_t>* buildingPathing) const {
    auto* type = GetUnitType(typeId);
    if (!type) return nullptr;

    int handleId = handleIdAllocator.CreateId();
    auto* unit = new CUnit(handleId, type, playerIndex, typeId, x, y, facing);

    if (buildingPathing) {
        (void)buildingPathing;
    }

    (void)simulation;
    return unit;
}

void CUnitData::AddDefaultAbilitiesToUnit(CSimulation& simulation, HandleIdAllocator& handleIdAllocator,
                                          const CUnitType& unitType, bool addDefault,
                                          int manaInitial, int speed, CUnit& unit) const {
    (void)simulation;
    (void)handleIdAllocator;
    (void)unitType;
    (void)addDefault;
    (void)manaInitial;
    (void)speed;
    (void)unit;
}

void CUnitData::ApplyPlayerUpgradesToUnit(CSimulation& simulation, int playerIndex,
                                          const CUnitType& unitType, CUnit& unit) const {
    (void)simulation;
    (void)playerIndex;
    (void)unitType;
    (void)unit;
}
