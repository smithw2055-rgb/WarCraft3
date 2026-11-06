#include "JassVM.h"
#include "JassBytecode.h"
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <sstream>

JassVM::~JassVM() = default;

JassVM::JassVM() {}

static bool JassValueToBool(const JassValue& value) {
    switch (value.type) {
        case JassValue::BOOLEAN:
            return value.boolVal;
        case JassValue::INTEGER:
            return value.intVal != 0;
        case JassValue::REAL:
            return value.realVal != 0.0f;
        case JassValue::STRING:
            return !value.strVal.empty();
        default:
            return false;
    }
}

bool JassVM::LoadScript(const std::string& source) {
    auto started = std::chrono::steady_clock::now();
    // vJASS textmacro expansion precedes lexing (ref TextMacros).
    JassLexer lexer(JassLexer::ExpandTextMacros(source));
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
    for (const auto& init : parser.GetInitializers()) {
        if (std::find(initializers_.begin(), initializers_.end(), init) == initializers_.end()) {
            initializers_.push_back(init);
        }
    }
    for (const auto& lib : parser.GetLibraries()) {
        libInitByLib_[lib.name] = lib.initializer;
        auto& needs = libNeedsByInit_[lib.initializer];
        for (const auto& dep : lib.needs) {
            if (std::find(needs.begin(), needs.end(), dep) == needs.end()) {
                needs.push_back(dep);
            }
        }
    }

    // Index functions, natives, structs, and execute global initializers
    if (program) {
        programs_.push_back(std::move(program));
        auto* loadedProgram = programs_.back().get();
        CollectScriptStats(loadedProgram);
        for (auto& child : loadedProgram->children) {
            if (child->type == AstNodeType::FUNCTION_DECL) {
                functions_[child->text] = child.get();
            } else if (child->type == AstNodeType::STRUCT_DECL) {
                JassStructDef def;
                def.name = child->text;
                def.parent = child->extendsType;
                def.isInterface = child->isInterface;
                def.delegates = child->delegates;
                for (auto& member : child->children) {
                    if (member->type == AstNodeType::FUNCTION_DECL) {
                        // Method text is already "Type.name" (mangled).
                        const std::string full = member->text;
                        const size_t dot = full.rfind('.');
                        const std::string methodName =
                            (dot == std::string::npos) ? full : full.substr(dot + 1);
                        def.methods.push_back({methodName, member->isStatic, member.get()});
                        functions_[full] = member.get();
                    } else if (member->type == AstNodeType::GLOBAL_DECL) {
                        JassStructMember info;
                        info.name = member->text;
                        info.type = member->returnType;
                        info.isArray = member->isArray;
                        info.isStatic = member->isStatic;
                        if (!member->children.empty()) {
                            info.defaultExpr = member->children[0].get();
                        }
                        if (info.isStatic) {
                            structStatics_[def.name + "." + info.name] =
                                info.defaultExpr ? ExecuteNode(info.defaultExpr)
                                                 : DefaultStructValue(info.type);
                        }
                        def.members.push_back(std::move(info));
                    }
                }
                structDefs_[def.name] = std::move(def);
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
        // vJASS hooks apply once all functions of this script are indexed.
        for (const auto& hook : parser.GetHooks()) {
            HookFunction(hook.first, hook.second);
        }
        CompileAndBindFunctions();
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

std::unordered_set<std::string> JassVM::GetFunctionNames() const {
    std::unordered_set<std::string> names;
    names.reserve(functions_.size());
    for (const auto& entry : functions_) {
        names.insert(entry.first);
    }
    return names;
}

void JassVM::CollectScriptStats(const AstNode* node) {
    if (!node) {
        return;
    }
    if (node->type == AstNodeType::NATIVE_DECL) {
        nativeDeclarations_[node->text]++;
    } else if (node->type == AstNodeType::FUNC_CALL) {
        functionCallCounts_[node->text]++;
    }
    for (const auto& child : node->children) {
        CollectScriptStats(child.get());
    }
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
    (void)CallFunction(funcName);
}

void JassVM::ExecuteInitializers() {
    // Stable topological order honoring library `needs` (ref init order):
    // repeatedly run the earliest-listed initializer whose deps are done.
    // Absent (optional) libs and cycles fall back to listed order.
    std::unordered_set<std::string> done;
    std::vector<std::string> pending = initializers_;
    auto depsMet = [&](const std::string& init) {
        auto it = libNeedsByInit_.find(init);
        if (it == libNeedsByInit_.end()) return true;
        for (const auto& depLib : it->second) {
            auto lit = libInitByLib_.find(depLib);
            if (lit == libInitByLib_.end()) continue;
            if (!done.count(lit->second)) return false;
        }
        return true;
    };
    bool progress = true;
    while (!pending.empty() && progress) {
        progress = false;
        for (auto it = pending.begin(); it != pending.end();) {
            if (!depsMet(*it)) {
                ++it;
                continue;
            }
            const std::string name = *it;
            it = pending.erase(it);
            auto fit = functions_.find(name);
            if (fit != functions_.end()) {
                ExecuteFunction(fit->second, {});
            }
            done.insert(name);
            progress = true;
        }
    }
    for (const auto& name : pending) {
        auto fit = functions_.find(name);
        if (fit != functions_.end()) {
            ExecuteFunction(fit->second, {});
        }
    }
}

bool JassVM::HookFunction(const std::string& orig, const std::string& replacement) {
    auto oit = functions_.find(orig);
    if (oit == functions_.end()) {
        errors_.push_back("hook: unknown function '" + orig + "'");
        return false;
    }
    auto rit = functions_.find(replacement);
    if (rit == functions_.end()) {
        errors_.push_back("hook: unknown function '" + replacement + "'");
        return false;
    }
    const AstNode* origNode = oit->second;
    const AstNode* replNode = rit->second;
    const std::string saved = orig + "@hooked" + std::to_string(++hookCounter_);
    // Redirect references to ORIG inside the replacement to the saved
    // original (nodes are owned by programs_; the rename is permanent).
    std::function<void(AstNode*)> rewrite = [&](AstNode* node) {
        if (!node) return;
        if ((node->type == AstNodeType::FUNC_CALL ||
             node->type == AstNodeType::LITERAL_FUNCTION_REF) &&
            node->text == orig) {
            node->text = saved;
        }
        for (auto& child : node->children) {
            rewrite(child.get());
        }
    };
    rewrite(const_cast<AstNode*>(replNode));
    functions_[saved] = origNode;
    functions_[orig] = replNode;
    CompileAndBindFunctions();
    return true;
}

const JassVM::JassStructDef* JassVM::FindStructDef(const std::string& name) const {
    auto it = structDefs_.find(name);
    return it != structDefs_.end() ? &it->second : nullptr;
}

bool JassVM::IsStructBaseOf(const std::string& base, const std::string& derived) const {
    for (const JassStructDef* d = FindStructDef(derived); d != nullptr;) {
        if (d->name == base) return true;
        d = d->parent.empty() ? nullptr : FindStructDef(d->parent);
    }
    return false;
}

int JassVM::ThistypeInstance(const std::string& structName) const {
    if (thistypeStack_.empty()) return 0;
    const auto& top = thistypeStack_.back();
    if (top.second == 0) return 0;
    if (top.first == structName || IsStructBaseOf(structName, top.first) ||
        IsStructBaseOf(top.first, structName)) {
        return top.second;
    }
    return 0;
}

int JassVM::FindDelegateInstance(const std::string& type, int instanceId, const std::string& name) const {
    int currentId = instanceId;
    std::string currentType = type;
    for (int hop = 0; hop < 4; ++hop) {
        const JassStructDef* start = FindStructDef(currentType);
        int nextId = 0;
        std::string nextType;
        for (const JassStructDef* d = start; d != nullptr;) {
            for (const auto& del : d->delegates) {
                auto host = structInstances_.find(currentId);
                if (host == structInstances_.end()) return 0;
                auto held = host->second.scalars.find(del.second);
                if (held == host->second.scalars.end() || held->second.intVal == 0) continue;
                auto target = structInstances_.find(held->second.intVal);
                if (target == structInstances_.end()) continue;
                if (FindStructMethod(target->second.type, name) ||
                    (FindStructDef(target->second.type) &&
                     FindStructMember(FindStructDef(target->second.type), name))) {
                    nextId = held->second.intVal;
                    nextType = target->second.type;
                    break;
                }
            }
            if (nextId != 0) break;
            d = d->parent.empty() ? nullptr : FindStructDef(d->parent);
        }
        if (nextId == 0) return 0;
        if (FindStructMethod(nextType, name) ||
            (FindStructDef(nextType) && FindStructMember(FindStructDef(nextType), name))) {
            return nextId;
        }
        currentId = nextId;
        currentType = nextType;
    }
    return 0;
}

const JassVM::JassStructMember* JassVM::FindStructMember(const JassStructDef* def,
                                                        const std::string& member) const {
    for (const JassStructDef* d = def; d != nullptr;) {
        for (const auto& m : d->members) {
            if (m.name == member) return &m;
        }
        d = d->parent.empty() ? nullptr : FindStructDef(d->parent);
    }
    return nullptr;
}

const JassVM::JassStructMethod* JassVM::FindStructMethod(const std::string& type,
                                                        const std::string& method) const {
    for (const JassStructDef* d = FindStructDef(type); d != nullptr;) {
        for (const auto& m : d->methods) {
            if (m.name == method) return &m;
        }
        d = d->parent.empty() ? nullptr : FindStructDef(d->parent);
    }
    return nullptr;
}

JassValue JassVM::DefaultStructValue(const std::string& type) const {
    if (type == "integer") return JassValue::Int(0);
    if (type == "real") return JassValue::Real(0.0f);
    if (type == "boolean") return JassValue::Bool(false);
    if (type == "string") return JassValue::Str("");
    return JassValue::Null(); // handles, codes, struct ids (0 = null)
}

int JassVM::AllocateStruct(const std::string& type) {
    const JassStructDef* def = FindStructDef(type);
    if (!def) return 0;
    if (def->isInterface) {
        errors_.push_back("Cannot allocate interface '" + type + "'");
        return 0;
    }
    const int id = nextStructId_++;
    JassStructInstance instance;
    instance.type = type;
    // Defaults apply root-first so derived members win.
    std::vector<const JassStructDef*> chain;
    for (const JassStructDef* d = def; d != nullptr;) {
        chain.push_back(d);
        d = d->parent.empty() ? nullptr : FindStructDef(d->parent);
    }
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        for (const auto& m : (*it)->members) {
            if (m.isStatic || m.isArray) continue;
            JassValue value = m.defaultExpr ? ExecuteNode(m.defaultExpr)
                                            : DefaultStructValue(m.type);
            instance.scalars[m.name] = value;
        }
    }
    structInstances_[id] = std::move(instance);
    return id;
}

JassValue JassVM::CallFunction(const std::string& funcName) {
    auto it = functions_.find(funcName);
    if (it == functions_.end()) {
        // Try as a native call (some maps call natives via Execute)
        auto nat = natives_.find(funcName);
        if (nat != natives_.end()) {
            return nat->second({});
        }
        return JassValue::Null();
    }
    return ExecuteFunction(it->second, {});
}

bool JassVM::PushThreadFrame(ThreadState& state, const std::string& functionName, const std::vector<JassValue>& args) {
    // Temporary frame tracer (WARSMASH_TRACE_NESTED=1).
    if (std::getenv("WARSMASH_TRACE_NESTED")) {
        std::cerr << "[NEST] " << functionName << std::endl;
    }
    auto it = functions_.find(functionName);
    if (it == functions_.end()) {
        return false;
    }
    const AstNode* func = it->second;
    if (func->children.empty()) {
        return false;
    }
    ThreadFrame frame;
    frame.function = func;
    frame.block = func->children[0].get();
    frame.statementIndex = 0;
    frame.callScope = true;
    if (func->localSlotCount > 0) {
        frame.scope.fastLocals.resize((size_t)func->localSlotCount, JassValue::Null());
    }
    for (size_t i = 0; i < func->parameters.size() && i < args.size(); ++i) {
        frame.scope.locals[func->parameters[i].first] = args[i];
        if (i < frame.scope.fastLocals.size()) {
            frame.scope.fastLocals[i] = args[i];
        }
    }
    state.frames.push_back(std::move(frame));
    return true;
}

bool JassVM::PushBlockFrame(ThreadState& state, const AstNode* block, bool loop) {
    if (!block) {
        return false;
    }
    ThreadFrame frame;
    frame.block = block;
    frame.statementIndex = 0;
    frame.callScope = false;
    frame.loop = loop;
    state.frames.push_back(std::move(frame));
    return true;
}

void JassVM::PopCurrentFunctionFrames(ThreadState& state) {
    while (!state.frames.empty()) {
        const bool wasCallScope = state.frames.back().callScope;
        state.frames.pop_back();
        if (wasCallScope) {
            break;
        }
    }
}

void JassVM::SyncCallStackFromThread(const ThreadState& state) {
    callStack_.clear();
    callStack_.reserve(state.frames.size());
    for (const ThreadFrame& frame : state.frames) {
        if (frame.callScope) {
            callStack_.push_back(frame.scope);
        }
    }
}

void JassVM::SyncThreadFromCallStack(ThreadState& state) {
    size_t scopeIndex = 0;
    for (ThreadFrame& frame : state.frames) {
        if (!frame.callScope) {
            continue;
        }
        if (scopeIndex >= callStack_.size()) {
            break;
        }
        frame.scope = callStack_[scopeIndex++];
    }
}

JassVM::ThreadRunResult JassVM::RunThread(ThreadState& state, const std::string& functionName) {
    // Temporary execution tracer (WARSMASH_TRACE_CALLS=1).
    if (!state.started && std::getenv("WARSMASH_TRACE_CALLS")) {
        std::cerr << "[TRACE] " << functionName << std::endl;
    }
    ThreadRunResult result;
    if (state.completed) {
        result.completed = true;
        return result;
    }
    if (!state.started) {
        state.functionName = functionName;
        state.started = true;
        if (!PushThreadFrame(state, functionName)) {
            state.completed = true;
            result.completed = true;
            return result;
        }
    }

    while (!state.frames.empty()) {
        ThreadFrame& frame = state.frames.back();
        if (!frame.block || frame.statementIndex >= frame.block->children.size()) {
            if (frame.loop) {
                if (--frame.loopIterationsRemaining <= 0) {
                    state.frames.pop_back();
                } else {
                    frame.statementIndex = 0;
                }
            } else {
                state.frames.pop_back();
            }
            continue;
        }

        const AstNode* stmt = frame.block->children[frame.statementIndex].get();
        ++frame.statementIndex;

        if (stmt->type == AstNodeType::RETURN_STMT) {
            returnNode_ = stmt;
            if (!stmt->children.empty()) {
                SyncCallStackFromThread(state);
                (void)ExecuteNode(stmt->children[0].get());
                SyncThreadFromCallStack(state);
            }
            returnNode_ = nullptr;
            PopCurrentFunctionFrames(state);
            continue;
        }

        SyncCallStackFromThread(state);
        if (stmt->type == AstNodeType::EXITWHEN) {
            if (frame.loop && !stmt->children.empty() && JassValueToBool(ExecuteNode(stmt->children[0].get()))) {
                SyncThreadFromCallStack(state);
                state.frames.pop_back();
                continue;
            }
            SyncThreadFromCallStack(state);
            if (sleepRequested_) {
                result.sleeping = true;
                result.sleepSeconds = sleepSeconds_;
                sleepRequested_ = false;
                sleepSeconds_ = 0.0f;
                return result;
            }
            continue;
        }

        if (stmt->type == AstNodeType::IF_STMT) {
            const AstNode* selectedBlock = nullptr;
            if (stmt->children.size() >= 2 && JassValueToBool(ExecuteNode(stmt->children[0].get()))) {
                selectedBlock = stmt->children[1].get();
            } else {
                for (size_t i = 2; i < stmt->children.size(); ++i) {
                    const AstNode* branch = stmt->children[i].get();
                    if (branch->type == AstNodeType::IF_STMT) {
                        if (branch->children.size() >= 2 && JassValueToBool(ExecuteNode(branch->children[0].get()))) {
                            selectedBlock = branch->children[1].get();
                            break;
                        }
                    } else {
                        selectedBlock = branch;
                        break;
                    }
                }
            }
            SyncThreadFromCallStack(state);
            if (sleepRequested_) {
                result.sleeping = true;
                result.sleepSeconds = sleepSeconds_;
                sleepRequested_ = false;
                sleepSeconds_ = 0.0f;
                return result;
            }
            if (selectedBlock) {
                PushBlockFrame(state, selectedBlock, false);
            }
            continue;
        }

        if (stmt->type == AstNodeType::LOOP_STMT) {
            SyncThreadFromCallStack(state);
            if (!stmt->children.empty()) {
                PushBlockFrame(state, stmt->children[0].get(), true);
            }
            continue;
        }

        if (stmt->type == AstNodeType::BLOCK) {
            SyncThreadFromCallStack(state);
            PushBlockFrame(state, stmt, false);
            continue;
        }

        if (stmt->type == AstNodeType::CALL_STMT) {
            auto fit = functions_.find(stmt->text);
            if (fit != functions_.end()) {
                std::vector<JassValue> args;
                for (auto& child : stmt->children) {
                    args.push_back(ExecuteNode(child.get()));
                    if (sleepRequested_) break;
                }
                SyncThreadFromCallStack(state);
                if (sleepRequested_) {
                    result.sleeping = true;
                    result.sleepSeconds = sleepSeconds_;
                    sleepRequested_ = false;
                    sleepSeconds_ = 0.0f;
                    return result;
                }
                if (!PushThreadFrame(state, stmt->text, args)) {
                    errors_.push_back("Unknown function: " + stmt->text);
                }
                continue;
            }
        }

        (void)ExecuteNode(stmt);
        SyncThreadFromCallStack(state);
        if (sleepRequested_) {
            result.sleeping = true;
            result.sleepSeconds = sleepSeconds_;
            sleepRequested_ = false;
            sleepSeconds_ = 0.0f;
            return result;
        }
        if (returnNode_) {
            returnNode_ = nullptr;
            PopCurrentFunctionFrames(state);
        }
    }

    state.completed = true;
    result.completed = true;
    callStack_.clear();
    return result;
}

bool JassVM::HasFunction(const std::string& funcName) const {
    return functions_.find(funcName) != functions_.end();
}

void JassVM::RequestSleep(float seconds) {
    sleepRequested_ = true;
    sleepSeconds_ = std::max(0.0f, seconds);
}

JassValue JassVM::ExecuteFunction(const AstNode* func, const std::vector<JassValue>& args) {
    if (!func) return JassValue::Null();
    if (bytecodeEnabled_ && thistypeStack_.empty()) {
        auto bit = compiledFunctions_.find(func->text);
        if (bit != compiledFunctions_.end() && bit->second && bit->second.get()) {
            // Verify the compiled chunk corresponds to this exact AstNode (not a hooked shadow)
            auto fit = functions_.find(func->text);
            if (fit != functions_.end() && fit->second == func) {
                return ExecuteBytecode(*bit->second, args);
            }
        }
    }

    // Push new scope
    callStack_.push_back(Scope());
    if (func->localSlotCount > 0) {
        callStack_.back().fastLocals.resize((size_t)func->localSlotCount, JassValue::Null());
    }

    // Set parameter values
    for (size_t i = 0; i < func->parameters.size() && i < args.size(); i++) {
        callStack_.back().locals[func->parameters[i].first] = args[i];
        if (i < callStack_.back().fastLocals.size()) {
            callStack_.back().fastLocals[i] = args[i];
        }
    }

    // Isolate return signaling: a nested callee's `return` must not unwind
    // the caller (previously any value-returning call aborted its caller).
    const AstNode* savedReturn = returnNode_;
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
            if (returnNode_ || sleepRequested_) break;
        }
    }

    callStack_.pop_back();
    // Consume this call's return signal; restore the outer one (null in
    // every reachable path, kept for safety).
    returnNode_ = savedReturn;
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
            // Check O(1) indexed fastLocals first, then locals map, then globals
            if (!callStack_.empty()) {
                auto& scope = callStack_.back();
                if (node->localSlot >= 0 && (size_t)node->localSlot < scope.fastLocals.size()) {
                    return scope.fastLocals[(size_t)node->localSlot];
                }
                auto it = scope.locals.find(node->text);
                if (it != scope.locals.end()) return it->second;
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
                    if (sleepRequested_) break;
                }
                if (sleepRequested_) return JassValue::Null();
                return nat->second(args);
            }
            // User function
            auto fit = functions_.find(node->text);
            if (fit != functions_.end()) {
                std::vector<JassValue> args;
                for (auto& child : node->children) {
                    args.push_back(ExecuteNode(child.get()));
                    if (sleepRequested_) break;
                }
                if (sleepRequested_) return JassValue::Null();
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
            // Member target: set obj.member[.sub] = value
            if (!node->children.empty() && node->children[0]->type == AstNodeType::MEMBER_ACCESS) {
                const AstNode* target = node->children[0].get();
                JassValue val;
                if (node->children.size() > 1) {
                    val = ExecuteNode(node->children[1].get());
                }
                // Resolve the base: static context (thistype@ / type name)
                // or a live instance id.
                const AstNode* baseNode = target->children[0].get();
                std::string staticType;
                int instanceId = 0;
                bool haveInstance = false;
                if (baseNode->type == AstNodeType::VARIABLE) {
                    const std::string& base = baseNode->text;
                    if (base.compare(0, 9, "thistype@") == 0) {
                        instanceId = ThistypeInstance(base.substr(9));
                        if (instanceId != 0) {
                            haveInstance = true;
                        } else {
                            staticType = base.substr(9);
                        }
                    } else if (FindStructDef(base)) {
                        staticType = base;
                    } else {
                        JassValue baseVal = ExecuteNode(baseNode);
                        if (baseVal.type == JassValue::INTEGER && baseVal.intVal != 0) {
                            instanceId = baseVal.intVal;
                            haveInstance = true;
                        }
                    }
                } else {
                    JassValue baseVal = ExecuteNode(baseNode);
                    if (baseVal.type == JassValue::INTEGER && baseVal.intVal != 0) {
                        instanceId = baseVal.intVal;
                        haveInstance = true;
                    }
                }
                int index = 0;
                bool hasIndex = false;
                if (target->children.size() > 1) {
                    index = (int)ExecuteNode(target->children[1].get()).intVal;
                    hasIndex = true;
                }
                if (!staticType.empty()) {
                    if (hasIndex) {
                        structStatics_[staticType + "." + target->text +
                                       "[" + std::to_string(index) + "]"] = val;
                    } else {
                        structStatics_[staticType + "." + target->text] = val;
                    }
                    return val;
                }
                if (haveInstance) {
                    auto it = structInstances_.find(instanceId);
                    if (it != structInstances_.end()) {
                        // Static members stay in the static table even when
                        // reached through an instance; store under the
                        // defining struct's key.
                        const JassStructDef* idef = FindStructDef(it->second.type);
                        const JassStructMember* mdef =
                            idef ? FindStructMember(idef, target->text) : nullptr;
                        if (mdef && mdef->isStatic) {
                            for (const JassStructDef* d = idef; d != nullptr;) {
                                bool declares = false;
                                for (const auto& m : d->members) {
                                    if (m.name == target->text && m.isStatic) {
                                        declares = true;
                                        break;
                                    }
                                }
                                if (declares) {
                                    const std::string k = d->name + "." + target->text +
                                        (hasIndex ? "[" + std::to_string(index) + "]" : "");
                                    structStatics_[k] = val;
                                    break;
                                }
                                d = d->parent.empty() ? nullptr : FindStructDef(d->parent);
                            }
                            return val;
                        }
                        if (hasIndex) {
                            it->second.arrays[target->text][index] = val;
                        } else if (!mdef) {
                            const int forwarded = FindDelegateInstance(it->second.type, instanceId, target->text);
                            if (forwarded != 0) {
                                structInstances_[forwarded].scalars[target->text] = val;
                            } else {
                                it->second.scalars[target->text] = val;
                            }
                        } else {
                            it->second.scalars[target->text] = val;
                        }
                    }
                    return val;
                }
                return val;
            }
            // Local declaration
            if (!node->returnType.empty()) {
                // This is a local declaration
                if (node->isArray) {
                    // Array declaration: local integer array arr
                    // Create an empty array in locals
                    if (!callStack_.empty()) {
                        callStack_.back().locals[node->text] = JassValue::Null(); // placeholder for array
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
                    auto& scope = callStack_.back();
                    if (node->localSlot >= 0 && (size_t)node->localSlot < scope.fastLocals.size()) {
                        scope.fastLocals[(size_t)node->localSlot] = val;
                    }
                    scope.locals[node->text] = val;
                }
                return val;
            }

            // Set statement (assignment)
            // Array element assignment carries both target and value
            // (children = [ArrayAccess, value]). A lone ArrayAccess child
            // is the VALUE of `set scalar = arr[i]` (ref InitGlobals force
            // copies like `set udg_AAAPG_Arthas = bj_FORCE_PLAYER[1]`), so
            // it must fall through to simple assignment below.
            if (!node->children.empty() && node->children[0]->type == AstNodeType::ARRAY_ACCESS &&
                node->children.size() > 1) {
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
                if (std::getenv("WARSMASH_TRACE_ARRAY")) {
                    const bool toLocal = !callStack_.empty() &&
                        callStack_.back().locals.find(arrName) != callStack_.back().locals.end();
                    std::cerr << "[ARR-WRITE] " << key << (toLocal ? " LOCAL" : " GLOBAL") << std::endl;
                }
                if (!callStack_.empty() && callStack_.back().locals.find(arrName) != callStack_.back().locals.end()) {
                    callStack_.back().locals[key] = val;
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
            // Write to O(1) indexed local if bound, else local map, else global
            if (!callStack_.empty()) {
                auto& scope = callStack_.back();
                if (node->localSlot >= 0 && (size_t)node->localSlot < scope.fastLocals.size()) {
                    scope.fastLocals[(size_t)node->localSlot] = val;
                    scope.locals[node->text] = val;
                    return val;
                }
                if (scope.locals.find(node->text) != scope.locals.end()) {
                    scope.locals[node->text] = val;
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
                auto& locals = callStack_.back().locals;
                auto it = locals.find(key);
                if (it != locals.end()) {
                    if (std::getenv("WARSMASH_TRACE_ARRAY")) {
                        std::cerr << "[ARR-READ] " << key << " LOCAL" << std::endl;
                    }
                    return it->second;
                }
            }
            auto it = globals_.find(key);
            if (std::getenv("WARSMASH_TRACE_ARRAY")) {
                std::cerr << "[ARR-READ] " << key << (it != globals_.end() ? " GLOBAL-HIT" : " MISS") << std::endl;
            }
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
                    if (sleepRequested_) break;
                }
                if (!sleepRequested_) nat->second(args);
            } else {
                auto fit = functions_.find(node->text);
                if (fit != functions_.end()) {
                    std::vector<JassValue> args;
                    for (auto& child : node->children) {
                        args.push_back(ExecuteNode(child.get()));
                        if (sleepRequested_) break;
                    }
                    if (!sleepRequested_) ExecuteFunction(fit->second, args);
                } else {
                    errors_.push_back("Unknown function: " + node->text);
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
                    if (returnNode_ || sleepRequested_) return r;
                }
            } else {
                // Check elseif/else
                for (size_t i = 2; i < node->children.size(); i++) {
                    if (node->children[i]->type == AstNodeType::IF_STMT) {
                        auto elseifCond = ExecuteNode(node->children[i]->children[0].get());
                        if (elseifCond.boolVal) {
                            for (auto& stmt : node->children[i]->children[1]->children) {
                                auto r = ExecuteNode(stmt.get());
                                if (returnNode_ || sleepRequested_) return r;
                            }
                            break;
                        }
                    } else {
                        // else block
                        for (auto& stmt : node->children[i]->children) {
                            auto r = ExecuteNode(stmt.get());
                            if (returnNode_ || sleepRequested_) return r;
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
                    if (returnNode_ || sleepRequested_) return r;
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
                if (returnNode_ || sleepRequested_) return r;
            }
            return JassValue::Null();
        }

        case AstNodeType::MEMBER_ACCESS: {
            if (node->children.empty()) return JassValue::Null();
            const AstNode* baseNode = node->children[0].get();
            std::string staticType;
            int instanceId = 0;
            if (baseNode->type == AstNodeType::VARIABLE) {
                const std::string& base = baseNode->text;
                if (base.compare(0, 9, "thistype@") == 0) {
                    instanceId = ThistypeInstance(base.substr(9));
                    if (instanceId == 0) staticType = base.substr(9);
                } else if (FindStructDef(base)) {
                    staticType = base;
                } else {
                    JassValue baseVal = ExecuteNode(baseNode);
                    if (baseVal.type == JassValue::INTEGER) instanceId = baseVal.intVal;
                }
            } else {
                JassValue baseVal = ExecuteNode(baseNode);
                if (baseVal.type == JassValue::INTEGER) instanceId = baseVal.intVal;
            }
            bool hasIndex = false;
            int index = 0;
            if (node->children.size() > 1) {
                index = (int)ExecuteNode(node->children[1].get()).intVal;
                hasIndex = true;
            }
            if (!staticType.empty()) {
                if (hasIndex) {
                    auto it = structStatics_.find(staticType + "." + node->text +
                                                  "[" + std::to_string(index) + "]");
                    if (it != structStatics_.end()) return it->second;
                    return JassValue::Null();
                }
                auto it = structStatics_.find(staticType + "." + node->text);
                if (it != structStatics_.end()) return it->second;
                return JassValue::Null();
            }
            if (instanceId == 0) return JassValue::Null();
            auto it = structInstances_.find(instanceId);
            if (it == structInstances_.end()) return JassValue::Null();
            // Static members stay in the static table even when reached
            // through an instance (ref `obj.staticMember` / thistype).
            if (const JassStructDef* idef = FindStructDef(it->second.type)) {
                if (const JassStructMember* mdef = FindStructMember(idef, node->text)) {
                    if (mdef->isStatic) {
                        // The defining struct may be an ancestor: walk up.
                        for (const JassStructDef* d = idef; d != nullptr;) {
                            const std::string k = d->name + "." + node->text +
                                (hasIndex ? "[" + std::to_string(index) + "]" : "");
                            auto sit = structStatics_.find(k);
                            if (sit != structStatics_.end()) return sit->second;
                            d = d->parent.empty() ? nullptr : FindStructDef(d->parent);
                        }
                        return JassValue::Null();
                    }
                }
            }
            if (it == structInstances_.end()) return JassValue::Null();
            if (hasIndex) {
                auto ait = it->second.arrays.find(node->text);
                if (ait == it->second.arrays.end()) return JassValue::Null();
                auto vit = ait->second.find(index);
                if (vit == ait->second.end()) return JassValue::Null();
                return vit->second;
            }
            auto vit = it->second.scalars.find(node->text);
            if (vit == it->second.scalars.end()) {
                const int forwarded = FindDelegateInstance(it->second.type, instanceId, node->text);
                if (forwarded != 0) {
                    auto fit = structInstances_.find(forwarded);
                    if (fit != structInstances_.end()) {
                        auto fvit = fit->second.scalars.find(node->text);
                        if (fvit != fit->second.scalars.end()) return fvit->second;
                    }
                }
                return JassValue::Null();
            }
            return vit->second;
        }

        case AstNodeType::METHOD_CALL: {
            if (node->children.empty()) return JassValue::Null();
            const AstNode* baseNode = node->children[0].get();
            std::vector<JassValue> args;
            for (size_t i = 1; i < node->children.size(); ++i) {
                args.push_back(ExecuteNode(node->children[i].get()));
                if (sleepRequested_) return JassValue::Null();
            }
            std::string staticType;
            int instanceId = 0;
            if (baseNode->type == AstNodeType::VARIABLE) {
                const std::string& base = baseNode->text;
                if (base.compare(0, 9, "thistype@") == 0) {
                    // Enclosing instance when related, else static context.
                    instanceId = ThistypeInstance(base.substr(9));
                    if (instanceId == 0) staticType = base.substr(9);
                } else if (FindStructDef(base)) {
                    staticType = base;
                } else {
                    JassValue baseVal = ExecuteNode(baseNode);
                    if (baseVal.type == JassValue::INTEGER) instanceId = baseVal.intVal;
                }
            } else {
                JassValue baseVal = ExecuteNode(baseNode);
                if (baseVal.type == JassValue::INTEGER) instanceId = baseVal.intVal;
            }
            const std::string& method = node->text;
            if (!staticType.empty()) {
                if (method == "allocate") {
                    return JassValue::Int(AllocateStruct(staticType));
                }
                const JassStructMethod* m = FindStructMethod(staticType, method);
                if (m && m->func) {
                    if (!m->isStatic) {
                        errors_.push_back("Non-static method '" + method + "' called statically");
                        return JassValue::Null();
                    }
                    thistypeStack_.emplace_back(staticType, 0);
                    JassValue result = ExecuteFunction(m->func, args);
                    thistypeStack_.pop_back();
                    return result;
                }
                errors_.push_back("Unknown static method: " + staticType + "." + method);
                return JassValue::Null();
            }
            if (instanceId == 0) {
                errors_.push_back("Method '" + method + "' called on null");
                return JassValue::Null();
            }
            auto iit = structInstances_.find(instanceId);
            if (iit == structInstances_.end()) {
                errors_.push_back("Method '" + method + "' called on destroyed instance");
                return JassValue::Null();
            }
            if (method == "deallocate" || method == "destroy") {
                structInstances_.erase(iit);
                return JassValue::Null();
            }
            const JassStructMethod* m = FindStructMethod(iit->second.type, method);
            int callId = instanceId;
            std::string callType = iit->second.type;
            if (!(m && m->func)) {
                const int forwarded = FindDelegateInstance(iit->second.type, instanceId, method);
                if (forwarded != 0) {
                    auto fit = structInstances_.find(forwarded);
                    if (fit != structInstances_.end()) {
                        m = FindStructMethod(fit->second.type, method);
                        callId = forwarded;
                        callType = fit->second.type;
                    }
                }
            }
            if (m && m->func) {
                thistypeStack_.emplace_back(callType, callId);
                JassValue result;
                if (m->isStatic) {
                    result = ExecuteFunction(m->func, args);
                } else {
                    std::vector<JassValue> withThis;
                    withThis.reserve(args.size() + 1);
                    withThis.push_back(JassValue::Int(callId));
                    withThis.insert(withThis.end(), args.begin(), args.end());
                    result = ExecuteFunction(m->func, withThis);
                }
                thistypeStack_.pop_back();
                return result;
            }
            errors_.push_back("Unknown method: " + method);
            return JassValue::Null();
        }

        default:
            return JassValue::Null();
    }
}


void JassVM::CompileAndBindFunctions() {
    auto isStructPred = [this](const std::string& name) -> bool {
        return FindStructDef(name) != nullptr;
    };
    for (auto& [funcName, funcNode] : functions_) {
        if (!funcNode) continue;
        JassBytecodeCompiler::BindFunctionLocalSlots(const_cast<AstNode*>(funcNode), isStructPred);
        auto chunk = std::make_unique<JassBytecodeChunk>();
        if (JassBytecodeCompiler::CompileFunction(funcNode, isStructPred, *chunk)) {
            compiledFunctions_[funcName] = std::move(chunk);
        } else {
            compiledFunctions_.erase(funcName);
        }
    }
}

const JassBytecodeChunk* JassVM::GetCompiledBytecode(const std::string& funcName) const {
    auto it = compiledFunctions_.find(funcName);
    return (it != compiledFunctions_.end()) ? it->second.get() : nullptr;
}

JassValue JassVM::ExecuteBytecode(const JassBytecodeChunk& chunk, const std::vector<JassValue>& args) {
    ++bytecodeExecutedCount_;

    std::vector<JassValue> localSlots = chunk.localDefaults;
    if ((int)localSlots.size() < chunk.numLocalSlots) {
        localSlots.resize((size_t)chunk.numLocalSlots, JassValue::Null());
    }
    for (size_t i = 0; i < (size_t)chunk.numParams && i < args.size() && i < localSlots.size(); ++i) {
        localSlots[i] = args[i];
    }

    std::vector<JassValue> stack;
    stack.reserve(32);

    const auto& code = chunk.code;
    const size_t codeLen = code.size();
    size_t ip = 0;
    int loopGuard = 2000000;

    while (ip < codeLen) {
        const JassInstruction& ins = code[ip++];
        switch (ins.op) {
            case JassOpCode::OP_CONST_INT:
                stack.push_back(JassValue::Int(ins.intOperand));
                break;
            case JassOpCode::OP_CONST_REAL:
                stack.push_back(JassValue::Real(ins.realOperand));
                break;
            case JassOpCode::OP_CONST_BOOL:
                stack.push_back(JassValue::Bool(ins.intOperand != 0));
                break;
            case JassOpCode::OP_CONST_STR:
                stack.push_back(JassValue::Str(chunk.stringPool[ins.strIndex]));
                break;
            case JassOpCode::OP_CONST_NULL:
                stack.push_back(JassValue::Null());
                break;

            case JassOpCode::OP_GET_LOCAL_SLOT:
                stack.push_back(localSlots[(size_t)ins.intOperand]);
                break;
            case JassOpCode::OP_SET_LOCAL_SLOT: {
                JassValue val = stack.empty() ? JassValue::Null() : std::move(stack.back());
                if (!stack.empty()) stack.pop_back();
                localSlots[(size_t)ins.intOperand] = std::move(val);
                break;
            }

            case JassOpCode::OP_GET_GLOBAL: {
                const std::string& name = chunk.stringPool[ins.strIndex];
                auto it = globals_.find(name);
                stack.push_back(it != globals_.end() ? it->second : JassValue::Null());
                break;
            }
            case JassOpCode::OP_SET_GLOBAL: {
                JassValue val = stack.empty() ? JassValue::Null() : std::move(stack.back());
                if (!stack.empty()) stack.pop_back();
                const std::string& name = chunk.stringPool[ins.strIndex];
                globals_[name] = std::move(val);
                break;
            }

            case JassOpCode::OP_GET_GLOBAL_ARRAY: {
                int idx = 0;
                if (!stack.empty()) {
                    idx = stack.back().intVal;
                    stack.pop_back();
                }
                const std::string key = chunk.stringPool[ins.strIndex] + "[" + std::to_string(idx) + "]";
                auto it = globals_.find(key);
                stack.push_back(it != globals_.end() ? it->second : JassValue::Null());
                break;
            }
            case JassOpCode::OP_SET_GLOBAL_ARRAY: {
                JassValue val = stack.empty() ? JassValue::Null() : std::move(stack.back());
                if (!stack.empty()) stack.pop_back();
                int idx = 0;
                if (!stack.empty()) {
                    idx = stack.back().intVal;
                    stack.pop_back();
                }
                const std::string key = chunk.stringPool[ins.strIndex] + "[" + std::to_string(idx) + "]";
                globals_[key] = std::move(val);
                break;
            }

            case JassOpCode::OP_NEG: {
                if (stack.empty()) break;
                JassValue& v = stack.back();
                if (v.type == JassValue::REAL) v = JassValue::Real(-v.realVal);
                else v = JassValue::Int(-v.intVal);
                break;
            }
            case JassOpCode::OP_NOT: {
                if (stack.empty()) break;
                JassValue& v = stack.back();
                v = JassValue::Bool(!v.boolVal);
                break;
            }

            case JassOpCode::OP_ADD:
            case JassOpCode::OP_SUB:
            case JassOpCode::OP_MUL:
            case JassOpCode::OP_DIV:
            case JassOpCode::OP_EQ:
            case JassOpCode::OP_NEQ:
            case JassOpCode::OP_LT:
            case JassOpCode::OP_GT:
            case JassOpCode::OP_LE:
            case JassOpCode::OP_GE: {
                if (stack.size() < 2) break;
                JassValue right = std::move(stack.back()); stack.pop_back();
                JassValue left = std::move(stack.back()); stack.pop_back();
                const bool useReal = (left.type == JassValue::REAL || right.type == JassValue::REAL);
                switch (ins.op) {
                    case JassOpCode::OP_ADD:
                        if (left.type == JassValue::STRING || right.type == JassValue::STRING)
                            stack.push_back(JassValue::Str(left.strVal + right.strVal));
                        else if (useReal)
                            stack.push_back(JassValue::Real(left.realVal + right.realVal));
                        else
                            stack.push_back(JassValue::Int(left.intVal + right.intVal));
                        break;
                    case JassOpCode::OP_SUB:
                        stack.push_back(useReal ? JassValue::Real(left.realVal - right.realVal)
                                                : JassValue::Int(left.intVal - right.intVal));
                        break;
                    case JassOpCode::OP_MUL:
                        stack.push_back(useReal ? JassValue::Real(left.realVal * right.realVal)
                                                : JassValue::Int(left.intVal * right.intVal));
                        break;
                    case JassOpCode::OP_DIV:
                        if (useReal) {
                            stack.push_back(JassValue::Real(right.realVal == 0.0f ? 0.0f : left.realVal / right.realVal));
                        } else {
                            stack.push_back(JassValue::Int(right.intVal == 0 ? 0 : left.intVal / right.intVal));
                        }
                        break;
                    case JassOpCode::OP_EQ:
                        if (useReal) stack.push_back(JassValue::Bool(left.realVal == right.realVal));
                        else if (left.type == JassValue::STRING && right.type == JassValue::STRING)
                            stack.push_back(JassValue::Bool(left.strVal == right.strVal));
                        else stack.push_back(JassValue::Bool(left.intVal == right.intVal));
                        break;
                    case JassOpCode::OP_NEQ:
                        if (useReal) stack.push_back(JassValue::Bool(left.realVal != right.realVal));
                        else if (left.type == JassValue::STRING && right.type == JassValue::STRING)
                            stack.push_back(JassValue::Bool(left.strVal != right.strVal));
                        else stack.push_back(JassValue::Bool(left.intVal != right.intVal));
                        break;
                    case JassOpCode::OP_LT:
                        stack.push_back(JassValue::Bool(useReal ? (left.realVal < right.realVal) : (left.intVal < right.intVal)));
                        break;
                    case JassOpCode::OP_GT:
                        stack.push_back(JassValue::Bool(useReal ? (left.realVal > right.realVal) : (left.intVal > right.intVal)));
                        break;
                    case JassOpCode::OP_LE:
                        stack.push_back(JassValue::Bool(useReal ? (left.realVal <= right.realVal) : (left.intVal <= right.intVal)));
                        break;
                    case JassOpCode::OP_GE:
                        stack.push_back(JassValue::Bool(useReal ? (left.realVal >= right.realVal) : (left.intVal >= right.intVal)));
                        break;
                    default:
                        break;
                }
                break;
            }

            case JassOpCode::OP_JMP:
                if (ins.intOperand < (int32_t)ip && --loopGuard <= 0) {
                    return JassValue::Null();
                }
                ip = (size_t)ins.intOperand;
                break;

            case JassOpCode::OP_JMP_IF_FALSE: {
                bool cond = false;
                if (!stack.empty()) {
                    cond = JassValueToBool(stack.back());
                    stack.pop_back();
                }
                if (!cond) {
                    ip = (size_t)ins.intOperand;
                }
                break;
            }

            case JassOpCode::OP_JMP_IF_TRUE: {
                bool cond = false;
                if (!stack.empty()) {
                    cond = JassValueToBool(stack.back());
                    stack.pop_back();
                }
                if (cond) {
                    ip = (size_t)ins.intOperand;
                }
                break;
            }

            case JassOpCode::OP_CALL: {
                const int argCount = ins.intOperand;
                std::vector<JassValue> callArgs((size_t)argCount);
                for (int i = argCount - 1; i >= 0; --i) {
                    if (!stack.empty()) {
                        callArgs[(size_t)i] = std::move(stack.back());
                        stack.pop_back();
                    }
                }
                const std::string& calleeName = chunk.stringPool[ins.strIndex];
                JassValue ret = JassValue::Null();
                auto natIt = natives_.find(calleeName);
                if (natIt != natives_.end()) {
                    ret = natIt->second(callArgs);
                } else {
                    auto fIt = functions_.find(calleeName);
                    if (fIt != functions_.end()) {
                        ret = ExecuteFunction(fIt->second, callArgs);
                    } else {
                        errors_.push_back("Unknown function: " + calleeName);
                    }
                }
                if (sleepRequested_) return JassValue::Null();
                stack.push_back(std::move(ret));
                break;
            }

            case JassOpCode::OP_POP:
                if (!stack.empty()) stack.pop_back();
                break;

            case JassOpCode::OP_RETURN:
                return JassValue::Null();

            case JassOpCode::OP_RETURN_VAL: {
                if (stack.empty()) return JassValue::Null();
                JassValue ret = std::move(stack.back());
                stack.pop_back();
                return ret;
            }
        }
    }
    return JassValue::Null();
}
