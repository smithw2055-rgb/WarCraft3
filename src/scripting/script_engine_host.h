#pragma once
// ============================================================================
// Stage DD1-DD2: Unified Script Engine Host Abstraction (Godot 4
// ScriptLanguageExtension / Unity Scripting Backend style).
// Decouples engine simulation & presentation from JassVM so the project can
// seamlessly hot-swap to the self-developed RealScript scripting system.
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

    // O(1) indexed event subscription & dispatch
    virtual int SubscribeEvent(int eventId, const std::string& handlerFuncName, int filterHandle = -1) = 0;
    virtual bool UnsubscribeEvent(int subscriptionId) = 0;
    virtual int FireEvent(const ScriptEventPayload& payload) = 0;
    virtual void OnHandleDestroyed(int handleId) = 0;

    // Binary state snapshot for live hot-reload / cross-backend migration
    virtual std::vector<uint8_t> ExportHotReloadState() const = 0;
    virtual bool ImportHotReloadState(const std::vector<uint8_t>& stateBytes) = 0;
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

    int SubscribeEvent(int eventId, const std::string& handlerFuncName, int filterHandle = -1) override;
    bool UnsubscribeEvent(int subscriptionId) override;
    int FireEvent(const ScriptEventPayload& payload) override;
    void OnHandleDestroyed(int handleId) override;

    std::vector<uint8_t> ExportHotReloadState() const override;
    bool ImportHotReloadState(const std::vector<uint8_t>& stateBytes) override;

    JassVM& GetVm() { return *vm_; }
    const JassVM& GetVm() const { return *vm_; }

private:
    struct EventSub {
        int id = 0;
        int eventId = 0;
        int filterHandle = -1;
        std::string handlerFunc;
        bool active = true;
    };

    std::unique_ptr<JassVM> ownedVm_;
    JassVM* vm_ = nullptr;
    mutable std::unordered_map<std::string, ScriptValue> trackedGlobals_;
    std::unordered_map<int, std::vector<EventSub>> eventBuckets_;
    int nextSubId_ = 1;
    ScriptEventPayload lastEventPayload_{};
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

    int SubscribeEvent(int eventId, const std::string& handlerFuncName, int filterHandle = -1) override;
    bool UnsubscribeEvent(int subscriptionId) override;
    int FireEvent(const ScriptEventPayload& payload) override;
    void OnHandleDestroyed(int handleId) override;

    std::vector<uint8_t> ExportHotReloadState() const override;
    bool ImportHotReloadState(const std::vector<uint8_t>& stateBytes) override;

    const ScriptEventPayload& GetLastEventPayload() const { return lastEventPayload_; }
    uint64_t GetZeroAllocNativeCallCount() const { return zeroAllocNativeCalls_; }
    size_t GetLoadedModuleCount() const { return modules_.size(); }
    size_t GetActiveSubscriptionCount() const;

private:
    struct EventSub {
        int id = 0;
        int eventId = 0;
        int filterHandle = -1;
        std::string handlerFunc;
        bool active = true;
    };

    std::unordered_map<std::string, std::string> modules_;
    std::unordered_map<std::string, ScriptNativeCallback> natives_;
    std::unordered_map<std::string, RealScriptFunction> functions_;
    std::unordered_map<std::string, ScriptValue> globals_;
    std::unordered_map<int, std::vector<EventSub>> eventBuckets_;
    std::vector<ScriptValue> callArgsBuffer_;
    std::vector<ScriptValue> eventArgsBuffer_;
    uint64_t zeroAllocNativeCalls_ = 0;
    int nextSubId_ = 1;
    ScriptEventPayload lastEventPayload_{};
};

// ============================================================================
// 3. ScriptHostRouter: Manages active IScriptEngineHost & hot-swaps backends
//    (e.g. JassScriptHostAdapter -> RealScriptEngineHost) with automatic
//    native re-binding and global state migration.
// ============================================================================
class ScriptHostRouter {
public:
    ScriptHostRouter();
    explicit ScriptHostRouter(std::unique_ptr<IScriptEngineHost> initialHost);

    IScriptEngineHost* GetActiveHost() { return activeHost_.get(); }
    const IScriptEngineHost* GetActiveHost() const { return activeHost_.get(); }

    void RegisterSharedNative(const std::string& name, ScriptNativeCallback callback);

    template <typename Func>
    void RegisterSharedNativeAuto(const std::string& name, Func&& fn) {
        RegisterSharedNative(name, jass::BindNative(std::forward<Func>(fn)));
    }

    // Hot-swaps the active scripting backend, automatically re-registering all shared
    // natives and optionally migrating global variables across backends!
    bool HotSwapBackend(std::unique_ptr<IScriptEngineHost> newHost, bool migrateGlobals = true);

    uint32_t GetHotSwapCount() const { return hotSwapCount_; }

private:
    std::unique_ptr<IScriptEngineHost> activeHost_;
    std::unordered_map<std::string, ScriptNativeCallback> sharedNatives_;
    uint32_t hotSwapCount_ = 0;
};

} // namespace scripting
