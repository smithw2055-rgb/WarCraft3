#pragma once
#include "JassLexer.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

// AST node types
enum class AstNodeType {
    PROGRAM, GLOBAL_DECL, FUNCTION_DECL, NATIVE_DECL, TYPE_DECL,
    BLOCK, IF_STMT, LOOP_STMT, EXITWHEN, SET_STMT, CALL_STMT, RETURN_STMT,
    BINARY_OP, UNARY_OP, FUNC_CALL, ARRAY_ACCESS,
    LITERAL_INT, LITERAL_REAL, LITERAL_BOOL, LITERAL_STRING, LITERAL_NULL,
    LITERAL_RAWCODE, LITERAL_FUNCTION_REF,
    VARIABLE, PARAMETER, TYPE
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

private:
    const JassToken& Peek() const;
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
};
