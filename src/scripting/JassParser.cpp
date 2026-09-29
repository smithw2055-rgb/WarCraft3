#include "JassParser.h"
#include <iostream>
#include <functional>

JassParser::JassParser(const std::vector<JassToken>& tokens) : tokens_(tokens) {}

const JassToken& JassParser::Peek() const {
    return tokens_[pos_];
}

const JassToken& JassParser::PeekAt(size_t offset) const {
    static const JassToken kEof{JassTokenType::EOF_TOKEN, "", 0, 0, 0};
    return (pos_ + offset < tokens_.size()) ? tokens_[pos_ + offset] : kEof;
}

const JassToken& JassParser::Advance() {
    return tokens_[pos_++];
}

const JassToken& JassParser::Expect(JassTokenType type) {
    if (Peek().type != type) {
        errors_.push_back("Expected token type " + std::to_string((int)type) + " but got '" +
                          Peek().text + "' at line " + std::to_string(Peek().line));
    }
    return Advance();
}

bool JassParser::Match(JassTokenType type) {
    if (Peek().type == type) { Advance(); return true; }
    return false;
}

void JassParser::SkipNewlines() {
    while (Peek().type == JassTokenType::NEWLINE) Advance();
}

std::string JassParser::ParseType() {
    auto& tok = Peek();
    switch (tok.type) {
        case JassTokenType::TYPE_INTEGER: Advance(); return "integer";
        case JassTokenType::TYPE_REAL: Advance(); return "real";
        case JassTokenType::TYPE_BOOLEAN: Advance(); return "boolean";
        case JassTokenType::TYPE_STRING: Advance(); return "string";
        case JassTokenType::TYPE_HANDLE: Advance(); return "handle";
        case JassTokenType::TYPE_CODE: Advance(); return "code";
        case JassTokenType::KW_NOTHING: Advance(); return "nothing";
        case JassTokenType::IDENTIFIER: Advance(); return tok.text; // handle subtype
        default: return "nothing";
    }
}

std::unique_ptr<AstNode> JassParser::Parse() {
    return ParseProgram();
}

std::string JassParser::RewriteThistype(const std::string& name) const {
    if (name == "thistype" && !currentStruct_.empty()) {
        return "thistype@" + currentStruct_;
    }
    return name;
}

std::string JassParser::ResolveMethodRef(const std::string& base, const std::string& member) {
    if (base == "this" || base == "thistype") {
        if (currentStruct_.empty()) {
            errors_.push_back("'this' used outside a struct method");
            return member;
        }
        return currentStruct_ + "." + member;
    }
    return base + "." + member;
}

std::unique_ptr<AstNode> JassParser::ParseProgram() {
    auto program = std::make_unique<AstNode>(AstNodeType::PROGRAM);
    SkipNewlines();

    while (Peek().type != JassTokenType::EOF_TOKEN) {
        // Top-level scope/library wrappers flatten into the program.
        // An optional private/public prefix is accepted and ignored here
        // (top-level privacy has no effect, documented).
        size_t probe = pos_;
        if (tokens_[probe].type == JassTokenType::IDENTIFIER &&
            (tokens_[probe].text == "private" || tokens_[probe].text == "public")) {
            probe++;
        }
        if (probe < tokens_.size() && tokens_[probe].type == JassTokenType::IDENTIFIER &&
            (tokens_[probe].text == "scope" || tokens_[probe].text == "library")) {
            if (probe != pos_) Advance(); // drop the prefix
            const bool isLibrary = Peek().text == "library";
            ParseScopeOrLibrary(program.get(), isLibrary);
            SkipNewlines();
            continue;
        }
        std::string rawName;
        bool isPrivate = false;
        auto decl = ParseDecl(false, rawName, isPrivate);
        if (decl) program->children.push_back(std::move(decl));
        SkipNewlines();
    }
    ExpandImplements(program.get());
    return program;
}

// One declaration: optional private/public prefix, then a vJASS form
// (struct/scope/library, contextual identifiers) or one vanilla form.
// Returns nullptr when the input was consumed without producing a node.
std::unique_ptr<AstNode> JassParser::ParseDecl(bool defaultPrivate, std::string& outRawName, bool& outPrivate) {
    outRawName.clear();
    bool isPrivate = defaultPrivate;
    if (Peek().type == JassTokenType::IDENTIFIER &&
        (Peek().text == "private" || Peek().text == "public")) {
        isPrivate = (Advance().text == "private");
        SkipNewlines();
    }
    outPrivate = isPrivate;

    if (Peek().type == JassTokenType::IDENTIFIER) {
        const std::string& word = Peek().text;
        if (word == "struct") {
            auto node = ParseStructDecl();
            if (node) outRawName = node->text;
            return node;
        }
        if (word == "scope" || word == "library") {
            errors_.push_back("Nested scopes/libraries are not supported at line " +
                              std::to_string(Peek().line));
            Advance();
            SkipNewlines();
            return nullptr;
        }
        if (word == "module") {
            auto node = ParseModuleDecl();
            if (node) outRawName = node->text;
            return node;
        }
        if (word == "interface") {
            auto node = ParseInterfaceDecl();
            if (node) outRawName = node->text;
            return node;
        }
        if (word == "delegate") {
            errors_.push_back("delegate is only valid inside a struct at line " +
                              std::to_string(Peek().line));
            while (Peek().type != JassTokenType::NEWLINE && Peek().type != JassTokenType::EOF_TOKEN) Advance();
            return nullptr;
        }
    }
    auto node = ParseVanillaDecl();
    if (node) {
        if (node->type == AstNodeType::FUNCTION_DECL ||
            node->type == AstNodeType::NATIVE_DECL ||
            node->type == AstNodeType::GLOBAL_DECL ||
            node->type == AstNodeType::TYPE_DECL) {
            outRawName = node->text;
        }
    }
    return node;
}

std::unique_ptr<AstNode> JassParser::ParseVanillaDecl() {
    // type declaration: type X extends Y
    if (Peek().type == JassTokenType::KW_TYPE) {
        Advance();
        auto decl = std::make_unique<AstNode>(AstNodeType::TYPE_DECL);
        decl->text = Expect(JassTokenType::IDENTIFIER).text;
        if (Peek().type == JassTokenType::KW_EXTENDS || Peek().text == "extends") {
            Advance();
            decl->extendsType = ParseType();
        }
        return decl;
    }
    if (Match(JassTokenType::KW_GLOBALS)) {
        return ParseGlobalBlock();
    }
    if (Match(JassTokenType::KW_NATIVE)) {
        return ParseNativeDecl();
    }
    if (Peek().type == JassTokenType::KW_CONSTANT) {
        Advance();
        if (Match(JassTokenType::KW_NATIVE)) {
            return ParseNativeDecl(true);
        }
        if (Peek().type == JassTokenType::KW_FUNCTION) {
            return ParseFunctionDecl(true);
        }
        auto decl = std::make_unique<AstNode>(AstNodeType::GLOBAL_DECL);
        decl->isConstant = true;
        decl->returnType = ParseType();
        if (decl->returnType == "nothing") {
            Advance();
            return nullptr;
        }
        if (Match(JassTokenType::KW_ARRAY)) {
            decl->isArray = true;
        }
        if (Peek().type == JassTokenType::IDENTIFIER) {
            decl->text = Advance().text;
            if (Match(JassTokenType::KW_ARRAY)) {
                decl->isArray = true;
            }
            if (Match(JassTokenType::OP_ASSIGN)) {
                decl->children.push_back(ParseExpression());
            }
        }
        return decl;
    }
    if (Peek().type == JassTokenType::KW_FUNCTION) {
        return ParseFunctionDecl();
    }
    if (Peek().type == JassTokenType::KW_DEBUG) {
        Advance(); // skip debug
        // Skip the next statement
        while (Peek().type != JassTokenType::NEWLINE && Peek().type != JassTokenType::EOF_TOKEN) Advance();
        return nullptr;
    }
    // Could be a global variable declaration or constant global
    auto decl = std::make_unique<AstNode>(AstNodeType::GLOBAL_DECL);
    decl->isConstant = Match(JassTokenType::KW_CONSTANT);
    decl->returnType = ParseType();
    if (decl->returnType == "nothing") {
        // Unknown token, skip
        Advance();
        return nullptr;
    }
    if (Match(JassTokenType::KW_ARRAY)) {
        decl->isArray = true;
    }
    if (Peek().type == JassTokenType::IDENTIFIER) {
        decl->text = Advance().text;
        // Check for array keyword
        if (Match(JassTokenType::KW_ARRAY)) {
            decl->isArray = true;
        }
        if (Match(JassTokenType::OP_ASSIGN)) {
            decl->children.push_back(ParseExpression());
        }
    }
    return decl;
}

std::unique_ptr<AstNode> JassParser::ParseGlobalBlock() {
    auto block = std::make_unique<AstNode>(AstNodeType::BLOCK);
    SkipNewlines();

    while (Peek().type != JassTokenType::KW_ENDGLOBALS && Peek().type != JassTokenType::EOF_TOKEN) {
        if (Peek().type == JassTokenType::KW_TYPE) {
            // Type declaration can appear at top level
            Advance();
            auto decl = std::make_unique<AstNode>(AstNodeType::TYPE_DECL);
            decl->text = Expect(JassTokenType::IDENTIFIER).text;
            if (Peek().type == JassTokenType::KW_EXTENDS || Peek().text == "extends") {
                Advance();
                decl->extendsType = ParseType();
            }
            block->children.push_back(std::move(decl));
            SkipNewlines();
            continue;
        }

        auto decl = std::make_unique<AstNode>(AstNodeType::GLOBAL_DECL);
        decl->isConstant = Match(JassTokenType::KW_CONSTANT);
        decl->returnType = ParseType();
        if (decl->returnType == "nothing") {
            errors_.push_back("Unexpected token '" + Peek().text + "' in globals at line " + std::to_string(Peek().line));
            Advance();
            SkipNewlines();
            continue;
        }
        if (Match(JassTokenType::KW_ARRAY)) {
            decl->isArray = true;
        }
        if (Peek().type == JassTokenType::IDENTIFIER) {
            decl->text = Advance().text;
            // Check for array keyword
            if (Match(JassTokenType::KW_ARRAY)) {
                decl->isArray = true;
            }
            if (Match(JassTokenType::OP_ASSIGN)) {
                decl->children.push_back(ParseExpression());
            }
        }
        block->children.push_back(std::move(decl));
        SkipNewlines();
    }
    Expect(JassTokenType::KW_ENDGLOBALS);
    return block;
}

std::unique_ptr<AstNode> JassParser::ParseFunctionDecl(bool isConstant) {
    bool isConst = isConstant || Match(JassTokenType::KW_CONSTANT);
    Expect(JassTokenType::KW_FUNCTION);

    auto func = std::make_unique<AstNode>(AstNodeType::FUNCTION_DECL);
    func->isConstant = isConst;
    func->text = Expect(JassTokenType::IDENTIFIER).text;
    func->line = Peek().line;

    SkipNewlines();

    // Takes clause
    Expect(JassTokenType::KW_TAKES);
    SkipNewlines();
    if (Peek().type != JassTokenType::KW_RETURNS && Peek().type != JassTokenType::NEWLINE) {
        // Parse parameters
        if (Peek().type != JassTokenType::KW_NOTHING) {
            while (true) {
                std::string paramType = ParseType();
                if (Peek().type == JassTokenType::IDENTIFIER) {
                    std::string paramName = Advance().text;
                    func->parameters.push_back({paramName, paramType});
                }
                SkipNewlines();
                if (!Match(JassTokenType::COMMA)) break;
                SkipNewlines();
            }
        } else {
            Advance(); // nothing
        }
    }
    SkipNewlines();

    // Returns clause
    Expect(JassTokenType::KW_RETURNS);
    SkipNewlines();
    if (Peek().type != JassTokenType::NEWLINE) {
        func->returnType = ParseType();
    }
    SkipNewlines();

    // Body
    func->children.push_back(ParseBlock());

    return func;
}

std::unique_ptr<AstNode> JassParser::ParseNativeDecl(bool isConstant) {
    auto native = std::make_unique<AstNode>(AstNodeType::NATIVE_DECL);
    // Check for constant native
    if (isConstant || Peek().type == JassTokenType::KW_CONSTANT) {
        native->isConstant = true;
        if (!isConstant) Advance();
    }
    native->text = Expect(JassTokenType::IDENTIFIER).text;
    native->line = Peek().line;

    SkipNewlines();
    Expect(JassTokenType::KW_TAKES);
    SkipNewlines();
    if (Peek().type != JassTokenType::KW_RETURNS) {
        if (Peek().type != JassTokenType::KW_NOTHING) {
            while (true) {
                std::string paramType = ParseType();
                if (Peek().type == JassTokenType::IDENTIFIER) {
                    native->parameters.push_back({Advance().text, paramType});
                }
                SkipNewlines();
                if (!Match(JassTokenType::COMMA)) break;
                SkipNewlines();
            }
        } else {
            Advance();
        }
    }
    SkipNewlines();
    Expect(JassTokenType::KW_RETURNS);
    SkipNewlines();
    if (Peek().type != JassTokenType::NEWLINE) {
        native->returnType = ParseType();
    }
    return native;
}

std::unique_ptr<AstNode> JassParser::ParseBlock() {
    auto block = std::make_unique<AstNode>(AstNodeType::BLOCK);
    SkipNewlines();

    while (true) {
        auto& tok = Peek();

        if (tok.type == JassTokenType::KW_ENDFUNCTION ||
            tok.type == JassTokenType::KW_ENDMETHOD ||
            tok.type == JassTokenType::KW_ENDIF ||
            tok.type == JassTokenType::KW_ENDLOOP ||
            tok.type == JassTokenType::KW_ELSE ||
            tok.type == JassTokenType::KW_ELSEIF ||
            tok.type == JassTokenType::EOF_TOKEN)
            break;

        if (tok.type == JassTokenType::NEWLINE) { Advance(); continue; }

        block->children.push_back(ParseStatement());
        SkipNewlines();
    }
    return block;
}

std::unique_ptr<AstNode> JassParser::ParseStatement() {
    auto& tok = Peek();

    // debug statement - skip
    if (tok.type == JassTokenType::KW_DEBUG) {
        Advance();
        auto stmt = ParseStatement();
        return stmt;
    }

    // Local variable declaration
    if (tok.type == JassTokenType::KW_LOCAL) {
        Advance();
        auto stmt = std::make_unique<AstNode>(AstNodeType::SET_STMT);
        stmt->line = tok.line;
        stmt->returnType = ParseType();
        if (Match(JassTokenType::KW_ARRAY)) {
            stmt->isArray = true;
        }
        stmt->text = Expect(JassTokenType::IDENTIFIER).text;
        // Check for array keyword
        if (Match(JassTokenType::KW_ARRAY)) {
            stmt->isArray = true;
        }
        if (Match(JassTokenType::OP_ASSIGN)) {
            stmt->children.push_back(ParseExpression());
        }
        return stmt;
    }

    // Set statement (supports obj.member[.sub] chains and arr[i])
    if (tok.type == JassTokenType::KW_SET) {
        Advance();
        auto stmt = std::make_unique<AstNode>(AstNodeType::SET_STMT);
        stmt->line = tok.line;
        stmt->text = RewriteThistype(Expect(JassTokenType::IDENTIFIER).text);
        std::unique_ptr<AstNode> target;
        if (Match(JassTokenType::DOT)) {
            // Dotted target: build nested MEMBER_ACCESS, innermost first.
            target = std::make_unique<AstNode>(AstNodeType::MEMBER_ACCESS);
            target->text = Expect(JassTokenType::IDENTIFIER).text;
            target->children.push_back(std::make_unique<AstNode>(AstNodeType::VARIABLE, stmt->text));
            while (Match(JassTokenType::DOT)) {
                auto outer = std::make_unique<AstNode>(AstNodeType::MEMBER_ACCESS);
                outer->text = Expect(JassTokenType::IDENTIFIER).text;
                outer->children.push_back(std::move(target));
                target = std::move(outer);
            }
            if (Match(JassTokenType::LBRACKET)) {
                target->children.push_back(ParseExpression());
                Expect(JassTokenType::RBRACKET);
            }
            stmt->children.push_back(std::move(target));
        } else if (Match(JassTokenType::LBRACKET)) {
            auto arr = std::make_unique<AstNode>(AstNodeType::ARRAY_ACCESS);
            arr->text = stmt->text;
            arr->children.push_back(ParseExpression());
            Expect(JassTokenType::RBRACKET);
            stmt->children.push_back(std::move(arr));
        }
        if (Peek().type == JassTokenType::OP_ASSIGN) {
            Advance(); // =
            stmt->children.push_back(ParseExpression());
        }
        return stmt;
    }

    // Call statement (supports obj.method(args) and Type.method(args))
    if (tok.type == JassTokenType::KW_CALL) {
        Advance();
        std::string base = RewriteThistype(Expect(JassTokenType::IDENTIFIER).text);
        const int callLine = tok.line;
        if (Match(JassTokenType::DOT)) {
            auto node = std::make_unique<AstNode>(AstNodeType::METHOD_CALL);
            node->line = callLine;
            node->text = Expect(JassTokenType::IDENTIFIER).text;
            node->children.push_back(std::make_unique<AstNode>(AstNodeType::VARIABLE, base));
            Expect(JassTokenType::LPAREN);
            while (Peek().type != JassTokenType::RPAREN && Peek().type != JassTokenType::EOF_TOKEN) {
                node->children.push_back(ParseExpression());
                if (!Match(JassTokenType::COMMA)) break;
            }
            Expect(JassTokenType::RPAREN);
            return node;
        }
        auto stmt = std::make_unique<AstNode>(AstNodeType::CALL_STMT);
        stmt->line = callLine;
        stmt->text = base;
        Expect(JassTokenType::LPAREN);
        while (Peek().type != JassTokenType::RPAREN && Peek().type != JassTokenType::EOF_TOKEN) {
            stmt->children.push_back(ParseExpression());
            if (!Match(JassTokenType::COMMA)) break;
        }
        Expect(JassTokenType::RPAREN);
        return stmt;
    }

    // If statement
    if (tok.type == JassTokenType::KW_IF) {
        Advance();
        auto stmt = std::make_unique<AstNode>(AstNodeType::IF_STMT);
        stmt->line = tok.line;
        stmt->children.push_back(ParseExpression()); // condition
        Expect(JassTokenType::KW_THEN);
        stmt->children.push_back(ParseBlock()); // then block
        while (Match(JassTokenType::KW_ELSEIF)) {
            auto elseifNode = std::make_unique<AstNode>(AstNodeType::IF_STMT);
            elseifNode->children.push_back(ParseExpression());
            Expect(JassTokenType::KW_THEN);
            elseifNode->children.push_back(ParseBlock());
            stmt->children.push_back(std::move(elseifNode));
        }
        if (Match(JassTokenType::KW_ELSE)) {
            auto elseNode = ParseBlock();
            stmt->children.push_back(std::move(elseNode));
        }
        Expect(JassTokenType::KW_ENDIF);
        return stmt;
    }

    // Loop statement
    if (tok.type == JassTokenType::KW_LOOP) {
        Advance();
        auto stmt = std::make_unique<AstNode>(AstNodeType::LOOP_STMT);
        stmt->line = tok.line;
        Expect(JassTokenType::NEWLINE);
        stmt->children.push_back(ParseBlock());
        Expect(JassTokenType::KW_ENDLOOP);
        return stmt;
    }

    // Exitwhen
    if (tok.type == JassTokenType::KW_EXITWHEN) {
        Advance();
        auto stmt = std::make_unique<AstNode>(AstNodeType::EXITWHEN);
        stmt->line = tok.line;
        stmt->children.push_back(ParseExpression());
        return stmt;
    }

    // Return statement
    if (tok.type == JassTokenType::KW_RETURN) {
        Advance();
        auto stmt = std::make_unique<AstNode>(AstNodeType::RETURN_STMT);
        stmt->line = tok.line;
        if (Peek().type != JassTokenType::NEWLINE && Peek().type != JassTokenType::EOF_TOKEN) {
            stmt->children.push_back(ParseExpression());
        }
        return stmt;
    }

    // Skip unknown
    errors_.push_back("Unexpected token '" + tok.text + "' at line " + std::to_string(tok.line));
    Advance();
    return std::make_unique<AstNode>(AstNodeType::BLOCK);
}

// Expression parser with proper operator precedence
// Precedence (lowest to highest):
//   or
//   and
//   == !=
//   < > <= >=
//   + -
//   * /
//   unary (-, not)

std::unique_ptr<AstNode> JassParser::ParseExpression() {
    return ParseOrExpr();
}

std::unique_ptr<AstNode> JassParser::ParseOrExpr() {
    auto left = ParseAndExpr();
    while (Peek().type == JassTokenType::KW_OR) {
        auto node = std::make_unique<AstNode>(AstNodeType::BINARY_OP, "or");
        node->line = Peek().line;
        Advance();
        node->children.push_back(std::move(left));
        node->children.push_back(ParseAndExpr());
        left = std::move(node);
    }
    return left;
}

std::unique_ptr<AstNode> JassParser::ParseAndExpr() {
    auto left = ParseEqualityExpr();
    while (Peek().type == JassTokenType::KW_AND) {
        auto node = std::make_unique<AstNode>(AstNodeType::BINARY_OP, "and");
        node->line = Peek().line;
        Advance();
        node->children.push_back(std::move(left));
        node->children.push_back(ParseEqualityExpr());
        left = std::move(node);
    }
    return left;
}

std::unique_ptr<AstNode> JassParser::ParseEqualityExpr() {
    auto left = ParseComparisonExpr();
    while (Peek().type == JassTokenType::OP_EQ || Peek().type == JassTokenType::OP_NEQ) {
        auto node = std::make_unique<AstNode>(AstNodeType::BINARY_OP, Peek().text);
        node->line = Peek().line;
        Advance();
        node->children.push_back(std::move(left));
        node->children.push_back(ParseComparisonExpr());
        left = std::move(node);
    }
    return left;
}

std::unique_ptr<AstNode> JassParser::ParseComparisonExpr() {
    auto left = ParseAddSubExpr();
    while (Peek().type == JassTokenType::OP_LT || Peek().type == JassTokenType::OP_GT ||
           Peek().type == JassTokenType::OP_LE || Peek().type == JassTokenType::OP_GE) {
        auto node = std::make_unique<AstNode>(AstNodeType::BINARY_OP, Peek().text);
        node->line = Peek().line;
        Advance();
        node->children.push_back(std::move(left));
        node->children.push_back(ParseAddSubExpr());
        left = std::move(node);
    }
    return left;
}

std::unique_ptr<AstNode> JassParser::ParseAddSubExpr() {
    auto left = ParseMulDivExpr();
    while (Peek().type == JassTokenType::OP_PLUS || Peek().type == JassTokenType::OP_MINUS) {
        auto node = std::make_unique<AstNode>(AstNodeType::BINARY_OP, Peek().text);
        node->line = Peek().line;
        Advance();
        node->children.push_back(std::move(left));
        node->children.push_back(ParseMulDivExpr());
        left = std::move(node);
    }
    return left;
}

std::unique_ptr<AstNode> JassParser::ParseMulDivExpr() {
    auto left = ParseUnaryExpr();
    while (Peek().type == JassTokenType::OP_MUL || Peek().type == JassTokenType::OP_DIV) {
        auto node = std::make_unique<AstNode>(AstNodeType::BINARY_OP, Peek().text);
        node->line = Peek().line;
        Advance();
        node->children.push_back(std::move(left));
        node->children.push_back(ParseUnaryExpr());
        left = std::move(node);
    }
    return left;
}

std::unique_ptr<AstNode> JassParser::ParseUnaryExpr() {
    if (Peek().type == JassTokenType::OP_MINUS) {
        auto node = std::make_unique<AstNode>(AstNodeType::UNARY_OP, "-");
        node->line = Peek().line;
        Advance();
        node->children.push_back(ParseUnaryExpr());
        return node;
    }
    if (Peek().type == JassTokenType::KW_NOT) {
        auto node = std::make_unique<AstNode>(AstNodeType::UNARY_OP, "not");
        node->line = Peek().line;
        Advance();
        node->children.push_back(ParseUnaryExpr());
        return node;
    }
    return ParsePrimary();
}

std::unique_ptr<AstNode> JassParser::ParsePrimary() {
    auto& tok = Peek();

    switch (tok.type) {
        case JassTokenType::INTEGER_LITERAL: {
            auto node = std::make_unique<AstNode>(AstNodeType::LITERAL_INT, tok.text);
            node->numberValue = tok.numberValue;
            Advance();
            return node;
        }
        case JassTokenType::REAL_LITERAL: {
            auto node = std::make_unique<AstNode>(AstNodeType::LITERAL_REAL, tok.text);
            node->numberValue = tok.numberValue;
            Advance();
            return node;
        }
        case JassTokenType::STRING_LITERAL: {
            auto node = std::make_unique<AstNode>(AstNodeType::LITERAL_STRING);
            node->text = tok.text;
            Advance();
            return node;
        }
        case JassTokenType::RAWCODE_LITERAL: {
            auto node = std::make_unique<AstNode>(AstNodeType::LITERAL_RAWCODE, tok.text);
            node->numberValue = tok.numberValue;
            Advance();
            return node;
        }
        case JassTokenType::KW_TRUE: {
            Advance();
            return std::make_unique<AstNode>(AstNodeType::LITERAL_BOOL, "true");
        }
        case JassTokenType::KW_FALSE: {
            Advance();
            return std::make_unique<AstNode>(AstNodeType::LITERAL_BOOL, "false");
        }
        case JassTokenType::KW_NULL: {
            Advance();
            return std::make_unique<AstNode>(AstNodeType::LITERAL_NULL);
        }
        case JassTokenType::LPAREN: {
            Advance();
            auto expr = ParseExpression();
            Expect(JassTokenType::RPAREN);
            return expr;
        }
        case JassTokenType::DOT: {
            // Leading-dot shorthand: `.foo` / `.foo()` means `this.foo`.
            Advance();
            std::unique_ptr<AstNode> base =
                std::make_unique<AstNode>(AstNodeType::VARIABLE, "this");
            std::string member = Expect(JassTokenType::IDENTIFIER).text;
            while (true) {
                if (Peek().type == JassTokenType::LPAREN) {
                    auto node = std::make_unique<AstNode>(AstNodeType::METHOD_CALL, member);
                    node->line = tok.line;
                    node->children.push_back(std::move(base));
                    Advance(); // (
                    while (Peek().type != JassTokenType::RPAREN && Peek().type != JassTokenType::EOF_TOKEN) {
                        if (Peek().type == JassTokenType::NEWLINE) { Advance(); continue; }
                        node->children.push_back(ParseExpression());
                        if (!Match(JassTokenType::COMMA)) break;
                    }
                    Expect(JassTokenType::RPAREN);
                    base = std::move(node);
                } else {
                    auto node = std::make_unique<AstNode>(AstNodeType::MEMBER_ACCESS, member);
                    node->line = tok.line;
                    node->children.push_back(std::move(base));
                    if (Match(JassTokenType::LBRACKET)) {
                        node->children.push_back(ParseExpression());
                        Expect(JassTokenType::RBRACKET);
                    }
                    base = std::move(node);
                }
                if (Peek().type != JassTokenType::DOT) break;
                Advance();
                member = Expect(JassTokenType::IDENTIFIER).text;
            }
            return base;
        }
        case JassTokenType::IDENTIFIER: {
            std::string name = Advance().text;
            // `method a.b.c` function reference (vJASS method values).
            // Only the dotted shape is special; a bare `method`
            // identifier keeps working (retail-safe).
            if (name == "method" && Peek().type == JassTokenType::IDENTIFIER &&
                PeekAt(1).type == JassTokenType::DOT) {
                std::string path = Advance().text;
                while (Match(JassTokenType::DOT)) {
                    path += "." + Expect(JassTokenType::IDENTIFIER).text;
                }
                auto node = std::make_unique<AstNode>(AstNodeType::LITERAL_FUNCTION_REF);
                const size_t dot = path.find('.');
                if (dot != std::string::npos) {
                    node->text = ResolveMethodRef(path.substr(0, dot), path.substr(dot + 1));
                } else {
                    node->text = path;
                }
                return node;
            }
            name = RewriteThistype(name);
            if (Peek().type == JassTokenType::DOT) {
                // Member access / method call chain.
                std::unique_ptr<AstNode> base =
                    std::make_unique<AstNode>(AstNodeType::VARIABLE, name);
                while (Match(JassTokenType::DOT)) {
                    std::string member = Expect(JassTokenType::IDENTIFIER).text;
                    if (Peek().type == JassTokenType::LPAREN) {
                        auto node = std::make_unique<AstNode>(AstNodeType::METHOD_CALL, member);
                        node->line = tok.line;
                        node->children.push_back(std::move(base));
                        Advance(); // (
                        while (Peek().type != JassTokenType::RPAREN && Peek().type != JassTokenType::EOF_TOKEN) {
                            if (Peek().type == JassTokenType::NEWLINE) { Advance(); continue; }
                            node->children.push_back(ParseExpression());
                            if (!Match(JassTokenType::COMMA)) break;
                        }
                        Expect(JassTokenType::RPAREN);
                        base = std::move(node);
                    } else {
                        auto node = std::make_unique<AstNode>(AstNodeType::MEMBER_ACCESS, member);
                        node->line = tok.line;
                        node->children.push_back(std::move(base));
                        if (Match(JassTokenType::LBRACKET)) {
                            node->children.push_back(ParseExpression());
                            Expect(JassTokenType::RBRACKET);
                        }
                        base = std::move(node);
                    }
                }
                return base;
            }
            if (Peek().type == JassTokenType::LPAREN) {
                // Function call
                auto node = std::make_unique<AstNode>(AstNodeType::FUNC_CALL, name);
                Advance(); // (
                while (Peek().type != JassTokenType::RPAREN && Peek().type != JassTokenType::EOF_TOKEN) {
                    if (Peek().type == JassTokenType::NEWLINE) { Advance(); continue; }
                    node->children.push_back(ParseExpression());
                    if (!Match(JassTokenType::COMMA)) break;
                }
                Expect(JassTokenType::RPAREN);
                return node;
            } else if (Peek().type == JassTokenType::LBRACKET) {
                Advance(); // [
                auto node = std::make_unique<AstNode>(AstNodeType::ARRAY_ACCESS, name);
                node->children.push_back(ParseExpression());
                Expect(JassTokenType::RBRACKET);
                return node;
            } else {
                return std::make_unique<AstNode>(AstNodeType::VARIABLE, name);
            }
        }
        // function keyword for code type references
        case JassTokenType::KW_FUNCTION: {
            Advance();
            auto node = std::make_unique<AstNode>(AstNodeType::LITERAL_FUNCTION_REF);
            node->text = Expect(JassTokenType::IDENTIFIER).text;
            return node;
        }
        default:
            errors_.push_back("Unexpected token '" + tok.text + "' in expression at line " + std::to_string(tok.line));
            Advance();
            return std::make_unique<AstNode>(AstNodeType::LITERAL_NULL);
    }
}

std::unique_ptr<AstNode> JassParser::ParseStructDecl() {
    // At entry Peek() is the contextual `struct` identifier.
    const int structLine = Peek().line;
    Advance(); // struct
    auto node = std::make_unique<AstNode>(AstNodeType::STRUCT_DECL);
    node->line = structLine;
    node->text = Expect(JassTokenType::IDENTIFIER).text;
    if (Peek().type == JassTokenType::KW_EXTENDS) {
        Advance();
        node->extendsType = ParseType();
    }
    SkipNewlines();

    const std::string outerStruct = currentStruct_;
    currentStruct_ = node->text;
    ParseStructOrModuleBody(node.get(), false);
    Expect(JassTokenType::KW_ENDSTRUCT);
    currentStruct_ = outerStruct;
    return node;
}

std::unique_ptr<AstNode> JassParser::ParseInterfaceDecl() {
    const int interfaceLine = Peek().line;
    Advance(); // interface
    auto node = std::make_unique<AstNode>(AstNodeType::STRUCT_DECL);
    node->isInterface = true;
    node->line = interfaceLine;
    node->text = Expect(JassTokenType::IDENTIFIER).text;
    if (Peek().type == JassTokenType::KW_EXTENDS) {
        Advance();
        node->extendsType = ParseType();
    }
    SkipNewlines();
    const std::string outerStruct = currentStruct_;
    currentStruct_ = node->text;
    ParseStructOrModuleBody(node.get(), false);
    if (Peek().type == JassTokenType::IDENTIFIER && Peek().text == "endinterface") {
        Advance();
    } else {
        errors_.push_back("Expected 'endinterface' at line " + std::to_string(Peek().line));
    }
    currentStruct_ = outerStruct;
    return node;
}

std::unique_ptr<AstNode> JassParser::ParseModuleDecl() {
    // At entry Peek() is the contextual `module` identifier.
    const int moduleLine = Peek().line;
    Advance(); // module
    auto node = std::make_unique<AstNode>(AstNodeType::MODULE_DECL);
    node->line = moduleLine;
    node->text = Expect(JassTokenType::IDENTIFIER).text;
    SkipNewlines();

    const std::string outerStruct = currentStruct_;
    currentStruct_ = node->text;
    ParseStructOrModuleBody(node.get(), true);
    if (Peek().type == JassTokenType::IDENTIFIER && Peek().text == "endmodule") {
        Advance();
    } else {
        errors_.push_back("Expected 'endmodule' at line " + std::to_string(Peek().line));
    }
    currentStruct_ = outerStruct;
    return node;
}

static bool AtBodyEnd(const JassToken& tok, bool isModule, bool isInterface) {
    if (tok.type == JassTokenType::EOF_TOKEN) return true;
    if (isModule) return tok.type == JassTokenType::IDENTIFIER && tok.text == "endmodule";
    if (isInterface) return tok.type == JassTokenType::IDENTIFIER && tok.text == "endinterface";
    return tok.type == JassTokenType::KW_ENDSTRUCT;
}

void JassParser::ParseStructOrModuleBody(AstNode* node, bool isModule) {
    while (!AtBodyEnd(Peek(), isModule, node->isInterface)) {
        while (Peek().type == JassTokenType::IDENTIFIER &&
               (Peek().text == "private" || Peek().text == "public")) {
            // Struct-level privacy is accepted but not enforced
            // (documented); only scope-level privacy mangles names.
            Advance();
            SkipNewlines();
        }
        if (Peek().type == JassTokenType::IDENTIFIER && Peek().text == "defaults") {
            while (Peek().type != JassTokenType::NEWLINE && Peek().type != JassTokenType::EOF_TOKEN) Advance();
            SkipNewlines();
            continue;
        }
        if (Peek().type == JassTokenType::IDENTIFIER && Peek().text == "delegate") {
            const int line = Peek().line;
            Advance();
            SkipNewlines();
            const std::string typeName = ParseType();
            SkipNewlines();
            if (Peek().type != JassTokenType::IDENTIFIER) {
                errors_.push_back("Expected delegated member at line " + std::to_string(line));
            } else {
                node->delegates.push_back({typeName, Advance().text});
            }
            SkipNewlines();
            continue;
        }
        if (Peek().type == JassTokenType::IDENTIFIER && Peek().text == "interface") {
            hoisted_.push_back(ParseInterfaceDecl());
            continue;
        }
        if (Peek().type == JassTokenType::IDENTIFIER && Peek().text == "module") {
            hoisted_.push_back(ParseModuleDecl());
            continue;
        }
        if (Peek().type == JassTokenType::IDENTIFIER && Peek().text == "implement") {
            const int implLine = Peek().line;
            Advance();
            SkipNewlines();
            bool optional = false;
            if (Peek().type == JassTokenType::IDENTIFIER && Peek().text == "optional") {
                Advance();
                SkipNewlines();
                optional = true;
            }
            if (isModule) {
                if (Peek().type == JassTokenType::IDENTIFIER) {
                    ImplementRecord rec;
                    rec.structName = node->text;
                    rec.moduleName = Advance().text;
                    rec.optional = optional;
                    rec.intoModule = true;
                    pendingImplements_.push_back(std::move(rec));
                } else {
                    errors_.push_back("Expected module name after 'implement' at line " +
                                      std::to_string(implLine));
                }
            } else if (Peek().type == JassTokenType::IDENTIFIER) {
                pendingImplements_.push_back({node->text, Advance().text, optional});
            } else {
                errors_.push_back("Expected module name after 'implement' at line " +
                                  std::to_string(implLine));
            }
            SkipNewlines();
            continue;
        }
        bool isStatic = false;
        if (Peek().type == JassTokenType::IDENTIFIER && Peek().text == "static") {
            Advance();
            SkipNewlines();
            isStatic = true;
        }
        if (Peek().type == JassTokenType::IDENTIFIER && Peek().text == "stub") {
            Advance(); // accepted, no special semantics
            SkipNewlines();
        }
        if (Peek().type == JassTokenType::IDENTIFIER && Peek().text == "method") {
            Advance(); // method
            auto method = std::make_unique<AstNode>(AstNodeType::FUNCTION_DECL);
            method->isStatic = isStatic;
            method->text = node->text + "." + Expect(JassTokenType::IDENTIFIER).text;
            method->line = Peek().line;
            SkipNewlines();
            // takes clause (same shape as functions)
            Expect(JassTokenType::KW_TAKES);
            SkipNewlines();
            if (Peek().type != JassTokenType::KW_RETURNS && Peek().type != JassTokenType::NEWLINE) {
                if (Peek().type != JassTokenType::KW_NOTHING) {
                    while (true) {
                        std::string paramType = ParseType();
                        if (Peek().type == JassTokenType::IDENTIFIER) {
                            method->parameters.push_back({Advance().text, paramType});
                        }
                        SkipNewlines();
                        if (!Match(JassTokenType::COMMA)) break;
                        SkipNewlines();
                    }
                } else {
                    Advance();
                }
            }
            SkipNewlines();
            Expect(JassTokenType::KW_RETURNS);
            SkipNewlines();
            if (Peek().type != JassTokenType::NEWLINE) {
                method->returnType = ParseType();
            }
            SkipNewlines();
            if (!isStatic) {
                // Implicit receiver binds positionally like any parameter.
                method->parameters.insert(method->parameters.begin(), {"this", node->text});
            }
            SkipNewlines();
            const bool abstractMethod = node->isInterface &&
                Peek().type == JassTokenType::IDENTIFIER &&
                (Peek().text == "method" || Peek().text == "endinterface" ||
                 Peek().text == "static" || Peek().text == "defaults" ||
                 Peek().text == "operator" || Peek().text == "implement");
            if (abstractMethod || (node->isInterface && Peek().type == JassTokenType::NEWLINE &&
                                   PeekAt(1).type == JassTokenType::IDENTIFIER &&
                                   (PeekAt(1).text == "method" || PeekAt(1).text == "endinterface" ||
                                    PeekAt(1).text == "static" || PeekAt(1).text == "defaults"))) {
                method->children.push_back(std::make_unique<AstNode>(AstNodeType::BLOCK));
            } else {
                method->children.push_back(ParseBlock());
                Expect(JassTokenType::KW_ENDMETHOD);
            }
            node->children.push_back(std::move(method));
            SkipNewlines();
            continue;
        }
        // Member declaration: [readonly] type name [array [size]] [= expr]
        if (Peek().type == JassTokenType::IDENTIFIER && Peek().text == "readonly") {
            Advance();
            SkipNewlines();
        }
        auto member = std::make_unique<AstNode>(AstNodeType::GLOBAL_DECL);
        member->isStatic = isStatic;
        member->line = Peek().line;
        member->returnType = ParseType();
        if (member->returnType == "nothing") {
            errors_.push_back("Expected member declaration in struct '" + node->text +
                              "' at line " + std::to_string(Peek().line));
            Advance();
            SkipNewlines();
            continue;
        }
        if (Match(JassTokenType::KW_ARRAY)) member->isArray = true;
        member->text = Expect(JassTokenType::IDENTIFIER).text;
        if (Match(JassTokenType::KW_ARRAY)) member->isArray = true;
        if (Match(JassTokenType::LBRACKET)) {
            // Optional array size, ignored (dynamic storage).
            if (Peek().type != JassTokenType::RBRACKET) ParseExpression();
            Expect(JassTokenType::RBRACKET);
            member->isArray = true;
        }
        if (Match(JassTokenType::OP_ASSIGN)) {
            member->children.push_back(ParseExpression());
        }
        node->children.push_back(std::move(member));
        SkipNewlines();
    }
}

void JassParser::ParseScopeOrLibrary(AstNode* program, bool isLibrary) {
    // At entry Peek() is the contextual `scope`/`library` identifier.
    Advance();
    const std::string prefix = Expect(JassTokenType::IDENTIFIER).text;
    // Header: [initializer Name] [needs A, B, ...] in any order.
    std::vector<std::string> rawInitializers;
    while (Peek().type != JassTokenType::NEWLINE && Peek().type != JassTokenType::EOF_TOKEN) {
        if (Peek().type == JassTokenType::IDENTIFIER && Peek().text == "initializer") {
            Advance();
            if (Peek().type == JassTokenType::IDENTIFIER) {
                rawInitializers.push_back(Advance().text);
            }
            continue;
        }
        if (Peek().type == JassTokenType::IDENTIFIER && Peek().text == "needs") {
            Advance();
            while (Peek().type == JassTokenType::IDENTIFIER) {
                Advance();
                if (!Match(JassTokenType::COMMA)) break;
            }
            continue;
        }
        Advance();
    }
    SkipNewlines();

    // library members are private by default, scope members public.
    const bool defaultPrivate = isLibrary;
    std::vector<std::unique_ptr<AstNode>> body;
    std::unordered_set<std::string> privates;
    const size_t implementMark = pendingImplements_.size();
    const JassTokenType ender = isLibrary ? JassTokenType::KW_ENDLIBRARY : JassTokenType::KW_ENDSCOPE;
    while (Peek().type != ender && Peek().type != JassTokenType::EOF_TOKEN) {
        std::string rawName;
        bool isPrivate = false;
        auto decl = ParseDecl(defaultPrivate, rawName, isPrivate);
        if (decl) {
            // Modules stay file-scoped (never renamed); only their
            // implementers resolve through scope mangling.
            if (isPrivate && !rawName.empty() && decl->type != AstNodeType::MODULE_DECL) {
                privates.insert(rawName);
            }
            body.push_back(std::move(decl));
        }
        SkipNewlines();
    }
    Expect(ender);

    // Tag implements recorded inside this body with the scope prefix so
    // expansion resolves the (possibly mangled) struct.
    for (size_t i = implementMark; i < pendingImplements_.size(); i++) {
        pendingImplements_[i].scopePrefix = prefix;
    }
    // Flatten with privacy rewriting (supports forward references).
    for (auto& decl : body) {
        RewriteScopePrivacy(decl.get(), prefix, privates, {});
    }
    for (auto& name : rawInitializers) {
        auto it = privates.find(name);
        initializers_.push_back(it != privates.end() ? prefix + "__" + name : name);
    }
    for (auto& decl : body) {
        program->children.push_back(std::move(decl));
    }
}

void JassParser::RewriteScopePrivacy(AstNode* node, const std::string& prefix,
                                     const std::unordered_set<std::string>& privates,
                                     const std::vector<std::string>& rawInitializers) {
    if (!node || privates.empty()) return;
    auto mangle = [&](const std::string& name) -> std::string {
        if (name.empty() || privates.find(name) == privates.end()) return name;
        const std::string tagged = prefix + "__" + name;
        if (name.compare(0, tagged.size(), tagged) == 0) return name; // idempotent
        return tagged;
    };
    auto mangleDotted = [&](const std::string& dotted) -> std::string {
        const size_t dot = dotted.find('.');
        if (dot == std::string::npos) return mangle(dotted);
        const std::string head = dotted.substr(0, dot);
        if (head.compare(0, 9, "thistype@") == 0) {
            const std::string inner = head.substr(9);
            const std::string fixed = (privates.find(inner) == privates.end()) ? inner : prefix + "__" + inner;
            return "thistype@" + fixed + dotted.substr(dot);
        }
        return mangle(head) + dotted.substr(dot);
    };
    // Rename declarations.
    if (node->type == AstNodeType::FUNCTION_DECL || node->type == AstNodeType::NATIVE_DECL ||
        node->type == AstNodeType::GLOBAL_DECL || node->type == AstNodeType::TYPE_DECL) {
        node->text = mangle(node->text);
    } else if (node->type == AstNodeType::STRUCT_DECL) {
        node->text = mangle(node->text);
        node->extendsType = mangle(node->extendsType);
        for (auto& child : node->children) {
            if (child->type == AstNodeType::FUNCTION_DECL) {
                child->text = mangleDotted(child->text);
                for (auto& param : child->parameters) {
                    if (param.second == "thistype") continue;
                    param.second = mangle(param.second);
                }
                if (child->returnType != "thistype") {
                    child->returnType = mangle(child->returnType);
                }
            }
        }
    }
    // Collect function-local names so shadowing locals are not renamed.
    if (node->type == AstNodeType::FUNCTION_DECL) {
        std::unordered_set<std::string> locals;
        std::function<void(const AstNode*)> collect = [&](const AstNode* n) {
            if (!n) return;
            if (n->type == AstNodeType::SET_STMT && !n->returnType.empty() && !n->text.empty()) {
                locals.insert(n->text);
            }
            for (const auto& c : n->children) collect(c.get());
        };
        for (const auto& p : node->parameters) locals.insert(p.first);
        for (const auto& c : node->children) collect(c.get());
        std::function<void(AstNode*)> rewrite = [&](AstNode* n) {
            if (!n) return;
            switch (n->type) {
                case AstNodeType::VARIABLE:
                    if (n->text.compare(0, 9, "thistype@") == 0) {
                        n->text = mangleDotted(n->text);
                    } else if (locals.find(n->text) == locals.end()) {
                        n->text = mangle(n->text);
                    }
                    break;
                case AstNodeType::FUNC_CALL:
                case AstNodeType::CALL_STMT:
                    n->text = mangleDotted(n->text);
                    break;
                case AstNodeType::SET_STMT:
                    if (n->children.empty() || n->children[0]->type != AstNodeType::MEMBER_ACCESS) {
                        if (n->returnType.empty() && locals.find(n->text) == locals.end()) {
                            n->text = mangle(n->text);
                        }
                    }
                    break;
                case AstNodeType::LITERAL_FUNCTION_REF:
                    n->text = mangleDotted(n->text);
                    break;
                default:
                    break;
            }
            for (auto& p : n->parameters) {
                if (p.second != "thistype") p.second = mangle(p.second);
            }
            if (n->type != AstNodeType::FUNCTION_DECL) {
                if (n->returnType != "thistype") n->returnType = mangle(n->returnType);
            }
            for (auto& c : n->children) rewrite(c.get());
        };
        for (auto& c : node->children) rewrite(c.get());
        return;
    }
    // Top-level: rewrite references but not local decls (none here).
    std::function<void(AstNode*)> rewriteTop = [&](AstNode* n) {
        if (!n) return;
        if (n->type == AstNodeType::VARIABLE) n->text = mangleDotted(n->text);
        else if (n->type == AstNodeType::FUNC_CALL || n->type == AstNodeType::CALL_STMT) n->text = mangleDotted(n->text);
        else if (n->type == AstNodeType::LITERAL_FUNCTION_REF) n->text = mangleDotted(n->text);
        for (auto& c : n->children) rewriteTop(c.get());
    };
    for (auto& c : node->children) rewriteTop(c.get());
    (void)rawInitializers;
}

std::unique_ptr<AstNode> JassParser::CloneNode(const AstNode* node) {
    if (!node) return nullptr;
    auto copy = std::make_unique<AstNode>(node->type, node->text);
    copy->numberValue = node->numberValue;
    copy->returnType = node->returnType;
    copy->parameters = node->parameters;
    copy->isConstant = node->isConstant;
    copy->isArray = node->isArray;
    copy->isStatic = node->isStatic;
    copy->isInterface = node->isInterface;
    copy->delegates = node->delegates;
    copy->extendsType = node->extendsType;
    copy->line = node->line;
    for (const auto& child : node->children) {
        copy->children.push_back(CloneNode(child.get()));
    }
    return copy;
}

void JassParser::ExpandImplements(AstNode* program) {
    if (program) {
        for (auto& extra : hoisted_) {
            if (extra) program->children.push_back(std::move(extra));
        }
        hoisted_.clear();
    }
    if (!program || pendingImplements_.empty()) {
        // Still drop stray module nodes so the VM never sees them.
        if (program) {
            std::vector<std::unique_ptr<AstNode>> kept;
            for (auto& child : program->children) {
                if (child->type == AstNodeType::MODULE_DECL) {
                    moduleDefs_.push_back(std::move(child));
                } else {
                    kept.push_back(std::move(child));
                }
            }
            program->children = std::move(kept);
        }
        return;
    }
    // Collect module definitions (kept alive in moduleDefs_).
    std::unordered_map<std::string, AstNode*> modules;
    {
        std::vector<std::unique_ptr<AstNode>> kept;
        for (auto& child : program->children) {
            if (child->type == AstNodeType::MODULE_DECL) {
                modules[child->text] = child.get();
                moduleDefs_.push_back(std::move(child));
            } else {
                kept.push_back(std::move(child));
            }
        }
        program->children = std::move(kept);
    }
    auto findStruct = [&](const ImplementRecord& rec) -> AstNode* {
        const std::string qualified = rec.scopePrefix.empty()
            ? rec.structName
            : rec.scopePrefix + "__" + rec.structName;
        for (auto& child : program->children) {
            if (child->type == AstNodeType::STRUCT_DECL && child->text == qualified) {
                return child.get();
            }
        }
        // Fallback: unique suffix match (mangled names from scopes).
        AstNode* candidate = nullptr;
        const std::string suffix = "__" + rec.structName;
        for (auto& child : program->children) {
            if (child->type == AstNodeType::STRUCT_DECL &&
                child->text.size() > suffix.size() &&
                child->text.compare(child->text.size() - suffix.size(), suffix.size(), suffix) == 0) {
                if (candidate) return nullptr; // ambiguous
                candidate = child.get();
            }
        }
        return candidate;
    };
    // Rewrite thistype@Old -> thistype@New across a copied subtree.
    std::function<void(AstNode*, const std::string&, const std::string&)> rethistype =
        [&](AstNode* n, const std::string& from, const std::string& to) {
            if (!n) return;
            if (n->type == AstNodeType::VARIABLE && n->text == "thistype@" + from) {
                n->text = "thistype@" + to;
            }
            if (n->type == AstNodeType::LITERAL_FUNCTION_REF &&
                n->text.compare(0, from.size() + 1, from + ".") == 0) {
                n->text = to + n->text.substr(from.size());
            }
            for (auto& c : n->children) rethistype(c.get(), from, to);
        };
    auto copyMember = [&](const AstNode* child, AstNode* target, const std::string& from, const std::string& to) -> bool {
        if (child->type == AstNodeType::FUNCTION_DECL) {
            const size_t dot = child->text.find('.');
            const std::string methodName =
                (dot == std::string::npos) ? child->text : child->text.substr(dot + 1);
            for (const auto& own : target->children) {
                if (own->type != AstNodeType::FUNCTION_DECL) continue;
                const size_t odot = own->text.find('.');
                const std::string ownName =
                    (odot == std::string::npos) ? own->text : own->text.substr(odot + 1);
                if (ownName == methodName) return false;
            }
            auto copy = CloneNode(child);
            copy->text = to + "." + methodName;
            rethistype(copy.get(), from, to);
            target->children.push_back(std::move(copy));
            return true;
        }
        if (child->type == AstNodeType::GLOBAL_DECL) {
            for (const auto& own : target->children) {
                if (own->type == AstNodeType::GLOBAL_DECL && own->text == child->text) return false;
            }
            auto copy = CloneNode(child);
            rethistype(copy.get(), from, to);
            target->children.push_back(std::move(copy));
            return true;
        }
        return false;
    };
    for (int pass = 0; pass < 8; ++pass) {
        bool grew = false;
        for (const auto& rec : pendingImplements_) {
            if (!rec.intoModule) continue;
            auto mit = modules.find(rec.moduleName);
            auto target = modules.find(rec.structName);
            if (mit == modules.end() || target == modules.end()) {
                if (pass == 0 && mit == modules.end() && !rec.optional) {
                    errors_.push_back("implement of undefined module '" + rec.moduleName +
                                      "' for module '" + rec.structName + "'");
                }
                continue;
            }
            for (const auto& child : mit->second->children) {
                if (copyMember(child.get(), target->second, mit->first, target->first)) grew = true;
            }
        }
        if (!grew) break;
    }
    for (const auto& rec : pendingImplements_) {
        if (rec.intoModule) continue;
        auto mit = modules.find(rec.moduleName);
        if (mit == modules.end()) {
            if (!rec.optional) {
                errors_.push_back("implement of undefined module '" + rec.moduleName +
                                  "' for struct '" + rec.structName + "'");
            }
            continue;
        }
        AstNode* target = findStruct(rec);
        if (!target) {
            errors_.push_back("implement into undefined struct '" + rec.structName + "'");
            continue;
        }
        const std::string& moduleName = mit->first;
        AstNode* module = mit->second;
        for (const auto& child : module->children) {
            if (child->type == AstNodeType::FUNCTION_DECL) {
                const size_t dot = child->text.find('.');
                const std::string methodName =
                    (dot == std::string::npos) ? child->text : child->text.substr(dot + 1);
                bool exists = false;
                for (const auto& own : target->children) {
                    if (own->type != AstNodeType::FUNCTION_DECL) continue;
                    const size_t odot = own->text.find('.');
                    const std::string ownName =
                        (odot == std::string::npos) ? own->text : own->text.substr(odot + 1);
                    if (ownName == methodName) {
                        exists = true;
                        break;
                    }
                }
                // The struct's own definition wins (documented).
                if (exists) continue;
                auto copy = CloneNode(child.get());
                copy->text = target->text + "." + methodName;
                rethistype(copy.get(), moduleName, target->text);
                target->children.push_back(std::move(copy));
            } else if (child->type == AstNodeType::GLOBAL_DECL) {
                bool exists = false;
                for (const auto& own : target->children) {
                    if (own->type == AstNodeType::GLOBAL_DECL && own->text == child->text) {
                        exists = true;
                        break;
                    }
                }
                if (exists) continue;
                auto copy = CloneNode(child.get());
                rethistype(copy.get(), moduleName, target->text);
                target->children.push_back(std::move(copy));
            }
        }
    }
}
