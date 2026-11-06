#include "scripting/JassBytecode.h"
#include <unordered_set>

namespace {

JassValue DefaultValueForJassType(const std::string& type) {
    if (type == "integer") return JassValue::Int(0);
    if (type == "real") return JassValue::Real(0.0f);
    if (type == "boolean") return JassValue::Bool(false);
    if (type == "string") return JassValue::Str("");
    return JassValue::Null();
}

void BindExprSlotsRecursive(AstNode* node,
                            const std::unordered_map<std::string, int>& slotMap,
                            const std::unordered_set<std::string>& localArrays) {
    if (!node) return;
    if (node->type == AstNodeType::VARIABLE) {
        if (localArrays.count(node->text) == 0) {
            auto it = slotMap.find(node->text);
            if (it != slotMap.end()) {
                node->localSlot = it->second;
            }
        }
    } else if (node->type == AstNodeType::SET_STMT) {
        const bool isMember = !node->children.empty() &&
                              node->children[0]->type == AstNodeType::MEMBER_ACCESS;
        const bool isArrayAssign = !node->children.empty() &&
                                   node->children[0]->type == AstNodeType::ARRAY_ACCESS &&
                                   node->children.size() > 1;
        if (!isMember && !isArrayAssign && !node->isArray) {
            auto it = slotMap.find(node->text);
            if (it != slotMap.end()) {
                node->localSlot = it->second;
            }
        }
    }
    for (auto& child : node->children) {
        BindExprSlotsRecursive(child.get(), slotMap, localArrays);
    }
}

void CollectLocalDeclsRecursive(AstNode* node,
                                std::unordered_map<std::string, int>& slotMap,
                                std::vector<std::string>& slotNames,
                                std::vector<JassValue>& slotDefaults,
                                std::unordered_set<std::string>& localArrays,
                                const JassBytecodeCompiler::StructNamePredicate& isStructType) {
    if (!node) return;
    if (node->type == AstNodeType::SET_STMT && !node->returnType.empty()) {
        if (node->isArray) {
            localArrays.insert(node->text);
        } else if (node->text.compare(0, 9, "thistype@") != 0 &&
                   (!isStructType || !isStructType(node->text))) {
            if (slotMap.find(node->text) == slotMap.end()) {
                const int slot = static_cast<int>(slotNames.size());
                slotMap[node->text] = slot;
                slotNames.push_back(node->text);
                slotDefaults.push_back(DefaultValueForJassType(node->returnType));
            }
        }
    }
    for (auto& child : node->children) {
        CollectLocalDeclsRecursive(child.get(), slotMap, slotNames, slotDefaults, localArrays, isStructType);
    }
}

struct CompilerContext {
    JassBytecodeChunk& chunk;
    const JassBytecodeCompiler::StructNamePredicate& isStructType;
    std::vector<std::vector<size_t>> loopExitPatches;
    bool supported = true;

    size_t Emit(JassOpCode op, int32_t intOp = 0, float realOp = 0.0f, uint32_t strIdx = 0) {
        JassInstruction instr;
        instr.op = op;
        instr.intOperand = intOp;
        instr.realOperand = realOp;
        instr.strIndex = strIdx;
        chunk.code.push_back(instr);
        return chunk.code.size() - 1;
    }

    void PatchJump(size_t instrIdx, int32_t targetIdx) {
        chunk.code[instrIdx].intOperand = targetIdx;
    }

    bool CompileExpr(const AstNode* node) {
        if (!node || !supported) return false;
        switch (node->type) {
            case AstNodeType::LITERAL_INT:
            case AstNodeType::LITERAL_RAWCODE:
                Emit(JassOpCode::OP_CONST_INT, static_cast<int32_t>(node->numberValue));
                return true;
            case AstNodeType::LITERAL_REAL:
                Emit(JassOpCode::OP_CONST_REAL, 0, static_cast<float>(node->numberValue));
                return true;
            case AstNodeType::LITERAL_BOOL:
                Emit(JassOpCode::OP_CONST_BOOL, node->text == "true" ? 1 : 0);
                return true;
            case AstNodeType::LITERAL_STRING:
            case AstNodeType::LITERAL_FUNCTION_REF:
                Emit(JassOpCode::OP_CONST_STR, 0, 0.0f, chunk.InternString(node->text));
                return true;
            case AstNodeType::LITERAL_NULL:
                Emit(JassOpCode::OP_CONST_NULL);
                return true;
            case AstNodeType::VARIABLE: {
                if (node->text.compare(0, 9, "thistype@") == 0 ||
                    (isStructType && isStructType(node->text))) {
                    supported = false;
                    return false;
                }
                if (node->localSlot >= 0) {
                    Emit(JassOpCode::OP_GET_LOCAL_SLOT, node->localSlot);
                } else {
                    Emit(JassOpCode::OP_GET_GLOBAL, 0, 0.0f, chunk.InternString(node->text));
                }
                return true;
            }
            case AstNodeType::ARRAY_ACCESS: {
                if (node->children.empty()) return (supported = false);
                if (chunk.localNameToSlot.count(node->text) > 0) {
                    return (supported = false);
                }
                if (!CompileExpr(node->children[0].get())) return false;
                Emit(JassOpCode::OP_GET_GLOBAL_ARRAY, 0, 0.0f, chunk.InternString(node->text));
                return true;
            }
            case AstNodeType::UNARY_OP: {
                if (node->children.empty()) return (supported = false);
                if (!CompileExpr(node->children[0].get())) return false;
                if (node->text == "-") Emit(JassOpCode::OP_NEG);
                else if (node->text == "not") Emit(JassOpCode::OP_NOT);
                return true;
            }
            case AstNodeType::BINARY_OP: {
                if (node->children.size() < 2) return (supported = false);
                if (node->text == "and") {
                    if (!CompileExpr(node->children[0].get())) return false;
                    size_t jmpFalse = Emit(JassOpCode::OP_JMP_IF_FALSE, 0);
                    if (!CompileExpr(node->children[1].get())) return false;
                    size_t jmpEnd = Emit(JassOpCode::OP_JMP, 0);
                    PatchJump(jmpFalse, static_cast<int32_t>(chunk.code.size()));
                    Emit(JassOpCode::OP_CONST_BOOL, 0);
                    PatchJump(jmpEnd, static_cast<int32_t>(chunk.code.size()));
                    return true;
                }
                if (node->text == "or") {
                    if (!CompileExpr(node->children[0].get())) return false;
                    size_t jmpTrue = Emit(JassOpCode::OP_JMP_IF_TRUE, 0);
                    if (!CompileExpr(node->children[1].get())) return false;
                    size_t jmpEnd = Emit(JassOpCode::OP_JMP, 0);
                    PatchJump(jmpTrue, static_cast<int32_t>(chunk.code.size()));
                    Emit(JassOpCode::OP_CONST_BOOL, 1);
                    PatchJump(jmpEnd, static_cast<int32_t>(chunk.code.size()));
                    return true;
                }
                if (!CompileExpr(node->children[0].get())) return false;
                if (!CompileExpr(node->children[1].get())) return false;
                if (node->text == "+") Emit(JassOpCode::OP_ADD);
                else if (node->text == "-") Emit(JassOpCode::OP_SUB);
                else if (node->text == "*") Emit(JassOpCode::OP_MUL);
                else if (node->text == "/") Emit(JassOpCode::OP_DIV);
                else if (node->text == "==") Emit(JassOpCode::OP_EQ);
                else if (node->text == "!=") Emit(JassOpCode::OP_NEQ);
                else if (node->text == "<") Emit(JassOpCode::OP_LT);
                else if (node->text == ">") Emit(JassOpCode::OP_GT);
                else if (node->text == "<=") Emit(JassOpCode::OP_LE);
                else if (node->text == ">=") Emit(JassOpCode::OP_GE);
                else return (supported = false);
                return true;
            }
            case AstNodeType::FUNC_CALL: {
                for (const auto& arg : node->children) {
                    if (!CompileExpr(arg.get())) return false;
                }
                Emit(JassOpCode::OP_CALL, static_cast<int32_t>(node->children.size()), 0.0f,
                     chunk.InternString(node->text));
                return true;
            }
            default:
                supported = false;
                return false;
        }
    }

    bool CompileStmt(const AstNode* stmt) {
        if (!stmt || !supported) return false;
        switch (stmt->type) {
            case AstNodeType::BLOCK: {
                for (const auto& child : stmt->children) {
                    if (!CompileStmt(child.get())) return false;
                }
                return true;
            }
            case AstNodeType::SET_STMT: {
                if (!stmt->children.empty() && stmt->children[0]->type == AstNodeType::MEMBER_ACCESS) {
                    return (supported = false);
                }
                if (!stmt->returnType.empty()) {
                    // Local variable declaration
                    if (stmt->isArray) {
                        return (supported = false);
                    }
                    if (!stmt->children.empty()) {
                        if (!CompileExpr(stmt->children[0].get())) return false;
                    } else {
                        JassValue defVal = DefaultValueForJassType(stmt->returnType);
                        switch (defVal.type) {
                            case JassValue::INTEGER: Emit(JassOpCode::OP_CONST_INT, defVal.intVal); break;
                            case JassValue::REAL: Emit(JassOpCode::OP_CONST_REAL, 0, defVal.realVal); break;
                            case JassValue::BOOLEAN: Emit(JassOpCode::OP_CONST_BOOL, defVal.boolVal ? 1 : 0); break;
                            case JassValue::STRING: Emit(JassOpCode::OP_CONST_STR, 0, 0.0f, chunk.InternString(defVal.strVal)); break;
                            default: Emit(JassOpCode::OP_CONST_NULL); break;
                        }
                    }
                    if (stmt->localSlot < 0) return (supported = false);
                    Emit(JassOpCode::OP_SET_LOCAL_SLOT, stmt->localSlot);
                    return true;
                }
                if (!stmt->children.empty() && stmt->children[0]->type == AstNodeType::ARRAY_ACCESS &&
                    stmt->children.size() > 1) {
                    const auto& arrAccess = stmt->children[0];
                    if (arrAccess->children.empty()) return (supported = false);
                    if (!CompileExpr(arrAccess->children[0].get())) return false;
                    if (!CompileExpr(stmt->children[1].get())) return false;
                    Emit(JassOpCode::OP_SET_GLOBAL_ARRAY, 0, 0.0f, chunk.InternString(arrAccess->text));
                    return true;
                }
                if (stmt->children.empty()) {
                    Emit(JassOpCode::OP_CONST_NULL);
                } else {
                    if (!CompileExpr(stmt->children[0].get())) return false;
                }
                if (stmt->localSlot >= 0) {
                    Emit(JassOpCode::OP_SET_LOCAL_SLOT, stmt->localSlot);
                } else {
                    Emit(JassOpCode::OP_SET_GLOBAL, 0, 0.0f, chunk.InternString(stmt->text));
                }
                return true;
            }
            case AstNodeType::CALL_STMT: {
                for (const auto& arg : stmt->children) {
                    if (!CompileExpr(arg.get())) return false;
                }
                Emit(JassOpCode::OP_CALL, static_cast<int32_t>(stmt->children.size()), 0.0f,
                     chunk.InternString(stmt->text));
                Emit(JassOpCode::OP_POP);
                return true;
            }
            case AstNodeType::IF_STMT: {
                if (stmt->children.size() < 2) return (supported = false);
                if (!CompileExpr(stmt->children[0].get())) return false;
                size_t jmpNextBranch = Emit(JassOpCode::OP_JMP_IF_FALSE, 0);
                if (!CompileStmt(stmt->children[1].get())) return false;
                std::vector<size_t> endJumps;
                endJumps.push_back(Emit(JassOpCode::OP_JMP, 0));
                PatchJump(jmpNextBranch, static_cast<int32_t>(chunk.code.size()));

                for (size_t i = 2; i < stmt->children.size(); ++i) {
                    const AstNode* branch = stmt->children[i].get();
                    if (branch->type == AstNodeType::IF_STMT) {
                        if (branch->children.size() < 2) return (supported = false);
                        if (!CompileExpr(branch->children[0].get())) return false;
                        size_t jmpElseIf = Emit(JassOpCode::OP_JMP_IF_FALSE, 0);
                        if (!CompileStmt(branch->children[1].get())) return false;
                        endJumps.push_back(Emit(JassOpCode::OP_JMP, 0));
                        PatchJump(jmpElseIf, static_cast<int32_t>(chunk.code.size()));
                    } else {
                        if (!CompileStmt(branch)) return false;
                    }
                }
                const int32_t endTarget = static_cast<int32_t>(chunk.code.size());
                for (size_t j : endJumps) {
                    PatchJump(j, endTarget);
                }
                return true;
            }
            case AstNodeType::LOOP_STMT: {
                if (stmt->children.empty()) return true;
                const int32_t loopStart = static_cast<int32_t>(chunk.code.size());
                loopExitPatches.emplace_back();
                if (!CompileStmt(stmt->children[0].get())) return false;
                Emit(JassOpCode::OP_JMP, loopStart);
                const int32_t loopEnd = static_cast<int32_t>(chunk.code.size());
                for (size_t patchIdx : loopExitPatches.back()) {
                    PatchJump(patchIdx, loopEnd);
                }
                loopExitPatches.pop_back();
                return true;
            }
            case AstNodeType::EXITWHEN: {
                if (loopExitPatches.empty() || stmt->children.empty()) return (supported = false);
                if (!CompileExpr(stmt->children[0].get())) return false;
                size_t jmpExit = Emit(JassOpCode::OP_JMP_IF_TRUE, 0);
                loopExitPatches.back().push_back(jmpExit);
                return true;
            }
            case AstNodeType::RETURN_STMT: {
                if (!stmt->children.empty()) {
                    if (!CompileExpr(stmt->children[0].get())) return false;
                    Emit(JassOpCode::OP_RETURN_VAL);
                } else {
                    Emit(JassOpCode::OP_RETURN);
                }
                return true;
            }
            default:
                supported = false;
                return false;
        }
    }
};

} // namespace

void JassBytecodeCompiler::BindFunctionLocalSlots(AstNode* func,
                                                  const StructNamePredicate& isStructType) {
    if (!func || func->type != AstNodeType::FUNCTION_DECL) return;
    std::unordered_map<std::string, int> slotMap;
    std::vector<std::string> slotNames;
    std::vector<JassValue> slotDefaults;
    std::unordered_set<std::string> localArrays;

    for (const auto& [paramName, paramType] : func->parameters) {
        if (slotMap.find(paramName) == slotMap.end()) {
            const int slot = static_cast<int>(slotNames.size());
            slotMap[paramName] = slot;
            slotNames.push_back(paramName);
            slotDefaults.push_back(DefaultValueForJassType(paramType));
        }
    }

    if (!func->children.empty()) {
        CollectLocalDeclsRecursive(func->children[0].get(), slotMap, slotNames, slotDefaults, localArrays, isStructType);
        BindExprSlotsRecursive(func->children[0].get(), slotMap, localArrays);
    }
    func->localSlotCount = static_cast<int>(slotNames.size());
}

bool JassBytecodeCompiler::CompileFunction(const AstNode* func,
                                           const StructNamePredicate& isStructType,
                                           JassBytecodeChunk& outChunk) {
    if (!func || func->type != AstNodeType::FUNCTION_DECL) return false;
    outChunk = JassBytecodeChunk{};
    outChunk.functionName = func->text;
    outChunk.numParams = static_cast<int>(func->parameters.size());

    std::unordered_set<std::string> localArrays;
    for (const auto& [paramName, paramType] : func->parameters) {
        if (outChunk.localNameToSlot.find(paramName) == outChunk.localNameToSlot.end()) {
            const int slot = static_cast<int>(outChunk.slotToLocalName.size());
            outChunk.localNameToSlot[paramName] = slot;
            outChunk.slotToLocalName.push_back(paramName);
            outChunk.localDefaults.push_back(DefaultValueForJassType(paramType));
        }
    }

    if (!func->children.empty()) {
        CollectLocalDeclsRecursive(func->children[0].get(),
                                   outChunk.localNameToSlot,
                                   outChunk.slotToLocalName,
                                   outChunk.localDefaults,
                                   localArrays,
                                   isStructType);
    }

    if (!localArrays.empty()) {
        return false;
    }

    outChunk.numLocalSlots = static_cast<int>(outChunk.slotToLocalName.size());
    CompilerContext ctx{outChunk, isStructType, {}, true};
    if (!func->children.empty()) {
        if (!ctx.CompileStmt(func->children[0].get())) {
            return false;
        }
    }
    ctx.Emit(JassOpCode::OP_RETURN);
    return ctx.supported;
}
