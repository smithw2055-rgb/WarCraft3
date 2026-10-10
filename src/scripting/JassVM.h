#pragma once
#include "JassParser.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <variant>
#include <iostream>
#include <unordered_set>

// JASS value type
struct JassValue {
    enum Type { INTEGER, REAL, BOOLEAN, STRING, NULL_VAL } type = NULL_VAL;
    int intVal = 0;
    float realVal = 0.0f;
    bool boolVal = false;
    std::string strVal;

    JassValue() : type(NULL_VAL) {}
    static JassValue Int(int v) {
        JassValue j;
        j.type = INTEGER;
        j.intVal = v;
        j.realVal = static_cast<float>(v);
        return j;
    }
    static JassValue Real(float v) {
        JassValue j;
        j.type = REAL;
        j.realVal = v;
        j.intVal = static_cast<int>(v);
        return j;
    }
    static JassValue Bool(bool v) { JassValue j; j.type = BOOLEAN; j.boolVal = v; return j; }
    static JassValue Str(const std::string& v) { JassValue j; j.type = STRING; j.strVal = v; return j; }
    static JassValue Null() { return JassValue(); }

    float AsReal() const {
        if (type == REAL) return realVal;
        if (type == INTEGER) return static_cast<float>(intVal);
        return realVal;
    }
    int AsInt() const {
        if (type == INTEGER) return intVal;
        if (type == REAL) return static_cast<int>(realVal);
        return intVal;
    }
    bool AsBool() const {
        if (type == BOOLEAN) return boolVal;
        if (type == INTEGER) return intVal != 0;
        return boolVal;
    }
};

class JassVM {
public:
    JassVM();

    struct Scope {
        std::unordered_map<std::string, JassValue> locals;
    };

    struct ThreadFrame {
        const AstNode* function = nullptr;
        const AstNode* block = nullptr;
        size_t statementIndex = 0;
        Scope scope;
        bool callScope = true;
        bool loop = false;
        int loopIterationsRemaining = 1000000;
    };

    struct ThreadState {
        std::string functionName;
        bool started = false;
        bool completed = false;
        std::vector<ThreadFrame> frames;
    };

    struct ThreadRunResult {
        bool completed = false;
        bool sleeping = false;
        float sleepSeconds = 0.0f;
    };

    // Load and execute
    bool LoadScript(const std::string& source);
    bool LoadScriptFromMemory(const std::string& name, const uint8_t* data, size_t size);
    void Execute(const std::string& funcName = "main");
    // Runs scope/library initializers collected at load, in source order.
    // Called once at map init before abilities_main/main.
    void ExecuteInitializers();
    JassValue CallFunction(const std::string& funcName);
    ThreadRunResult RunThread(ThreadState& state, const std::string& functionName);
    bool HasFunction(const std::string& funcName) const;

    // Native function registration
    using NativeFunc = std::function<JassValue(const std::vector<JassValue>&)>;
    void RegisterNative(const std::string& name, NativeFunc func);

    // Variable access (for game integration)
    void SetGlobal(const std::string& name, const JassValue& val);
    JassValue GetGlobal(const std::string& name) const;

    bool IsValid() const { return !programs_.empty(); }
    void RequestSleep(float seconds);
    bool HasPendingSleep() const { return sleepRequested_; }
    float TakePendingSleepSeconds() {
        sleepRequested_ = false;
        float s = sleepSeconds_;
        sleepSeconds_ = 0.0f;
        return s;
    }
    const std::vector<std::string>& GetErrors() const { return errors_; }
    const std::unordered_map<std::string, int>& GetNativeDeclarations() const { return nativeDeclarations_; }
    const std::unordered_map<std::string, int>& GetFunctionCallCounts() const { return functionCallCounts_; }
    const std::unordered_map<std::string, NativeFunc>& GetRegisteredNatives() const { return natives_; }
    std::unordered_set<std::string> GetFunctionNames() const;

private:
    void CollectScriptStats(const AstNode* node);
    bool PushThreadFrame(ThreadState& state, const std::string& functionName, const std::vector<JassValue>& args = {});
    bool PushBlockFrame(ThreadState& state, const AstNode* block, bool loop = false);
    void PopCurrentFunctionFrames(ThreadState& state);
    void SyncCallStackFromThread(const ThreadState& state);
    void SyncThreadFromCallStack(ThreadState& state);
    JassValue ExecuteNode(const AstNode* node);
    JassValue ExecuteFunction(const AstNode* func, const std::vector<JassValue>& args);
    // vJASS struct support: instances live outside the value type as
    // integer ids (0 = null); members/statics in side tables.
    struct JassStructMember {
        std::string name;
        std::string type;
        bool isArray = false;
        bool isStatic = false;
        const AstNode* defaultExpr = nullptr;
    };
    struct JassStructMethod {
        std::string name;
        bool isStatic = false;
        const AstNode* func = nullptr; // mangled "Type.method" in functions_
    };
    struct JassStructDef {
        std::string name;
        std::string parent;
        bool isInterface = false;
        // type name, member that holds the delegated instance
        std::vector<std::pair<std::string, std::string>> delegates;
        std::vector<JassStructMember> members;
        std::vector<JassStructMethod> methods;
    };
    struct JassStructInstance {
        std::string type;
        std::unordered_map<std::string, JassValue> scalars;
        std::unordered_map<std::string, std::unordered_map<int, JassValue>> arrays;
    };
    const JassStructDef* FindStructDef(const std::string& name) const;
    bool IsStructBaseOf(const std::string& base, const std::string& derived) const;
    // Enclosing-instance id for thistype@S (0 = static context).
    int ThistypeInstance(const std::string& structName) const;
    int FindDelegateInstance(const std::string& type, int instanceId, const std::string& name) const;
    const JassStructMember* FindStructMember(const JassStructDef* def, const std::string& member) const;
    const JassStructMethod* FindStructMethod(const std::string& type, const std::string& method) const;
    int AllocateStruct(const std::string& type);
    JassValue DefaultStructValue(const std::string& type) const;

    std::vector<std::unique_ptr<AstNode>> programs_;
    std::unordered_map<std::string, JassValue> globals_;
    std::unordered_map<std::string, NativeFunc> natives_;
    std::unordered_map<std::string, const AstNode*> functions_;
    std::unordered_map<std::string, int> nativeDeclarations_;
    std::unordered_map<std::string, int> functionCallCounts_;
    std::vector<Scope> callStack_;
    std::vector<std::string> errors_;
    const AstNode* returnNode_ = nullptr;
    bool sleepRequested_ = false;
    float sleepSeconds_ = 0.0f;
    // vJASS struct runtime.
    std::unordered_map<std::string, JassStructDef> structDefs_;
    std::unordered_map<int, JassStructInstance> structInstances_;
    std::unordered_map<std::string, JassValue> structStatics_;
    int nextStructId_ = 1;
    std::vector<std::string> initializers_;
    // vJASS `hook ORIG REPL` (ref function hooking): calls to ORIG run
    // REPL, whose own references to ORIG reach the saved original.
    bool HookFunction(const std::string& orig, const std::string& replacement);
    int hookCounter_ = 0;
    // Library initializer dependencies (ref `library ... needs ...`).
    std::unordered_map<std::string, std::string> libInitByLib_;
    std::unordered_map<std::string, std::vector<std::string>> libNeedsByInit_;
    // Enclosing method-call stack for thistype@ resolution: (struct, id),
    // id 0 means static context. Lets `thistype.member` inside instance
    // methods reach the current instance (ref JassHelper behavior).
    std::vector<std::pair<std::string, int>> thistypeStack_;
};

// Forward declare for native implementations
class CSimulation;
class CUnit;
class CPlayer;
