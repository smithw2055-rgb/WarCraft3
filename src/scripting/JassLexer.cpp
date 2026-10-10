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
    {"endstruct", JassTokenType::KW_ENDSTRUCT},
    {"endmethod", JassTokenType::KW_ENDMETHOD},
    {"endscope", JassTokenType::KW_ENDSCOPE},
    {"endlibrary", JassTokenType::KW_ENDLIBRARY},

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

namespace {

// Split a runtextmacro argument list on top-level commas (paren- and
// quote-aware so nested calls and strings survive).
std::vector<std::string> SplitMacroArgs(const std::string& text) {
    std::vector<std::string> args;
    std::string cur;
    int depth = 0;
    bool inString = false;
    for (size_t i = 0; i < text.size(); i++) {
        const char c = text[i];
        if (inString) {
            cur += c;
            if (c == '"') inString = false;
        } else if (c == '"') {
            inString = true;
            cur += c;
        } else if (c == '(') {
            depth++;
            cur += c;
        } else if (c == ')') {
            if (depth > 0) depth--;
            cur += c;
        } else if (c == ',' && depth == 0) {
            args.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    args.push_back(cur);
    return args;
}

std::string TrimSpaces(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

} // namespace

std::string JassLexer::ExpandTextMacros(const std::string& source) {
    struct Macro {
        std::vector<std::string> params;
        std::vector<std::string> body;
    };
    // Split into lines, keeping terminators for faithful reassembly.
    std::vector<std::string> lines;
    {
        size_t start = 0;
        while (start <= source.size()) {
            size_t end = source.find('\n', start);
            if (end == std::string::npos) {
                lines.push_back(source.substr(start));
                break;
            }
            lines.push_back(source.substr(start, end - start + 1));
            start = end + 1;
        }
    }
    auto isDirective = [](const std::string& line, const char* word) -> bool {
        size_t p = line.find_first_not_of(" \t\r");
        if (p == std::string::npos || line.compare(p, 3, "//!") != 0) return false;
        p = line.find_first_not_of(" \t", p + 3);
        if (p == std::string::npos) return false;
        const size_t len = std::string(word).size();
        if (line.compare(p, len, word) != 0) return false;
        const size_t after = p + len;
        return after >= line.size() || line[after] == ' ' || line[after] == '\t' ||
               line[after] == '\r' || line[after] == '\n' || line[after] == '(';
    };
    auto directiveRest = [](const std::string& line) -> std::string {
        size_t p = line.find_first_not_of(" \t\r");
        p = line.find_first_not_of(" \t", p + 3);
        p = line.find_first_of(" \t(", p);
        if (p == std::string::npos) return "";
        if (line[p] == '(') return line.substr(p);
        return line.substr(line.find_first_not_of(" \t", p));
    };
    // Pass 1: collect definitions, drop their lines.
    std::unordered_map<std::string, Macro> macros;
    std::vector<std::string> code;
    for (size_t i = 0; i < lines.size();) {
        if (isDirective(lines[i], "textmacro")) {
            std::string rest = TrimSpaces(directiveRest(lines[i]));
            // Name, then space/comma separated params.
            std::string name;
            Macro macro;
            {
                size_t p = 0;
                while (p < rest.size() && rest[p] != ' ' && rest[p] != '\t' && rest[p] != ',' &&
                       rest[p] != '\r' && rest[p] != '\n') {
                    name += rest[p++];
                }
                std::string param;
                for (; p <= rest.size(); p++) {
                    const char c = p < rest.size() ? rest[p] : ' ';
                    if (c == ' ' || c == '\t' || c == ',' || c == '\r' || c == '\n') {
                        if (!param.empty()) {
                            macro.params.push_back(param);
                            param.clear();
                        }
                    } else {
                        param += c;
                    }
                }
            }
            i++;
            while (i < lines.size() && !isDirective(lines[i], "endtextmacro")) {
                macro.body.push_back(lines[i]);
                i++;
            }
            if (i < lines.size()) i++; // drop endtextmacro
            if (!name.empty()) macros[name] = std::move(macro);
        } else {
            code.push_back(lines[i]);
            i++;
        }
    }
    // Pass 2: expand invocations (bodies may nest; cap the passes).
    for (int pass = 0; pass < 32; pass++) {
        bool expanded = false;
        std::vector<std::string> out;
        for (const std::string& line : code) {
            if (!isDirective(line, "runtextmacro")) {
                out.push_back(line);
                continue;
            }
            std::string rest = TrimSpaces(directiveRest(line));
            std::string name;
            std::vector<std::string> args;
            {
                size_t p = rest.find('(');
                if (p == std::string::npos) {
                    name = TrimSpaces(rest);
                } else {
                    name = TrimSpaces(rest.substr(0, p));
                    size_t depth = 1;
                    size_t q = p + 1;
                    bool inString = false;
                    while (q < rest.size() && depth > 0) {
                        const char c = rest[q];
                        if (inString) {
                            if (c == '"') inString = false;
                        } else if (c == '"') {
                            inString = true;
                        } else if (c == '(') {
                            depth++;
                        } else if (c == ')') {
                            depth--;
                        }
                        q++;
                    }
                    const std::string inner = rest.substr(p + 1, (q - 1) - (p + 1));
                    if (!TrimSpaces(inner).empty() || inner.find(',') != std::string::npos) {
                        args = SplitMacroArgs(inner);
                        for (auto& a : args) a = TrimSpaces(a);
                    }
                }
            }
            auto mit = macros.find(name);
            if (mit == macros.end()) {
                out.push_back(line); // unknown macro: stays a comment
                continue;
            }
            expanded = true;
            for (std::string bodyLine : mit->second.body) {
                for (size_t pi = 0; pi < mit->second.params.size(); pi++) {
                    const std::string marker = "$" + mit->second.params[pi] + "$";
                    const std::string& value = pi < args.size() ? args[pi] : "";
                    size_t at = 0;
                    while ((at = bodyLine.find(marker, at)) != std::string::npos) {
                        bodyLine.replace(at, marker.size(), value);
                        at += value.size();
                    }
                }
                out.push_back(bodyLine);
            }
        }
        code.swap(out);
        if (!expanded) break;
    }
    std::string result;
    for (const auto& line : code) result += line;
    return result;
}
