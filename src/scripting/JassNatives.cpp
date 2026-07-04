#include "JassNatives.h"
#include "scripting/jass_animation_tags.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "audio/AudioSystem.h"
#include "ecs/player.h"
#include "ecs/item.h"
#include "ecs/abilities/ability_inventory.h"
#include "ecs/ability_factory.h"
#include "ecs/destructable.h"
#include "ecs/orders/order.h"
#include "ecs/pathing/world_collision.h"
#include "data/unitdata.h"
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
    queuedThreads_.push_back({functionName, context});
}

int JassNatives::PumpQueuedThreads(int maxCount) {
    int pumped = 0;
    while (!queuedThreads_.empty() && pumped < maxCount) {
        auto thread = std::move(queuedThreads_.front());
        queuedThreads_.pop_front();

        JassQueuedContext previous = CaptureContext();
        ApplyContext(thread.context);
        vm_.Execute(thread.functionName);
        ApplyContext(previous);
        pumped++;
    }
    if (!queuedThreads_.empty()) {
        std::cout << "JASS queue: pump limit reached, " << queuedThreads_.size()
                  << " threads remain" << std::endl;
    }
    return pumped;
}

bool JassNatives::EvaluateTriggerConditions(const JassTrigger& trigger, const JassQueuedContext& context,
                                            bool requireEnabled) {
    if (requireEnabled && !trigger.enabled) {
        return false;
    }
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
        JassValue result = vm_.CallFunction(conditionName);
        ApplyContext(previous);
        if (!JassValueToBool(result)) {
            return false;
        }
    }
    return true;
}

void JassNatives::QueueTriggerActions(const JassTrigger& trigger, const JassQueuedContext& context) {
    if (!EvaluateTriggerConditions(trigger, context)) {
        return;
    }
    for (const auto& funcName : trigger.actionFuncNames) {
        QueueFunctionWithContext(funcName, context);
    }
}

void JassNatives::FireUnitEvent(int unitSimHandle, int eventType) {
    for (auto& [id, trigger] : data_->triggers) {
        for (size_t i = 0; i < trigger->registeredUnitHandles.size(); i++) {
            int regUnitHandle = trigger->registeredUnitHandles[i];
            int regEventType = trigger->eventTypes[i];
            if (regEventType != eventType) continue;

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
                context.currentTriggerEventId = eventType;
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

    FireUnitEvent(unitSimHandle, eventType);

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
    if (targetSimHandle >= 0) {
        eventType = JASS_EVENT_UNIT_ISSUED_TARGET_ORDER;
    } else if (x != 0.0f || y != 0.0f) {
        eventType = JASS_EVENT_UNIT_ISSUED_POINT_ORDER;
    }

    FireUnitEvent(unitSimHandle, eventType);

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

    FireUnitEvent(unitSimHandle, eventType);

    data_->eventManipulatingUnitHandle = -1;
    data_->eventManipulatedItemHandle = -1;
}

void JassNatives::ForgetItem(int itemSimHandle) {
    if (itemSimHandle < 0) {
        return;
    }
    data_->items.erase(itemSimHandle);
}

int JassNatives::UpdateTimers(float dt) {
    int firedCount = 0;
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
    vm_.RegisterNative("CreateTimerDialog", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("DestroyTimerDialog", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("TimerDialogDisplay", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("TimerDialogSetTitle", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("TimerDialogSetRealTimeRemaining", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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
        const bool accepted = JassValueToBool(vmPtr->CallFunction(filterFunc));
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
        for (int uh : git->second->unitHandles) {
            data->enumUnitHandle = uh;
            if (vmPtr) vmPtr->Execute(funcName);
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
        const bool accepted = JassValueToBool(vmPtr->CallFunction(filterFunc));
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
        for (int playerId : fit->second->playerIds) {
            data->enumPlayerHandle = data->AllocHandle(JassHandleInfo::PLAYER, playerId);
            if (vmPtr) vmPtr->Execute(funcName);
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
    vm_.RegisterNative("GetLocationZ", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
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
        float ux = unit->GetX(), uy = unit->GetY();
        for (auto& r : rit->second->rects)
            if (ux >= r.minX && ux <= r.maxX && uy >= r.minY && uy <= r.maxY)
                return JassValue::Bool(true);
        return JassValue::Bool(false);
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
                auto* combat = sim->GetCombatSystem();
                if (combat) {
                    auto* atkType = src->GetUnitType();
                    int dice = 0, sides = 0;
                    if (atkType && !atkType->GetAttacks().empty()) {
                        dice = atkType->GetAttacks()[0].damageDice;
                        sides = atkType->GetAttacks()[0].damageSidesPerDie;
                    }
                    combat->ApplyDamage(*sim, src, tgt, true, false,
                                        CAttackType::NORMAL, CDamageType::NORMAL,
                                        "", dmg, 0, dice, sides);
                } else {
                    tgt->Damage(*sim, src, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", dmg);
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
    vm_.RegisterNative("SetUnitTimeScale", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetUnitScale", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetUnitColor", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetUnitVertexColor", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetUnitPathing", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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
        CUnitType* ut = unit->GetUnitType();
        return JassValue::Real(ut ? ut->GetDefaultAcquisitionRange() : 0.0f);
    });
    vm_.RegisterNative("SetUnitAcquireRange", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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
        CUnitType* ut = unit->GetUnitType();
        return JassValue::Real(ut ? ut->GetTurnRate() : 0.0f);
    });
    vm_.RegisterNative("SetUnitTurnSpeed", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("GetUnitPropWindow", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Real(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Real(0);
        CUnitType* ut = unit->GetUnitType();
        return JassValue::Real(ut ? ut->GetPropWindow() : 0.0f);
    });
    vm_.RegisterNative("SetUnitPropWindow", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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
    vm_.RegisterNative("GetUnitRallyPoint", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("SetUnitRallyPoint", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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
    vm_.RegisterNative("GetLocalPlayer", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
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
    vm_.RegisterNative("GetPlayerName", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Str("Player"); });
    vm_.RegisterNative("SetPlayerName", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("GetPlayerController", [](const std::vector<JassValue>&) -> JassValue {
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
    vm_.RegisterNative("IsPlayerObserver", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("IsPlayerRacePrefSet", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("GetPlayerRace", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("GetPlayerColor", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
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
        if (args.size() < 2 || !sim) return JassValue::Int(1);
        auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        if (p) return JassValue::Int(p->GetTechLevel(War3ID((uint32_t)args[1].intVal)));
        return JassValue::Int(0);
    });
    vm_.RegisterNative("SetPlayerTechMaxAllowed", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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
    vm_.RegisterNative("GetPlayerTechCount", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
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
    vm_.RegisterNative("GetPlayerHandicap", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(1.0f); });
    vm_.RegisterNative("SetPlayerHandicap", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("GetPlayerHandicapXP", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(1.0f); });
    vm_.RegisterNative("SetPlayerHandicapXP", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
}

// ============================================================
// 14. Hero natives
// ============================================================
void JassNatives::RegisterHeroNatives() {
    vm_.RegisterNative("GetHeroLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Int(unit->GetHeroLevel()); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("SetHeroLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetHeroLevel(args[1].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetHeroXP", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("SetHeroXP", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("GetHeroStr", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Int(unit->GetUnitType()->GetStartingStrength()); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetHeroAgi", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Int(unit->GetUnitType()->GetStartingAgility()); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetHeroInt", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Int(unit->GetUnitType()->GetStartingIntelligence()); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("SetHeroStr", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetHeroAgi", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetHeroInt", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("ModifyHeroStat", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("UnitAddHeroAbility", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(true); });
    vm_.RegisterNative("SelectHeroSkill", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("GetHeroSkillPoints", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("UnitModifySkillPoints", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(true); });
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
                accepted = JassValueToBool(vmPtr->CallFunction(filterFunc));
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
                vmPtr->CallFunction(actionFunc);
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
    vm_.RegisterNative("DestructableRestoreLife", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("QueueDestructableAnimation", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetDestructableAnimation", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("WaygateActivate", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("WaygateSetDestination", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("WaygateGetDestinationX", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
    vm_.RegisterNative("WaygateGetDestinationY", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
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
        if (it != data->triggers.end()) it->second->actionFuncNames.push_back(args[1].strVal);
        return JassValue::Null();
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
        JassQueuedContext context = CaptureContext();
        context.currentTriggerId = args[0].intVal;
        context.currentTriggerEventId = 0;
        for (const auto& funcName : it->second->actionFuncNames) {
            QueueFunctionWithContext(funcName, context);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetTriggerEvalCount", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("GetTriggerExecCount", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
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
    vm_.RegisterNative("GetEnumDestructable", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
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
    vm_.RegisterNative("GetSummonedUnit", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetSummoningUnit", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetConstructedStructure", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetTrainedUnit", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetResearched", [](const std::vector<JassValue>&) -> JassValue {
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
    vm_.RegisterNative("KillSoundWhenDone", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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
    vm_.RegisterNative("SetSoundChannel", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("AttachSoundToUnit", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("StartSound", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int sndSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (sndSim < 0) return JassValue::Null();
        auto sit = data->sounds.find(sndSim);
        if (sit == data->sounds.end()) return JassValue::Null();
        auto* audio = sim ? sim->GetAudioSystem() : nullptr;
        if (audio && !sit->second->label.empty()) {
            audio->Play2D(sit->second->label, sit->second->volume);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("StopSound", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)sim; (void)data;
        return JassValue::Null();
    });
    vm_.RegisterNative("DestroySound", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) { data->sounds.erase(id); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetSoundDuration", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("PlaySound", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int sndSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (sndSim < 0) return JassValue::Null();
        auto sit = data->sounds.find(sndSim);
        if (sit == data->sounds.end() || !sim) return JassValue::Null();
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
    vm_.RegisterNative("VolumeGroupReset", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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
    vm_.RegisterNative("SetFogStateRect", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetFogStateRadius", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("FogMaskEnable", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("FogEnable", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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
    vm_.RegisterNative("AddLightning", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LIGHTNING, data->nextHandleId++));
    });
    vm_.RegisterNative("AddLightningEx", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LIGHTNING, data->nextHandleId++));
    });
    vm_.RegisterNative("DestroyLightning", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::LIGHTNING);
        if (id >= 0) data->FreeHandle(args[0].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("MoveLightning", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("MoveLightningEx", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("GetLightningColor", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(1.0f); });
    vm_.RegisterNative("SetLightningColor", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
}

// ============================================================
// 24. Camera natives
// ============================================================
void JassNatives::RegisterCameraNatives() {
    vm_.RegisterNative("CreateCameraSetup", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->AllocHandle(JassHandleInfo::CAMERA_SETUP, data->nextHandleId++));
    });
    vm_.RegisterNative("CameraSetupSetField", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("CameraSetupGetField", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
    vm_.RegisterNative("CameraSetupSetDestPosition", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("CameraSetupGetDestPositionLoc", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("CameraSetupGetDestPositionX", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
    vm_.RegisterNative("CameraSetupGetDestPositionY", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
    vm_.RegisterNative("CameraSetupApply", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("CameraSetupApplyWithZ", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("CameraSetupApplyForceDuration", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("CameraSetupApplyForceDurationWithZ", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("CameraSetupApplyForPlayer", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("CameraSetupApplyForPlayerWithZ", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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
    vm_.RegisterNative("SetCameraBounds", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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
    vm_.RegisterNative("GetCameraEyePositionX", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetEyeX ? data->camGetEyeX() : 0);
    });
    vm_.RegisterNative("GetCameraEyePositionY", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetEyeY ? data->camGetEyeY() : 0);
    });
    vm_.RegisterNative("GetCameraEyePositionZ", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetEyeZ ? data->camGetEyeZ() : 0);
    });
    vm_.RegisterNative("GetCameraBoundMinX", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
    vm_.RegisterNative("GetCameraBoundMinY", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
    vm_.RegisterNative("GetCameraBoundMaxX", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
    vm_.RegisterNative("GetCameraBoundMaxY", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
    vm_.RegisterNative("GetCameraMargin", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
    vm_.RegisterNative("GetCameraField", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetField ? data->camGetField() : 0);
    });
    vm_.RegisterNative("SetCameraField", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 2 && data->camSetField)
            data->camSetField(args[0].intVal, args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("AdjustCameraField", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("ResetToGameCamera", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (data->camReset) data->camReset();
        return JassValue::Null();
    });
    vm_.RegisterNative("StopCamera", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetCinematicCamera", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetSkyModel", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
}

// ============================================================
// 25. Game natives
// ============================================================
void JassNatives::RegisterGameNatives() {
    vm_.RegisterNative("BJDebugMsg", [](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty()) std::cout << "[JASS] " << args[0].strVal << std::endl;
        return JassValue::Null();
    });
    vm_.RegisterNative("GetGameSpeed", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(2);
    });
    vm_.RegisterNative("GetGameTypeSelected", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("VersionGet", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(1);
    });
    vm_.RegisterNative("IsFogEnabled", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("IsFogMaskEnabled", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(true);
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
    vm_.RegisterNative("StartSound", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int sndSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (sndSim < 0) return JassValue::Null();
        auto sit = data->sounds.find(sndSim);
        if (sit == data->sounds.end() || !sim) return JassValue::Null();
        auto* audio = sim->GetAudioSystem();
        if (audio && !sit->second->label.empty())
            audio->Play2D(sit->second->label, sit->second->volume);
        return JassValue::Null();
    });
    vm_.RegisterNative("StopSound", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("AttachSoundToUnit", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    // Cinematic
    vm_.RegisterNative("SetCinematicMode", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetSkyModel", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetTimeOfDayScale", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("GetTimeOfDayScale", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(1.0f); });
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
    vm_.RegisterNative("SetTeams", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetPlayers", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (data && !args.empty()) {
            data->configuredPlayerCount = args[0].intVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetGamePlacement", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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
    vm_.RegisterNative("SetPlayerColor", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetPlayerRace", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetPlayerHandicap", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetPlayerHandicapXP", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetPlayerTeam", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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
    vm_.RegisterNative("SetGameDifficulty", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("GetGameDifficulty", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("GetDefaultDifficulty", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("SetGameType", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetMapFlags", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("IsMapFlagSet", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("GetIntegerGameState", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("GetFloatGameState", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0.0f); });
    vm_.RegisterNative("SetIntegerGameState", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetFloatGameState", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetGameSpeed", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("UseCustomHeroAbilities", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetTerrainAvailable", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetFogState", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetFogMask", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetFogMaskStyle", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetFogZHeight", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetWaterDeformStyle", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetWaterColor", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetWaterTransparency", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetWaterZ", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetSky", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetSkyModel", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetSkyTexture", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetSkyColor", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetSkyFog", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetPortrait", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetPortraitText", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetUnitPortrait", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetSoundVolume", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetMusicVolume", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PlayMusic", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PlayMusicEx", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("StopMusic", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("ResumeMusic", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PlayThematicMusic", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PlayThematicMusicEx", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("EndThematicMusic", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetMusicPlayPosition", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetThematicMusicPlayPosition", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("GetLocalizedHotkey", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || args[0].strVal.empty()) return JassValue::Int(0);
        return JassValue::Int((unsigned char)args[0].strVal[0]);
    });
    vm_.RegisterNative("DialogCreate", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("DialogDestroy", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("DialogClear", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("DialogSetMessage", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("DialogAddButton", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("DialogAddQuitButton", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("DialogDisplay", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("CreateMultiboard", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("DestroyMultiboard", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("MultiboardDisplay", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("MultiboardMinimize", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("MultiboardClear", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("MultiboardSetTitleText", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("MultiboardGetRowCount", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("MultiboardGetColumnCount", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("MultiboardSetColumnCount", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("MultiboardSetRowCount", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("MultiboardGetItem", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("MultiboardReleaseItem", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("MultiboardSetItemValue", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("MultiboardSetItemStyle", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("MultiboardSetItemWidth", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("MultiboardSetItemIcon", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("CreateLeaderboard", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("DestroyLeaderboard", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("LeaderboardDisplay", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("LeaderboardGetPlayerIndex", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(-1); });
    vm_.RegisterNative("LeaderboardAddItem", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("LeaderboardRemovePlayerItem", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("LeaderboardSetItemValue", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });

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
                    accepted = JassValueToBool(vmPtr->CallFunction(filterFunc));
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
        return JassValue::Int(data->AllocHandle(JassHandleInfo::PLAYER, 0));
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

    vm_.RegisterNative("GetGameSpeed", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(2);
    });

    vm_.RegisterNative("GetGameTypeSelected", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(true);
    });

    vm_.RegisterNative("VersionGet", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(1);
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

    vm_.RegisterNative("SetPlayerTeam", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        // args: player, team
        (void)data;
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
