#include "JassParser.h"
#include <iostream>

JassParser::JassParser(const std::vector<JassToken>& tokens) : tokens_(tokens) {}

const JassToken& JassParser::Peek() const {
    return tokens_[pos_];
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
        case JassTokenType::IDENTIFIER: Advance(); return tok.text; // handle subtype
        default: return "nothing";
    }
}

std::unique_ptr<AstNode> JassParser::Parse() {
    return ParseProgram();
}

std::unique_ptr<AstNode> JassParser::ParseProgram() {
    auto program = std::make_unique<AstNode>(AstNodeType::PROGRAM);
    SkipNewlines();

    while (Peek().type != JassTokenType::EOF_TOKEN) {
        // type declaration: type X extends Y
        if (Peek().type == JassTokenType::KW_TYPE) {
            Advance();
            auto decl = std::make_unique<AstNode>(AstNodeType::TYPE_DECL);
            decl->text = Expect(JassTokenType::IDENTIFIER).text;
            if (Peek().type == JassTokenType::KW_EXTENDS || Peek().text == "extends") {
                Advance();
                decl->extendsType = Expect(JassTokenType::IDENTIFIER).text;
            }
            program->children.push_back(std::move(decl));
            SkipNewlines();
        }
        else if (Match(JassTokenType::KW_GLOBALS)) {
            program->children.push_back(ParseGlobalBlock());
        } else if (Match(JassTokenType::KW_NATIVE)) {
            program->children.push_back(ParseNativeDecl());
        } else if (Peek().type == JassTokenType::KW_FUNCTION || Peek().type == JassTokenType::KW_CONSTANT) {
            program->children.push_back(ParseFunctionDecl());
        } else if (Peek().type == JassTokenType::KW_DEBUG) {
            Advance(); // skip debug
            // Skip the next statement
            while (Peek().type != JassTokenType::NEWLINE && Peek().type != JassTokenType::EOF_TOKEN) Advance();
            SkipNewlines();
        } else {
            // Could be a global variable declaration or constant global
            auto decl = std::make_unique<AstNode>(AstNodeType::GLOBAL_DECL);
            decl->isConstant = Match(JassTokenType::KW_CONSTANT);
            decl->returnType = ParseType();
            if (decl->returnType == "nothing") {
                // Unknown token, skip
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
            program->children.push_back(std::move(decl));
            SkipNewlines();
        }
        SkipNewlines();
    }
    return program;
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
                decl->extendsType = Expect(JassTokenType::IDENTIFIER).text;
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

std::unique_ptr<AstNode> JassParser::ParseFunctionDecl() {
    bool isConst = Match(JassTokenType::KW_CONSTANT);
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

std::unique_ptr<AstNode> JassParser::ParseNativeDecl() {
    auto native = std::make_unique<AstNode>(AstNodeType::NATIVE_DECL);
    // Check for constant native
    if (Peek().type == JassTokenType::KW_CONSTANT) {
        native->isConstant = true;
        Advance();
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

    // Set statement
    if (tok.type == JassTokenType::KW_SET) {
        Advance();
        auto stmt = std::make_unique<AstNode>(AstNodeType::SET_STMT);
        stmt->line = tok.line;
        stmt->text = Expect(JassTokenType::IDENTIFIER).text;
        if (Match(JassTokenType::LBRACKET)) {
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

    // Call statement
    if (tok.type == JassTokenType::KW_CALL) {
        Advance();
        auto stmt = std::make_unique<AstNode>(AstNodeType::CALL_STMT);
        stmt->line = tok.line;
        stmt->text = Expect(JassTokenType::IDENTIFIER).text;
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
        case JassTokenType::IDENTIFIER: {
            std::string name = Advance().text;
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
