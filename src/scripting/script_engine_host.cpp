#include "scripting/script_engine_host.h"
#include "ecs/lockstep.h"
#include <cstring>
#include <sstream>

namespace scripting {

namespace {

void WriteU32(std::vector<uint8_t>& buf, uint32_t v) {
    uint8_t bytes[4];
    SimWireWriteLE32(bytes, v);
    buf.insert(buf.end(), bytes, bytes + 4);
}

bool ReadU32(const std::vector<uint8_t>& buf, size_t& offset, uint32_t& out) {
    if (offset + 4 > buf.size()) return false;
    out = SimWireReadLE32(buf.data() + offset);
    offset += 4;
    return true;
}

void WriteString(std::vector<uint8_t>& buf, const std::string& s) {
    WriteU32(buf, static_cast<uint32_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

bool ReadString(const std::vector<uint8_t>& buf, size_t& offset, std::string& out) {
    uint32_t len = 0;
    if (!ReadU32(buf, offset, len)) return false;
    if (offset + len > buf.size()) return false;
    out.assign(reinterpret_cast<const char*>(buf.data() + offset), len);
    offset += len;
    return true;
}

constexpr uint32_t kHotReloadMagicV1 = 0x53485352u; // 'RSHS'
constexpr uint32_t kHotReloadMagicV2 = 0x32485352u; // 'RSH2' (globals + active event subscriptions)

} // namespace

// ============================================================================
// 0. IScriptEngineHost Shared Base Implementation
// ============================================================================

int IScriptEngineHost::SubscribeEvent(int eventId, const std::string& handlerFuncName, int filterHandle) {
    const int subId = nextSubId_++;
    eventBuckets_[eventId].push_back({subId, eventId, filterHandle, handlerFuncName, true});
    return subId;
}

bool IScriptEngineHost::UnsubscribeEvent(int subscriptionId) {
    for (auto& [evId, vec] : eventBuckets_) {
        (void)evId;
        for (auto& sub : vec) {
            if (sub.id == subscriptionId && sub.active) {
                sub.active = false;
                return true;
            }
        }
    }
    return false;
}

void IScriptEngineHost::OnHandleDestroyed(int handleId) {
    if (handleId < 0) return;
    for (auto& [evId, vec] : eventBuckets_) {
        (void)evId;
        for (auto& sub : vec) {
            if (sub.filterHandle == handleId) {
                sub.active = false;
            }
        }
    }
}

size_t IScriptEngineHost::GetActiveSubscriptionCount() const {
    size_t activeCount = 0;
    for (const auto& [evId, vec] : eventBuckets_) {
        (void)evId;
        for (const auto& sub : vec) {
            if (sub.active) ++activeCount;
        }
    }
    return activeCount;
}

std::vector<uint8_t> IScriptEngineHost::ExportHotReloadState() const {
    const auto globals = SnapshotGlobals();
    std::vector<uint8_t> buf;
    WriteU32(buf, kHotReloadMagicV2);
    WriteU32(buf, static_cast<uint32_t>(globals.size()));
    for (const auto& [name, val] : globals) {
        WriteString(buf, name);
        buf.push_back(static_cast<uint8_t>(val.type));
        WriteU32(buf, static_cast<uint32_t>(val.intVal));
        uint32_t fBits = 0;
        std::memcpy(&fBits, &val.realVal, sizeof(float));
        WriteU32(buf, fBits);
        buf.push_back(val.boolVal ? 1u : 0u);
        WriteString(buf, val.strVal);
    }

    // Collect active event subscriptions for cross-backend migration
    std::vector<const ScriptEventSubscription*> activeSubs;
    for (const auto& [evId, vec] : eventBuckets_) {
        (void)evId;
        for (const auto& sub : vec) {
            if (sub.active) {
                activeSubs.push_back(&sub);
            }
        }
    }
    WriteU32(buf, static_cast<uint32_t>(activeSubs.size()));
    for (const ScriptEventSubscription* sub : activeSubs) {
        WriteU32(buf, static_cast<uint32_t>(sub->id));
        WriteU32(buf, static_cast<uint32_t>(sub->eventId));
        WriteU32(buf, static_cast<uint32_t>(sub->filterHandle));
        WriteString(buf, sub->handlerFunc);
    }
    WriteU32(buf, static_cast<uint32_t>(nextSubId_));
    return buf;
}

bool IScriptEngineHost::ImportHotReloadState(const std::vector<uint8_t>& stateBytes) {
    size_t offset = 0;
    uint32_t magic = 0;
    uint32_t count = 0;
    if (!ReadU32(stateBytes, offset, magic)) return false;
    if (magic != kHotReloadMagicV1 && magic != kHotReloadMagicV2) return false;
    if (!ReadU32(stateBytes, offset, count)) return false;

    for (uint32_t i = 0; i < count; ++i) {
        std::string name;
        if (!ReadString(stateBytes, offset, name)) return false;
        if (offset >= stateBytes.size()) return false;
        const auto valType = static_cast<JassValue::Type>(stateBytes[offset++]);
        uint32_t intBits = 0;
        uint32_t floatBits = 0;
        if (!ReadU32(stateBytes, offset, intBits)) return false;
        if (!ReadU32(stateBytes, offset, floatBits)) return false;
        if (offset >= stateBytes.size()) return false;
        const bool bVal = (stateBytes[offset++] != 0);
        std::string sVal;
        if (!ReadString(stateBytes, offset, sVal)) return false;

        ScriptValue v;
        v.type = valType;
        v.intVal = static_cast<int>(intBits);
        std::memcpy(&v.realVal, &floatBits, sizeof(float));
        v.boolVal = bVal;
        v.strVal = std::move(sVal);
        SetGlobal(name, v);
    }

    if (magic == kHotReloadMagicV2 && offset < stateBytes.size()) {
        uint32_t subCount = 0;
        if (!ReadU32(stateBytes, offset, subCount)) return false;
        eventBuckets_.clear();
        int maxLoadedSubId = 0;
        for (uint32_t i = 0; i < subCount; ++i) {
            uint32_t id = 0, evId = 0, filterH = 0;
            std::string handler;
            if (!ReadU32(stateBytes, offset, id) ||
                !ReadU32(stateBytes, offset, evId) ||
                !ReadU32(stateBytes, offset, filterH) ||
                !ReadString(stateBytes, offset, handler)) {
                eventBuckets_.clear();
                return false;
            }
            const int signedId = static_cast<int>(id);
            const int signedEvId = static_cast<int>(evId);
            if (signedId > maxLoadedSubId) maxLoadedSubId = signedId;
            eventBuckets_[signedEvId].push_back({
                signedId,
                signedEvId,
                static_cast<int>(filterH),
                std::move(handler),
                true
            });
        }
        uint32_t savedNextSubId = 1;
        if (ReadU32(stateBytes, offset, savedNextSubId)) {
            nextSubId_ = std::max(nextSubId_, static_cast<int>(savedNextSubId));
        }
        nextSubId_ = std::max(nextSubId_, std::max(1, maxLoadedSubId + 1));
    }
    return true;
}

// ============================================================================
// 1. JassScriptHostAdapter Implementation
// ============================================================================

JassScriptHostAdapter::JassScriptHostAdapter()
    : ownedVm_(std::make_unique<JassVM>())
    , vm_(ownedVm_.get()) {}

JassScriptHostAdapter::JassScriptHostAdapter(JassVM* externalVm)
    : vm_(externalVm) {
    if (!vm_) {
        ownedVm_ = std::make_unique<JassVM>();
        vm_ = ownedVm_.get();
    }
}

bool JassScriptHostAdapter::LoadModule(const std::string& moduleName, const std::string& sourceCode) {
    (void)moduleName;
    if (!vm_) return false;
    return vm_->LoadScript(sourceCode);
}

void JassScriptHostAdapter::RegisterNative(const std::string& name, ScriptNativeCallback callback) {
    if (!vm_) return;
    vm_->RegisterNative(name, std::move(callback));
}

ScriptValue JassScriptHostAdapter::CallFunction(const std::string& funcName,
                                                const std::vector<ScriptValue>& args) {
    if (!vm_) return ScriptValue::Null();
    if (!args.empty()) {
        const auto& nats = vm_->GetRegisteredNatives();
        auto it = nats.find(funcName);
        if (it != nats.end() && it->second) {
            return it->second(args);
        }
    }
    return vm_->CallFunction(funcName);
}

void JassScriptHostAdapter::SetGlobal(const std::string& name, const ScriptValue& value) {
    trackedGlobals_[name] = value;
    if (vm_) {
        vm_->SetGlobal(name, value);
    }
}

ScriptValue JassScriptHostAdapter::GetGlobal(const std::string& name) const {
    if (vm_) {
        ScriptValue val = vm_->GetGlobal(name);
        if (val.type != JassValue::NULL_VAL) {
            trackedGlobals_[name] = val;
            return val;
        }
    }
    auto it = trackedGlobals_.find(name);
    return (it != trackedGlobals_.end()) ? it->second : ScriptValue::Null();
}

std::unordered_map<std::string, ScriptValue> JassScriptHostAdapter::SnapshotGlobals() const {
    if (vm_) {
        for (const auto& [k, v] : vm_->GetGlobals()) {
            if (v.type != JassValue::NULL_VAL) {
                trackedGlobals_[k] = v;
            }
        }
    }
    for (auto& [k, v] : trackedGlobals_) {
        if (vm_) {
            ScriptValue live = vm_->GetGlobal(k);
            if (live.type != JassValue::NULL_VAL) {
                v = live;
            }
        }
    }
    return trackedGlobals_;
}

int JassScriptHostAdapter::FireEvent(const ScriptEventPayload& payload) {
    lastEventPayload_ = payload;
    auto it = eventBuckets_.find(payload.eventId);
    if (it == eventBuckets_.end()) return 0;
    int fired = 0;
    for (const auto& sub : it->second) {
        if (!sub.active) continue;
        if (sub.filterHandle >= 0 && sub.filterHandle != payload.sourceHandle) continue;
        CallFunction(sub.handlerFunc);
        ++fired;
    }
    return fired;
}

// ============================================================================
// 2. RealScriptEngineHost Implementation
// ============================================================================

RealScriptEngineHost::RealScriptEngineHost() {
    callArgsBuffer_.reserve(16);
    eventArgsBuffer_.reserve(8);
}

bool RealScriptEngineHost::LoadModule(const std::string& moduleName, const std::string& sourceCode) {
    if (moduleName.empty()) return false;
    modules_[moduleName] = sourceCode;

    // Lightweight declarative global initializer parser for RealScript modules:
    // Lines matching "let <ident> = <int|float|string>;" initialize globals_ directly.
    std::istringstream iss(sourceCode);
    std::string token;
    while (iss >> token) {
        if (token == "let" || token == "var") {
            std::string varName, eqSign, rawVal;
            if (iss >> varName >> eqSign >> rawVal && eqSign == "=") {
                if (!rawVal.empty() && rawVal.back() == ';') {
                    rawVal.pop_back();
                }
                if (!rawVal.empty() && rawVal.front() == '"' && rawVal.back() == '"' && rawVal.size() >= 2) {
                    globals_[varName] = ScriptValue::Str(rawVal.substr(1, rawVal.size() - 2));
                } else if (rawVal.find('.') != std::string::npos) {
                    globals_[varName] = ScriptValue::Real(std::strtof(rawVal.c_str(), nullptr));
                } else if (rawVal == "true" || rawVal == "false") {
                    globals_[varName] = ScriptValue::Bool(rawVal == "true");
                } else {
                    globals_[varName] = ScriptValue::Int(static_cast<int>(std::strtol(rawVal.c_str(), nullptr, 10)));
                }
            }
        }
    }
    return true;
}

void RealScriptEngineHost::RegisterNative(const std::string& name, ScriptNativeCallback callback) {
    natives_[name] = std::move(callback);
}

void RealScriptEngineHost::RegisterScriptFunction(const std::string& name, RealScriptFunction fn) {
    functions_[name] = std::move(fn);
}

ScriptValue RealScriptEngineHost::CallFunction(const std::string& funcName,
                                               const std::vector<ScriptValue>& args) {
    auto fnIt = functions_.find(funcName);
    if (fnIt != functions_.end() && fnIt->second) {
        return fnIt->second(*this, args);
    }
    auto natIt = natives_.find(funcName);
    if (natIt != natives_.end() && natIt->second) {
        const size_t prevCap = callArgsBuffer_.capacity();
        if (&args != &callArgsBuffer_) {
            callArgsBuffer_.assign(args.begin(), args.end());
        }
        if (callArgsBuffer_.capacity() == prevCap) {
            ++zeroAllocNativeCalls_;
        }
        return natIt->second(callArgsBuffer_);
    }
    return ScriptValue::Null();
}

ScriptValue RealScriptEngineHost::InvokeNativeZeroAlloc(const std::string& nativeName,
                                                        std::initializer_list<ScriptValue> args) {
    auto natIt = natives_.find(nativeName);
    if (natIt == natives_.end() || !natIt->second) {
        return ScriptValue::Null();
    }
    const size_t prevCap = callArgsBuffer_.capacity();
    callArgsBuffer_.assign(args.begin(), args.end());
    if (callArgsBuffer_.capacity() == prevCap) {
        ++zeroAllocNativeCalls_;
    }
    return natIt->second(callArgsBuffer_);
}

void RealScriptEngineHost::SetGlobal(const std::string& name, const ScriptValue& value) {
    globals_[name] = value;
}

ScriptValue RealScriptEngineHost::GetGlobal(const std::string& name) const {
    auto it = globals_.find(name);
    return (it != globals_.end()) ? it->second : ScriptValue::Null();
}

std::unordered_map<std::string, ScriptValue> RealScriptEngineHost::SnapshotGlobals() const {
    return globals_;
}

int RealScriptEngineHost::FireEvent(const ScriptEventPayload& payload) {
    lastEventPayload_ = payload;
    auto it = eventBuckets_.find(payload.eventId);
    if (it == eventBuckets_.end()) return 0;

    int fired = 0;
    const size_t prevCap = eventArgsBuffer_.capacity();
    eventArgsBuffer_.resize(3);
    eventArgsBuffer_[0] = ScriptValue::Int(payload.sourceHandle);
    eventArgsBuffer_[1] = ScriptValue::Int(payload.targetHandle);
    eventArgsBuffer_[2] = ScriptValue::Real(payload.numericValue);
    if (eventArgsBuffer_.capacity() == prevCap) {
        ++zeroAllocNativeCalls_;
    }

    for (const auto& sub : it->second) {
        if (!sub.active) continue;
        if (sub.filterHandle >= 0 && sub.filterHandle != payload.sourceHandle) continue;
        eventArgsBuffer_[0] = ScriptValue::Int(payload.sourceHandle);
        eventArgsBuffer_[1] = ScriptValue::Int(payload.targetHandle);
        eventArgsBuffer_[2] = ScriptValue::Real(payload.numericValue);
        CallFunction(sub.handlerFunc, eventArgsBuffer_);
        ++fired;
    }
    return fired;
}

// ============================================================================
// 3. ScriptHostRouter Implementation
// ============================================================================

ScriptHostRouter::ScriptHostRouter()
    : activeHost_(std::make_unique<JassScriptHostAdapter>()) {}

ScriptHostRouter::ScriptHostRouter(std::unique_ptr<IScriptEngineHost> initialHost)
    : activeHost_(std::move(initialHost)) {
    if (!activeHost_) {
        activeHost_ = std::make_unique<JassScriptHostAdapter>();
    }
}

void ScriptHostRouter::RegisterSharedNative(const std::string& name, ScriptNativeCallback callback) {
    sharedNatives_[name] = callback;
    if (activeHost_) {
        activeHost_->RegisterNative(name, std::move(callback));
    }
}

bool ScriptHostRouter::HotSwapBackend(std::unique_ptr<IScriptEngineHost> newHost, bool migrateGlobals) {
    if (!newHost) return false;

    std::vector<uint8_t> stateSnapshot;
    if (migrateGlobals && activeHost_) {
        stateSnapshot = activeHost_->ExportHotReloadState();
    }

    for (const auto& [name, cb] : sharedNatives_) {
        newHost->RegisterNative(name, cb);
    }

    if (migrateGlobals && !stateSnapshot.empty()) {
        if (!newHost->ImportHotReloadState(stateSnapshot)) {
            return false;
        }
    }

    activeHost_ = std::move(newHost);
    ++hotSwapCount_;
    return true;
}

} // namespace scripting
