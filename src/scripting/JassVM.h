#pragma once
#include "JassParser.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <stack>
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
    static JassValue Int(int v) { JassValue j; j.type = INTEGER; j.intVal = v; return j; }
    static JassValue Real(float v) { JassValue j; j.type = REAL; j.realVal = v; return j; }
    static JassValue Bool(bool v) { JassValue j; j.type = BOOLEAN; j.boolVal = v; return j; }
    static JassValue Str(const std::string& v) { JassValue j; j.type = STRING; j.strVal = v; return j; }
    static JassValue Null() { return JassValue(); }
};

class JassVM {
public:
    JassVM();

    // Load and execute
    bool LoadScript(const std::string& source);
    bool LoadScriptFromMemory(const std::string& name, const uint8_t* data, size_t size);
    void Execute(const std::string& funcName = "main");
    JassValue CallFunction(const std::string& funcName);
    bool HasFunction(const std::string& funcName) const;

    // Native function registration
    using NativeFunc = std::function<JassValue(const std::vector<JassValue>&)>;
    void RegisterNative(const std::string& name, NativeFunc func);

    // Variable access (for game integration)
    void SetGlobal(const std::string& name, const JassValue& val);
    JassValue GetGlobal(const std::string& name) const;

    bool IsValid() const { return !programs_.empty(); }
    const std::vector<std::string>& GetErrors() const { return errors_; }
    const std::unordered_map<std::string, int>& GetNativeDeclarations() const { return nativeDeclarations_; }
    const std::unordered_map<std::string, int>& GetFunctionCallCounts() const { return functionCallCounts_; }
    const std::unordered_map<std::string, NativeFunc>& GetRegisteredNatives() const { return natives_; }
    std::unordered_set<std::string> GetFunctionNames() const;

private:
    void CollectScriptStats(const AstNode* node);
    JassValue ExecuteNode(const AstNode* node);
    JassValue ExecuteFunction(const AstNode* func, const std::vector<JassValue>& args);

    // Variable scopes
    struct Scope {
        std::unordered_map<std::string, JassValue> locals;
    };

    std::vector<std::unique_ptr<AstNode>> programs_;
    std::unordered_map<std::string, JassValue> globals_;
    std::unordered_map<std::string, NativeFunc> natives_;
    std::unordered_map<std::string, const AstNode*> functions_;
    std::unordered_map<std::string, int> nativeDeclarations_;
    std::unordered_map<std::string, int> functionCallCounts_;
    std::stack<Scope> callStack_;
    std::vector<std::string> errors_;
    const AstNode* returnNode_ = nullptr;
};

// Forward declare for native implementations
class CSimulation;
class CUnit;
class CPlayer;
