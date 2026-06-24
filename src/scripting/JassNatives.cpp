#include "JassNatives.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "audio/AudioSystem.h"
#include "ecs/player.h"
#include "ecs/item.h"
#include "ecs/destructable.h"
#include "ecs/orders/order.h"
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
    context.filterUnitHandle = data_->filterUnitHandle;
    context.enumUnitHandle = data_->enumUnitHandle;
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
    context.eventKillingUnitHandle = data_->eventKillingUnitHandle;
    context.eventDyingUnitHandle = data_->eventDyingUnitHandle;
    context.eventRevivingUnitHandle = data_->eventRevivingUnitHandle;
    context.eventTransportUnitHandle = data_->eventTransportUnitHandle;
    context.eventLoadedUnitHandle = data_->eventLoadedUnitHandle;
    return context;
}

void JassNatives::ApplyContext(const JassQueuedContext& context) {
    if (!data_) return;
    data_->currentTriggerId = context.currentTriggerId;
    data_->currentTriggerUnitHandle = context.currentTriggerUnitHandle;
    data_->filterUnitHandle = context.filterUnitHandle;
    data_->enumUnitHandle = context.enumUnitHandle;
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
    data_->eventKillingUnitHandle = context.eventKillingUnitHandle;
    data_->eventDyingUnitHandle = context.eventDyingUnitHandle;
    data_->eventRevivingUnitHandle = context.eventRevivingUnitHandle;
    data_->eventTransportUnitHandle = context.eventTransportUnitHandle;
    data_->eventLoadedUnitHandle = context.eventLoadedUnitHandle;
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

void JassNatives::FireUnitEvent(int unitSimHandle, int eventType) {
    for (auto& [id, trigger] : data_->triggers) {
        for (size_t i = 0; i < trigger->registeredUnitHandles.size(); i++) {
            int regUnitHandle = trigger->registeredUnitHandles[i];
            int regEventType = trigger->eventTypes[i];
            if (regEventType != eventType) continue;

            int simHandle = data_->GetSimHandle(regUnitHandle, JassHandleInfo::UNIT);
            if (simHandle == unitSimHandle || regUnitHandle == 0) {
                JassQueuedContext context = CaptureContext();
                context.currentTriggerId = id;
                context.currentTriggerUnitHandle = regUnitHandle;
                for (const auto& funcName : trigger->actionFuncNames) {
                    QueueFunctionWithContext(funcName, context);
                }
                break;
            }
        }
    }
}

void JassNatives::FireDamageEvent(int unitSimHandle, int sourceSimHandle, float damage, int eventType) {
    data_->eventDamage = damage;
    data_->eventDamageSourceHandle = sourceSimHandle;
    data_->eventAttackingUnitHandle = sourceSimHandle;
    FireUnitEvent(unitSimHandle, eventType);
    data_->eventDamage = 0;
    data_->eventDamageSourceHandle = -1;
    data_->eventAttackingUnitHandle = -1;
}

void JassNatives::FireDeathEvent(int unitSimHandle, int killerSimHandle, int eventType) {
    // Set event context: dying unit and killing unit
    // We need to create JASS handles for these sim handles
    data_->eventDyingUnitHandle = data_->AllocHandle(JassHandleInfo::UNIT, unitSimHandle);
    data_->eventKillingUnitHandle = killerSimHandle >= 0 ?
        data_->AllocHandle(JassHandleInfo::UNIT, killerSimHandle) : -1;
    data_->eventTriggerWidgetHandle = data_->eventDyingUnitHandle;

    FireUnitEvent(unitSimHandle, eventType);

    data_->eventDyingUnitHandle = -1;
    data_->eventKillingUnitHandle = -1;
    data_->eventTriggerWidgetHandle = -1;
}

void JassNatives::FireOrderEvent(int unitSimHandle, int orderId, int targetSimHandle, float x, float y) {
    data_->eventOrderedUnitHandle = data_->AllocHandle(JassHandleInfo::UNIT, unitSimHandle);
    data_->eventIssuedOrderId = orderId;

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
}

int JassNatives::UpdateTimers(float dt) {
    int firedCount = 0;
    for (auto& [id, timer] : data_->timers) {
        (void)id;
        if (timer->paused || timer->remaining <= 0) continue;
        timer->remaining -= dt;
        if (timer->remaining <= 0) {
            timer->remaining = 0;
            if (!timer->callbackFunc.empty()) {
                QueueFunction(timer->callbackFunc);
                firedCount++;
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

void JassNatives::RegisterAll() {
    RegisterConfigNatives();
    RegisterCommonNatives();
}

void JassNatives::RegisterConfigNatives() {
    RegisterHandleNatives();
    RegisterMathNatives();
    RegisterConversionNatives();
    RegisterRandomNatives();
    RegisterConfigPlayerNatives();
    std::cout << "JassNatives: registered config natives" << std::endl;
}

void JassNatives::RegisterCommonNatives() {
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
        int id = args[0].intVal;
        char buf[5] = {
            (char)((id >> 24) & 0xFF), (char)((id >> 16) & 0xFF),
            (char)((id >> 8) & 0xFF), (char)(id & 0xFF), 0
        };
        return JassValue::Str(buf);
    });
    vm_.RegisterNative("UnitId2String", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Str("");
        int id = args[0].intVal;
        char buf[5] = {
            (char)((id >> 24) & 0xFF), (char)((id >> 16) & 0xFF),
            (char)((id >> 8) & 0xFF), (char)(id & 0xFF), 0
        };
        return JassValue::Str(buf);
    });
    vm_.RegisterNative("String2UnitId", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        const std::string& s = args[0].strVal;
        if (s.size() < 4) return JassValue::Int(0);
        int id = ((unsigned char)s[0] << 24) | ((unsigned char)s[1] << 16) |
                 ((unsigned char)s[2] << 8) | ((unsigned char)s[3]);
        return JassValue::Int(id);
    });
    vm_.RegisterNative("String2OrderId", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        const std::string& s = args[0].strVal;
        if (s.size() < 4) return JassValue::Int(0);
        int id = ((unsigned char)s[0] << 24) | ((unsigned char)s[1] << 16) |
                 ((unsigned char)s[2] << 8) | ((unsigned char)s[3]);
        return JassValue::Int(id);
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
        "ConvertBuff", "ConvertEffectType", "ConvertUnitType", "ConvertHeroStat"
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
    vm_.RegisterNative("GroupEnumUnitsOfType", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Null();
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Null();
        int typeId = args[1].intVal;
        War3ID w3id((uint32_t)typeId);
        git->second->unitHandles.clear();
        if (sim) {
            for (auto* unit : sim->GetUnits())
                if (unit->GetTypeId() == w3id)
                    git->second->unitHandles.push_back(data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId()));
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupEnumUnitsInRange", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Null();
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Null();
        float x = args[1].realVal, y = args[2].realVal, radius = args[3].realVal;
        float rSq = radius * radius;
        git->second->unitHandles.clear();
        if (sim) {
            for (auto* unit : sim->GetUnits()) {
                float dx = unit->GetX() - x, dy = unit->GetY() - y;
                if (dx * dx + dy * dy <= rSq)
                    git->second->unitHandles.push_back(data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId()));
            }
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupEnumUnitsOfPlayer", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Null();
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Null();
        int playerSimId = data->GetSimHandle(args[1].intVal, JassHandleInfo::PLAYER);
        git->second->unitHandles.clear();
        if (sim) {
            for (auto* unit : sim->GetUnits())
                if (unit->GetPlayerIndex() == playerSimId)
                    git->second->unitHandles.push_back(data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId()));
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupEnumUnitsInRect", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
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
        git->second->unitHandles.clear();
        if (sim) {
            for (auto* unit : sim->GetUnits()) {
                float ux = unit->GetX(), uy = unit->GetY();
                if (ux >= minX && ux <= maxX && uy >= minY && uy <= maxY)
                    git->second->unitHandles.push_back(data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId()));
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
        for (int uh : git->second->unitHandles) {
            data->enumUnitHandle = uh;
            if (vmPtr) vmPtr->Execute(funcName);
        }
        data->enumUnitHandle = -1;
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
    vm_.RegisterNative("ForceEnumPlayers", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("ForceEnumAllies", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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
    vm_.RegisterNative("CreateUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (!sim || args.size() < 5) return JassValue::Null();
        int playerId = args[0].intVal;
        int unitTypeId = args[1].intVal;
        float x = args[2].realVal, y = args[3].realVal, facing = args[4].realVal;
        War3ID w3id((uint32_t)unitTypeId);
        CUnit* unit = sim->CreateUnit(w3id, playerId, x, y, facing);
        if (unit) return JassValue::Int(data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId()));
        return JassValue::Null();
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
    vm_.RegisterNative("SetUnitPosition", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetPosition(args[1].realVal, args[2].realVal); }
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
    vm_.RegisterNative("AddUnitAnimationProperties", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim) return JassValue::Null();
        if (data->unitAddAnimationProperties) data->unitAddAnimationProperties(simId, args[1].strVal, args[2].boolVal);
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
        return JassValue::Int(ab ? 1 : 0);
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
    vm_.RegisterNative("GetPlayerState", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        int playerId = args[0].intVal;
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
    vm_.RegisterNative("SetPlayerState", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        auto* p = sim->GetPlayer(args[0].intVal);
        if (!p) return JassValue::Null();
        switch (args[1].intVal) {
            case 1: p->SetGold(args[2].intVal); break;
            case 2: p->SetLumber(args[2].intVal); break;
            case 4: p->SetFoodCap(args[2].intVal); break;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsPlayerAlly", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        auto* p1 = sim->GetPlayer(args[0].intVal), *p2 = sim->GetPlayer(args[1].intVal);
        if (p1 && p2) return JassValue::Bool(p1->HasAlliance(*p2, CAllianceType::ALLIED));
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("IsPlayerEnemy", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        return JassValue::Bool(args[0].intVal != args[1].intVal);
    });
    vm_.RegisterNative("IsPlayerInForce", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int pSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::FORCE);
        if (pSim < 0) return JassValue::Bool(false);
        auto fit = data->forces.find(pSim);
        if (fit == data->forces.end()) return JassValue::Bool(false);
        return JassValue::Bool(std::find(fit->second->playerIds.begin(), fit->second->playerIds.end(), args[0].intVal) != fit->second->playerIds.end());
    });
    vm_.RegisterNative("IsPlayerObserver", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("IsPlayerRacePrefSet", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("GetPlayerRace", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("GetPlayerColor", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("GetPlayerStartLocation", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("GetPlayerTechMaxAllowed", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Int(1);
        auto* p = sim->GetPlayer(args[0].intVal);
        if (p) return JassValue::Int(p->GetTechLevel(War3ID((uint32_t)args[1].intVal)));
        return JassValue::Int(0);
    });
    vm_.RegisterNative("SetPlayerTechMaxAllowed", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetPlayerTechResearched", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        auto* p = sim->GetPlayer(args[0].intVal);
        if (p) p->SetTechLevel(War3ID((uint32_t)args[1].intVal), args[2].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("GetPlayerTechCount", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("SetPlayerAlliance", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Null();
        auto* src = sim->GetPlayer(args[0].intVal);
        if (src) src->SetAlliance(args[1].intVal, static_cast<CAllianceType>(args[2].intVal & 0xF), args[3].boolVal);
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
        auto item = std::make_unique<CItem>(data->nextHandleId, w3id, x, y);
        int handleId = item->GetHandleId();
        if (sim) {
            int jassHandle = data->AllocHandle(JassHandleInfo::ITEM, handleId);
            return JassValue::Int(jassHandle);
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("CreateItemLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        int locId = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (locId < 0) return JassValue::Int(0);
        auto locIt = data->locations.find(locId);
        if (locIt == data->locations.end()) return JassValue::Int(0);
        War3ID w3id((uint32_t)args[0].intVal);
        auto item = std::make_unique<CItem>(data->nextHandleId, w3id, locIt->second->x, locIt->second->y);
        return JassValue::Int(data->AllocHandle(JassHandleInfo::ITEM, item->GetHandleId()));
    });
    vm_.RegisterNative("RemoveItem", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::ITEM);
        if (id >= 0) data->FreeHandle(args[0].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("GetItemTypeId", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        (void)sim; (void)data;
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetItemLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(100);
        (void)sim; (void)data;
        return JassValue::Real(100);
    });
    vm_.RegisterNative("SetItemLife", [data = data_, sim = sim_](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("GetItemX", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        (void)sim; (void)data;
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetItemY", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        (void)sim; (void)data;
        return JassValue::Real(0);
    });
    vm_.RegisterNative("SetItemPosition", [data = data_, sim = sim_](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("GetItemPlayer", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("SetItemPlayer", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("GetItemCharges", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("SetItemCharges", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("GetItemName", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Str(""); });
    vm_.RegisterNative("UnitAddItem", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("UnitAddItemById", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Bool(false);
        War3ID w3id((uint32_t)args[1].intVal);
        CUnit* unit = sim->GetUnit(uSim);
        if (!unit) return JassValue::Bool(false);
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("UnitRemoveItem", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("UnitRemoveItemFromSlot", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("UnitHasItem", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("UnitItemInSlot", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("UnitInventorySize", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(6); });
    vm_.RegisterNative("UnitDropItemPoint", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("UnitDropItemSlot", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("UnitDropItemTarget", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("UnitUseItem", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("UnitUseItemPoint", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("UnitUseItemTarget", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
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
    vm_.RegisterNative("DisableTrigger", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("EnableTrigger", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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
    vm_.RegisterNative("TriggerAddCondition", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("TriggerClearActions", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) it->second->actionFuncNames.clear();
        return JassValue::Null();
    });
    vm_.RegisterNative("GetTriggeringTrigger", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->currentTriggerId >= 0 ? JassValue::Int(data->currentTriggerId) : JassValue::Null();
    });
    vm_.RegisterNative("GetTriggerUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->currentTriggerUnitHandle >= 0 ? JassValue::Int(data->currentTriggerUnitHandle) : JassValue::Null();
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
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int(args[0].intVal);
    });
    vm_.RegisterNative("Filter", [](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        return JassValue::Int(args[0].intVal);
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
    vm_.RegisterNative("GetFilterUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->filterUnitHandle >= 0 ? JassValue::Int(data->filterUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetEnumUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->enumUnitHandle >= 0 ? JassValue::Int(data->enumUnitHandle) : JassValue::Int(0);
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
    vm_.RegisterNative("GetSpellTargetLocation", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto loc = std::make_shared<JassLocation>();
        loc->x = data->eventSpellTargetX;
        loc->y = data->eventSpellTargetY;
        int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    });
    vm_.RegisterNative("GetTriggerPlayer", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventTriggerPlayerHandle >= 0 ? JassValue::Int(data->eventTriggerPlayerHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetTriggerWidget", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventTriggerWidgetHandle >= 0 ? JassValue::Int(data->eventTriggerWidgetHandle) : JassValue::Int(0);
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
    vm_.RegisterNative("GetKillingUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventKillingUnitHandle >= 0 ? JassValue::Int(data->eventKillingUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetDyingUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventDyingUnitHandle >= 0 ? JassValue::Int(data->eventDyingUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetRevivingUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventRevivingUnitHandle >= 0 ? JassValue::Int(data->eventRevivingUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetTransportUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventTransportUnitHandle >= 0 ? JassValue::Int(data->eventTransportUnitHandle) : JassValue::Int(0);
    });
    vm_.RegisterNative("GetLoadedUnit", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return data->eventLoadedUnitHandle >= 0 ? JassValue::Int(data->eventLoadedUnitHandle) : JassValue::Int(0);
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
    vm_.RegisterNative("SetGameDifficulty", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetGameType", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("SetMapFlags", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
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

    // ===== Extended Unit Natives =====
    vm_.RegisterNative("UnitAddAbility", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Bool(false);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Bool(false);
        // TODO: create ability from data and add to unit
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("UnitRemoveAbility", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Bool(false);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Bool(false);
        // TODO: find and remove ability
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("GetUnitAbilityLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Int(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Int(0);
        War3ID abilityCode(static_cast<uint32_t>(args[1].intVal));
        auto* ability = unit->GetAbility(abilityCode);
        return JassValue::Int(ability ? 1 : 0);
    });
    vm_.RegisterNative("SetUnitAbilityLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Int(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Int(0);
        // TODO: set ability level
        return JassValue::Int(args[2].intVal);
    });
    vm_.RegisterNative("UnitHasAbility", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Bool(false);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Bool(false);
        War3ID abilityCode(static_cast<uint32_t>(args[1].intVal));
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
    vm_.RegisterNative("GetPlayerGold", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        auto* p = sim->GetPlayer(args[0].intVal);
        return JassValue::Int(p ? p->GetGold() : 0);
    });
    vm_.RegisterNative("SetPlayerGold", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        auto* p = sim->GetPlayer(args[0].intVal);
        if (p) p->SetGold(args[1].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("GetPlayerLumber", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        auto* p = sim->GetPlayer(args[0].intVal);
        return JassValue::Int(p ? p->GetLumber() : 0);
    });
    vm_.RegisterNative("SetPlayerLumber", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        auto* p = sim->GetPlayer(args[0].intVal);
        if (p) p->SetLumber(args[1].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPlayerAlliance", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Null();
        auto* p1 = sim->GetPlayer(args[0].intVal);
        auto* p2 = sim->GetPlayer(args[1].intVal);
        if (p1 && p2) {
            int allianceType = args[2].intVal;
            bool flag = args[3].boolVal;
            p1->SetAlliance(args[1].intVal, static_cast<CAllianceType>(allianceType), flag);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetPlayerAlliance", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        auto* p1 = sim->GetPlayer(args[0].intVal);
        auto* p2 = sim->GetPlayer(args[1].intVal);
        if (p1 && p2) {
            return JassValue::Bool(p1->HasAlliance(*p2, static_cast<CAllianceType>(args[2].intVal)));
        }
        return JassValue::Bool(false);
    });

    // ===== Extended Group Natives =====
    vm_.RegisterNative("GroupEnumUnitsInRangeOfLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
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
        git->second->unitHandles.clear();
        if (sim) {
            for (auto* unit : sim->GetUnits()) {
                float dx = unit->GetX() - x, dy = unit->GetY() - y;
                if (dx * dx + dy * dy <= rSq)
                    git->second->unitHandles.push_back(data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId()));
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

    vm_.RegisterNative("SetPlayerName", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int p = args[0].intVal;
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
        int playerIdx = args[0].intVal;
        int locIdx = args[1].intVal;
        if (playerIdx >= 0 && playerIdx < 24) {
            data->playerMapConfig[playerIdx].startLocationIndex = locIdx;
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
