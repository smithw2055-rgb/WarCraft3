#pragma once
#include "scripting/JassNatives.h"
#include "scripting/jass_binder.h"
#include "scripting/jass_animation_tags.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "audio/AudioSystem.h"
#include "ecs/player.h"
#include "ecs/item.h"
#include "ecs/abilities/ability_inventory.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/ability_harvest.h"
#include "ecs/abilities/ability_spell_base.h"
#include "ecs/abilities/ability_queue.h"
#include "ecs/ability.h"
#include "ecs/buff.h"
#include "ecs/buffs.h"
#include "ecs/abilities/ability_sell_items.h"
#include "ecs/abilities/ability_rally.h"
#include "ecs/ability_factory.h"
#include "ecs/destructable.h"
#include "ecs/orders/order.h"
#include "ecs/pathing/world_collision.h"
#include "data/unitdata.h"
#include "data/destructabledata.h"
#include "data/abilitydata.h"
#include "data/itemdata.h"
#include "data/upgradedata.h"
#include "data/combat_types.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <random>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

inline std::string GameCacheSaveDir() {
    std::error_code ec;
    fs::path local = fs::current_path(ec) / "Save";
    if (!ec) {
        std::error_code mk;
        fs::create_directories(local, mk);
        if (!mk && fs::is_directory(local, ec) && !ec) return local.string();
    }
    fs::path dir = fs::temp_directory_path(ec) / "warsmash-save";
    if (!ec) fs::create_directories(dir, ec);
    return dir.string();
}

inline std::string SaveSlotSafeName(const std::string& slot) {
    std::string safe;
    for (char c : slot) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-') {
            safe += c;
        } else {
            safe += '_';
        }
    }
    if (safe.empty() || safe == "." || safe == "..") safe = "quicksave";
    return safe;
}

inline std::string SaveSlotDir(const std::string& slot) {
    return (fs::path(GameCacheSaveDir()) / ("slot_" + SaveSlotSafeName(slot))).string();
}

inline std::string SaveSlotMetadata(const std::string& slot) {
    return (fs::path(SaveSlotDir(slot)) / "save.w3s").string();
}

inline bool WriteSaveSlotMetadata(const std::string& slot, const std::string& mapPath) {
    std::error_code ec;
    fs::create_directories(SaveSlotDir(slot), ec);
    if (ec) return false;
    std::ofstream f(SaveSlotMetadata(slot), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << "WARSMASH-SAVE-1\n";
    f << "slot " << SaveSlotSafeName(slot) << "\n";
    f << "map " << (mapPath.empty() ? "unknown" : mapPath) << "\n";
    f.flush();
    return (bool)f;
}


inline uint32_t& JassFallbackRandState() { static uint32_t s = 0x12345678u; return s; }

inline bool IsPointInJassRegion(const JassRegion& region, float x, float y) {
    for (const auto& r : region.rects) {
        if (x >= r.minX && x <= r.maxX && y >= r.minY && y <= r.maxY) {
            return true;
        }
    }
    for (const auto& c : region.cells) {
        if (x >= c.minX && x <= c.maxX && y >= c.minY && y <= c.maxY) {
            return true;
        }
    }
    return false;
}


// Player-state values mirror the GetPlayerState native.
inline double GetPlayerStateValue(CSimulation& sim, int playerId, int state) {
    CPlayer* p = sim.GetPlayer(playerId);
    if (!p) return 0.0;
    switch (state) {
        case 1: return (double)p->GetGold();
        case 2: return (double)p->GetLumber();
        case 3: return (double)p->GetHeroTokens();
        case 4: return (double)p->GetFoodCap();
        case 5: return (double)p->GetFoodUsed();
        case 6: return p->HasAlliedVictory() ? 1.0 : 0.0;
        case 7: return p->GetGivesBounty() ? 1.0 : 0.0;
        case 8: return (double)p->GetFoodCapMod();
        case 11: return p->IsObserver() ? 1.0 : 0.0;
        case 13: return (double)p->GetGoldUpkeepRate();
        case 14: return (double)p->GetLumberUpkeepRate();
        case 25: return (double)p->GetGameResultCode();
        default: return 0.0;
    }
}



inline int AbilityToHandle(JassNativesData& data, War3ID code, int level = 1, int unitSimId = -1) {
    const int abilityId = data.nextAbilityId++;
    auto ability = std::make_shared<JassAbilityHandle>();
    ability->code = code;
    ability->level = std::max(1, level);
    ability->unitSimId = unitSimId;
    data.abilities[abilityId] = ability;
    return data.AllocHandle(JassHandleInfo::ABILITY, abilityId);
}

inline std::shared_ptr<JassAbilityHandle> HandleToAbility(const std::shared_ptr<JassNativesData>& data,
                                                          int handle) {
    if (!data) return nullptr;
    auto* info = data->GetHandleInfo(handle);
    if (!info || info->type != JassHandleInfo::ABILITY) return nullptr;
    auto it = data->abilities.find(info->id);
    return it != data->abilities.end() ? it->second : nullptr;
}

inline War3ID AbilityCodeFromJassArg(const std::shared_ptr<JassNativesData>& data, int rawOrHandle) {
    auto ability = HandleToAbility(data, rawOrHandle);
    return ability ? ability->code : War3ID(static_cast<uint32_t>(rawOrHandle));
}


// the deterministic sim RNG so map setup stays in sync.
inline uint32_t PickWeightedPoolEntry(const std::vector<std::pair<uint32_t, double>>& entries, double roll01) {
    double total = 0.0;
    for (const auto& e : entries) {
        if (e.second > 0.0) total += e.second;
    }
    if (total <= 0.0 || entries.empty()) return 0;
    double r = roll01 * total;
    for (const auto& e : entries) {
        if (e.second <= 0.0) continue;
        r -= e.second;
        if (r <= 0.0) return e.first;
    }
    return entries.back().first;
}


// ConvertDamageType ids (same table as the UnitDamageTarget path).
inline CAttackType ToScriptAttackType(int ordinal) {
    switch (ordinal) {
        case 0: return CAttackType::NORMAL;
        case 1: return CAttackType::NORMAL; // MELEE has no table column here
        case 2: return CAttackType::PIERCE;
        case 3: return CAttackType::SIEGE;
        case 4: return CAttackType::MAGIC;
        case 5: return CAttackType::CHAOS;
        case 6: return CAttackType::HERO;
        case 7: return CAttackType::SPELLS;
        default: return CAttackType::NORMAL;
    }
}

inline CDamageType ToScriptDamageType(int ordinal) {
    switch (ordinal) {
        case 4: return CDamageType::NORMAL;
        case 5: return CDamageType::ENHANCED;
        case 8: return CDamageType::FIRE;
        case 9: return CDamageType::COLD;
        case 10: return CDamageType::LIGHTNING;
        case 11: return CDamageType::POISON;
        case 12: return CDamageType::DISEASE;
        case 13: return CDamageType::DIVINE;
        case 14: return CDamageType::MAGIC;
        case 15: return CDamageType::SONIC;
        case 16: return CDamageType::ACID;
        case 17: return CDamageType::FORCE;
        case 18: return CDamageType::DEATH;
        case 19: return CDamageType::MIND;
        case 20: return CDamageType::PLANT;
        case 21: return CDamageType::DEFENSIVE;
        case 22: return CDamageType::DEMOLITION;
        case 23: return CDamageType::SLOW_POISON;
        case 24: return CDamageType::SPIRIT_LINK;
        case 25: return CDamageType::SHADOW_STRIKE;
        case 26: return CDamageType::UNIVERSAL;
        default: return CDamageType::NORMAL;
    }
}

// BuffEx matching: positive/negative/timedLife are backed by CBuff state;
// magic/physical/aura/autoDispel have no taxonomy yet and match everything
// (documented approximation; full purge/count-all queries are exact).
inline bool BuffExMatches(const CBuff* buff, bool wantPositive, bool wantNegative, bool timedLifeOnly) {
    if (!buff) return false;
    if (timedLifeOnly && dynamic_cast<const CBuffTimedLife*>(buff) == nullptr) return false;
    if (buff->IsPositive() && !wantPositive) return false;
    if (buff->IsNegative() && !wantNegative) return false;
    return true;
}



inline float JassFallbackRand01() {
    uint32_t& s_fallbackRand = JassFallbackRandState();
    s_fallbackRand = s_fallbackRand * 1664525u + 1013904223u;
    return (float)(s_fallbackRand >> 16) * (1.0f / 65536.0f);
}

inline CItemClass ItemClassFromJassOrdinal(int ordinal) {
    switch (ordinal) {
    case 0: return CItemClass::PERMANENT;
    case 1: return CItemClass::CHARGED;
    case 2: return CItemClass::POWERUP;
    case 3: return CItemClass::ARTIFACT;
    case 4: return CItemClass::PURCHASABLE;
    case 5: return CItemClass::CAMPAIGN;
    case 6: return CItemClass::MISCELLANEOUS;
    case 7: return CItemClass::UNKNOWN;
    case 8: return CItemClass::ANY;
    default: return CItemClass::UNKNOWN;
    }
}

inline std::vector<std::string> ResolveSecondaryTagNames(
    const std::shared_ptr<JassNativesData>& data,
    int secondaryTagsHandle) {
    std::vector<std::string> tags;
    if (!data || secondaryTagsHandle == 0) return tags;

    if (const int ordinal = data->GetSimHandle(secondaryTagsHandle, JassHandleInfo::SECONDARY_TAG); ordinal >= 0) {
        const char* name = JassAnimationSecondaryTagName(ordinal);
        if (name[0]) tags.emplace_back(name);
        return tags;
    }

    const int setId = data->GetSimHandle(secondaryTagsHandle, JassHandleInfo::SECONDARY_TAGS);
    if (setId < 0) return tags;
    auto it = data->secondaryTagSets.find(setId);
    if (it == data->secondaryTagSets.end()) return tags;

    for (int ordinal : it->second) {
        const char* name = JassAnimationSecondaryTagName(ordinal);
        if (name[0]) tags.emplace_back(name);
    }
    std::sort(tags.begin(), tags.end());
    return tags;
}

inline std::string ResolvePrimaryTagName(const std::shared_ptr<JassNativesData>& data, int primaryTagHandle) {
    if (!data || primaryTagHandle == 0) return "";
    const int ordinal = data->GetSimHandle(primaryTagHandle, JassHandleInfo::PRIMARY_TAG);
    if (ordinal < 0) return "";
    return JassAnimationPrimaryTagName(ordinal);
}

inline int ParseOrderString(const std::string& orderStr) {
    const int tableId = OrderIdFromString(orderStr);
    if (tableId != 0) return tableId;
    if (orderStr == "smart") return OrderIds::SMART;
    if (orderStr == "move") return OrderIds::MOVE;
    if (orderStr == "attack") return OrderIds::ATTACK;
    if (orderStr == "attackground") return OrderIds::ATTACK_GROUND;
    if (orderStr == "stop") return OrderIds::STOP;
    if (orderStr == "holdposition") return OrderIds::HOLD_POSITION;
    if (orderStr == "patrol") return OrderIds::PATROL;
    if (orderStr == "harvest") return OrderIds::HARVEST;
    if (orderStr == "returnresources") return OrderIds::RETURN_RESOURCES;
    if (orderStr == "repair") return OrderIds::REPAIR;
    if (orderStr == "setrally") return OrderIds::SET_RALLY;
    if (orderStr == "rally") return OrderIds::RALLY;

    if (orderStr.size() == 4) {
        return ((unsigned char)orderStr[0] << 24) | ((unsigned char)orderStr[1] << 16) |
               ((unsigned char)orderStr[2] << 8) | (unsigned char)orderStr[3];
    }

    return 0;
}

inline int ResolvePlayerId(const std::shared_ptr<JassNativesData>& data, int value) {
    int playerId = data ? data->GetSimHandle(value, JassHandleInfo::PLAYER) : -1;
    if (playerId >= 0) return playerId;
    return (value >= 0 && value < 24) ? value : -1;
}

inline CWidget* ResolveOrderTargetWidget(const std::shared_ptr<JassNativesData>& data,
                                         CSimulation* sim, int targetJassHandle) {
    if (!data || !sim || targetJassHandle <= 0) return nullptr;
    JassHandleInfo* info = data->GetHandleInfo(targetJassHandle);
    if (!info) return nullptr;
    switch (info->type) {
        case JassHandleInfo::UNIT: return sim->GetUnit(info->id);
        case JassHandleInfo::DESTRUCTABLE: return sim->GetDestructable(info->id);
        case JassHandleInfo::ITEM: return sim->GetItem(info->id);
        default: return nullptr;
    }
}

inline int RawcodeFromString(const std::string& value) {
    if (value.size() < 4) return 0;
    return ((unsigned char)value[0] << 24) | ((unsigned char)value[1] << 16) |
           ((unsigned char)value[2] << 8) | (unsigned char)value[3];
}

inline std::string RawcodeToString(int value) {
    char buf[5] = {
        (char)((value >> 24) & 0xFF), (char)((value >> 16) & 0xFF),
        (char)((value >> 8) & 0xFF), (char)(value & 0xFF), 0
    };
    return std::string(buf);
}

inline int ParseRefTargetOrdinal(const std::string& raw) {
    static const std::unordered_map<std::string, int> kTargets = {
        {"air", 0}, {"alive", 1}, {"aliv", 1}, {"allies", 2}, {"alli", 2}, {"ally", 2},
        {"dead", 3}, {"debris", 4}, {"debr", 4}, {"enemies", 5}, {"enem", 5}, {"enemy", 5},
        {"ground", 6}, {"grou", 6}, {"hero", 7}, {"invulnerable", 8}, {"invu", 8},
        {"item", 9}, {"mechanical", 10}, {"mech", 10}, {"neutral", 11}, {"neut", 11},
        {"none", 12}, {"nonhero", 13}, {"nonh", 13}, {"nonsapper", 14},
        {"notself", 15}, {"nots", 15}, {"organic", 16}, {"orga", 16},
        {"player", 17}, {"play", 17}, {"sapper", 18}, {"self", 19},
        {"structure", 20}, {"stru", 20}, {"terrain", 21}, {"terr", 21},
        {"tree", 22}, {"vulnerable", 23}, {"vuln", 23}, {"wall", 24}, {"ward", 25},
        {"ancient", 26}, {"nonancient", 27}, {"friend", 28}, {"frie", 28},
        {"bridge", 29}, {"decoration", 30}, {"deco", 30},
    };
    std::string key = raw;
    for (auto& c : key) c = (char)std::tolower((unsigned char)c);
    auto it = kTargets.find(key);
    return it != kTargets.end() ? it->second : -1;
}

inline int UnitIdFromLegacyName(CSimulation* sim, const std::string& legacyName) {
    if (!sim || !sim->GetUnitData()) return 0;
    for (const auto& [typeId, unitType] : sim->GetUnitData()->GetAllTypes()) {
        if (unitType && unitType->GetLegacyName() == legacyName) {
            return (int)typeId.value;
        }
    }
    return 0;
}

inline std::string UnitLegacyNameFromId(CSimulation* sim, int typeId) {
    if (!sim || !sim->GetUnitData()) return "";
    CUnitType* unitType = sim->GetUnitData()->GetUnitType(War3ID((uint32_t)typeId));
    return unitType ? unitType->GetLegacyName() : "";
}

inline CAbilityHero* FindHeroAbility(CSimulation* sim, int unitSimId) {
    if (!sim) return nullptr;
    CUnit* unit = sim->GetUnit(unitSimId);
    if (!unit || !unit->IsHero()) return nullptr;
    CAbility* ability = unit->GetAbility(War3ID("AHer"));
    return dynamic_cast<CAbilityHero*>(ability);
}

inline int UnitRaceToJassOrdinal(const std::string& raceName) {
    std::string lower;
    lower.reserve(raceName.size());
    for (char c : raceName) lower.push_back((char)std::tolower((unsigned char)c));
    if (lower == "human") return 1;
    if (lower == "orc") return 2;
    if (lower == "undead") return 3;
    if (lower == "nightelf" || lower == "night elf") return 4;
    if (lower == "naga") return 5;
    if (lower == "creep" || lower == "creeps") return 6;
    if (lower == "demon") return 7;
    if (lower == "critter" || lower == "critters") return 8;
    return 9;
}

inline CItem* GetJassItem(const std::shared_ptr<JassNativesData>& data, int jassHandle) {
    if (!data) {
        return nullptr;
    }
    const int itemId = data->GetSimHandle(jassHandle, JassHandleInfo::ITEM);
    if (itemId < 0) {
        return nullptr;
    }
    auto it = data->items.find(itemId);
    return it != data->items.end() ? it->second : nullptr;
}

inline CWidget* GetJassWidget(const std::shared_ptr<JassNativesData>& data,
                              CSimulation* sim, int jassHandle) {
    if (!data || !sim) {
        return nullptr;
    }
    JassHandleInfo* info = data->GetHandleInfo(jassHandle);
    if (!info) {
        return nullptr;
    }
    switch (info->type) {
    case JassHandleInfo::UNIT:
        return sim->GetUnit(info->id);
    case JassHandleInfo::DESTRUCTABLE:
        return sim->GetDestructable(info->id);
    case JassHandleInfo::ITEM:
        return GetJassItem(data, jassHandle);
    default:
        return nullptr;
    }
}

inline bool JassValueToBool(const JassValue& value) {
    switch (value.type) {
    case JassValue::BOOLEAN:
        return value.boolVal;
    case JassValue::INTEGER:
        return value.intVal != 0;
    case JassValue::REAL:
        return value.realVal != 0.0f;
    case JassValue::STRING:
        return !value.strVal.empty();
    case JassValue::NULL_VAL:
    default:
        return false;
    }
}

inline JassValue CallSyncFunction(JassVM* vm, const std::string& funcName) {
    if (!vm || funcName.empty() || !vm->HasFunction(funcName)) return JassValue::Null();
    JassValue result = vm->CallFunction(funcName);
    if (vm->HasPendingSleep()) vm->TakePendingSleepSeconds();
    return result;
}

inline void ExecuteSyncFunction(JassVM* vm, const std::string& funcName) {
    (void)CallSyncFunction(vm, funcName);
}

inline bool TriggerSleepsHonored(const std::shared_ptr<JassNativesData>& data) {
    if (!data) return true;
    auto it = data->triggers.find(data->currentTriggerId);
    if (it == data->triggers.end() || !it->second) return true;
    return it->second->waitOnSleeps;
}
