#include "scripting/jass_natives_internal.h"
#include "ui/UIFrame.h"
#include "ui/ButtonFrame.h"

// ============================================================

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

float JassNativesData::TerrainDeformOffset(float x, float y) const {
    float offset = 0.0f;
    for (const auto& entry : terrainDeforms) {
        const auto& deform = entry.second;
        if (!deform || deform->stopped || deform->radius <= 0.0f) continue;
        const float dx = x - deform->x;
        const float dy = y - deform->y;
        const float dist2 = dx * dx + dy * dy;
        const float radius2 = deform->radius * deform->radius;
        if (dist2 > radius2) continue;
        const float falloff = 1.0f - std::sqrt(dist2) / deform->radius;
        const float sign = deform->kind == 0 ? -1.0f : 1.0f;
        offset += sign * deform->depth * falloff;
    }
    return offset;
}

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
    context.eventSpellTargetDestructableHandle = data_->eventSpellTargetDestructableHandle;
    context.eventSpellTargetItemHandle = data_->eventSpellTargetItemHandle;
    context.eventTriggerPlayerHandle = data_->eventTriggerPlayerHandle;
    context.eventTriggerWidgetHandle = data_->eventTriggerWidgetHandle;
    context.eventEnteringUnitHandle = data_->eventEnteringUnitHandle;
    context.eventLeavingUnitHandle = data_->eventLeavingUnitHandle;
    context.eventManipulatingUnitHandle = data_->eventManipulatingUnitHandle;
    context.eventManipulatedItemHandle = data_->eventManipulatedItemHandle;
    context.eventBuyingUnitHandle = data_->eventBuyingUnitHandle;
    context.eventSellingUnitHandle = data_->eventSellingUnitHandle;
    context.eventSoldItemHandle = data_->eventSoldItemHandle;
    context.eventSoldUnitHandle = data_->eventSoldUnitHandle;
    context.eventChangingUnitPrevOwnerHandle = data_->eventChangingUnitPrevOwnerHandle;
    context.eventRescuerUnitHandle = data_->eventRescuerUnitHandle;
    context.eventTrainedUnitTypeId = data_->eventTrainedUnitTypeId;
    context.eventConstructedUnitHandle = data_->eventConstructedUnitHandle;
    context.eventCancelledUnitHandle = data_->eventCancelledUnitHandle;
    context.eventConstructingUnitHandle = data_->eventConstructingUnitHandle;
    context.eventLearnedSkillId = data_->eventLearnedSkillId;
    context.eventLearnedSkillLevel = data_->eventLearnedSkillLevel;
    context.eventOrderedUnitHandle = data_->eventOrderedUnitHandle;
    context.eventIssuedOrderId = data_->eventIssuedOrderId;
    context.eventOrderPointX = data_->eventOrderPointX;
    context.eventOrderPointY = data_->eventOrderPointY;
    context.eventOrderTargetHandle = data_->eventOrderTargetHandle;
    context.eventKillingUnitHandle = data_->eventKillingUnitHandle;
    context.eventDyingUnitHandle = data_->eventDyingUnitHandle;
    context.eventDecayingUnitHandle = data_->eventDecayingUnitHandle;
    context.eventRevivingUnitHandle = data_->eventRevivingUnitHandle;
    context.eventTransportUnitHandle = data_->eventTransportUnitHandle;
    context.eventLoadedUnitHandle = data_->eventLoadedUnitHandle;
    context.eventTrainedUnitHandle = data_->eventTrainedUnitHandle;
    context.eventSummonedUnitHandle = data_->eventSummonedUnitHandle;
    context.eventSummoningUnitHandle = data_->eventSummoningUnitHandle;
    context.eventResearchingUnitHandle = data_->eventResearchingUnitHandle;
    context.eventResearchedId = data_->eventResearchedId;
    context.eventExpiredTimerHandle = data_->eventExpiredTimerHandle;
    context.eventTrackableHandle = data_->eventTrackableHandle;
    context.eventClickedButtonHandle = data_->eventClickedButtonHandle;
    context.eventClickedDialogHandle = data_->eventClickedDialogHandle;
    context.eventTriggeringRegionHandle = data_->eventTriggeringRegionHandle;
    context.eventTriggerFrameHandle = data_->eventTriggerFrameHandle;
    context.eventTriggerFrameEvent = data_->eventTriggerFrameEvent;
    context.eventTriggerFrameValue = data_->eventTriggerFrameValue;
    context.eventTriggerFrameText = data_->eventTriggerFrameText;
    context.eventChatString = data_->eventChatString;
    context.eventChatMatched = data_->eventChatMatched;
    context.eventSpellOrderId = data_->eventSpellOrderId;
    context.eventSpellTargetType = data_->eventSpellTargetType;
    context.triggerCastId = data_->triggerCastId;
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
    data_->eventSpellTargetDestructableHandle = context.eventSpellTargetDestructableHandle;
    data_->eventSpellTargetItemHandle = context.eventSpellTargetItemHandle;
    data_->eventTriggerPlayerHandle = context.eventTriggerPlayerHandle;
    data_->eventTriggerWidgetHandle = context.eventTriggerWidgetHandle;
    data_->eventEnteringUnitHandle = context.eventEnteringUnitHandle;
    data_->eventLeavingUnitHandle = context.eventLeavingUnitHandle;
    data_->eventManipulatingUnitHandle = context.eventManipulatingUnitHandle;
    data_->eventManipulatedItemHandle = context.eventManipulatedItemHandle;
    data_->eventBuyingUnitHandle = context.eventBuyingUnitHandle;
    data_->eventSellingUnitHandle = context.eventSellingUnitHandle;
    data_->eventSoldItemHandle = context.eventSoldItemHandle;
    data_->eventSoldUnitHandle = context.eventSoldUnitHandle;
    data_->eventChangingUnitPrevOwnerHandle = context.eventChangingUnitPrevOwnerHandle;
    data_->eventRescuerUnitHandle = context.eventRescuerUnitHandle;
    data_->eventTrainedUnitTypeId = context.eventTrainedUnitTypeId;
    data_->eventConstructedUnitHandle = context.eventConstructedUnitHandle;
    data_->eventCancelledUnitHandle = context.eventCancelledUnitHandle;
    data_->eventConstructingUnitHandle = context.eventConstructingUnitHandle;
    data_->eventLearnedSkillId = context.eventLearnedSkillId;
    data_->eventLearnedSkillLevel = context.eventLearnedSkillLevel;
    data_->eventOrderedUnitHandle = context.eventOrderedUnitHandle;
    data_->eventIssuedOrderId = context.eventIssuedOrderId;
    data_->eventOrderPointX = context.eventOrderPointX;
    data_->eventOrderPointY = context.eventOrderPointY;
    data_->eventOrderTargetHandle = context.eventOrderTargetHandle;
    data_->eventKillingUnitHandle = context.eventKillingUnitHandle;
    data_->eventDyingUnitHandle = context.eventDyingUnitHandle;
    data_->eventDecayingUnitHandle = context.eventDecayingUnitHandle;
    data_->eventRevivingUnitHandle = context.eventRevivingUnitHandle;
    data_->eventTransportUnitHandle = context.eventTransportUnitHandle;
    data_->eventLoadedUnitHandle = context.eventLoadedUnitHandle;
    data_->eventTrainedUnitHandle = context.eventTrainedUnitHandle;
    data_->eventSummonedUnitHandle = context.eventSummonedUnitHandle;
    data_->eventSummoningUnitHandle = context.eventSummoningUnitHandle;
    data_->eventResearchingUnitHandle = context.eventResearchingUnitHandle;
    data_->eventResearchedId = context.eventResearchedId;
    data_->eventExpiredTimerHandle = context.eventExpiredTimerHandle;
    data_->eventTrackableHandle = context.eventTrackableHandle;
    data_->eventClickedButtonHandle = context.eventClickedButtonHandle;
    data_->eventClickedDialogHandle = context.eventClickedDialogHandle;
    data_->eventTriggeringRegionHandle = context.eventTriggeringRegionHandle;
    data_->eventTriggerFrameHandle = context.eventTriggerFrameHandle;
    data_->eventTriggerFrameEvent = context.eventTriggerFrameEvent;
    data_->eventTriggerFrameValue = context.eventTriggerFrameValue;
    data_->eventTriggerFrameText = context.eventTriggerFrameText;
    data_->eventChatString = context.eventChatString;
    data_->eventChatMatched = context.eventChatMatched;
    data_->eventSpellOrderId = context.eventSpellOrderId;
    data_->eventSpellTargetType = context.eventSpellTargetType;
    data_->triggerCastId = context.triggerCastId;
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

        // A woken TriggerExecuteWait caller finishes its join first: the
        // callee remainder runs before the caller resumes. Re-sleep parks
        // the thread back in sleepingThreads_ (handled inside).
        if (thread.joinId != 0 && !ContinueTriggerJoin(thread)) {
            pumped++;
            continue;
        }

        JassQueuedContext previous = CaptureContext();
        ApplyContext(thread.context);
        const size_t errorCountBefore = vm_.GetErrors().size();
        activePumpThread_ = &thread;
        JassVM::ThreadRunResult result = vm_.RunThread(thread.vmState, thread.functionName);
        activePumpThread_ = nullptr;
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

bool JassNatives::ContinueTriggerJoin(JassQueuedThread& thread) {
    auto jit = triggerJoins_.find(thread.joinId);
    if (jit == triggerJoins_.end()) {
        thread.joinId = 0;
        return true;
    }
    JassTriggerJoin& join = jit->second;
    JassQueuedContext previous = CaptureContext();
    ApplyContext(join.calleeContext);
    // Finish the suspended child first; it may sleep again.
    if (join.childActive) {
        JassVM::ThreadRunResult result = vm_.RunThread(join.suspendedChild,
                                                        join.suspendedChild.functionName);
        if (!result.completed && result.sleeping) {
            thread.sleepRemaining = result.sleepSeconds;
            ApplyContext(previous);
            sleepingThreads_.push_back(std::move(thread));
            return false;
        }
        join.childActive = false;
    }
    // Then the not-yet-run actions, each of which may sleep in turn.
    while (!join.pendingActions.empty()) {
        std::string funcName = std::move(join.pendingActions.front());
        join.pendingActions.erase(join.pendingActions.begin());
        if (funcName.empty() || !vm_.HasFunction(funcName)) continue;
        JassVM::ThreadState child;
        JassVM::ThreadRunResult result = vm_.RunThread(child, funcName);
        if (result.completed) continue;
        if (!result.sleeping) continue;
        join.suspendedChild = std::move(child);
        join.childActive = true;
        thread.sleepRemaining = result.sleepSeconds;
        ApplyContext(previous);
        sleepingThreads_.push_back(std::move(thread));
        return false;
    }
    triggerJoins_.erase(jit);
    thread.joinId = 0;
    ApplyContext(previous);
    return true;
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

void JassNatives::FireDecayEvent(int unitSimHandle) {
    if (unitSimHandle < 0 || !sim_ || !sim_->GetUnit(unitSimHandle)) {
        return;
    }
    data_->eventDecayingUnitHandle =
        data_->AllocHandle(JassHandleInfo::UNIT, unitSimHandle);
    FireUnitEvent(unitSimHandle, JASS_EVENT_UNIT_DECAY,
                  JASS_EVENT_PLAYER_UNIT_DECAY);
    data_->eventDecayingUnitHandle = -1;
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

void JassNatives::FireSellItemEvent(int buyerSimHandle, int sellerSimHandle, int itemSimHandle) {
    if (buyerSimHandle < 0 || sellerSimHandle < 0 || itemSimHandle < 0) {
        return;
    }
    // Buyer doubles as the manipulating unit so generic item-event triggers
    // observe the transaction; shop handles ride the dedicated fields.
    data_->eventManipulatingUnitHandle = data_->AllocHandle(JassHandleInfo::UNIT, buyerSimHandle);
    data_->eventManipulatedItemHandle = data_->AllocHandle(JassHandleInfo::ITEM, itemSimHandle);
    data_->eventBuyingUnitHandle = data_->eventManipulatingUnitHandle;
    data_->eventSellingUnitHandle = data_->AllocHandle(JassHandleInfo::UNIT, sellerSimHandle);
    data_->eventSoldItemHandle = data_->eventManipulatedItemHandle;
    if (sim_) {
        if (CItem* item = sim_->GetItem(itemSimHandle)) {
            data_->items[itemSimHandle] = item;
        }
    }
    FireUnitEvent(buyerSimHandle, JASS_EVENT_UNIT_SELL_ITEM, JASS_EVENT_PLAYER_UNIT_SELL_ITEM);

    data_->eventManipulatingUnitHandle = -1;
    data_->eventManipulatedItemHandle = -1;
    data_->eventBuyingUnitHandle = -1;
    data_->eventSellingUnitHandle = -1;
    data_->eventSoldItemHandle = -1;
}

void JassNatives::FireSellUnitEvent(int buyerSimHandle, int sellerSimHandle, int hiredSimHandle) {
    // Mercenary hire (ref SELL, not SELL_ITEM): the hired unit is the
    // triggering unit; buyer may be absent (-1, player-level purchase).
    if (sellerSimHandle < 0 || hiredSimHandle < 0) {
        return;
    }
    data_->eventBuyingUnitHandle =
        buyerSimHandle >= 0 ? data_->AllocHandle(JassHandleInfo::UNIT, buyerSimHandle) : -1;
    data_->eventSellingUnitHandle = data_->AllocHandle(JassHandleInfo::UNIT, sellerSimHandle);
    data_->eventSoldUnitHandle = data_->AllocHandle(JassHandleInfo::UNIT, hiredSimHandle);
    FireUnitEvent(hiredSimHandle, JASS_EVENT_UNIT_SELL, JASS_EVENT_PLAYER_UNIT_SELL);

    data_->eventBuyingUnitHandle = -1;
    data_->eventSellingUnitHandle = -1;
    data_->eventSoldUnitHandle = -1;
}

void JassNatives::FirePawnItemEvent(int sellerSimHandle, int shopSimHandle, int itemSimHandle) {
    // Pawn-back sale (ref PAWN_ITEM): the pawner sells, the shop buys.
    if (sellerSimHandle < 0 || shopSimHandle < 0 || itemSimHandle < 0) {
        return;
    }
    data_->eventManipulatingUnitHandle = data_->AllocHandle(JassHandleInfo::UNIT, sellerSimHandle);
    data_->eventManipulatedItemHandle = data_->AllocHandle(JassHandleInfo::ITEM, itemSimHandle);
    data_->eventSellingUnitHandle = data_->eventManipulatingUnitHandle;
    data_->eventBuyingUnitHandle = data_->AllocHandle(JassHandleInfo::UNIT, shopSimHandle);
    data_->eventSoldItemHandle = data_->eventManipulatedItemHandle;
    if (sim_) {
        if (CItem* item = sim_->GetItem(itemSimHandle)) {
            data_->items[itemSimHandle] = item;
        }
    }
    FireUnitEvent(sellerSimHandle, JASS_EVENT_UNIT_PAWN_ITEM, JASS_EVENT_PLAYER_UNIT_PAWN_ITEM);

    data_->eventManipulatingUnitHandle = -1;
    data_->eventManipulatedItemHandle = -1;
    data_->eventBuyingUnitHandle = -1;
    data_->eventSellingUnitHandle = -1;
    data_->eventSoldItemHandle = -1;
}

void JassNatives::FireChangeOwnerEvent(int unitSimHandle, int prevOwnerPlayerIndex) {
    if (unitSimHandle < 0 || prevOwnerPlayerIndex < 0) {
        return;
    }
    data_->eventChangingUnitPrevOwnerHandle =
        data_->AllocHandle(JassHandleInfo::PLAYER, prevOwnerPlayerIndex);
    FireUnitEvent(unitSimHandle, JASS_EVENT_UNIT_CHANGE_OWNER, JASS_EVENT_PLAYER_UNIT_CHANGE_OWNER);

    data_->eventChangingUnitPrevOwnerHandle = -1;
}

void JassNatives::FireHeroSkillEvent(int unitSimHandle, int skillId, int newLevel) {
    // Unit-only event in retail (no player-unit counterpart).
    if (unitSimHandle < 0) {
        return;
    }
    data_->eventLearnedSkillId = skillId;
    data_->eventLearnedSkillLevel = newLevel;
    FireUnitEvent(unitSimHandle, JASS_EVENT_UNIT_HERO_SKILL);

    data_->eventLearnedSkillId = 0;
    data_->eventLearnedSkillLevel = 0;
}

void JassNatives::FireConstructFinishEvent(int buildingSimHandle) {
    if (buildingSimHandle < 0) {
        return;
    }
    // The finished structure is both the constructed and (just-finished)
    // constructing structure (mirrors the trained-event pattern).
    data_->eventConstructedUnitHandle = data_->AllocHandle(JassHandleInfo::UNIT, buildingSimHandle);
    data_->eventConstructingUnitHandle = data_->eventConstructedUnitHandle;
    FireUnitEvent(buildingSimHandle, JASS_EVENT_UNIT_CONSTRUCT_FINISH,
                  JASS_EVENT_PLAYER_UNIT_CONSTRUCT_FINISH);

    data_->eventConstructedUnitHandle = -1;
    data_->eventConstructingUnitHandle = -1;
}

void JassNatives::FireConstructCancelEvent(int buildingSimHandle) {
    if (buildingSimHandle < 0) {
        return;
    }
    data_->eventCancelledUnitHandle = data_->AllocHandle(JassHandleInfo::UNIT, buildingSimHandle);
    data_->eventConstructingUnitHandle = data_->eventCancelledUnitHandle;
    FireUnitEvent(buildingSimHandle, JASS_EVENT_UNIT_CONSTRUCT_CANCEL,
                  JASS_EVENT_PLAYER_UNIT_CONSTRUCT_CANCEL);

    data_->eventCancelledUnitHandle = -1;
    data_->eventConstructingUnitHandle = -1;
}

void JassNatives::FireTrainStartEvent(int buildingSimHandle, int typeId) {
    // No unit exists yet: the type id carries the context (ref train start).
    if (buildingSimHandle < 0) {
        return;
    }
    data_->eventTrainedUnitTypeId = typeId;
    FireUnitEvent(buildingSimHandle, JASS_EVENT_UNIT_TRAIN_START,
                  JASS_EVENT_PLAYER_UNIT_TRAIN_START);

    data_->eventTrainedUnitTypeId = 0;
}

void JassNatives::FireTrainCancelEvent(int buildingSimHandle, int typeId) {
    if (buildingSimHandle < 0) {
        return;
    }
    data_->eventTrainedUnitTypeId = typeId;
    FireUnitEvent(buildingSimHandle, JASS_EVENT_UNIT_TRAIN_CANCEL,
                  JASS_EVENT_PLAYER_UNIT_TRAIN_CANCEL);

    data_->eventTrainedUnitTypeId = 0;
}

void JassNatives::FireResearchStartEvent(int buildingSimHandle, int upgradeId) {
    if (buildingSimHandle < 0) {
        return;
    }
    data_->eventResearchedId = upgradeId;
    data_->eventResearchingUnitHandle =
        data_->AllocHandle(JassHandleInfo::UNIT, buildingSimHandle);
    FireUnitEvent(buildingSimHandle, JASS_EVENT_UNIT_RESEARCH_START,
                  JASS_EVENT_PLAYER_UNIT_RESEARCH_START);

    data_->eventResearchedId = 0;
    data_->eventResearchingUnitHandle = -1;
}

void JassNatives::FireResearchCancelEvent(int buildingSimHandle, int upgradeId) {
    if (buildingSimHandle < 0) {
        return;
    }
    data_->eventResearchedId = upgradeId;
    data_->eventResearchingUnitHandle =
        data_->AllocHandle(JassHandleInfo::UNIT, buildingSimHandle);
    FireUnitEvent(buildingSimHandle, JASS_EVENT_UNIT_RESEARCH_CANCEL,
                  JASS_EVENT_PLAYER_UNIT_RESEARCH_CANCEL);

    data_->eventResearchedId = 0;
    data_->eventResearchingUnitHandle = -1;
}

void JassNatives::FireUpgradeStartEvent(int buildingSimHandle, int upgradeId) {
    // Upgrade ids ride the researched slot (no dedicated retail getter).
    if (buildingSimHandle < 0) {
        return;
    }
    data_->eventResearchedId = upgradeId;
    data_->eventResearchingUnitHandle =
        data_->AllocHandle(JassHandleInfo::UNIT, buildingSimHandle);
    FireUnitEvent(buildingSimHandle, JASS_EVENT_UNIT_UPGRADE_START,
                  JASS_EVENT_PLAYER_UNIT_UPGRADE_START);

    data_->eventResearchedId = 0;
    data_->eventResearchingUnitHandle = -1;
}

void JassNatives::FireUpgradeFinishEvent(int buildingSimHandle, int upgradeId) {
    if (buildingSimHandle < 0) {
        return;
    }
    data_->eventResearchedId = upgradeId;
    data_->eventResearchingUnitHandle =
        data_->AllocHandle(JassHandleInfo::UNIT, buildingSimHandle);
    FireUnitEvent(buildingSimHandle, JASS_EVENT_UNIT_UPGRADE_FINISH,
                  JASS_EVENT_PLAYER_UNIT_UPGRADE_FINISH);

    data_->eventResearchedId = 0;
    data_->eventResearchingUnitHandle = -1;
}

void JassNatives::ForgetItem(int itemSimHandle) {
    if (itemSimHandle < 0) {
        return;
    }
    data_->items.erase(itemSimHandle);
}
void JassNatives::FireRegionEvent(int regionSimId, int unitSimHandle, bool isEnter) {
    if (regionSimId < 0 || unitSimHandle < 0) {
        return;
    }
    const int eventCode = isEnter ? 4001 : 4002;
    CUnit* unit = sim_ ? sim_->GetUnit(unitSimHandle) : nullptr;
    for (auto& [triggerId, trigger] : data_->triggers) {
        int matchedRegionHandle = 0;
        bool registered = false;
        // Prefer the filter-carrying region events (ref CRegionTriggerEnter);
        // fall back to bare registeredEvents for legacy registrations.
        bool hasRegionEvents = false;
        bool filterPass = false;
        bool unfiltered = false;
        for (const auto& ev : trigger->regionEvents) {
            if (ev.code != eventCode) continue;
            if (data_->GetSimHandle(ev.regionHandle, JassHandleInfo::REGION) != regionSimId) continue;
            hasRegionEvents = true;
            matchedRegionHandle = ev.regionHandle;
            if (ev.filterFunc.empty()) {
                unfiltered = true;
                break;
            }
            if (unit && vmPtr_) {
                const int unitHandle = data_->AllocHandle(JassHandleInfo::UNIT, unitSimHandle);
                const int prev = data_->filterUnitHandle;
                data_->filterUnitHandle = unitHandle;
                const bool accepted = JassValueToBool(CallSyncFunction(vmPtr_, ev.filterFunc));
                data_->filterUnitHandle = prev;
                if (accepted) {
                    filterPass = true;
                    break;
                }
            }
        }
        if (hasRegionEvents) {
            if (!unfiltered && !filterPass) continue;
            registered = true;
        } else {
            for (const auto& [regionHandle, code] : trigger->registeredEvents) {
                if (code != eventCode) continue;
                if (data_->GetSimHandle(regionHandle, JassHandleInfo::REGION) == regionSimId) {
                    registered = true;
                    matchedRegionHandle = regionHandle;
                    break;
                }
            }
            if (!registered) continue;
        }
        JassQueuedContext context = CaptureContext();
        context.currentTriggerId = triggerId;
        context.currentTriggerEventId = eventCode;
        context.eventTriggeringRegionHandle = matchedRegionHandle;
        if (unit) {
            context.currentTriggerUnitHandle =
                data_->AllocHandle(JassHandleInfo::UNIT, unitSimHandle);
            // Filters and conditions observe the crossing unit (ref filter
            // scope); actions use GetEnteringUnit/GetLeavingUnit.
            context.filterUnitHandle = context.currentTriggerUnitHandle;
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

void JassNatives::UpdateAI() {
    if (!data_) return;
    for (auto& [playerId, controller] : data_->aiControllers) {
        if (controller) controller->Update();
    }
}

// Ages floating text: drift by velocity unless suspended, and retire
// non-permanent tags past lifespan (handles freed like DestroyTextTag).
void JassNatives::UpdateTextTags(float dt) {
    if (!data_ || dt <= 0.0f) return;
    std::vector<int> expired;
    for (auto& [id, tag] : data_->textTags) {
        if (!tag || tag->suspended) continue;
        tag->age += dt;
        tag->x += tag->vx * dt;
        tag->y += tag->vy * dt;
        if (!tag->permanent && tag->lifespan > 0.0f && tag->age >= tag->lifespan) {
            expired.push_back(id);
        }
    }
    for (int id : expired) {
        data_->textTags.erase(id);
        for (auto it = data_->handles.begin(); it != data_->handles.end(); ++it) {
            if (it->second.type == JassHandleInfo::TEXT_TAG && it->second.id == id) {
                data_->FreeHandle(it->first);
                break;
            }
        }
    }
}

void JassNatives::UpdateRegions() {
    if (!sim_ || !data_) {
        return;
    }    // Fast path: nothing to track.
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

void JassNatives::FireRescueEvent(int rescuedSimHandle, int rescuerSimHandle) {
    if (rescuedSimHandle < 0 || rescuerSimHandle < 0) {
        return;
    }
    // Trigger unit is the rescued one; GetRescuer() reports the rescuer.
    data_->eventRescuerUnitHandle =
        data_->AllocHandle(JassHandleInfo::UNIT, rescuerSimHandle);
    FireUnitEvent(rescuedSimHandle, JASS_EVENT_UNIT_RESCUED,
                  JASS_EVENT_PLAYER_UNIT_RESCUED);

    data_->eventRescuerUnitHandle = -1;
}

void JassNatives::FireLoadedEvent(int transportSimHandle, int passengerSimHandle) {
    if (transportSimHandle < 0 || passengerSimHandle < 0) {
        return;
    }
    // Trigger unit is the passenger (rescue shape); the transport rides
    // the dedicated fields for GetTransportUnit/GetLoadedUnit.
    data_->eventTransportUnitHandle =
        data_->AllocHandle(JassHandleInfo::UNIT, transportSimHandle);
    data_->eventLoadedUnitHandle =
        data_->AllocHandle(JassHandleInfo::UNIT, passengerSimHandle);
    FireUnitEvent(passengerSimHandle, JASS_EVENT_UNIT_LOADED,
                  JASS_EVENT_PLAYER_UNIT_LOADED);

    data_->eventTransportUnitHandle = -1;
    data_->eventLoadedUnitHandle = -1;
}
void JassNatives::UpdateRescue() {
    if (!sim_ || !data_ || data_->unitRescue.empty()) {
        return;
    }
    // Drop entries for gone units.
    for (auto it = data_->unitRescue.begin(); it != data_->unitRescue.end();) {
        if (!sim_->GetUnit(it->first)) {
            it = data_->unitRescue.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto& [rescuedSim, info] : data_->unitRescue) {
        if (!info.rescuable) continue;
        CUnit* rescued = sim_->GetUnit(rescuedSim);
        if (!rescued || rescued->IsDead()) continue;
        if (rescued->GetPlayerIndex() == info.player) continue;
        const float rangeSq = info.range * info.range;
        for (CUnit* viewer : sim_->GetUnits()) {
            if (!viewer || viewer->IsDead() || viewer == rescued) continue;
            if (viewer->GetPlayerIndex() != info.player) continue;
            const float dx = viewer->GetX() - rescued->GetX();
            const float dy = viewer->GetY() - rescued->GetY();
            if (dx * dx + dy * dy > rangeSq) continue;
            // Transfer ownership (ref rescue): same remove/add/set pattern
            // as charm transfers.
            if (CPlayer* oldOwner = sim_->GetPlayer(rescued->GetPlayerIndex())) {
                oldOwner->RemoveUnit(rescued);
            }
            rescued->SetPlayerIndex(info.player);
            if (CPlayer* newOwner = sim_->GetPlayer(info.player)) {
                newOwner->AddUnit(rescued);
            }
            FireRescueEvent(rescuedSim, viewer->GetHandleId());
            break;
        }
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

void JassNatives::FireRevivableEvent(int heroSimHandle) {
    if (heroSimHandle < 0) return;
    // Trigger unit is the fallen hero (ref onHeroDeath revivable scope).
    FireUnitEvent(heroSimHandle, JASS_EVENT_UNIT_HERO_REVIVABLE,
                  JASS_EVENT_PLAYER_HERO_REVIVABLE);
}

void JassNatives::FireReviveStartEvent(int buildingSimHandle, int heroSimHandle) {
    if (buildingSimHandle < 0 || heroSimHandle < 0) return;
    data_->eventRevivingUnitHandle =
        data_->AllocHandle(JassHandleInfo::UNIT, heroSimHandle);
    FireUnitEvent(buildingSimHandle, JASS_EVENT_UNIT_HERO_REVIVE_START,
                  JASS_EVENT_PLAYER_HERO_REVIVE_START);
    data_->eventRevivingUnitHandle = -1;
}

void JassNatives::FireReviveCancelEvent(int buildingSimHandle, int heroSimHandle) {
    if (buildingSimHandle < 0 || heroSimHandle < 0) return;
    data_->eventRevivingUnitHandle =
        data_->AllocHandle(JassHandleInfo::UNIT, heroSimHandle);
    FireUnitEvent(buildingSimHandle, JASS_EVENT_UNIT_HERO_REVIVE_CANCEL,
                  JASS_EVENT_PLAYER_HERO_REVIVE_CANCEL);
    data_->eventRevivingUnitHandle = -1;
}

void JassNatives::FireSpellEvent(int casterSimHandle, int stage, int targetSimHandle,
                                float x, float y) {
    if (casterSimHandle < 0) return;
    // stage: 0=channel 1=cast 2=effect 3=finish 4=endcast (ref
    // EVENT_UNIT_SPELL_* 289-293 + player counterparts 272-276).
    // Target context feeds GetSpellTargetUnit/Destructable/Item/X/Y (ref
    // spell event state); exactly one widget-kind handle is live per cast.
    data_->eventSpellTargetX = x;
    data_->eventSpellTargetY = y;
    data_->eventSpellTargetUnitHandle = -1;
    data_->eventSpellTargetDestructableHandle = -1;
    data_->eventSpellTargetItemHandle = -1;
    if (targetSimHandle >= 0 && sim_) {
        if (CUnit* targetUnit = sim_->GetUnit(targetSimHandle)) {
            data_->eventSpellTargetUnitHandle =
                data_->AllocHandle(JassHandleInfo::UNIT, targetUnit->GetHandleId());
        } else if (CDestructable* targetDest = sim_->GetDestructable(targetSimHandle)) {
            data_->eventSpellTargetDestructableHandle =
                data_->AllocHandle(JassHandleInfo::DESTRUCTABLE, targetDest->GetHandleId());
        } else if (CItem* targetItem = sim_->GetItem(targetSimHandle)) {
            data_->items[targetItem->GetHandleId()] = targetItem;
            data_->eventSpellTargetItemHandle =
                data_->AllocHandle(JassHandleInfo::ITEM, targetItem->GetHandleId());
        }
    }
    static const int kUnit[5] = {
        JASS_EVENT_UNIT_SPELL_CHANNEL, JASS_EVENT_UNIT_SPELL_CAST,
        JASS_EVENT_UNIT_SPELL_EFFECT, JASS_EVENT_UNIT_SPELL_FINISH,
        JASS_EVENT_UNIT_SPELL_ENDCAST,
    };
    static const int kPlayer[5] = {
        JASS_EVENT_PLAYER_UNIT_SPELL_CHANNEL, JASS_EVENT_PLAYER_UNIT_SPELL_CAST,
        JASS_EVENT_PLAYER_UNIT_SPELL_EFFECT, JASS_EVENT_PLAYER_UNIT_SPELL_FINISH,
        JASS_EVENT_PLAYER_UNIT_SPELL_ENDCAST,
    };
    if (stage < 0 || stage > 4) return;
    FireUnitEvent(casterSimHandle, kUnit[stage], kPlayer[stage]);
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
        // Displayed timer dialogs scale the countdown (ref TimerDialogSetSpeed
        // changes the timer rate; first displayed dialog bound here wins).
        float speed = 1.0f;
        for (const auto& [dialogId, dialog] : data_->timerDialogs) {
            (void)dialogId;
            if (dialog && dialog->displayed && dialog->timerSimId == id) {
                speed = std::max(0.0f, dialog->speed);
                break;
            }
        }
        timer->remaining -= dt * speed;
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
    // Decay per-unit ability cooldowns started via StartUnitAbilityCooldown.
    if (data_ && dt > 0.0f && !data_->abilityCooldowns.empty()) {
        for (auto it = data_->abilityCooldowns.begin(); it != data_->abilityCooldowns.end();) {
            it->second -= dt;
            if (it->second <= 0.0f) {
                it = data_->abilityCooldowns.erase(it);
            } else {
                ++it;
            }
        }
    }
    // Tick live sim-ability cooldowns (ref cooldown sweep; behavior_spell
    // starts them on cast but nothing decayed them, freezing recasts).
    if (sim_ && dt > 0.0f) {
        for (CUnit* unit : sim_->GetUnits()) {
            if (!unit) continue;
            for (CAbility* ability : unit->GetAbilities()) {
                if (auto* spell = dynamic_cast<CAbilitySpellBase*>(ability)) {
                    spell->UpdateCooldown(dt);
                }
            }
        }
    }
    UpdateTrackables();
    UpdateProjectiles(dt);
    FireTickTriggers();
    FireStateTriggers();
    // Cutscene clock override expiry (ref false time of day).
    if (data_ && sim_ && data_->falseTime.remaining > 0.0f && dt > 0.0f) {
        data_->falseTime.remaining -= dt;
        if (data_->falseTime.remaining <= 0.0f) {
            sim_->SetTimeOfDaySuspended(data_->falseTime.wasSuspended);
        }
    }
    // Terrain deform durations are milliseconds (common.j); non-permanent
    // deforms auto-stop so crater/ripple/wave lift fades like ref.
    if (data_ && dt > 0.0f && !data_->terrainDeforms.empty()) {
        const float dtMs = dt * 1000.0f;
        for (auto& [id, deform] : data_->terrainDeforms) {
            (void)id;
            if (!deform || deform->stopped || deform->permanent) continue;
            if (deform->duration <= 0) continue;
            deform->duration -= (int)dtMs;
            if (deform->duration <= 0) {
                deform->duration = 0;
                deform->stopped = true;
            }
        }
    }
    // Minimap pings age out after their duration (ref ping timeouts);
    // non-positive durations display until cleared by the next ping pass.
    if (data_ && dt > 0.0f && !data_->minimapPings.empty()) {
        auto& pings = data_->minimapPings;
        for (auto& ping : pings) ping.age += dt;
        pings.erase(std::remove_if(pings.begin(), pings.end(),
                                   [](const JassNativesData::JassMinimapPing& ping) {
                                       return ping.duration > 0.0f && ping.age >= ping.duration;
                                   }),
                    pings.end());
    }
    // Cinematic filter transition clock (ref CCineFilter age).
    if (data_ && dt > 0.0f && data_->cineFilter.displayed) {
        data_->cineFilter.age += dt;
    }
    return firedCount;
}

void JassNatives::FireTrackableEvent(int trackableHandle, int eventCode) {
    if (!data_ || trackableHandle == 0) return;
    for (auto& [triggerId, trigger] : data_->triggers) {
        if (!trigger) continue;
        bool matches = false;
        for (const auto& event : trigger->registeredEvents) {
            if (event.first == trackableHandle && event.second == eventCode) {
                matches = true;
                break;
            }
        }
        if (!matches) continue;
        JassQueuedContext context = CaptureContext();
        context.currentTriggerId = triggerId;
        context.currentTriggerEventId = eventCode;
        context.eventTrackableHandle = trackableHandle;
        QueueTriggerActions(*trigger, context);
    }
}

void JassNatives::UpdateTrackables() {
    if (!data_ || !sim_) return;
    constexpr float kEnterRadius = 128.0f;
    const float limit = kEnterRadius * kEnterRadius;
    for (auto& [id, track] : data_->trackables) {
        (void)id;
        if (!track || track->jassHandle == 0) continue;
        bool inside = false;
        for (CUnit* unit : sim_->GetUnits()) {
            if (!unit || unit->IsDead() || unit->IsHidden()) continue;
            const float dx = unit->GetX() - track->x;
            const float dy = unit->GetY() - track->y;
            if (dx * dx + dy * dy <= limit) {
                inside = true;
                break;
            }
        }
        if (!track->occupancyReady) {
            track->unitInside = inside;
            track->occupancyReady = true;
            continue;
        }
        if (inside && !track->unitInside) {
            FireTrackableEvent(track->jassHandle, 4102);
        }
        track->unitInside = inside;
    }
}

// Advances JASS projectiles (ref CJassProjectile motion): homing records
// re-aim at their live unit target each tick; arrival within the impact
// radius deals the stored damage through the combat system (spell,
// non-attack). Dead/vanished targets fizzle; point targets pop silently.
void JassNatives::UpdateProjectiles(float dt) {
    if (!data_ || !sim_ || dt <= 0.0f || data_->projectiles.empty()) return;
    constexpr float kImpactRadius = 32.0f;
    constexpr float kMaxTravel = 4096.0f;
    std::vector<int> finished;
    for (auto& [id, proj] : data_->projectiles) {
        if (!proj || proj->done) {
            finished.push_back(id);
            continue;
        }
        float destX = proj->tx, destY = proj->ty;
        bool targetAlive = false;
        if (proj->hasUnitTarget) {
            CUnit* target = sim_->GetUnit(proj->targetSimId);
            if (target && !target->IsDead()) {
                destX = target->GetX();
                destY = target->GetY();
                targetAlive = true;
                if (proj->homing) {
                    proj->tx = destX;
                    proj->ty = destY;
                }
            } else {
                finished.push_back(id);
                continue;
            }
        }
        if (!proj->hasUnitTarget && !proj->hasPointTarget) {
            finished.push_back(id);
            continue;
        }
        float dx = destX - proj->x;
        float dy = destY - proj->y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        const float step = proj->speed * dt;
        if (dist <= std::max(kImpactRadius, step)) {
            if (proj->hasUnitTarget && targetAlive && proj->damage != 0.0f) {
                CUnit* target = sim_->GetUnit(proj->targetSimId);
                CUnit* source = proj->casterSimId >= 0 ? sim_->GetUnit(proj->casterSimId) : nullptr;
                if (target) {
                    sim_->GetCombatSystem()->ApplyDamage(*sim_, source, target, false, true,
                                                         CAttackType::SPELLS, CDamageType::NORMAL,
                                                         "", proj->damage, 0.0f, 0, 0);
                }
            }
            finished.push_back(id);
            continue;
        }
        if (dist > 0.0f) {
            proj->x += dx / dist * step;
            proj->y += dy / dist * step;
            proj->facing = std::atan2(dy, dx);
        }
        proj->traveled += step;
        if (proj->traveled > kMaxTravel) {
            finished.push_back(id);
        }
    }
    for (int id : finished) {
        data_->projectiles.erase(id);
        for (auto it = data_->handles.begin(); it != data_->handles.end(); ++it) {
            if (it->second.type == JassHandleInfo::PROJECTILE && it->second.id == id) {
                data_->FreeHandle(it->first);
                break;
            }
        }
    }
}

void JassNatives::FireTickTriggers() {
    if (!data_) return;
    // Snapshot: actions may register/unregister ticks while firing.
    const std::vector<int> onTick(data_->onTickTriggers.begin(), data_->onTickTriggers.end());
    for (int triggerId : onTick) {
        auto it = data_->triggers.find(triggerId);
        if (it == data_->triggers.end() || !it->second) {
            data_->onTickTriggers.erase(triggerId);
            continue;
        }
        JassQueuedContext context = CaptureContext();
        context.currentTriggerId = triggerId;
        QueueTriggerActions(*it->second, context);
    }
    std::vector<std::pair<int, int>> onUnitTick(data_->onUnitTickTriggers.begin(),
                                                data_->onUnitTickTriggers.end());
    for (const auto& [triggerId, unitSim] : onUnitTick) {
        auto it = data_->triggers.find(triggerId);
        if (it == data_->triggers.end() || !it->second) {
            data_->onUnitTickTriggers.erase(triggerId);
            continue;
        }
        if (!sim_ || !sim_->GetUnit(unitSim)) {
            data_->onUnitTickTriggers.erase(triggerId);
            continue;
        }
        JassQueuedContext context = CaptureContext();
        context.currentTriggerId = triggerId;
        context.currentTriggerUnitHandle = data_->AllocHandle(JassHandleInfo::UNIT, unitSim);
        QueueTriggerActions(*it->second, context);
    }
}

// Limit-op evaluation (ref CLimitOp ordinals: LT0 LE1 EQ2 GE3 GT4 NE5)
// over doubles; exact equality like the ref Double comparison.
static bool ApplyLimitOp(int opcode, double value, double limit) {
    switch (opcode) {
        case 0: return value < limit;
        case 1: return value <= limit;
        case 2: return value == limit;
        case 3: return value >= limit;
        case 4: return value > limit;
        case 5: return value != limit;
        default: return false;
    }
}

// Unit-state values mirror the GetUnitState native (LIFE0 MAX_LIFE1
// MANA2 MAX_MANA3); unknown states read 0.
static float GetUnitStateValue(CUnit* unit, int state) {
    if (!unit) return 0.0f;
    switch (state) {
        case 0: return unit->GetLife();
        case 1: return unit->GetMaxLife();
        case 2: return unit->GetMana();
        case 3: return (float)unit->GetMaxMana();
        default: return 0.0f;
    }
}

void JassNatives::FireChatEvent(int fromPlayerHandle, const std::string& message) {
    if (!data_) return;
    const int fromSim = data_->GetSimHandle(fromPlayerHandle, JassHandleInfo::PLAYER);
    for (auto& [triggerId, trigger] : data_->triggers) {
        if (!trigger || trigger->chatEvents.empty()) continue;
        bool matched = false;
        std::string matchedFind;
        for (const auto& ev : trigger->chatEvents) {
            const int evSim = data_->GetSimHandle(ev.playerHandle, JassHandleInfo::PLAYER);
            if (evSim != fromSim) continue;
            if (ev.exact ? message == ev.find : message.find(ev.find) != std::string::npos) {
                matched = true;
                matchedFind = ev.find;
                break;
            }
        }
        if (!matched) continue;
        JassQueuedContext context = CaptureContext();
        context.currentTriggerId = triggerId;
        context.eventTriggerPlayerHandle = fromPlayerHandle;
        context.eventChatString = message;
        context.eventChatMatched = matchedFind;
        QueueTriggerActions(*trigger, context);
    }
    data_->eventChatString = message;
    data_->eventChatMatched.clear();
}

void JassNatives::FireAllianceChange(int playerIdA, int playerIdB) {
    if (!data_) return;
    for (auto& [triggerId, trigger] : data_->triggers) {
        if (!trigger) continue;
        bool matched = false;
        int matchedHandle = 0;
        for (const auto& [handle, code] : trigger->registeredEvents) {
            if (code != 200) continue;
            const int evSim = data_->GetSimHandle(handle, JassHandleInfo::PLAYER);
            if (evSim == playerIdA || evSim == playerIdB) {
                matched = true;
                matchedHandle = handle;
                break;
            }
        }
        if (!matched) continue;
        JassQueuedContext context = CaptureContext();
        context.currentTriggerId = triggerId;
        context.eventTriggerPlayerHandle = matchedHandle;
        QueueTriggerActions(*trigger, context);
    }
}

void JassNatives::FireCinematicSkipEvent(int playerIndex) {
    // ESC during cinematic mode (ref EVENT_PLAYER_END_CINEMATIC = 17, wired
    // by TryInitCinematicBehaviorBJ to CancelCineSceneBJ).
    if (!data_ || playerIndex < 0) return;
    for (auto& [triggerId, trigger] : data_->triggers) {
        if (!trigger) continue;
        bool matched = false;
        int matchedHandle = 0;
        for (const auto& [handle, code] : trigger->registeredEvents) {
            if (code != 17) continue;
            if (data_->GetSimHandle(handle, JassHandleInfo::PLAYER) == playerIndex) {
                matched = true;
                matchedHandle = handle;
                break;
            }
        }
        if (!matched) continue;
        JassQueuedContext context = CaptureContext();
        context.currentTriggerId = triggerId;
        context.currentTriggerEventId = 17;
        context.eventTriggerPlayerHandle = matchedHandle;
        QueueTriggerActions(*trigger, context);
    }
}

void JassNatives::FireStateTriggers() {
    if (!data_ || !sim_) return;
    for (auto& [triggerId, trigger] : data_->triggers) {
        if (!trigger) continue;
        // Unit life/mana thresholds.
        for (auto& ev : trigger->unitStateEvents) {
            int uSim = data_->GetSimHandle(ev.unitHandle, JassHandleInfo::UNIT);
            CUnit* unit = uSim >= 0 ? sim_->GetUnit(uSim) : nullptr;
            const bool now = unit && ApplyLimitOp(ev.opcode, GetUnitStateValue(unit, ev.state), ev.limit);
            if (now && !ev.wasTrue) {
                JassQueuedContext context = CaptureContext();
                context.currentTriggerId = triggerId;
                context.currentTriggerUnitHandle = ev.unitHandle;
                QueueTriggerActions(*trigger, context);
            }
            ev.wasTrue = now;
        }
        // Proximity entry (fires on false -> true with the entering unit).
        for (auto& ev : trigger->unitInRangeEvents) {
            int cSim = data_->GetSimHandle(ev.unitHandle, JassHandleInfo::UNIT);
            CUnit* center = cSim >= 0 ? sim_->GetUnit(cSim) : nullptr;
            int enteringHandle = 0;
            bool inside = false;
            if (center && !center->IsDead() && ev.range > 0.0f) {
                const float rSq = ev.range * ev.range;
                for (CUnit* unit : sim_->GetUnits()) {
                    if (!unit || unit == center || unit->IsDead()) continue;
                    const float dx = unit->GetX() - center->GetX();
                    const float dy = unit->GetY() - center->GetY();
                    if (dx * dx + dy * dy > rSq) continue;
                    if (!ev.filterFunc.empty() && vmPtr_) {
                        const int uh = data_->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
                        const int prev = data_->filterUnitHandle;
                        data_->filterUnitHandle = uh;
                        const bool accepted = JassValueToBool(CallSyncFunction(vmPtr_, ev.filterFunc));
                        data_->filterUnitHandle = prev;
                        data_->FreeHandle(uh);
                        if (!accepted) continue;
                    }
                    inside = true;
                    enteringHandle = data_->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
                    break;
                }
            }
            if (inside && !ev.inside) {
                JassQueuedContext context = CaptureContext();
                context.currentTriggerId = triggerId;
                context.currentTriggerUnitHandle = ev.unitHandle;
                context.eventEnteringUnitHandle = enteringHandle;
                QueueTriggerActions(*trigger, context);
            }
            ev.inside = inside;
        }
        // Player resource thresholds.
        for (auto& ev : trigger->playerStateEvents) {
            const int playerId = ResolvePlayerId(data_, ev.playerHandle);
            const bool now = ApplyLimitOp(ev.opcode, GetPlayerStateValue(*sim_, playerId, ev.state), ev.limit);
            if (now && !ev.wasTrue) {
                JassQueuedContext context = CaptureContext();
                context.currentTriggerId = triggerId;
                context.eventTriggerPlayerHandle = ev.playerHandle;
                QueueTriggerActions(*trigger, context);
            }
            ev.wasTrue = now;
        }
        // Variable watches (int/real globals only).
        for (auto& ev : trigger->varEvents) {
            bool now = false;
            if (!ev.varName.empty() && vmPtr_) {
                const JassValue v = vmPtr_->GetGlobal(ev.varName);
                if (v.type == JassValue::INTEGER) {
                    now = ApplyLimitOp(ev.opcode, (double)v.intVal, ev.limit);
                } else if (v.type == JassValue::REAL) {
                    now = ApplyLimitOp(ev.opcode, (double)v.realVal, ev.limit);
                }
            }
            if (now && !ev.wasTrue) {
                JassQueuedContext context = CaptureContext();
                context.currentTriggerId = triggerId;
                QueueTriggerActions(*trigger, context);
            }
            ev.wasTrue = now;
        }
        // Time-of-day threshold (TIME_OF_DAY only, like ref).
        for (auto& ev : trigger->gameStateEvents) {
            if (ev.state != 2) continue;
            const bool now = ApplyLimitOp(ev.opcode, (double)sim_->GetGameTimeOfDay(), ev.limit);
            if (now && !ev.wasTrue) {
                JassQueuedContext context = CaptureContext();
                context.currentTriggerId = triggerId;
                QueueTriggerActions(*trigger, context);
            }
            ev.wasTrue = now;
        }
    }
}

void JassNatives::NotifyTrackableClick(float x, float y) {
    if (!data_) return;
    constexpr float kHitRadius = 96.0f;
    float best = kHitRadius * kHitRadius;
    int bestHandle = 0;
    for (auto& [id, track] : data_->trackables) {
        (void)id;
        if (!track || track->jassHandle == 0) continue;
        const float dx = track->x - x;
        const float dy = track->y - y;
        const float dist = dx * dx + dy * dy;
        if (dist <= best) {
            best = dist;
            bestHandle = track->jassHandle;
        }
    }
    if (bestHandle != 0) FireTrackableEvent(bestHandle, 4101);
}

void JassNatives::FireDialogButtonEvent(int buttonJassHandle) {
    if (!data_ || buttonJassHandle == 0) return;
    int btnSim = data_->GetSimHandle(buttonJassHandle, JassHandleInfo::BUTTON);
    if (btnSim < 0) return;
    auto bit = data_->dialogButtons.find(btnSim);
    if (bit == data_->dialogButtons.end()) return;
    // Find the parent dialog handle for GetClickedDialog.
    int dialogJass = 0;
    for (const auto& [handle, info] : data_->handles) {
        if (info.type == JassHandleInfo::DIALOG && info.id == bit->second->dialogSimId) {
            dialogJass = handle;
            break;
        }
    }
    for (auto& [triggerId, trigger] : data_->triggers) {
        if (!trigger) continue;
        bool matches = false;
        for (const auto& event : trigger->registeredEvents) {
            if ((event.second == 4202 && event.first == buttonJassHandle) ||
                (event.second == 4201 && event.first == dialogJass)) {
                matches = true;
                break;
            }
        }
        if (!matches) continue;
        JassQueuedContext context = CaptureContext();
        context.currentTriggerId = triggerId;
        context.eventClickedButtonHandle = buttonJassHandle;
        context.eventClickedDialogHandle = dialogJass;
        QueueTriggerActions(*trigger, context);
    }
    data_->eventClickedButtonHandle = buttonJassHandle;
    data_->eventClickedDialogHandle = dialogJass;
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

void JassNatives::SetBoundGameUI(GameUI* gui) {
    if (data_) data_->boundGameUi = gui;
}

GameUI* JassNatives::GetBoundGameUI() const {
    return data_ ? data_->boundGameUi : nullptr;
}

UIFrame* JassNatives::GetLiveUIFrame(int frameJassHandle) const {
    if (!data_) return nullptr;
    int simId = data_->GetSimHandle(frameJassHandle, JassHandleInfo::FRAME);
    if (simId < 0) return nullptr;
    auto it = data_->frames.find(simId);
    if (it == data_->frames.end() || !it->second) return nullptr;
    return it->second->uiNode.get();
}

void JassNatives::FireFrameEvent(int frameSimId, int eventCode, float value, const std::string& text) {
    if (!data_ || frameSimId < 0) return;
    int frameJassHandle = data_->AllocHandle(JassHandleInfo::FRAME, frameSimId);
    for (auto& [trigId, trigPtr] : data_->triggers) {
        (void)trigId;
        if (!trigPtr || !trigPtr->enabled) continue;
        bool matched = false;
        for (const auto& reg : trigPtr->frameEvents) {
            if (reg.frameSimId == frameSimId && (reg.eventCode == eventCode || reg.eventCode == 0)) {
                matched = true;
                break;
            }
        }
        if (!matched) continue;
        JassQueuedContext ctx = CaptureContext();
        ctx.currentTriggerId = trigPtr->id;
        ctx.currentTriggerEventId = eventCode;
        ctx.eventTriggerFrameHandle = frameJassHandle;
        ctx.eventTriggerFrameEvent = eventCode;
        ctx.eventTriggerFrameValue = value;
        ctx.eventTriggerFrameText = text;
        if (EvaluateTriggerConditions(*trigPtr, ctx)) {
            QueueTriggerActions(*trigPtr, ctx);
        }
    }
}

void JassNatives::WireSimulationEvents(CSimulation& sim) {
    sim.SetUnitDeathCallback(
        [this, &sim](int unitHandle, int killerHandle, int) {
            FireDeathEvent(unitHandle, killerHandle, JASS_EVENT_UNIT_DEATH);
            if (CUnit* dead = sim.GetUnit(unitHandle)) {
                if (dead->IsHero()) FireRevivableEvent(unitHandle);
            }
        }
    );
    sim.SetUnitDamageCallback(
        [this](int unitHandle, int sourceHandle, float damage) {
            FireDamageEvent(unitHandle, sourceHandle, damage, JASS_EVENT_UNIT_DAMAGED);
        }
    );
    sim.SetUnitOrderCallback(
        [this](int unitHandle, int orderId, int targetHandle, float x, float y) {
            FireOrderEvent(unitHandle, orderId, targetHandle, x, y);
        }
    );
    sim.SetItemEventCallback(
        [this](int unitHandle, int itemHandle, int eventType) {
            FireItemEvent(unitHandle, itemHandle, eventType);
        }
    );
    sim.SetUnitLoadedCallback(
        [this](int transportHandle, int passengerHandle) {
            FireLoadedEvent(transportHandle, passengerHandle);
        }
    );
    sim.SetUnitDecayCallback(
        [this](int unitHandle) {
            FireDecayEvent(unitHandle);
        }
    );
    sim.SetShopEventCallback(
        [this](int buyerHandle, int sellerHandle, int itemHandle) {
            FireSellItemEvent(buyerHandle, sellerHandle, itemHandle);
        }
    );
    sim.SetShopUnitEventCallback(
        [this](int buyerHandle, int sellerHandle, int hiredHandle) {
            FireSellUnitEvent(buyerHandle, sellerHandle, hiredHandle);
        }
    );
    sim.SetPawnEventCallback(
        [this](int buyerHandle, int sellerHandle, int itemHandle) {
            FirePawnItemEvent(sellerHandle, buyerHandle, itemHandle);
        }
    );
    sim.SetHeroSkillCallback(
        [this](int unitHandle, int skillId, int newLevel) {
            FireHeroSkillEvent(unitHandle, skillId, newLevel);
        }
    );
    sim.SetQueueEventCallback(
        [this](int buildingHandle, int typeId, int kind, bool started) {
            if (kind == 0) {
                if (started) FireTrainStartEvent(buildingHandle, typeId);
                else FireTrainCancelEvent(buildingHandle, typeId);
            } else if (kind == 1) {
                if (started) FireResearchStartEvent(buildingHandle, typeId);
                else FireResearchCancelEvent(buildingHandle, typeId);
            } else if (kind == 3) {
                if (started) FireReviveStartEvent(buildingHandle, typeId);
                else FireReviveCancelEvent(buildingHandle, typeId);
            } else {
                if (started) FireUpgradeStartEvent(buildingHandle, typeId);
                else FireUpgradeFinishEvent(buildingHandle, typeId);
            }
        }
    );
    sim.SetConstructEventCallback(
        [this](int buildingHandle, bool finished) {
            if (finished) FireConstructFinishEvent(buildingHandle);
            else FireConstructCancelEvent(buildingHandle);
        }
    );
    sim.SetOwnerChangedCallback(
        [this](int unitHandle, int prevOwnerIndex) {
            FireChangeOwnerEvent(unitHandle, prevOwnerIndex);
        }
    );
    sim.SetItemRemovedCallback(
        [this](int itemHandle) {
            ForgetItem(itemHandle);
        }
    );
    sim.SetUnitTrainedCallback(
        [this](int buildingHandle, int trainedHandle) {
            FireTrainedEvent(buildingHandle, trainedHandle);
        }
    );
    sim.SetUnitSummonedCallback(
        [this](int summonerHandle, int summonedHandle) {
            FireSummonEvent(summonerHandle, summonedHandle);
        }
    );
    sim.SetResearchFinishedCallback(
        [this](int unitHandle, int upgradeId) {
            FireResearchEvent(unitHandle, upgradeId);
        }
    );
    sim.SetHeroRevivedCallback(
        [this](int reviverHandle, int revivedHandle) {
            FireReviveEvent(reviverHandle, revivedHandle);
        }
    );
    sim.SetSpellEventCallback(
        [this](int casterHandle, int stage, int targetSimHandle, float x, float y) {
            FireSpellEvent(casterHandle, stage, targetSimHandle, x, y);
        }
    );
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
    RegisterTerrainEditNatives();
    RegisterHashtableNatives();
    RegisterGameCacheNatives();
    RegisterProjectileNatives();
    RegisterTargetingNatives();
    RegisterBonusNatives();
    RegisterMiscNatives();
    RegisterFrameNatives();
    RegisterAbilityBuilderNatives();
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
        if (args[0].type == JassValue::STRING) {
            return JassValue::Int(OrderIdFromString(args[0].strVal));
        }
        return JassValue::Int(args[0].intVal);
    });
    vm_.RegisterNative("OrderId2String", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("");
        const std::string tableName = OrderStringFromId(args[0].intVal);
        if (!tableName.empty()) return JassValue::Str(tableName);
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
    // Bitwise natives (ref Jass2 BlzBitAnd/Or/Xor): plain int ops.
    vm_.RegisterNative("BlzBitAnd", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Int(0);
        return JassValue::Int(args[0].intVal & args[1].intVal);
    });
    vm_.RegisterNative("BlzBitOr", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Int(0);
        return JassValue::Int(args[0].intVal | args[1].intVal);
    });
    vm_.RegisterNative("BlzBitXor", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Int(0);
        return JassValue::Int(args[0].intVal ^ args[1].intVal);
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
        // Retail/ref parity: Java String.hashCode() (ref Jass2 StringHash),
        // NOT std::hash (which varies per process/platform and desyncs any
        // map using StringHash as a hashtable key). h = 31*h + c per UTF-16
        // code unit; JASS strings here are byte strings so hash per byte.
        const std::string& s = args[0].strVal;
        int32_t h = 0;
        for (unsigned char c : s) h = (int32_t)(31 * h + c);
        return JassValue::Int((int)h);
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
        "ConvertRarityControl", "ConvertVersion",
        // Warsmash abilitybuilder / targeting converts (ref Jass2): opaque
        // int ordinals, passthrough like the retail set above.
        "ConvertAbilityCategory", "ConvertAbilityDisableType", "ConvertAutocastType",
        "ConvertBehaviorCategory", "ConvertColor", "ConvertDataFieldLetter",
        "ConvertNonStackingStatBonusType", "ConvertOrderButtonType", "ConvertResourceType",
        "ConvertStateModType", "ConvertTargetType", "ConvertTextTagConfigType",
        "ConvertWorldEditorDataType", "ConvertABConfType",
    };
    for (auto name : convertNames) vm_.RegisterNative(name, convertPassthrough);
    vm_.RegisterNative("ConvertPlayerColor", convertPassthrough);

    vm_.RegisterNative("GetLocalizedString", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("");
        return JassValue::Str(args[0].strVal);
    });
    // Rawcode helpers (ref Jass2 string2Rawcode/Rawcode2String): FourCC is
    // the retail alias of String2Rawcode.
    auto rawcodeFromStr = [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int(RawcodeFromString(args[0].strVal));
    };
    vm_.RegisterNative("String2Rawcode", rawcodeFromStr);
    vm_.RegisterNative("FourCC", rawcodeFromStr);
    vm_.RegisterNative("Rawcode2String", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("");
        return JassValue::Str(RawcodeToString(args[0].intVal));
    });
    // Ref WarsmashGetRawcode2String (War3ID.toString) matches Rawcode2String.
    vm_.RegisterNative("WarsmashGetRawcode2String", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("");
        return JassValue::Str(RawcodeToString(args[0].intVal));
    });
    // Damage-type lookup by name (ref CDamageType.valueOf): map common.j
    // names to ConvertDamageType ordinals, unknown -> 0.
    vm_.RegisterNative("String2DamageType", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        static const std::unordered_map<std::string, int> kDamageTypes = {
            {"UNKNOWN", 0}, {"NORMAL", 4}, {"ENHANCED", 5}, {"FIRE", 8},
            {"COLD", 9}, {"LIGHTNING", 10}, {"POISON", 11}, {"DISEASE", 12},
            {"DIVINE", 13}, {"MAGIC", 14}, {"SONIC", 15}, {"ACID", 16},
            {"FORCE", 17}, {"DEATH", 18}, {"MIND", 19}, {"PLANT", 20},
            {"DEFENSIVE", 21}, {"DEMOLITION", 22}, {"SLOW_POISON", 23},
            {"SPIRIT_LINK", 24}, {"SHADOW_STRIKE", 25}, {"UNIVERSAL", 26},
        };
        std::string key = args[0].strVal;
        for (auto& c : key) c = (char)std::toupper((unsigned char)c);
        auto it = kDamageTypes.find(key);
        return JassValue::Int(it != kDamageTypes.end() ? it->second : 0);
    });
    // Target-type lookup (ref CTargetType.parseTargetType): ordinals follow
    // the ref declaration order so ConvertTargetType(i) round-trips.
    // Unknown -> 0 (null targettype handle in JASS terms).
    vm_.RegisterNative("ParseTargetType", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        const int ordinal = ParseRefTargetOrdinal(args[0].strVal);
        return JassValue::Int(ordinal >= 0 ? ordinal : 0);
    });
    // Buff-type lookups (ref NonStackingStatBuffType/StateModBuffType
    // valueOf): ref declaration ordinals; unknown -> 0.
    vm_.RegisterNative("String2NonStackingStatBonusType", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        static const std::unordered_map<std::string, int> kTypes = {
            {"MELEEATK", 0}, {"MELEEATKPCT", 1}, {"RNGDATK", 2}, {"RNGDATKPCT", 3},
            {"ATKSPD", 4}, {"DEF", 5}, {"DEFPCT", 6}, {"HPGEN", 7}, {"HPGENPCT", 8},
            {"MAXHPGENPCT", 9}, {"MPGEN", 10}, {"MPGENPCT", 11}, {"MAXMPGENPCT", 12},
            {"MVSPD", 13}, {"MVSPDPCT", 14}, {"HPSTEAL", 15}, {"THORNS", 16}, {"THORNSPCT", 17},
            {"MAXHP", 18}, {"MAXHPPCT", 19}, {"MAXMP", 20}, {"MAXMPPCT", 21},
            {"ALLATK", 22}, {"ALLATKPCT", 23},
        };
        std::string key = args[0].strVal;
        for (auto& c : key) c = (char)std::toupper((unsigned char)c);
        auto it = kTypes.find(key);
        return JassValue::Int(it != kTypes.end() ? it->second : 0);
    });
    vm_.RegisterNative("String2StateModType", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        static const std::unordered_map<std::string, int> kTypes = {
            {"ETHEREAL", 0}, {"RESISTANT", 1}, {"SLEEPING", 2}, {"STUN", 3},
            {"MAGIC_IMMUNE", 4}, {"MORPH_IMMUNE", 5}, {"SNARED", 6},
            {"DISABLE_AUTO_ATTACK", 7}, {"DISABLE_AUTO_CAST", 8}, {"DISABLE_ASSIST_ALLY", 9},
            {"DISABLE_ATTACK", 10}, {"DISABLE_MELEE_ATTACK", 11}, {"DISABLE_RANGED_ATTACK", 12},
            {"DISABLE_SPECIAL_ATTACK", 13}, {"DISABLE_SPELLS", 14}, {"DISABLE_UNIT_COLLISION", 15},
            {"DISABLE_BUILDING_COLLISION", 16}, {"INVULNERABLE", 17}, {"INVISIBLE", 18},
            {"DETECTOR", 19}, {"DETECTED", 20},
        };
        std::string key = args[0].strVal;
        for (auto& c : key) c = (char)std::toupper((unsigned char)c);
        auto it = kTypes.find(key);
        return JassValue::Int(it != kTypes.end() ? it->second : 0);
    });
}

// ============================================================
// 4. Random
// ============================================================
void JassNatives::RegisterRandomNatives() {
    // Synced game RNG (ref lockstep): rolls consume the sim stream so
    // replays reproduce; SetRandomSeed reseeds the whole game RNG.
    vm_.RegisterNative("GetRandomInt", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Int(0);
        int low = args[0].intVal, high = args[1].intVal;
        if (low > high) std::swap(low, high);
        const float r = sim ? sim->SimRand01() : JassFallbackRand01();
        const int range = high - low + 1;
        return JassValue::Int(low + (range > 0 ? (int)(r * (float)range) % range : 0));
    });
    vm_.RegisterNative("GetRandomReal", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Real(0);
        float low = args[0].realVal, high = args[1].realVal;
        if (low > high) std::swap(low, high);
        const float r = sim ? sim->SimRand01() : JassFallbackRand01();
        return JassValue::Real(low + r * (high - low));
    });
    vm_.RegisterNative("SetRandomSeed", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        if (sim) {
            sim->SeedRng((uint32_t)args[0].intVal);
        } else {
            JassFallbackRandState() = (uint32_t)args[0].intVal;
        }
        return JassValue::Int(0);
    });
}

// ============================================================
// 5. Timer natives
// ============================================================
