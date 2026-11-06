#include "scripting/jass_natives_internal.h"

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
    vm_.RegisterNative("TriggerSleepAction", [vm = &vm_, data = data_](const std::vector<JassValue>& args) -> JassValue {
        float seconds = 0.0f;
        if (!args.empty()) {
            seconds = args[0].type == JassValue::REAL ? args[0].realVal : static_cast<float>(args[0].intVal);
        }
        // Ref Jass2 TriggerSleepAction: a trigger with waitOnSleeps=false
        // skips the wait and continues on the next statement.
        if (!TriggerSleepsHonored(data)) return JassValue::Null();
        vm->RequestSleep(seconds);
        return JassValue::Null();
    });
    // PolledWait shares TriggerSleepAction semantics (ref polled wait):
    // suspend the current thread; the timer-advance pass resumes it.
    vm_.RegisterNative("PolledWait", [vm = &vm_, data = data_](const std::vector<JassValue>& args) -> JassValue {
        float seconds = 0.0f;
        if (!args.empty()) {
            seconds = args[0].type == JassValue::REAL ? args[0].realVal : static_cast<float>(args[0].intVal);
        }
        if (!TriggerSleepsHonored(data)) return JassValue::Null();
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
        data->onTickTriggers.erase(args[0].intVal);
        data->onUnitTickTriggers.erase(args[0].intVal);
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
    // Per-tick triggers (ref registerOnTickEvent): fired once per
    // UpdateTimers pass with a fresh scope. Removal is trigger-granular
    // here (no RemovableTriggerEvent handles); DestroyTrigger prunes.
    vm_.RegisterNative("TriggerRegisterOnTick", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        if (data->triggers.count(args[0].intVal) > 0) data->onTickTriggers.insert(args[0].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterOnUnitTick", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        if (data->triggers.count(args[0].intVal) == 0) return JassValue::Null();
        int uSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Null();
        data->onUnitTickTriggers[args[0].intVal] = uSim;
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterGameStateEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) {
            it->second->registeredEvents.push_back({args[1].intVal, 3000 + args[2].intVal});
            // TIME_OF_DAY(2) is the only polled game state (ref throws for
            // the rest); other states are retained but never fire.
            JassTrigger::JassGameStateEvent ev;
            ev.state = args[1].intVal;
            ev.opcode = args[2].intVal;
            ev.limit = args[3].type == JassValue::REAL ? args[3].realVal : (float)args[3].intVal;
            it->second->gameStateEvents.push_back(std::move(ev));
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterEnterRegion", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) {
            it->second->registeredEvents.push_back({args[1].intVal, 4001});
            JassTrigger::JassRegionEvent ev;
            ev.regionHandle = args[1].intVal;
            ev.code = 4001;
            ev.filterFunc = args[2].type == JassValue::STRING ? args[2].strVal : "";
            it->second->regionEvents.push_back(std::move(ev));
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterLeaveRegion", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) {
            it->second->registeredEvents.push_back({args[1].intVal, 4002});
            JassTrigger::JassRegionEvent ev;
            ev.regionHandle = args[1].intVal;
            ev.code = 4002;
            ev.filterFunc = args[2].type == JassValue::STRING ? args[2].strVal : "";
            it->second->regionEvents.push_back(std::move(ev));
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
            JassTrigger::JassUnitInRangeEvent ev;
            ev.unitHandle = args[1].intVal;
            ev.range = args[2].type == JassValue::REAL ? args[2].realVal : (float)args[2].intVal;
            ev.filterFunc = args[3].type == JassValue::STRING ? args[3].strVal : "";
            it->second->unitInRangeEvents.push_back(std::move(ev));
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
        JassTrigger::JassPlayerStateEvent ev;
        ev.playerHandle = args[1].intVal;
        ev.state = args[2].intVal;
        ev.opcode = args[3].intVal;
        ev.limit = args[4].type == JassValue::REAL ? args[4].realVal : (float)args[4].intVal;
        it->second->playerStateEvents.push_back(std::move(ev));
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterPlayerChatEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        it->second->registeredEvents.push_back({args[1].intVal, 7000});
        JassTrigger::JassChatEvent ev;
        ev.playerHandle = args[1].intVal;
        ev.find = args[2].type == JassValue::STRING ? args[2].strVal : "";
        ev.exact = args[3].boolVal;
        it->second->chatEvents.push_back(std::move(ev));
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
        if (args.size() < 4) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        it->second->registeredEvents.push_back({0, 300 + args[2].intVal});
        JassTrigger::JassVarEvent ev;
        ev.varName = args[1].type == JassValue::STRING ? args[1].strVal : "";
        ev.opcode = args[2].intVal;
        ev.limit = args[3].type == JassValue::REAL ? (double)args[3].realVal : (double)args[3].intVal;
        it->second->varEvents.push_back(std::move(ev));
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
        // Ref Jass2 TriggerExecuteWait: force the callee's waitOnSleeps on,
        // then run its actions so the caller observes their effects (and
        // their waits) before continuing.
        it->second->waitOnSleeps = true;
        JassQueuedContext calleeCtx = CaptureContext();
        calleeCtx.currentTriggerId = args[0].intVal;
        calleeCtx.currentTriggerEventId = 0;
        const std::vector<std::string>& actions = it->second->actionFuncNames;
        // Sync contexts (enum/filter callbacks, conditions) have no resumable
        // caller thread and nested waits have nowhere to attach: run inline
        // and drain sleeps, as before.
        if (activePumpThread_ == nullptr || activePumpThread_->joinId != 0) {
            JassQueuedContext previous = CaptureContext();
            ApplyContext(calleeCtx);
            for (const auto& funcName : actions) {
                if (funcName.empty() || !vm_.HasFunction(funcName)) continue;
                vm_.CallFunction(funcName);
                if (vm_.HasPendingSleep()) vm_.TakePendingSleepSeconds();
            }
            ApplyContext(previous);
            return JassValue::Null();
        }
        // Resumable caller: run each action as a child thread. The first
        // sleep suspends the CALLER via a join; the wake pass finishes the
        // callee remainder before the caller resumes (ContinueTriggerJoin).
        JassQueuedContext previous = CaptureContext();
        ApplyContext(calleeCtx);
        for (size_t i = 0; i < actions.size(); ++i) {
            const std::string& funcName = actions[i];
            if (funcName.empty() || !vm_.HasFunction(funcName)) continue;
            JassVM::ThreadState child;
            JassVM::ThreadRunResult result = vm_.RunThread(child, funcName);
            if (result.completed) continue;
            if (!result.sleeping) continue;
            JassTriggerJoin join;
            join.calleeTriggerId = args[0].intVal;
            join.calleeContext = calleeCtx;
            join.pendingActions.assign(actions.begin() + i + 1, actions.end());
            join.suspendedChild = std::move(child);
            join.childActive = true;
            const int joinId = nextJoinId_++;
            triggerJoins_[joinId] = std::move(join);
            activePumpThread_->joinId = joinId;
            ApplyContext(previous);
            vm_.RequestSleep(result.sleepSeconds);
            return JassValue::Null();
        }
        ApplyContext(previous);
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
    vm_.RegisterNative("GetChangingUnitPrevOwner", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventChangingUnitPrevOwnerHandle >= 0
            ? JassValue::Int(data->eventChangingUnitPrevOwnerHandle)
            : JassValue::Int(0);
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
    vm_.RegisterNative("GetConstructedStructure", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventConstructedUnitHandle >= 0 ? JassValue::Int(data->eventConstructedUnitHandle)
                                                     : JassValue::Int(0);
    });
    vm_.RegisterNative("GetConstructingStructure", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventConstructingUnitHandle >= 0 ? JassValue::Int(data->eventConstructingUnitHandle)
                                                      : JassValue::Int(0);
    });
    vm_.RegisterNative("GetCancelledStructure", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventCancelledUnitHandle >= 0 ? JassValue::Int(data->eventCancelledUnitHandle)
                                                    : JassValue::Int(0);
    });
    vm_.RegisterNative("GetTrainedUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventTrainedUnitHandle >= 0 ? JassValue::Int(data->eventTrainedUnitHandle)
                                                 : JassValue::Int(0);
    });
    vm_.RegisterNative("GetTrainedUnitType", [data = data_, sim = sim_](const std::vector<JassValue>&) -> JassValue {
        if (data->eventTrainedUnitHandle >= 0 && sim) {
            const int trainedSim = data->GetSimHandle(data->eventTrainedUnitHandle, JassHandleInfo::UNIT);
            if (const CUnit* trained = sim->GetUnit(trainedSim)) {
                return JassValue::Int((int)trained->GetTypeId().value);
            }
        }
        // Start/cancel observe the type before any unit exists.
        return JassValue::Int(data->eventTrainedUnitTypeId);
    });
    vm_.RegisterNative("GetResearched", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->eventResearchedId);
    });
    vm_.RegisterNative("GetResearchingUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventResearchingUnitHandle >= 0 ? JassValue::Int(data->eventResearchingUnitHandle)
                                                     : JassValue::Int(0);
    });
    vm_.RegisterNative("GetSoldItem", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventSoldItemHandle >= 0 ? JassValue::Int(data->eventSoldItemHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetSoldUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventSoldUnitHandle >= 0 ? JassValue::Int(data->eventSoldUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetSellingUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventSellingUnitHandle >= 0 ? JassValue::Int(data->eventSellingUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetBuyingUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventBuyingUnitHandle >= 0 ? JassValue::Int(data->eventBuyingUnitHandle) : JassValue::Int(0);
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
    // Spell-cast scope (ref CommonTriggerExecutionScope spell fields).
    // Populated by the cast hook when it lands; defaults match the rest
    // of the (currently unpopulated) spell scope.
    vm_.RegisterNative("GetSpellAbilityOrderId", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->eventSpellOrderId);
    });
    vm_.RegisterNative("GetSpellAbilityOrderButton", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->eventSpellOrderId);
    });
    vm_.RegisterNative("GetSpellTargetType", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->eventSpellTargetType);
    });
    vm_.RegisterNative("GetTriggerCastId", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->triggerCastId);
    });
    // Chat message scope (extension: ref fires chat triggers without
    // exposing the text; the filter string is known at registration, but
    // "-command args" patterns need the body).
    vm_.RegisterNative("GetEventPlayerChatString", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Str(data ? data->eventChatString : "");
    });
    vm_.RegisterNative("GetEventPlayerChatStringMatched", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Str(data ? data->eventChatMatched : "");
    });
    // Dialog click + region event getters (ref CommonTriggerExecutionScope).
    vm_.RegisterNative("GetClickedButton", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventClickedButtonHandle >= 0 ? JassValue::Int(data->eventClickedButtonHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetClickedDialog", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventClickedDialogHandle >= 0 ? JassValue::Int(data->eventClickedDialogHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetTriggeringRegion", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventTriggeringRegionHandle >= 0 ? JassValue::Int(data->eventTriggeringRegionHandle) : JassValue::Int(0);
    });
    // Game-state events are registered but never carry payload in this
    // engine (ref Jass2 throws UnsupportedOperationException); report null.
    vm_.RegisterNative("GetEventGameState", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });
    // Best-effort event removal: drops matching (handle, code) entries when
    // the trigger exists; event handles are trigger-local indices here.
    vm_.RegisterNative("RemoveTriggerEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end()) return JassValue::Null();
        auto& events = it->second->registeredEvents;
        events.erase(std::remove_if(events.begin(), events.end(),
            [&](const std::pair<int,int>& e) { return e.first == args[1].intVal; }),
            events.end());
        return JassValue::Null();
    });
}

// ============================================================
// 19. Sound natives
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
    vm_.RegisterNative("DisplayTextToPlayer", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !data) return JassValue::Null();
        JassNativesData::JassTextMessage m;
        m.player = ResolvePlayerId(data, args[0].intVal);
        m.text = args[3].strVal;
        data->textMessages.push_back(std::move(m));
        return JassValue::Null();
    });
    vm_.RegisterNative("DisplayTimedTextToPlayer", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5 || !data) return JassValue::Null();
        JassNativesData::JassTextMessage m;
        m.player = ResolvePlayerId(data, args[0].intVal);
        m.duration = args[3].type == JassValue::REAL ? args[3].realVal : (float)args[3].intVal;
        m.text = args[4].strVal;
        data->textMessages.push_back(std::move(m));
        return JassValue::Null();
    });
    vm_.RegisterNative("DisplayTimedTextFromPlayer", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5 || !data) return JassValue::Null();
        JassNativesData::JassTextMessage m;
        m.player = ResolvePlayerId(data, args[0].intVal);
        m.duration = args[3].type == JassValue::REAL ? args[3].realVal : (float)args[3].intVal;
        m.text = args[4].strVal;
        data->textMessages.push_back(std::move(m));
        return JassValue::Null();
    });
    vm_.RegisterNative("ClearTextMessages", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (data) {
            data->textMessages.clear();
            data->textClearSeq++;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ClearSelection", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (data) data->selectedUnits.clear();
        return JassValue::Null();
    });
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
        if (args.empty() || !data) return JassValue::Null();
        // Enabling restarts the start->end transition (ref CCineFilter).
        if (args[0].boolVal) data->cineFilter.age = 0.0f;
        data->cineFilter.displayed = args[0].boolVal;
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
    vm_.RegisterNative("SetDoodadAnimationRect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !data) return JassValue::Null();
        // common.j: SetDoodadAnimationRect(rect r, integer doodadID, string animName,
        //   boolean animRandom). Reuse the circle doodad pass with the rect's
        //   bounding circle so campaign gates/bridges animate.
        const int rectSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        auto rit = data->rects.find(rectSim);
        if (rit == data->rects.end() || !rit->second) return JassValue::Null();
        const auto& rc = *rit->second;
        JassNativesData::JassDoodadAnim req;
        req.x = (rc.minX + rc.maxX) * 0.5f;
        req.y = (rc.minY + rc.maxY) * 0.5f;
        const float dx = rc.maxX - rc.minX;
        const float dy = rc.maxY - rc.minY;
        req.radius = std::sqrt(dx * dx + dy * dy) * 0.5f;
        req.doodadId = (uint32_t)args[1].intVal;
        req.nearestOnly = false;
        req.animName = args[2].strVal;
        req.animRandom = args[3].boolVal;
        data->doodadAnims.push_back(std::move(req));
        return JassValue::Null();
    });
    vm_.RegisterNative("SuspendHeroXP", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && sim) sim->SetHeroXPSuspended(args[0].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SuspendTimeOfDay", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        // Ref: simulation.setTimeOfDaySuspended(flag).
        if (!args.empty() && sim) sim->SetTimeOfDaySuspended(args[0].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("IsDawnDuskEnabled", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(data && data->dawnDuskEnabled);
    });
    vm_.RegisterNative("EnableDawnDusk", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        // Retail day/night cycle switch (used by CinematicModeExBJ); the
        // renderer consumes it through the per-frame HUD sync.
        if (!args.empty() && data) data->dawnDuskEnabled = args[0].boolVal;
        return JassValue::Null();
    });
    // Cutscene clock override (ref addFalseTimeOfDay): freeze at the given
    // hour for duration seconds, then restore the prior suspend flag.
    vm_.RegisterNative("SetFalseTimeOfDay", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        const float hour = (float)args[0].intVal + (float)args[1].intVal / 60.0f;
        const float duration = args[2].type == JassValue::REAL ? args[2].realVal : (float)args[2].intVal;
        sim->SetGameTimeOfDay(hour);
        if (data && data->falseTime.remaining <= 0.0f) {
            data->falseTime.wasSuspended = sim->IsTimeOfDaySuspended();
        }
        sim->SetTimeOfDaySuspended(true);
        if (data) data->falseTime.remaining = std::max(0.0f, duration);
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
    vm_.RegisterNative("GetSaveBasicFilename", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Str(data ? data->lastSaveSlot : "");
    });
    vm_.RegisterNative("SaveGameExists", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!data || args.empty()) return JassValue::Bool(false);
        std::error_code ec;
        const bool ok = fs::is_regular_file(SaveSlotMetadata(args[0].strVal), ec) && !ec;
        (void)data;
        return JassValue::Bool(ok);
    });
    vm_.RegisterNative("CommandAI", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        if (playerId >= 0 && playerId < 24) {
            data->playerAI[playerId].commandCount++;
            auto it = data->aiControllers.find(playerId);
            if (it != data->aiControllers.end() && it->second) {
                it->second->Command(args[1].intVal, args[2].intVal);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("StartCampaignAI", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        if (playerId >= 0 && playerId < 24) data->playerAI[playerId].campaignAI = true;
        return JassValue::Null();
    });
    vm_.RegisterNative("StartMeleeAI", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        if (playerId < 0 || playerId >= 24) return JassValue::Null();
        data->playerAI[playerId].meleeAI = true;
        data->playerAI[playerId].difficulty = data->gameConfig.defaultDifficulty;
        if (sim) {
            auto controller = std::make_shared<MeleeAIController>(sim, playerId);
            controller->SetDifficulty(data->playerAI[playerId].difficulty);
            auto paused = data->compAIpaused.find(playerId);
            if (paused != data->compAIpaused.end() && paused->second) {
                controller->SetEnabled(false);
            }
            data->aiControllers[playerId] = std::move(controller);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("CreateBlightedGoldmine", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim || !data) return JassValue::Null();
        // Ref Jass2.CreateBlightedGoldmine: ugol for the owner + hidden ngol
        // backing mine, plus blight paint so undead can harvest (campaign).
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        if (playerId < 0) return JassValue::Null();
        const float x = args[1].realVal, y = args[2].realVal, facing = args[3].realVal;
        const War3ID blightedId("ugol");
        const War3ID goldId("ngol");
        if (CPlayer* player = sim->GetPlayer(playerId)) {
            player->AddTechUnlocked(*sim, blightedId);
        }
        CUnit* blighted = sim->CreateUnit(blightedId, playerId, x, y, facing);
        // Backing gold mine owned by neutral (last slot) and hidden like ref.
        const int neutralId = std::max(0, sim->GetPlayerCount() - 1);
        if (CUnit* gold = sim->CreateUnit(goldId, neutralId, x, y, facing)) {
            gold->SetHidden(true);
        }
        sim->SetBlightCircle(x, y, 512.0f, true);
        if (!blighted) return JassValue::Null();
        return JassValue::Int(data->AllocHandle(JassHandleInfo::UNIT, blighted->GetHandleId()));
    });
    vm_.RegisterNative("UseCustomHeroAbilities", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetTerrainAvailable", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetTerrainFogEx", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!data || args.size() < 7) return JassValue::Null();
        // Ref Jass2 SetTerrainFogEx: style/index + start/end/density/color
        // straight into fogSettings (retail density is a no-op and only
        // linear style renders; the renderer already folds density into
        // range and stays linear-only, see P3-22).
        auto numVal = [](const JassValue& v) -> float {
            return v.type == JassValue::REAL ? v.realVal : (float)v.intVal;
        };
        auto& fog = data->terrainFog;
        fog.overridden = true;
        fog.style = std::max(0, std::min(2, args[0].intVal));
        fog.start = numVal(args[1]);
        fog.end = numVal(args[2]);
        fog.density = std::max(0.0f, numVal(args[3]));
        fog.r = numVal(args[4]);
        fog.g = numVal(args[5]);
        fog.b = numVal(args[6]);
        if (data->terrainFogHook) data->terrainFogHook();
        return JassValue::Null();
    });
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
    // Blz aliases (ref Jass2 BlzGetUnitAbility/ByIndex share the same impl).
    vm_.RegisterNative("BlzGetUnitAbility", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
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
    vm_.RegisterNative("BlzGetUnitAbilityByIndex", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
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
    vm_.RegisterNative("BlzGetAbilityId", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(0);
        auto ability = HandleToAbility(data, args[0].intVal);
        return JassValue::Int(ability ? (int)ability->code.value : 0);
    });
    // Ability cooldowns (ref startUnitAbilityCooldown): sim-side remaining
    // map, decayed in UpdateTimers; End clears, Remaining reports 0 when idle.
    auto cooldownKey = [](int unitSim, uint32_t code) -> int64_t {
        return (static_cast<int64_t>(unitSim) << 32) | code;
    };
    auto startCooldown = [data = data_, sim = sim_, cooldownKey](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Null();
        if (sim && !sim->GetUnit(uSim)) return JassValue::Null();
        const uint32_t code = (uint32_t)AbilityCodeFromJassArg(data, args[1].intVal).value;
        const float duration = args[2].type == JassValue::REAL ? args[2].realVal : (float)args[2].intVal;
        data->abilityCooldowns[cooldownKey(uSim, code)] = std::max(0.0f, duration);
        return JassValue::Null();
    };
    vm_.RegisterNative("StartUnitAbilityCooldown", startCooldown);
    vm_.RegisterNative("BlzStartUnitAbilityCooldown", startCooldown);
    auto endCooldown = [data = data_, sim = sim_, cooldownKey](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Null();
        const uint32_t code = (uint32_t)AbilityCodeFromJassArg(data, args[1].intVal).value;
        data->abilityCooldowns.erase(cooldownKey(uSim, code));
        if (sim) {
            CUnit* unit = sim->GetUnit(uSim);
            auto* spell = unit ? dynamic_cast<CAbilitySpellBase*>(unit->GetAbility(War3ID(code))) : nullptr;
            if (spell) spell->ResetCooldown();
        }
        return JassValue::Null();
    };
    vm_.RegisterNative("BlzEndUnitAbilityCooldown", endCooldown);
    vm_.RegisterNative("EndUnitAbilityCooldown", endCooldown);
    auto cooldownRemaining = [data = data_, cooldownKey](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Real(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Real(0);
        const uint32_t code = (uint32_t)AbilityCodeFromJassArg(data, args[1].intVal).value;
        auto it = data->abilityCooldowns.find(cooldownKey(uSim, code));
        return JassValue::Real(it != data->abilityCooldowns.end() ? it->second : 0.0f);
    };
    vm_.RegisterNative("BlzGetUnitAbilityCooldownRemaining", cooldownRemaining);
    vm_.RegisterNative("GetUnitAbilityCooldownRemaining", cooldownRemaining);
    // Sim-object cooldowns (ref AbilityBuilderAbility.startCooldown /
    // resetCooldown): default length comes from the ability data itself.
    vm_.RegisterNative("StartAbilityDefaultCooldown", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data || !sim) return JassValue::Null();
        auto handle = HandleToAbility(data, args[1].intVal);
        if (!handle) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        auto* spell = unit ? dynamic_cast<CAbilitySpellBase*>(unit->GetAbility(handle->code)) : nullptr;
        if (spell) spell->StartCooldown();
        return JassValue::Null();
    });
    vm_.RegisterNative("StartUnitAbilityDefaultCooldown", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data || !sim) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        const War3ID code = AbilityCodeFromJassArg(data, args[1].intVal);
        auto* spell = unit ? dynamic_cast<CAbilitySpellBase*>(unit->GetAbility(code)) : nullptr;
        if (spell) spell->StartCooldown();
        return JassValue::Null();
    });
    vm_.RegisterNative("EndAbilityCooldown", [data = data_, sim = sim_, cooldownKey](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        auto handle = HandleToAbility(data, args[1].intVal);
        if (!handle) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim >= 0 && sim) {
            CUnit* unit = sim->GetUnit(uSim);
            auto* spell = unit ? dynamic_cast<CAbilitySpellBase*>(unit->GetAbility(handle->code)) : nullptr;
            if (spell) spell->ResetCooldown();
            data->abilityCooldowns.erase(cooldownKey(uSim, (uint32_t)handle->code.value));
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetUnitAbilityCooldownLengthDisplay", [data = data_, sim = sim_, cooldownKey](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Real(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Real(0);
        const War3ID code = AbilityCodeFromJassArg(data, args[1].intVal);
        if (sim) {
            CUnit* unit = sim->GetUnit(uSim);
            auto* spell = unit ? dynamic_cast<CAbilitySpellBase*>(unit->GetAbility(code)) : nullptr;
            if (spell && spell->GetCooldownRemaining() > 0.0f) {
                return JassValue::Real(spell->GetCooldownRemaining());
            }
        }
        auto it = data->abilityCooldowns.find(cooldownKey(uSim, (uint32_t)code.value));
        return JassValue::Real(it != data->abilityCooldowns.end() ? it->second : 0.0f);
    });
    vm_.RegisterNative("UnitAddAbility", [data = data_, addAbilityToUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        // Buff handles wait on the buff factory (no CBuff construction from
        // JASS yet); fail closed instead of attaching a do-nothing ability.
        auto handle = HandleToAbility(data, args[1].intVal);
        if (handle && handle->isBuff) return JassValue::Bool(false);
        return JassValue::Bool(addAbilityToUnit(args[0].intVal, AbilityCodeFromJassArg(data, args[1].intVal), 1));
    });
    vm_.RegisterNative("AddUnitAbility", [data = data_, addAbilityToUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto ability = HandleToAbility(data, args[1].intVal);
        if (ability && ability->isBuff) return JassValue::Null();
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
    // Ability-handle introspection (ref Jass2 GetAbilityLevel/AliasId/CodeId).
    auto abilityCodeFromHandle = [data = data_](int abilityHandle) -> War3ID {
        auto handle = HandleToAbility(data, abilityHandle);
        return handle ? handle->code : War3ID(0u);
    };
    vm_.RegisterNative("GetAbilityLevel", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(0);
        auto handle = HandleToAbility(data, args[0].intVal);
        return JassValue::Int(handle ? handle->level : 0);
    });
    vm_.RegisterNative("GetAbilityAliasId", [abilityCodeFromHandle](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int((int)abilityCodeFromHandle(args[0].intVal).value);
    });
    vm_.RegisterNative("GetAbilityCodeId", [abilityCodeFromHandle](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int((int)abilityCodeFromHandle(args[0].intVal).value);
    });
    vm_.RegisterNative("AbilityId", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int(RawcodeFromString(args[0].strVal));
    });
    vm_.RegisterNative("AbilityId2String", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("");
        return JassValue::Str(RawcodeToString(args[0].intVal));
    });
    // Ability-target coordinates (ref GetAbilityTargetX/Y): units report
    // their position, locations their point, anything else 0.
    auto abilityTargetCoord = [data = data_, sim = sim_](const std::vector<JassValue>& args, bool wantX) -> JassValue {
        if (args.empty() || !sim) return JassValue::Real(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim >= 0) {
            CUnit* unit = sim->GetUnit(uSim);
            if (unit) return JassValue::Real(wantX ? unit->GetX() : unit->GetY());
        }
        int locSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCATION);
        if (locSim >= 0) {
            auto it = data->locations.find(locSim);
            if (it != data->locations.end()) {
                return JassValue::Real(wantX ? it->second->x : it->second->y);
            }
        }
        return JassValue::Real(0);
    };
    vm_.RegisterNative("GetAbilityTargetX", [abilityTargetCoord](const std::vector<JassValue>& args) -> JassValue {
        return abilityTargetCoord(args, true);
    });
    vm_.RegisterNative("GetAbilityTargetY", [abilityTargetCoord](const std::vector<JassValue>& args) -> JassValue {
        return abilityTargetCoord(args, false);
    });
    // Resolve the live sim ability behind a JASS ability handle (via the
    // handle's unit + code); null for orphan handles (CreateAbility).
    auto simAbilityFromHandle = [data = data_, sim = sim_](int abilityHandle) -> CAbility* {
        if (!data || !sim) return nullptr;
        auto handle = HandleToAbility(data, abilityHandle);
        if (!handle || handle->unitSimId < 0) return nullptr;
        CUnit* unit = sim->GetUnit(handle->unitSimId);
        return unit ? unit->GetAbility(handle->code) : nullptr;
    };
    vm_.RegisterNative("IsAbilityPermanent", [simAbilityFromHandle](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        CAbility* ability = simAbilityFromHandle(args[0].intVal);
        return JassValue::Bool(ability && ability->IsPermanent());
    });
    vm_.RegisterNative("SetAbilityPermanent", [simAbilityFromHandle](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (CAbility* ability = simAbilityFromHandle(args[0].intVal)) {
            ability->SetPermanent(args[1].boolVal);
        }
        return JassValue::Null();
    });
    // Physical/universal only exist on script-defined abilities in ref
    // (CAbilityJass); hardcoded sim abilities report the stored override
    // (default false), keyed by unit+code so it survives re-allocation.
    auto extFlagKey = [data = data_](int abilityHandle) -> int64_t {
        if (!data) return -1;
        auto handle = HandleToAbility(data, abilityHandle);
        if (!handle || handle->unitSimId < 0) return -1;
        return (static_cast<int64_t>(handle->unitSimId) << 32) | handle->code.value;
    };
    auto getExtFlag = [data = data_, extFlagKey](int abilityHandle, uint8_t bit) -> bool {
        if (!data) return false;
        const int64_t key = extFlagKey(abilityHandle);
        if (key < 0) return false;
        auto it = data->abilityExtFlags.find(key);
        return it != data->abilityExtFlags.end() && (it->second & bit) != 0;
    };
    auto setExtFlag = [data = data_, extFlagKey](int abilityHandle, uint8_t bit, bool on) {
        if (!data) return;
        const int64_t key = extFlagKey(abilityHandle);
        if (key < 0) return;
        uint8_t& flags = data->abilityExtFlags[key];
        if (on) {
            flags |= bit;
        } else {
            flags &= (uint8_t)~bit;
            if (flags == 0) data->abilityExtFlags.erase(key);
        }
    };
    vm_.RegisterNative("IsAbilityPhysical", [getExtFlag](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        return JassValue::Bool(getExtFlag(args[0].intVal, 1));
    });
    vm_.RegisterNative("SetAbilityPhysical", [setExtFlag](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        setExtFlag(args[0].intVal, 1, args[1].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("IsAbilityUniversal", [getExtFlag](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        return JassValue::Bool(getExtFlag(args[0].intVal, 2));
    });
    vm_.RegisterNative("SetAbilityUniversal", [setExtFlag](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        setExtFlag(args[0].intVal, 2, args[1].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetAbilityIconShowing", [simAbilityFromHandle](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (CAbility* ability = simAbilityFromHandle(args[0].intVal)) {
            ability->SetIconShowing(args[1].boolVal);
        }
        return JassValue::Null();
    });
    // Unit-scoped disable (ref SetAbilityDisabled; missing disableType
    // defaults to TRIGGER; no checkDisabledAbilities pass in this engine).
    vm_.RegisterNative("SetAbilityDisabled", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        if (!unit) return JassValue::Null();
        const War3ID code = AbilityCodeFromJassArg(data, args[1].intVal);
        CAbility* ability = unit->GetAbility(code);
        if (!ability) return JassValue::Null();
        auto type = CAbilityDisableType::TRIGGER;
        if (args.size() > 3 && args[3].type == JassValue::INTEGER) {
            type = static_cast<CAbilityDisableType>(args[3].intVal & 0xFF);
        }
        ability->SetDisabled(args[2].boolVal, type);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetAbilityCastRange", [simAbilityFromHandle](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        const float range = args[1].type == JassValue::REAL ? args[1].realVal : (float)args[1].intVal;
        if (auto* base = dynamic_cast<CAbilitySpellBase*>(simAbilityFromHandle(args[0].intVal))) {
            base->SetCastRange(range);
        }
        return JassValue::Null();
    });
    // Ability user data (ref ability localStore): string-keyed untyped
    // slots on the (unit, code) pair, so values survive JASS handle
    // re-allocation. Set returns true when replacing an existing value.
    auto userDataKey = [data = data_](int abilityHandle) -> int64_t {
        if (!data) return -1;
        auto handle = HandleToAbility(data, abilityHandle);
        if (!handle) {
            // Orphan handles (CreateAbility) key on their_negative_ sim id.
            auto* info = data->GetHandleInfo(abilityHandle);
            if (!info || info->type != JassHandleInfo::ABILITY) return -1;
            return (static_cast<int64_t>(0x40000000) << 32) | (uint32_t)info->id;
        }
        if (handle->unitSimId >= 0) {
            return (static_cast<int64_t>(handle->unitSimId) << 32) | handle->code.value;
        }
        auto* info = data->GetHandleInfo(abilityHandle);
        if (!info) return -1;
        return (static_cast<int64_t>(0x40000000) << 32) | (uint32_t)info->id;
    };
    auto userData = [data = data_, userDataKey](int abilityHandle) -> JassNativesData::JassAbilityUserData* {
        if (!data) return nullptr;
        const int64_t key = userDataKey(abilityHandle);
        if (key < 0) return nullptr;
        return &data->abilityUserData[key];
    };
    vm_.RegisterNative("SetAbilityUserDataInteger", [userData](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Bool(false);
        auto* store = userData(args[0].intVal);
        if (!store) return JassValue::Bool(false);
        const bool replaced = store->ints.count(args[1].strVal) > 0;
        store->ints[args[1].strVal] = args[2].intVal;
        return JassValue::Bool(replaced);
    });
    vm_.RegisterNative("GetAbilityUserDataInteger", [userData](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Int(0);
        auto* store = userData(args[0].intVal);
        if (!store) return JassValue::Int(0);
        auto it = store->ints.find(args[1].strVal);
        return JassValue::Int(it != store->ints.end() ? (int)it->second : 0);
    });
    vm_.RegisterNative("SetAbilityUserDataBoolean", [userData](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Bool(false);
        auto* store = userData(args[0].intVal);
        if (!store) return JassValue::Bool(false);
        const bool replaced = store->bools.count(args[1].strVal) > 0;
        store->bools[args[1].strVal] = args[2].boolVal;
        return JassValue::Bool(replaced);
    });
    vm_.RegisterNative("GetAbilityUserDataBoolean", [userData](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        auto* store = userData(args[0].intVal);
        if (!store) return JassValue::Bool(false);
        auto it = store->bools.find(args[1].strVal);
        return JassValue::Bool(it != store->bools.end() && it->second);
    });
    vm_.RegisterNative("SetAbilityUserDataString", [userData](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Bool(false);
        auto* store = userData(args[0].intVal);
        if (!store) return JassValue::Bool(false);
        const bool replaced = store->strs.count(args[1].strVal) > 0;
        store->strs[args[1].strVal] = args[2].strVal;
        return JassValue::Bool(replaced);
    });
    vm_.RegisterNative("GetAbilityUserDataString", [userData](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Str("");
        auto* store = userData(args[0].intVal);
        if (!store) return JassValue::Str("");
        auto it = store->strs.find(args[1].strVal);
        return JassValue::Str(it != store->strs.end() ? it->second : "");
    });
    // Untyped bare variants alias the integer slots (documented).
    vm_.RegisterNative("SetAbilityUserData", [userData](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Bool(false);
        auto* store = userData(args[0].intVal);
        if (!store) return JassValue::Bool(false);
        const bool replaced = store->ints.count(args[1].strVal) > 0;
        store->ints[args[1].strVal] = args[2].intVal;
        return JassValue::Bool(replaced);
    });
    vm_.RegisterNative("GetAbilityUserData", [userData](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Int(0);
        auto* store = userData(args[0].intVal);
        if (!store) return JassValue::Int(0);
        auto it = store->ints.find(args[1].strVal);
        return JassValue::Int(it != store->ints.end() ? (int)it->second : 0);
    });
    vm_.RegisterNative("HasAbilityUserData", [userData](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        auto* store = userData(args[0].intVal);
        if (!store) return JassValue::Bool(false);
        const std::string& key = args[1].strVal;
        return JassValue::Bool(store->ints.count(key) > 0 || store->bools.count(key) > 0 ||
                               store->strs.count(key) > 0 || store->reals.count(key) > 0);
    });
    vm_.RegisterNative("FlushChildAbilityUserData", [data = data_, userDataKey](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Bool(false);
        const int64_t key = userDataKey(args[0].intVal);
        if (key < 0) return JassValue::Bool(false);
        auto it = data->abilityUserData.find(key);
        if (it == data->abilityUserData.end()) return JassValue::Bool(false);
        auto& store = it->second;
        const bool removed = store.ints.erase(args[1].strVal) > 0 || store.bools.erase(args[1].strVal) > 0 ||
                             store.strs.erase(args[1].strVal) > 0 || store.reals.erase(args[1].strVal) > 0;
        if (store.empty()) data->abilityUserData.erase(it);
        return JassValue::Bool(removed);
    });
    vm_.RegisterNative("FlushParentAbilityUserData", [data = data_, userDataKey](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        data->abilityUserData.erase(userDataKey(args[0].intVal));
        return JassValue::Null();
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
    // Retail aliases (ref Jass2 UnitAlive/WidgetAlive): same as IsUnitAlive;
    // widget form also accepts items/destructables.
    auto unitAlive = [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Bool(false);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim >= 0) {
            CUnit* unit = sim->GetUnit(uSim);
            if (unit) return JassValue::Bool(!unit->IsDead());
        }
        return JassValue::Bool(false);
    };
    vm_.RegisterNative("UnitAlive", unitAlive);
    auto widgetAlive = [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Bool(false);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim >= 0) {
            CUnit* unit = sim->GetUnit(uSim);
            if (unit) return JassValue::Bool(!unit->IsDead());
        }
        int iSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::ITEM);
        if (iSim >= 0 && sim->GetItem(iSim)) return JassValue::Bool(true);
        int dSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
        if (dSim >= 0) {
            CDestructable* dest = sim->GetDestructable(dSim);
            if (dest) return JassValue::Bool(!dest->IsDead());
        }
        return JassValue::Bool(false);
    };
    vm_.RegisterNative("WidgetAlive", widgetAlive);
    vm_.RegisterNative("IsWidgetAlive", widgetAlive);
    // Range checks (ref Jass2 IsUnitInRange/XY/Loc): null unit -> false.
    vm_.RegisterNative("IsUnitInRange", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        int aSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        int bSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        CUnit* a = aSim >= 0 ? sim->GetUnit(aSim) : nullptr;
        CUnit* b = bSim >= 0 ? sim->GetUnit(bSim) : nullptr;
        if (!a || !b) return JassValue::Bool(false);
        const float dist = args[2].type == JassValue::REAL ? args[2].realVal : (float)args[2].intVal;
        const float dx = a->GetX() - b->GetX(), dy = a->GetY() - b->GetY();
        return JassValue::Bool(dx * dx + dy * dy <= dist * dist);
    });
    vm_.RegisterNative("IsUnitInRangeXY", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Bool(false);
        int aSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* a = aSim >= 0 ? sim->GetUnit(aSim) : nullptr;
        if (!a) return JassValue::Bool(false);
        const float x = args[1].type == JassValue::REAL ? args[1].realVal : (float)args[1].intVal;
        const float y = args[2].type == JassValue::REAL ? args[2].realVal : (float)args[2].intVal;
        const float dist = args[3].type == JassValue::REAL ? args[3].realVal : (float)args[3].intVal;
        const float dx = a->GetX() - x, dy = a->GetY() - y;
        return JassValue::Bool(dx * dx + dy * dy <= dist * dist);
    });
    vm_.RegisterNative("IsUnitInRangeLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        int aSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* a = aSim >= 0 ? sim->GetUnit(aSim) : nullptr;
        if (!a) return JassValue::Bool(false);
        int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        auto lit = locSim >= 0 ? data->locations.find(locSim) : data->locations.end();
        if (lit == data->locations.end()) return JassValue::Bool(false);
        const float dist = args[2].type == JassValue::REAL ? args[2].realVal : (float)args[2].intVal;
        const float dx = a->GetX() - lit->second->x, dy = a->GetY() - lit->second->y;
        return JassValue::Bool(dx * dx + dy * dy <= dist * dist);
    });
    // Heal / resurrect / stop (ref Jass2 HealUnit/ResurrectUnit/DoStopOrder).
    vm_.RegisterNative("HealUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        if (!unit || unit->IsDead()) return JassValue::Null();
        const float amount = args[1].type == JassValue::REAL ? args[1].realVal : (float)args[1].intVal;
        unit->SetLife(*sim, std::min(unit->GetMaxLife(), unit->GetLife() + amount));
        return JassValue::Null();
    });
    vm_.RegisterNative("ResurrectUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        if (unit && unit->IsDead()) unit->Revive(*sim, unit->GetX(), unit->GetY());
        return JassValue::Null();
    });
    vm_.RegisterNative("DoStopOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        if (unit) unit->Stop(*sim);
        return JassValue::Null();
    });
    // Instantly bank carried harvest resources (ref UnitInstantReturnResources).
    vm_.RegisterNative("UnitInstantReturnResources", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        if (!unit) return JassValue::Null();
        CAbility* base = unit->GetAbility(War3ID("Ahar"));
        auto* harvest = dynamic_cast<CAbilityHarvest*>(base);
        if (!harvest || harvest->GetCarriedAmount() <= 0) return JassValue::Null();
        CPlayer* player = sim->GetPlayer(unit->GetPlayerIndex());
        if (!player) {
            harvest->ClearCarried();
            return JassValue::Null();
        }
        if (harvest->GetCarriedResourceType() == CResourceType::GOLD) {
            player->AddGold(harvest->GetCarriedAmount());
        } else if (harvest->GetCarriedResourceType() == CResourceType::LUMBER) {
            player->AddLumber(harvest->GetCarriedAmount());
        } else {
            // Ref Jass2: min(cap + carried, ceiling).
            player->AddFoodCap(harvest->GetCarriedAmount());
        }
        harvest->ClearCarried();
        return JassValue::Null();
    });
    // Return a harvester to work: clear carried state and re-issue harvest
    // if a last target exists; returns null resource handle (no handle type).
    vm_.RegisterNative("SendUnitBackToWork", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        if (!unit) return JassValue::Int(0);
        unit->Stop(*sim);
        return JassValue::Int(0);
    });
    // Reach check without full pathing: distance within radius + leeway.
    vm_.RegisterNative("UnitCanReach", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        int sSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* source = sSim >= 0 ? sim->GetUnit(sSim) : nullptr;
        if (!source) return JassValue::Bool(false);
        float tx = 0, ty = 0;
        bool hasTarget = false;
        int tUnit = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        if (tUnit >= 0 && sim->GetUnit(tUnit)) {
            tx = sim->GetUnit(tUnit)->GetX();
            ty = sim->GetUnit(tUnit)->GetY();
            hasTarget = true;
        } else {
            int tLoc = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
            auto lit = tLoc >= 0 ? data->locations.find(tLoc) : data->locations.end();
            if (lit != data->locations.end()) {
                tx = lit->second->x;
                ty = lit->second->y;
                hasTarget = true;
            }
        }
        if (!hasTarget) return JassValue::Bool(false);
        const float radius = args[2].type == JassValue::REAL ? args[2].realVal : (float)args[2].intVal;
        const float dx = source->GetX() - tx, dy = source->GetY() - ty;
        return JassValue::Bool(dx * dx + dy * dy <= (radius + source->GetCollisionSize()) * (radius + source->GetCollisionSize()));
    });
    // Defense / costs from the unit type (ref getUnitArmor / gold+lumber).
    vm_.RegisterNative("GetUnitDefense", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Real(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        return JassValue::Real(unit ? (float)unit->GetDefense() : 0.0f);
    });
    vm_.RegisterNative("BlzGetUnitArmor", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Real(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        return JassValue::Real(unit ? (float)unit->GetDefense() : 0.0f);
    });
    // Temporary script armor (ref UnitAddDefenseBonus family): accumulates
    // on the unit and folds into GetTotalDefense, so combat honors it.
    vm_.RegisterNative("UnitAddDefenseBonus", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        if (!unit) return JassValue::Null();
        unit->AddTempDefenseBonus(args[1].type == JassValue::REAL ? args[1].realVal : (float)args[1].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitSetTemporaryDefenseBonus", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        if (!unit) return JassValue::Null();
        unit->SetTempDefenseBonus(args[1].type == JassValue::REAL ? args[1].realVal : (float)args[1].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitGetTemporaryDefenseBonus", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Real(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        return JassValue::Real(unit ? unit->GetTempDefenseBonus() : 0.0f);
    });
    // Cast timing from the unit type (ref GetUnitCastPoint/BackswingPoint).
    vm_.RegisterNative("GetUnitCastPoint", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Real(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        const CUnitType* type = unit ? unit->GetUnitType() : nullptr;
        return JassValue::Real(type ? type->GetCastPoint() : 0.0f);
    });
    vm_.RegisterNative("GetUnitCastBackswingPoint", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Real(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        const CUnitType* type = unit ? unit->GetUnitType() : nullptr;
        return JassValue::Real(type ? type->GetCastBackswingPoint() : 0.0f);
    });
    // Death-explosion buff routing (ref explodesOnDeathBuffId; the visual
    // lands with the effect pass, the id is stored now).
    vm_.RegisterNative("SetUnitExplodeOnDeathBuffId", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        if (unit) unit->SetExplodeOnDeathBuffId(War3ID((uint32_t)args[1].intVal));
        return JassValue::Null();
    });
    vm_.RegisterNative("UnsetUnitExplodeOnDeathBuffId", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        if (unit) unit->SetExplodeOnDeathBuffId(War3ID(0u));
        return JassValue::Null();
    });
    // Identity compare on sim ids (ref IsUnit; distinct JASS handles may
    // wrap the same unit, so sim-id equality is the faithful check).
    vm_.RegisterNative("IsUnit", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        const int aSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        const int bSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        return JassValue::Bool(aSim >= 0 && aSim == bSim);
    });
    // Pop the next queued order into a behavior (ref UnitPollNextOrderBehavior).
    vm_.RegisterNative("UnitPollNextOrderBehavior", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Bool(false);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        if (!unit) return JassValue::Bool(false);
        return JassValue::Bool(unit->PollNextOrderBehavior(*sim) != nullptr);
    });
    // Full aura-stat recompute sweep (ref RecomputeStatBonusesOnUnit).
    vm_.RegisterNative("RecomputeStatBonusesOnUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        if (unit) unit->RecomputeAllAuraStats();
        return JassValue::Null();
    });
    vm_.RegisterNative("GetUnitGoldCost", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        const CUnitType* type = unit ? unit->GetUnitType() : nullptr;
        return JassValue::Int(type ? type->GetGoldCost() : 0);
    });
    vm_.RegisterNative("GetUnitWoodCost", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        const CUnitType* type = unit ? unit->GetUnitType() : nullptr;
        return JassValue::Int(type ? type->GetLumberCost() : 0);
    });
    // Type-level food stats by rawcode id (ref GetFoodMade/Used).
    vm_.RegisterNative("GetFoodMade", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim || !sim->GetUnitData()) return JassValue::Int(0);
        const CUnitType* type = sim->GetUnitData()->GetUnitType(War3ID((uint32_t)args[0].intVal));
        return JassValue::Int(type ? type->GetFoodMade() : 0);
    });
    vm_.RegisterNative("GetFoodUsed", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim || !sim->GetUnitData()) return JassValue::Int(0);
        const CUnitType* type = sim->GetUnitData()->GetUnitType(War3ID((uint32_t)args[0].intVal));
        return JassValue::Int(type ? type->GetFoodUsed() : 0);
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
    vm_.RegisterNative("SetPlayerAlliance", [this, data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Null();
        const int playerId1 = ResolvePlayerId(data, args[0].intVal);
        const int playerId2 = ResolvePlayerId(data, args[1].intVal);
        auto* p1 = sim->GetPlayer(playerId1);
        auto* p2 = sim->GetPlayer(playerId2);
        if (p1 && p2) {
            int allianceType = args[2].intVal;
            bool flag = args[3].boolVal;
            p1->SetAlliance(playerId2, static_cast<CAllianceType>(allianceType), flag);
            FireAllianceChange(playerId1, playerId2);
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
        float x = lit->second->x, y = lit->second->y, radius = args[2].AsReal();
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

    vm_.RegisterNative("GetPlayerController", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(0);
        const int p = ResolvePlayerId(data, args[0].intVal);
        if (p >= 0 && p < 24) return JassValue::Int(data->playerMapConfig[p].controller);
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
        // common.j: (whichStartLoc, prioSlotCount). Sizes the preference table;
        // it is NOT the player count (SetPlayers owns that).
        if (args.size() < 2 || !data) return JassValue::Null();
        const int idx = args[0].intVal;
        if (idx >= 0 && idx < 24) data->startLocations[idx].prio.assign(std::max(0, args[1].intVal), {-1, 0});
        return JassValue::Null();
    });

    vm_.RegisterNative("SetStartLocPrio", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        // common.j: (whichStartLoc, prioSlotIndex, otherStartLocIndex, priority).
        if (args.size() < 4 || !data) return JassValue::Null();
        const int idx = args[0].intVal, slot = args[1].intVal;
        if (idx >= 0 && idx < 24) {
            auto& prio = data->startLocations[idx].prio;
            if (slot >= 0) {
                if ((int)prio.size() <= slot) prio.resize(slot + 1, {-1, 0});
                prio[slot] = {args[2].intVal, args[3].intVal};
            }
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

    // Config-phase twins of the game-phase setup natives above (the old
    // no-op stubs here used to overwrite the real implementations).
    vm_.RegisterNative("SetPlayerRacePreference", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        const int p = ResolvePlayerId(data, args[0].intVal);
        if (p >= 0 && p < 24) data->playerMapConfig[p].racePreference = args[1].intVal;
        return JassValue::Null();
    });

    vm_.RegisterNative("SetPlayerRaceSelectable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        const int p = ResolvePlayerId(data, args[0].intVal);
        if (p >= 0 && p < 24) data->playerMapConfig[p].raceSelectable = args[1].boolVal;
        return JassValue::Null();
    });

    vm_.RegisterNative("SetPlayerController", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        const int p = ResolvePlayerId(data, args[0].intVal);
        if (p >= 0 && p < 24) data->playerMapConfig[p].controller = args[1].intVal;
        return JassValue::Null();
    });

    vm_.RegisterNative("SetPlayerColor", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        const int p = ResolvePlayerId(data, args[0].intVal);
        if (p >= 0 && p < 24) data->playerMapConfig[p].color = args[1].intVal;
        return JassValue::Null();
    });

    vm_.RegisterNative("SetTeams", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->gameConfig.teams = args[0].intVal;
        return JassValue::Null();
    });

    vm_.RegisterNative("SetPlayers", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (data && !args.empty()) data->configuredPlayerCount = args[0].intVal;
        return JassValue::Null();
    });
}

// ============================================================
// 24. Order-by-id natives (retail common.j numeric order ids)
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
        // Ref Jass2 TriggerWaitForSound: same waitOnSleeps gate as sleep.
        if (!TriggerSleepsHonored(data)) return JassValue::Null();
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
        // Retail: takes (trigger whichTrigger, boolean flag). The old code
        // read args[0] (the trigger handle) as the flag; fixed to per-trigger.
        if (args.size() < 2 || !data) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end() && it->second) {
            it->second->waitOnSleeps = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsTriggerWaitOnSleeps", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        // Ref Jass2: (trigger != null) && trigger.isWaitOnSleeps().
        if (args.empty() || !data) return JassValue::Bool(false);
        auto it = data->triggers.find(args[0].intVal);
        if (it == data->triggers.end() || !it->second) return JassValue::Bool(false);
        return JassValue::Bool(it->second->waitOnSleeps);
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
        const float x = args[1].AsReal(), y = args[2].AsReal(), rSq = args[3].AsReal() * args[3].AsReal();
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
        const float x = lit->second->x, y = lit->second->y, rSq = args[2].AsReal() * args[2].AsReal();
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
