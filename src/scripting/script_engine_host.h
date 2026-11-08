#pragma once
// ============================================================================
// Stage DD1-DD2 & Phase 27: Unified Script Engine Host Abstraction (Godot 4
// ScriptLanguageExtension / Unity Scripting Backend style).
// Decouples engine simulation & presentation from JassVM so the project can
// seamlessly hot-swap to the self-developed RealScript scripting system, with
// shared base event-subscription table & full subscription migration on hot-swap.
// ============================================================================
#include "scripting/JassVM.h"
#include "scripting/jass_binder.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace scripting {

enum class ScriptBackendKind : uint8_t {
    JassLegacy = 0,
    RealScript = 1,
    Custom = 2
};

using ScriptValue = JassValue;
using ScriptNativeCallback = std::function<ScriptValue(const std::vector<ScriptValue>&)>;

struct ScriptEventPayload {
    int eventId = 0;
    int sourceHandle = -1;
    int targetHandle = -1;
    int playerIndex = -1;
    float numericValue = 0.0f;
    std::string tag;
};

struct ScriptEventSubscription {
    int id = 0;
    int eventId = 0;
    int filterHandle = -1;
    std::string handlerFunc;
    bool active = true;
};

class IScriptEngineHost {
public:
    virtual ~IScriptEngineHost() = default;

    virtual ScriptBackendKind GetBackendKind() const = 0;
    virtual const char* GetBackendName() const = 0;

    virtual bool LoadModule(const std::string& moduleName, const std::string& sourceCode) = 0;
    virtual void RegisterNative(const std::string& name, ScriptNativeCallback callback) = 0;
    virtual ScriptValue CallFunction(const std::string& funcName,
                                     const std::vector<ScriptValue>& args = {}) = 0;

    virtual void SetGlobal(const std::string& name, const ScriptValue& value) = 0;
    virtual ScriptValue GetGlobal(const std::string& name) const = 0;
    virtual std::unordered_map<std::string, ScriptValue> SnapshotGlobals() const = 0;

    // Phase 27: Shared O(1) indexed event subscription & dispatch in base class
    virtual int SubscribeEvent(int eventId, const std::string& handlerFuncName, int filterHandle = -1);
    virtual bool UnsubscribeEvent(int subscriptionId);
    virtual int FireEvent(const ScriptEventPayload& payload) = 0;
    virtual void OnHandleDestroyed(int handleId);

    size_t GetActiveSubscriptionCount() const;
    const ScriptEventPayload& GetLastEventPayload() const { return lastEventPayload_; }

    // Binary state snapshot for live hot-reload / cross-backend migration
    // (includes both globals and active event subscriptions).
    virtual std::vector<uint8_t> ExportHotReloadState() const;
    virtual bool ImportHotReloadState(const std::vector<uint8_t>& stateBytes);

protected:
    std::unordered_map<int, std::vector<ScriptEventSubscription>> eventBuckets_;
    int nextSubId_ = 1;
    ScriptEventPayload lastEventPayload_{};
};

// ============================================================================
// 1. JassScriptHostAdapter: Wraps existing JassVM behind IScriptEngineHost
// ============================================================================
class JassScriptHostAdapter final : public IScriptEngineHost {
public:
    JassScriptHostAdapter();
    explicit JassScriptHostAdapter(JassVM* externalVm);
    ~JassScriptHostAdapter() override = default;

    ScriptBackendKind GetBackendKind() const override { return ScriptBackendKind::JassLegacy; }
    const char* GetBackendName() const override { return "JassVM"; }

    bool LoadModule(const std::string& moduleName, const std::string& sourceCode) override;
    void RegisterNative(const std::string& name, ScriptNativeCallback callback) override;
    ScriptValue CallFunction(const std::string& funcName,
                             const std::vector<ScriptValue>& args = {}) override;

    void SetGlobal(const std::string& name, const ScriptValue& value) override;
    ScriptValue GetGlobal(const std::string& name) const override;
    std::unordered_map<std::string, ScriptValue> SnapshotGlobals() const override;

    int FireEvent(const ScriptEventPayload& payload) override;

    JassVM& GetVm() { return *vm_; }
    const JassVM& GetVm() const { return *vm_; }

private:
    std::unique_ptr<JassVM> ownedVm_;
    JassVM* vm_ = nullptr;
    mutable std::unordered_map<std::string, ScriptValue> trackedGlobals_;
};

// ============================================================================
// 2. RealScriptEngineHost: Zero-allocation-ready host runtime for RealScript
// ============================================================================
class RealScriptEngineHost final : public IScriptEngineHost {
public:
    using RealScriptFunction = std::function<ScriptValue(RealScriptEngineHost&, const std::vector<ScriptValue>&)>;

    RealScriptEngineHost();
    ~RealScriptEngineHost() override = default;

    ScriptBackendKind GetBackendKind() const override { return ScriptBackendKind::RealScript; }
    const char* GetBackendName() const override { return "RealScript"; }

    bool LoadModule(const std::string& moduleName, const std::string& sourceCode) override;
    void RegisterNative(const std::string& name, ScriptNativeCallback callback) override;
    void RegisterScriptFunction(const std::string& name, RealScriptFunction fn);
    ScriptValue CallFunction(const std::string& funcName,
                             const std::vector<ScriptValue>& args = {}) override;
    ScriptValue InvokeNativeZeroAlloc(const std::string& nativeName,
                                      std::initializer_list<ScriptValue> args);

    void SetGlobal(const std::string& name, const ScriptValue& value) override;
    ScriptValue GetGlobal(const std::string& name) const override;
    std::unordered_map<std::string, ScriptValue> SnapshotGlobals() const override;

    int FireEvent(const ScriptEventPayload& payload) override;

    uint64_t GetZeroAllocNativeCallCount() const { return zeroAllocNativeCalls_; }
    size_t GetLoadedModuleCount() const { return modules_.size(); }

private:
    std::unordered_map<std::string, std::string> modules_;
    std::unordered_map<std::string, ScriptNativeCallback> natives_;
    std::unordered_map<std::string, RealScriptFunction> functions_;
    std::unordered_map<std::string, ScriptValue> globals_;
    std::vector<ScriptValue> callArgsBuffer_;
    std::vector<ScriptValue> eventArgsBuffer_;
    uint64_t zeroAllocNativeCalls_ = 0;
};

// ============================================================================
// 3. ScriptHostRouter: Manages active IScriptEngineHost & hot-swaps backends
//    (e.g. JassScriptHostAdapter -> RealScriptEngineHost) with automatic
//    native re-binding, global state migration, and event-subscription migration.
// ============================================================================
class ScriptHostRouter {
public:
    ScriptHostRouter();
    explicit ScriptHostRouter(std::unique_ptr<IScriptEngineHost> initialHost);

    IScriptEngineHost& ActiveHost() { return *activeHost_; }
    const IScriptEngineHost& ActiveHost() const { return *activeHost_; }
    IScriptEngineHost* GetActiveHost() { return activeHost_.get(); }
    const IScriptEngineHost* GetActiveHost() const { return activeHost_.get(); }

    ScriptBackendKind GetActiveBackendKind() const { return activeHost_->GetBackendKind(); }

    // Register a native across both the current host and all future hot-swapped hosts
    void RegisterSharedNative(const std::string& name, ScriptNativeCallback callback);

    template <typename Func>
    void RegisterSharedNativeAuto(const std::string& name, Func&& fn) {
        RegisterSharedNative(name, jass::BindNative(std::forward<Func>(fn)));
    }

    // Hot-swap the active script backend, optionally migrating all globals and event subscriptions
    bool HotSwapBackend(std::unique_ptr<IScriptEngineHost> newHost, bool migrateGlobals = true);

    uint32_t GetHotSwapCount() const { return hotSwapCount_; }
    size_t GetSharedNativeCount() const { return sharedNatives_.size(); }

private:
    std::unique_ptr<IScriptEngineHost> activeHost_;
    std::unordered_map<std::string, ScriptNativeCallback> sharedNatives_;
    uint32_t hotSwapCount_ = 0;
};

} // namespace scripting
