#pragma once
#include <string>
#include <vector>

enum class JassTokenType {
    // Keywords
    KW_FUNCTION, KW_ENDFUNCTION, KW_GLOBALS, KW_ENDGLOBALS,
    KW_IF, KW_THEN, KW_ELSEIF, KW_ELSE, KW_ENDIF,
    KW_LOOP, KW_ENDLOOP, KW_EXITWHEN,
    KW_RETURN, KW_CALL, KW_SET, KW_LOCAL, KW_CONSTANT, KW_NATIVE,
    KW_TAKES, KW_RETURNS, KW_NOTHING,
    KW_TRUE, KW_FALSE, KW_NULL,
    KW_NOT, KW_AND, KW_OR,
    KW_TYPE, KW_EXTENDS, KW_ARRAY, KW_DEBUG,
    // vJASS block terminators (zero collisions in retail scripts; the
    // matching openers struct/scope/library stay contextual identifiers).
    KW_ENDSTRUCT, KW_ENDMETHOD, KW_ENDSCOPE, KW_ENDLIBRARY,

    // Types
    TYPE_INTEGER, TYPE_REAL, TYPE_BOOLEAN, TYPE_STRING,
    TYPE_HANDLE, TYPE_CODE,

    // Operators
    OP_PLUS, OP_MINUS, OP_MUL, OP_DIV,
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LE, OP_GE,
    OP_ASSIGN,

    // Delimiters
    LPAREN, RPAREN, LBRACKET, RBRACKET, COMMA, DOT,
    NEWLINE, EOF_TOKEN,

    // Literals
    IDENTIFIER, INTEGER_LITERAL, REAL_LITERAL, STRING_LITERAL,
    RAWCODE_LITERAL,

    UNKNOWN
};

struct JassToken {
    JassTokenType type = JassTokenType::UNKNOWN;
    std::string text;
    double numberValue = 0;
    int line = 0;
    int column = 0;
};

class JassLexer {
public:
    JassLexer(const std::string& source);

    std::vector<JassToken> Tokenize();
    const std::vector<std::string>& GetErrors() const { return errors_; }

private:
    char Peek() const;
    char Advance();
    bool Match(char c);
    void SkipWhitespace();
    void SkipComment();
    JassToken ReadNumber();
    JassToken ReadString();
    JassToken ReadIdentifierOrKeyword();

    std::string source_;
    size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
    std::vector<std::string> errors_;
};
