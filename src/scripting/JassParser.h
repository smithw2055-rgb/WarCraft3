#pragma once
#include "JassLexer.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

// AST node types
enum class AstNodeType {
    PROGRAM, GLOBAL_DECL, FUNCTION_DECL, NATIVE_DECL, TYPE_DECL,
    BLOCK, IF_STMT, LOOP_STMT, EXITWHEN, SET_STMT, CALL_STMT, RETURN_STMT,
    BINARY_OP, UNARY_OP, FUNC_CALL, ARRAY_ACCESS,
    LITERAL_INT, LITERAL_REAL, LITERAL_BOOL, LITERAL_STRING, LITERAL_NULL,
    LITERAL_RAWCODE, LITERAL_FUNCTION_REF,
    VARIABLE, PARAMETER, TYPE,
    // vJASS nodes
    STRUCT_DECL,   // text=name, extendsType=parent, children=members+methods
    MODULE_DECL,   // like STRUCT_DECL; expanded away, never reaches the VM
    MEMBER_ACCESS, // text=member, children=[baseExpr, optional indexExpr]
    METHOD_CALL,   // text=method, children=[baseExpr, args...]
};

struct AstNode {
    AstNodeType type;
    std::string text;
    double numberValue = 0;
    std::vector<std::unique_ptr<AstNode>> children;
    std::string returnType;
    std::vector<std::pair<std::string, std::string>> parameters; // name, type
    bool isConstant = false;
    bool isArray = false;
    bool isStatic = false; // vJASS static members/methods
    bool isInterface = false;
    // delegate Type member: unknown calls forward to that member.
    std::vector<std::pair<std::string, std::string>> delegates;
    std::string extendsType; // for type declarations
    int line = 0;

    AstNode() = default;
    AstNode(AstNodeType t, const std::string& s = "") : type(t), text(s) {}
};

class JassParser {
public:
    JassParser(const std::vector<JassToken>& tokens);

    std::unique_ptr<AstNode> Parse();
    const std::vector<std::string>& GetErrors() const { return errors_; }
    // Scope/library initializers, in source order (run before main).
    const std::vector<std::string>& GetInitializers() const { return initializers_; }
    // Library dependency records for initializer topological order (ref
    // `library ... needs ...`: dependencies run first).
    struct LibraryInfo {
        std::string name;
        std::string initializer;
        std::vector<std::string> needs;
    };
    const std::vector<LibraryInfo>& GetLibraries() const { return libraries_; }
    // vJASS `hook ORIG REPL` pairs, scope-resolved (ref function hooking).
    const std::vector<std::pair<std::string, std::string>>& GetHooks() const { return hooks_; }

private:
    // vJASS contextual keywords (kept as identifiers by the lexer to avoid
    // breaking retail scripts that use words like `this` as identifiers).
    static bool IsVjassDeclOpener(const std::string& text) {
        return text == "struct" || text == "scope" || text == "library" ||
               text == "module" || text == "interface";
    }
    bool AtDeclOpener(const std::string& word) const {
        return Peek().type == JassTokenType::IDENTIFIER && Peek().text == word;
    }
    // Rewrite a bare `thistype` use to thistype@Struct while inside a
    // struct method body (carries the struct for static resolution).
    std::string RewriteThistype(const std::string& name) const;
    // Parse one top-level-or-scope-body declaration, honoring an optional
    // private/public prefix. Returns nullptr for wrappers handled inline.
    std::unique_ptr<AstNode> ParseDecl(bool inScope);
    std::unique_ptr<AstNode> ParseStructDecl();
    std::unique_ptr<AstNode> ParseInterfaceDecl();
    // Parses `module Name ... endmodule` into a MODULE_DECL node (kept
    // aside for implement-expansion, never emitted to the program).
    std::unique_ptr<AstNode> ParseModuleDecl();
    void ParseStructOrModuleBody(AstNode* node, bool isModule);
    void ParseScopeOrLibrary(AstNode* program, bool isLibrary);
    std::string ResolveMethodRef(const std::string& base, const std::string& member);
    // Parse one declaration (vJASS or vanilla). Consumes an optional
    // private/public prefix; returns the node plus its raw name and
    // whether it was declared private (for scope rewriting).
    std::unique_ptr<AstNode> ParseDecl(bool defaultPrivate, std::string& outRawName, bool& outPrivate);
    std::unique_ptr<AstNode> ParseVanillaDecl();
    // Apply scope privacy after a body is fully parsed: rename private
    // declarations to Prefix__Name and rewrite all references (supports
    // forward references). Collects initializer names into initializers_.
    void RewriteScopePrivacy(AstNode* body, const std::string& prefix,
                             const std::unordered_set<std::string>& privates,
                             const std::vector<std::string>& rawInitializers);
    const JassToken& Peek() const;
    const JassToken& PeekAt(size_t offset) const;
    const JassToken& Advance();
    const JassToken& Expect(JassTokenType type);
    bool Match(JassTokenType type);
    void SkipNewlines();
    std::string ParseType();

    std::unique_ptr<AstNode> ParseProgram();
    std::unique_ptr<AstNode> ParseGlobalBlock();
    std::unique_ptr<AstNode> ParseFunctionDecl(bool isConstant = false);
    std::unique_ptr<AstNode> ParseNativeDecl(bool isConstant = false);
    std::unique_ptr<AstNode> ParseBlock();
    std::unique_ptr<AstNode> ParseStatement();
    // vJASS `static if` (ref compile-time conditionals): true when the next
    // tokens are the contextual `static` identifier followed by `if`.
    bool AtStaticIf() const {
        return Peek().type == JassTokenType::IDENTIFIER && Peek().text == "static" &&
               PeekAt(1).type == JassTokenType::KW_IF;
    }
    // Compile-time condition value: constant literals, LIBRARY_* presence,
    // arithmetic/comparison/logic over constants. False + error otherwise.
    bool EvalStaticCondition(const AstNode* expr, bool& out);
    // Advance past an untaken branch body (stops at elseif/else/endif at
    // depth 1, or EOF). Balances nested ifs so vanilla blocks inside pass.
    void SkipStaticBranch();
    // Statement context: BLOCK holding the taken branch's statements.
    std::unique_ptr<AstNode> ParseStaticIfStmt();
    // Declaration context: appends taken declarations (with raw names and
    // privacy for scope rewriting).
    struct RawDecl {
        std::unique_ptr<AstNode> node;
        std::string rawName;
        bool isPrivate = false;
    };
    void ParseStaticIfDecls(std::vector<RawDecl>& out, bool defaultPrivate,
                            const std::string& scopePrefix = "");
    std::unique_ptr<AstNode> ParseExpression();
    std::unique_ptr<AstNode> ParseOrExpr();
    std::unique_ptr<AstNode> ParseAndExpr();
    std::unique_ptr<AstNode> ParseEqualityExpr();
    std::unique_ptr<AstNode> ParseComparisonExpr();
    std::unique_ptr<AstNode> ParseAddSubExpr();
    std::unique_ptr<AstNode> ParseMulDivExpr();
    std::unique_ptr<AstNode> ParseUnaryExpr();
    std::unique_ptr<AstNode> ParsePrimary();

    const std::vector<JassToken>& tokens_;
    size_t pos_ = 0;
    std::vector<std::string> errors_;
    std::vector<std::string> initializers_;
    // Mangled struct name while parsing its body (for thistype@ rewrite).
    std::string currentStruct_;
    // Pending `implement` records: (struct, module, optional, scope).
    struct ImplementRecord {
        std::string structName;
        std::string moduleName;
        bool optional = false;
        std::string scopePrefix; // enclosing scope, "" at top level
        bool intoModule = false;
    };
    std::vector<ImplementRecord> pendingImplements_;
    std::vector<std::unique_ptr<AstNode>> moduleDefs_; // kept alive for expansion
    std::vector<std::unique_ptr<AstNode>> hoisted_; // nested module/interface lifted to the program
    // vJASS `LIBRARY_Name` presence flags, defined as each library closes
    // (ref static library constants for `static if` conditions).
    std::unordered_map<std::string, bool> staticDefines_;
    std::vector<LibraryInfo> libraries_;
    std::vector<std::pair<std::string, std::string>> hooks_;
    struct HookRecord {
        std::string orig;
        std::string repl;
        std::string scopePrefix;
    };
    std::vector<HookRecord> hookRecords_;
    std::unordered_map<std::string, std::unordered_set<std::string>> scopePrivates_;
    // True for a top-level/scope-level `hook ORIG REPL` directive (two
    // bare identifiers on one line; calls like hook(a) don't match).
    bool AtHookDirective() const {
        return Peek().type == JassTokenType::IDENTIFIER && Peek().text == "hook" &&
               PeekAt(1).type == JassTokenType::IDENTIFIER &&
               PeekAt(2).type == JassTokenType::IDENTIFIER &&
               (PeekAt(3).type == JassTokenType::NEWLINE ||
                PeekAt(3).type == JassTokenType::EOF_TOKEN);
    }
    // Consumes the directive, tagging the enclosing scope for later
    // private-name resolution (empty prefix at top level).
    void ParseHookDirective(const std::string& scopePrefix);
    void ResolveHooks();
    // Expands pending implements into struct decls and drops MODULE_DECL
    // nodes. Runs once at the end of ParseProgram.
    void ExpandImplements(AstNode* program);
    static std::unique_ptr<AstNode> CloneNode(const AstNode* node);
};
