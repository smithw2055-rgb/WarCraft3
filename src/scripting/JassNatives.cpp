#include "JassNatives.h"
#include "scripting/jass_animation_tags.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "audio/AudioSystem.h"
#include "ecs/player.h"
#include "ecs/item.h"
#include "ecs/abilities/ability_inventory.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/ability.h"
#include "ecs/buff.h"
#include "ecs/buffs.h"
#include "ecs/abilities/ability_sell_items.h"
#include "ecs/ability_factory.h"
#include "ecs/destructable.h"
#include "ecs/orders/order.h"
#include "ecs/pathing/world_collision.h"
#include "data/unitdata.h"
#include "data/abilitydata.h"
#include "data/itemdata.h"
#include "data/combat_types.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <random>
#include <algorithm>
#include <cstring>
#include <cctype>

static std::mt19937 s_rng(std::random_device{}());

static CItemClass ItemClassFromJassOrdinal(int ordinal) {
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

static std::vector<std::string> ResolveSecondaryTagNames(
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

static std::string ResolvePrimaryTagName(const std::shared_ptr<JassNativesData>& data, int primaryTagHandle) {
    if (!data || primaryTagHandle == 0) return "";
    const int ordinal = data->GetSimHandle(primaryTagHandle, JassHandleInfo::PRIMARY_TAG);
    if (ordinal < 0) return "";
    return JassAnimationPrimaryTagName(ordinal);
}

static int ParseOrderString(const std::string& orderStr) {
    if (orderStr == "smart") return OrderIds::SMART;
    if (orderStr == "move") return OrderIds::MOVE;
    if (orderStr == "attack") return OrderIds::ATTACK;
    if (orderStr == "attackground") return OrderIds::ATTACK_GROUND;
    if (orderStr == "stop") return OrderIds::STOP;
    if (orderStr == "holdposition") return OrderIds::HOLD_POSITION;
    if (orderStr == "patrol") return OrderIds::PATROL;
    if (orderStr == "harvest") return OrderIds::HARVEST;
    if (orderStr == "returnresources") return OrderIds::RETURN_RESOURCES;
    if (orderStr == "repair") return War3ID("Arep").value;
    if (orderStr == "setrally") return OrderIds::SET_RALLY;
    if (orderStr == "rally") return OrderIds::RALLY;

    if (orderStr.size() == 4) {
        return ((unsigned char)orderStr[0] << 24) | ((unsigned char)orderStr[1] << 16) |
               ((unsigned char)orderStr[2] << 8) | (unsigned char)orderStr[3];
    }

    return 0;
}

static int ResolvePlayerId(const std::shared_ptr<JassNativesData>& data, int value) {
    int playerId = data ? data->GetSimHandle(value, JassHandleInfo::PLAYER) : -1;
    if (playerId >= 0) return playerId;
    return (value >= 0 && value < 24) ? value : -1;
}

static int RawcodeFromString(const std::string& value) {
    if (value.size() < 4) return 0;
    return ((unsigned char)value[0] << 24) | ((unsigned char)value[1] << 16) |
           ((unsigned char)value[2] << 8) | (unsigned char)value[3];
}

static std::string RawcodeToString(int value) {
    char buf[5] = {
        (char)((value >> 24) & 0xFF), (char)((value >> 16) & 0xFF),
        (char)((value >> 8) & 0xFF), (char)(value & 0xFF), 0
    };
    return std::string(buf);
}

static int UnitIdFromLegacyName(CSimulation* sim, const std::string& legacyName) {
    if (!sim || !sim->GetUnitData()) return 0;
    for (const auto& [typeId, unitType] : sim->GetUnitData()->GetAllTypes()) {
        if (unitType && unitType->GetLegacyName() == legacyName) {
            return (int)typeId.value;
        }
    }
    return 0;
}

static std::string UnitLegacyNameFromId(CSimulation* sim, int typeId) {
    if (!sim || !sim->GetUnitData()) return "";
    CUnitType* unitType = sim->GetUnitData()->GetUnitType(War3ID((uint32_t)typeId));
    return unitType ? unitType->GetLegacyName() : "";
}

static CAbilityHero* FindHeroAbility(CSimulation* sim, int unitSimId) {
    if (!sim) return nullptr;
    CUnit* unit = sim->GetUnit(unitSimId);
    if (!unit || !unit->IsHero()) return nullptr;
    CAbility* ability = unit->GetAbility(War3ID("AHer"));
    return dynamic_cast<CAbilityHero*>(ability);
}

// JASS race ordinals (common.j ConvertRace): 1=Human, 2=Orc, 3=Undead,
// 4=NightElf, 5=Naga, 6=Creep, 7=Demon, 8=Critter, 9=Other.
static int UnitRaceToJassOrdinal(const std::string& raceName) {
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

int JassNativesData::CreateItem(CSimulation* sim, War3ID typeId, float x, float y) {
    if (!sim) {
        return -1;
    }
    CItem* item = sim->CreateItem(typeId, x, y);
    if (!item) {
        return -1;
    }
    const int itemId = item->GetHandleId();
    items[itemId] = item;
    return itemId;
}

int JassNativesData::CreateItemHandle(CSimulation* sim, War3ID typeId, float x, float y) {
    const int itemId = CreateItem(sim, typeId, x, y);
    if (itemId < 0) {
        return 0;
    }
    return AllocHandle(JassHandleInfo::ITEM, itemId);
}

static CItem* GetJassItem(const std::shared_ptr<JassNativesData>& data, int jassHandle) {
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

static CWidget* GetJassWidget(const std::shared_ptr<JassNativesData>& data,
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

static bool JassValueToBool(const JassValue& value) {
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

// Synchronous function invocation for enumeration/filter callbacks
// (ForGroup/ForForce/Enum*). A TriggerSleepAction inside such a callback
// cannot suspend the queue model, so the request is drained instead of
// leaking into sleepRequested_ and truncating the outer thread (the
// queued-thread equivalent lives in RunThread/PumpQueuedThreads).
static JassValue CallSyncFunction(JassVM* vm, const std::string& funcName) {
    if (!vm || funcName.empty() || !vm->HasFunction(funcName)) return JassValue::Null();
    JassValue result = vm->CallFunction(funcName);
    if (vm->HasPendingSleep()) vm->TakePendingSleepSeconds();
    return result;
}

static void ExecuteSyncFunction(JassVM* vm, const std::string& funcName) {
    (void)CallSyncFunction(vm, funcName);
}

// ============================================================
// JassNativesData
// ============================================================
int JassNativesData::AllocHandle(JassHandleInfo::Type type, int id) {
    int h = nextHandleId++;
    handles[h] = {type, id};
    return h;
}

void JassNativesData::FreeHandle(int handle) {
    handles.erase(handle);
}

int JassNativesData::GetSimHandle(int jassHandle, JassHandleInfo::Type type) const {
    auto it = handles.find(jassHandle);
    if (it != handles.end() && it->second.type == type)
        return it->second.id;
    return -1;
}

JassHandleInfo* JassNativesData::GetHandleInfo(int jassHandle) {
    auto it = handles.find(jassHandle);
    if (it != handles.end()) return &it->second;
    return nullptr;
}

// ============================================================
// JassNatives
// ============================================================
JassNatives::JassNatives(JassVM& vm, CSimulation* sim)
    : data_(std::make_shared<JassNativesData>())
    , vm_(vm)
    , vmPtr_(&vm)
    , sim_(sim)
{
}

JassQueuedContext JassNatives::CaptureContext() const {
    JassQueuedContext context;
    if (!data_) return context;
    context.currentTriggerId = data_->currentTriggerId;
    context.currentTriggerUnitHandle = data_->currentTriggerUnitHandle;
    context.currentTriggerEventId = data_->currentTriggerEventId;
    context.filterUnitHandle = data_->filterUnitHandle;
    context.enumUnitHandle = data_->enumUnitHandle;
    context.filterItemHandle = data_->filterItemHandle;
    context.enumItemHandle = data_->enumItemHandle;
    context.filterDestructableHandle = data_->filterDestructableHandle;
    context.enumDestructableHandle = data_->enumDestructableHandle;
    context.filterPlayerHandle = data_->filterPlayerHandle;
    context.enumPlayerHandle = data_->enumPlayerHandle;
    context.eventDamage = data_->eventDamage;
    context.eventDamageSourceHandle = data_->eventDamageSourceHandle;
    context.eventAttackingUnitHandle = data_->eventAttackingUnitHandle;
    context.eventSpellAbilityId = data_->eventSpellAbilityId;
    context.eventSpellAbilityUnitHandle = data_->eventSpellAbilityUnitHandle;
    context.eventSpellTargetX = data_->eventSpellTargetX;
    context.eventSpellTargetY = data_->eventSpellTargetY;
    context.eventSpellTargetUnitHandle = data_->eventSpellTargetUnitHandle;
    context.eventTriggerPlayerHandle = data_->eventTriggerPlayerHandle;
    context.eventTriggerWidgetHandle = data_->eventTriggerWidgetHandle;
    context.eventEnteringUnitHandle = data_->eventEnteringUnitHandle;
    context.eventLeavingUnitHandle = data_->eventLeavingUnitHandle;
    context.eventManipulatingUnitHandle = data_->eventManipulatingUnitHandle;
    context.eventManipulatedItemHandle = data_->eventManipulatedItemHandle;
    context.eventOrderedUnitHandle = data_->eventOrderedUnitHandle;
    context.eventIssuedOrderId = data_->eventIssuedOrderId;
    context.eventOrderPointX = data_->eventOrderPointX;
    context.eventOrderPointY = data_->eventOrderPointY;
    context.eventOrderTargetHandle = data_->eventOrderTargetHandle;
    context.eventKillingUnitHandle = data_->eventKillingUnitHandle;
    context.eventDyingUnitHandle = data_->eventDyingUnitHandle;
    context.eventRevivingUnitHandle = data_->eventRevivingUnitHandle;
    context.eventTransportUnitHandle = data_->eventTransportUnitHandle;
    context.eventLoadedUnitHandle = data_->eventLoadedUnitHandle;
    context.eventTrainedUnitHandle = data_->eventTrainedUnitHandle;
    context.eventSummonedUnitHandle = data_->eventSummonedUnitHandle;
    context.eventSummoningUnitHandle = data_->eventSummoningUnitHandle;
    context.eventResearchingUnitHandle = data_->eventResearchingUnitHandle;
    context.eventResearchedId = data_->eventResearchedId;
    context.eventExpiredTimerHandle = data_->eventExpiredTimerHandle;
    return context;
}

void JassNatives::ApplyContext(const JassQueuedContext& context) {
    if (!data_) return;
    data_->currentTriggerId = context.currentTriggerId;
    data_->currentTriggerUnitHandle = context.currentTriggerUnitHandle;
    data_->currentTriggerEventId = context.currentTriggerEventId;
    data_->filterUnitHandle = context.filterUnitHandle;
    data_->enumUnitHandle = context.enumUnitHandle;
    data_->filterItemHandle = context.filterItemHandle;
    data_->enumItemHandle = context.enumItemHandle;
    data_->filterDestructableHandle = context.filterDestructableHandle;
    data_->enumDestructableHandle = context.enumDestructableHandle;
    data_->filterPlayerHandle = context.filterPlayerHandle;
    data_->enumPlayerHandle = context.enumPlayerHandle;
    data_->eventDamage = context.eventDamage;
    data_->eventDamageSourceHandle = context.eventDamageSourceHandle;
    data_->eventAttackingUnitHandle = context.eventAttackingUnitHandle;
    data_->eventSpellAbilityId = context.eventSpellAbilityId;
    data_->eventSpellAbilityUnitHandle = context.eventSpellAbilityUnitHandle;
    data_->eventSpellTargetX = context.eventSpellTargetX;
    data_->eventSpellTargetY = context.eventSpellTargetY;
    data_->eventSpellTargetUnitHandle = context.eventSpellTargetUnitHandle;
    data_->eventTriggerPlayerHandle = context.eventTriggerPlayerHandle;
    data_->eventTriggerWidgetHandle = context.eventTriggerWidgetHandle;
    data_->eventEnteringUnitHandle = context.eventEnteringUnitHandle;
    data_->eventLeavingUnitHandle = context.eventLeavingUnitHandle;
    data_->eventManipulatingUnitHandle = context.eventManipulatingUnitHandle;
    data_->eventManipulatedItemHandle = context.eventManipulatedItemHandle;
    data_->eventOrderedUnitHandle = context.eventOrderedUnitHandle;
    data_->eventIssuedOrderId = context.eventIssuedOrderId;
    data_->eventOrderPointX = context.eventOrderPointX;
    data_->eventOrderPointY = context.eventOrderPointY;
    data_->eventOrderTargetHandle = context.eventOrderTargetHandle;
    data_->eventKillingUnitHandle = context.eventKillingUnitHandle;
    data_->eventDyingUnitHandle = context.eventDyingUnitHandle;
    data_->eventRevivingUnitHandle = context.eventRevivingUnitHandle;
    data_->eventTransportUnitHandle = context.eventTransportUnitHandle;
    data_->eventLoadedUnitHandle = context.eventLoadedUnitHandle;
    data_->eventTrainedUnitHandle = context.eventTrainedUnitHandle;
    data_->eventSummonedUnitHandle = context.eventSummonedUnitHandle;
    data_->eventSummoningUnitHandle = context.eventSummoningUnitHandle;
    data_->eventResearchingUnitHandle = context.eventResearchingUnitHandle;
    data_->eventResearchedId = context.eventResearchedId;
    data_->eventExpiredTimerHandle = context.eventExpiredTimerHandle;
}

void JassNatives::QueueFunction(const std::string& functionName) {
    QueueFunctionWithContext(functionName, CaptureContext());
}

void JassNatives::QueueFunctionWithContext(const std::string& functionName, const JassQueuedContext& context) {
    if (functionName.empty()) return;
    if (!vm_.HasFunction(functionName)) {
        std::cerr << "JASS queue: missing function " << functionName << std::endl;
        return;
    }
    JassQueuedThread thread;
    thread.functionName = functionName;
    thread.context = context;
    queuedThreads_.push_back(std::move(thread));
}

int JassNatives::PumpQueuedThreads(int maxCount) {
    int pumped = 0;
    while (!queuedThreads_.empty() && pumped < maxCount) {
        auto thread = std::move(queuedThreads_.front());
        queuedThreads_.pop_front();

        JassQueuedContext previous = CaptureContext();
        ApplyContext(thread.context);
        const size_t errorCountBefore = vm_.GetErrors().size();
        JassVM::ThreadRunResult result = vm_.RunThread(thread.vmState, thread.functionName);
        const auto& errors = vm_.GetErrors();
        for (size_t i = errorCountBefore; i < errors.size(); ++i) {
            std::cerr << "JASS runtime error in " << thread.functionName << ": " << errors[i] << std::endl;
        }
        ApplyContext(previous);
        if (result.sleeping) {
            thread.sleepRemaining = result.sleepSeconds;
            if (thread.sleepRemaining <= 0.0f) {
                queuedThreads_.push_back(std::move(thread));
            } else {
                sleepingThreads_.push_back(std::move(thread));
            }
        }
        pumped++;
    }
    if (!queuedThreads_.empty()) {
        std::cout << "JASS queue: pump limit reached, " << queuedThreads_.size()
                  << " threads remain" << std::endl;
    }
    return pumped;
}

bool JassNatives::EvaluateTriggerConditions(JassTrigger& trigger, const JassQueuedContext& context,
                                            bool requireEnabled) {
    if (requireEnabled && !trigger.enabled) {
        return false;
    }
    ++trigger.evalCount;
    for (const auto& conditionName : trigger.conditionFuncNames) {
        if (conditionName.empty()) {
            continue;
        }
        if (!vm_.HasFunction(conditionName)) {
            std::cerr << "JASS trigger: missing condition " << conditionName << std::endl;
            return false;
        }

        JassQueuedContext previous = CaptureContext();
        ApplyContext(context);
        JassValue result = CallSyncFunction(&vm_, conditionName);
        ApplyContext(previous);
        if (!JassValueToBool(result)) {
            return false;
        }
    }
    return true;
}

void JassNatives::QueueTriggerActions(JassTrigger& trigger, const JassQueuedContext& context) {
    if (!EvaluateTriggerConditions(trigger, context)) {
        return;
    }
    ++trigger.execCount;
    for (const auto& funcName : trigger.actionFuncNames) {
        QueueFunctionWithContext(funcName, context);
    }
}

void JassNatives::FireUnitEvent(int unitSimHandle, int eventType, int playerEventType) {
    for (auto& [id, trigger] : data_->triggers) {
        for (size_t i = 0; i < trigger->registeredUnitHandles.size(); i++) {
            int regUnitHandle = trigger->registeredUnitHandles[i];
            int regEventType = trigger->eventTypes[i];
            // Match either the unit-event id (TriggerRegisterUnitEvent) or
            // the player-unit-event counterpart (TriggerRegisterPlayerUnitEvent).
            if (regEventType != eventType && regEventType != playerEventType) continue;

            bool matches = regUnitHandle == 0;
            JassHandleInfo* info = data_->GetHandleInfo(regUnitHandle);
            if (info && info->type == JassHandleInfo::UNIT) {
                matches = info->id == unitSimHandle;
            } else if (info && (info->type == JassHandleInfo::DESTRUCTABLE ||
                                info->type == JassHandleInfo::ITEM)) {
                matches = info->id == unitSimHandle;
            } else if (info && info->type == JassHandleInfo::PLAYER && sim_) {
                if (CUnit* unit = sim_->GetUnit(unitSimHandle)) {
                    matches = unit->GetPlayerIndex() == info->id;
                }
            } else {
                int simHandle = data_->GetSimHandle(regUnitHandle, JassHandleInfo::UNIT);
                matches = simHandle == unitSimHandle;
            }

            if (matches) {
                JassQueuedContext context = CaptureContext();
                context.currentTriggerId = id;
                context.currentTriggerUnitHandle = -1;
                // Report the actually-registered event id (unit vs player
                // variant) so GetTriggerEventId matches ref behavior.
                context.currentTriggerEventId = regEventType;
                if (sim_) {
                    if (CUnit* unit = sim_->GetUnit(unitSimHandle)) {
                        context.currentTriggerUnitHandle =
                            data_->AllocHandle(JassHandleInfo::UNIT, unitSimHandle);
                        context.eventTriggerPlayerHandle =
                            data_->AllocHandle(JassHandleInfo::PLAYER, unit->GetPlayerIndex());
                    }
                }
                QueueTriggerActions(*trigger, context);
                break;
            }
        }
    }
}

void JassNatives::FireDamageEvent(int unitSimHandle, int sourceSimHandle, float damage, int eventType) {
    int sourceHandle = -1;
    if (sim_) {
        if (CUnit* sourceUnit = sim_->GetUnit(sourceSimHandle)) {
            sourceHandle = data_->AllocHandle(JassHandleInfo::UNIT, sourceUnit->GetHandleId());
        }
    }
    data_->eventDamage = damage;
    data_->eventDamageSourceHandle = sourceHandle;
    data_->eventAttackingUnitHandle = sourceHandle;
    FireUnitEvent(unitSimHandle, eventType);
    data_->eventDamage = 0;
    data_->eventDamageSourceHandle = -1;
    data_->eventAttackingUnitHandle = -1;
}

void JassNatives::FireDeathEvent(int unitSimHandle, int killerSimHandle, int eventType) {
    data_->eventDyingUnitHandle = -1;
    data_->eventTriggerWidgetHandle = -1;
    if (sim_) {
        if (CUnit* unit = sim_->GetUnit(unitSimHandle)) {
            data_->eventDyingUnitHandle =
                data_->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
            data_->eventTriggerWidgetHandle = data_->eventDyingUnitHandle;
        } else if (CDestructable* destructable = sim_->GetDestructable(unitSimHandle)) {
            data_->eventTriggerWidgetHandle =
                data_->AllocHandle(JassHandleInfo::DESTRUCTABLE, destructable->GetHandleId());
        } else if (CItem* item = sim_->GetItem(unitSimHandle)) {
            data_->items[item->GetHandleId()] = item;
            data_->eventTriggerWidgetHandle =
                data_->AllocHandle(JassHandleInfo::ITEM, item->GetHandleId());
        }
    } else {
        data_->eventDyingUnitHandle = data_->AllocHandle(JassHandleInfo::UNIT, unitSimHandle);
        data_->eventTriggerWidgetHandle = data_->eventDyingUnitHandle;
    }
    data_->eventKillingUnitHandle =
        (killerSimHandle >= 0 && (!sim_ || sim_->GetUnit(killerSimHandle)))
            ? data_->AllocHandle(JassHandleInfo::UNIT, killerSimHandle)
            : -1;

    const int playerEventType = (eventType == JASS_EVENT_UNIT_DEATH)
                                    ? JASS_EVENT_PLAYER_UNIT_DEATH
                                    : -1;
    FireUnitEvent(unitSimHandle, eventType, playerEventType);

    data_->eventDyingUnitHandle = -1;
    data_->eventKillingUnitHandle = -1;
    data_->eventTriggerWidgetHandle = -1;
}

void JassNatives::FireOrderEvent(int unitSimHandle, int orderId, int targetSimHandle, float x, float y) {
    data_->eventOrderedUnitHandle = data_->AllocHandle(JassHandleInfo::UNIT, unitSimHandle);
    data_->eventIssuedOrderId = orderId;
    data_->eventOrderPointX = x;
    data_->eventOrderPointY = y;
    data_->eventOrderTargetHandle = -1;
    if (targetSimHandle >= 0 && sim_) {
        if (CUnit* targetUnit = sim_->GetUnit(targetSimHandle)) {
            data_->eventOrderTargetHandle =
                data_->AllocHandle(JassHandleInfo::UNIT, targetUnit->GetHandleId());
        } else if (CDestructable* targetDest = sim_->GetDestructable(targetSimHandle)) {
            data_->eventOrderTargetHandle =
                data_->AllocHandle(JassHandleInfo::DESTRUCTABLE, targetDest->GetHandleId());
        } else if (CItem* targetItem = sim_->GetItem(targetSimHandle)) {
            data_->items[targetItem->GetHandleId()] = targetItem;
            data_->eventOrderTargetHandle =
                data_->AllocHandle(JassHandleInfo::ITEM, targetItem->GetHandleId());
        }
    }

    // Determine which event type to fire based on whether there's a target or point
    int eventType = JASS_EVENT_UNIT_ISSUED_ORDER;
    int playerEventType = JASS_EVENT_PLAYER_UNIT_ISSUED_ORDER;
    if (targetSimHandle >= 0) {
        eventType = JASS_EVENT_UNIT_ISSUED_TARGET_ORDER;
        playerEventType = JASS_EVENT_PLAYER_UNIT_ISSUED_TARGET_ORDER;
    } else if (x != 0.0f || y != 0.0f) {
        eventType = JASS_EVENT_UNIT_ISSUED_POINT_ORDER;
        playerEventType = JASS_EVENT_PLAYER_UNIT_ISSUED_POINT_ORDER;
    }

    FireUnitEvent(unitSimHandle, eventType, playerEventType);

    data_->eventOrderedUnitHandle = -1;
    data_->eventIssuedOrderId = 0;
    data_->eventOrderPointX = 0.0f;
    data_->eventOrderPointY = 0.0f;
    data_->eventOrderTargetHandle = -1;
}

void JassNatives::FireItemEvent(int unitSimHandle, int itemSimHandle, int eventType) {
    if (unitSimHandle < 0 || itemSimHandle < 0) {
        return;
    }

    data_->eventManipulatingUnitHandle = data_->AllocHandle(JassHandleInfo::UNIT, unitSimHandle);
    data_->eventManipulatedItemHandle = data_->AllocHandle(JassHandleInfo::ITEM, itemSimHandle);
    if (sim_) {
        if (CItem* item = sim_->GetItem(itemSimHandle)) {
            data_->items[itemSimHandle] = item;
        }
    }

    int playerEventType = -1;
    if (eventType == JASS_EVENT_UNIT_DROP_ITEM) {
        playerEventType = JASS_EVENT_PLAYER_UNIT_DROP_ITEM;
    } else if (eventType == JASS_EVENT_UNIT_PICKUP_ITEM) {
        playerEventType = JASS_EVENT_PLAYER_UNIT_PICKUP_ITEM;
    } else if (eventType == JASS_EVENT_UNIT_USE_ITEM) {
        playerEventType = JASS_EVENT_PLAYER_UNIT_USE_ITEM;
    }
    FireUnitEvent(unitSimHandle, eventType, playerEventType);

    data_->eventManipulatingUnitHandle = -1;
    data_->eventManipulatedItemHandle = -1;
}

void JassNatives::ForgetItem(int itemSimHandle) {
    if (itemSimHandle < 0) {
        return;
    }
    data_->items.erase(itemSimHandle);
}

static bool IsPointInJassRegion(const JassRegion& region, float x, float y) {
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

void JassNatives::FireRegionEvent(int regionSimId, int unitSimHandle, bool isEnter) {
    if (regionSimId < 0 || unitSimHandle < 0) {
        return;
    }
    const int eventCode = isEnter ? 4001 : 4002;
    CUnit* unit = sim_ ? sim_->GetUnit(unitSimHandle) : nullptr;
    for (auto& [triggerId, trigger] : data_->triggers) {
        bool registered = false;
        for (const auto& [regionHandle, code] : trigger->registeredEvents) {
            if (code != eventCode) continue;
            if (data_->GetSimHandle(regionHandle, JassHandleInfo::REGION) == regionSimId) {
                registered = true;
                break;
            }
        }
        if (!registered) continue;
        JassQueuedContext context = CaptureContext();
        context.currentTriggerId = triggerId;
        context.currentTriggerEventId = eventCode;
        if (unit) {
            context.currentTriggerUnitHandle =
                data_->AllocHandle(JassHandleInfo::UNIT, unitSimHandle);
            context.eventTriggerPlayerHandle =
                data_->AllocHandle(JassHandleInfo::PLAYER, unit->GetPlayerIndex());
            if (isEnter) {
                context.eventEnteringUnitHandle = context.currentTriggerUnitHandle;
            } else {
                context.eventLeavingUnitHandle = context.currentTriggerUnitHandle;
            }
        }
        QueueTriggerActions(*trigger, context);
    }
}

void JassNatives::UpdateRegions() {
    if (!sim_ || !data_) {
        return;
    }
    // Fast path: nothing to track.
    if (data_->regions.empty()) {
        if (!regionMembership_.empty()) regionMembership_.clear();
        return;
    }
    bool anyRegionTrigger = false;
    for (const auto& [triggerId, trigger] : data_->triggers) {
        (void)triggerId;
        for (const auto& [handle, code] : trigger->registeredEvents) {
            (void)handle;
            if (code == 4001 || code == 4002) {
                anyRegionTrigger = true;
                break;
            }
        }
        if (anyRegionTrigger) break;
    }
    if (!anyRegionTrigger) {
        if (!regionMembership_.empty()) regionMembership_.clear();
        return;
    }
    // Drop membership for deleted regions.
    for (auto it = regionMembership_.begin(); it != regionMembership_.end();) {
        if (data_->regions.find(it->first) == data_->regions.end()) {
            it = regionMembership_.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto& [regionSimId, region] : data_->regions) {
        if (!region) continue;
        std::unordered_set<int> inside;
        for (CUnit* unit : sim_->GetUnits()) {
            if (!unit || unit->IsDead()) continue;
            if (IsPointInJassRegion(*region, unit->GetX(), unit->GetY())) {
                inside.insert(unit->GetHandleId());
            }
        }
        auto memIt = regionMembership_.find(regionSimId);
        if (memIt == regionMembership_.end()) {
            // Baseline: record without firing (avoids a startup burst for
            // preplaced units; ref parity for steady-state migration).
            regionMembership_[regionSimId] = std::move(inside);
            continue;
        }
        std::unordered_set<int>& previous = memIt->second;
        for (int handle : inside) {
            if (previous.find(handle) == previous.end()) {
                FireRegionEvent(regionSimId, handle, true);
            }
        }
        for (int handle : previous) {
            if (inside.find(handle) == inside.end()) {
                FireRegionEvent(regionSimId, handle, false);
            }
        }
        previous = std::move(inside);
    }
}

void JassNatives::FireTrainedEvent(int buildingSimHandle, int trainedSimHandle) {
    if (buildingSimHandle < 0 || trainedSimHandle < 0) {
        return;
    }
    data_->eventTrainedUnitHandle =
        data_->AllocHandle(JassHandleInfo::UNIT, trainedSimHandle);
    FireUnitEvent(buildingSimHandle, JASS_EVENT_UNIT_TRAIN_FINISH,
                  JASS_EVENT_PLAYER_UNIT_TRAIN_FINISH);
    data_->eventTrainedUnitHandle = -1;
}

void JassNatives::FireSummonEvent(int summonerSimHandle, int summonedSimHandle) {
    if (summonerSimHandle < 0 || summonedSimHandle < 0) {
        return;
    }
    data_->eventSummoningUnitHandle =
        data_->AllocHandle(JassHandleInfo::UNIT, summonerSimHandle);
    data_->eventSummonedUnitHandle =
        data_->AllocHandle(JassHandleInfo::UNIT, summonedSimHandle);
    FireUnitEvent(summonerSimHandle, JASS_EVENT_UNIT_SUMMON,
                  JASS_EVENT_PLAYER_UNIT_SUMMON);
    data_->eventSummoningUnitHandle = -1;
    data_->eventSummonedUnitHandle = -1;
}

void JassNatives::FireResearchEvent(int unitSimHandle, int upgradeId) {
    if (unitSimHandle < 0) {
        return;
    }
    data_->eventResearchingUnitHandle =
        data_->AllocHandle(JassHandleInfo::UNIT, unitSimHandle);
    data_->eventResearchedId = upgradeId;
    FireUnitEvent(unitSimHandle, JASS_EVENT_UNIT_RESEARCH_FINISH,
                  JASS_EVENT_PLAYER_UNIT_RESEARCH_FINISH);
    data_->eventResearchingUnitHandle = -1;
    data_->eventResearchedId = 0;
}

void JassNatives::FireReviveEvent(int reviverSimHandle, int revivedSimHandle) {
    if (reviverSimHandle < 0 || revivedSimHandle < 0) {
        return;
    }
    data_->eventRevivingUnitHandle =
        data_->AllocHandle(JassHandleInfo::UNIT, revivedSimHandle);
    FireUnitEvent(reviverSimHandle, JASS_EVENT_UNIT_HERO_REVIVE_FINISH,
                  JASS_EVENT_PLAYER_HERO_REVIVE_FINISH);
    data_->eventRevivingUnitHandle = -1;
}

int JassNatives::UpdateTimers(float dt) {
    int firedCount = 0;
    const size_t sleepingCount = sleepingThreads_.size();
    for (size_t i = 0; i < sleepingCount; ++i) {
        JassQueuedThread thread = std::move(sleepingThreads_.front());
        sleepingThreads_.pop_front();
        thread.sleepRemaining -= std::max(0.0f, dt);
        if (thread.sleepRemaining <= 0.0f) {
            thread.sleepRemaining = 0.0f;
            queuedThreads_.push_back(std::move(thread));
            firedCount++;
        } else {
            sleepingThreads_.push_back(std::move(thread));
        }
    }

    for (auto& [id, timer] : data_->timers) {
        (void)id;
        if (timer->paused || timer->remaining <= 0) continue;
        timer->remaining -= dt;
        if (timer->remaining <= 0) {
            timer->remaining = 0;
            JassQueuedContext context = CaptureContext();
            context.eventExpiredTimerHandle = data_->AllocHandle(JassHandleInfo::TIMER, id);
            if (!timer->callbackFunc.empty()) {
                QueueFunctionWithContext(timer->callbackFunc, context);
                firedCount++;
            }
            for (auto& [triggerId, trigger] : data_->triggers) {
                bool matches = false;
                for (const auto& event : trigger->registeredEvents) {
                    if ((event.first == id && event.second == 1001) ||
                        (event.first == -1 && event.second == 1000 + id)) {
                        matches = true;
                        break;
                    }
                }
                if (!matches) continue;

                JassQueuedContext triggerContext = context;
                triggerContext.currentTriggerId = triggerId;
                triggerContext.currentTriggerEventId = 0;
                for (const auto& event : trigger->registeredEvents) {
                    if ((event.first == id && event.second == 1001) ||
                        (event.first == -1 && event.second == 1000 + id)) {
                        triggerContext.currentTriggerEventId = event.second;
                        break;
                    }
                }
                const size_t before = queuedThreads_.size();
                QueueTriggerActions(*trigger, triggerContext);
                firedCount += static_cast<int>(queuedThreads_.size() - before);
            }
            if (timer->periodic) {
                timer->remaining = timer->timeout;
            }
        }
    }
    return firedCount;
}

static int UnitToHandle(JassNativesData& data, CUnit* unit) {
    if (!unit) return -1;
    return data.AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
}

static CUnit* HandleToUnit(JassNativesData& data, CSimulation* sim, int handle) {
    if (!sim) return nullptr;
    int simId = data.GetSimHandle(handle, JassHandleInfo::UNIT);
    if (simId < 0) return nullptr;
    return sim->GetUnit(simId);
}

static int AbilityToHandle(JassNativesData& data, War3ID code, int level = 1, int unitSimId = -1) {
    const int abilityId = data.nextAbilityId++;
    auto ability = std::make_shared<JassAbilityHandle>();
    ability->code = code;
    ability->level = std::max(1, level);
    ability->unitSimId = unitSimId;
    data.abilities[abilityId] = ability;
    return data.AllocHandle(JassHandleInfo::ABILITY, abilityId);
}

static std::shared_ptr<JassAbilityHandle> HandleToAbility(const std::shared_ptr<JassNativesData>& data,
                                                          int handle) {
    if (!data) return nullptr;
    auto* info = data->GetHandleInfo(handle);
    if (!info || info->type != JassHandleInfo::ABILITY) return nullptr;
    auto it = data->abilities.find(info->id);
    return it != data->abilities.end() ? it->second : nullptr;
}

static War3ID AbilityCodeFromJassArg(const std::shared_ptr<JassNativesData>& data, int rawOrHandle) {
    auto ability = HandleToAbility(data, rawOrHandle);
    return ability ? ability->code : War3ID(static_cast<uint32_t>(rawOrHandle));
}

void JassNatives::RegisterAll() {
    RegisterConfigNatives();
    RegisterCommonNatives();
}

void JassNatives::RegisterConfigNatives() {
    RegisterHandleNatives();
    RegisterMathNatives();
    RegisterConversionNatives();
    RegisterRandomNatives();
    RegisterGroupNatives();
    RegisterConfigPlayerNatives();
    std::cout << "JassNatives: registered config natives" << std::endl;
}

void JassNatives::RegisterCommonNatives() {
    RegisterHandleNatives();
    RegisterMathNatives();
    RegisterConversionNatives();
    RegisterRandomNatives();
    RegisterTimerNatives();
    RegisterGroupNatives();
    RegisterForceNatives();
    RegisterLocationNatives();
    RegisterRectNatives();
    RegisterRegionNatives();
    RegisterUnitNatives();
    RegisterWidgetNatives();
    RegisterPlayerNatives();
    RegisterHeroNatives();
    RegisterItemNatives();
    RegisterDestructableNatives();
    RegisterTriggerNatives();
    RegisterEventNatives();
    RegisterSoundNatives();
    RegisterTextTagNatives();
    RegisterEffectNatives();
    RegisterFogNatives();
    RegisterWeatherNatives();
    RegisterLightningNatives();
    RegisterCameraNatives();
    RegisterGameNatives();
    RegisterOrderIdNatives();
    RegisterTerrainQueryNatives();
    RegisterPlayerTeamNatives();
    RegisterVisualNatives();
    RegisterPoolStockNatives();
    RegisterUnitStateNatives();
    RegisterCameraFlowNatives();
    RegisterTriggerSoundNatives();
    RegisterUIStateNatives();
    RegisterBoardNatives();
    std::cout << "JassNatives: registered common natives" << std::endl;
}

// ============================================================
// 1. Handle utilities
// ============================================================
void JassNatives::RegisterHandleNatives() {
    vm_.RegisterNative("GetHandleId", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int(args[0].intVal);
    });
    vm_.RegisterNative("OrderId", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int(args[0].intVal);
    });
    vm_.RegisterNative("OrderId2String", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("");
        return JassValue::Str(RawcodeToString(args[0].intVal));
    });
    vm_.RegisterNative("UnitId", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        const std::string& name = args[0].strVal;
        int id = UnitIdFromLegacyName(sim, name);
        return JassValue::Int(id != 0 ? id : RawcodeFromString(name));
    });
    vm_.RegisterNative("UnitId2String", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("");
        std::string legacyName = UnitLegacyNameFromId(sim, args[0].intVal);
        return JassValue::Str(!legacyName.empty() ? legacyName : RawcodeToString(args[0].intVal));
    });
    vm_.RegisterNative("String2UnitId", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int(RawcodeFromString(args[0].strVal));
    });
    vm_.RegisterNative("String2OrderId", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int(RawcodeFromString(args[0].strVal));
    });
}

// ============================================================
// 2. Math
// ============================================================
void JassNatives::RegisterMathNatives() {
    vm_.RegisterNative("Sin", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real(std::sin(args[0].realVal));
    });
    vm_.RegisterNative("Cos", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real(std::cos(args[0].realVal));
    });
    vm_.RegisterNative("Tan", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real(std::tan(args[0].realVal));
    });
    vm_.RegisterNative("Asin", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real(std::asin(args[0].realVal));
    });
    vm_.RegisterNative("Acos", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real(std::acos(args[0].realVal));
    });
    vm_.RegisterNative("Atan", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real(std::atan(args[0].realVal));
    });
    vm_.RegisterNative("Atan2", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Real(0);
        return JassValue::Real(std::atan2(args[0].realVal, args[1].realVal));
    });
    vm_.RegisterNative("SquareRoot", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real(std::sqrt(args[0].realVal));
    });
    vm_.RegisterNative("Pow", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Real(0);
        return JassValue::Real(std::pow(args[0].realVal, args[1].realVal));
    });
    vm_.RegisterNative("ModuloInteger", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || args[1].intVal == 0) return JassValue::Int(0);
        return JassValue::Int(args[0].intVal % args[1].intVal);
    });
    vm_.RegisterNative("ModuloReal", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || args[1].realVal == 0) return JassValue::Real(0);
        return JassValue::Real(std::fmod(args[0].realVal, args[1].realVal));
    });
    vm_.RegisterNative("Log", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real(std::log(args[0].realVal));
    });
    vm_.RegisterNative("Exp", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real(std::exp(args[0].realVal));
    });
    vm_.RegisterNative("Max", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Int(0);
        return JassValue::Int(std::max(args[0].intVal, args[1].intVal));
    });
    vm_.RegisterNative("Min", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Int(0);
        return JassValue::Int(std::min(args[0].intVal, args[1].intVal));
    });
    vm_.RegisterNative("Deg2Rad", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real(args[0].realVal * 3.14159265359f / 180.0f);
    });
    vm_.RegisterNative("Rad2Deg", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real(args[0].realVal * 180.0f / 3.14159265359f);
    });
}

// ============================================================
// 3. Type Conversion & String
// ============================================================
void JassNatives::RegisterConversionNatives() {
    vm_.RegisterNative("I2S", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("0");
        return JassValue::Str(std::to_string(args[0].intVal));
    });
    vm_.RegisterNative("R2S", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("0");
        std::ostringstream ss; ss << args[0].realVal;
        return JassValue::Str(ss.str());
    });
    vm_.RegisterNative("R2SW", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Str("0");
        std::ostringstream ss; ss.precision(args[2].intVal);
        ss << std::fixed << args[0].realVal;
        return JassValue::Str(ss.str());
    });
    vm_.RegisterNative("S2I", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        try { return JassValue::Int(std::stoi(args[0].strVal)); }
        catch (...) { return JassValue::Int(0); }
    });
    vm_.RegisterNative("R2I", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int((int)args[0].realVal);
    });
    vm_.RegisterNative("I2R", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real((float)args[0].intVal);
    });
    vm_.RegisterNative("StringLength", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int((int)args[0].strVal.size());
    });
    vm_.RegisterNative("SubString", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Str("");
        const std::string& s = args[0].strVal;
        int start = args[1].intVal, end = args[2].intVal;
        if (start < 0) start = 0;
        if (end > (int)s.size()) end = (int)s.size();
        if (start >= end) return JassValue::Str("");
        return JassValue::Str(s.substr(start, end - start));
    });
    vm_.RegisterNative("StringCase", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Str("");
        std::string s = args[0].strVal;
        bool upper = args[1].boolVal;
        for (auto& c : s) c = (char)(upper ? std::toupper(c) : std::tolower(c));
        return JassValue::Str(s);
    });
    vm_.RegisterNative("StringHash", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int((int)std::hash<std::string>{}(args[0].strVal));
    });

    auto convertPassthrough = [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int(args[0].intVal);
    };

    const char* convertNames[] = {
        "ConvertRace", "ConvertAllianceType", "ConvertRacePref", "ConvertIGameState",
        "ConvertFGameState", "ConvertPlayerState", "ConvertPlayerScore",
        "ConvertPlayerGameResult", "ConvertUnitState", "ConvertUnitEvent",
        "ConvertWidgetEvent", "ConvertGameEvent", "ConvertPlayerEvent",
        "ConvertPlayerUnitEvent", "ConvertLimitOp", "ConvertMapDensity",
        "ConvertMapControl", "ConvertPlacement", "ConvertMapVisibility",
        "ConvertMapSetting", "ConvertSoundType", "ConvertAttackType",
        "ConvertDamageType", "ConvertWeaponType", "ConvertDefenseType",
        "ConvertArmorType", "ConvertOriginFrameType", "ConvertFramePointType",
        "ConvertTextureTileFlag", "ConvertCamDistanceSetting", "ConvertItemType",
        "ConvertBuff", "ConvertEffectType", "ConvertUnitType", "ConvertHeroStat",
        "ConvertPlayerColor", "ConvertPlayerSlotState", "ConvertMapFlag",
        "ConvertGameType", "ConvertPathingType", "ConvertVolumeGroup",
        "ConvertBlendMode", "ConvertCameraField", "ConvertGameSpeed",
        "ConvertGameDifficulty", "ConvertTexMapFlags", "ConvertAIDifficulty",
        "ConvertFogState", "ConvertStartLocPrio", "ConvertDialogEvent",
        "ConvertRarityControl", "ConvertVersion"
    };
    for (auto name : convertNames) vm_.RegisterNative(name, convertPassthrough);
    vm_.RegisterNative("ConvertPlayerColor", convertPassthrough);

    vm_.RegisterNative("GetLocalizedString", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("");
        return JassValue::Str(args[0].strVal);
    });
}

// ============================================================
// 4. Random
// ============================================================
void JassNatives::RegisterRandomNatives() {
    vm_.RegisterNative("GetRandomInt", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Int(0);
        int low = args[0].intVal, high = args[1].intVal;
        if (low > high) std::swap(low, high);
        return JassValue::Int(std::uniform_int_distribution<int>(low, high)(s_rng));
    });
    vm_.RegisterNative("GetRandomReal", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Real(0);
        float low = args[0].realVal, high = args[1].realVal;
        if (low > high) std::swap(low, high);
        return JassValue::Real(std::uniform_real_distribution<float>(low, high)(s_rng));
    });
    vm_.RegisterNative("SetRandomSeed", [](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty()) s_rng.seed(args[0].intVal);
        return JassValue::Int(0);
    });
}

// ============================================================
// 5. Timer natives
// ============================================================
void JassNatives::RegisterTimerNatives() {
    vm_.RegisterNative("CreateTimer", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto timer = std::make_shared<JassTimer>();
        int id = data->nextTimerId++;
        data->timers[id] = timer;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::TIMER, id));
    });
    vm_.RegisterNative("DestroyTimer", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::TIMER);
        if (simId >= 0) { data->timers.erase(simId); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("TimerStart", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::TIMER);
        if (simId >= 0) {
            auto it = data->timers.find(simId);
            if (it != data->timers.end()) {
                it->second->timeout = args[1].realVal;
                it->second->remaining = args[1].realVal;
                it->second->periodic = args[2].boolVal;
                it->second->callbackFunc = args[3].strVal;
                it->second->paused = false;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerSleepAction", [vm = &vm_](const std::vector<JassValue>& args) -> JassValue {
        float seconds = 0.0f;
        if (!args.empty()) {
            seconds = args[0].type == JassValue::REAL ? args[0].realVal : static_cast<float>(args[0].intVal);
        }
        vm->RequestSleep(seconds);
        return JassValue::Null();
    });
    vm_.RegisterNative("TimerGetElapsed", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::TIMER);
        if (simId >= 0) {
            auto it = data->timers.find(simId);
            if (it != data->timers.end())
                return JassValue::Real(it->second->timeout - it->second->remaining);
        }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("TimerGetRemaining", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::TIMER);
        if (simId >= 0) {
            auto it = data->timers.find(simId);
            if (it != data->timers.end()) return JassValue::Real(it->second->remaining);
        }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("TimerGetTimeout", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::TIMER);
        if (simId >= 0) {
            auto it = data->timers.find(simId);
            if (it != data->timers.end()) return JassValue::Real(it->second->timeout);
        }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("PauseTimer", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::TIMER);
        if (simId >= 0) { auto it = data->timers.find(simId); if (it != data->timers.end()) it->second->paused = true; }
        return JassValue::Null();
    });
    vm_.RegisterNative("ResumeTimer", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::TIMER);
        if (simId >= 0) { auto it = data->timers.find(simId); if (it != data->timers.end()) it->second->paused = false; }
        return JassValue::Null();
    });
    vm_.RegisterNative("CreateTimerDialog", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        auto dialog = std::make_shared<JassTimerDialog>();
        if (!args.empty() && data) {
            dialog->timerSimId = data->GetSimHandle(args[0].intVal, JassHandleInfo::TIMER);
        }
        int id = data->nextTimerDialogId++;
        data->timerDialogs[id] = dialog;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::TIMER_DIALOG, id));
    });
    vm_.RegisterNative("DestroyTimerDialog", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::TIMER_DIALOG);
        if (simId >= 0) {
            data->timerDialogs.erase(simId);
            data->FreeHandle(args[0].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TimerDialogDisplay", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::TIMER_DIALOG);
        if (simId >= 0) {
            auto it = data->timerDialogs.find(simId);
            if (it != data->timerDialogs.end()) it->second->displayed = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TimerDialogSetTitle", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::TIMER_DIALOG);
        if (simId >= 0) {
            auto it = data->timerDialogs.find(simId);
            if (it != data->timerDialogs.end()) it->second->title = args[1].strVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TimerDialogSetRealTimeRemaining", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::TIMER_DIALOG);
        if (simId < 0) return JassValue::Null();
        auto dit = data->timerDialogs.find(simId);
        if (dit == data->timerDialogs.end()) return JassValue::Null();
        auto tit = data->timers.find(dit->second->timerSimId);
        if (tit != data->timers.end()) {
            tit->second->remaining = std::max(0.0f, args[1].realVal);
        }
        return JassValue::Null();
    });
}

// ============================================================
// 6. Group natives
// ============================================================
void JassNatives::RegisterGroupNatives() {
    vm_.RegisterNative("CreateGroup", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto group = std::make_shared<JassGroup>();
        int id = data->nextGroupId++;
        data->groups[id] = group;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::GROUP, id));
    });
    vm_.RegisterNative("DestroyGroup", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId >= 0) { data->groups.erase(simId); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupAddUnit", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId >= 0) {
            auto it = data->groups.find(simId);
            if (it != data->groups.end()) {
                auto& vec = it->second->unitHandles;
                if (std::find(vec.begin(), vec.end(), args[1].intVal) == vec.end())
                    vec.push_back(args[1].intVal);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupRemoveUnit", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId >= 0) {
            auto it = data->groups.find(simId);
            if (it != data->groups.end()) {
                auto& vec = it->second->unitHandles;
                vec.erase(std::remove(vec.begin(), vec.end(), args[1].intVal), vec.end());
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupClear", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId >= 0) { auto it = data->groups.find(simId); if (it != data->groups.end()) it->second->unitHandles.clear(); }
        return JassValue::Null();
    });
    auto filterFuncAt = [](const std::vector<JassValue>& args, size_t index) -> std::string {
        return (args.size() > index && args[index].type == JassValue::STRING) ? args[index].strVal : "";
    };
    auto acceptFilteredUnit = [data = data_, vmPtr = vmPtr_](int unitHandle, const std::string& filterFunc) {
        if (filterFunc.empty() || !vmPtr) return true;
        const int previousFilterUnit = data->filterUnitHandle;
        data->filterUnitHandle = unitHandle;
        const bool accepted = JassValueToBool(CallSyncFunction(vmPtr, filterFunc));
        data->filterUnitHandle = previousFilterUnit;
        return accepted;
    };
    vm_.RegisterNative("GroupEnumUnitsOfType", [data = data_, sim = sim_, filterFuncAt, acceptFilteredUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Null();
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Null();
        const std::string unitName = args[1].type == JassValue::STRING
            ? args[1].strVal
            : "";
        const War3ID rawcodeType((uint32_t)args[1].intVal);
        const std::string filterFunc = filterFuncAt(args, 2);
        git->second->unitHandles.clear();
        if (sim) {
            for (auto* unit : sim->GetUnits()) {
                const CUnitType* unitType = unit ? unit->GetUnitType() : nullptr;
                const bool typeMatches = unitType
                    ? (!unitName.empty() ? unitType->GetLegacyName() == unitName
                                         : unit->GetTypeId() == rawcodeType)
                    : false;
                const int unitHandle = data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
                if (typeMatches && acceptFilteredUnit(unitHandle, filterFunc))
                    git->second->unitHandles.push_back(unitHandle);
            }
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupEnumUnitsInRange", [data = data_, sim = sim_, filterFuncAt, acceptFilteredUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Null();
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Null();
        float x = args[1].realVal, y = args[2].realVal, radius = args[3].realVal;
        float rSq = radius * radius;
        const std::string filterFunc = filterFuncAt(args, 4);
        git->second->unitHandles.clear();
        if (sim) {
            for (auto* unit : sim->GetUnits()) {
                float dx = unit->GetX() - x, dy = unit->GetY() - y;
                const int unitHandle = data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
                if (dx * dx + dy * dy <= rSq && acceptFilteredUnit(unitHandle, filterFunc))
                    git->second->unitHandles.push_back(unitHandle);
            }
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupEnumUnitsOfPlayer", [data = data_, sim = sim_, filterFuncAt, acceptFilteredUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Null();
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Null();
        int playerSimId = data->GetSimHandle(args[1].intVal, JassHandleInfo::PLAYER);
        const std::string filterFunc = filterFuncAt(args, 2);
        git->second->unitHandles.clear();
        if (sim) {
            for (auto* unit : sim->GetUnits()) {
                const int unitHandle = data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
                if (unit->GetPlayerIndex() == playerSimId && acceptFilteredUnit(unitHandle, filterFunc))
                    git->second->unitHandles.push_back(unitHandle);
            }
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupEnumUnitsInRect", [data = data_, sim = sim_, filterFuncAt, acceptFilteredUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int gid = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (gid < 0) return JassValue::Null();
        auto git = data->groups.find(gid);
        if (git == data->groups.end()) return JassValue::Null();
        int rid = data->GetSimHandle(args[1].intVal, JassHandleInfo::RECT);
        if (rid < 0) return JassValue::Null();
        auto rit = data->rects.find(rid);
        if (rit == data->rects.end()) return JassValue::Null();
        float minX = rit->second->minX, minY = rit->second->minY;
        float maxX = rit->second->maxX, maxY = rit->second->maxY;
        const std::string filterFunc = filterFuncAt(args, 2);
        git->second->unitHandles.clear();
        if (sim) {
            for (auto* unit : sim->GetUnits()) {
                float ux = unit->GetX(), uy = unit->GetY();
                const int unitHandle = data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
                if (ux >= minX && ux <= maxX && uy >= minY && uy <= maxY && acceptFilteredUnit(unitHandle, filterFunc))
                    git->second->unitHandles.push_back(unitHandle);
            }
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupEnumUnitsSelected", [data = data_, sim = sim_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupImmediateOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Bool(false);
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Bool(false);
        int orderId = ParseOrderString(args[1].strVal);
        if (orderId <= 0) return JassValue::Bool(false);
        for (int uh : git->second->unitHandles) {
            int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT);
            if (uSim >= 0 && sim) {
                CUnit* u = sim->GetUnit(uSim);
                if (u) u->IssueOrder(std::make_unique<COrderNoTarget>(orderId));
            }
        }
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("GroupPointOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Bool(false);
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Bool(false);
        int orderId = ParseOrderString(args[1].strVal);
        if (orderId <= 0) return JassValue::Bool(false);
        float x = args[2].realVal, y = args[3].realVal;
        for (int uh : git->second->unitHandles) {
            int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT);
            if (uSim >= 0 && sim) {
                CUnit* u = sim->GetUnit(uSim);
                if (u) u->IssueOrder(std::make_unique<COrderTargetPoint>(orderId, x, y));
            }
        }
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("GroupTargetOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Bool(false);
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Bool(false);
        int orderId = ParseOrderString(args[1].strVal);
        if (orderId <= 0) return JassValue::Bool(false);
        int tSim = data->GetSimHandle(args[2].intVal, JassHandleInfo::UNIT);
        if (tSim < 0 || !sim) return JassValue::Bool(false);
        CUnit* target = sim->GetUnit(tSim);
        if (!target) return JassValue::Bool(false);
        for (int uh : git->second->unitHandles) {
            int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT);
            if (uSim >= 0) {
                CUnit* u = sim->GetUnit(uSim);
                if (u) u->IssueOrder(std::make_unique<COrderTargetWidget>(orderId, target));
            }
        }
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("ForGroup", [data = data_, vmPtr = vmPtr_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Null();
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Null();
        const std::string& funcName = args[1].strVal;
        if (funcName.empty()) return JassValue::Null();
        const int previousEnumUnit = data->enumUnitHandle;
        // Snapshot: callbacks may add/remove units (GroupRemoveUnit,
        // DestroyGroup) while iterating.
        const std::vector<int> snapshot = git->second->unitHandles;
        for (int uh : snapshot) {
            data->enumUnitHandle = uh;
            ExecuteSyncFunction(vmPtr, funcName);
        }
        data->enumUnitHandle = previousEnumUnit;
        return JassValue::Null();
    });
    vm_.RegisterNative("FirstOfGroup", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Int(0);
        auto git = data->groups.find(simId);
        if (git == data->groups.end() || git->second->unitHandles.empty()) return JassValue::Int(0);
        return JassValue::Int(git->second->unitHandles.front());
    });
    vm_.RegisterNative("CountUnitsInGroup", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Int(0);
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Int(0);
        return JassValue::Int((int)git->second->unitHandles.size());
    });
    vm_.RegisterNative("IsUnitInGroup", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Bool(false);
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Bool(false);
        auto& vec = git->second->unitHandles;
        return JassValue::Bool(std::find(vec.begin(), vec.end(), args[1].intVal) != vec.end());
    });
}

// ============================================================
// 7. Force natives
// ============================================================
void JassNatives::RegisterForceNatives() {
    vm_.RegisterNative("CreateForce", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto force = std::make_shared<JassForce>();
        int id = data->nextForceId++;
        data->forces[id] = force;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::FORCE, id));
    });
    vm_.RegisterNative("DestroyForce", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        if (simId >= 0) { data->forces.erase(simId); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("ForceAddPlayer", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        if (simId < 0) return JassValue::Null();
        auto fit = data->forces.find(simId);
        if (fit == data->forces.end()) return JassValue::Null();
        int pSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::PLAYER);
        if (pSim >= 0 && std::find(fit->second->playerIds.begin(), fit->second->playerIds.end(), pSim) == fit->second->playerIds.end())
            fit->second->playerIds.push_back(pSim);
        return JassValue::Null();
    });
    vm_.RegisterNative("ForceRemovePlayer", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        if (simId < 0) return JassValue::Null();
        auto fit = data->forces.find(simId);
        if (fit == data->forces.end()) return JassValue::Null();
        int pSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::PLAYER);
        auto& v = fit->second->playerIds;
        v.erase(std::remove(v.begin(), v.end(), pSim), v.end());
        return JassValue::Null();
    });
    vm_.RegisterNative("ForceClear", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        if (simId >= 0) { auto fit = data->forces.find(simId); if (fit != data->forces.end()) fit->second->playerIds.clear(); }
        return JassValue::Null();
    });
    auto addPlayerToForce = [data = data_](JassForce& force, int playerId) {
        if (std::find(force.playerIds.begin(), force.playerIds.end(), playerId) == force.playerIds.end()) {
            force.playerIds.push_back(playerId);
        }
    };
    auto acceptPlayer = [data = data_, vmPtr = vmPtr_](int playerId, const std::string& filterFunc) {
        if (filterFunc.empty() || !vmPtr) return true;
        const int previousFilterPlayer = data->filterPlayerHandle;
        data->filterPlayerHandle = data->AllocHandle(JassHandleInfo::PLAYER, playerId);
        const bool accepted = JassValueToBool(CallSyncFunction(vmPtr, filterFunc));
        data->filterPlayerHandle = previousFilterPlayer;
        return accepted;
    };
    vm_.RegisterNative("ForceEnumPlayers", [data = data_, sim = sim_, addPlayerToForce, acceptPlayer](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int forceId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        if (forceId < 0) return JassValue::Null();
        auto fit = data->forces.find(forceId);
        if (fit == data->forces.end()) return JassValue::Null();
        const std::string filterFunc = (args.size() > 1 && args[1].type == JassValue::STRING) ? args[1].strVal : "";
        const int playerCount = sim ? sim->GetPlayerCount() : 24;
        for (int playerId = 0; playerId < playerCount; ++playerId) {
            if (acceptPlayer(playerId, filterFunc)) {
                addPlayerToForce(*fit->second, playerId);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ForceEnumPlayersCounted", [data = data_, sim = sim_, addPlayerToForce, acceptPlayer](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int forceId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        if (forceId < 0) return JassValue::Null();
        auto fit = data->forces.find(forceId);
        if (fit == data->forces.end()) return JassValue::Null();
        const std::string filterFunc = (args[1].type == JassValue::STRING) ? args[1].strVal : "";
        const int limit = std::max(0, args[2].intVal);
        const int playerCount = sim ? sim->GetPlayerCount() : 24;
        int acceptedCount = 0;
        for (int playerId = 0; playerId < playerCount && acceptedCount < limit; ++playerId) {
            if (acceptPlayer(playerId, filterFunc)) {
                addPlayerToForce(*fit->second, playerId);
                ++acceptedCount;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ForceEnumAllies", [data = data_, sim = sim_, addPlayerToForce, acceptPlayer](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int forceId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        int sourcePlayerId = ResolvePlayerId(data, args[1].intVal);
        if (forceId < 0 || sourcePlayerId < 0) return JassValue::Null();
        auto fit = data->forces.find(forceId);
        CPlayer* sourcePlayer = sim->GetPlayer(sourcePlayerId);
        if (fit == data->forces.end() || !sourcePlayer) return JassValue::Null();
        const std::string filterFunc = (args.size() > 2 && args[2].type == JassValue::STRING) ? args[2].strVal : "";
        for (int playerId = 0; playerId < sim->GetPlayerCount(); ++playerId) {
            CPlayer* player = sim->GetPlayer(playerId);
            if (player && sourcePlayer->HasAlliance(*player, CAllianceType::PASSIVE) && acceptPlayer(playerId, filterFunc)) {
                addPlayerToForce(*fit->second, playerId);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ForceEnumEnemies", [data = data_, sim = sim_, addPlayerToForce, acceptPlayer](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int forceId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        int sourcePlayerId = ResolvePlayerId(data, args[1].intVal);
        if (forceId < 0 || sourcePlayerId < 0) return JassValue::Null();
        auto fit = data->forces.find(forceId);
        CPlayer* sourcePlayer = sim->GetPlayer(sourcePlayerId);
        if (fit == data->forces.end() || !sourcePlayer) return JassValue::Null();
        const std::string filterFunc = (args.size() > 2 && args[2].type == JassValue::STRING) ? args[2].strVal : "";
        for (int playerId = 0; playerId < sim->GetPlayerCount(); ++playerId) {
            CPlayer* player = sim->GetPlayer(playerId);
            if (player && !sourcePlayer->HasAlliance(*player, CAllianceType::PASSIVE) && acceptPlayer(playerId, filterFunc)) {
                addPlayerToForce(*fit->second, playerId);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ForForce", [data = data_, vmPtr = vmPtr_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int forceId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        if (forceId < 0) return JassValue::Null();
        auto fit = data->forces.find(forceId);
        if (fit == data->forces.end()) return JassValue::Null();
        const std::string& funcName = args[1].strVal;
        if (funcName.empty()) return JassValue::Null();
        const int previousEnumPlayer = data->enumPlayerHandle;
        // Snapshot: callback may mutate the force.
        const std::vector<int> snapshot = fit->second->playerIds;
        for (int playerId : snapshot) {
            data->enumPlayerHandle = data->AllocHandle(JassHandleInfo::PLAYER, playerId);
            ExecuteSyncFunction(vmPtr, funcName);
        }
        data->enumPlayerHandle = previousEnumPlayer;
        return JassValue::Null();
    });
    vm_.RegisterNative("CountPlayersInForce", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        if (simId < 0) return JassValue::Int(0);
        auto fit = data->forces.find(simId);
        if (fit == data->forces.end()) return JassValue::Int(0);
        return JassValue::Int((int)fit->second->playerIds.size());
    });
}

// ============================================================
// 8. Location natives
// ============================================================
void JassNatives::RegisterLocationNatives() {
    vm_.RegisterNative("Location", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        float x = args.size() > 0 ? args[0].realVal : 0;
        float y = args.size() > 1 ? args[1].realVal : 0;
        auto loc = std::make_shared<JassLocation>();
        loc->x = x; loc->y = y;
        int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    });
    vm_.RegisterNative("RemoveLocation", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCATION);
        if (simId >= 0) { data->locations.erase(simId); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("MoveLocation", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCATION);
        if (simId >= 0) {
            auto it = data->locations.find(simId);
            if (it != data->locations.end()) { it->second->x = args[1].realVal; it->second->y = args[2].realVal; }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetLocationX", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCATION);
        if (simId >= 0) { auto it = data->locations.find(simId); if (it != data->locations.end()) return JassValue::Real(it->second->x); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetLocationY", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCATION);
        if (simId >= 0) { auto it = data->locations.find(simId); if (it != data->locations.end()) return JassValue::Real(it->second->y); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetLocationZ", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCATION);
        if (simId < 0) return JassValue::Real(0);
        auto it = data->locations.find(simId);
        if (it == data->locations.end() || !it->second) return JassValue::Real(0);
        // Ref Jass2 GetLocationZ: war3MapViewer.terrain.getGroundHeight(x, y).
        if (data->terrainGroundHeight) {
            return JassValue::Real(data->terrainGroundHeight(it->second->x, it->second->y));
        }
        return JassValue::Real(0);
    });
}

// ============================================================
// 9. Rect natives
// ============================================================
void JassNatives::RegisterRectNatives() {
    vm_.RegisterNative("Rect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Int(0);
        auto rect = std::make_shared<JassRect>();
        rect->minX = args[0].realVal; rect->minY = args[1].realVal;
        rect->maxX = args[2].realVal; rect->maxY = args[3].realVal;
        int id = data->nextRectId++;
        data->rects[id] = rect;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::RECT, id));
    });
    vm_.RegisterNative("RectFromLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Int(0);
        int minSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCATION);
        int maxSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (minSim < 0 || maxSim < 0) return JassValue::Int(0);
        auto minIt = data->locations.find(minSim), maxIt = data->locations.find(maxSim);
        if (minIt == data->locations.end() || maxIt == data->locations.end()) return JassValue::Int(0);
        auto rect = std::make_shared<JassRect>();
        rect->minX = std::min(minIt->second->x, maxIt->second->x);
        rect->minY = std::min(minIt->second->y, maxIt->second->y);
        rect->maxX = std::max(minIt->second->x, maxIt->second->x);
        rect->maxY = std::max(minIt->second->y, maxIt->second->y);
        int id = data->nextRectId++;
        data->rects[id] = rect;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::RECT, id));
    });
    vm_.RegisterNative("RemoveRect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (simId >= 0) { data->rects.erase(simId); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("MoveRectToLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        const int rectId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        const int locId = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (rectId < 0 || locId < 0) return JassValue::Null();
        auto rectIt = data->rects.find(rectId);
        auto locIt = data->locations.find(locId);
        if (rectIt == data->rects.end() || locIt == data->locations.end()) return JassValue::Null();
        JassRect& rect = *rectIt->second;
        const float halfWidth = (rect.maxX - rect.minX) * 0.5f;
        const float halfHeight = (rect.maxY - rect.minY) * 0.5f;
        rect.minX = locIt->second->x - halfWidth;
        rect.maxX = locIt->second->x + halfWidth;
        rect.minY = locIt->second->y - halfHeight;
        rect.maxY = locIt->second->y + halfHeight;
        return JassValue::Null();
    });
    vm_.RegisterNative("GetRectCenterX", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (simId >= 0) { auto it = data->rects.find(simId); if (it != data->rects.end()) return JassValue::Real((it->second->minX + it->second->maxX) * 0.5f); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetRectCenterY", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (simId >= 0) { auto it = data->rects.find(simId); if (it != data->rects.end()) return JassValue::Real((it->second->minY + it->second->maxY) * 0.5f); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetRectMinX", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (simId >= 0) { auto it = data->rects.find(simId); if (it != data->rects.end()) return JassValue::Real(it->second->minX); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetRectMinY", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (simId >= 0) { auto it = data->rects.find(simId); if (it != data->rects.end()) return JassValue::Real(it->second->minY); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetRectMaxX", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (simId >= 0) { auto it = data->rects.find(simId); if (it != data->rects.end()) return JassValue::Real(it->second->maxX); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetRectMaxY", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (simId >= 0) { auto it = data->rects.find(simId); if (it != data->rects.end()) return JassValue::Real(it->second->maxY); }
        return JassValue::Real(0);
    });
}

// ============================================================
// 10. Region natives
// ============================================================
void JassNatives::RegisterRegionNatives() {
    vm_.RegisterNative("CreateRegion", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto region = std::make_shared<JassRegion>();
        int id = data->nextRegionId++;
        data->regions[id] = region;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::REGION, id));
    });
    vm_.RegisterNative("RemoveRegion", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        if (simId >= 0) { data->regions.erase(simId); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("RegionAddRect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        int rectSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::RECT);
        if (rSim < 0 || rectSim < 0) return JassValue::Null();
        auto rit = data->regions.find(rSim); auto rectIt = data->rects.find(rectSim);
        if (rit == data->regions.end() || rectIt == data->rects.end()) return JassValue::Null();
        rit->second->rectIds.push_back(rectSim);
        rit->second->rects.push_back(*rectIt->second);
        return JassValue::Null();
    });
    vm_.RegisterNative("RegionClearRect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        int rectSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::RECT);
        if (rSim < 0) return JassValue::Null();
        auto rit = data->regions.find(rSim);
        if (rit == data->regions.end()) return JassValue::Null();
        auto& ids = rit->second->rectIds;
        ids.erase(std::remove(ids.begin(), ids.end(), rectSim), ids.end());
        rit->second->rects.clear();
        for (int rid : rit->second->rectIds) {
            auto rectIt = data->rects.find(rid);
            if (rectIt != data->rects.end()) rit->second->rects.push_back(*rectIt->second);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsUnitInRegion", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        if (rSim < 0) return JassValue::Bool(false);
        auto rit = data->regions.find(rSim);
        if (rit == data->regions.end()) return JassValue::Bool(false);
        int uSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Bool(false);
        CUnit* unit = sim->GetUnit(uSim);
        if (!unit) return JassValue::Bool(false);
        return JassValue::Bool(IsPointInJassRegion(*rit->second, unit->GetX(), unit->GetY()));
    });
}

// ============================================================
// 11. Unit natives (expanded)
// ============================================================
void JassNatives::RegisterUnitNatives() {
    vm_.RegisterNative("ConvertPrimaryTag", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const int ordinal = args[0].intVal;
        if (!JassAnimationPrimaryTagName(ordinal)[0]) return JassValue::Null();
        return JassValue::Int(data->AllocHandle(JassHandleInfo::PRIMARY_TAG, ordinal));
    });
    vm_.RegisterNative("ConvertSecondaryTag", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const int ordinal = args[0].intVal;
        if (!JassAnimationSecondaryTagName(ordinal)[0]) return JassValue::Null();
        return JassValue::Int(data->AllocHandle(JassHandleInfo::SECONDARY_TAG, ordinal));
    });
    vm_.RegisterNative("CreatePrimaryTags", [data = data_](const std::vector<JassValue>&) -> JassValue {
        const int id = data->nextPrimaryTagSetId++;
        data->primaryTagSets[id] = {};
        return JassValue::Int(data->AllocHandle(JassHandleInfo::PRIMARY_TAGS, id));
    });
    vm_.RegisterNative("CreateSecondaryTags", [data = data_](const std::vector<JassValue>&) -> JassValue {
        const int id = data->nextSecondaryTagSetId++;
        data->secondaryTagSets[id] = {};
        return JassValue::Int(data->AllocHandle(JassHandleInfo::SECONDARY_TAGS, id));
    });
    vm_.RegisterNative("PrimaryTagsAdd", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PRIMARY_TAGS);
        const int tag = data->GetSimHandle(args[1].intVal, JassHandleInfo::PRIMARY_TAG);
        if (setId < 0 || tag < 0) return JassValue::Bool(false);
        return JassValue::Bool(data->primaryTagSets[setId].insert(tag).second);
    });
    vm_.RegisterNative("SecondaryTagsAdd", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::SECONDARY_TAGS);
        const int tag = data->GetSimHandle(args[1].intVal, JassHandleInfo::SECONDARY_TAG);
        if (setId < 0 || tag < 0) return JassValue::Bool(false);
        return JassValue::Bool(data->secondaryTagSets[setId].insert(tag).second);
    });
    vm_.RegisterNative("PrimaryTagsRemove", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PRIMARY_TAGS);
        const int tag = data->GetSimHandle(args[1].intVal, JassHandleInfo::PRIMARY_TAG);
        if (setId < 0 || tag < 0) return JassValue::Bool(false);
        return JassValue::Bool(data->primaryTagSets[setId].erase(tag) > 0);
    });
    vm_.RegisterNative("SecondaryTagsRemove", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::SECONDARY_TAGS);
        const int tag = data->GetSimHandle(args[1].intVal, JassHandleInfo::SECONDARY_TAG);
        if (setId < 0 || tag < 0) return JassValue::Bool(false);
        return JassValue::Bool(data->secondaryTagSets[setId].erase(tag) > 0);
    });
    vm_.RegisterNative("PrimaryTagsContains", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PRIMARY_TAGS);
        const int tag = data->GetSimHandle(args[1].intVal, JassHandleInfo::PRIMARY_TAG);
        if (setId < 0 || tag < 0) return JassValue::Bool(false);
        auto it = data->primaryTagSets.find(setId);
        return JassValue::Bool(it != data->primaryTagSets.end() && it->second.count(tag) > 0);
    });
    vm_.RegisterNative("SecondaryTagsContains", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::SECONDARY_TAGS);
        const int tag = data->GetSimHandle(args[1].intVal, JassHandleInfo::SECONDARY_TAG);
        if (setId < 0 || tag < 0) return JassValue::Bool(false);
        auto it = data->secondaryTagSets.find(setId);
        return JassValue::Bool(it != data->secondaryTagSets.end() && it->second.count(tag) > 0);
    });
    vm_.RegisterNative("PrimaryTagsSize", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PRIMARY_TAGS);
        auto it = data->primaryTagSets.find(setId);
        return JassValue::Int(it != data->primaryTagSets.end() ? (int)it->second.size() : 0);
    });
    vm_.RegisterNative("SecondaryTagsSize", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::SECONDARY_TAGS);
        auto it = data->secondaryTagSets.find(setId);
        return JassValue::Int(it != data->secondaryTagSets.end() ? (int)it->second.size() : 0);
    });
    vm_.RegisterNative("PrimaryTagsAny", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PRIMARY_TAGS);
        auto it = data->primaryTagSets.find(setId);
        if (it == data->primaryTagSets.end() || it->second.empty()) return JassValue::Null();
        return JassValue::Int(data->AllocHandle(JassHandleInfo::PRIMARY_TAG, *it->second.begin()));
    });
    vm_.RegisterNative("SecondaryTagsAny", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::SECONDARY_TAGS);
        auto it = data->secondaryTagSets.find(setId);
        if (it == data->secondaryTagSets.end() || it->second.empty()) return JassValue::Null();
        return JassValue::Int(data->AllocHandle(JassHandleInfo::SECONDARY_TAG, *it->second.begin()));
    });
    vm_.RegisterNative("DestroyPrimaryTags", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PRIMARY_TAGS);
        if (setId >= 0) data->primaryTagSets.erase(setId);
        data->FreeHandle(args[0].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("DestroySecondaryTags", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::SECONDARY_TAGS);
        if (setId >= 0) data->secondaryTagSets.erase(setId);
        data->FreeHandle(args[0].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("PopulateTags", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        const int primarySetId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PRIMARY_TAGS);
        const int secondarySetId = data->GetSimHandle(args[1].intVal, JassHandleInfo::SECONDARY_TAGS);
        if (primarySetId < 0 || secondarySetId < 0) return JassValue::Null();
        std::vector<int> primaryTags;
        std::vector<int> secondaryTags;
        JassPopulateAnimationTagsFromSelector(args[2].strVal, primaryTags, secondaryTags);
        auto& primarySet = data->primaryTagSets[primarySetId];
        auto& secondarySet = data->secondaryTagSets[secondarySetId];
        primarySet.clear();
        secondarySet.clear();
        primarySet.insert(primaryTags.begin(), primaryTags.end());
        secondarySet.insert(secondaryTags.begin(), secondaryTags.end());
        return JassValue::Null();
    });

    auto createUnitByTypeId = [data = data_, sim = sim_](int playerArg, War3ID typeId,
                                                          float x, float y, float facing,
                                                          bool addTechUnlocked = true) -> JassValue {
        if (!sim) return JassValue::Null();
        const int playerId = ResolvePlayerId(data, playerArg);
        if (playerId < 0) return JassValue::Null();
        CUnit* unit = sim->CreateUnit(typeId, playerId, x, y, facing);
        if (!unit) return JassValue::Null();
        if (addTechUnlocked) {
            if (CPlayer* player = sim->GetPlayer(playerId)) {
                player->AddTechUnlocked(*sim, typeId);
            }
        }
        return JassValue::Int(data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId()));
    };

    vm_.RegisterNative("CreateUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (!sim || args.size() < 5) return JassValue::Null();
        int playerId = ResolvePlayerId(data, args[0].intVal);
        if (playerId < 0) return JassValue::Null();
        int unitTypeId = args[1].intVal;
        float x = args[2].realVal, y = args[3].realVal, facing = args[4].realVal;
        War3ID w3id((uint32_t)unitTypeId);
        CUnit* unit = sim->CreateUnit(w3id, playerId, x, y, facing);
        if (unit) {
            if (CPlayer* player = sim->GetPlayer(playerId)) {
                player->AddTechUnlocked(*sim, w3id);
            }
        }
        if (unit) return JassValue::Int(data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId()));
        return JassValue::Null();
    });
    vm_.RegisterNative("CreateUnitByName", [sim = sim_, createUnitByTypeId](const std::vector<JassValue>& args) -> JassValue {
        if (!sim || args.size() < 5) return JassValue::Null();
        const int typeId = UnitIdFromLegacyName(sim, args[1].strVal);
        if (typeId == 0) return JassValue::Null();
        return createUnitByTypeId(args[0].intVal, War3ID((uint32_t)typeId),
                                  args[2].realVal, args[3].realVal, args[4].realVal);
    });
    vm_.RegisterNative("CreateUnitAtLoc", [data = data_, createUnitByTypeId](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        float x = 0.0f;
        float y = 0.0f;
        const int locId = data->GetSimHandle(args[2].intVal, JassHandleInfo::LOCATION);
        if (locId >= 0) {
            auto locIt = data->locations.find(locId);
            if (locIt != data->locations.end()) {
                x = locIt->second->x;
                y = locIt->second->y;
            }
        }
        return createUnitByTypeId(args[0].intVal, War3ID((uint32_t)args[1].intVal), x, y, args[3].realVal);
    });
    vm_.RegisterNative("CreateUnitAtLocByName", [data = data_, sim = sim_, createUnitByTypeId](const std::vector<JassValue>& args) -> JassValue {
        if (!sim || args.size() < 4) return JassValue::Null();
        const int typeId = UnitIdFromLegacyName(sim, args[1].strVal);
        if (typeId == 0) return JassValue::Null();
        float x = 0.0f;
        float y = 0.0f;
        const int locId = data->GetSimHandle(args[2].intVal, JassHandleInfo::LOCATION);
        if (locId >= 0) {
            auto locIt = data->locations.find(locId);
            if (locIt != data->locations.end()) {
                x = locIt->second->x;
                y = locIt->second->y;
            }
        }
        return createUnitByTypeId(args[0].intVal, War3ID((uint32_t)typeId), x, y, args[3].realVal);
    });
    vm_.RegisterNative("CreateCorpse", [data = data_, sim = sim_, createUnitByTypeId](const std::vector<JassValue>& args) -> JassValue {
        if (!sim || args.size() < 5) return JassValue::Null();
        JassValue handle = createUnitByTypeId(args[0].intVal, War3ID((uint32_t)args[1].intVal),
                                             args[2].realVal, args[3].realVal, args[4].realVal,
                                             false);
        int simId = data->GetSimHandle(handle.intVal, JassHandleInfo::UNIT);
        if (simId >= 0) {
            CUnit* unit = sim->GetUnit(simId);
            if (unit) unit->SetLife(*sim, 0.0f);
        }
        return handle;
    });
    vm_.RegisterNative("BlzCreateUnitWithSkin", [createUnitByTypeId](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6) return JassValue::Null();
        return createUnitByTypeId(args[0].intVal, War3ID((uint32_t)args[1].intVal),
                                  args[2].realVal, args[3].realVal, args[4].realVal,
                                  false);
    });
    vm_.RegisterNative("RemoveUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (!sim || args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) {
            CUnit* unit = sim->GetUnit(simId);
            if (unit) sim->RemoveUnit(unit);
            data->FreeHandle(args[0].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("KillUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (!sim || args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetLife(*sim, 0.0f); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitLookAt", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });
    vm_.RegisterNative("ResetUnitLookAt", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitResetCooldown", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitOwner", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (!sim || args.size() < 2) return JassValue::Null();
        const int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        const int playerId = ResolvePlayerId(data, args[1].intVal);
        if (simId >= 0 && playerId >= 0 && sim->GetPlayer(playerId)) {
            if (CUnit* unit = sim->GetUnit(simId)) {
                unit->SetPlayerIndex(playerId);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("RemoveGuardPosition", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });
    vm_.RegisterNative("RecycleGuardPosition", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });
    vm_.RegisterNative("RemoveAllGuardPositions", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });
    vm_.RegisterNative("GetUnitTypeId", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Int(unit->GetTypeId().value); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetUnitState", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) {
            CUnit* unit = sim->GetUnit(simId);
            if (unit) {
                switch (args[1].intVal) {
                    case 0: return JassValue::Real(unit->GetLife());
                    case 1: return JassValue::Real(unit->GetMaxLife());
                    case 2: return JassValue::Real(unit->GetMana());
                    case 3: return JassValue::Real((float)unit->GetMaxMana());
                }
            }
        }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("SetUnitState", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) {
            CUnit* unit = sim->GetUnit(simId);
            if (unit) {
                switch (args[1].intVal) {
                    case 0: unit->SetLife(*sim, args[2].realVal); break;
                    case 2: unit->SetMana(args[2].realVal); break;
                }
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetUnitX", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetX()); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetUnitY", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetY()); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetUnitFacing", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetFacing()); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetUnitLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        auto loc = std::make_shared<JassLocation>();
        if (!args.empty() && sim) {
            int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
            if (simId >= 0) {
                CUnit* unit = sim->GetUnit(simId);
                if (unit) {
                    loc->x = unit->GetX();
                    loc->y = unit->GetY();
                }
            }
        }
        int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    });
    vm_.RegisterNative("SetUnitUserData", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) {
            data->unitUserData[simId] = args[1].intVal;
        }
        (void)sim;
        return JassValue::Null();
    });
    vm_.RegisterNative("GetUnitUserData", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Int(0);
        auto it = data->unitUserData.find(simId);
        return JassValue::Int(it != data->unitUserData.end() ? it->second : 0);
    });
    vm_.RegisterNative("SetUnitX", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) {
            CUnit* unit = sim->GetUnit(simId);
            if (unit) unit->SetPositionAndCheckUnstuck(args[1].realVal, unit->GetY(), *sim);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitY", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) {
            CUnit* unit = sim->GetUnit(simId);
            if (unit) unit->SetPositionAndCheckUnstuck(unit->GetX(), args[1].realVal, *sim);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitPosition", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) {
            CUnit* unit = sim->GetUnit(simId);
            if (unit) unit->SetPositionAndCheckUnstuck(args[1].realVal, args[2].realVal, *sim);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitPositionLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (simId >= 0 && locSim >= 0) {
            CUnit* unit = sim->GetUnit(simId);
            auto locIt = data->locations.find(locSim);
            if (unit && locIt != data->locations.end()) {
                unit->SetPositionAndCheckUnstuck(locIt->second->x, locIt->second->y, *sim);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitDamageTarget", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        int srcSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        int tgtSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        if (srcSim >= 0 && tgtSim >= 0) {
            CUnit* src = sim->GetUnit(srcSim);
            CWidget* tgt = sim->GetUnit(tgtSim);
            if (src && tgt) {
                float dmg = args[2].realVal;
                // UnitDamageTarget(whichUnit, target, amount, attack, ranged,
                //                   attackType, damageType, weaponType)
                // Mirror ref CAttackTypeJass.VALUES / CDamageType ordinals.
                bool isAttack = args.size() > 3 ? JassValueToBool(args[3]) : true;
                bool isRanged = args.size() > 4 ? JassValueToBool(args[4]) : false;
                CAttackType attackType = CAttackType::NORMAL;
                if (args.size() > 5) {
                    switch (args[5].intVal) {
                        case 0: attackType = CAttackType::SPELLS; break;
                        case 1: attackType = CAttackType::NORMAL; break;
                        case 2: attackType = CAttackType::PIERCE; break;
                        case 3: attackType = CAttackType::SIEGE; break;
                        case 4: attackType = CAttackType::MAGIC; break;
                        case 5: attackType = CAttackType::CHAOS; break;
                        case 6: attackType = CAttackType::HERO; break;
                        default: attackType = CAttackType::NORMAL; break;
                    }
                }
                CDamageType damageType = CDamageType::NORMAL;
                if (args.size() > 6) {
                    // Ref CDamageType ordinal: 4=NORMAL, 5=ENHANCED, 8=FIRE,
                    // 9=COLD, 10=LIGHTNING, 11=POISON, 12=DISEASE, 13=DIVINE,
                    // 14=MAGIC, 15=SONIC, 16=ACID, 17=FORCE, 18=DEATH,
                    // 19=MIND, 20=PLANT, 21=DEFENSIVE, 22=DEMOLITION,
                    // 23=SLOW_POISON, 24=SPIRIT_LINK, 25=SHADOW_STRIKE,
                    // 26=UNIVERSAL (common.j ConvertDamageType ids).
                    switch (args[6].intVal) {
                        case 4: damageType = CDamageType::NORMAL; break;
                        case 5: damageType = CDamageType::ENHANCED; break;
                        case 8: damageType = CDamageType::FIRE; break;
                        case 9: damageType = CDamageType::COLD; break;
                        case 10: damageType = CDamageType::LIGHTNING; break;
                        case 11: damageType = CDamageType::POISON; break;
                        case 12: damageType = CDamageType::DISEASE; break;
                        case 13: damageType = CDamageType::DIVINE; break;
                        case 14: damageType = CDamageType::MAGIC; break;
                        case 15: damageType = CDamageType::SONIC; break;
                        case 16: damageType = CDamageType::ACID; break;
                        case 17: damageType = CDamageType::FORCE; break;
                        case 18: damageType = CDamageType::DEATH; break;
                        case 19: damageType = CDamageType::MIND; break;
                        case 20: damageType = CDamageType::PLANT; break;
                        case 21: damageType = CDamageType::DEFENSIVE; break;
                        case 22: damageType = CDamageType::DEMOLITION; break;
                        case 23: damageType = CDamageType::SLOW_POISON; break;
                        case 24: damageType = CDamageType::SPIRIT_LINK; break;
                        case 25: damageType = CDamageType::SHADOW_STRIKE; break;
                        case 26: damageType = CDamageType::UNIVERSAL; break;
                        default: damageType = CDamageType::NORMAL; break;
                    }
                }
                auto* combat = sim->GetCombatSystem();
                if (combat) {
                    auto* atkType = src->GetUnitType();
                    int dice = 0, sides = 0;
                    if (atkType && !atkType->GetAttacks().empty()) {
                        dice = atkType->GetAttacks()[0].damageDice;
                        sides = atkType->GetAttacks()[0].damageSidesPerDie;
                    }
                    combat->ApplyDamage(*sim, src, tgt, isAttack, isRanged,
                                        attackType, damageType,
                                        "", dmg, 0, dice, sides);
                } else {
                    tgt->Damage(*sim, src, isAttack, isRanged, attackType, damageType, "", dmg);
                }
                return JassValue::Bool(true);
            }
        }
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("IssuePointOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Bool(false);
        int srcSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (srcSim < 0) return JassValue::Bool(false);
        CUnit* src = sim->GetUnit(srcSim);
        if (!src) return JassValue::Bool(false);
        std::string orderStr = args[1].strVal;
        float x = args[2].realVal, y = args[3].realVal;
        int orderId = ParseOrderString(orderStr);
        if (orderId > 0)
            src->IssueOrder(std::make_unique<COrderTargetPoint>(orderId, x, y));
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("IssueImmediateOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        int srcSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (srcSim < 0) return JassValue::Bool(false);
        CUnit* src = sim->GetUnit(srcSim);
        if (!src) return JassValue::Bool(false);
        std::string orderStr = args[1].strVal;
        int orderId = ParseOrderString(orderStr);
        if (orderId > 0)
            src->IssueOrder(std::make_unique<COrderNoTarget>(orderId));
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("IssueTargetOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        int srcSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (srcSim < 0) return JassValue::Bool(false);
        CUnit* src = sim->GetUnit(srcSim);
        if (!src) return JassValue::Bool(false);
        int orderId = ParseOrderString(args[1].strVal);
        if (orderId <= 0) return JassValue::Bool(false);
        int tgtSim = data->GetSimHandle(args[2].intVal, JassHandleInfo::UNIT);
        CUnit* tgt = tgtSim >= 0 ? sim->GetUnit(tgtSim) : nullptr;
        if (!tgt) return JassValue::Bool(false);
        src->IssueOrder(std::make_unique<COrderTargetWidget>(orderId, tgt));
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("IssuePointOrderLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        int srcSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (srcSim < 0) return JassValue::Bool(false);
        int orderId = ParseOrderString(args[1].strVal);
        if (orderId <= 0) return JassValue::Bool(false);
        int locSim = data->GetSimHandle(args[2].intVal, JassHandleInfo::LOCATION);
        if (locSim < 0) return JassValue::Bool(false);
        auto locIt = data->locations.find(locSim);
        if (locIt == data->locations.end()) return JassValue::Bool(false);
        CUnit* src = sim->GetUnit(srcSim);
        if (!src) return JassValue::Bool(false);
        src->IssueOrder(std::make_unique<COrderTargetPoint>(orderId, locIt->second->x, locIt->second->y));
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("IssueBuildOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Bool(false);
        int srcSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (srcSim < 0) return JassValue::Bool(false);
        CUnit* src = sim->GetUnit(srcSim);
        if (!src) return JassValue::Bool(false);
        War3ID w3id((uint32_t)args[1].intVal);
        src->IssueOrder(std::make_unique<COrderTargetPoint>(w3id.value, args[2].realVal, args[3].realVal));
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("SetUnitFacing", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetFacing(args[1].realVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitFacingTimed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetFacing(args[1].realVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitAnimation", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim) return JassValue::Null();
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Null();
        if (data->unitSetAnimation) data->unitSetAnimation(simId, args[1].strVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitAnimationByIndex", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim) return JassValue::Null();
        if (data->unitSetAnimationByIndex) data->unitSetAnimationByIndex(simId, args[1].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitAnimationWithRarity", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim) return JassValue::Null();
        if (data->unitSetAnimation) data->unitSetAnimation(simId, args[1].strVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("QueueUnitAnimation", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim) return JassValue::Null();
        if (data->unitQueueAnimation) data->unitQueueAnimation(simId, args[1].strVal);
        else if (data->unitSetAnimation) data->unitSetAnimation(simId, args[1].strVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitAnimationByTag", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim || !sim->GetUnit(simId)) return JassValue::Null();
        const std::string primary = ResolvePrimaryTagName(data, args[2].intVal);
        std::vector<std::string> secondary = ResolveSecondaryTagNames(data, args[3].intVal);
        if (data->unitSetAnimationByTags) {
            data->unitSetAnimationByTags(simId, primary, secondary, args[4].realVal, 0.0f, JassValueToBool(args[1]));
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitAnimationByTagWithDuration", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim || !sim->GetUnit(simId)) return JassValue::Null();
        const std::string primary = ResolvePrimaryTagName(data, args[2].intVal);
        std::vector<std::string> secondary = ResolveSecondaryTagNames(data, args[3].intVal);
        if (data->unitSetAnimationByTags) {
            data->unitSetAnimationByTags(simId, primary, secondary, 1.0f, args[4].realVal, JassValueToBool(args[1]));
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitAnimationToWalk", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim || !sim->GetUnit(simId)) return JassValue::Null();
        if (data->unitSetAnimationByTags) {
            data->unitSetAnimationByTags(simId, "walk", {}, 1.0f, 0.0f, JassValueToBool(args[1]));
        } else if (data->unitSetAnimation) {
            data->unitSetAnimation(simId, "walk");
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitAnimationByIndexEx", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim || !sim->GetUnit(simId)) return JassValue::Null();
        if (data->unitSetAnimationByIndexEx) data->unitSetAnimationByIndexEx(simId, args[2].intVal, args[3].realVal);
        else if (data->unitSetAnimationByIndex) data->unitSetAnimationByIndex(simId, args[2].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("QueueUnitAnimationByTag", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim || !sim->GetUnit(simId)) return JassValue::Null();
        const std::string primary = ResolvePrimaryTagName(data, args[1].intVal);
        std::vector<std::string> secondary = ResolveSecondaryTagNames(data, args[2].intVal);
        if (data->unitQueueAnimationByTags) data->unitQueueAnimationByTags(simId, primary, secondary);
        else if (data->unitSetAnimationByTags) data->unitSetAnimationByTags(simId, primary, secondary, 1.0f, 0.0f, false);
        return JassValue::Null();
    });
    vm_.RegisterNative("AddUnitAnimationProperties", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim) return JassValue::Null();
        if (data->unitAddAnimationProperties) data->unitAddAnimationProperties(simId, args[1].strVal, args[2].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("AddUnitAnimationSecondaryTag", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim || !sim->GetUnit(simId)) return JassValue::Bool(false);
        const int ordinal = data->GetSimHandle(args[1].intVal, JassHandleInfo::SECONDARY_TAG);
        const char* tag = JassAnimationSecondaryTagName(ordinal);
        if (!tag[0]) return JassValue::Bool(false);
        if (data->unitSetAnimationSecondaryTag) return JassValue::Bool(data->unitSetAnimationSecondaryTag(simId, tag, true));
        if (data->unitAddAnimationProperties) data->unitAddAnimationProperties(simId, tag, true);
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("RemoveUnitAnimationSecondaryTag", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim || !sim->GetUnit(simId)) return JassValue::Bool(false);
        const int ordinal = data->GetSimHandle(args[1].intVal, JassHandleInfo::SECONDARY_TAG);
        const char* tag = JassAnimationSecondaryTagName(ordinal);
        if (!tag[0]) return JassValue::Bool(false);
        if (data->unitSetAnimationSecondaryTag) return JassValue::Bool(data->unitSetAnimationSecondaryTag(simId, tag, false));
        if (data->unitAddAnimationProperties) data->unitAddAnimationProperties(simId, tag, false);
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("ForceResetUnitCurrentAnimation", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0 && sim && sim->GetUnit(simId) && data->unitForceResetAnimation) {
            data->unitForceResetAnimation(simId);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitTimeScale", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetTimeScale(args[1].realVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitScale", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetScale(args[1].realVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitColor", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetPlayerColor(args[1].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitVertexColor", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0 && sim) {
            CUnit* unit = sim->GetUnit(simId);
            if (unit) unit->SetVertexColor(args[1].intVal, args[2].intVal, args[3].intVal, args[4].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitPathing", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetPathingEnabled(args[1].boolVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("PauseUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetHidden(args.size() > 1 ? args[1].boolVal : true); }
        return JassValue::Null();
    });
    vm_.RegisterNative("ShowUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetHidden(!args[1].boolVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsUnitHidden", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Bool(unit->IsHidden()); }
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("IsUnitPaused", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Bool(unit->IsHidden()); }
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("IsUnitVisible", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(true); });
    vm_.RegisterNative("IsUnitInvisible", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("IsUnitSelected", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("IsUnitAlly", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        const int unitId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        const int playerId = ResolvePlayerId(data, args[1].intVal);
        CUnit* unit = sim->GetUnit(unitId);
        return JassValue::Bool(unit && unit->GetPlayerIndex() == playerId);
    });
    vm_.RegisterNative("IsUnitEnemy", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        const int unitId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        const int playerId = ResolvePlayerId(data, args[1].intVal);
        CUnit* unit = sim->GetUnit(unitId);
        return JassValue::Bool(unit && unit->GetPlayerIndex() != playerId);
    });
    vm_.RegisterNative("GetUnitRace", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim) return JassValue::Int(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit || !unit->GetUnitType()) return JassValue::Int(0);
        return JassValue::Int(UnitRaceToJassOrdinal(unit->GetUnitType()->GetUnitRace().name));
    });
    vm_.RegisterNative("IsUnitRace", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim) return JassValue::Bool(false);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit || !unit->GetUnitType()) return JassValue::Bool(false);
        // Ref Jass2 IsUnitRace compares unit race against a race handle id.
        return JassValue::Bool(UnitRaceToJassOrdinal(unit->GetUnitType()->GetUnitRace().name) == args[1].intVal);
    });
    vm_.RegisterNative("IsUnitIdType", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim) return JassValue::Bool(false);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Bool(false);
        CUnitType* ut = unit->GetUnitType();
        const int idType = args[1].intVal;
        // common.j UNIT_TYPE_* ordinals.
        switch (idType) {
            case 0: return JassValue::Bool(unit->IsHero()); // HERO
            case 1: return JassValue::Bool(unit->IsDead()); // DEAD
            case 2: return JassValue::Bool(ut && ut->IsBuilding()); // STRUCTURE
            case 3: return JassValue::Bool(ut && ut->GetMovementType() == PathingGridMovementType::FLY); // FLYING
            case 4: return JassValue::Bool(ut && ut->GetMovementType() != PathingGridMovementType::FLY); // GROUND
            case 5: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::GIANT)); // GIANT
            case 6: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::UNDEAD)); // UNDEAD
            case 7: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::SUMMONED)); // SUMMONED
            case 8: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::MECHANICAL)); // MECHANICAL
            case 9: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::PEON)); // PEON
            case 10: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::SAPPER)); // SAPPER
            case 11: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::TOWNHALL)); // TOWNHALL
            case 12: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::ANCIENT)); // ANCIENT
            case 13: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::TREE)); // TREE
            case 14: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::WARD)); // WARD
            default: return JassValue::Bool(false);
        }
    });
    vm_.RegisterNative("IsUnitType", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Bool(false);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Bool(false);
        int typeCode = args[1].intVal;
        CUnitType* ut = unit->GetUnitType();
        switch (typeCode) {
            case 0: return JassValue::Bool(unit->IsHero()); // HERO
            case 1: return JassValue::Bool(unit->IsDead()); // DEAD
            case 2: return JassValue::Bool(ut && ut->IsBuilding()); // STRUCTURE
            case 3: return JassValue::Bool(ut && ut->GetMovementType() == PathingGridMovementType::FLY); // FLYING
            case 4: return JassValue::Bool(ut && ut->GetMovementType() != PathingGridMovementType::FLY); // GROUND
            case 5: { // ATTACKS_FLYING
                if (!ut) return JassValue::Bool(false);
                auto& attacks = ut->GetAttacks();
                for (auto& a : attacks) { if (a.CanTargetAir()) return JassValue::Bool(true); }
                return JassValue::Bool(false);
            }
            case 6: { // ATTACKS_GROUND
                if (!ut) return JassValue::Bool(false);
                auto& attacks = ut->GetAttacks();
                for (auto& a : attacks) { if (a.CanTargetGround()) return JassValue::Bool(true); }
                return JassValue::Bool(false);
            }
            case 7: { // MELEE_ATTACKER
                if (!ut) return JassValue::Bool(false);
                auto& attacks = ut->GetAttacks();
                for (auto& a : attacks) { if (a.GetRange() < 200.0f) return JassValue::Bool(true); }
                return JassValue::Bool(false);
            }
            case 8: { // RANGED_ATTACKER
                if (!ut) return JassValue::Bool(false);
                auto& attacks = ut->GetAttacks();
                for (auto& a : attacks) { if (a.GetRange() >= 200.0f) return JassValue::Bool(true); }
                return JassValue::Bool(false);
            }
            case 9: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::GIANT)); // GIANT
            case 10: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::SUMMONED)); // SUMMONED
            case 14: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::UNDEAD)); // UNDEAD
            case 15: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::MECHANICAL)); // MECHANICAL
            case 16: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::PEON)); // PEON
            case 17: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::SAPPER)); // SAPPER
            case 18: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::TOWNHALL)); // TOWNHALL
            case 19: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::ANCIENT)); // ANCIENT
            default: return JassValue::Bool(false);
        }
    });
    vm_.RegisterNative("GetUnitAbilityLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Int(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Int(0);
        War3ID abilId((int)args[1].intVal);
        CAbility* ab = unit->GetAbility(abilId);
        return JassValue::Int(ab ? ab->GetLevel() : 0);
    });
    vm_.RegisterNative("SetUnitAbilityLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Int(args[2].intVal);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Int(args[2].intVal);
        War3ID abilId((int)args[1].intVal);
        CAbility* ab = unit->GetAbility(abilId);
        if (ab) { ab->SetLevel(args[2].intVal); return JassValue::Int(args[2].intVal); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetUnitAcquireRange", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Real(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Real(0);
        if (unit->GetAcquireRange() >= 0.0f) return JassValue::Real(unit->GetAcquireRange());
        CUnitType* ut = unit->GetUnitType();
        return JassValue::Real(ut ? ut->GetDefaultAcquisitionRange() : 0.0f);
    });
    vm_.RegisterNative("GetUnitDefaultAcquireRange", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Real(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Real(0);
        CUnitType* ut = unit->GetUnitType();
        return JassValue::Real(ut ? ut->GetDefaultAcquisitionRange() : 0.0f);
    });
    vm_.RegisterNative("SetUnitAcquireRange", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetAcquireRange(args[1].realVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitExploded", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetLife(*sim, 0.0f); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitInvulnerable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetInvulnerable(args[1].boolVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetUnitMoveSpeed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real((float)unit->GetSpeed()); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("SetUnitMoveSpeed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetSpeed((int)args[1].realVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetUnitDefaultMoveSpeed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Real(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Real(0);
        CUnitType* ut = unit->GetUnitType();
        return JassValue::Real(ut ? (float)ut->GetSpeed() : 0.0f);
    });
    vm_.RegisterNative("GetUnitFlyHeight", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetFlyHeight()); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("SetUnitFlyHeight", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetFlyingHeight(args[1].realVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetUnitTurnSpeed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Real(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Real(0);
        return JassValue::Real(unit->GetTurnRate());
    });
    vm_.RegisterNative("SetUnitTurnSpeed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetTurnRate(args[1].realVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetUnitPropWindow", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Real(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Real(0);
        return JassValue::Real(unit->GetEffectivePropWindow());
    });
    vm_.RegisterNative("SetUnitPropWindow", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetPropWindow(args[1].realVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetUnitFoodUsed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Int(unit->GetFoodUsed()); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetUnitFoodMade", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Int(unit->GetFoodMade()); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetUnitName", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("");
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) {
            CUnit* unit = sim->GetUnit(simId);
            if (unit && unit->GetUnitType()) return JassValue::Str(unit->GetUnitType()->GetName());
        }
        return JassValue::Str("");
    });
    vm_.RegisterNative("GetUnitPointValue", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Int(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Int(0);
        CUnitType* ut = unit->GetUnitType();
        return JassValue::Int(ut ? ut->GetPointValue() : 0);
    });
    vm_.RegisterNative("GetUnitRallyPoint", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim) return JassValue::Int(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Int(0);
        auto loc = std::make_shared<JassLocation>();
        loc->x = unit->GetRallyX();
        loc->y = unit->GetRallyY();
        int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    });
    vm_.RegisterNative("SetUnitRallyPoint", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        int locId = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (simId < 0 || locId < 0 || !sim) return JassValue::Null();
        CUnit* unit = sim->GetUnit(simId);
        auto it = data->locations.find(locId);
        if (unit && it != data->locations.end() && it->second) {
            unit->SetRallyPoint(it->second->x, it->second->y);
        }
        return JassValue::Null();
    });
}

// ============================================================
// 12. Widget natives
// ============================================================
void JassNatives::RegisterWidgetNatives() {
    vm_.RegisterNative("GetWidgetLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetLife()); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("SetWidgetLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetLife(*sim, args[1].realVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetWidgetX", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetX()); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetWidgetY", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetY()); }
        return JassValue::Real(0);
    });
}

// ============================================================
// 13. Player natives
// ============================================================
void JassNatives::RegisterPlayerNatives() {
    vm_.RegisterNative("Player", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        return JassValue::Int(data->AllocHandle(JassHandleInfo::PLAYER, args[0].intVal));
    });
    vm_.RegisterNative("GetLocalPlayer", [data = data_](const std::vector<JassValue>&) -> JassValue {
        const int index = data ? data->localPlayerIndex : 0;
        return JassValue::Int(data ? data->AllocHandle(JassHandleInfo::PLAYER, index) : 0);
    });
    vm_.RegisterNative("GetOwningPlayer", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) {
            CUnit* unit = sim->GetUnit(simId);
            if (unit) return JassValue::Int(data->AllocHandle(JassHandleInfo::PLAYER, unit->GetPlayerIndex()));
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetPlayerId", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(-1);
        return JassValue::Int(data->GetSimHandle(args[0].intVal, JassHandleInfo::PLAYER));
    });
    vm_.RegisterNative("GetPlayerName", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("Player");
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
                return JassValue::Str(p->GetName());
            }
        }
        return JassValue::Str("Player");
    });
    vm_.RegisterNative("SetPlayerName", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
                p->SetName(args[1].strVal);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetPlayerController", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
                return JassValue::Int((int)p->GetController());
            }
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetPlayerSlotState", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        if (sim && sim->GetPlayer(playerId)) return JassValue::Int(1);
        if (playerId >= 0 && playerId < (int)data->playerMapConfig.size() &&
            data->playerMapConfig[playerId].slotActive) {
            return JassValue::Int(1);
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetPlayerState", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        int playerId = ResolvePlayerId(data, args[0].intVal);
        auto* p = sim->GetPlayer(playerId);
        if (!p) return JassValue::Int(0);
        switch (args[1].intVal) {
            case 1: return JassValue::Int(p->GetGold());
            case 2: return JassValue::Int(p->GetLumber());
            case 4: return JassValue::Int(p->GetFoodCap());
            case 5: return JassValue::Int(p->GetFoodUsed());
            case 8: return JassValue::Int((int)p->GetFoodCapMod());
            default: return JassValue::Int(0);
        }
    });
    vm_.RegisterNative("SetPlayerState", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        if (!p) return JassValue::Null();
        switch (args[1].intVal) {
            case 1: p->SetGold(args[2].intVal); break;
            case 2: p->SetLumber(args[2].intVal); break;
            case 4: p->SetFoodCap(args[2].intVal); break;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsPlayerAlly", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        auto* p1 = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        auto* p2 = sim->GetPlayer(ResolvePlayerId(data, args[1].intVal));
        if (p1 && p2) return JassValue::Bool(p1->HasAlliance(*p2, CAllianceType::ALLIED));
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("IsPlayerEnemy", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        return JassValue::Bool(ResolvePlayerId(data, args[0].intVal) != ResolvePlayerId(data, args[1].intVal));
    });
    vm_.RegisterNative("IsPlayerInForce", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int playerId = ResolvePlayerId(data, args[0].intVal);
        int forceId = data->GetSimHandle(args[1].intVal, JassHandleInfo::FORCE);
        if (playerId < 0 || forceId < 0) return JassValue::Bool(false);
        auto fit = data->forces.find(forceId);
        if (fit == data->forces.end()) return JassValue::Bool(false);
        return JassValue::Bool(std::find(fit->second->playerIds.begin(), fit->second->playerIds.end(), playerId) != fit->second->playerIds.end());
    });
    vm_.RegisterNative("IsPlayerObserver", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
                return JassValue::Bool(p->IsObserver());
            }
        }
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("IsPlayerRacePrefSet", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("GetPlayerRace", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
                return JassValue::Int(p->GetRace());
            }
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetPlayerColor", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
                return JassValue::Int(p->GetColor());
            }
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetPlayerStartLocation", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        if (playerId >= 0 && playerId < (int)data->playerMapConfig.size()) {
            const int startIndex = data->playerMapConfig[playerId].startLocationIndex;
            return JassValue::Int(startIndex >= 0 ? startIndex : playerId);
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetPlayerTechMaxAllowed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Int(-1);
        auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        if (p) return JassValue::Int(p->GetTechMaxAllowed(War3ID((uint32_t)args[1].intVal)));
        return JassValue::Int(-1);
    });
    vm_.RegisterNative("SetPlayerTechMaxAllowed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        if (p) p->SetTechMaxAllowed(War3ID((uint32_t)args[1].intVal), args[2].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPlayerAbilityAvailable", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetPlayerTechResearched", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        if (p) p->SetTechLevel(War3ID((uint32_t)args[1].intVal), args[2].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("GetPlayerTechResearched", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        return JassValue::Int(p ? p->GetTechLevel(War3ID((uint32_t)args[1].intVal)) : 0);
    });
    vm_.RegisterNative("GetPlayerTechCount", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        return JassValue::Int(p ? p->GetTechCount() : 0);
    });
    vm_.RegisterNative("GetPlayerUnitCount", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        int count = 0;
        for (auto* unit : sim->GetUnits()) {
            if (unit && unit->GetPlayerIndex() == playerId) ++count;
        }
        return JassValue::Int(count);
    });
    vm_.RegisterNative("GetPlayerStructureCount", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        int count = 0;
        for (auto* unit : sim->GetUnits()) {
            if (unit && unit->GetPlayerIndex() == playerId && unit->IsBuilding()) ++count;
        }
        return JassValue::Int(count);
    });
    vm_.RegisterNative("SetPlayerAlliance", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Null();
        const int sourcePlayerId = ResolvePlayerId(data, args[0].intVal);
        const int targetPlayerId = ResolvePlayerId(data, args[1].intVal);
        auto* src = sim->GetPlayer(sourcePlayerId);
        if (src && targetPlayerId >= 0) {
            src->SetAlliance(targetPlayerId, static_cast<CAllianceType>(args[2].intVal & 0xF), args[3].boolVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetPlayerHandicap", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(1.0f);
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
                return JassValue::Real(p->GetHandicap());
            }
        }
        return JassValue::Real(1.0f);
    });
    vm_.RegisterNative("SetPlayerHandicap", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
                p->SetHandicap(args[1].realVal);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetPlayerHandicapXP", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(1.0f);
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
                return JassValue::Real(p->GetHandicapXP());
            }
        }
        return JassValue::Real(1.0f);
    });
    vm_.RegisterNative("SetPlayerHandicapXP", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
                p->SetHandicapXP(args[1].realVal);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetAllyColorFilterState", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->allyColorFilter = args[0].intVal != 0;
        return JassValue::Null();
    });
}

// ============================================================
// 14. Hero natives
// ============================================================
void JassNatives::RegisterHeroNatives() {
    vm_.RegisterNative("GetHeroLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (CAbilityHero* hero = FindHeroAbility(sim, simId)) {
            return JassValue::Int(hero->GetHeroLevel());
        }
        if (sim) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Int(unit->GetHeroLevel()); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("SetHeroLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Null();
        if (CAbilityHero* hero = FindHeroAbility(sim, simId)) {
            hero->SetHeroLevel(*sim, sim->GetUnit(simId), args[1].intVal);
        } else if (CUnit* unit = sim->GetUnit(simId)) {
            unit->SetHeroLevel(args[1].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ReviveHeroLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (!sim || args.size() < 2) return JassValue::Int(0);
        const int unitId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        const int locId = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (unitId < 0) return JassValue::Int(0);
        CUnit* unit = sim->GetUnit(unitId);
        if (!unit) return JassValue::Int(0);
        auto locIt = data->locations.find(locId);
        if (locIt != data->locations.end() && locIt->second) {
            unit->SetPosition(locIt->second->x, locIt->second->y);
        }
        unit->SetLife(*sim, std::max(1.0f, unit->GetMaxLife() * 0.5f));
        return JassValue::Int(args[0].intVal);
    });
    vm_.RegisterNative("GetHeroXP", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        if (CAbilityHero* hero = FindHeroAbility(sim, data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT))) {
            return JassValue::Int(hero->GetXP());
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("SetHeroXP", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        if (CAbilityHero* hero = FindHeroAbility(sim, data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT))) {
            hero->SetXP(*sim, sim->GetUnit(data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT)), args[1].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("AddHeroXP", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (CAbilityHero* hero = FindHeroAbility(sim, simId)) {
            hero->AddXP(*sim, sim->GetUnit(simId), args[1].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetHeroStr", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (CAbilityHero* hero = FindHeroAbility(sim, simId)) {
            return JassValue::Int(hero->GetStrength().GetCurrent(hero->GetHeroLevel()));
        }
        if (sim) { CUnit* unit = sim->GetUnit(simId); if (unit && unit->GetUnitType()) return JassValue::Int(unit->GetUnitType()->GetStartingStrength()); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetHeroAgi", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (CAbilityHero* hero = FindHeroAbility(sim, simId)) {
            return JassValue::Int(hero->GetAgility().GetCurrent(hero->GetHeroLevel()));
        }
        if (sim) { CUnit* unit = sim->GetUnit(simId); if (unit && unit->GetUnitType()) return JassValue::Int(unit->GetUnitType()->GetStartingAgility()); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetHeroInt", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (CAbilityHero* hero = FindHeroAbility(sim, simId)) {
            return JassValue::Int(hero->GetIntelligence().GetCurrent(hero->GetHeroLevel()));
        }
        if (sim) { CUnit* unit = sim->GetUnit(simId); if (unit && unit->GetUnitType()) return JassValue::Int(unit->GetUnitType()->GetStartingIntelligence()); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("SetHeroStr", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (CAbilityHero* hero = FindHeroAbility(sim, simId)) {
            hero->SetStrengthBase(args[1].intVal);
            hero->AddStrengthBonus(*sim, sim->GetUnit(simId), 0);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetHeroAgi", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (CAbilityHero* hero = FindHeroAbility(sim, simId)) {
            hero->SetAgilityBase(args[1].intVal);
            hero->AddAgilityBonus(*sim, sim->GetUnit(simId), 0);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetHeroInt", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (CAbilityHero* hero = FindHeroAbility(sim, simId)) {
            hero->SetIntelligenceBase(args[1].intVal);
            hero->AddIntelligenceBonus(*sim, sim->GetUnit(simId), 0);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ModifyHeroStat", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        // (whichHero, statId, modifyMethod, value): stat 0=Str,1=Agi,2=Int;
        // method 0=set,1=add,2=subtract (common.j MODIFY_HERO_*).
        if (args.size() < 4 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CAbilityHero* hero = FindHeroAbility(sim, simId);
        if (!hero) return JassValue::Null();
        CUnit* unit = sim->GetUnit(simId);
        const int stat = args[1].intVal;
        const int method = args[2].intVal;
        const int value = args[3].intVal;
        if (stat == 0) {
            if (method == 0) { hero->SetStrengthBase(value); hero->AddStrengthBonus(*sim, unit, 0); }
            if (method == 1) hero->AddStrengthBonus(*sim, unit, value);
            if (method == 2) hero->AddStrengthBonus(*sim, unit, -value);
        } else if (stat == 1) {
            if (method == 0) { hero->SetAgilityBase(value); hero->AddAgilityBonus(*sim, unit, 0); }
            if (method == 1) hero->AddAgilityBonus(*sim, unit, value);
            if (method == 2) hero->AddAgilityBonus(*sim, unit, -value);
        } else if (stat == 2) {
            if (method == 0) { hero->SetIntelligenceBase(value); hero->AddIntelligenceBonus(*sim, unit, 0); }
            if (method == 1) hero->AddIntelligenceBonus(*sim, unit, value);
            if (method == 2) hero->AddIntelligenceBonus(*sim, unit, -value);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitAddHeroAbility", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(true); });
    vm_.RegisterNative("SelectHeroSkill", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (CAbilityHero* hero = FindHeroAbility(sim, simId)) {
            hero->SelectHeroSkill(*sim, sim->GetUnit(simId), War3ID((uint32_t)args[1].intVal));
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetHeroSkillPoints", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        if (CAbilityHero* hero = FindHeroAbility(sim, data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT))) {
            return JassValue::Int(hero->GetSkillPoints());
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("UnitModifySkillPoints", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        CAbilityHero* hero = FindHeroAbility(sim, data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT));
        if (!hero) return JassValue::Bool(false);
        hero->SetSkillPoints(std::max(0, hero->GetSkillPoints() + args[1].intVal));
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("UnitStripHeroLevel", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("UnitRemoveHeroAbility", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
}

// ============================================================
// 15. Item natives
// ============================================================
void JassNatives::RegisterItemNatives() {
    vm_.RegisterNative("CreateItem", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Int(0);
        War3ID w3id((uint32_t)args[0].intVal);
        float x = args[1].realVal, y = args[2].realVal;
        return JassValue::Int(data ? data->CreateItemHandle(sim, w3id, x, y) : 0);
    });
    vm_.RegisterNative("BlzCreateItemWithSkin", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Int(0);
        War3ID w3id((uint32_t)args[0].intVal);
        float x = args[1].realVal, y = args[2].realVal;
        return JassValue::Int(data ? data->CreateItemHandle(sim, w3id, x, y) : 0);
    });
    vm_.RegisterNative("ChooseRandomItem", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim || !sim->GetItemData()) return JassValue::Int(0);
        const War3ID itemId = sim->GetItemData()->ChooseRandomItem(args[0].intVal, s_rng);
        return JassValue::Int((int)itemId.value);
    });
    vm_.RegisterNative("ChooseRandomItemEx", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim || !sim->GetItemData()) return JassValue::Int(0);
        const CItemClass itemClass = ItemClassFromJassOrdinal(args[0].intVal);
        const War3ID itemId = sim->GetItemData()->ChooseRandomItem(itemClass, args[1].intVal, s_rng);
        return JassValue::Int((int)itemId.value);
    });
    vm_.RegisterNative("CreateItemLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        int locId = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (locId < 0) return JassValue::Int(0);
        auto locIt = data->locations.find(locId);
        if (locIt == data->locations.end()) return JassValue::Int(0);
        War3ID w3id((uint32_t)args[0].intVal);
        return JassValue::Int(data ? data->CreateItemHandle(sim, w3id, locIt->second->x, locIt->second->y) : 0);
    });
    vm_.RegisterNative("RemoveItem", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const int itemId = data ? data->GetSimHandle(args[0].intVal, JassHandleInfo::ITEM) : -1;
        if (sim && itemId >= 0) {
            if (auto* item = GetJassItem(data, args[0].intVal)) {
                item->SetLife(*sim, 0.0f);
                sim->RemoveItem(item);
            }
        }
        if (itemId >= 0) {
            data->items.erase(itemId);
            data->FreeHandle(args[0].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetItemTypeId", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        (void)sim;
        auto* item = GetJassItem(data, args[0].intVal);
        return JassValue::Int(item ? (int)item->GetTypeId().value : 0);
    });
    vm_.RegisterNative("GetItemLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(100);
        (void)sim;
        auto* item = GetJassItem(data, args[0].intVal);
        return JassValue::Real(item ? item->GetLife() : 100.0f);
    });
    vm_.RegisterNative("SetItemLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        if (auto* item = GetJassItem(data, args[0].intVal)) {
            item->SetLife(*sim, args[1].realVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetItemX", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        (void)sim;
        auto* item = GetJassItem(data, args[0].intVal);
        return JassValue::Real(item ? item->GetX() : 0.0f);
    });
    vm_.RegisterNative("GetItemY", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        (void)sim;
        auto* item = GetJassItem(data, args[0].intVal);
        return JassValue::Real(item ? item->GetY() : 0.0f);
    });
    vm_.RegisterNative("SetItemPosition", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        (void)sim;
        if (auto* item = GetJassItem(data, args[0].intVal)) {
            item->SetPosition(args[1].realVal, args[2].realVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetItemDropOnDeath", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto* item = GetJassItem(data, args[0].intVal)) {
            item->SetDroppedOnDeath(args[1].boolVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetItemDroppable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto* item = GetJassItem(data, args[0].intVal)) {
            item->SetDroppable(args[1].boolVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetItemPawnable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto* item = GetJassItem(data, args[0].intVal)) {
            item->SetPawnable(args[1].boolVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetItemInvulnerable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto* item = GetJassItem(data, args[0].intVal)) {
            item->SetInvulnerable(args[1].boolVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsItemInvulnerable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        auto* item = GetJassItem(data, args[0].intVal);
        return JassValue::Bool(item ? item->IsInvulnerable() : false);
    });
    vm_.RegisterNative("SetItemVisible", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto* item = GetJassItem(data, args[0].intVal)) {
            item->SetHidden(!args[1].boolVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsItemVisible", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        auto* item = GetJassItem(data, args[0].intVal);
        return JassValue::Bool(item ? !item->IsHidden() : false);
    });
    vm_.RegisterNative("IsItemPowerup", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        auto* item = GetJassItem(data, args[0].intVal);
        return JassValue::Bool(item ? item->IsPowerup() : false);
    });
    vm_.RegisterNative("IsItemOwned", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        auto* item = GetJassItem(data, args[0].intVal);
        return JassValue::Bool(item ? item->GetOwner() != nullptr : false);
    });
    vm_.RegisterNative("IsItemSellable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim || !sim->GetItemData()) return JassValue::Bool(false);
        auto* item = GetJassItem(data, args[0].intVal);
        const CItemType* type = item ? sim->GetItemData()->GetItem(item->GetTypeId()) : nullptr;
        return JassValue::Bool(type ? type->sellable : false);
    });
    vm_.RegisterNative("IsItemPawnable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        auto* item = GetJassItem(data, args[0].intVal);
        return JassValue::Bool(item ? item->IsPawnable() : false);
    });
    vm_.RegisterNative("IsItemIdPowerup", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim || !sim->GetItemData()) return JassValue::Bool(false);
        const CItemType* type = sim->GetItemData()->GetItem(War3ID((uint32_t)args[0].intVal));
        return JassValue::Bool(type ? type->powerup : false);
    });
    vm_.RegisterNative("IsItemIdSellable", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim || !sim->GetItemData()) return JassValue::Bool(false);
        const CItemType* type = sim->GetItemData()->GetItem(War3ID((uint32_t)args[0].intVal));
        return JassValue::Bool(type ? type->sellable : false);
    });
    vm_.RegisterNative("IsItemIdPawnable", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim || !sim->GetItemData()) return JassValue::Bool(false);
        const CItemType* type = sim->GetItemData()->GetItem(War3ID((uint32_t)args[0].intVal));
        return JassValue::Bool(type ? type->pawnable : false);
    });
    vm_.RegisterNative("IsItemIdPerishable", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim || !sim->GetItemData()) return JassValue::Bool(false);
        const CItemType* type = sim->GetItemData()->GetItem(War3ID((uint32_t)args[0].intVal));
        return JassValue::Bool(type ? type->perishable : false);
    });
    vm_.RegisterNative("EnumItemsInRect", [data = data_, sim = sim_, vmPtr = vmPtr_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const int rectId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (rectId < 0) return JassValue::Null();
        auto rectIt = data->rects.find(rectId);
        if (rectIt == data->rects.end()) return JassValue::Null();

        const std::string filterFunc = (args.size() > 1 && args[1].type == JassValue::STRING) ? args[1].strVal : "";
        const std::string actionFunc = (args.size() > 2 && args[2].type == JassValue::STRING) ? args[2].strVal : "";
        const int previousFilterItem = data->filterItemHandle;
        const int previousEnumItem = data->enumItemHandle;
        const auto rect = rectIt->second;

        std::vector<int> matchingItems;
        auto acceptItem = [&](int itemId, CItem* item) {
            if (!item || item->IsDead() || item->IsHidden()) return;
            data->items[itemId] = item;
            const int itemHandle = data->AllocHandle(JassHandleInfo::ITEM, itemId);
            bool accepted = true;
            if (!filterFunc.empty() && vmPtr) {
                data->filterItemHandle = itemHandle;
                accepted = JassValueToBool(CallSyncFunction(vmPtr, filterFunc));
            }
            if (accepted) {
                matchingItems.push_back(itemHandle);
            }
        };

        bool usedWorldCollision = false;
        if (sim && sim->GetWorldCollision()) {
            std::vector<CItem*> worldItems;
            sim->GetWorldCollision()->EnumItemsInRect(rect->minX, rect->minY,
                                                      rect->maxX - rect->minX,
                                                      rect->maxY - rect->minY,
                                                      worldItems);
            for (CItem* item : worldItems) {
                acceptItem(item->GetHandleId(), item);
            }
            usedWorldCollision = true;
        }
        if (!usedWorldCollision) {
            for (const auto& [itemId, item] : data->items) {
                if (!item || item->IsDead() || item->IsHidden()) continue;
                const float x = item->GetX();
                const float y = item->GetY();
                if (x < rect->minX || x > rect->maxX || y < rect->minY || y > rect->maxY) continue;
                acceptItem(itemId, item);
            }
        }

        data->filterItemHandle = previousFilterItem;
        if (!actionFunc.empty() && vmPtr) {
            for (int itemHandle : matchingItems) {
                data->enumItemHandle = itemHandle;
                CallSyncFunction(vmPtr, actionFunc);
            }
        }
        data->enumItemHandle = previousEnumItem;
        return JassValue::Null();
    });
    vm_.RegisterNative("GetItemPlayer", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        auto* item = GetJassItem(data, args[0].intVal);
        auto* owner = item ? item->GetOwner() : nullptr;
        return owner ? JassValue::Int(data->AllocHandle(JassHandleInfo::PLAYER, owner->GetPlayerIndex()))
                     : JassValue::Null();
    });
    vm_.RegisterNative("SetItemPlayer", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("GetItemCharges", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        auto* item = GetJassItem(data, args[0].intVal);
        return JassValue::Int(item ? item->GetCharges() : 0);
    });
    vm_.RegisterNative("SetItemCharges", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto* item = GetJassItem(data, args[0].intVal)) {
            item->SetCharges(args[1].intVal);
            item->SetCharged(args[1].intVal > 0);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetItemName", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("");
        auto* item = GetJassItem(data, args[0].intVal);
        return JassValue::Str(item ? item->GetName() : "");
    });
    vm_.RegisterNative("GetItemLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim || !sim->GetItemData()) return JassValue::Int(0);
        auto* item = GetJassItem(data, args[0].intVal);
        if (!item) return JassValue::Int(0);
        const CItemType* type = sim->GetItemData()->GetItem(item->GetTypeId());
        return JassValue::Int(type ? type->level : 0);
    });
    vm_.RegisterNative("GetItemType", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        auto* item = GetJassItem(data, args[0].intVal);
        return JassValue::Int(item ? (int)item->GetItemClass() : 0);
    });
    vm_.RegisterNative("SetItemDropID", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto* item = GetJassItem(data, args[0].intVal)) {
            item->SetDropId(War3ID((uint32_t)args[1].intVal));
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetItemDropID", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        auto* item = GetJassItem(data, args[0].intVal);
        return JassValue::Int(item ? (int)item->GetDropId().value : 0);
    });
    vm_.RegisterNative("GetItemUserData", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        auto* item = GetJassItem(data, args[0].intVal);
        return JassValue::Int(item ? item->GetUserData() : 0);
    });
    vm_.RegisterNative("SetItemUserData", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto* item = GetJassItem(data, args[0].intVal)) {
            item->SetUserData(args[1].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitAddItem", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        auto* unit = sim->GetUnit(uSim);
        auto* item = GetJassItem(data, args[1].intVal);
        if (!unit || !item) return JassValue::Bool(false);
        auto* inventory = dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv")));
        return JassValue::Bool(inventory ? inventory->PickUpItem(*sim, unit, item) : false);
    });
    vm_.RegisterNative("UnitAddItemById", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Int(0);
        War3ID w3id((uint32_t)args[1].intVal);
        CUnit* unit = sim->GetUnit(uSim);
        if (!unit) return JassValue::Int(0);
        const int itemHandle = data ? data->CreateItemHandle(sim, w3id, unit->GetX(), unit->GetY()) : 0;
        if (auto* item = GetJassItem(data, itemHandle)) {
            if (auto* inventory = dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv")))) {
                inventory->PickUpItem(*sim, unit, item);
            }
        }
        return JassValue::Int(itemHandle);
    });
    vm_.RegisterNative("UnitAddItemToSlotById", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Int(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Int(0);
        CUnit* unit = sim->GetUnit(uSim);
        if (!unit) return JassValue::Int(0);
        War3ID w3id((uint32_t)args[1].intVal);
        const int itemHandle = data ? data->CreateItemHandle(sim, w3id, unit->GetX(), unit->GetY()) : 0;
        if (auto* item = GetJassItem(data, itemHandle)) {
            if (auto* inventory = dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv")))) {
                inventory->PickUpItem(*sim, unit, item, args[2].intVal);
            }
        }
        return JassValue::Int(itemHandle);
    });
    vm_.RegisterNative("UnitRemoveItem", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        auto* unit = sim->GetUnit(data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT));
        auto* item = GetJassItem(data, args[1].intVal);
        auto* inventory = unit ? dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv"))) : nullptr;
        if (inventory && item) {
            inventory->DropItem(*sim, unit, item);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitRemoveItemFromSlot", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        auto* unit = sim->GetUnit(data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT));
        auto* inventory = unit ? dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv"))) : nullptr;
        if (!inventory) return JassValue::Int(0);
        CItem* item = inventory->GetItem(args[1].intVal);
        if (!item) return JassValue::Int(0);
        const int handle = data->AllocHandle(JassHandleInfo::ITEM, item->GetHandleId());
        inventory->DropItem(*sim, unit, args[1].intVal);
        return JassValue::Int(handle);
    });
    vm_.RegisterNative("UnitHasItem", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        auto* unit = sim->GetUnit(data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT));
        auto* item = GetJassItem(data, args[1].intVal);
        auto* inventory = unit ? dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv"))) : nullptr;
        return JassValue::Bool(inventory && item && inventory->HasItem(item));
    });
    vm_.RegisterNative("UnitItemInSlot", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        auto* unit = sim->GetUnit(data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT));
        auto* inventory = unit ? dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv"))) : nullptr;
        if (!inventory) return JassValue::Int(0);
        CItem* item = inventory->GetItem(args[1].intVal);
        if (!item) return JassValue::Int(0);
        return JassValue::Int(data->AllocHandle(JassHandleInfo::ITEM, item->GetHandleId()));
    });
    vm_.RegisterNative("UnitInventorySize", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        auto* unit = sim->GetUnit(data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT));
        auto* inventory = unit ? dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv"))) : nullptr;
        return JassValue::Int(inventory ? inventory->GetItemCapacity() : 0);
    });
    vm_.RegisterNative("UnitDropItemPoint", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Bool(false);
        auto* unit = sim->GetUnit(data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT));
        auto* item = GetJassItem(data, args[1].intVal);
        auto* inventory = unit ? dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv"))) : nullptr;
        return JassValue::Bool(inventory && item && inventory->DropItemAt(*sim, unit, item, args[2].realVal, args[3].realVal));
    });
    vm_.RegisterNative("UnitDropItemSlot", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        auto* unit = sim->GetUnit(data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT));
        auto* item = GetJassItem(data, args[1].intVal);
        auto* inventory = unit ? dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv"))) : nullptr;
        return JassValue::Bool(inventory && item && inventory->MoveItemToSlot(item, args[2].intVal));
    });
    vm_.RegisterNative("UnitDropItemTarget", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        auto* unit = sim->GetUnit(data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT));
        auto* item = GetJassItem(data, args[1].intVal);
        auto* target = GetJassWidget(data, sim, args[2].intVal);
        auto* inventory = unit ? dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv"))) : nullptr;
        return JassValue::Bool(inventory && item && target &&
                               inventory->DropItemAt(*sim, unit, item, target->GetX(), target->GetY()));
    });
    vm_.RegisterNative("UnitUseItem", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        auto* unit = sim->GetUnit(data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT));
        auto* item = GetJassItem(data, args[1].intVal);
        auto* inventory = unit ? dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv"))) : nullptr;
        return JassValue::Bool(inventory && item && inventory->UseItem(*sim, unit, item));
    });
    vm_.RegisterNative("UnitUseItemPoint", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Bool(false);
        auto* unit = sim->GetUnit(data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT));
        auto* item = GetJassItem(data, args[1].intVal);
        auto* inventory = unit ? dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv"))) : nullptr;
        return JassValue::Bool(inventory && item && inventory->UseItem(*sim, unit, item));
    });
    vm_.RegisterNative("UnitUseItemTarget", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        auto* unit = sim->GetUnit(data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT));
        auto* item = GetJassItem(data, args[1].intVal);
        auto* inventory = unit ? dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv"))) : nullptr;
        return JassValue::Bool(inventory && item && inventory->UseItem(*sim, unit, item));
    });
}

// ============================================================
// 16. Destructable natives
// ============================================================
void JassNatives::RegisterDestructableNatives() {
    vm_.RegisterNative("CreateDestructable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5 || !sim) return JassValue::Int(0);
        War3ID w3id((uint32_t)args[0].intVal);
        float x = args[1].realVal, y = args[2].realVal, face = args[3].realVal, scale = args[4].realVal;
        (void)face; (void)scale;
        auto* dest = sim->CreateDestructable(w3id, x, y, 100.0f);
        if (!dest) return JassValue::Int(0);
        int jassHandle = data->AllocHandle(JassHandleInfo::DESTRUCTABLE, dest->GetHandleId());
        return JassValue::Int(jassHandle);
    });
    vm_.RegisterNative("CreateDestructableZ", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5 || !sim) return JassValue::Int(0);
        War3ID w3id((uint32_t)args[0].intVal);
        float x = args[1].realVal, y = args[2].realVal;
        auto* dest = sim->CreateDestructable(w3id, x, y, 100.0f);
        if (!dest) return JassValue::Int(0);
        return JassValue::Int(data->AllocHandle(JassHandleInfo::DESTRUCTABLE, dest->GetHandleId()));
    });
    vm_.RegisterNative("CreateDeadDestructable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5 || !sim) return JassValue::Int(0);
        War3ID w3id((uint32_t)args[0].intVal);
        float x = args[1].realVal, y = args[2].realVal;
        auto* dest = sim->CreateDestructable(w3id, x, y, 0.0f);
        if (!dest) return JassValue::Int(0);
        return JassValue::Int(data->AllocHandle(JassHandleInfo::DESTRUCTABLE, dest->GetHandleId()));
    });
    vm_.RegisterNative("CreateDeadDestructableZ", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5 || !sim) return JassValue::Int(0);
        War3ID w3id((uint32_t)args[0].intVal);
        float x = args[1].realVal, y = args[2].realVal;
        auto* dest = sim->CreateDestructable(w3id, x, y, 0.0f);
        if (!dest) return JassValue::Int(0);
        return JassValue::Int(data->AllocHandle(JassHandleInfo::DESTRUCTABLE, dest->GetHandleId()));
    });
    vm_.RegisterNative("RemoveDestructable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        if (sim) {
            if (auto* destructable = sim->GetDestructable(id)) {
                sim->RemoveDestructable(destructable);
            }
        }
        if (id >= 0) data->FreeHandle(args[0].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("KillDestructable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Null();
        const int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        if (auto* destructable = sim->GetDestructable(id)) {
            destructable->SetLife(*sim, 0.0f);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetDestructableTypeId", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        if (!sim) return JassValue::Int(0);
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        auto* destructable = sim->GetDestructable(id);
        return JassValue::Int(destructable ? (int)destructable->GetTypeId().value : 0);
    });
    vm_.RegisterNative("GetDestructableLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(100);
        if (!sim) return JassValue::Real(100);
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        auto* destructable = sim->GetDestructable(id);
        return JassValue::Real(destructable ? destructable->GetLife() : 100.0f);
    });
    vm_.RegisterNative("SetDestructableLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        if (auto* destructable = sim->GetDestructable(id)) {
            destructable->SetLife(*sim, args[1].realVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetDestructableMaxLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Real(100);
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        auto* destructable = sim->GetDestructable(id);
        return JassValue::Real(destructable ? destructable->GetMaxLife() : 100.0f);
    });
    vm_.RegisterNative("SetDestructableMaxLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        if (auto* destructable = sim->GetDestructable(id)) {
            destructable->SetMaxLife(args[1].realVal);
            if (destructable->GetLife() > destructable->GetMaxLife()) {
                destructable->SetLife(*sim, destructable->GetMaxLife());
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetDestructableX", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        if (!sim) return JassValue::Real(0);
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        auto* destructable = sim->GetDestructable(id);
        return JassValue::Real(destructable ? destructable->GetX() : 0.0f);
    });
    vm_.RegisterNative("GetDestructableY", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        if (!sim) return JassValue::Real(0);
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        auto* destructable = sim->GetDestructable(id);
        return JassValue::Real(destructable ? destructable->GetY() : 0.0f);
    });
    vm_.RegisterNative("DestructableRestoreLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        // (destructable, life, birth): restore life up to max.
        if (args.size() < 2 || !sim) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        if (id >= 0) {
            if (auto* destructable = sim->GetDestructable(id)) {
                destructable->SetLife(*sim, std::min(args[1].realVal, destructable->GetMaxLife()));
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("QueueDestructableAnimation", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        if (id >= 0) {
            if (auto* destructable = sim->GetDestructable(id)) destructable->QueueAnimation(args[1].strVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetDestructableAnimation", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        if (id >= 0) {
            if (auto* destructable = sim->GetDestructable(id)) destructable->SetAnimation(args[1].strVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("WaygateActivate", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetWaygateActive(args[1].boolVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("WaygateSetDestination", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0 && sim) {
            CUnit* unit = sim->GetUnit(simId);
            if (unit) unit->SetWaygateDestination(args[1].realVal, args[2].realVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("WaygateGetDestinationX", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetWaygateX()); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("WaygateGetDestinationY", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetWaygateY()); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("EnumDestructablesInRect", [data = data_, sim = sim_, vmPtr = vmPtr_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const int rectId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (rectId < 0) return JassValue::Null();
        auto rectIt = data->rects.find(rectId);
        if (rectIt == data->rects.end()) return JassValue::Null();

        const std::string filterFunc = (args.size() > 1 && args[1].type == JassValue::STRING) ? args[1].strVal : "";
        const std::string actionFunc = (args.size() > 2 && args[2].type == JassValue::STRING) ? args[2].strVal : "";
        const int previousFilterDest = data->filterDestructableHandle;
        const int previousEnumDest = data->enumDestructableHandle;
        const auto rect = rectIt->second;

        std::vector<int> matching;
        if (sim) {
            for (CDestructable* dest : sim->GetDestructables()) {
                if (!dest || dest->IsDead()) continue;
                const float x = dest->GetX();
                const float y = dest->GetY();
                if (x < rect->minX || x > rect->maxX || y < rect->minY || y > rect->maxY) continue;
                const int destHandle = data->AllocHandle(JassHandleInfo::DESTRUCTABLE,
                                                         dest->GetHandleId());
                bool accepted = true;
                if (!filterFunc.empty() && vmPtr && vmPtr->HasFunction(filterFunc)) {
                    data->filterDestructableHandle = destHandle;
                    accepted = JassValueToBool(CallSyncFunction(vmPtr, filterFunc));
                }
                if (accepted) {
                    matching.push_back(destHandle);
                }
            }
        }

        data->filterDestructableHandle = previousFilterDest;
        if (!actionFunc.empty() && vmPtr && vmPtr->HasFunction(actionFunc)) {
            // Snapshot: the action may kill/remove destructables.
            for (int destHandle : matching) {
                data->enumDestructableHandle = destHandle;
                CallSyncFunction(vmPtr, actionFunc);
            }
        }
        data->enumDestructableHandle = previousEnumDest;
        return JassValue::Null();
    });
}

// ============================================================
// 17. Trigger & Event natives
// ============================================================
void JassNatives::RegisterTriggerNatives() {
    vm_.RegisterNative("CreateTrigger", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto trig = std::make_shared<JassTrigger>();
        int id = data->nextTriggerId++;
        trig->id = id;
        data->triggers[id] = trig;
        return JassValue::Int(id);
    });
    vm_.RegisterNative("DestroyTrigger", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        data->triggers.erase(args[0].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("DisableTrigger", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty()) {
            auto it = data->triggers.find(args[0].intVal);
            if (it != data->triggers.end()) it->second->enabled = false;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("EnableTrigger", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty()) {
            auto it = data->triggers.find(args[0].intVal);
            if (it != data->triggers.end()) it->second->enabled = true;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsTriggerEnabled", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        auto it = data->triggers.find(args[0].intVal);
        return JassValue::Bool(it != data->triggers.end() && it->second->enabled);
    });
    vm_.RegisterNative("TriggerRegisterUnitEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) {
            it->second->registeredUnitHandles.push_back(args[1].intVal);
            it->second->eventTypes.push_back(args[2].intVal);
            it->second->registeredEvents.push_back({args[1].intVal, args[2].intVal});
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterDeathEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) {
            it->second->registeredUnitHandles.push_back(args[1].intVal);
            it->second->eventTypes.push_back(JASS_EVENT_UNIT_DEATH);
            it->second->registeredEvents.push_back({args[1].intVal, JASS_EVENT_UNIT_DEATH});
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterTimerEvent", [data = data_, vmPtr = vmPtr_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        float timeout = args[1].realVal;
        bool periodic = args[2].boolVal;
        (void)periodic;
        auto timer = std::make_shared<JassTimer>();
        timer->timeout = timeout;
        timer->remaining = timeout;
        timer->periodic = periodic;
        int timerId = data->nextTimerId++;
        data->timers[timerId] = timer;
        it->second->registeredEvents.push_back({-1, 1000 + timerId});
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterTimerExpireEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        int timerSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::TIMER);
        if (timerSim < 0) return JassValue::Null();
        it->second->registeredEvents.push_back({timerSim, 1001});
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterGameStateEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) {
            it->second->registeredEvents.push_back({args[1].intVal, 3000 + args[2].intVal});
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterEnterRegion", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) {
            it->second->registeredEvents.push_back({args[1].intVal, 4001});
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterLeaveRegion", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) {
            it->second->registeredEvents.push_back({args[1].intVal, 4002});
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterUnitInRange", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) {
            it->second->registeredUnitHandles.push_back(args[1].intVal);
            it->second->eventTypes.push_back(5001);
            it->second->registeredEvents.push_back({args[1].intVal, 5001});
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterPlayerUnitEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        it->second->registeredUnitHandles.push_back(args[1].intVal);
        it->second->eventTypes.push_back(args[2].intVal);
        it->second->registeredEvents.push_back({args[1].intVal, args[2].intVal});
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterPlayerEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        it->second->registeredEvents.push_back({args[1].intVal, args[2].intVal});
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterPlayerStateEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        it->second->registeredEvents.push_back({args[1].intVal, 6000 + args[2].intVal});
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterPlayerChatEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        it->second->registeredEvents.push_back({args[1].intVal, 7000});
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterPlayerAllianceChange", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        it->second->registeredEvents.push_back({args[1].intVal, 200});
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterWidgetEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        it->second->registeredEvents.push_back({args[1].intVal, args[2].intVal});
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterGameEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        it->second->registeredEvents.push_back({0, args[1].intVal});
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterVariableEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        it->second->registeredEvents.push_back({0, 300 + args[2].intVal});
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerAddAction", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        // Returns the action index as its handle so TriggerRemoveAction can
        // revoke it (previously returned nothing; ignored returns unaffected).
        it->second->actionFuncNames.push_back(args[1].strVal);
        return JassValue::Int((int)it->second->actionFuncNames.size() - 1);
    });
    vm_.RegisterNative("TriggerAddCondition", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) {
            it->second->conditionFuncNames.push_back(args[1].strVal);
        }
        return args[1];
    });
    vm_.RegisterNative("TriggerRemoveCondition", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) {
            auto& conditions = it->second->conditionFuncNames;
            auto condIt = std::find(conditions.begin(), conditions.end(), args[1].strVal);
            if (condIt != conditions.end()) conditions.erase(condIt);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerClearConditions", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) it->second->conditionFuncNames.clear();
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerClearActions", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) it->second->actionFuncNames.clear();
        return JassValue::Null();
    });
    vm_.RegisterNative("GetTriggeringTrigger", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->currentTriggerId >= 0 ? JassValue::Int(data->currentTriggerId) : JassValue::Null();
    });
    vm_.RegisterNative("GetTriggerEventId", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->currentTriggerEventId);
    });
    vm_.RegisterNative("GetTriggerUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->currentTriggerUnitHandle >= 0 ? JassValue::Int(data->currentTriggerUnitHandle) : JassValue::Null();
    });
    vm_.RegisterNative("TriggerEvaluate", [this](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        auto it = data_->triggers.find(args[0].intVal);
        if (it == data_->triggers.end()) return JassValue::Bool(false);
        JassQueuedContext context = CaptureContext();
        context.currentTriggerId = args[0].intVal;
        context.currentTriggerEventId = 0;
        return JassValue::Bool(EvaluateTriggerConditions(*it->second, context, false));
    });
    vm_.RegisterNative("TriggerExecute", [this](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        auto it = data_->triggers.find(args[0].intVal);
        if (it == data_->triggers.end()) return JassValue::Null();
        JassQueuedContext context = CaptureContext();
        context.currentTriggerId = args[0].intVal;
        context.currentTriggerEventId = 0;
        for (const auto& funcName : it->second->actionFuncNames) {
            QueueFunctionWithContext(funcName, context);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerExecuteWait", [this](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        auto it = data_->triggers.find(args[0].intVal);
        if (it == data_->triggers.end()) return JassValue::Null();
        // Unlike TriggerExecute (queue and return), run the actions
        // synchronously so the caller observes their effects before
        // continuing (ref CTriggerExecuteWait semantics for the no-sleep
        // case). Actions that request TriggerSleepAction abort their
        // remaining statements, matching CallFunction behavior elsewhere.
        JassQueuedContext context = CaptureContext();
        context.currentTriggerId = args[0].intVal;
        context.currentTriggerEventId = 0;
        for (const auto& funcName : it->second->actionFuncNames) {
            if (funcName.empty() || !vm_.HasFunction(funcName)) continue;
            JassQueuedContext previous = CaptureContext();
            ApplyContext(context);
            vm_.CallFunction(funcName);
            // A synchronous action cannot suspend: drop the sleep request so
            // later executions are not poisoned (see RunThread for the
            // queued-thread equivalent).
            if (vm_.HasPendingSleep()) vm_.TakePendingSleepSeconds();
            ApplyContext(previous);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetTriggerEvalCount", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(0);
        // Note: CreateTrigger returns the raw trigger id (not an
        // allocated handle), matching TriggerRegisterUnitEvent.
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end() && it->second) return JassValue::Int(it->second->evalCount);
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetTriggerExecCount", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(0);
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end() && it->second) return JassValue::Int(it->second->execCount);
        return JassValue::Int(0);
    });
    vm_.RegisterNative("And", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        return JassValue::Bool(args[0].boolVal && args[1].boolVal);
    });
    vm_.RegisterNative("Or", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        return JassValue::Bool(args[0].boolVal || args[1].boolVal);
    });
    vm_.RegisterNative("Not", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        return JassValue::Bool(!args[0].boolVal);
    });
    vm_.RegisterNative("Condition", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        return args[0];
    });
    vm_.RegisterNative("Filter", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        return args[0];
    });
    vm_.RegisterNative("DestroyBoolExpr", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });
    vm_.RegisterNative("ExecuteFunc", [this](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        QueueFunction(args[0].strVal);
        return JassValue::Null();
    });
}

// ============================================================
// 18. Event response natives
// ============================================================
void JassNatives::RegisterEventNatives() {
    auto makeSpellTargetLocation = [data = data_]() -> JassValue {
        auto loc = std::make_shared<JassLocation>();
        loc->x = data->eventSpellTargetX;
        loc->y = data->eventSpellTargetY;
        int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    };

    vm_.RegisterNative("GetFilterUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->filterUnitHandle >= 0 ? JassValue::Int(data->filterUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetEnumUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->enumUnitHandle >= 0 ? JassValue::Int(data->enumUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetFilterItem", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->filterItemHandle >= 0 ? JassValue::Int(data->filterItemHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetEnumItem", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->enumItemHandle >= 0 ? JassValue::Int(data->enumItemHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetEnumDestructable", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->enumDestructableHandle >= 0 ? JassValue::Int(data->enumDestructableHandle)
                                                 : JassValue::Int(0);
    });
    vm_.RegisterNative("GetFilterDestructable", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->filterDestructableHandle >= 0 ? JassValue::Int(data->filterDestructableHandle)
                                                   : JassValue::Int(0);
    });
    vm_.RegisterNative("GetFilterPlayer", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->filterPlayerHandle >= 0 ? JassValue::Int(data->filterPlayerHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetEnumPlayer", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->enumPlayerHandle >= 0 ? JassValue::Int(data->enumPlayerHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetEventDamage", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->eventDamage);
    });
    vm_.RegisterNative("SetEventDamage", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty()) data->eventDamage = args[0].realVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("GetEventDamageSource", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventDamageSourceHandle >= 0 ? JassValue::Int(data->eventDamageSourceHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetEventAttackingUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventAttackingUnitHandle >= 0 ? JassValue::Int(data->eventAttackingUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetAttacker", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventAttackingUnitHandle >= 0 ? JassValue::Int(data->eventAttackingUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetSpellAbilityId", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->eventSpellAbilityId);
    });
    vm_.RegisterNative("GetSpellAbilityUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventSpellAbilityUnitHandle >= 0 ? JassValue::Int(data->eventSpellAbilityUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetSpellTargetX", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->eventSpellTargetX);
    });
    vm_.RegisterNative("GetSpellTargetY", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->eventSpellTargetY);
    });
    vm_.RegisterNative("GetSpellTargetUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventSpellTargetUnitHandle >= 0 ? JassValue::Int(data->eventSpellTargetUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetSpellTargetLoc", [makeSpellTargetLocation](const std::vector<JassValue>&) -> JassValue {
        return makeSpellTargetLocation();
    });
    vm_.RegisterNative("GetSpellTargetPoint", [makeSpellTargetLocation](const std::vector<JassValue>&) -> JassValue {
        return makeSpellTargetLocation();
    });
    vm_.RegisterNative("GetSpellTargetLocation", [makeSpellTargetLocation](const std::vector<JassValue>&) -> JassValue {
        return makeSpellTargetLocation();
    });
    vm_.RegisterNative("GetTriggerPlayer", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventTriggerPlayerHandle >= 0 ? JassValue::Int(data->eventTriggerPlayerHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetTriggerWidget", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventTriggerWidgetHandle >= 0 ? JassValue::Int(data->eventTriggerWidgetHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetTriggerDestructable", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto* info = data->GetHandleInfo(data->eventTriggerWidgetHandle);
        return (info && info->type == JassHandleInfo::DESTRUCTABLE)
            ? JassValue::Int(data->eventTriggerWidgetHandle)
            : JassValue::Int(0);
    });
    vm_.RegisterNative("GetEnteringUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventEnteringUnitHandle >= 0 ? JassValue::Int(data->eventEnteringUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetLeavingUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventLeavingUnitHandle >= 0 ? JassValue::Int(data->eventLeavingUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetManipulatingUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventManipulatingUnitHandle >= 0 ? JassValue::Int(data->eventManipulatingUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetManipulatedItem", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventManipulatedItemHandle >= 0 ? JassValue::Int(data->eventManipulatedItemHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetOrderedUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventOrderedUnitHandle >= 0 ? JassValue::Int(data->eventOrderedUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetIssuedOrderId", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->eventIssuedOrderId);
    });
    vm_.RegisterNative("GetOrderPointX", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->eventOrderPointX);
    });
    vm_.RegisterNative("GetOrderPointY", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->eventOrderPointY);
    });
    vm_.RegisterNative("GetOrderPointLoc", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto loc = std::make_shared<JassLocation>();
        loc->x = data->eventOrderPointX;
        loc->y = data->eventOrderPointY;
        int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    });
    vm_.RegisterNative("GetOrderTarget", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventOrderTargetHandle >= 0 ? JassValue::Int(data->eventOrderTargetHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetOrderTargetUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto* info = data->GetHandleInfo(data->eventOrderTargetHandle);
        return (info && info->type == JassHandleInfo::UNIT) ? JassValue::Int(data->eventOrderTargetHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetOrderTargetDestructable", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto* info = data->GetHandleInfo(data->eventOrderTargetHandle);
        return (info && info->type == JassHandleInfo::DESTRUCTABLE) ? JassValue::Int(data->eventOrderTargetHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetOrderTargetItem", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto* info = data->GetHandleInfo(data->eventOrderTargetHandle);
        return (info && info->type == JassHandleInfo::ITEM) ? JassValue::Int(data->eventOrderTargetHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetKillingUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventKillingUnitHandle >= 0 ? JassValue::Int(data->eventKillingUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetDyingUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventDyingUnitHandle >= 0 ? JassValue::Int(data->eventDyingUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetRevivingUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventRevivingUnitHandle >= 0 ? JassValue::Int(data->eventRevivingUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetChangingUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->currentTriggerUnitHandle >= 0 ? JassValue::Int(data->currentTriggerUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetChangingUnitPrevOwner", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetLevelingUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->currentTriggerUnitHandle >= 0 ? JassValue::Int(data->currentTriggerUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetLearningUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->currentTriggerUnitHandle >= 0 ? JassValue::Int(data->currentTriggerUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetSummonedUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventSummonedUnitHandle >= 0 ? JassValue::Int(data->eventSummonedUnitHandle)
                                                  : JassValue::Int(0);
    });
    vm_.RegisterNative("GetSummoningUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventSummoningUnitHandle >= 0 ? JassValue::Int(data->eventSummoningUnitHandle)
                                                   : JassValue::Int(0);
    });
    vm_.RegisterNative("GetConstructedStructure", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetConstructingStructure", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetCancelledStructure", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetTrainedUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventTrainedUnitHandle >= 0 ? JassValue::Int(data->eventTrainedUnitHandle)
                                                 : JassValue::Int(0);
    });
    vm_.RegisterNative("GetTrainedUnitType", [data = data_, sim = sim_](const std::vector<JassValue>&) -> JassValue {
        if (data->eventTrainedUnitHandle < 0 || !sim) return JassValue::Int(0);
        const int trainedSim = data->GetSimHandle(data->eventTrainedUnitHandle, JassHandleInfo::UNIT);
        const CUnit* trained = sim->GetUnit(trainedSim);
        return JassValue::Int(trained ? (int)trained->GetTypeId().value : 0);
    });
    vm_.RegisterNative("GetResearched", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->eventResearchedId);
    });
    vm_.RegisterNative("GetResearchingUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventResearchingUnitHandle >= 0 ? JassValue::Int(data->eventResearchingUnitHandle)
                                                     : JassValue::Int(0);
    });
    vm_.RegisterNative("GetSoldItem", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetSoldUnit", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetSellingUnit", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetBuyingUnit", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetTransportUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventTransportUnitHandle >= 0 ? JassValue::Int(data->eventTransportUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetLoadedUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventLoadedUnitHandle >= 0 ? JassValue::Int(data->eventLoadedUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetExpiredTimer", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventExpiredTimerHandle >= 0 ? JassValue::Int(data->eventExpiredTimerHandle) : JassValue::Int(0);
    });
}

// ============================================================
// 19. Sound natives
// ============================================================
void JassNatives::RegisterSoundNatives() {
    vm_.RegisterNative("CreateSound", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6) return JassValue::Int(0);
        auto s = std::make_shared<JassSound>();
        s->label = args[0].strVal;
        s->looping = args[1].boolVal;
        int id = data->nextSoundId++;
        data->sounds[id] = s;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::SOUND, id));
    });
    vm_.RegisterNative("CreateSoundFromLabel", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Int(0);
        auto s = std::make_shared<JassSound>();
        s->label = args[0].strVal;
        s->looping = args[1].boolVal;
        int id = data->nextSoundId++;
        data->sounds[id] = s;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::SOUND, id));
    });
    vm_.RegisterNative("CreateMIDISound", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        auto s = std::make_shared<JassSound>();
        if (!args.empty()) {
            s->label = args[0].strVal;
        }
        int id = data->nextSoundId++;
        data->sounds[id] = s;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::SOUND, id));
    });
    vm_.RegisterNative("KillSoundWhenDone", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) it->second->killWhenDone = true;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundVolume", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) { auto it = data->sounds.find(id); if (it != data->sounds.end()) it->second->volume = args[1].realVal / 127.0f; }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundPitch", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) { auto it = data->sounds.find(id); if (it != data->sounds.end()) it->second->pitch = args[1].realVal; }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundDuration", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        const int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) it->second->durationMs = args[1].intVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundParamsFromLabel", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        const int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end() && !args[1].strVal.empty()) it->second->label = args[1].strVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundPosition", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        const int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) {
                it->second->x = args[1].realVal;
                it->second->y = args[2].realVal;
                it->second->z = args[3].realVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundChannel", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) it->second->channel = args[1].intVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundDistances", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) {
                it->second->minDistance = args[1].realVal;
                it->second->maxDistance = args[2].realVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundDistanceCutoff", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) it->second->distanceCutoff = args[1].realVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("RegisterStackedSound", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !data) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) {
                it->second->stackedByPosition = args[1].boolVal;
                it->second->stackedWidth = args[2].realVal;
                it->second->stackedHeight = args[3].realVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("AttachSoundToUnit", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        int unitId = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) it->second->attachedUnitSimId = unitId;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("StartSound", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int sndSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (sndSim < 0) return JassValue::Null();
        auto sit = data->sounds.find(sndSim);
        if (sit == data->sounds.end()) return JassValue::Null();
        sit->second->playing = true;
        auto* audio = sim ? sim->GetAudioSystem() : nullptr;
        if (audio && !sit->second->label.empty()) {
            audio->Play2D(sit->second->label, sit->second->volume);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("StopSound", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int sndSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (sndSim >= 0) {
            auto sit = data->sounds.find(sndSim);
            if (sit != data->sounds.end()) {
                sit->second->playing = false;
                if (args[1].boolVal) sit->second->killWhenDone = true;
            }
        }
        (void)sim;
        return JassValue::Null();
    });
    vm_.RegisterNative("DestroySound", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) { data->sounds.erase(id); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetSoundDuration", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(0);
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) return JassValue::Int(it->second->durationMs);
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("PlaySound", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int sndSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (sndSim < 0) return JassValue::Null();
        auto sit = data->sounds.find(sndSim);
        if (sit == data->sounds.end() || !sim) return JassValue::Null();
        sit->second->playing = true;
        auto* audio = sim->GetAudioSystem();
        if (audio && !sit->second->label.empty())
            audio->Play2D(sit->second->label, sit->second->volume);
        return JassValue::Null();
    });
    vm_.RegisterNative("PlaySoundAtPoint", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int sndSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (sndSim < 0) return JassValue::Null();
        auto sit = data->sounds.find(sndSim);
        if (sit == data->sounds.end() || !sim) return JassValue::Null();
        auto* audio = sim->GetAudioSystem();
        if (audio && !sit->second->label.empty())
            audio->Play3D(sit->second->label, args[1].realVal, args[2].realVal, 0, sit->second->volume);
        return JassValue::Null();
    });
    vm_.RegisterNative("VolumeGroupReset", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!data) return JassValue::Null();
        (void)args;
        data->music.volume = 127;
        return JassValue::Null();
    });
}

// ============================================================
// 20. TextTag natives
// ============================================================
void JassNatives::RegisterTextTagNatives() {
    vm_.RegisterNative("CreateTextTag", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto tag = std::make_shared<JassTextTag>();
        int id = data->nextTextTagId++;
        data->textTags[id] = tag;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::TEXT_TAG, id));
    });
    vm_.RegisterNative("DestroyTextTag", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { data->textTags.erase(id); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagText", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { auto it = data->textTags.find(id); if (it != data->textTags.end()) { it->second->text = args[1].strVal; it->second->height = args[2].realVal; } }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagPos", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { auto it = data->textTags.find(id); if (it != data->textTags.end()) { it->second->x = args[1].realVal; it->second->y = args[2].realVal; } }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagPosUnit", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagColor", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) {
            auto it = data->textTags.find(id);
            if (it != data->textTags.end()) {
                int a = args[1].intVal, r = args[2].intVal, g = args[3].intVal, b = args[4].intVal;
                it->second->color = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagVisibility", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { auto it = data->textTags.find(id); if (it != data->textTags.end()) it->second->visible = args[1].boolVal; }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagSuspended", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { auto it = data->textTags.find(id); if (it != data->textTags.end()) it->second->suspended = args[1].boolVal; }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagPermanent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { auto it = data->textTags.find(id); if (it != data->textTags.end()) it->second->permanent = args[1].boolVal; }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagAge", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagLifespan", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { auto it = data->textTags.find(id); if (it != data->textTags.end()) it->second->lifespan = args[1].realVal; }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagFadepoint", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { auto it = data->textTags.find(id); if (it != data->textTags.end()) it->second->fadepoint = args[1].realVal; }
        return JassValue::Null();
    });
    vm_.RegisterNative("CreateFloatingTextFromString", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });
}

// ============================================================
// 21. Special Effect natives
// ============================================================
void JassNatives::RegisterEffectNatives() {
    vm_.RegisterNative("AddSpecialEffect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Int(0);
        auto e = std::make_shared<JassEffect>();
        e->modelPath = args[0].strVal;
        e->x = args[1].realVal;
        e->y = args[2].realVal;
        int id = data->nextEffectId++;
        data->effects[id] = e;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::EFFECT, id));
    });
    vm_.RegisterNative("AddSpecialEffectLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Int(0);
        auto e = std::make_shared<JassEffect>();
        e->modelPath = args[0].strVal;
        int id = data->nextEffectId++;
        data->effects[id] = e;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::EFFECT, id));
    });
    vm_.RegisterNative("AddSpecialEffectTarget", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Int(0);
        auto e = std::make_shared<JassEffect>();
        e->modelPath = args[0].strVal;
        int id = data->nextEffectId++;
        data->effects[id] = e;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::EFFECT, id));
    });
    vm_.RegisterNative("DestroyEffect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::EFFECT);
        if (id >= 0) { data->effects.erase(id); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("AddWeatherEffect", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("RemoveWeatherEffect", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("EnableWeatherEffect", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("TerrainDeformCrater", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("TerrainDeformRegistration", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
}

// ============================================================
// 22. Fog natives
// ============================================================
void JassNatives::RegisterFogNatives() {
    vm_.RegisterNative("CreateFogModifierRect", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->AllocHandle(JassHandleInfo::FOG_MODIFIER, data->nextHandleId++));
    });
    vm_.RegisterNative("CreateFogModifierRadius", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->AllocHandle(JassHandleInfo::FOG_MODIFIER, data->nextHandleId++));
    });
    vm_.RegisterNative("CreateFogModifierRadiusLoc", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->AllocHandle(JassHandleInfo::FOG_MODIFIER, data->nextHandleId++));
    });
    vm_.RegisterNative("DestroyFogModifier", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::FOG_MODIFIER);
        if (id >= 0) data->FreeHandle(args[0].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("FogModifierStart", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("FogModifierStop", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetTerrainFog", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("ResetTerrainFog", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetFogStateRect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        // Recorded for renderer consumption; full per-cell fog comes later.
        (void)args; (void)data;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetFogStateRadius", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        (void)args; (void)data;
        return JassValue::Null();
    });
    vm_.RegisterNative("IsLocationVisibleToPlayer", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(true); });
    vm_.RegisterNative("IsLocationMaskedToPlayer", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("FogMaskEnable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->fog.maskEnabled = args[0].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("FogEnable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->fog.fogEnabled = args[0].boolVal;
        return JassValue::Null();
    });
}

// ============================================================
// 23. Weather & Lightning natives
// ============================================================
void JassNatives::RegisterWeatherNatives() {
    vm_.RegisterNative("AddWeatherEffect", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->AllocHandle(JassHandleInfo::WEATHER, data->nextHandleId++));
    });
    vm_.RegisterNative("RemoveWeatherEffect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::WEATHER);
        if (id >= 0) data->FreeHandle(args[0].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("EnableWeatherEffect", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
}

void JassNatives::RegisterLightningNatives() {
    vm_.RegisterNative("AddLightning", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        auto bolt = std::make_shared<JassNativesData::JassLightning>();
        if (args.size() >= 6) {
            bolt->code = args[0].strVal;
            bolt->checkVisibility = args[1].boolVal;
            bolt->x1 = args[2].realVal; bolt->y1 = args[3].realVal;
            bolt->x2 = args[4].realVal; bolt->y2 = args[5].realVal;
        }
        const int id = data->nextHandleId++;
        data->lightnings[id] = bolt;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LIGHTNING, id));
    });
    vm_.RegisterNative("AddLightningEx", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        auto bolt = std::make_shared<JassNativesData::JassLightning>();
        if (args.size() >= 8) {
            bolt->code = args[0].strVal;
            bolt->checkVisibility = args[1].boolVal;
            bolt->x1 = args[2].realVal; bolt->y1 = args[3].realVal; bolt->z1 = args[4].realVal;
            bolt->x2 = args[5].realVal; bolt->y2 = args[6].realVal; bolt->z2 = args[7].realVal;
        }
        const int id = data->nextHandleId++;
        data->lightnings[id] = bolt;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LIGHTNING, id));
    });
    vm_.RegisterNative("DestroyLightning", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::LIGHTNING);
        if (id >= 0) {
            data->lightnings.erase(id);
            data->FreeHandle(args[0].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MoveLightning", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6) return JassValue::Bool(false);
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::LIGHTNING);
        if (id < 0) return JassValue::Bool(false);
        auto it = data->lightnings.find(id);
        if (it == data->lightnings.end()) return JassValue::Bool(false);
        it->second->checkVisibility = args[1].boolVal;
        it->second->x1 = args[2].realVal; it->second->y1 = args[3].realVal;
        it->second->x2 = args[4].realVal; it->second->y2 = args[5].realVal;
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("MoveLightningEx", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 8) return JassValue::Bool(false);
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::LIGHTNING);
        if (id < 0) return JassValue::Bool(false);
        auto it = data->lightnings.find(id);
        if (it == data->lightnings.end()) return JassValue::Bool(false);
        it->second->checkVisibility = args[1].boolVal;
        it->second->x1 = args[2].realVal; it->second->y1 = args[3].realVal; it->second->z1 = args[4].realVal;
        it->second->x2 = args[5].realVal; it->second->y2 = args[6].realVal; it->second->z2 = args[7].realVal;
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("GetLightningColor", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(1.0f); });
    auto lightningColor = [data = data_](const std::vector<JassValue>& args, float JassNativesData::JassLightning::*field) -> JassValue {
        if (args.empty()) return JassValue::Real(1.0f);
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::LIGHTNING);
        if (id < 0) return JassValue::Real(1.0f);
        auto it = data->lightnings.find(id);
        if (it == data->lightnings.end()) return JassValue::Real(1.0f);
        return JassValue::Real(it->second.get()->*field);
    };
    vm_.RegisterNative("GetLightningColorA", [lightningColor](const std::vector<JassValue>& args) -> JassValue {
        return lightningColor(args, &JassNativesData::JassLightning::a);
    });
    vm_.RegisterNative("GetLightningColorR", [lightningColor](const std::vector<JassValue>& args) -> JassValue {
        return lightningColor(args, &JassNativesData::JassLightning::r);
    });
    vm_.RegisterNative("GetLightningColorG", [lightningColor](const std::vector<JassValue>& args) -> JassValue {
        return lightningColor(args, &JassNativesData::JassLightning::g);
    });
    vm_.RegisterNative("GetLightningColorB", [lightningColor](const std::vector<JassValue>& args) -> JassValue {
        return lightningColor(args, &JassNativesData::JassLightning::b);
    });
    vm_.RegisterNative("SetLightningColor", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Bool(false);
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::LIGHTNING);
        if (id < 0) return JassValue::Bool(false);
        auto it = data->lightnings.find(id);
        if (it == data->lightnings.end()) return JassValue::Bool(false);
        it->second->r = args[1].realVal;
        it->second->g = args[2].realVal;
        it->second->b = args[3].realVal;
        it->second->a = args[4].realVal;
        return JassValue::Bool(true);
    });
}

// ============================================================
// 24. Camera natives
// ============================================================
void JassNatives::RegisterCameraNatives() {
    vm_.RegisterNative("CreateCameraSetup", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (!data) return JassValue::Int(0);
        int id = data->nextCameraSetupId++;
        data->cameraSetups[id] = std::make_shared<JassNativesData::JassCameraSetup>();
        return JassValue::Int(data->AllocHandle(JassHandleInfo::CAMERA_SETUP, id));
    });
    vm_.RegisterNative("CameraSetupSetField", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::CAMERA_SETUP);
        if (simId >= 0) {
            auto it = data->cameraSetups.find(simId);
            if (it != data->cameraSetups.end()) it->second->fields[args[1].intVal] = args[2].realVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetupGetField", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::CAMERA_SETUP);
        if (simId >= 0) {
            auto it = data->cameraSetups.find(simId);
            if (it != data->cameraSetups.end()) {
                auto fit = it->second->fields.find(args[1].intVal);
                if (fit != it->second->fields.end()) return JassValue::Real(fit->second);
            }
        }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("CameraSetupSetDestPosition", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::CAMERA_SETUP);
        if (simId >= 0) {
            auto it = data->cameraSetups.find(simId);
            if (it != data->cameraSetups.end()) {
                it->second->destX = args[1].realVal;
                it->second->destY = args[2].realVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetupGetDestPositionLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::CAMERA_SETUP);
        if (simId < 0) return JassValue::Int(0);
        auto it = data->cameraSetups.find(simId);
        if (it == data->cameraSetups.end()) return JassValue::Int(0);
        auto loc = std::make_shared<JassLocation>();
        loc->x = it->second->destX;
        loc->y = it->second->destY;
        int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    });
    vm_.RegisterNative("CameraSetupGetDestPositionX", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::CAMERA_SETUP);
        if (simId >= 0) {
            auto it = data->cameraSetups.find(simId);
            if (it != data->cameraSetups.end()) return JassValue::Real(it->second->destX);
        }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("CameraSetupGetDestPositionY", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::CAMERA_SETUP);
        if (simId >= 0) {
            auto it = data->cameraSetups.find(simId);
            if (it != data->cameraSetups.end()) return JassValue::Real(it->second->destY);
        }
        return JassValue::Real(0);
    });
    // Apply routes the stored destination through the engine camera
    // callbacks (ref CameraManager.applyCameraSetup). ForPlayer variants
    // only apply for the local player, matching the reference.
    auto applyCameraSetup = [data = data_](int setupHandle, float zOffset) {
        if (!data) return;
        int simId = data->GetSimHandle(setupHandle, JassHandleInfo::CAMERA_SETUP);
        if (simId < 0) return;
        auto it = data->cameraSetups.find(simId);
        if (it == data->cameraSetups.end()) return;
        if (data->camSetTarget) data->camSetTarget(it->second->destX, zOffset, it->second->destY);
    };
    vm_.RegisterNative("CameraSetupApply", [data = data_, applyCameraSetup](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 1) return JassValue::Null();
        applyCameraSetup(args[0].intVal, 0.0f);
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetupApplyWithZ", [data = data_, applyCameraSetup](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        applyCameraSetup(args[0].intVal, args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetupApplyForceDuration", [data = data_, applyCameraSetup](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        applyCameraSetup(args[0].intVal, 0.0f);
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetupApplyForceDurationWithZ", [data = data_, applyCameraSetup](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        const float zOffset = args.size() >= 4 ? args[3].realVal : 0.0f;
        applyCameraSetup(args[0].intVal, zOffset);
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetupApplyForPlayer", [data = data_, applyCameraSetup](const std::vector<JassValue>& args) -> JassValue {
        // (doPan, setup, player, duration) per common.j.
        if (args.size() < 3 || !data) return JassValue::Null();
        if (ResolvePlayerId(data, args[2].intVal) == data->localPlayerIndex) {
            applyCameraSetup(args[1].intVal, 0.0f);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetupApplyForPlayerWithZ", [data = data_, applyCameraSetup](const std::vector<JassValue>& args) -> JassValue {
        // (doPan, setup, player, duration, zOffset) per common.j.
        if (args.size() < 4 || !data) return JassValue::Null();
        if (ResolvePlayerId(data, args[2].intVal) == data->localPlayerIndex) {
            applyCameraSetup(args[1].intVal, args.size() >= 5 ? args[4].realVal : 0.0f);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCameraPosition", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 3 && data->camSetPosition)
            data->camSetPosition(args[0].realVal, args[1].realVal, args[2].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCameraLookAt", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 3 && data->camSetTarget)
            data->camSetTarget(args[0].realVal, args[1].realVal, args[2].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCameraBounds", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        // Ref: 8 corner coords collapse to a min/max rect.
        if (args.size() < 8 || !data) return JassValue::Null();
        float minX = args[0].realVal, maxX = args[0].realVal;
        float minY = args[1].realVal, maxY = args[1].realVal;
        for (size_t i = 1; i < 4; ++i) {
            const float x = args[i * 2].realVal;
            const float y = args[i * 2 + 1].realVal;
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minY = std::min(minY, y); maxY = std::max(maxY, y);
        }
        data->cameraBounds.minX = minX;
        data->cameraBounds.minY = minY;
        data->cameraBounds.maxX = maxX;
        data->cameraBounds.maxY = maxY;
        return JassValue::Null();
    });
    vm_.RegisterNative("PanCameraTo", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 2 && data->camSetTarget)
            data->camSetTarget(args[0].realVal, 0, args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("PanCameraToTimed", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 3 && data->camSetTarget)
            data->camSetTarget(args[0].realVal, 0, args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("GetCameraTargetPositionX", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetX ? data->camGetX() : 0);
    });
    vm_.RegisterNative("GetCameraTargetPositionY", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetY ? data->camGetY() : 0);
    });
    vm_.RegisterNative("GetCameraTargetPositionZ", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetZ ? data->camGetZ() : 0);
    });
    vm_.RegisterNative("GetCameraTargetPositionLoc", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto loc = std::make_shared<JassLocation>();
        loc->x = data->camGetX ? data->camGetX() : 0.0f;
        loc->y = data->camGetY ? data->camGetY() : 0.0f;
        const int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    });
    vm_.RegisterNative("GetCameraEyePositionX", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetEyeX ? data->camGetEyeX() : 0);
    });
    vm_.RegisterNative("GetCameraEyePositionY", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetEyeY ? data->camGetEyeY() : 0);
    });
    vm_.RegisterNative("GetCameraEyePositionZ", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetEyeZ ? data->camGetEyeZ() : 0);
    });
    vm_.RegisterNative("GetCameraEyePositionLoc", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto loc = std::make_shared<JassLocation>();
        loc->x = data->camGetEyeX ? data->camGetEyeX() : 0.0f;
        loc->y = data->camGetEyeY ? data->camGetEyeY() : 0.0f;
        const int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    });
    vm_.RegisterNative("GetCameraBoundMinX", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data ? data->cameraBounds.minX : 0);
    });
    vm_.RegisterNative("GetCameraBoundMinY", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data ? data->cameraBounds.minY : 0);
    });
    vm_.RegisterNative("GetCameraBoundMaxX", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data ? data->cameraBounds.maxX : 0);
    });
    vm_.RegisterNative("GetCameraBoundMaxY", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data ? data->cameraBounds.maxY : 0);
    });
    vm_.RegisterNative("GetCameraMargin", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data ? data->cameraBounds.margin : 0);
    });
    vm_.RegisterNative("GetCameraField", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetField ? data->camGetField() : 0);
    });
    vm_.RegisterNative("SetCameraField", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 2 && data->camSetField)
            data->camSetField(args[0].intVal, args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("AdjustCameraField", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data || !data->camSetField) return JassValue::Null();
        const float current = data->camGetField ? data->camGetField() : 0.0f;
        data->camSetField(args[0].intVal, current + args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("ResetToGameCamera", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (data->camReset) data->camReset();
        return JassValue::Null();
    });
    vm_.RegisterNative("StopCamera", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (data) data->cameraStopped = true;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCinematicCamera", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->cinematicCameraModel = args[0].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSkyModel", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->sky.model = args[0].strVal;
        return JassValue::Null();
    });
}

// ============================================================
// 25. Game natives
// ============================================================
void JassNatives::RegisterGameNatives() {
    vm_.RegisterNative("BJDebugMsg", [](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty()) std::cout << "[JASS] " << args[0].strVal << std::endl;
        return JassValue::Null();
    });
    vm_.RegisterNative("GetGameSpeed", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data ? data->gameConfig.gameSpeed : 2);
    });
    vm_.RegisterNative("GetGameTypeSelected", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("VersionGet", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(1);
    });
    vm_.RegisterNative("IsFogEnabled", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(data ? data->fog.fogEnabled : true);
    });
    vm_.RegisterNative("IsFogMaskEnabled", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(data ? data->fog.maskEnabled : true);
    });
    vm_.RegisterNative("DisplayTextToPlayer", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 4) std::cout << "[JASS->P" << args[0].intVal << "] " << args[3].strVal << std::endl;
        return JassValue::Null();
    });
    vm_.RegisterNative("DisplayTimedTextToPlayer", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 4) std::cout << "[JASS->P" << args[0].intVal << "] " << args[3].strVal << std::endl;
        return JassValue::Null();
    });
    vm_.RegisterNative("DisplayTimedTextFromPlayer", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("ClearTextMessages", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("ClearSelection", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("EnableUserUI", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetReservedLocalHeroButtons", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetAltMinimapIcon", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("StartSound", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int sndSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (sndSim < 0) return JassValue::Null();
        auto sit = data->sounds.find(sndSim);
        if (sit == data->sounds.end() || !sim) return JassValue::Null();
        sit->second->playing = true;
        auto* audio = sim->GetAudioSystem();
        if (audio && !sit->second->label.empty())
            audio->Play2D(sit->second->label, sit->second->volume);
        return JassValue::Null();
    });
    vm_.RegisterNative("StopSound", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        int sndSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (sndSim >= 0) {
            auto sit = data->sounds.find(sndSim);
            if (sit != data->sounds.end()) {
                sit->second->playing = false;
                if (args[1].boolVal) sit->second->killWhenDone = true;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("AttachSoundToUnit", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        int unitId = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) it->second->attachedUnitSimId = unitId;
        }
        return JassValue::Null();
    });
    // Cinematic
    vm_.RegisterNative("SetCinematicMode", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->cinematicMode = args[0].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetDayNightModels", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        data->dayNightTerrainModel = args[0].strVal;
        data->dayNightUnitModel = args[1].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSkyModel", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->sky.model = args[0].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("PlayModelCinematic", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetCampaignMenuRace", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetCampaignMenuRaceEx", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetCineFilterTexture", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->cineFilter.texture = args[0].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCineFilterBlendMode", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->cineFilter.blendMode = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCineFilterTexMapFlags", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->cineFilter.texMapFlags = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCineFilterStartUV", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !data) return JassValue::Null();
        for (int i = 0; i < 4; ++i) data->cineFilter.startUV[i] = args[i].realVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCineFilterEndUV", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !data) return JassValue::Null();
        for (int i = 0; i < 4; ++i) data->cineFilter.endUV[i] = args[i].realVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCineFilterStartColor", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !data) return JassValue::Null();
        for (int i = 0; i < 4; ++i) data->cineFilter.startColor[i] = args[i].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCineFilterEndColor", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !data) return JassValue::Null();
        for (int i = 0; i < 4; ++i) data->cineFilter.endColor[i] = args[i].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCineFilterDuration", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->cineFilter.duration = args[0].realVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("DisplayCineFilter", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->cineFilter.displayed = args[0].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("NewSoundEnvironment", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->music.soundEnvironment = args[0].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetSmoothingFactor", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->cameraSmoothingFactor = args[0].realVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetDoodadAnimationRect", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SuspendHeroXP", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && sim) sim->SetHeroXPSuspended(args[0].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SuspendTimeOfDay", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        // Ref: simulation.setTimeOfDaySuspended(flag).
        if (!args.empty() && sim) sim->SetTimeOfDaySuspended(args[0].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitSuspendDecay", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetDecaySuspended(args[1].boolVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPlayerOnScoreScreen", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    auto getQuest = [data = data_](int handle) -> std::shared_ptr<JassQuest> {
        if (!data) return nullptr;
        const int id = data->GetSimHandle(handle, JassHandleInfo::QUEST);
        if (id < 0) return nullptr;
        auto it = data->quests.find(id);
        return it != data->quests.end() ? it->second : nullptr;
    };
    auto getQuestItem = [data = data_](int handle) -> std::shared_ptr<JassQuestItem> {
        if (!data) return nullptr;
        const int id = data->GetSimHandle(handle, JassHandleInfo::QUEST_ITEM);
        if (id < 0) return nullptr;
        auto it = data->questItems.find(id);
        return it != data->questItems.end() ? it->second : nullptr;
    };
    vm_.RegisterNative("CreateQuest", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (!data) return JassValue::Int(0);
        const int id = data->nextQuestId++;
        data->quests[id] = std::make_shared<JassQuest>();
        return JassValue::Int(data->AllocHandle(JassHandleInfo::QUEST, id));
    });
    vm_.RegisterNative("DestroyQuest", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!data || args.empty()) return JassValue::Null();
        const int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::QUEST);
        if (id >= 0) {
            auto questIt = data->quests.find(id);
            if (questIt != data->quests.end() && questIt->second) {
                std::unordered_set<int> itemIds(questIt->second->itemIds.begin(), questIt->second->itemIds.end());
                for (int itemId : itemIds) {
                    data->questItems.erase(itemId);
                }
                std::vector<int> itemHandles;
                for (const auto& [handle, info] : data->handles) {
                    if (info.type == JassHandleInfo::QUEST_ITEM && itemIds.count(info.id) > 0) {
                        itemHandles.push_back(handle);
                    }
                }
                for (int itemHandle : itemHandles) {
                    data->FreeHandle(itemHandle);
                }
            }
            data->quests.erase(id);
            data->FreeHandle(args[0].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsQuestRequired", [getQuest](const std::vector<JassValue>& args) -> JassValue {
        auto quest = args.empty() ? nullptr : getQuest(args[0].intVal);
        return JassValue::Bool(quest ? quest->required : false);
    });
    vm_.RegisterNative("IsQuestDiscovered", [getQuest](const std::vector<JassValue>& args) -> JassValue {
        auto quest = args.empty() ? nullptr : getQuest(args[0].intVal);
        return JassValue::Bool(quest ? quest->discovered : false);
    });
    vm_.RegisterNative("IsQuestCompleted", [getQuest](const std::vector<JassValue>& args) -> JassValue {
        auto quest = args.empty() ? nullptr : getQuest(args[0].intVal);
        return JassValue::Bool(quest ? quest->completed : false);
    });
    vm_.RegisterNative("IsQuestFailed", [getQuest](const std::vector<JassValue>& args) -> JassValue {
        auto quest = args.empty() ? nullptr : getQuest(args[0].intVal);
        return JassValue::Bool(quest ? quest->failed : false);
    });
    vm_.RegisterNative("IsQuestEnabled", [getQuest](const std::vector<JassValue>& args) -> JassValue {
        auto quest = args.empty() ? nullptr : getQuest(args[0].intVal);
        return JassValue::Bool(quest ? quest->enabled : false);
    });
    vm_.RegisterNative("IsQuestItemCompleted", [getQuestItem](const std::vector<JassValue>& args) -> JassValue {
        auto item = args.empty() ? nullptr : getQuestItem(args[0].intVal);
        return JassValue::Bool(item ? item->completed : false);
    });
    vm_.RegisterNative("QuestSetTitle", [getQuest](const std::vector<JassValue>& args) -> JassValue {
        auto quest = args.empty() ? nullptr : getQuest(args[0].intVal);
        if (quest && args.size() >= 2) quest->title = args[1].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("QuestSetDescription", [getQuest](const std::vector<JassValue>& args) -> JassValue {
        auto quest = args.empty() ? nullptr : getQuest(args[0].intVal);
        if (quest && args.size() >= 2) quest->description = args[1].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("QuestSetIconPath", [getQuest](const std::vector<JassValue>& args) -> JassValue {
        auto quest = args.empty() ? nullptr : getQuest(args[0].intVal);
        if (quest && args.size() >= 2) quest->iconPath = args[1].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("QuestSetRequired", [getQuest](const std::vector<JassValue>& args) -> JassValue {
        auto quest = args.empty() ? nullptr : getQuest(args[0].intVal);
        if (quest && args.size() >= 2) quest->required = JassValueToBool(args[1]);
        return JassValue::Null();
    });
    vm_.RegisterNative("QuestSetDiscovered", [getQuest](const std::vector<JassValue>& args) -> JassValue {
        auto quest = args.empty() ? nullptr : getQuest(args[0].intVal);
        if (quest && args.size() >= 2) quest->discovered = JassValueToBool(args[1]);
        return JassValue::Null();
    });
    vm_.RegisterNative("QuestSetCompleted", [getQuest](const std::vector<JassValue>& args) -> JassValue {
        auto quest = args.empty() ? nullptr : getQuest(args[0].intVal);
        if (quest && args.size() >= 2) quest->completed = JassValueToBool(args[1]);
        return JassValue::Null();
    });
    vm_.RegisterNative("QuestSetFailed", [getQuest](const std::vector<JassValue>& args) -> JassValue {
        auto quest = args.empty() ? nullptr : getQuest(args[0].intVal);
        if (quest && args.size() >= 2) quest->failed = JassValueToBool(args[1]);
        return JassValue::Null();
    });
    vm_.RegisterNative("QuestSetEnabled", [getQuest](const std::vector<JassValue>& args) -> JassValue {
        auto quest = args.empty() ? nullptr : getQuest(args[0].intVal);
        if (quest && args.size() >= 2) quest->enabled = JassValueToBool(args[1]);
        return JassValue::Null();
    });
    vm_.RegisterNative("QuestCreateItem", [data = data_, getQuest](const std::vector<JassValue>& args) -> JassValue {
        if (!data || args.empty()) return JassValue::Int(0);
        auto quest = getQuest(args[0].intVal);
        if (!quest) return JassValue::Int(0);
        const int id = data->nextQuestItemId++;
        data->questItems[id] = std::make_shared<JassQuestItem>();
        quest->itemIds.push_back(id);
        return JassValue::Int(data->AllocHandle(JassHandleInfo::QUEST_ITEM, id));
    });
    vm_.RegisterNative("QuestItemSetDescription", [getQuestItem](const std::vector<JassValue>& args) -> JassValue {
        auto item = args.empty() ? nullptr : getQuestItem(args[0].intVal);
        if (item && args.size() >= 2) item->description = args[1].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("QuestItemSetCompleted", [getQuestItem](const std::vector<JassValue>& args) -> JassValue {
        auto item = args.empty() ? nullptr : getQuestItem(args[0].intVal);
        if (item && args.size() >= 2) item->completed = JassValueToBool(args[1]);
        return JassValue::Null();
    });
    vm_.RegisterNative("FlashQuestDialogButton", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("ForceQuestDialogUpdate", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetTimeOfDayScale", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && sim) sim->SetTimeOfDayScale(args[0].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("GetTimeOfDayScale", [sim = sim_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(sim ? sim->GetTimeOfDayScale() : 1.0f);
    });
    vm_.RegisterNative("SetTimeOfDay", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && sim) sim->SetGameTimeOfDay(args[0].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("GetTimeOfDay", [sim = sim_](const std::vector<JassValue>&) -> JassValue {
        if (sim) return JassValue::Real(sim->GetGameTimeOfDay());
        return JassValue::Real(0);
    });
    vm_.RegisterNative("IsDayTime", [sim = sim_](const std::vector<JassValue>&) -> JassValue {
        if (sim) return JassValue::Bool(sim->IsDaytime());
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("IsNightTime", [sim = sim_](const std::vector<JassValue>&) -> JassValue {
        if (sim) return JassValue::Bool(sim->IsNighttime());
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("GetGameDay", [sim = sim_](const std::vector<JassValue>&) -> JassValue {
        if (sim) return JassValue::Int((int)(sim->GetGameTimeOfDay() / 24.0f / 60.0f));
        return JassValue::Int(0);
    });
    // Map config
    vm_.RegisterNative("SetMapName", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetMapDescription", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetTeams", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->gameConfig.teams = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPlayers", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (data && !args.empty()) {
            data->configuredPlayerCount = args[0].intVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetGamePlacement", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->gameConfig.placement = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("DefineStartLocation", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (data && args.size() >= 3) {
            const int index = args[0].intVal;
            if (index >= 0 && index < (int)data->startLocations.size()) {
                data->startLocations[index].x = args[1].realVal;
                data->startLocations[index].y = args[2].realVal;
                data->startLocations[index].defined = true;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("DefineStartLocationLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (data && args.size() >= 2) {
            const int index = args[0].intVal;
            const int locId = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
            if (index >= 0 && index < (int)data->startLocations.size()) {
                auto locIt = data->locations.find(locId);
                if (locIt != data->locations.end() && locIt->second) {
                    data->startLocations[index].x = locIt->second->x;
                    data->startLocations[index].y = locIt->second->y;
                    data->startLocations[index].defined = true;
                }
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPlayerSlotAvailable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (data && args.size() >= 2) {
            const int playerId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PLAYER);
            if (playerId >= 0 && playerId < (int)data->playerMapConfig.size()) {
                data->playerMapConfig[playerId].slotActive = (args[1].intVal != 0);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPlayerColor", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
                p->SetColor(args[1].intVal);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPlayerRace", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
                p->SetRace(args[1].intVal);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPlayerHandicap", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
                p->SetHandicap(args[1].realVal);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPlayerHandicapXP", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
                p->SetHandicapXP(args[1].realVal);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPlayerTeam", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        const int p = ResolvePlayerId(data, args[0].intVal);
        if (p >= 0 && p < 24) data->playerMapConfig[p].team = args[1].intVal;
        if (sim) {
            if (CPlayer* player = sim->GetPlayer(p)) player->SetTeam(args[1].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPlayerAlliance", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetPlayerStartLocation", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (data && args.size() >= 2) {
            const int playerId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PLAYER);
            const int startIndex = args[1].intVal;
            if (playerId >= 0 && playerId < (int)data->playerMapConfig.size()) {
                data->playerMapConfig[playerId].startLocationIndex = startIndex;
                data->playerMapConfig[playerId].slotActive = true;
                if (sim && startIndex >= 0 && startIndex < (int)data->startLocations.size()) {
                    const auto& start = data->startLocations[startIndex];
                    if (start.defined) {
                        if (auto* player = sim->GetPlayer(playerId)) {
                            player->SetStartLocation(start.x, start.y);
                        }
                    }
                }
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ForcePlayerStartLocation", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (data && args.size() >= 2) {
            const int playerId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PLAYER);
            const int startIndex = args[1].intVal;
            if (playerId >= 0 && playerId < (int)data->playerMapConfig.size()) {
                data->playerMapConfig[playerId].startLocationIndex = startIndex;
                data->playerMapConfig[playerId].slotActive = true;
                if (sim && startIndex >= 0 && startIndex < (int)data->startLocations.size()) {
                    const auto& start = data->startLocations[startIndex];
                    if (start.defined) {
                        if (auto* player = sim->GetPlayer(playerId)) {
                            player->SetStartLocation(start.x, start.y);
                        }
                    }
                }
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetStartLocationX", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0.0f);
        const int index = args[0].intVal;
        if (data && index >= 0 && index < (int)data->startLocations.size() && data->startLocations[index].defined) {
            return JassValue::Real(data->startLocations[index].x);
        }
        return JassValue::Real(0.0f);
    });
    vm_.RegisterNative("GetStartLocationY", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0.0f);
        const int index = args[0].intVal;
        if (data && index >= 0 && index < (int)data->startLocations.size() && data->startLocations[index].defined) {
            return JassValue::Real(data->startLocations[index].y);
        }
        return JassValue::Real(0.0f);
    });
    vm_.RegisterNative("GetStartLocationLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        auto loc = std::make_shared<JassLocation>();
        if (!args.empty() && data) {
            const int index = args[0].intVal;
            if (index >= 0 && index < (int)data->startLocations.size() && data->startLocations[index].defined) {
                loc->x = data->startLocations[index].x;
                loc->y = data->startLocations[index].y;
            }
        }
        int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    });
    vm_.RegisterNative("GetResourceAmount", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data || !sim) return JassValue::Int(0);
        const int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        if (simId >= 0) {
            auto* destructable = sim->GetDestructable(simId);
            return JassValue::Int(destructable ? (int)destructable->GetLife() : 0);
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("SetResourceAmount", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data || !sim) return JassValue::Null();
        const int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        if (simId >= 0) {
            if (auto* destructable = sim->GetDestructable(simId)) {
                destructable->SetLife(*sim, static_cast<float>(std::max(0, args[1].intVal)));
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetGameDifficulty", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->gameConfig.difficulty = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("GetGameDifficulty", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data ? data->gameConfig.difficulty : 1);
    });
    vm_.RegisterNative("GetDefaultDifficulty", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data ? data->gameConfig.defaultDifficulty : 1);
    });
    vm_.RegisterNative("SetGameType", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->gameConfig.gameType = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetMapFlags", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->gameConfig.mapFlags = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("IsMapFlagSet", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Bool(false);
        return JassValue::Bool((data->gameConfig.mapFlags & args[0].intVal) != 0);
    });
    vm_.RegisterNative("GetIntegerGameState", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        const int id = args[0].intVal;
        // Ref GetIntegerGameState: DIVINE_INTERVENTION(0) and
        // DISCONNECTED(1) both read 0 (TODO in ref).
        if (id == 0 || id == 1) return JassValue::Int(0);
        (void)sim;
        if (data) {
            auto it = data->intGameStates.find(id);
            if (it != data->intGameStates.end()) return JassValue::Int(it->second);
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetFloatGameState", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0.0f);
        // Ref: TIME_OF_DAY(2) reads the simulation clock.
        if (args[0].intVal == 2 && sim) return JassValue::Real(sim->GetGameTimeOfDay());
        return JassValue::Real(0.0f);
    });
    vm_.RegisterNative("SetIntegerGameState", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        const int id = args[0].intVal;
        if (id == 0 || id == 1) return JassValue::Null();
        data->intGameStates[id] = args[1].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetFloatGameState", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        // Ref: TIME_OF_DAY(2) writes the simulation clock.
        if (args[0].intVal == 2) sim->SetGameTimeOfDay(args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetGameSpeed", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->gameConfig.gameSpeed = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetAllItemTypeSlots", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetAllUnitTypeSlots", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetItemTypeSlots", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetUnitTypeSlots", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("InitGameCache", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("ReloadGameCachesFromDisk", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(true); });
    vm_.RegisterNative("GetSaveBasicFilename", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Str(""); });
    vm_.RegisterNative("SaveGameExists", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("SetMissionAvailable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        const int index = args[0].intVal;
        if (index >= 0 && index < 12) data->gameConfig.missionAvailable[index] = args[1].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("Preloader", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("CommandAI", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        if (playerId >= 0 && playerId < 24) data->playerAI[playerId].commandCount++;
        return JassValue::Null();
    });
    vm_.RegisterNative("StartCampaignAI", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        if (playerId >= 0 && playerId < 24) data->playerAI[playerId].campaignAI = true;
        return JassValue::Null();
    });
    vm_.RegisterNative("StartMeleeAI", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        if (playerId >= 0 && playerId < 24) data->playerAI[playerId].meleeAI = true;
        return JassValue::Null();
    });
    vm_.RegisterNative("CreateBlightedGoldmine", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("UseCustomHeroAbilities", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetTerrainAvailable", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetTerrainFogEx", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetFogState", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        (void)args; (void)data;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetFogMask", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->fog.maskEnabled = args[0].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetFogMaskStyle", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->fog.maskStyle = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetFogZHeight", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->fog.zHeight = args[0].realVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetWaterDeformStyle", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->water.deformStyle = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetWaterColor", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !data) return JassValue::Null();
        for (int i = 0; i < 4; ++i) data->water.color[i] = args[i].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetWaterBaseColor", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !data) return JassValue::Null();
        for (int i = 0; i < 4; ++i) data->water.baseColor[i] = args[i].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetWaterTransparency", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->water.transparency = args[0].realVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetWaterZ", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->water.z = args[0].realVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSky", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!data) return JassValue::Null();
        (void)args;
        data->sky = JassNativesData::JassSkyState();
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSkyModel", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->sky.model = args[0].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSkyTexture", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->sky.texture = args[0].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSkyColor", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        for (int i = 0; i < 3; ++i) data->sky.color[i] = args[i].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSkyFog", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        data->sky.fogNear = args[0].realVal;
        data->sky.fogFar = args[1].realVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPortrait", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetPortraitText", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetUnitPortrait", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetSoundVolume", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) it->second->volume = args[1].realVal / 127.0f;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetMusicVolume", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->music.volume = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetMapMusic", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        data->music.mapMusic = args[0].strVal;
        data->music.mapMusicRandom = args[1].boolVal;
        data->music.mapMusicIndex = args[2].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("ClearMapMusic", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!data) return JassValue::Null();
        (void)args;
        data->music.mapMusic.clear();
        return JassValue::Null();
    });
    vm_.RegisterNative("PlayMusic", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        data->music.current = args[0].strVal;
        data->music.playing = true;
        data->music.playPositionMs = 0;
        return JassValue::Null();
    });
    vm_.RegisterNative("PlayMusicEx", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        data->music.current = args[0].strVal;
        data->music.playing = true;
        data->music.playPositionMs = args[1].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("StopMusic", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!data) return JassValue::Null();
        (void)args;
        data->music.playing = false;
        return JassValue::Null();
    });
    vm_.RegisterNative("ResumeMusic", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!data) return JassValue::Null();
        (void)args;
        if (!data->music.current.empty()) data->music.playing = true;
        return JassValue::Null();
    });
    vm_.RegisterNative("PlayThematicMusic", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        data->music.current = args[0].strVal;
        data->music.playing = true;
        data->music.playPositionMs = 0;
        return JassValue::Null();
    });
    vm_.RegisterNative("PlayThematicMusicEx", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        data->music.current = args[0].strVal;
        data->music.playing = true;
        data->music.playPositionMs = args[1].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("EndThematicMusic", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        // Ref: stops music, then resumes the map music.
        if (!data) return JassValue::Null();
        (void)args;
        data->music.playing = false;
        if (!data->music.mapMusic.empty()) {
            data->music.current = data->music.mapMusic;
            data->music.playing = true;
            data->music.playPositionMs = 0;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetMusicPlayPosition", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->music.playPositionMs = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetThematicMusicPlayPosition", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->music.playPositionMs = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("GetLocalizedHotkey", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || args[0].strVal.empty()) return JassValue::Int(0);
        return JassValue::Int((unsigned char)args[0].strVal[0]);
    });
    vm_.RegisterNative("DialogCreate", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (!data) return JassValue::Int(0);
        int id = data->nextDialogId++;
        data->dialogs[id] = std::make_shared<JassNativesData::JassDialog>();
        return JassValue::Int(data->AllocHandle(JassHandleInfo::DIALOG, id));
    });
    vm_.RegisterNative("DialogDestroy", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::DIALOG);
        if (simId >= 0) {
            auto dit = data->dialogs.find(simId);
            if (dit != data->dialogs.end()) {
                for (int buttonId : dit->second->buttonIds) data->dialogButtons.erase(buttonId);
                data->dialogs.erase(dit);
            }
            data->FreeHandle(args[0].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("DialogClear", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::DIALOG);
        if (simId >= 0) {
            auto it = data->dialogs.find(simId);
            if (it != data->dialogs.end()) {
                for (int buttonId : it->second->buttonIds) data->dialogButtons.erase(buttonId);
                it->second->buttonIds.clear();
                it->second->message.clear();
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("DialogSetMessage", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::DIALOG);
        if (simId >= 0) {
            auto it = data->dialogs.find(simId);
            if (it != data->dialogs.end()) it->second->message = args[1].strVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("DialogAddButton", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::DIALOG);
        if (simId < 0) return JassValue::Int(0);
        auto dit = data->dialogs.find(simId);
        if (dit == data->dialogs.end()) return JassValue::Int(0);
        int id = data->nextDialogButtonId++;
        auto button = std::make_shared<JassNativesData::JassDialogButton>();
        button->dialogSimId = simId;
        button->text = args[1].strVal;
        button->hotkey = args[2].intVal;
        data->dialogButtons[id] = button;
        dit->second->buttonIds.push_back(id);
        return JassValue::Int(data->AllocHandle(JassHandleInfo::BUTTON, id));
    });
    vm_.RegisterNative("DialogAddQuitButton", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !data) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::DIALOG);
        if (simId < 0) return JassValue::Int(0);
        auto dit = data->dialogs.find(simId);
        if (dit == data->dialogs.end()) return JassValue::Int(0);
        int id = data->nextDialogButtonId++;
        auto button = std::make_shared<JassNativesData::JassDialogButton>();
        button->dialogSimId = simId;
        button->quitButton = true;
        button->showScore = args[1].boolVal;
        button->text = args[2].strVal;
        button->hotkey = args[3].intVal;
        data->dialogButtons[id] = button;
        dit->second->buttonIds.push_back(id);
        return JassValue::Int(data->AllocHandle(JassHandleInfo::BUTTON, id));
    });
    vm_.RegisterNative("DialogDisplay", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[1].intVal, JassHandleInfo::DIALOG);
        if (simId >= 0) {
            auto it = data->dialogs.find(simId);
            if (it != data->dialogs.end()) it->second->displayed = args[2].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("CreateMultiboard", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (!data) return JassValue::Int(0);
        int id = data->nextMultiboardId++;
        data->multiboards[id] = std::make_shared<JassNativesData::JassMultiboard>();
        return JassValue::Int(data->AllocHandle(JassHandleInfo::MULTIBOARD, id));
    });
    vm_.RegisterNative("DestroyMultiboard", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARD);
        if (simId >= 0) {
            auto bit = data->multiboards.find(simId);
            if (bit != data->multiboards.end()) {
                for (const auto& [cell, itemId] : bit->second->items) data->multiboardItems.erase(itemId);
                data->multiboards.erase(bit);
            }
            data->FreeHandle(args[0].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardDisplay", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARD);
        if (simId >= 0) {
            auto it = data->multiboards.find(simId);
            if (it != data->multiboards.end() && !it->second->suppressDisplay) {
                it->second->displayed = args[1].boolVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsMultiboardDisplayed", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARD);
        if (simId >= 0) {
            auto it = data->multiboards.find(simId);
            if (it != data->multiboards.end()) return JassValue::Bool(it->second->displayed);
        }
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("MultiboardMinimize", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARD);
        if (simId >= 0) {
            auto it = data->multiboards.find(simId);
            if (it != data->multiboards.end()) it->second->minimized = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsMultiboardMinimized", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARD);
        if (simId >= 0) {
            auto it = data->multiboards.find(simId);
            if (it != data->multiboards.end()) return JassValue::Bool(it->second->minimized);
        }
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("MultiboardClear", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARD);
        if (simId >= 0) {
            auto it = data->multiboards.find(simId);
            if (it != data->multiboards.end()) {
                for (const auto& [cell, itemId] : it->second->items) data->multiboardItems.erase(itemId);
                it->second->items.clear();
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardSetTitleText", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARD);
        if (simId >= 0) {
            auto it = data->multiboards.find(simId);
            if (it != data->multiboards.end()) it->second->title = args[1].strVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardGetTitleText", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Str("");
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARD);
        if (simId >= 0) {
            auto it = data->multiboards.find(simId);
            if (it != data->multiboards.end()) return JassValue::Str(it->second->title);
        }
        return JassValue::Str("");
    });
    vm_.RegisterNative("MultiboardGetRowCount", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARD);
        if (simId >= 0) {
            auto it = data->multiboards.find(simId);
            if (it != data->multiboards.end()) return JassValue::Int(it->second->rows);
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("MultiboardGetColumnCount", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARD);
        if (simId >= 0) {
            auto it = data->multiboards.find(simId);
            if (it != data->multiboards.end()) return JassValue::Int(it->second->cols);
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("MultiboardSetColumnCount", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARD);
        if (simId >= 0) {
            auto it = data->multiboards.find(simId);
            if (it != data->multiboards.end()) it->second->cols = std::max(0, args[1].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardSetRowCount", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARD);
        if (simId >= 0) {
            auto it = data->multiboards.find(simId);
            if (it != data->multiboards.end()) it->second->rows = std::max(0, args[1].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardGetItem", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARD);
        if (simId < 0) return JassValue::Int(0);
        auto bit = data->multiboards.find(simId);
        if (bit == data->multiboards.end()) return JassValue::Int(0);
        const std::pair<int, int> cell(args[1].intVal, args[2].intVal);
        auto iit = bit->second->items.find(cell);
        if (iit != bit->second->items.end()) {
            return JassValue::Int(data->AllocHandle(JassHandleInfo::MULTIBOARDITEM, iit->second));
        }
        int id = data->nextMultiboardItemId++;
        auto item = std::make_shared<JassNativesData::JassMultiboardItem>();
        item->boardSimId = simId;
        item->row = cell.first;
        item->col = cell.second;
        data->multiboardItems[id] = item;
        bit->second->items[cell] = id;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::MULTIBOARDITEM, id));
    });
    vm_.RegisterNative("MultiboardReleaseItem", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARDITEM);
        if (simId >= 0) data->FreeHandle(args[0].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardSetItemValue", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARDITEM);
        if (simId >= 0) {
            auto it = data->multiboardItems.find(simId);
            if (it != data->multiboardItems.end()) it->second->value = args[1].strVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardGetItemValue", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Str("");
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARDITEM);
        if (simId >= 0) {
            auto it = data->multiboardItems.find(simId);
            if (it != data->multiboardItems.end()) return JassValue::Str(it->second->value);
        }
        return JassValue::Str("");
    });
    vm_.RegisterNative("MultiboardSetItemStyle", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARDITEM);
        if (simId >= 0) {
            auto it = data->multiboardItems.find(simId);
            if (it != data->multiboardItems.end()) {
                it->second->showValue = args[1].boolVal;
                it->second->showIcon = args[2].boolVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardSetItemWidth", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARDITEM);
        if (simId >= 0) {
            auto it = data->multiboardItems.find(simId);
            if (it != data->multiboardItems.end()) it->second->width = args[1].realVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardSetItemIcon", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARDITEM);
        if (simId >= 0) {
            auto it = data->multiboardItems.find(simId);
            if (it != data->multiboardItems.end()) it->second->icon = args[1].strVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardSuppressDisplay", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARD);
        if (simId >= 0) {
            auto it = data->multiboards.find(simId);
            if (it != data->multiboards.end()) it->second->suppressDisplay = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("CreateLeaderboard", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (!data) return JassValue::Int(0);
        int id = data->nextLeaderboardId++;
        data->leaderboards[id] = std::make_shared<JassNativesData::JassLeaderboard>();
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LEADERBOARD, id));
    });
    vm_.RegisterNative("DestroyLeaderboard", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LEADERBOARD);
        if (simId >= 0) {
            data->leaderboards.erase(simId);
            data->FreeHandle(args[0].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("LeaderboardDisplay", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LEADERBOARD);
        if (simId >= 0) {
            auto it = data->leaderboards.find(simId);
            if (it != data->leaderboards.end()) it->second->displayed = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsLeaderboardDisplayed", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LEADERBOARD);
        if (simId >= 0) {
            auto it = data->leaderboards.find(simId);
            if (it != data->leaderboards.end()) return JassValue::Bool(it->second->displayed);
        }
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("LeaderboardGetPlayerIndex", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Int(-1);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LEADERBOARD);
        if (simId < 0) return JassValue::Int(-1);
        auto it = data->leaderboards.find(simId);
        if (it == data->leaderboards.end()) return JassValue::Int(-1);
        const int playerId = ResolvePlayerId(data, args[1].intVal);
        for (size_t i = 0; i < it->second->items.size(); ++i) {
            if (it->second->items[i].playerId == playerId) return JassValue::Int((int)i);
        }
        return JassValue::Int(-1);
    });
    vm_.RegisterNative("LeaderboardGetItemCount", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LEADERBOARD);
        if (simId >= 0) {
            auto it = data->leaderboards.find(simId);
            if (it != data->leaderboards.end()) return JassValue::Int((int)it->second->items.size());
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("LeaderboardAddItem", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LEADERBOARD);
        if (simId < 0) return JassValue::Null();
        auto it = data->leaderboards.find(simId);
        if (it == data->leaderboards.end()) return JassValue::Null();
        JassNativesData::JassLeaderboardItem item;
        item.label = args[1].strVal;
        item.value = args[2].intVal;
        item.playerId = ResolvePlayerId(data, args[3].intVal);
        it->second->items.push_back(item);
        return JassValue::Null();
    });
    vm_.RegisterNative("LeaderboardRemovePlayerItem", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LEADERBOARD);
        if (simId < 0) return JassValue::Null();
        auto it = data->leaderboards.find(simId);
        if (it == data->leaderboards.end()) return JassValue::Null();
        const int playerId = ResolvePlayerId(data, args[1].intVal);
        auto& items = it->second->items;
        items.erase(std::remove_if(items.begin(), items.end(),
                                   [playerId](const JassNativesData::JassLeaderboardItem& item) {
                                       return item.playerId == playerId;
                                   }),
                    items.end());
        return JassValue::Null();
    });
    vm_.RegisterNative("LeaderboardSetItemValue", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LEADERBOARD);
        if (simId < 0) return JassValue::Null();
        auto it = data->leaderboards.find(simId);
        if (it == data->leaderboards.end()) return JassValue::Null();
        const int index = args[1].intVal;
        if (index >= 0 && index < (int)it->second->items.size()) {
            it->second->items[(size_t)index].value = args[2].intVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("LeaderboardSetLabel", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LEADERBOARD);
        if (simId >= 0) {
            auto it = data->leaderboards.find(simId);
            if (it != data->leaderboards.end()) it->second->label = args[1].strVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("LeaderboardClear", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LEADERBOARD);
        if (simId >= 0) {
            auto it = data->leaderboards.find(simId);
            if (it != data->leaderboards.end()) it->second->items.clear();
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("LeaderboardHasPlayerItem", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LEADERBOARD);
        if (simId < 0) return JassValue::Bool(false);
        auto it = data->leaderboards.find(simId);
        if (it == data->leaderboards.end()) return JassValue::Bool(false);
        const int playerId = ResolvePlayerId(data, args[1].intVal);
        for (const auto& item : it->second->items) {
            if (item.playerId == playerId) return JassValue::Bool(true);
        }
        return JassValue::Bool(false);
    });

    // ===== Extended Unit Natives =====
    auto addAbilityToUnit = [data = data_, sim = sim_](int unitHandle, War3ID abilityCode, int level) -> bool {
        if (!data || !sim) return false;
        int simId = data->GetSimHandle(unitHandle, JassHandleInfo::UNIT);
        if (simId < 0) return false;
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return false;
        if (auto* existing = unit->GetAbility(abilityCode)) {
            existing->SetLevel(std::max(existing->GetLevel(), std::max(1, level)));
            return true;
        }
        auto ability = AbilityFactory::CreateAbility(sim->AllocateHandleId(), abilityCode);
        if (!ability) return false;
        ability->SetLevel(std::max(1, level));
        unit->AddAbility(*sim, std::move(ability));
        return unit->GetAbility(abilityCode) != nullptr;
    };
    auto removeAbilityFromUnit = [data = data_, sim = sim_](int unitHandle, War3ID abilityCode) -> bool {
        if (!data || !sim) return false;
        int simId = data->GetSimHandle(unitHandle, JassHandleInfo::UNIT);
        if (simId < 0) return false;
        CUnit* unit = sim->GetUnit(simId);
        return unit && unit->RemoveAbility(*sim, abilityCode);
    };
    auto getUnitAbility = [data = data_, sim = sim_](int unitHandle, War3ID abilityCode) -> CAbility* {
        if (!data || !sim) return nullptr;
        int simId = data->GetSimHandle(unitHandle, JassHandleInfo::UNIT);
        if (simId < 0) return nullptr;
        CUnit* unit = sim->GetUnit(simId);
        return unit ? unit->GetAbility(abilityCode) : nullptr;
    };
    vm_.RegisterNative("CreateAbility", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        return JassValue::Int(AbilityToHandle(*data, War3ID(static_cast<uint32_t>(args[0].intVal))));
    });
    vm_.RegisterNative("GetUnitAbility", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Null();
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Null();
        War3ID abilityCode(static_cast<uint32_t>(args[1].intVal));
        CAbility* ability = unit->GetAbility(abilityCode);
        return ability ? JassValue::Int(AbilityToHandle(*data, ability->GetCode(), ability->GetLevel(), simId))
                       : JassValue::Null();
    });
    vm_.RegisterNative("GetUnitAbilityByIndex", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Null();
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Null();
        const int index = args[1].intVal;
        const auto& abilities = unit->GetAbilities();
        if (index < 0 || index >= static_cast<int>(abilities.size()) || !abilities[(size_t)index]) {
            return JassValue::Null();
        }
        CAbility* ability = abilities[(size_t)index];
        return JassValue::Int(AbilityToHandle(*data, ability->GetCode(), ability->GetLevel(), simId));
    });
    vm_.RegisterNative("UnitAddAbility", [data = data_, addAbilityToUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        return JassValue::Bool(addAbilityToUnit(args[0].intVal, AbilityCodeFromJassArg(data, args[1].intVal), 1));
    });
    vm_.RegisterNative("AddUnitAbility", [data = data_, addAbilityToUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto ability = HandleToAbility(data, args[1].intVal);
        const War3ID abilityCode = ability ? ability->code : AbilityCodeFromJassArg(data, args[1].intVal);
        const int level = ability ? ability->level : 1;
        addAbilityToUnit(args[0].intVal, abilityCode, level);
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitRemoveAbility", [data = data_, removeAbilityFromUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        return JassValue::Bool(removeAbilityFromUnit(args[0].intVal, AbilityCodeFromJassArg(data, args[1].intVal)));
    });
    vm_.RegisterNative("RemoveUnitAbility", [data = data_, removeAbilityFromUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        removeAbilityFromUnit(args[0].intVal, AbilityCodeFromJassArg(data, args[1].intVal));
        return JassValue::Null();
    });
    vm_.RegisterNative("GetUnitAbilityLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Int(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Int(0);
        War3ID abilityCode = AbilityCodeFromJassArg(data, args[1].intVal);
        auto* ability = unit->GetAbility(abilityCode);
        return JassValue::Int(ability ? ability->GetLevel() : 0);
    });
    vm_.RegisterNative("SetUnitAbilityLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Int(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Int(0);
        War3ID abilityCode = AbilityCodeFromJassArg(data, args[1].intVal);
        auto* ability = unit->GetAbility(abilityCode);
        if (!ability) return JassValue::Int(0);
        const int level = std::max(1, args[2].intVal);
        ability->SetLevel(level);
        return JassValue::Int(ability->GetLevel());
    });
    vm_.RegisterNative("SetAbilityLevel", [data = data_, getUnitAbility](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        auto abilityHandle = HandleToAbility(data, args[1].intVal);
        if (!abilityHandle) return JassValue::Null();
        const int level = std::max(1, args[2].intVal);
        abilityHandle->level = level;
        if (auto* ability = getUnitAbility(args[0].intVal, abilityHandle->code)) {
            ability->SetLevel(level);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitHasAbility", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Bool(false);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Bool(false);
        War3ID abilityCode = AbilityCodeFromJassArg(data, args[1].intVal);
        return JassValue::Bool(unit->GetAbility(abilityCode) != nullptr);
    });
    vm_.RegisterNative("GetUnitLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(1);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Int(1);
        CUnit* unit = sim->GetUnit(simId);
        return JassValue::Int(unit ? unit->GetHeroLevel() : 1);
    });
    vm_.RegisterNative("SetUnitLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetHeroLevel(args[1].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetUnitFoodUsed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Int(unit->GetFoodUsed()); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetUnitFoodMade", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Int(unit->GetFoodMade()); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("IsUnitDead", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Bool(unit->IsDead()); }
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("IsUnitAlive", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Bool(!unit->IsDead()); }
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("IsUnitHidden", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Bool(unit->IsHidden()); }
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("ShowUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetHidden(!args[1].boolVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitInvulnerable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetInvulnerable(args[1].boolVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitMana", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetMana(args[1].realVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetUnitMana", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetMana()); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetUnitMaxMana", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real((float)unit->GetMaxMana()); }
        return JassValue::Real(0);
    });

    // ===== Extended Player Natives =====
    vm_.RegisterNative("GetPlayerGold", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        return JassValue::Int(p ? p->GetGold() : 0);
    });
    vm_.RegisterNative("SetPlayerGold", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        if (p) p->SetGold(args[1].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("GetPlayerLumber", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        return JassValue::Int(p ? p->GetLumber() : 0);
    });
    vm_.RegisterNative("SetPlayerLumber", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        if (p) p->SetLumber(args[1].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPlayerAlliance", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Null();
        const int playerId1 = ResolvePlayerId(data, args[0].intVal);
        const int playerId2 = ResolvePlayerId(data, args[1].intVal);
        auto* p1 = sim->GetPlayer(playerId1);
        auto* p2 = sim->GetPlayer(playerId2);
        if (p1 && p2) {
            int allianceType = args[2].intVal;
            bool flag = args[3].boolVal;
            p1->SetAlliance(playerId2, static_cast<CAllianceType>(allianceType), flag);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetPlayerAlliance", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        auto* p1 = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        auto* p2 = sim->GetPlayer(ResolvePlayerId(data, args[1].intVal));
        if (p1 && p2) {
            return JassValue::Bool(p1->HasAlliance(*p2, static_cast<CAllianceType>(args[2].intVal)));
        }
        return JassValue::Bool(false);
    });

    // ===== Extended Group Natives =====
    vm_.RegisterNative("GroupEnumUnitsInRangeOfLoc", [data = data_, sim = sim_, vmPtr = vmPtr_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Null();
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Null();
        int locId = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (locId < 0) return JassValue::Null();
        auto lit = data->locations.find(locId);
        if (lit == data->locations.end()) return JassValue::Null();
        float x = lit->second->x, y = lit->second->y, radius = args[2].realVal;
        float rSq = radius * radius;
        const std::string filterFunc = (args.size() > 3 && args[3].type == JassValue::STRING) ? args[3].strVal : "";
        git->second->unitHandles.clear();
        if (sim) {
            for (auto* unit : sim->GetUnits()) {
                float dx = unit->GetX() - x, dy = unit->GetY() - y;
                const int unitHandle = data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
                if (dx * dx + dy * dy > rSq) continue;
                bool accepted = true;
                if (!filterFunc.empty() && vmPtr) {
                    const int previousFilterUnit = data->filterUnitHandle;
                    data->filterUnitHandle = unitHandle;
                    accepted = JassValueToBool(CallSyncFunction(vmPtr, filterFunc));
                    data->filterUnitHandle = previousFilterUnit;
                }
                if (accepted)
                    git->second->unitHandles.push_back(unitHandle);
            }
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupAddUnitFast", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId >= 0) {
            auto it = data->groups.find(simId);
            if (it != data->groups.end()) it->second->unitHandles.push_back(args[1].intVal);
        }
        return JassValue::Null();
    });

    // ===== Missing Conversion Natives =====
    vm_.RegisterNative("Deg2Rad", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real(args[0].realVal * 3.14159265f / 180.0f);
    });
    vm_.RegisterNative("Rad2Deg", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real(args[0].realVal * 180.0f / 3.14159265f);
    });
    vm_.RegisterNative("I2R", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real((float)args[0].intVal);
    });
    vm_.RegisterNative("R2I", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int((int)args[0].realVal);
    });
    vm_.RegisterNative("I2S", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("0");
        return JassValue::Str(std::to_string(args[0].intVal));
    });
    vm_.RegisterNative("R2S", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("0");
        return JassValue::Str(std::to_string(args[0].realVal));
    });
    vm_.RegisterNative("S2I", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int(std::atoi(args[0].strVal.c_str()));
    });
    vm_.RegisterNative("S2R", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real((float)std::atof(args[0].strVal.c_str()));
    });
}

// ============================================================
// Config-phase player setup natives
// ============================================================
void JassNatives::RegisterConfigPlayerNatives() {
    RegisterTimerNatives(); // maps use timers in config

    vm_.RegisterNative("Player", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const int playerId = std::max(0, std::min(23, args[0].intVal));
        return JassValue::Int(data->AllocHandle(JassHandleInfo::PLAYER, playerId));
    });

    vm_.RegisterNative("GetLocalPlayer", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->AllocHandle(JassHandleInfo::PLAYER, data->localPlayerIndex));
    });

    vm_.RegisterNative("GetPlayerId", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(-1);
        return JassValue::Int(ResolvePlayerId(data, args[0].intVal));
    });

    vm_.RegisterNative("GetPlayerController", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });

    vm_.RegisterNative("GetPlayerSlotState", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        if (playerId >= 0 && playerId < (int)data->playerMapConfig.size() &&
            data->playerMapConfig[playerId].slotActive) {
            return JassValue::Int(1);
        }
        return JassValue::Int(0);
    });

    vm_.RegisterNative("GetGameSpeed", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data ? data->gameConfig.gameSpeed : 2);
    });

    vm_.RegisterNative("GetGameTypeSelected", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(true);
    });

    vm_.RegisterNative("VersionGet", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(1);
    });

    vm_.RegisterNative("SetMapName", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });

    vm_.RegisterNative("SetMapDescription", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });

    vm_.RegisterNative("SetGamePlacement", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });

    vm_.RegisterNative("IsFogEnabled", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(true);
    });

    vm_.RegisterNative("IsFogMaskEnabled", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(true);
    });

    vm_.RegisterNative("GetCameraMargin", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(0.0f);
    });

    vm_.RegisterNative("SetPlayerName", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int p = ResolvePlayerId(data, args[0].intVal);
        if (p >= 0 && p < 24) {
            data->playerMapConfig[p].slotActive = true;
        }
        return JassValue::Null();
    });

    vm_.RegisterNative("SetPlayerTeam", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        const int p = ResolvePlayerId(data, args[0].intVal);
        if (p >= 0 && p < 24) data->playerMapConfig[p].team = args[1].intVal;
        if (sim) {
            if (CPlayer* player = sim->GetPlayer(p)) player->SetTeam(args[1].intVal);
        }
        return JassValue::Null();
    });

    vm_.RegisterNative("SetPlayerState", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });

    vm_.RegisterNative("SetPlayerAlliance", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });

    vm_.RegisterNative("SetStartLocPrioCount", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        data->configuredPlayerCount = args[0].intVal;
        return JassValue::Null();
    });

    vm_.RegisterNative("SetStartLocPrio", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        int idx = args[0].intVal;
        if (idx >= 0 && idx < 24) {
            auto& loc = data->startLocations[idx];
            loc.x = args[2].realVal;
            loc.y = args[3].realVal;
            loc.defined = true;
        }
        return JassValue::Null();
    });

    vm_.RegisterNative("DefineStartLocation", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int idx = args[0].intVal;
        if (idx >= 0 && idx < 24) {
            auto& loc = data->startLocations[idx];
            loc.x = args[1].realVal;
            loc.y = args[2].realVal;
            loc.defined = true;
        }
        return JassValue::Null();
    });

    vm_.RegisterNative("SetPlayerStartLocation", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int playerIdx = ResolvePlayerId(data, args[0].intVal);
        int locIdx = args[1].intVal;
        if (playerIdx >= 0 && playerIdx < 24) {
            data->playerMapConfig[playerIdx].startLocationIndex = locIdx;
            data->playerMapConfig[playerIdx].slotActive = true;
        }
        return JassValue::Null();
    });

    vm_.RegisterNative("ForcePlayerStartLocation", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int playerIdx = ResolvePlayerId(data, args[0].intVal);
        int locIdx = args[1].intVal;
        if (playerIdx >= 0 && playerIdx < 24) {
            data->playerMapConfig[playerIdx].startLocationIndex = locIdx;
            data->playerMapConfig[playerIdx].slotActive = true;
        }
        return JassValue::Null();
    });

    vm_.RegisterNative("SetPlayerSlotAvailable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        const int playerIdx = ResolvePlayerId(data, args[0].intVal);
        if (playerIdx >= 0 && playerIdx < (int)data->playerMapConfig.size()) {
            data->playerMapConfig[playerIdx].slotActive = (args[1].intVal != 0);
        }
        return JassValue::Null();
    });

    vm_.RegisterNative("InitBlizzard", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });

    vm_.RegisterNative("SetPlayerRacePreference", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });

    vm_.RegisterNative("SetPlayerRaceSelectable", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });

    vm_.RegisterNative("SetPlayerController", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });

    vm_.RegisterNative("SetPlayerColor", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });

    vm_.RegisterNative("SetTeams", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });

    vm_.RegisterNative("SetPlayers", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });
}

// ============================================================
// 24. Order-by-id natives (retail common.j numeric order ids)
// ============================================================
void JassNatives::RegisterOrderIdNatives() {
    // Shared issuers: same dispatch as the string-order twins, but the
    // order id comes straight from the script (ref ById natives).
    auto issueNoTarget = [data = data_, sim = sim_](int unitHandle, int orderId) -> bool {
        if (orderId <= 0 || !sim) return false;
        int uSim = data->GetSimHandle(unitHandle, JassHandleInfo::UNIT);
        if (uSim < 0) return false;
        CUnit* u = sim->GetUnit(uSim);
        if (!u) return false;
        u->IssueOrder(std::make_unique<COrderNoTarget>(orderId));
        return true;
    };
    auto issuePoint = [data = data_, sim = sim_](int unitHandle, int orderId, float x, float y) -> bool {
        if (orderId <= 0 || !sim) return false;
        int uSim = data->GetSimHandle(unitHandle, JassHandleInfo::UNIT);
        if (uSim < 0) return false;
        CUnit* u = sim->GetUnit(uSim);
        if (!u) return false;
        u->IssueOrder(std::make_unique<COrderTargetPoint>(orderId, x, y));
        return true;
    };
    auto issueTarget = [data = data_, sim = sim_](int unitHandle, int orderId, int targetHandle) -> bool {
        if (orderId <= 0 || !sim) return false;
        int uSim = data->GetSimHandle(unitHandle, JassHandleInfo::UNIT);
        if (uSim < 0) return false;
        CUnit* u = sim->GetUnit(uSim);
        if (!u) return false;
        int tSim = data->GetSimHandle(targetHandle, JassHandleInfo::UNIT);
        CUnit* target = tSim >= 0 ? sim->GetUnit(tSim) : nullptr;
        if (!target) return false;
        u->IssueOrder(std::make_unique<COrderTargetWidget>(orderId, target));
        return true;
    };
    auto groupUnits = [data = data_](int groupHandle) -> std::vector<int> {
        int simId = data->GetSimHandle(groupHandle, JassHandleInfo::GROUP);
        if (simId < 0) return {};
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return {};
        return git->second->unitHandles;
    };
    auto locationXY = [data = data_](int locHandle, float& x, float& y) -> bool {
        int locSim = data->GetSimHandle(locHandle, JassHandleInfo::LOCATION);
        if (locSim < 0) return false;
        auto it = data->locations.find(locSim);
        if (it == data->locations.end()) return false;
        x = it->second->x;
        y = it->second->y;
        return true;
    };

    vm_.RegisterNative("IssueImmediateOrderById", [issueNoTarget](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        return JassValue::Bool(issueNoTarget(args[0].intVal, args[1].intVal));
    });
    vm_.RegisterNative("IssuePointOrderById", [issuePoint](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Bool(false);
        return JassValue::Bool(issuePoint(args[0].intVal, args[1].intVal, args[2].realVal, args[3].realVal));
    });
    vm_.RegisterNative("IssuePointOrderByIdLoc", [issuePoint, locationXY](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Bool(false);
        float x = 0, y = 0;
        if (!locationXY(args[2].intVal, x, y)) return JassValue::Bool(false);
        return JassValue::Bool(issuePoint(args[0].intVal, args[1].intVal, x, y));
    });
    vm_.RegisterNative("IssueTargetOrderById", [issueTarget](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Bool(false);
        return JassValue::Bool(issueTarget(args[0].intVal, args[1].intVal, args[2].intVal));
    });
    vm_.RegisterNative("IssueBuildOrderById", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Bool(false);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Bool(false);
        CUnit* u = sim->GetUnit(uSim);
        if (!u) return JassValue::Bool(false);
        // args[1] is the building/unit rawcode used as order id (ref).
        u->IssueOrder(std::make_unique<COrderTargetPoint>((int)(uint32_t)args[1].intVal, args[2].realVal, args[3].realVal));
        return JassValue::Bool(true);
    });
    // Instant orders execute through the same queue; behaviorally identical
    // here because orders dispatch on the next sim tick (ref instant queue).
    vm_.RegisterNative("IssueInstantPointOrder", [issuePoint](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Bool(false);
        return JassValue::Bool(issuePoint(args[0].intVal, ParseOrderString(args[1].strVal), args[2].realVal, args[3].realVal));
    });
    vm_.RegisterNative("IssueInstantPointOrderById", [issuePoint](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Bool(false);
        return JassValue::Bool(issuePoint(args[0].intVal, args[1].intVal, args[2].realVal, args[3].realVal));
    });
    vm_.RegisterNative("IssueInstantTargetOrder", [issueTarget](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Bool(false);
        return JassValue::Bool(issueTarget(args[0].intVal, ParseOrderString(args[1].strVal), args[2].intVal));
    });
    vm_.RegisterNative("IssueInstantTargetOrderById", [issueTarget](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Bool(false);
        return JassValue::Bool(issueTarget(args[0].intVal, args[1].intVal, args[2].intVal));
    });
    // Neutral orders: the sim has no neutral-order gate, so these issue
    // exactly like player orders (ref neutral dispatch without controller).
    vm_.RegisterNative("IssueNeutralImmediateOrderById", [issueNoTarget](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Bool(false);
        return JassValue::Bool(issueNoTarget(args[1].intVal, args[2].intVal));
    });
    vm_.RegisterNative("IssueNeutralPointOrderById", [issuePoint](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Bool(false);
        return JassValue::Bool(issuePoint(args[1].intVal, args[2].intVal, args[3].realVal, args[4].realVal));
    });
    vm_.RegisterNative("IssueNeutralTargetOrderById", [issueTarget](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Bool(false);
        return JassValue::Bool(issueTarget(args[1].intVal, args[2].intVal, args[3].intVal));
    });
    vm_.RegisterNative("IssueNeutralImmediateOrder", [issueNoTarget](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Bool(false);
        return JassValue::Bool(issueNoTarget(args[1].intVal, ParseOrderString(args[2].strVal)));
    });
    vm_.RegisterNative("IssueNeutralPointOrder", [issuePoint](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Bool(false);
        return JassValue::Bool(issuePoint(args[1].intVal, ParseOrderString(args[2].strVal), args[3].realVal, args[4].realVal));
    });
    vm_.RegisterNative("IssueNeutralTargetOrder", [issueTarget](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Bool(false);
        return JassValue::Bool(issueTarget(args[1].intVal, ParseOrderString(args[2].strVal), args[3].intVal));
    });

    vm_.RegisterNative("GroupImmediateOrderById", [data = data_, sim = sim_, groupUnits](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || args[1].intVal <= 0 || !sim) return JassValue::Bool(false);
        for (int uh : groupUnits(args[0].intVal)) {
            int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT);
            if (uSim >= 0) {
                if (CUnit* u = sim->GetUnit(uSim)) u->IssueOrder(std::make_unique<COrderNoTarget>(args[1].intVal));
            }
        }
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("GroupPointOrderById", [data = data_, sim = sim_, groupUnits](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || args[1].intVal <= 0 || !sim) return JassValue::Bool(false);
        for (int uh : groupUnits(args[0].intVal)) {
            int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT);
            if (uSim >= 0) {
                if (CUnit* u = sim->GetUnit(uSim)) u->IssueOrder(std::make_unique<COrderTargetPoint>(args[1].intVal, args[2].realVal, args[3].realVal));
            }
        }
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("GroupPointOrderByIdLoc", [data = data_, sim = sim_, groupUnits, locationXY](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || args[1].intVal <= 0 || !sim) return JassValue::Bool(false);
        float x = 0, y = 0;
        if (!locationXY(args[2].intVal, x, y)) return JassValue::Bool(false);
        for (int uh : groupUnits(args[0].intVal)) {
            int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT);
            if (uSim >= 0) {
                if (CUnit* u = sim->GetUnit(uSim)) u->IssueOrder(std::make_unique<COrderTargetPoint>(args[1].intVal, x, y));
            }
        }
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("GroupPointOrderLoc", [data = data_, sim = sim_, groupUnits, locationXY](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        int orderId = ParseOrderString(args[1].strVal);
        if (orderId <= 0) return JassValue::Bool(false);
        float x = 0, y = 0;
        if (!locationXY(args[2].intVal, x, y)) return JassValue::Bool(false);
        for (int uh : groupUnits(args[0].intVal)) {
            int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT);
            if (uSim >= 0) {
                if (CUnit* u = sim->GetUnit(uSim)) u->IssueOrder(std::make_unique<COrderTargetPoint>(orderId, x, y));
            }
        }
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("GroupTargetOrderById", [data = data_, sim = sim_, groupUnits](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || args[1].intVal <= 0 || !sim) return JassValue::Bool(false);
        int tSim = data->GetSimHandle(args[2].intVal, JassHandleInfo::UNIT);
        CUnit* target = tSim >= 0 ? sim->GetUnit(tSim) : nullptr;
        if (!target) return JassValue::Bool(false);
        for (int uh : groupUnits(args[0].intVal)) {
            int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT);
            if (uSim >= 0) {
                if (CUnit* u = sim->GetUnit(uSim)) u->IssueOrder(std::make_unique<COrderTargetWidget>(args[1].intVal, target));
            }
        }
        return JassValue::Bool(true);
    });
}

// ============================================================
// 25. Terrain / rect / region query natives
// ============================================================
void JassNatives::RegisterTerrainQueryNatives() {
    // Retail pathingtype ordinals (common.j ConvertPathingType 0..7).
    constexpr int kPathAny = 0, kPathWalk = 1, kPathFly = 2, kPathBuild = 3;
    constexpr int kPathPeon = 4, kPathBlight = 5, kPathFloat = 6, kPathAmphib = 7;

    vm_.RegisterNative("GetTerrainType", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        const W3EMap* map = sim->GetTerrainMap();
        if (!map) return JassValue::Int(0);
        const W3ETileCorner* corner = W3ECornerAt(*map, args[0].realVal, args[1].realVal);
        if (!corner) return JassValue::Int(0);
        const int paletteIndex = corner->GroundTexture();
        if (paletteIndex < 0 || paletteIndex >= (int)map->groundTilePalette.size()) return JassValue::Int(0);
        return JassValue::Int((int)map->groundTilePalette[(size_t)paletteIndex]);
    });
    vm_.RegisterNative("GetTerrainVariance", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        const W3EMap* map = sim->GetTerrainMap();
        if (!map) return JassValue::Int(0);
        const W3ETileCorner* corner = W3ECornerAt(*map, args[0].realVal, args[1].realVal);
        return JassValue::Int(corner ? corner->GroundVariation() : 0);
    });
    vm_.RegisterNative("GetTerrainCliffLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        const W3EMap* map = sim->GetTerrainMap();
        if (!map) return JassValue::Int(0);
        const W3ETileCorner* corner = W3ECornerAt(*map, args[0].realVal, args[1].realVal);
        return JassValue::Int(corner ? corner->LayerHeight() : 0);
    });
    vm_.RegisterNative("IsTerrainPathable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        const float x = args[0].realVal, y = args[1].realVal;
        const int type = args[2].intVal;
        const W3EMap* map = sim->GetTerrainMap();
        const W3ETileCorner* corner = map ? W3ECornerAt(*map, x, y) : nullptr;
        const bool inMap = corner != nullptr;
        const bool water = inMap && corner->IsWater();
        const bool boundary = inMap && corner->IsBoundary();
        auto walkable = [&]() -> bool {
            if (!inMap) return false;
            if (water || boundary) return false;
            if (sim->GetPathfinding() && sim->GetPathfinding()->HasPathingMap()) {
                return sim->GetPathfinding()->IsWorldPointPathableForUnit(x, y, 0.0f, PathingGridMovementType::FOOT, nullptr);
            }
            return true;
        };
        switch (type) {
            case kPathAny: return JassValue::Bool(walkable() || (inMap && !boundary));
            case kPathWalk:
            case kPathPeon: return JassValue::Bool(walkable());
            case kPathFly: return JassValue::Bool(inMap && !boundary);
            case kPathBuild: return JassValue::Bool(walkable() && !sim->IsPointBlighted(x, y));
            case kPathBlight: return JassValue::Bool(sim->IsPointBlighted(x, y));
            case kPathFloat: return JassValue::Bool(inMap && water && !boundary);
            case kPathAmphib: return JassValue::Bool(walkable() || (inMap && water && !boundary));
            default: return JassValue::Bool(false);
        }
    });
    vm_.RegisterNative("IsPointBlighted", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        return JassValue::Bool(sim->IsPointBlighted(args[0].realVal, args[1].realVal));
    });
    // Retail takes the owning player first; blight is global in the sim.
    vm_.RegisterNative("SetBlight", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 5 || !sim) return JassValue::Null();
        sim->SetBlightCircle(args[1].realVal, args[2].realVal, args[3].realVal, args[4].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetBlightRect", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        int rectSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::RECT);
        if (rectSim < 0) return JassValue::Null();
        auto it = data->rects.find(rectSim);
        if (it == data->rects.end()) return JassValue::Null();
        const JassRect& r = *it->second;
        sim->SetBlightRect(r.minX, r.minY, r.maxX, r.maxY, args[2].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetBlightPoint", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 5 || !sim) return JassValue::Null();
        // Ref setBlight(x, y, radius, blighted): circular paint.
        sim->SetBlightCircle(args[1].realVal, args[2].realVal, args[3].realVal, args[4].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetBlightLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Null();
        int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (locSim < 0) return JassValue::Null();
        auto it = data->locations.find(locSim);
        if (it == data->locations.end()) return JassValue::Null();
        sim->SetBlightCircle(it->second->x, it->second->y, args[2].realVal, args[3].boolVal);
        return JassValue::Null();
    });

    vm_.RegisterNative("SetRect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        int rectSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (rectSim < 0) return JassValue::Null();
        auto it = data->rects.find(rectSim);
        if (it == data->rects.end()) return JassValue::Null();
        JassRect& r = *it->second;
        r.minX = args[1].realVal; r.minY = args[2].realVal;
        r.maxX = args[3].realVal; r.maxY = args[4].realVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetRectFromLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int rectSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        int minSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        int maxSim = data->GetSimHandle(args[2].intVal, JassHandleInfo::LOCATION);
        if (rectSim < 0 || minSim < 0 || maxSim < 0) return JassValue::Null();
        auto rit = data->rects.find(rectSim);
        auto minIt = data->locations.find(minSim), maxIt = data->locations.find(maxSim);
        if (rit == data->rects.end() || minIt == data->locations.end() || maxIt == data->locations.end()) {
            return JassValue::Null();
        }
        JassRect& r = *rit->second;
        r.minX = std::min(minIt->second->x, maxIt->second->x);
        r.minY = std::min(minIt->second->y, maxIt->second->y);
        r.maxX = std::max(minIt->second->x, maxIt->second->x);
        r.maxY = std::max(minIt->second->y, maxIt->second->y);
        return JassValue::Null();
    });
    vm_.RegisterNative("MoveRectTo", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int rectSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (rectSim < 0) return JassValue::Null();
        auto it = data->rects.find(rectSim);
        if (it == data->rects.end()) return JassValue::Null();
        JassRect& r = *it->second;
        const float halfW = (r.maxX - r.minX) * 0.5f;
        const float halfH = (r.maxY - r.minY) * 0.5f;
        r.minX = args[1].realVal - halfW; r.maxX = args[1].realVal + halfW;
        r.minY = args[2].realVal - halfH; r.maxY = args[2].realVal + halfH;
        return JassValue::Null();
    });

    static constexpr float kRegionCellSize = 32.0f;
    vm_.RegisterNative("RegionAddCell", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        if (rSim < 0) return JassValue::Null();
        auto it = data->regions.find(rSim);
        if (it == data->regions.end()) return JassValue::Null();
        JassRect cell;
        cell.minX = args[1].realVal * kRegionCellSize;
        cell.minY = args[2].realVal * kRegionCellSize;
        cell.maxX = cell.minX + kRegionCellSize;
        cell.maxY = cell.minY + kRegionCellSize;
        it->second->cells.push_back(cell);
        return JassValue::Null();
    });
    vm_.RegisterNative("RegionAddCellAtLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (rSim < 0 || locSim < 0) return JassValue::Null();
        auto rit = data->regions.find(rSim);
        auto lit = data->locations.find(locSim);
        if (rit == data->regions.end() || lit == data->locations.end()) return JassValue::Null();
        JassRect cell;
        cell.minX = std::floor(lit->second->x / kRegionCellSize) * kRegionCellSize;
        cell.minY = std::floor(lit->second->y / kRegionCellSize) * kRegionCellSize;
        cell.maxX = cell.minX + kRegionCellSize;
        cell.maxY = cell.minY + kRegionCellSize;
        rit->second->cells.push_back(cell);
        return JassValue::Null();
    });
    vm_.RegisterNative("RegionClearCell", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        if (rSim < 0) return JassValue::Null();
        auto it = data->regions.find(rSim);
        if (it == data->regions.end()) return JassValue::Null();
        const float x0 = args[1].realVal * kRegionCellSize;
        const float y0 = args[2].realVal * kRegionCellSize;
        auto& cells = it->second->cells;
        cells.erase(std::remove_if(cells.begin(), cells.end(), [&](const JassRect& c) {
            return c.minX == x0 && c.minY == y0;
        }), cells.end());
        return JassValue::Null();
    });
    vm_.RegisterNative("RegionClearCellAtLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (rSim < 0 || locSim < 0) return JassValue::Null();
        auto rit = data->regions.find(rSim);
        auto lit = data->locations.find(locSim);
        if (rit == data->regions.end() || lit == data->locations.end()) return JassValue::Null();
        const float x0 = std::floor(lit->second->x / kRegionCellSize) * kRegionCellSize;
        const float y0 = std::floor(lit->second->y / kRegionCellSize) * kRegionCellSize;
        auto& cells = rit->second->cells;
        cells.erase(std::remove_if(cells.begin(), cells.end(), [&](const JassRect& c) {
            return c.minX == x0 && c.minY == y0;
        }), cells.end());
        return JassValue::Null();
    });
    vm_.RegisterNative("IsPointInRegion", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Bool(false);
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        if (rSim < 0) return JassValue::Bool(false);
        auto it = data->regions.find(rSim);
        if (it == data->regions.end()) return JassValue::Bool(false);
        return JassValue::Bool(IsPointInJassRegion(*it->second, args[1].realVal, args[2].realVal));
    });
    vm_.RegisterNative("IsLocationInRegion", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (rSim < 0 || locSim < 0) return JassValue::Bool(false);
        auto rit = data->regions.find(rSim);
        auto lit = data->locations.find(locSim);
        if (rit == data->regions.end() || lit == data->locations.end()) return JassValue::Bool(false);
        return JassValue::Bool(IsPointInJassRegion(*rit->second, lit->second->x, lit->second->y));
    });
    vm_.RegisterNative("GetWorldBounds", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)args;
        auto rect = std::make_shared<JassRect>();
        if (sim) {
            if (const W3EMap* map = sim->GetTerrainMap()) {
                rect->minX = map->offsetX;
                rect->minY = map->offsetY;
                rect->maxX = map->offsetX + (float)map->tileWidth * 128.0f;
                rect->maxY = map->offsetY + (float)map->tileHeight * 128.0f;
            }
        }
        int id = data->nextRectId++;
        data->rects[id] = rect;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::RECT, id));
    });
}

// ============================================================
// 26. Player / team / start-location natives
// ============================================================
void JassNatives::RegisterPlayerTeamNatives() {
    auto allPlayersForce = [data = data_, sim = sim_]() -> int {
        auto force = std::make_shared<JassForce>();
        if (sim) {
            for (int i = 0; i < sim->GetPlayerCount(); i++) {
                force->playerIds.push_back(i);
            }
        }
        int id = data->nextForceId++;
        data->forces[id] = force;
        return data->AllocHandle(JassHandleInfo::FORCE, id);
    };
    vm_.RegisterNative("GetPlayers", [allPlayersForce](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(allPlayersForce());
    });
    vm_.RegisterNative("GetTeams", [allPlayersForce](const std::vector<JassValue>&) -> JassValue {
        // Retail returns all slots as well; team subdivision is per-player.
        return JassValue::Int(allPlayersForce());
    });
    vm_.RegisterNative("GetPlayerTeam", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
            return JassValue::Int(p->GetTeam());
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("SetPlayerTaxRate", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        const int p1 = ResolvePlayerId(data, args[0].intVal);
        const int p2 = ResolvePlayerId(data, args[1].intVal);
        const int64_t key = ((int64_t)(p1 + 64) * 128 + (p2 + 64)) * 16 + (args[2].intVal & 15);
        data->playerTaxRates[key] = args[3].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("GetPlayerTaxRate", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Int(0);
        const int p1 = ResolvePlayerId(data, args[0].intVal);
        const int p2 = ResolvePlayerId(data, args[1].intVal);
        const int64_t key = ((int64_t)(p1 + 64) * 128 + (p2 + 64)) * 16 + (args[2].intVal & 15);
        auto it = data->playerTaxRates.find(key);
        return JassValue::Int(it != data->playerTaxRates.end() ? it->second : 0);
    });
    vm_.RegisterNative("GetPlayerSelectable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(true);
        const int p = ResolvePlayerId(data, args[0].intVal);
        auto it = data->playerSelectable.find(p);
        // No retail setter exists; slots are selectable unless recorded off.
        return JassValue::Bool(it != data->playerSelectable.end() ? it->second : true);
    });
    vm_.RegisterNative("GetStartLocPrio", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        (void)data; (void)args;
        // Start-location priority tables are not part of w3i; melee start
        // positions come from the config env. Default: no priority.
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetStartLocPrioSlot", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        (void)data; (void)args;
        return JassValue::Int(-1);
    });
    vm_.RegisterNative("GetUnitCurrentOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Int(0);
        CUnit* u = sim->GetUnit(uSim);
        return JassValue::Int(u ? u->GetLastOrderId() : 0);
    });
    vm_.RegisterNative("SetMapFlag", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->gameConfig.mapFlags |= (uint32_t)args[0].intVal;
        return JassValue::Null();
    });
}

// ============================================================
// 27. Visual natives: spell effects, images, trackables, ubersplats
// ============================================================
void JassNatives::RegisterVisualNatives() {
    auto makeEffect = [data = data_]() -> std::pair<int, std::shared_ptr<JassEffect>> {
        auto e = std::make_shared<JassEffect>();
        int id = data->nextEffectId++;
        data->effects[id] = e;
        return {data->AllocHandle(JassHandleInfo::EFFECT, id), e};
    };
    // Ability art lookup: ById natives resolve the model path from the
    // ability profile (ref ability art); string spell natives pass it.
    auto abilityArt = [data = data_, sim = sim_](int abilityId, int type) -> std::string {
        (void)data;
        if (!sim || type < 0 || type > 6) return "";
        const CAbilityData* abilityData = sim->GetAbilityData();
        if (!abilityData) return "";
        const CAbilityDefFull* def = abilityData->GetAbility(War3ID((uint32_t)abilityId));
        if (!def || type >= (int)def->effectArtByType.size()) return "";
        return def->effectArtByType[(size_t)type];
    };
    auto resolveWidget = [data = data_](int widgetHandle) -> int {
        // Spell targets may be units, items or destructables; first hit wins.
        // Stored for renderer consumption alongside the attach point.
        static const JassHandleInfo::Type kinds[] = {
            JassHandleInfo::UNIT, JassHandleInfo::ITEM, JassHandleInfo::DESTRUCTABLE,
        };
        for (auto kind : kinds) {
            int simId = data->GetSimHandle(widgetHandle, kind);
            if (simId >= 0) return simId;
        }
        return -1;
    };

    vm_.RegisterNative("AddSpellEffect", [makeEffect, abilityArt](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Int(0);
        auto [handle, e] = makeEffect();
        // Retail shows the ability's own art of type t (abilityString is a
        // 4-char code); a longer string is kept verbatim defensively.
        e->abilityId = RawcodeFromString(args[0].strVal);
        e->effectType = args[1].intVal;
        e->modelPath = args[0].strVal.size() == 4
            ? abilityArt(e->abilityId, e->effectType)
            : args[0].strVal;
        e->x = args[2].realVal;
        e->y = args[3].realVal;
        return JassValue::Int(handle);
    });
    vm_.RegisterNative("AddSpellEffectLoc", [data = data_, makeEffect, abilityArt](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Int(0);
        auto [handle, e] = makeEffect();
        e->abilityId = RawcodeFromString(args[0].strVal);
        e->effectType = args[1].intVal;
        e->modelPath = args[0].strVal.size() == 4
            ? abilityArt(e->abilityId, e->effectType)
            : args[0].strVal;
        int locSim = data->GetSimHandle(args[2].intVal, JassHandleInfo::LOCATION);
        if (locSim >= 0) {
            auto it = data->locations.find(locSim);
            if (it != data->locations.end()) {
                e->x = it->second->x;
                e->y = it->second->y;
            }
        }
        return JassValue::Int(handle);
    });
    vm_.RegisterNative("AddSpellEffectById", [makeEffect, abilityArt](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Int(0);
        auto [handle, e] = makeEffect();
        e->abilityId = args[0].intVal;
        e->effectType = args[1].intVal;
        e->modelPath = abilityArt(args[0].intVal, args[1].intVal);
        e->x = args[2].realVal;
        e->y = args[3].realVal;
        return JassValue::Int(handle);
    });
    vm_.RegisterNative("AddSpellEffectByIdLoc", [data = data_, makeEffect, abilityArt](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Int(0);
        auto [handle, e] = makeEffect();
        e->abilityId = args[0].intVal;
        e->effectType = args[1].intVal;
        e->modelPath = abilityArt(args[0].intVal, args[1].intVal);
        int locSim = data->GetSimHandle(args[2].intVal, JassHandleInfo::LOCATION);
        if (locSim >= 0) {
            auto it = data->locations.find(locSim);
            if (it != data->locations.end()) {
                e->x = it->second->x;
                e->y = it->second->y;
            }
        }
        return JassValue::Int(handle);
    });
    vm_.RegisterNative("AddSpellEffectTarget", [makeEffect, resolveWidget](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Int(0);
        auto [handle, e] = makeEffect();
        e->modelPath = args[0].strVal;
        e->effectType = args[1].intVal;
        e->targetSimHandle = resolveWidget(args[2].intVal);
        e->attachPoint = args[3].strVal;
        return JassValue::Int(handle);
    });
    vm_.RegisterNative("AddSpellEffectTargetById", [makeEffect, abilityArt, resolveWidget](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Int(0);
        auto [handle, e] = makeEffect();
        e->abilityId = args[0].intVal;
        e->effectType = args[1].intVal;
        e->modelPath = abilityArt(args[0].intVal, args[1].intVal);
        e->targetSimHandle = resolveWidget(args[2].intVal);
        e->attachPoint = args[3].strVal;
        return JassValue::Int(handle);
    });
    vm_.RegisterNative("GetAbilityEffect", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 3 || !sim) return JassValue::Str("");
        const CAbilityData* abilityData = sim->GetAbilityData();
        if (!abilityData) return JassValue::Str("");
        const CAbilityDefFull* def = abilityData->GetAbility(War3ID::FromString(args[0].strVal));
        if (!def) return JassValue::Str("");
        const int type = args[1].intVal;
        // Single art value per type in the Func profiles (no index lists).
        if (type < 0 || type >= (int)def->effectArtByType.size()) return JassValue::Str("");
        return JassValue::Str(def->effectArtByType[(size_t)type]);
    });
    vm_.RegisterNative("GetAbilityEffectById", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 3 || !sim) return JassValue::Str("");
        const CAbilityData* abilityData = sim->GetAbilityData();
        if (!abilityData) return JassValue::Str("");
        const CAbilityDefFull* def = abilityData->GetAbility(War3ID((uint32_t)args[0].intVal));
        if (!def) return JassValue::Str("");
        const int type = args[1].intVal;
        if (type < 0 || type >= (int)def->effectArtByType.size()) return JassValue::Str("");
        return JassValue::Str(def->effectArtByType[(size_t)type]);
    });
    vm_.RegisterNative("GetAbilitySound", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 3 || !sim) return JassValue::Str("");
        const CAbilityData* abilityData = sim->GetAbilityData();
        if (!abilityData) return JassValue::Str("");
        const CAbilityDefFull* def = abilityData->GetAbility(War3ID::FromString(args[0].strVal));
        // Func profiles carry a single Effectsound per ability (no per-type).
        return JassValue::Str(def ? def->effectSound : "");
    });
    vm_.RegisterNative("GetAbilitySoundById", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 3 || !sim) return JassValue::Str("");
        const CAbilityData* abilityData = sim->GetAbilityData();
        if (!abilityData) return JassValue::Str("");
        const CAbilityDefFull* def = abilityData->GetAbility(War3ID((uint32_t)args[0].intVal));
        return JassValue::Str(def ? def->effectSound : "");
    });

    vm_.RegisterNative("CreateImage", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 11) return JassValue::Int(0);
        auto img = std::make_shared<JassImage>();
        img->file = args[0].strVal;
        img->sizeX = args[1].realVal; img->sizeY = args[2].realVal; img->sizeZ = args[3].realVal;
        img->posX = args[4].realVal; img->posY = args[5].realVal; img->posZ = args[6].realVal;
        img->originX = args[7].realVal; img->originY = args[8].realVal; img->originZ = args[9].realVal;
        img->imageType = args[10].intVal;
        int id = data->nextImageId++;
        data->images[id] = img;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::IMAGE, id));
    });
    vm_.RegisterNative("DestroyImage", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) { data->images.erase(id); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("ShowImage", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) it->second->visible = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetImageRender", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) it->second->render = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetImageRenderAlways", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) it->second->renderAlways = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetImageAboveWater", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) it->second->aboveWater = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetImageColor", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) {
                it->second->red = args[1].intVal;
                it->second->green = args[2].intVal;
                it->second->blue = args[3].intVal;
                it->second->alpha = args[4].intVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetImageConstantHeight", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) it->second->constantHeight = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetImagePosition", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) {
                it->second->posX = args[1].realVal;
                it->second->posY = args[2].realVal;
                it->second->posZ = args[3].realVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetImageType", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) it->second->imageType = args[1].intVal;
        }
        return JassValue::Null();
    });

    vm_.RegisterNative("CreateTrackable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Int(0);
        auto t = std::make_shared<JassTrackable>();
        t->modelPath = args[0].strVal;
        t->x = args[1].realVal;
        t->y = args[2].realVal;
        t->facing = args[3].realVal;
        int id = data->nextTrackableId++;
        data->trackables[id] = t;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::TRACKABLE, id));
    });
    // Retail has no DestroyTrackable; hit/track event interest is stored on
    // triggers below and fired once the render picking path exists.
    vm_.RegisterNative("TriggerRegisterTrackableHitEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        // Triggers are keyed by raw id (ref CreateTrigger convention).
        if (data->GetSimHandle(args[1].intVal, JassHandleInfo::TRACKABLE) < 0) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) it->second->registeredEvents.emplace_back(args[1].intVal, 4101);
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterTrackableTrackEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (data->GetSimHandle(args[1].intVal, JassHandleInfo::TRACKABLE) < 0) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) it->second->registeredEvents.emplace_back(args[1].intVal, 4102);
        return JassValue::Null();
    });

    vm_.RegisterNative("CreateUbersplat", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 9) return JassValue::Int(0);
        auto s = std::make_shared<JassUbersplat>();
        s->x = args[0].realVal;
        s->y = args[1].realVal;
        s->name = args[2].strVal;
        s->red = args[3].intVal;
        s->green = args[4].intVal;
        s->blue = args[5].intVal;
        s->alpha = args[6].intVal;
        s->forcePaused = args[7].boolVal;
        s->noBirthTime = args[8].boolVal;
        int id = data->nextUbersplatId++;
        data->ubersplats[id] = s;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::UBERSPLAT, id));
    });
    vm_.RegisterNative("DestroyUbersplat", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::UBERSPLAT);
        if (id >= 0) { data->ubersplats.erase(id); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("FinishUbersplat", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::UBERSPLAT);
        if (id >= 0) {
            auto it = data->ubersplats.find(id);
            if (it != data->ubersplats.end()) it->second->finished = true;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ResetUbersplat", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::UBERSPLAT);
        if (id >= 0) {
            auto it = data->ubersplats.find(id);
            if (it != data->ubersplats.end()) it->second->finished = false;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUbersplatRender", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::UBERSPLAT);
        if (id >= 0) {
            auto it = data->ubersplats.find(id);
            if (it != data->ubersplats.end()) it->second->render = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUbersplatRenderAlways", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::UBERSPLAT);
        if (id >= 0) {
            auto it = data->ubersplats.find(id);
            if (it != data->ubersplats.end()) it->second->renderAlways = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ShowUbersplat", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::UBERSPLAT);
        if (id >= 0) {
            auto it = data->ubersplats.find(id);
            if (it != data->ubersplats.end()) it->second->render = args[1].boolVal;
        }
        return JassValue::Null();
    });
}

// ============================================================
// 28. Pool + shop-stock natives
// ============================================================
// Weighted roll shared by pools (ref pool placement). roll01 comes from
// the deterministic sim RNG so map setup stays in sync.
static uint32_t PickWeightedPoolEntry(const std::vector<std::pair<uint32_t, double>>& entries, double roll01) {
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

void JassNatives::RegisterPoolStockNatives() {
    vm_.RegisterNative("CreateItemPool", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto pool = std::make_shared<JassNativesData::JassWeightedPool>();
        int id = data->nextItemPoolId++;
        data->itemPools[id] = pool;
        return JassValue::Int(id);
    });
    vm_.RegisterNative("DestroyItemPool", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty()) data->itemPools.erase(args[0].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("ItemPoolAddItemType", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        auto it = data->itemPools.find(args[0].intVal);
        if (it == data->itemPools.end()) return JassValue::Null();
        it->second->entries.emplace_back((uint32_t)args[1].intVal, args[2].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("ItemPoolRemoveItemType", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto it = data->itemPools.find(args[0].intVal);
        if (it == data->itemPools.end()) return JassValue::Null();
        auto& entries = it->second->entries;
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const auto& e) {
            return e.first == (uint32_t)args[1].intVal;
        }), entries.end());
        return JassValue::Null();
    });
    vm_.RegisterNative("PlaceRandomItem", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        auto it = data->itemPools.find(args[0].intVal);
        if (it == data->itemPools.end()) return JassValue::Null();
        const uint32_t picked = PickWeightedPoolEntry(it->second->entries, sim->SimRand01());
        if (picked == 0) return JassValue::Null();
        return JassValue::Int(data->CreateItemHandle(sim, War3ID(picked), args[1].realVal, args[2].realVal));
    });
    vm_.RegisterNative("CreateUnitPool", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto pool = std::make_shared<JassNativesData::JassWeightedPool>();
        int id = data->nextUnitPoolId++;
        data->unitPools[id] = pool;
        return JassValue::Int(id);
    });
    vm_.RegisterNative("DestroyUnitPool", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty()) data->unitPools.erase(args[0].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitPoolAddUnitType", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        auto it = data->unitPools.find(args[0].intVal);
        if (it == data->unitPools.end()) return JassValue::Null();
        it->second->entries.emplace_back((uint32_t)args[1].intVal, args[2].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitPoolRemoveUnitType", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto it = data->unitPools.find(args[0].intVal);
        if (it == data->unitPools.end()) return JassValue::Null();
        auto& entries = it->second->entries;
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const auto& e) {
            return e.first == (uint32_t)args[1].intVal;
        }), entries.end());
        return JassValue::Null();
    });
    vm_.RegisterNative("PlaceRandomUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5 || !sim) return JassValue::Null();
        auto it = data->unitPools.find(args[0].intVal);
        if (it == data->unitPools.end()) return JassValue::Null();
        const uint32_t picked = PickWeightedPoolEntry(it->second->entries, sim->SimRand01());
        if (picked == 0) return JassValue::Null();
        CUnit* unit = sim->CreateUnit(War3ID(picked), ResolvePlayerId(data, args[1].intVal),
                                      args[2].realVal, args[3].realVal, args[4].realVal);
        if (!unit) return JassValue::Null();
        return JassValue::Int(data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId()));
    });
    vm_.RegisterNative("ChooseRandomCreep", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.empty() || !sim) return JassValue::Int(0);
        const CUnitData* unitData = sim->GetUnitData();
        if (!unitData) return JassValue::Int(0);
        // Melee creep tables are level bands over the creep races (ref has
        // no Jass2-native equivalent either; approximated from unit data).
        std::vector<uint32_t> candidates;
        for (const auto& [id, type] : unitData->GetAllTypes()) {
            if (!type || type->IsBuilding() || type->IsHero()) continue;
            if (type->GetLevel() != args[0].intVal) continue;
            const std::string& race = type->GetUnitRace().name;
            if (race == "creeps" || race == "demon" || race == "naga") {
                candidates.push_back(id.value);
            }
        }
        if (candidates.empty()) return JassValue::Int(0);
        return JassValue::Int((int)candidates[(size_t)(sim->SimRand01() * (float)candidates.size()) % candidates.size()]);
    });
    vm_.RegisterNative("ChooseRandomNPBuilding", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)args; (void)data;
        if (!sim) return JassValue::Int(0);
        const CUnitData* unitData = sim->GetUnitData();
        if (!unitData) return JassValue::Int(0);
        std::vector<uint32_t> candidates;
        for (const auto& [id, type] : unitData->GetAllTypes()) {
            if (!type || !type->IsBuilding()) continue;
            if (type->GetUnitRace().name == "other") {
                candidates.push_back(id.value);
            }
        }
        if (candidates.empty()) return JassValue::Int(0);
        return JassValue::Int((int)candidates[(size_t)(sim->SimRand01() * (float)candidates.size()) % candidates.size()]);
    });

    auto findShop = [data = data_, sim = sim_](int unitHandle) -> CAbilitySellItems* {
        if (!sim) return nullptr;
        int uSim = data->GetSimHandle(unitHandle, JassHandleInfo::UNIT);
        if (uSim < 0) return nullptr;
        CUnit* u = sim->GetUnit(uSim);
        if (!u) return nullptr;
        return dynamic_cast<CAbilitySellItems*>(u->GetAbility(War3ID("Asei")));
    };
    auto allShops = [data = data_, sim = sim_]() -> std::vector<CAbilitySellItems*> {
        std::vector<CAbilitySellItems*> out;
        if (!sim) return out;
        for (CUnit* u : sim->GetUnits()) {
            if (!u) continue;
            if (auto* shop = dynamic_cast<CAbilitySellItems*>(u->GetAbility(War3ID("Asei")))) {
                out.push_back(shop);
            }
        }
        return out;
    };
    vm_.RegisterNative("AddItemToStock", [findShop](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        if (auto* shop = findShop(args[0].intVal)) {
            shop->AddStockItem(War3ID((uint32_t)args[1].intVal), args[2].intVal, args[3].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("AddItemToAllStock", [data = data_, allShops](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        const uint32_t id = (uint32_t)args[0].intVal;
        data->globalShopStock.push_back({id, args[1].intVal, args[2].intVal, false});
        for (auto* shop : allShops()) shop->AddStockItem(War3ID(id), args[1].intVal, args[2].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("RemoveItemFromStock", [findShop](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto* shop = findShop(args[0].intVal)) {
            shop->RemoveStock(War3ID((uint32_t)args[1].intVal));
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("RemoveItemFromAllStock", [data = data_, allShops](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const uint32_t id = (uint32_t)args[0].intVal;
        auto& global = data->globalShopStock;
        global.erase(std::remove_if(global.begin(), global.end(), [&](const auto& e) {
            return e.id == id && !e.isUnit;
        }), global.end());
        for (auto* shop : allShops()) shop->RemoveStock(War3ID(id));
        return JassValue::Null();
    });
    vm_.RegisterNative("AddUnitToStock", [findShop](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        if (auto* shop = findShop(args[0].intVal)) {
            shop->AddStockUnit(War3ID((uint32_t)args[1].intVal), args[2].intVal, args[3].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("AddUnitToAllStock", [data = data_, allShops](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        const uint32_t id = (uint32_t)args[0].intVal;
        data->globalShopStock.push_back({id, args[1].intVal, args[2].intVal, true});
        for (auto* shop : allShops()) shop->AddStockUnit(War3ID(id), args[1].intVal, args[2].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("RemoveUnitFromStock", [findShop](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto* shop = findShop(args[0].intVal)) {
            shop->RemoveStock(War3ID((uint32_t)args[1].intVal));
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("RemoveUnitFromAllStock", [data = data_, allShops](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const uint32_t id = (uint32_t)args[0].intVal;
        auto& global = data->globalShopStock;
        global.erase(std::remove_if(global.begin(), global.end(), [&](const auto& e) {
            return e.id == id && e.isUnit;
        }), global.end());
        for (auto* shop : allShops()) shop->RemoveStock(War3ID(id));
        return JassValue::Null();
    });
    vm_.RegisterNative("AddResourceAmount", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        const int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        if (simId >= 0) {
            if (auto* destructable = sim->GetDestructable(simId)) {
                destructable->SetLife(*sim, destructable->GetLife() + (float)args[1].intVal);
            }
        }
        return JassValue::Null();
    });
}

// ============================================================
// 29. Unit-state natives
// ============================================================
// Retail type ordinals (common.j): attack 0 NORMAL,1 MELEE(fallback NORMAL),
// 2 PIERCE,3 SIEGE,4 MAGIC,5 CHAOS,6 HERO,7 SPELLS; damage uses the sparse
// ConvertDamageType ids (same table as the UnitDamageTarget path).
static CAttackType ToScriptAttackType(int ordinal) {
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

static CDamageType ToScriptDamageType(int ordinal) {
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
static bool BuffExMatches(const CBuff* buff, bool wantPositive, bool wantNegative, bool timedLifeOnly) {
    if (!buff) return false;
    if (timedLifeOnly && dynamic_cast<const CBuffTimedLife*>(buff) == nullptr) return false;
    if (buff->IsPositive() && !wantPositive) return false;
    if (buff->IsNegative() && !wantNegative) return false;
    return true;
}

void JassNatives::RegisterUnitStateNatives() {
    auto getUnit = [data = data_, sim = sim_](int unitHandle) -> CUnit* {
        if (!sim) return nullptr;
        int uSim = data->GetSimHandle(unitHandle, JassHandleInfo::UNIT);
        return uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
    };

    vm_.RegisterNative("UnitAddType", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        CUnit* u = getUnit(args[0].intVal);
        if (!u) return JassValue::Bool(false);
        u->AddUnitTypeFlag(args[1].intVal);
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("UnitRemoveType", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        CUnit* u = getUnit(args[0].intVal);
        if (!u) return JassValue::Bool(false);
        u->RemoveUnitTypeFlag(args[1].intVal);
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("UnitMakeAbilityPermanent", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Bool(false);
        CUnit* u = getUnit(args[0].intVal);
        if (!u) return JassValue::Bool(false);
        CAbility* ability = u->GetAbility(War3ID((uint32_t)args[2].intVal));
        if (!ability) return JassValue::Bool(false);
        ability->SetPermanent(args[1].boolVal);
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("UnitApplyTimedLife", [getUnit, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        CUnit* u = getUnit(args[0].intVal);
        if (!u) return JassValue::Null();
        u->AddBuff(std::make_unique<CBuffTimedLife>(War3ID((uint32_t)args[1].intVal), args[2].realVal), *sim);
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitPauseTimedLife", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        CUnit* u = getUnit(args[0].intVal);
        if (!u) return JassValue::Null();
        for (const auto& buff : u->GetBuffs()) {
            if (auto* timed = dynamic_cast<CBuffTimedLife*>(buff.get())) {
                timed->SetSuspended(args[1].boolVal);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitAddSleep", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (CUnit* u = getUnit(args[0].intVal)) u->SetCanSleep(args[1].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitAddSleepPerm", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (CUnit* u = getUnit(args[0].intVal)) u->SetCanSleepPerm(args[1].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitCanSleep", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        CUnit* u = getUnit(args[0].intVal);
        return JassValue::Bool(u && u->CanSleep());
    });
    vm_.RegisterNative("UnitCanSleepPerm", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        CUnit* u = getUnit(args[0].intVal);
        return JassValue::Bool(u && u->CanSleepPerm());
    });
    vm_.RegisterNative("UnitIsSleeping", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        CUnit* u = getUnit(args[0].intVal);
        return JassValue::Bool(u && u->IsAsleep());
    });
    vm_.RegisterNative("UnitWakeUp", [getUnit, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Null();
        if (CUnit* u = getUnit(args[0].intVal)) u->RemoveBuff(War3ID("BUsl"), *sim);
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitIgnoreAlarm", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        CUnit* u = getUnit(args[0].intVal);
        if (!u) return JassValue::Bool(false);
        u->SetIgnoreAlarm(args[1].boolVal);
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("UnitIgnoreAlarmToggled", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        CUnit* u = getUnit(args[0].intVal);
        return JassValue::Bool(u && u->IsAlarmIgnored());
    });
    vm_.RegisterNative("UnitShareVision", [data = data_, getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0 || !getUnit(args[0].intVal)) return JassValue::Null();
        // No per-player FoW consumer yet; retained for the vision pass.
        const int player = ResolvePlayerId(data, args[1].intVal);
        if (args[2].boolVal) {
            data->unitSharedVision[uSim].insert(player);
        } else {
            data->unitSharedVision[uSim].erase(player);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitSetConstructionProgress", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        CUnit* u = getUnit(args[0].intVal);
        if (!u) return JassValue::Null();
        // Retail takes percent; the field stores seconds (see behavior_build).
        float buildTime = 1.0f;
        if (u->GetUnitType() && u->GetUnitType()->GetBuildTime() > 0) {
            buildTime = (float)u->GetUnitType()->GetBuildTime();
        }
        u->SetConstructionProgress(args[1].intVal / 100.0f * buildTime);
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitSetUpgradeProgress", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (CUnit* u = getUnit(args[0].intVal)) u->SetUpgradeProgress((float)args[1].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitSetUsesAltIcon", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (CUnit* u = getUnit(args[0].intVal)) u->SetUsesAltIcon(args[1].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitCountBuffsEx", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 8) return JassValue::Int(0);
        CUnit* u = getUnit(args[0].intVal);
        if (!u) return JassValue::Int(0);
        int count = 0;
        for (const auto& buff : u->GetBuffs()) {
            if (BuffExMatches(buff.get(), args[1].boolVal, args[2].boolVal, args[5].boolVal)) count++;
        }
        return JassValue::Int(count);
    });
    vm_.RegisterNative("UnitHasBuffsEx", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 8) return JassValue::Bool(false);
        CUnit* u = getUnit(args[0].intVal);
        if (!u) return JassValue::Bool(false);
        for (const auto& buff : u->GetBuffs()) {
            if (BuffExMatches(buff.get(), args[1].boolVal, args[2].boolVal, args[5].boolVal)) return JassValue::Bool(true);
        }
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("UnitRemoveBuffs", [getUnit, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        if (CUnit* u = getUnit(args[0].intVal)) {
            u->RemoveBuffsIf(*sim, args[1].boolVal, args[2].boolVal, false);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitRemoveBuffsEx", [getUnit, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 8 || !sim) return JassValue::Null();
        if (CUnit* u = getUnit(args[0].intVal)) {
            u->RemoveBuffsIf(*sim, args[1].boolVal, args[2].boolVal, args[5].boolVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitDamagePoint", [data = data_, sim = sim_, getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 11 || !sim) return JassValue::Bool(false);
        CUnit* source = getUnit(args[0].intVal);
        const float delay = args[1].realVal;
        const float radius = args[2].realVal;
        const float x = args[3].realVal, y = args[4].realVal;
        const float amount = args[5].realVal;
        const bool isAttack = args[6].boolVal;
        const bool isRanged = args[7].boolVal;
        const CAttackType attackType = ToScriptAttackType(args[8].intVal);
        const CDamageType damageType = ToScriptDamageType(args[9].intVal);
        auto strike = [sim, source, radius, x, y, amount, isAttack, isRanged, attackType, damageType]() {
            for (CUnit* target : sim->GetUnits()) {
                if (!target || target->IsDead()) continue;
                const float dx = target->GetX() - x, dy = target->GetY() - y;
                if (dx * dx + dy * dy > radius * radius) continue;
                target->Damage(*sim, source, isAttack, isRanged, attackType, damageType, "", amount, 0);
            }
        };
        if (delay > 0.0f) {
            sim->AddDelayedAction(delay, std::move(strike));
        } else {
            strike();
        }
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("UnitAddIndicator", [data = data_, getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0 || !getUnit(args[0].intVal)) return JassValue::Null();
        // Retained for the minimap/renderer indicator pass.
        JassNativesData::JassUnitIndicator ind;
        ind.red = args[1].intVal; ind.green = args[2].intVal;
        ind.blue = args[3].intVal; ind.alpha = args[4].intVal;
        data->unitIndicators[uSim] = ind;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitFog", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        data->globalUnitFog.a = args[0].realVal;
        data->globalUnitFog.b = args[1].realVal;
        data->globalUnitFog.c = args[2].realVal;
        data->globalUnitFog.d = args[3].realVal;
        data->globalUnitFog.e = args[4].realVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("WaygateIsActive", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        CUnit* u = getUnit(args[0].intVal);
        return JassValue::Bool(u && u->IsWaygateActive());
    });
    vm_.RegisterNative("CachePlayerHeroData", [](const std::vector<JassValue>&) -> JassValue {
        // Hero persistence needs the save system, which does not exist yet.
        return JassValue::Null();
    });
}

// ============================================================
// 30. Camera + game-flow natives
// ============================================================
void JassNatives::RegisterCameraFlowNatives() {
    vm_.RegisterNative("CameraSetSourceNoise", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        // Retained for the renderer shake pass; no consumer yet.
        data->sourceNoise.magnitude = args[0].realVal;
        data->sourceNoise.velocity = args[1].realVal;
        data->sourceNoise.vertOnly = false;
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetSourceNoiseEx", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        data->sourceNoise.magnitude = args[0].realVal;
        data->sourceNoise.velocity = args[1].realVal;
        data->sourceNoise.vertOnly = args[2].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetTargetNoise", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        data->targetNoise.magnitude = args[0].realVal;
        data->targetNoise.velocity = args[1].realVal;
        data->targetNoise.vertOnly = false;
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetTargetNoiseEx", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        data->targetNoise.magnitude = args[0].realVal;
        data->targetNoise.velocity = args[1].realVal;
        data->targetNoise.vertOnly = args[2].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("PanCameraToTimedWithZ", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        // Duration is renderer-side; the target callback takes the final pose.
        if (args.size() >= 3 && data->camSetTarget)
            data->camSetTarget(args[0].realVal, args[2].realVal, args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("PanCameraToWithZ", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 3 && data->camSetTarget)
            data->camSetTarget(args[0].realVal, args[2].realVal, args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCameraQuickPosition", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 2 && data->camSetPosition)
            data->camSetPosition(args[0].realVal, 0, args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCameraRotateMode", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        // Retained for the renderer orbit pass; no consumer yet.
        data->rotateMode.active = true;
        data->rotateMode.x = args[0].realVal;
        data->rotateMode.y = args[1].realVal;
        data->rotateMode.radians = args[2].realVal;
        data->rotateMode.duration = args[3].realVal;
        return JassValue::Null();
    });
    auto setController = [data = data_, sim = sim_](bool orient, const std::vector<JassValue>& args) -> JassValue {
        const size_t need = orient ? 3 : 4;
        if (args.size() < need || !sim) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        JassNativesData::JassCameraController ctl;
        if (uSim >= 0 && sim->GetUnit(uSim)) {
            ctl.unitSimId = uSim;
            ctl.xOffset = args[1].realVal;
            ctl.yOffset = args[2].realVal;
            ctl.inheritOrientation = !orient && args[3].boolVal;
            ctl.active = true;
        }
        if (orient) {
            data->orientController = ctl;
        } else {
            data->targetController = ctl;
        }
        return JassValue::Null();
    };
    vm_.RegisterNative("SetCameraTargetController", [setController](const std::vector<JassValue>& args) -> JassValue {
        return setController(false, args);
    });
    vm_.RegisterNative("SetCameraOrientController", [setController](const std::vector<JassValue>& args) -> JassValue {
        return setController(true, args);
    });

    vm_.RegisterNative("PauseGame", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (!args.empty() && sim) sim->SetPaused(args[0].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("PauseCompAI", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        // No AI runtime exists; retained per player for the future AI pass.
        data->compAIpaused[ResolvePlayerId(data, args[0].intVal)] = args[1].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("GetAIDifficulty", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        (void)args;
        // No per-player AI exists; the lobby difficulty applies to everyone.
        return JassValue::Int(data ? data->gameConfig.defaultDifficulty : 1);
    });
    vm_.RegisterNative("EndGame", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (data) {
            data->gameEnded = true;
            if (!args.empty()) data->gameVictory = args[0].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("CripplePlayer", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        const int player = ResolvePlayerId(data, args[0].intVal);
        if (args[2].boolVal) {
            data->crippledPlayers.insert(player);
        } else {
            data->crippledPlayers.erase(player);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsNoVictoryCheat", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(data && data->noVictoryCheat);
    });
    vm_.RegisterNative("IsNoDefeatCheat", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(data && data->noDefeatCheat);
    });
    vm_.RegisterNative("Cheat", [](const std::vector<JassValue>&) -> JassValue {
        // Single-player cheat codes are not implemented.
        return JassValue::Null();
    });
    vm_.RegisterNative("DisableRestartMission", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->restartDisabled = args[0].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("RemovePlayer", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        const int player = ResolvePlayerId(data, args[0].intVal);
        // Sim player slots are index-stable, so removal only closes the
        // lobby slot; the sim unit stays addressable (documented).
        if (player >= 0 && player < (int)data->playerMapConfig.size()) {
            data->playerMapConfig[(size_t)player].slotActive = false;
        }
        data->removedPlayerResults[player] = args[1].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPlayerUnitsOwner", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        const int oldOwner = ResolvePlayerId(data, args[0].intVal);
        const int newOwner = args[1].intVal;
        CPlayer* from = sim->GetPlayer(oldOwner);
        CPlayer* to = sim->GetPlayer(newOwner);
        if (!from) return JassValue::Null();
        std::vector<CUnit*> moving;
        for (CUnit* u : sim->GetUnits()) {
            if (u && u->GetPlayerIndex() == oldOwner) moving.push_back(u);
        }
        for (CUnit* u : moving) {
            from->RemoveUnit(u);
            u->SetPlayerIndex(newOwner);
            if (to) to->AddUnit(u);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetGameTypeSupported", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (args[1].boolVal) {
            data->supportedGameTypes.insert(args[0].intVal);
        } else {
            data->supportedGameTypes.erase(args[0].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsGameTypeSupported", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        return JassValue::Bool(data && data->supportedGameTypes.count(args[0].intVal) > 0);
    });
    vm_.RegisterNative("SetResourceDensity", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->resourceDensity = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCreatureDensity", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->creatureDensity = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("VersionCompatible", [](const std::vector<JassValue>&) -> JassValue {
        // No version-gated behavior exists; all script versions run.
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("VersionSupported", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("PlayCinematic", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (data) {
            data->cinematic.playing = true;
            if (!args.empty()) data->cinematic.movie = args[0].strVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCinematicScene", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6) return JassValue::Null();
        if (data) {
            data->cinematic.playing = true;
            data->cinematic.portraitUnitId = args[0].intVal;
            data->cinematic.portraitColor = args[1].intVal;
            data->cinematic.speakerTitle = args[2].strVal;
            data->cinematic.text = args[3].strVal;
            data->cinematic.sceneDuration = args[4].realVal;
            data->cinematic.voiceDuration = args[5].realVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("EndCinematicScene", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (data) data->cinematic.playing = false;
        return JassValue::Null();
    });
    vm_.RegisterNative("ForceCinematicSubtitles", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->cinematic.subtitles = args[0].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetIntroShotModel", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->cinematic.introModel = args[0].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetIntroShotText", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->cinematic.introText = args[0].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("DisplayLoadDialog", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (data) data->cinematic.loadDialog = true;
        return JassValue::Null();
    });
    // Preload calls are asset-cache hints; loading here is synchronous so
    // they complete immediately with nothing to do.
    vm_.RegisterNative("Preload", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PreloadEnd", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PreloadEndEx", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PreloadGenClear", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PreloadGenStart", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PreloadGenEnd", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PreloadRefresh", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PreloadStart", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("Preloader", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
}

// ============================================================
// 31. Trigger extras + sound state + counted group enums
// ============================================================
void JassNatives::RegisterTriggerSoundNatives() {
    vm_.RegisterNative("TriggerRegisterUnitStateEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        // Fired by the trigger state poll pass once wired; retained so the
        // registration itself is never lost.
        JassTrigger::JassUnitStateEvent ev;
        ev.unitHandle = args[1].intVal;
        ev.state = args[2].intVal;
        ev.opcode = args[3].intVal;
        ev.limit = args[4].realVal;
        it->second->unitStateEvents.push_back(ev);
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterFilterUnitEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        it->second->registeredUnitHandles.push_back(args[1].intVal);
        it->second->eventTypes.push_back(args[2].intVal);
        it->second->registeredEvents.emplace_back(args[1].intVal, args[2].intVal);
        // The filter runs as an extra condition (ref filtered unit events).
        if (args[3].type == JassValue::STRING && !args[3].strVal.empty()) {
            it->second->conditionFuncNames.push_back(args[3].strVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterDialogEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        // Fired from the dialog UI on display; stored for that pass.
        it->second->registeredEvents.emplace_back(args[1].intVal, 4201);
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterDialogButtonEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        it->second->registeredEvents.emplace_back(args[1].intVal, 4202);
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRemoveAction", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        // TriggerAddAction returns the action index as its handle.
        const int index = args[1].intVal;
        auto& actions = it->second->actionFuncNames;
        if (index >= 0 && index < (int)actions.size()) {
            actions.erase(actions.begin() + index);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ResetTrigger", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        it->second->evalCount = 0;
        it->second->execCount = 0;
        it->second->enabled = true;
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerWaitForSound", [data = data_, vm = &vm_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        const int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        float seconds = 0.0f;
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) {
                seconds = it->second->durationMs / 1000.0f - args[1].realVal;
            }
        }
        if (seconds > 0.0f) vm->RequestSleep(seconds);
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerWaitOnSleeps", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->triggerWaitOnSleeps = args[0].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("IsTriggerWaitOnSleeps", [data = data_](const std::vector<JassValue>&) -> JassValue {
        // The executor always waits today; false is recorded for the pass
        // that will honor it (documented limitation).
        return JassValue::Bool(!data || data->triggerWaitOnSleeps);
    });
    vm_.RegisterNative("TriggerSyncStart", [data = data_](const std::vector<JassValue>&) -> JassValue {
        // Sync needs the net protocol; stays unready (documented).
        if (data) data->syncReady = false;
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerSyncReady", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(data && data->syncReady);
    });

    auto getSound = [data = data_](int soundHandle) -> std::shared_ptr<JassSound> {
        int id = data->GetSimHandle(soundHandle, JassHandleInfo::SOUND);
        if (id < 0) return nullptr;
        auto it = data->sounds.find(id);
        return it != data->sounds.end() ? it->second : nullptr;
    };
    vm_.RegisterNative("GetSoundFileDuration", [](const std::vector<JassValue>&) -> JassValue {
        // Needs the decode pipeline to stat the file; no hook exists yet.
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetSoundIsPlaying", [getSound](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        auto s = getSound(args[0].intVal);
        return JassValue::Bool(s && s->playing);
    });
    vm_.RegisterNative("GetSoundIsLoading", [](const std::vector<JassValue>&) -> JassValue {
        // Loads are synchronous; nothing is ever in flight.
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("CreateSoundFilenameWithLabel", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 7) return JassValue::Int(0);
        auto s = std::make_shared<JassSound>();
        s->file = args[0].strVal;
        s->label = args[6].strVal;
        s->looping = args[1].boolVal;
        int id = data->nextSoundId++;
        data->sounds[id] = s;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::SOUND, id));
    });
    vm_.RegisterNative("SetSoundConeAngles", [getSound](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        if (auto s = getSound(args[0].intVal)) {
            s->coneInside = args[1].realVal;
            s->coneOutside = args[2].realVal;
            s->coneOutsideVolume = args[3].intVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundConeOrientation", [getSound](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        if (auto s = getSound(args[0].intVal)) {
            s->orientX = args[1].realVal;
            s->orientY = args[2].realVal;
            s->orientZ = args[3].realVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundPlayPosition", [getSound](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto s = getSound(args[0].intVal)) s->playPositionMs = args[1].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundVelocity", [getSound](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        if (auto s = getSound(args[0].intVal)) {
            s->velX = args[1].realVal;
            s->velY = args[2].realVal;
            s->velZ = args[3].realVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("UnregisterStackedSound", [getSound](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        if (auto s = getSound(args[0].intVal)) {
            s->stackedByPosition = false;
            s->stackedWidth = 0.0f;
            s->stackedHeight = 0.0f;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("VolumeGroupSetVolume", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        // Consumed by the audio pass on play (ref volume groups).
        data->volumeGroups[args[0].intVal] = (int)(args[1].realVal * 127.0f);
        return JassValue::Null();
    });

    vm_.RegisterNative("SetTextTagVelocity", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id < 0) return JassValue::Null();
        auto it = data->textTags.find(id);
        if (it == data->textTags.end()) return JassValue::Null();
        it->second->vx = args[1].realVal;
        it->second->vy = args[2].realVal;
        return JassValue::Null();
    });

    // Counted group enums: same sweep as the uncounted twins, capped at
    // countLimit matches (non-positive limit means uncapped).
    auto filterAt = [](const std::vector<JassValue>& args, size_t index) -> std::string {
        return (args.size() > index && args[index].type == JassValue::STRING) ? args[index].strVal : "";
    };
    auto acceptUnit = [data = data_, vmPtr = vmPtr_](int unitHandle, const std::string& filterFunc) {
        if (filterFunc.empty() || !vmPtr) return true;
        const int previous = data->filterUnitHandle;
        data->filterUnitHandle = unitHandle;
        const bool accepted = JassValueToBool(CallSyncFunction(vmPtr, filterFunc));
        data->filterUnitHandle = previous;
        return accepted;
    };
    auto groupOf = [data = data_](int groupHandle) -> std::shared_ptr<JassGroup> {
        int simId = data->GetSimHandle(groupHandle, JassHandleInfo::GROUP);
        if (simId < 0) return nullptr;
        auto it = data->groups.find(simId);
        return it != data->groups.end() ? it->second : nullptr;
    };
    auto cappedPush = [](std::vector<int>& out, int handle, int limit) -> bool {
        if (limit > 0 && (int)out.size() >= limit) return false;
        out.push_back(handle);
        return true;
    };
    vm_.RegisterNative("GroupEnumUnitsInRangeCounted", [data = data_, sim = sim_, groupOf, filterAt, acceptUnit, cappedPush](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6) return JassValue::Null();
        auto group = groupOf(args[0].intVal);
        if (!group || !sim) return JassValue::Null();
        const float x = args[1].realVal, y = args[2].realVal, rSq = args[3].realVal * args[3].realVal;
        const std::string filter = filterAt(args, 4);
        const int limit = args[5].intVal;
        group->unitHandles.clear();
        for (auto* unit : sim->GetUnits()) {
            const float dx = unit->GetX() - x, dy = unit->GetY() - y;
            if (dx * dx + dy * dy > rSq) continue;
            const int uh = data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
            if (acceptUnit(uh, filter) && !cappedPush(group->unitHandles, uh, limit)) break;
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupEnumUnitsInRangeOfLocCounted", [data = data_, sim = sim_, groupOf, filterAt, acceptUnit, cappedPush](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        auto group = groupOf(args[0].intVal);
        if (!group || !sim) return JassValue::Null();
        int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (locSim < 0) return JassValue::Null();
        auto lit = data->locations.find(locSim);
        if (lit == data->locations.end()) return JassValue::Null();
        const float x = lit->second->x, y = lit->second->y, rSq = args[2].realVal * args[2].realVal;
        const std::string filter = filterAt(args, 3);
        const int limit = args[4].intVal;
        group->unitHandles.clear();
        for (auto* unit : sim->GetUnits()) {
            const float dx = unit->GetX() - x, dy = unit->GetY() - y;
            if (dx * dx + dy * dy > rSq) continue;
            const int uh = data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
            if (acceptUnit(uh, filter) && !cappedPush(group->unitHandles, uh, limit)) break;
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupEnumUnitsInRectCounted", [data = data_, sim = sim_, groupOf, filterAt, acceptUnit, cappedPush](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        auto group = groupOf(args[0].intVal);
        if (!group || !sim) return JassValue::Null();
        int rectSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::RECT);
        if (rectSim < 0) return JassValue::Null();
        auto rit = data->rects.find(rectSim);
        if (rit == data->rects.end()) return JassValue::Null();
        const JassRect& r = *rit->second;
        const std::string filter = filterAt(args, 2);
        const int limit = args[3].intVal;
        group->unitHandles.clear();
        for (auto* unit : sim->GetUnits()) {
            const float ux = unit->GetX(), uy = unit->GetY();
            if (ux < r.minX || ux > r.maxX || uy < r.minY || uy > r.maxY) continue;
            const int uh = data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
            if (acceptUnit(uh, filter) && !cappedPush(group->unitHandles, uh, limit)) break;
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupEnumUnitsOfTypeCounted", [data = data_, sim = sim_, groupOf, filterAt, acceptUnit, cappedPush](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        auto group = groupOf(args[0].intVal);
        if (!group || !sim) return JassValue::Null();
        const std::string unitName = args[1].type == JassValue::STRING ? args[1].strVal : "";
        const War3ID rawcodeType((uint32_t)args[1].intVal);
        const std::string filter = filterAt(args, 2);
        const int limit = args[3].intVal;
        group->unitHandles.clear();
        for (auto* unit : sim->GetUnits()) {
            const CUnitType* unitType = unit ? unit->GetUnitType() : nullptr;
            const bool typeMatches = unitType
                ? (!unitName.empty() ? unitType->GetLegacyName() == unitName
                                     : unit->GetTypeId() == rawcodeType)
                : false;
            if (!typeMatches) continue;
            const int uh = data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
            if (acceptUnit(uh, filter) && !cappedPush(group->unitHandles, uh, limit)) break;
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
}

// ============================================================
// 32. Leaderboard / multiboard / timer-dialog setters
// ============================================================
// Retained script-side board state for the UI board pass; no on-screen
// consumer yet (documented per native where relevant).
void JassNatives::RegisterBoardNatives() {
    auto getBoard = [data = data_](int boardHandle) -> std::shared_ptr<JassNativesData::JassLeaderboard> {
        int simId = data->GetSimHandle(boardHandle, JassHandleInfo::LEADERBOARD);
        if (simId < 0) return nullptr;
        auto it = data->leaderboards.find(simId);
        return it != data->leaderboards.end() ? it->second : nullptr;
    };
    auto boardItem = [](const std::shared_ptr<JassNativesData::JassLeaderboard>& board, int index)
        -> JassNativesData::JassLeaderboardItem* {
        if (!board || index < 0 || index >= (int)board->items.size()) return nullptr;
        return &board->items[(size_t)index];
    };
    vm_.RegisterNative("LeaderboardGetLabelText", [getBoard](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("");
        auto board = getBoard(args[0].intVal);
        return JassValue::Str(board ? board->label : "");
    });
    vm_.RegisterNative("LeaderboardRemoveItem", [getBoard](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto board = getBoard(args[0].intVal)) {
            const int index = args[1].intVal;
            if (index >= 0 && index < (int)board->items.size()) {
                board->items.erase(board->items.begin() + index);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("LeaderboardSetItemLabel", [getBoard, boardItem](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        if (auto* item = boardItem(getBoard(args[0].intVal), args[1].intVal)) {
            item->label = args[2].strVal;
        }
        return JassValue::Null();
    });
    auto itemColor = [getBoard, boardItem](const std::vector<JassValue>& args, bool label) -> JassValue {
        if (args.size() < 6) return JassValue::Null();
        if (auto* item = boardItem(getBoard(args[0].intVal), args[1].intVal)) {
            int* color = label ? item->labelColor : item->valueColor;
            color[0] = args[2].intVal; color[1] = args[3].intVal;
            color[2] = args[4].intVal; color[3] = args[5].intVal;
        }
        return JassValue::Null();
    };
    vm_.RegisterNative("LeaderboardSetItemLabelColor", [itemColor](const std::vector<JassValue>& args) -> JassValue {
        return itemColor(args, true);
    });
    vm_.RegisterNative("LeaderboardSetItemValueColor", [itemColor](const std::vector<JassValue>& args) -> JassValue {
        return itemColor(args, false);
    });
    vm_.RegisterNative("LeaderboardSetItemStyle", [getBoard, boardItem](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        if (auto* item = boardItem(getBoard(args[0].intVal), args[1].intVal)) {
            item->showLabel = args[2].boolVal;
            item->showValue = args[3].boolVal;
            item->showIcon = args[4].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("LeaderboardSetLabelColor", [getBoard](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        if (auto board = getBoard(args[0].intVal)) {
            board->labelColor[0] = args[1].intVal; board->labelColor[1] = args[2].intVal;
            board->labelColor[2] = args[3].intVal; board->labelColor[3] = args[4].intVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("LeaderboardSetValueColor", [getBoard](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        if (auto board = getBoard(args[0].intVal)) {
            board->valueColor[0] = args[1].intVal; board->valueColor[1] = args[2].intVal;
            board->valueColor[2] = args[3].intVal; board->valueColor[3] = args[4].intVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("LeaderboardSetSizeByItemCount", [getBoard](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto board = getBoard(args[0].intVal)) board->sizeByItemCount = args[1].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("LeaderboardSetStyle", [getBoard](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        if (auto board = getBoard(args[0].intVal)) {
            board->showLabel = args[1].boolVal;
            board->showNames = args[2].boolVal;
            board->showValues = args[3].boolVal;
            board->showIcons = args[4].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("LeaderboardSortItemsByValue", [getBoard](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto board = getBoard(args[0].intVal)) {
            const bool asc = args[1].boolVal;
            std::stable_sort(board->items.begin(), board->items.end(),
                [asc](const auto& a, const auto& b) { return asc ? a.value < b.value : a.value > b.value; });
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("LeaderboardSortItemsByPlayer", [getBoard](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto board = getBoard(args[0].intVal)) {
            const bool asc = args[1].boolVal;
            std::stable_sort(board->items.begin(), board->items.end(),
                [asc](const auto& a, const auto& b) { return asc ? a.playerId < b.playerId : a.playerId > b.playerId; });
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("LeaderboardSortItemsByLabel", [getBoard](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto board = getBoard(args[0].intVal)) {
            const bool asc = args[1].boolVal;
            std::stable_sort(board->items.begin(), board->items.end(),
                [asc](const auto& a, const auto& b) { return asc ? a.label < b.label : a.label > b.label; });
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("PlayerSetLeaderboard", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        const int player = ResolvePlayerId(data, args[0].intVal);
        const int boardSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LEADERBOARD);
        if (player >= 0 && boardSim >= 0 && data->leaderboards.count(boardSim)) {
            data->playerLeaderboards[player] = boardSim;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("PlayerGetLeaderboard", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const int player = ResolvePlayerId(data, args[0].intVal);
        auto it = data->playerLeaderboards.find(player);
        if (it == data->playerLeaderboards.end()) return JassValue::Null();
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LEADERBOARD, it->second));
    });

    auto getMultiboard = [data = data_](int boardHandle) -> std::shared_ptr<JassNativesData::JassMultiboard> {
        int simId = data->GetSimHandle(boardHandle, JassHandleInfo::MULTIBOARD);
        if (simId < 0) return nullptr;
        auto it = data->multiboards.find(simId);
        return it != data->multiboards.end() ? it->second : nullptr;
    };
    vm_.RegisterNative("MultiboardSetTitleTextColor", [getMultiboard](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        if (auto board = getMultiboard(args[0].intVal)) {
            board->titleColor[0] = args[1].intVal; board->titleColor[1] = args[2].intVal;
            board->titleColor[2] = args[3].intVal; board->titleColor[3] = args[4].intVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardSetItemsStyle", [getMultiboard, data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        if (auto board = getMultiboard(args[0].intVal)) {
            for (const auto& [cell, itemId] : board->items) {
                auto it = data->multiboardItems.find(itemId);
                if (it == data->multiboardItems.end()) continue;
                it->second->showValue = args[1].boolVal;
                it->second->showIcon = args[2].boolVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardSetItemsValue", [getMultiboard, data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto board = getMultiboard(args[0].intVal)) {
            for (const auto& [cell, itemId] : board->items) {
                auto it = data->multiboardItems.find(itemId);
                if (it != data->multiboardItems.end()) it->second->value = args[1].strVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardSetItemsValueColor", [getMultiboard](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        if (auto board = getMultiboard(args[0].intVal)) {
            // No per-item color fields exist yet; the broadcast color is
            // recorded on the board title color as the visible proxy until
            // the board item pass lands (documented).
            board->titleColor[0] = args[1].intVal; board->titleColor[1] = args[2].intVal;
            board->titleColor[2] = args[3].intVal; board->titleColor[3] = args[4].intVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardSetItemsWidth", [getMultiboard, data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto board = getMultiboard(args[0].intVal)) {
            for (const auto& [cell, itemId] : board->items) {
                auto it = data->multiboardItems.find(itemId);
                if (it != data->multiboardItems.end()) it->second->width = args[1].realVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardSetItemsIcon", [getMultiboard, data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto board = getMultiboard(args[0].intVal)) {
            for (const auto& [cell, itemId] : board->items) {
                auto it = data->multiboardItems.find(itemId);
                if (it != data->multiboardItems.end()) it->second->icon = args[1].strVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MultiboardSetItemValueColor", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6) return JassValue::Null();
        // Per-item colors land with the board item pass; validated handles
        // keep the call safe until then (documented).
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARDITEM);
        if (simId < 0 || !data->multiboardItems.count(simId)) return JassValue::Null();
        return JassValue::Null();
    });

    auto getTimerDialog = [data = data_](int dialogHandle) -> std::shared_ptr<JassTimerDialog> {
        int simId = data->GetSimHandle(dialogHandle, JassHandleInfo::TIMER_DIALOG);
        if (simId < 0) return nullptr;
        auto it = data->timerDialogs.find(simId);
        return it != data->timerDialogs.end() ? it->second : nullptr;
    };
    vm_.RegisterNative("TimerDialogSetTitleColor", [getTimerDialog](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        if (auto dialog = getTimerDialog(args[0].intVal)) {
            dialog->titleColor[0] = args[1].intVal; dialog->titleColor[1] = args[2].intVal;
            dialog->titleColor[2] = args[3].intVal; dialog->titleColor[3] = args[4].intVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TimerDialogSetTimeColor", [getTimerDialog](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        if (auto dialog = getTimerDialog(args[0].intVal)) {
            dialog->timeColor[0] = args[1].intVal; dialog->timeColor[1] = args[2].intVal;
            dialog->timeColor[2] = args[3].intVal; dialog->timeColor[3] = args[4].intVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TimerDialogSetSpeed", [getTimerDialog](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto dialog = getTimerDialog(args[0].intVal)) dialog->speed = args[1].realVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("IsTimerDialogDisplayed", [getTimerDialog](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        auto dialog = getTimerDialog(args[0].intVal);
        return JassValue::Bool(dialog && dialog->displayed);
    });
}

// ============================================================
// 33. UI-state natives: input toggles, filters, pings, defeat
// ============================================================
// These retain script-visible UI state for the UI/renderer passes; none
// take effect without their consumer yet (documented per native).
void JassNatives::RegisterUIStateNatives() {
    auto uiFlag = [data = data_](const std::vector<JassValue>& args, bool JassNativesData::JassUIConfig::*field, size_t valueIndex = 0) -> JassValue {
        if (args.size() <= valueIndex || !data) return JassValue::Null();
        data->uiConfig.*field = args[valueIndex].boolVal;
        return JassValue::Null();
    };
    vm_.RegisterNative("EnableDragSelect", [uiFlag](const std::vector<JassValue>& args) -> JassValue {
        return uiFlag(args, &JassNativesData::JassUIConfig::dragSelect, 0);
    });
    vm_.RegisterNative("EnablePreSelect", [uiFlag](const std::vector<JassValue>& args) -> JassValue {
        return uiFlag(args, &JassNativesData::JassUIConfig::preSelect, 0);
    });
    vm_.RegisterNative("EnableSelect", [uiFlag](const std::vector<JassValue>& args) -> JassValue {
        return uiFlag(args, &JassNativesData::JassUIConfig::select, 0);
    });
    vm_.RegisterNative("EnableUserControl", [uiFlag](const std::vector<JassValue>& args) -> JassValue {
        return uiFlag(args, &JassNativesData::JassUIConfig::userControl, 0);
    });
    vm_.RegisterNative("EnableOcclusion", [uiFlag](const std::vector<JassValue>& args) -> JassValue {
        return uiFlag(args, &JassNativesData::JassUIConfig::occlusion, 0);
    });
    vm_.RegisterNative("EnableWorldFogBoundary", [uiFlag](const std::vector<JassValue>& args) -> JassValue {
        return uiFlag(args, &JassNativesData::JassUIConfig::worldFogBoundary, 0);
    });
    vm_.RegisterNative("ShowInterface", [uiFlag](const std::vector<JassValue>& args) -> JassValue {
        return uiFlag(args, &JassNativesData::JassUIConfig::showInterface, 0);
    });
    vm_.RegisterNative("EnableMinimapFilterButtons", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        data->uiConfig.minimapAllyFilter = args[0].boolVal;
        data->uiConfig.minimapCreepFilter = args[1].boolVal;
        data->uiConfig.minimapFilterButtons = args[0].boolVal || args[1].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("GetAllyColorFilterState", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data && data->allyColorFilter ? 1 : 0);
    });
    vm_.RegisterNative("GetCreepCampFilterState", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(data && data->creepCampFilter);
    });
    vm_.RegisterNative("SetCreepCampFilterState", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->creepCampFilter = args[0].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("ForceUICancel", [](const std::vector<JassValue>&) -> JassValue {
        // Needs menu-focus state, which no UI pass owns yet.
        return JassValue::Null();
    });
    vm_.RegisterNative("ForceUIKey", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Null();
    });
    vm_.RegisterNative("PingMinimap", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        JassNativesData::JassMinimapPing ping;
        ping.x = args[0].realVal;
        ping.y = args[1].realVal;
        ping.duration = args[2].realVal;
        data->minimapPings.push_back(ping);
        return JassValue::Null();
    });
    vm_.RegisterNative("PingMinimapEx", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 7) return JassValue::Null();
        JassNativesData::JassMinimapPing ping;
        ping.x = args[0].realVal;
        ping.y = args[1].realVal;
        ping.duration = args[2].realVal;
        ping.red = args[3].intVal;
        ping.green = args[4].intVal;
        ping.blue = args[5].intVal;
        ping.extra = args[6].boolVal;
        data->minimapPings.push_back(ping);
        return JassValue::Null();
    });
    vm_.RegisterNative("IsCineFilterDisplayed", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(data && data->cineFilter.displayed);
    });
    vm_.RegisterNative("CreateDefeatCondition", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto dc = std::make_shared<JassNativesData::JassDefeatCondition>();
        int id = data->nextDefeatConditionId++;
        data->defeatConditions[id] = dc;
        return JassValue::Int(id);
    });
    vm_.RegisterNative("DefeatConditionSetDescription", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto it = data->defeatConditions.find(args[0].intVal);
        if (it != data->defeatConditions.end()) it->second->description = args[1].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("DestroyDefeatCondition", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty()) data->defeatConditions.erase(args[0].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("DestroyFilter", [](const std::vector<JassValue>&) -> JassValue {
        // Filter() is passthrough (no allocation), so nothing to free.
        return JassValue::Null();
    });
    vm_.RegisterNative("DestroyCondition", [](const std::vector<JassValue>&) -> JassValue {
        // Condition() is passthrough (no allocation), so nothing to free.
        return JassValue::Null();
    });
    vm_.RegisterNative("AddIndicator", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        // Widget may be a unit, item or destructable; first hit wins.
        static const JassHandleInfo::Type kinds[] = {
            JassHandleInfo::UNIT, JassHandleInfo::ITEM, JassHandleInfo::DESTRUCTABLE,
        };
        for (auto kind : kinds) {
            const int simId = data->GetSimHandle(args[0].intVal, kind);
            if (simId < 0) continue;
            JassNativesData::JassUnitIndicator ind;
            ind.red = args[1].intVal;
            ind.green = args[2].intVal;
            ind.blue = args[3].intVal;
            ind.alpha = args[4].intVal;
            data->unitIndicators[simId] = ind;
            return JassValue::Null();
        }
        return JassValue::Null();
    });
}
