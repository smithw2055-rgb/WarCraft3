#include "unitdata.h"
#include "gameplay_constants.h"
#include "ecs/unit_type.h"
#include "ecs/unit.h"
#include "ecs/handle_id.h"
#include "resource/mpq_storm.h"
#include <iostream>
#include <algorithm>
#include <set>

static std::string GetStr(const TxtTable& tbl, const std::string& section, const std::string& key,
                          const std::string& def = "") {
    return tbl.GetString(section, key, def);
}

static int GetInt(const TxtTable& tbl, const std::string& section, const std::string& key, int def = 0) {
    return tbl.GetInt(section, key, def);
}

static float GetFloat(const TxtTable& tbl, const std::string& section, const std::string& key, float def = 0) {
    return tbl.GetFloat(section, key, def);
}

CUnitData::CUnitData(const CGameplayConstants& gameplayConstants,
                      const TxtTable& unitDataTbl, const TxtTable& unitAbilities,
                      const TxtTable& unitBalance, const TxtTable& unitWeapons,
                      const TxtTable& unitUI) {
    (void)gameplayConstants;
    (void)unitUI;

    std::set<std::string> allIDs;
    for (auto& s : unitDataTbl.Sections()) allIDs.insert(s);
    for (auto& s : unitBalance.Sections()) allIDs.insert(s);
    for (auto& s : unitAbilities.Sections()) allIDs.insert(s);

    for (auto& sid : allIDs) {
        if (sid.size() != 4) continue;
        War3ID id = War3ID::FromString(sid);

        std::string name = GetStr(unitDataTbl, sid, "Name", sid);
        std::string legacyName = GetStr(unitDataTbl, sid, "LegacyName");
        if (legacyName.empty()) legacyName = name;

        int maxLife = GetInt(unitBalance, sid, "HP", 100);
        float lifeRegen = GetFloat(unitBalance, sid, "regenHP", 0.0f);
        float manaRegen = GetFloat(unitBalance, sid, "regenMana", 0.0f);
        std::string regenTypeStr = GetStr(unitBalance, sid, "regenType");
        CRegenType lifeRegenType = CRegenTypeInfo::FromString(regenTypeStr);

        int manaInitial = GetInt(unitBalance, sid, "mana0", 0);
        int manaMaximum = GetInt(unitBalance, sid, "manaN", 0);
        int speed = GetInt(unitBalance, sid, "spd", 270);
        int defense = GetInt(unitBalance, sid, "def", 0);
        int defenseUpgradeBonus = GetInt(unitBalance, sid, "defUp", 0);
        int sightDay = GetInt(unitBalance, sid, "sightDay", 1200);
        int sightNight = GetInt(unitBalance, sid, "sightNight", 800);
        bool isBldg = GetInt(unitBalance, sid, "isbldg", 0) != 0;
        std::string moveTypeStr = GetStr(unitBalance, sid, "moveType", "foot");
        float defaultFlyingHeight = GetFloat(unitBalance, sid, "flyingHeight", 0.0f);
        float collisionSize = GetFloat(unitBalance, sid, "collision", 32.0f);
        float propWindow = GetFloat(unitBalance, sid, "propWin", 1.0f);
        float turnRate = GetFloat(unitBalance, sid, "turnRate", 0.6f);
        float impactZ = GetFloat(unitBalance, sid, "ImpactZ", 0.0f);
        float deathTime = GetFloat(unitBalance, sid, "deathTime", 1.0f);
        bool canRaise = GetInt(unitBalance, sid, "raise", 0) != 0;
        bool canDecay = GetInt(unitBalance, sid, "decay", 1) != 0;
        float defaultAcq = GetFloat(unitBalance, sid, "acqRange", 0.0f);
        float minAtkRange = GetFloat(unitBalance, sid, "minAtkRange", 0.0f);
        std::string defTypeStr = GetStr(unitBalance, sid, "defType", "none");
        CDefenseType defenseType = CDefenseTypeInfo::FromString(defTypeStr);
        std::string armorType = GetStr(unitBalance, sid, "armorType");

        int goldCost = GetInt(unitBalance, sid, "goldcost", 0);
        int lumberCost = GetInt(unitBalance, sid, "lumbercost", 0);
        int foodUsed = GetInt(unitBalance, sid, "foodUsed", 0);
        int foodMade = GetInt(unitBalance, sid, "foodMade", 0);
        int buildTime = GetInt(unitBalance, sid, "buildTime", 0);
        int level = GetInt(unitBalance, sid, "level", 0);
        bool hero = GetInt(unitBalance, sid, "isHero", 0) != 0;

        int strength = GetInt(unitBalance, sid, "STR", 0);
        float strengthPerLevel = GetFloat(unitBalance, sid, "STRplus", 0.0f);
        int agility = GetInt(unitBalance, sid, "AGI", 0);
        float agilityPerLevel = GetFloat(unitBalance, sid, "AGIplus", 0.0f);
        int intelligence = GetInt(unitBalance, sid, "INT", 0);
        float intPerLevel = GetFloat(unitBalance, sid, "INTplus", 0.0f);
        std::string primAttrStr = GetStr(unitBalance, sid, "primary", "AGI");
        CPrimaryAttribute primaryAttr = CPrimaryAttribute::AGILITY;
        if (primAttrStr == "STR") primaryAttr = CPrimaryAttribute::STRENGTH;
        else if (primAttrStr == "INT") primaryAttr = CPrimaryAttribute::INTELLIGENCE;

        bool canFlee = GetInt(unitBalance, sid, "canFlee", 0) != 0;
        int priority = GetInt(unitBalance, sid, "prio", 0);
        int goldBountyBase = GetInt(unitBalance, sid, "bountyGoldBase", 0);
        int goldBountyDice = GetInt(unitBalance, sid, "bountyGoldDice", 0);
        int goldBountySides = GetInt(unitBalance, sid, "bountyGoldSides", 0);
        int lumberBountyBase = GetInt(unitBalance, sid, "bountyLumberBase", 0);
        int lumberBountyDice = GetInt(unitBalance, sid, "bountyLumberDice", 0);
        int lumberBountySides = GetInt(unitBalance, sid, "bountyLumberSides", 0);

        // Ability list
        std::vector<War3ID> abilityIDs;
        std::string abilStr = GetStr(unitAbilities, sid, "abilList");
        if (!abilStr.empty()) {
            size_t pos = 0;
            while (pos < abilStr.size()) {
                size_t comma = abilStr.find(',', pos);
                std::string code = abilStr.substr(pos, comma - pos);
                code.erase(0, code.find_first_not_of(" \t"));
                code.erase(code.find_last_not_of(" \t") + 1);
                if (code.size() == 4) abilityIDs.push_back(War3ID::FromString(code));
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
        }

        // Hero abilities
        std::vector<War3ID> heroAbilityIDs;
        std::string heroAbilStr = GetStr(unitAbilities, sid, "heroAbilList");
        if (heroAbilStr.empty()) heroAbilStr = GetStr(unitAbilities, sid, "heroabilList");
        if (!heroAbilStr.empty()) {
            size_t pos = 0;
            while (pos < heroAbilStr.size()) {
                size_t comma = heroAbilStr.find(',', pos);
                std::string code = heroAbilStr.substr(pos, comma - pos);
                code.erase(0, code.find_first_not_of(" \t"));
                code.erase(code.find_last_not_of(" \t") + 1);
                if (code.size() == 4) heroAbilityIDs.push_back(War3ID::FromString(code));
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
        }

        // Movement type
        PathingGridMovementType movementType = PathingGridMovementType::FOOT;
        std::string mt = GetStr(unitBalance, sid, "moveType", "foot");
        for (auto& c : mt) c = (char)std::tolower(c);
        if (mt.find("fly") != std::string::npos) movementType = PathingGridMovementType::FLY;
        else if (mt.find("hover") != std::string::npos) movementType = PathingGridMovementType::HOVER;
        else if (mt.find("float") != std::string::npos) movementType = PathingGridMovementType::FLOAT;
        else if (mt.find("amph") != std::string::npos) movementType = PathingGridMovementType::AMPHIBIOUS;

        // Attacks
        std::vector<CUnitAttack> attacks;
        int attacksEnabled = GetInt(unitBalance, sid, "weapsOn", 1);

        for (int ai = 1; ai <= 2; ai++) {
            std::string suffix = std::to_string(ai);
            std::string atkTypeStr = GetStr(unitWeapons, sid, "atkType" + suffix);
            if (atkTypeStr.empty()) continue;

            CUnitAttack atk;
            atk.index = ai - 1;
            atk.attackType = CAttackTypeInfo::FromString(atkTypeStr);
            atk.weaponType = CWeaponTypeInfo::FromString(GetStr(unitWeapons, sid, "weapType" + suffix));
            atk.damageBase = GetInt(unitWeapons, sid, "dmgplus" + suffix, 0);
            atk.damageDice = GetInt(unitWeapons, sid, "dice" + suffix, 0);
            atk.damageSidesPerDie = GetInt(unitWeapons, sid, "sides" + suffix, 0);
            atk.damageUpgradeAmount = GetInt(unitWeapons, sid, "dmgUp" + suffix, 0);
            atk.cooldown = GetFloat(unitWeapons, sid, "cool" + suffix, 1.0f);
            atk.damagePoint = GetFloat(unitWeapons, sid, "dmgpt" + suffix, 0.0f);
            atk.backswingPoint = GetFloat(unitWeapons, sid, "backSw" + suffix, 0.0f);
            atk.range = GetFloat(unitWeapons, sid, "range" + suffix, 100.0f);
            atk.minRange = GetFloat(unitWeapons, sid, "minRange" + suffix, 0.0f);
            atk.projectileSpeed = GetFloat(unitWeapons, sid, "projSpd" + suffix, 0.0f);
            atk.projectileArt = GetStr(unitWeapons, sid, "projArt" + suffix);
            atk.areaFull = GetFloat(unitWeapons, sid, "Farea" + suffix, 0.0f);
            atk.areaHalf = GetFloat(unitWeapons, sid, "Harea" + suffix, 0.0f);
            atk.areaQuarter = GetFloat(unitWeapons, sid, "Qarea" + suffix, 0.0f);
            atk.targetCount = GetInt(unitWeapons, sid, "targCount" + suffix, 0);
            atk.areaSpread = GetFloat(unitWeapons, sid, "areaSpread" + suffix, 0.0f);
            atk.weaponSound = GetStr(unitWeapons, sid, "weapSnd" + suffix);
            attacks.push_back(atk);
        }

        // Hero proper names
        std::vector<std::string> heroNames;
        std::string namesStr = GetStr(unitDataTbl, sid, "Propernames");
        if (!namesStr.empty()) {
            size_t pos = 0;
            while (pos < namesStr.size()) {
                size_t comma = namesStr.find(',', pos);
                std::string n = namesStr.substr(pos, comma - pos);
                n.erase(0, n.find_first_not_of(" \t"));
                n.erase(n.find_last_not_of(" \t") + 1);
                if (!n.empty()) heroNames.push_back(n);
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
        }
        int properNameCount = GetInt(unitBalance, sid, "nameCount", (int)heroNames.size());

        // Structures built / units trained / researches
        std::vector<War3ID> buildsList, trainsList, researchesList, itemsSoldList;

        auto parseList = [](const std::string& str, std::vector<War3ID>& out) {
            if (str.empty()) return;
            size_t pos = 0;
            while (pos < str.size()) {
                size_t comma = str.find(',', pos);
                std::string code = str.substr(pos, comma - pos);
                code.erase(0, code.find_first_not_of(" \t"));
                code.erase(code.find_last_not_of(" \t") + 1);
                if (code.size() == 4) out.push_back(War3ID::FromString(code));
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
        };

        parseList(GetStr(unitBalance, sid, "builds"), buildsList);
        parseList(GetStr(unitBalance, sid, "train"), trainsList);
        if (trainsList.empty()) parseList(GetStr(unitBalance, sid, "trainList"), trainsList);
        parseList(GetStr(unitBalance, sid, "researches"), researchesList);
        parseList(GetStr(unitBalance, sid, "soldItems"), itemsSoldList);

        War3ID defaultAutocast;
        std::string autoStr = GetStr(unitAbilities, sid, "DefaultActiveAbility");
        if (!autoStr.empty()) defaultAutocast = War3ID::FromString(autoStr);

        std::unordered_set<CUnitClassification> emptyClass;
        std::unordered_set<CTargetType> emptyTargeted;
        std::vector<War3ID> emptyVec;
        std::unordered_map<CUpgradeClass, War3ID> emptyUpgradeMap;
        CUnitRace defaultRace;
        std::unordered_set<CBuildingPathingType> emptyPathTypes;
        std::vector<CUnitTypeRequirement> emptyReqs;
        std::vector<std::vector<CUnitTypeRequirement>> emptyTiers;

        auto type = std::make_unique<CUnitType>(
            name, legacyName, id, maxLife, lifeRegen, manaRegen, lifeRegenType,
            manaInitial, manaMaximum, speed, defense, defaultAutocast,
            abilityIDs, isBldg, movementType, defaultFlyingHeight, collisionSize,
            emptyClass,
            attacks, attacksEnabled, armorType, canRaise, canDecay,
            defenseType, impactZ, deathTime,
            emptyTargeted,
            defaultAcq, minAtkRange,
            buildsList, trainsList, researchesList,
            emptyVec, emptyUpgradeMap, emptyVec, itemsSoldList, emptyVec, defaultRace,
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
    TxtTable unitDataTbl, unitAbilities, unitBalance, unitWeapons, unitUI;

    auto loadTxt = [&](TxtTable& tbl, const std::string& path) {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile(path, buf)) {
            tbl.Load(buf.data(), buf.size());
        }
    };

    // Load core data files
    loadTxt(unitDataTbl, "Units\\UnitData.slk");
    loadTxt(unitAbilities, "Units\\UnitAbilities.slk");
    loadTxt(unitBalance, "Units\\UnitBalance.slk");
    loadTxt(unitWeapons, "Units\\UnitWeapons.slk");
    loadTxt(unitUI, "Units\\UnitUI.slk");

    // Load func/string files
    std::vector<std::string> funcFiles = {
        "Units\\CampaignUnitFunc.txt", "Units\\CampaignUnitStrings.txt",
        "Units\\HumanUnitFunc.txt", "Units\\HumanUnitStrings.txt",
        "Units\\NeutralUnitFunc.txt", "Units\\NeutralUnitStrings.txt",
        "Units\\NightElfUnitFunc.txt", "Units\\NightElfUnitStrings.txt",
        "Units\\OrcUnitFunc.txt", "Units\\OrcUnitStrings.txt",
        "Units\\UndeadUnitFunc.txt", "Units\\UndeadUnitStrings.txt",
        "Units\\UnitSkin.txt", "Units\\UnitWeaponsFunc.txt", "Units\\UnitWeaponsSkin.txt"
    };
    for (auto& f : funcFiles) loadTxt(unitDataTbl, f);

    return CUnitData(gameplayConstants, unitDataTbl, unitAbilities, unitBalance, unitWeapons, unitUI);
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
        (void)buildingPathing; // TODO: set building pathing
    }

    (void)simulation;
    return unit;
}

void CUnitData::AddDefaultAbilitiesToUnit(CSimulation& simulation, HandleIdAllocator& handleIdAllocator,
                                           const CUnitType& unitType, bool addDefault,
                                           int manaInitial, int speed, CUnit& unit) const {
    (void)simulation; (void)handleIdAllocator; (void)unitType;
    (void)addDefault; (void)manaInitial; (void)speed; (void)unit;
}

void CUnitData::ApplyPlayerUpgradesToUnit(CSimulation& simulation, int playerIndex,
                                           const CUnitType& unitType, CUnit& unit) const {
    (void)simulation; (void)playerIndex; (void)unitType; (void)unit;
}
