#pragma once
#include "scripting/JassParser.h"
#include "scripting/JassVM.h"
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

enum class JassOpCode : uint8_t {
    OP_CONST_INT = 0,
    OP_CONST_REAL,
    OP_CONST_BOOL,
    OP_CONST_STR,
    OP_CONST_NULL,

    OP_GET_LOCAL_SLOT,       // intOperand = slot index (0 .. numLocalSlots-1)
    OP_SET_LOCAL_SLOT,       // intOperand = slot index, pops value from stack

    OP_GET_LOCAL_ARRAY_SLOT, // intOperand = array slot index (0 .. numLocalArraySlots-1), pops index, pushes element
    OP_SET_LOCAL_ARRAY_SLOT, // intOperand = array slot index, pops value then index

    OP_GET_GLOBAL,           // strIndex = global variable name
    OP_SET_GLOBAL,           // strIndex = global variable name, pops value

    OP_GET_GLOBAL_ARRAY,     // strIndex = array name, pops index, pushes element
    OP_SET_GLOBAL_ARRAY,     // strIndex = array name, pops value then index

    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_NEG,
    OP_NOT,

    OP_EQ,
    OP_NEQ,
    OP_LT,
    OP_GT,
    OP_LE,
    OP_GE,

    OP_JMP,                  // intOperand = target instruction index
    OP_JMP_IF_FALSE,         // intOperand = target instruction index (pops condition)
    OP_JMP_IF_TRUE,          // intOperand = target instruction index (pops condition)

    OP_CALL,                 // strIndex = func name, intOperand = argCount (pushes return JassValue)
    OP_POP,                  // discards top of stack
    OP_RETURN,               // returns Null
    OP_RETURN_VAL,           // pops and returns top of stack
};

struct JassInstruction {
    JassOpCode op = JassOpCode::OP_RETURN;
    int32_t intOperand = 0;
    float realOperand = 0.0f;
    uint32_t strIndex = 0;
};

struct JassBytecodeChunk {
    std::string functionName;
    int numParams = 0;
    int numLocalSlots = 0;
    int numLocalArraySlots = 0;
    uint32_t constantFoldedCount = 0;
    std::vector<JassInstruction> code;
    std::vector<std::string> stringPool;
    std::vector<JassValue> localDefaults;
    std::unordered_map<std::string, int> localNameToSlot;
    std::vector<std::string> slotToLocalName;
    std::unordered_map<std::string, int> localArrayNameToSlot;
    std::vector<std::string> slotToLocalArrayName;
    std::vector<JassValue> localArrayDefaults;

    uint32_t InternString(const std::string& s) {
        for (size_t i = 0; i < stringPool.size(); ++i) {
            if (stringPool[i] == s) return static_cast<uint32_t>(i);
        }
        stringPool.push_back(s);
        return static_cast<uint32_t>(stringPool.size() - 1);
    }
};

class JassBytecodeCompiler {
public:
    using StructNamePredicate = std::function<bool(const std::string&)>;

    // Assigns dense 0..N-1 integer slot indices to parameters and scalar
    // local declarations on the AST in-place (`node->localSlot`).
    static void BindFunctionLocalSlots(AstNode* func,
                                       const StructNamePredicate& isStructType);

    // Compiles a FUNCTION_DECL AST node into a flat stack-based bytecode chunk
    // (supporting scalar locals, local arrays, and compile-time constant folding).
    static bool CompileFunction(const AstNode* func,
                                const StructNamePredicate& isStructType,
                                JassBytecodeChunk& outChunk);
};
