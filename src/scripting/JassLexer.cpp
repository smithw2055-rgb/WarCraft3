#include "JassLexer.h"
#include <unordered_map>
#include <iostream>

static const std::unordered_map<std::string, JassTokenType> s_keywords = {
    {"function", JassTokenType::KW_FUNCTION},
    {"endfunction", JassTokenType::KW_ENDFUNCTION},
    {"globals", JassTokenType::KW_GLOBALS},
    {"endglobals", JassTokenType::KW_ENDGLOBALS},
    {"if", JassTokenType::KW_IF},
    {"then", JassTokenType::KW_THEN},
    {"elseif", JassTokenType::KW_ELSEIF},
    {"else", JassTokenType::KW_ELSE},
    {"endif", JassTokenType::KW_ENDIF},
    {"loop", JassTokenType::KW_LOOP},
    {"endloop", JassTokenType::KW_ENDLOOP},
    {"exitwhen", JassTokenType::KW_EXITWHEN},
    {"return", JassTokenType::KW_RETURN},
    {"call", JassTokenType::KW_CALL},
    {"set", JassTokenType::KW_SET},
    {"local", JassTokenType::KW_LOCAL},
    {"constant", JassTokenType::KW_CONSTANT},
    {"native", JassTokenType::KW_NATIVE},
    {"takes", JassTokenType::KW_TAKES},
    {"returns", JassTokenType::KW_RETURNS},
    {"nothing", JassTokenType::KW_NOTHING},
    {"true", JassTokenType::KW_TRUE},
    {"false", JassTokenType::KW_FALSE},
    {"null", JassTokenType::KW_NULL},
    {"not", JassTokenType::KW_NOT},
    {"and", JassTokenType::KW_AND},
    {"or", JassTokenType::KW_OR},
    {"type", JassTokenType::KW_TYPE},
    {"extends", JassTokenType::KW_EXTENDS},
    {"array", JassTokenType::KW_ARRAY},
    {"debug", JassTokenType::KW_DEBUG},

    {"integer", JassTokenType::TYPE_INTEGER},
    {"real", JassTokenType::TYPE_REAL},
    {"boolean", JassTokenType::TYPE_BOOLEAN},
    {"string", JassTokenType::TYPE_STRING},
    {"handle", JassTokenType::TYPE_HANDLE},
    {"code", JassTokenType::TYPE_CODE},
};

JassLexer::JassLexer(const std::string& source) : source_(source) {}

char JassLexer::Peek() const {
    if (pos_ >= source_.size()) return '\0';
    return source_[pos_];
}

char JassLexer::Advance() {
    char c = source_[pos_];
    if (c == '\n') { line_++; column_ = 1; }
    else column_++;
    pos_++;
    return c;
}

bool JassLexer::Match(char c) {
    if (Peek() != c) return false;
    Advance();
    return true;
}

void JassLexer::SkipWhitespace() {
    while (pos_ < source_.size()) {
        char c = Peek();
        if (c == ' ' || c == '\t' || c == '\r') { Advance(); }
        else if (c == '\n') { break; }
        else break;
    }
}

void JassLexer::SkipComment() {
    if (Match('/')) {
        if (Match('/')) {
            while (Peek() != '\n' && Peek() != '\0') Advance();
        } else if (Match('*')) {
            while (pos_ + 1 < source_.size()) {
                if (source_[pos_] == '*' && source_[pos_ + 1] == '/') {
                    Advance(); Advance(); return;
                }
                Advance();
            }
            errors_.push_back("Unterminated block comment");
        }
    }
}

JassToken JassLexer::ReadNumber() {
    JassToken token;
    token.line = line_;
    token.column = column_;
    std::string num;
    while (pos_ < source_.size() && (std::isdigit(Peek()) || Peek() == '.')) {
        num += Advance();
    }
    token.text = num;
    if (num.find('.') != std::string::npos) {
        token.type = JassTokenType::REAL_LITERAL;
        token.numberValue = std::stod(num);
    } else {
        token.type = JassTokenType::INTEGER_LITERAL;
        token.numberValue = (double)std::stoll(num);
    }
    return token;
}

JassToken JassLexer::ReadString() {
    JassToken token;
    token.line = line_;
    token.column = column_;
    token.type = JassTokenType::STRING_LITERAL;
    Advance(); // opening quote
    while (pos_ < source_.size() && Peek() != '"') {
        if (Peek() == '\\') { Advance(); if (Peek()) Advance(); }
        else token.text += Advance();
    }
    if (Peek() == '"') Advance(); // closing quote
    else errors_.push_back("Unterminated string literal");
    return token;
}

JassToken JassLexer::ReadIdentifierOrKeyword() {
    JassToken token;
    token.line = line_;
    token.column = column_;
    while (pos_ < source_.size() && (std::isalnum(Peek()) || Peek() == '_'))
        token.text += Advance();

    auto it = s_keywords.find(token.text);
    if (it != s_keywords.end()) token.type = it->second;
    else token.type = JassTokenType::IDENTIFIER;
    return token;
}

std::vector<JassToken> JassLexer::Tokenize() {
    std::vector<JassToken> tokens;
    while (pos_ < source_.size()) {
        SkipWhitespace();
        char c = Peek();
        if (c == '\0') break;

        // Newlines
        if (c == '\n') {
            Advance();
            tokens.push_back({JassTokenType::NEWLINE, "\\n", 0, line_, column_});
            continue;
        }

        // Comments
        if (c == '/' && (pos_ + 1 < source_.size() && (source_[pos_ + 1] == '/' || source_[pos_ + 1] == '*'))) {
            SkipComment();
            continue;
        }

        // Single-line comments with // not at current position
        // handled by SkipComment above

        // Numbers
        if (std::isdigit(c) || (c == '.' && pos_ + 1 < source_.size() && std::isdigit(source_[pos_ + 1]))) {
            // Check for hex 0x prefix
            if (c == '0' && pos_ + 1 < source_.size() && source_[pos_ + 1] == 'x') {
                Advance(); Advance(); // skip 0x
                JassToken tok;
                tok.line = line_;
                tok.column = column_;
                tok.type = JassTokenType::INTEGER_LITERAL;
                std::string hex;
                while (pos_ < source_.size() && (std::isxdigit(Peek()) || Peek() == '_')) {
                    if (Peek() != '_') hex += Advance();
                    else Advance();
                }
                tok.numberValue = (double)std::stoll(hex, nullptr, 16);
                tok.text = "0x" + hex;
                tokens.push_back(tok);
                continue;
            }
            tokens.push_back(ReadNumber());
            continue;
        }

        // Hex $FF
        if (c == '$') {
            Advance();
            JassToken tok;
            tok.line = line_;
            tok.column = column_;
            tok.type = JassTokenType::INTEGER_LITERAL;
            std::string hex;
            while (pos_ < source_.size() && (std::isxdigit(Peek()) || Peek() == '_')) {
                if (Peek() != '_') hex += Advance();
                else Advance();
            }
            tok.numberValue = (double)std::stoll(hex, nullptr, 16);
            tok.text = "$" + hex;
            tokens.push_back(tok);
            continue;
        }

        // Rawcode 'hfoo'
        if (c == '\'') {
            Advance();
            JassToken tok;
            tok.line = line_;
            tok.column = column_;
            tok.type = JassTokenType::RAWCODE_LITERAL;
            std::string raw;
            while (pos_ < source_.size() && Peek() != '\'') {
                raw += Advance();
            }
            if (Peek() == '\'') Advance();
            tok.text = raw;
            // Convert to integer: each char is a byte, packed big-endian
            int val = 0;
            for (size_t i = 0; i < raw.size() && i < 4; i++) {
                val = (val << 8) | (unsigned char)raw[i];
            }
            tok.numberValue = (double)val;
            tokens.push_back(tok);
            continue;
        }

        // Strings
        if (c == '"') {
            tokens.push_back(ReadString());
            continue;
        }

        // Identifiers and keywords
        if (std::isalpha(c) || c == '_') {
            tokens.push_back(ReadIdentifierOrKeyword());
            continue;
        }

        // Operators and delimiters
        JassToken tok;
        tok.line = line_;
        tok.column = column_;
        tok.text = std::string(1, c);
        Advance();

        switch (c) {
            case '+': tok.type = JassTokenType::OP_PLUS; break;
            case '-': tok.type = JassTokenType::OP_MINUS; break;
            case '*': tok.type = JassTokenType::OP_MUL; break;
            case '/': tok.type = JassTokenType::OP_DIV; break;
            case '(': tok.type = JassTokenType::LPAREN; break;
            case ')': tok.type = JassTokenType::RPAREN; break;
            case '[': tok.type = JassTokenType::LBRACKET; break;
            case ']': tok.type = JassTokenType::RBRACKET; break;
            case ',': tok.type = JassTokenType::COMMA; break;
            case '.': tok.type = JassTokenType::DOT; break;
            case '=': {
                if (Match('=')) { tok.type = JassTokenType::OP_EQ; tok.text = "=="; }
                else tok.type = JassTokenType::OP_ASSIGN;
                break;
            }
            case '!': {
                if (Match('=')) { tok.type = JassTokenType::OP_NEQ; tok.text = "!="; }
                else tok.type = JassTokenType::UNKNOWN;
                break;
            }
            case '<': {
                if (Match('=')) { tok.type = JassTokenType::OP_LE; tok.text = "<="; }
                else tok.type = JassTokenType::OP_LT;
                break;
            }
            case '>': {
                if (Match('=')) { tok.type = JassTokenType::OP_GE; tok.text = ">="; }
                else tok.type = JassTokenType::OP_GT;
                break;
            }
            default: tok.type = JassTokenType::UNKNOWN;
        }
        tokens.push_back(tok);
    }

    tokens.push_back({JassTokenType::EOF_TOKEN, "", 0, line_, column_});
    return tokens;
}
