#include "JassVM.h"
#include <cmath>
#include <chrono>
#include <sstream>

JassVM::JassVM() {}

bool JassVM::LoadScript(const std::string& source) {
    auto started = std::chrono::steady_clock::now();
    JassLexer lexer(source);
    auto tokens = lexer.Tokenize();
    auto lexed = std::chrono::steady_clock::now();
    if (!lexer.GetErrors().empty()) {
        for (auto& e : lexer.GetErrors()) errors_.push_back("Lexer: " + e);
    }

    JassParser parser(tokens);
    auto program = parser.Parse();
    auto parsed = std::chrono::steady_clock::now();
    if (!parser.GetErrors().empty()) {
        for (auto& e : parser.GetErrors()) errors_.push_back("Parser: " + e);
    }

    // Index functions, natives, and execute global initializers
    if (program) {
        programs_.push_back(std::move(program));
        auto* loadedProgram = programs_.back().get();
        for (auto& child : loadedProgram->children) {
            if (child->type == AstNodeType::FUNCTION_DECL) {
                functions_[child->text] = child.get();
            } else if (child->type == AstNodeType::GLOBAL_DECL) {
                // Execute global initializer
                if (!child->children.empty()) {
                    globals_[child->text] = ExecuteNode(child->children[0].get());
                } else if (child->isArray) {
                    // Array globals start as null (will be created on first access)
                } else {
                    // Initialize based on type
                    if (child->returnType == "integer") globals_[child->text] = JassValue::Int(0);
                    else if (child->returnType == "real") globals_[child->text] = JassValue::Real(0.0f);
                    else if (child->returnType == "boolean") globals_[child->text] = JassValue::Bool(false);
                    else if (child->returnType == "string") globals_[child->text] = JassValue::Str("");
                    else globals_[child->text] = JassValue::Null();
                }
            } else if (child->type == AstNodeType::BLOCK) {
                // globals/endglobals block
                for (auto& decl : child->children) {
                    if (decl->type == AstNodeType::GLOBAL_DECL) {
                        if (!decl->children.empty()) {
                            globals_[decl->text] = ExecuteNode(decl->children[0].get());
                        } else if (decl->isArray) {
                            // Array globals start as null
                        } else {
                            if (decl->returnType == "integer") globals_[decl->text] = JassValue::Int(0);
                            else if (decl->returnType == "real") globals_[decl->text] = JassValue::Real(0.0f);
                            else if (decl->returnType == "boolean") globals_[decl->text] = JassValue::Bool(false);
                            else if (decl->returnType == "string") globals_[decl->text] = JassValue::Str("");
                            else globals_[decl->text] = JassValue::Null();
                        }
                    }
                    // TYPE_DECL nodes are just metadata, skip
                }
            }
        }
    }
    auto initialized = std::chrono::steady_clock::now();
    const auto lexMs = std::chrono::duration_cast<std::chrono::milliseconds>(lexed - started).count();
    const auto parseMs = std::chrono::duration_cast<std::chrono::milliseconds>(parsed - lexed).count();
    const auto initMs = std::chrono::duration_cast<std::chrono::milliseconds>(initialized - parsed).count();
    std::cout << "JASS VM: load complete tokens=" << tokens.size()
              << " lexMs=" << lexMs
              << " parseMs=" << parseMs
              << " initMs=" << initMs << std::endl;

    return !programs_.empty();
}

bool JassVM::LoadScriptFromMemory(const std::string& name, const uint8_t* data, size_t size) {
    std::string source((const char*)data, size);
    return LoadScript(source);
}

void JassVM::RegisterNative(const std::string& name, NativeFunc func) {
    natives_[name] = std::move(func);
}

void JassVM::SetGlobal(const std::string& name, const JassValue& val) {
    globals_[name] = val;
}

JassValue JassVM::GetGlobal(const std::string& name) const {
    auto it = globals_.find(name);
    if (it != globals_.end()) return it->second;
    return JassValue::Null();
}

void JassVM::Execute(const std::string& funcName) {
    auto it = functions_.find(funcName);
    if (it == functions_.end()) {
        // Try as a native call (some maps call natives via Execute)
        auto nat = natives_.find(funcName);
        if (nat != natives_.end()) {
            nat->second({});
        }
        return;
    }
    ExecuteFunction(it->second, {});
}

bool JassVM::HasFunction(const std::string& funcName) const {
    return functions_.find(funcName) != functions_.end();
}

JassValue JassVM::ExecuteFunction(const AstNode* func, const std::vector<JassValue>& args) {
    // Push new scope
    callStack_.push(Scope());

    // Set parameter values
    for (size_t i = 0; i < func->parameters.size() && i < args.size(); i++) {
        callStack_.top().locals[func->parameters[i].first] = args[i];
    }

    returnNode_ = nullptr;
    JassValue result;

    // Execute body (block is first child)
    if (!func->children.empty()) {
        for (auto& stmt : func->children[0]->children) {
            if (stmt->type == AstNodeType::RETURN_STMT) {
                returnNode_ = stmt.get();
                if (!stmt->children.empty()) {
                    result = ExecuteNode(stmt->children[0].get());
                }
                break;
            }
            result = ExecuteNode(stmt.get());
            if (returnNode_) break;
        }
    }

    callStack_.pop();
    return result;
}

JassValue JassVM::ExecuteNode(const AstNode* node) {
    if (!node) return JassValue::Null();

    switch (node->type) {
        case AstNodeType::LITERAL_INT:
            return JassValue::Int((int)node->numberValue);
        case AstNodeType::LITERAL_REAL:
            return JassValue::Real((float)node->numberValue);
        case AstNodeType::LITERAL_BOOL:
            return JassValue::Bool(node->text == "true");
        case AstNodeType::LITERAL_STRING:
            return JassValue::Str(node->text);
        case AstNodeType::LITERAL_NULL:
            return JassValue::Null();
        case AstNodeType::LITERAL_RAWCODE:
            return JassValue::Int((int)node->numberValue);
        case AstNodeType::LITERAL_FUNCTION_REF:
            // Store function name as string handle
            return JassValue::Str(node->text);

        case AstNodeType::VARIABLE: {
            // Check locals first, then globals
            if (!callStack_.empty()) {
                auto& locals = callStack_.top().locals;
                auto it = locals.find(node->text);
                if (it != locals.end()) return it->second;
            }
            auto it = globals_.find(node->text);
            if (it != globals_.end()) return it->second;
            return JassValue::Null();
        }

        case AstNodeType::FUNC_CALL: {
            // Check if it's a native
            auto nat = natives_.find(node->text);
            if (nat != natives_.end()) {
                std::vector<JassValue> args;
                for (auto& child : node->children) {
                    args.push_back(ExecuteNode(child.get()));
                }
                return nat->second(args);
            }
            // User function
            auto fit = functions_.find(node->text);
            if (fit != functions_.end()) {
                std::vector<JassValue> args;
                for (auto& child : node->children) {
                    args.push_back(ExecuteNode(child.get()));
                }
                return ExecuteFunction(fit->second, args);
            }
            errors_.push_back("Unknown function: " + node->text);
            return JassValue::Null();
        }

        case AstNodeType::BINARY_OP: {
            if (node->children.size() < 2) return JassValue::Null();

            // Boolean short-circuit for 'and' and 'or'
            if (node->text == "and") {
                auto left = ExecuteNode(node->children[0].get());
                if (!left.boolVal) return JassValue::Bool(false);
                auto right = ExecuteNode(node->children[1].get());
                return JassValue::Bool(right.boolVal);
            }
            if (node->text == "or") {
                auto left = ExecuteNode(node->children[0].get());
                if (left.boolVal) return JassValue::Bool(true);
                auto right = ExecuteNode(node->children[1].get());
                return JassValue::Bool(right.boolVal);
            }

            auto left = ExecuteNode(node->children[0].get());
            auto right = ExecuteNode(node->children[1].get());

            // Determine if we should use real arithmetic
            bool useReal = (left.type == JassValue::REAL || right.type == JassValue::REAL);

            if (node->text == "+") {
                if (left.type == JassValue::STRING || right.type == JassValue::STRING)
                    return JassValue::Str(left.strVal + right.strVal);
                if (useReal)
                    return JassValue::Real(left.realVal + right.realVal);
                return JassValue::Int(left.intVal + right.intVal);
            }
            if (node->text == "-") {
                if (useReal)
                    return JassValue::Real(left.realVal - right.realVal);
                return JassValue::Int(left.intVal - right.intVal);
            }
            if (node->text == "*") {
                if (useReal)
                    return JassValue::Real(left.realVal * right.realVal);
                return JassValue::Int(left.intVal * right.intVal);
            }
            if (node->text == "/") {
                if (useReal) {
                    if (right.realVal == 0.0f) return JassValue::Real(0.0f);
                    return JassValue::Real(left.realVal / right.realVal);
                }
                if (right.intVal == 0) return JassValue::Int(0);
                return JassValue::Int(left.intVal / right.intVal);
            }

            // Comparisons - use real if either operand is real
            if (node->text == "==") {
                if (useReal) return JassValue::Bool(left.realVal == right.realVal);
                if (left.type == JassValue::STRING && right.type == JassValue::STRING)
                    return JassValue::Bool(left.strVal == right.strVal);
                return JassValue::Bool(left.intVal == right.intVal);
            }
            if (node->text == "!=") {
                if (useReal) return JassValue::Bool(left.realVal != right.realVal);
                if (left.type == JassValue::STRING && right.type == JassValue::STRING)
                    return JassValue::Bool(left.strVal != right.strVal);
                return JassValue::Bool(left.intVal != right.intVal);
            }
            if (node->text == "<") {
                if (useReal) return JassValue::Bool(left.realVal < right.realVal);
                return JassValue::Bool(left.intVal < right.intVal);
            }
            if (node->text == ">") {
                if (useReal) return JassValue::Bool(left.realVal > right.realVal);
                return JassValue::Bool(left.intVal > right.intVal);
            }
            if (node->text == "<=") {
                if (useReal) return JassValue::Bool(left.realVal <= right.realVal);
                return JassValue::Bool(left.intVal <= right.intVal);
            }
            if (node->text == ">=") {
                if (useReal) return JassValue::Bool(left.realVal >= right.realVal);
                return JassValue::Bool(left.intVal >= right.intVal);
            }
            return JassValue::Null();
        }

        case AstNodeType::UNARY_OP: {
            auto val = ExecuteNode(node->children[0].get());
            if (node->text == "-") {
                if (val.type == JassValue::REAL) return JassValue::Real(-val.realVal);
                return JassValue::Int(-val.intVal);
            }
            if (node->text == "not") return JassValue::Bool(!val.boolVal);
            return val;
        }

        case AstNodeType::SET_STMT: {
            // Local declaration
            if (!node->returnType.empty()) {
                // This is a local declaration
                if (node->isArray) {
                    // Array declaration: local integer array arr
                    // Create an empty array in locals
                    if (!callStack_.empty()) {
                        callStack_.top().locals[node->text] = JassValue::Null(); // placeholder for array
                    }
                    return JassValue::Null();
                }
                JassValue val;
                if (!node->children.empty()) {
                    val = ExecuteNode(node->children[0].get());
                } else {
                    // Default initialization
                    if (node->returnType == "integer") val = JassValue::Int(0);
                    else if (node->returnType == "real") val = JassValue::Real(0.0f);
                    else if (node->returnType == "boolean") val = JassValue::Bool(false);
                    else if (node->returnType == "string") val = JassValue::Str("");
                    else val = JassValue::Null();
                }
                if (!callStack_.empty()) {
                    callStack_.top().locals[node->text] = val;
                }
                return val;
            }

            // Set statement (assignment)
            if (!node->children.empty() && node->children[0]->type == AstNodeType::ARRAY_ACCESS) {
                // Array element assignment: set arr[i] = val
                auto& arrAccess = node->children[0];
                int index = (int)ExecuteNode(arrAccess->children[0].get()).intVal;
                JassValue val;
                if (node->children.size() > 1) {
                    val = ExecuteNode(node->children[1].get());
                }
                // Store in array storage
                std::string arrName = arrAccess->text;
                std::string key = arrName + "[" + std::to_string(index) + "]";
                if (!callStack_.empty() && callStack_.top().locals.find(arrName) != callStack_.top().locals.end()) {
                    callStack_.top().locals[key] = val;
                } else {
                    globals_[key] = val;
                }
                return val;
            }

            // Simple assignment: set x = val
            JassValue val;
            if (!node->children.empty()) {
                val = ExecuteNode(node->children[0].get());
            }
            // Write to local if exists, otherwise global
            if (!callStack_.empty()) {
                auto& locals = callStack_.top().locals;
                if (locals.find(node->text) != locals.end()) {
                    locals[node->text] = val;
                    return val;
                }
            }
            globals_[node->text] = val;
            return val;
        }

        case AstNodeType::ARRAY_ACCESS: {
            // Reading from array: arr[i]
            int index = 0;
            if (!node->children.empty()) {
                index = (int)ExecuteNode(node->children[0].get()).intVal;
            }
            std::string key = node->text + "[" + std::to_string(index) + "]";
            // Check locals first
            if (!callStack_.empty()) {
                auto& locals = callStack_.top().locals;
                auto it = locals.find(key);
                if (it != locals.end()) return it->second;
            }
            auto it = globals_.find(key);
            if (it != globals_.end()) return it->second;
            return JassValue::Null();
        }

        case AstNodeType::CALL_STMT: {
            // Similar to FUNC_CALL but no return value (statement)
            auto nat = natives_.find(node->text);
            if (nat != natives_.end()) {
                std::vector<JassValue> args;
                for (auto& child : node->children) {
                    args.push_back(ExecuteNode(child.get()));
                }
                nat->second(args);
            } else {
                auto fit = functions_.find(node->text);
                if (fit != functions_.end()) {
                    std::vector<JassValue> args;
                    for (auto& child : node->children) {
                        args.push_back(ExecuteNode(child.get()));
                    }
                    ExecuteFunction(fit->second, args);
                }
            }
            return JassValue::Null();
        }

        case AstNodeType::IF_STMT: {
            // children: [0]=condition, [1]=thenBlock, [2+]=elseif/else
            if (node->children.size() < 2) return JassValue::Null();
            auto cond = ExecuteNode(node->children[0].get());
            if (cond.boolVal) {
                for (auto& stmt : node->children[1]->children) {
                    auto r = ExecuteNode(stmt.get());
                    if (returnNode_) return r;
                }
            } else {
                // Check elseif/else
                for (size_t i = 2; i < node->children.size(); i++) {
                    if (node->children[i]->type == AstNodeType::IF_STMT) {
                        auto elseifCond = ExecuteNode(node->children[i]->children[0].get());
                        if (elseifCond.boolVal) {
                            for (auto& stmt : node->children[i]->children[1]->children) {
                                auto r = ExecuteNode(stmt.get());
                                if (returnNode_) return r;
                            }
                            break;
                        }
                    } else {
                        // else block
                        for (auto& stmt : node->children[i]->children) {
                            auto r = ExecuteNode(stmt.get());
                            if (returnNode_) return r;
                        }
                    }
                }
            }
            return JassValue::Null();
        }

        case AstNodeType::LOOP_STMT: {
            int maxIterations = 1000000; // safety limit
            while (maxIterations-- > 0) {
                bool exited = false;
                for (auto& stmt : node->children[0]->children) {
                    if (stmt->type == AstNodeType::EXITWHEN) {
                        auto cond = ExecuteNode(stmt->children[0].get());
                        if (cond.boolVal) { exited = true; break; }
                        continue;
                    }
                    auto r = ExecuteNode(stmt.get());
                    if (returnNode_) return r;
                }
                if (exited) break;
            }
            return JassValue::Null();
        }

        case AstNodeType::EXITWHEN: {
            // Handled by loop
            return JassValue::Null();
        }

        case AstNodeType::RETURN_STMT: {
            returnNode_ = node;
            if (!node->children.empty()) {
                return ExecuteNode(node->children[0].get());
            }
            return JassValue::Null();
        }

        case AstNodeType::BLOCK: {
            for (auto& stmt : node->children) {
                auto r = ExecuteNode(stmt.get());
                if (returnNode_) return r;
            }
            return JassValue::Null();
        }

        default:
            return JassValue::Null();
    }
}
