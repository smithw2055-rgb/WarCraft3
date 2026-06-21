#include "FdfParser.h"
#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <iostream>

FdfParser::FdfParser(const std::string& content) : input_(content) {}

std::string FdfParser::Unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

static bool isIdStart(char c) { return std::isalpha(c) || c == '_'; }
static bool isIdCont(char c) { return std::isalnum(c) || c == '_'; }

void FdfParser::Lex() {
    tokens_.clear();
    size_t i = 0;
    auto skipWS = [&]() {
        while (i < input_.size() && (input_[i] == ' ' || input_[i] == '\t' || input_[i] == '\r' || input_[i] == '\n'))
            i++;
    };
    while (i < input_.size()) {
        skipWS();
        if (i >= input_.size()) break;

        // BOM skip
        if ((unsigned char)input_[i] == 0xEF && i + 2 < input_.size() &&
            (unsigned char)input_[i+1] == 0xBB && (unsigned char)input_[i+2] == 0xBF) {
            i += 3;
            continue;
        }

        // Single-line comment
        if (i + 1 < input_.size() && input_[i] == '/' && input_[i+1] == '/') {
            while (i < input_.size() && input_[i] != '\n') i++;
            continue;
        }
        // Multi-line comment
        if (i + 1 < input_.size() && input_[i] == '/' && input_[i+1] == '*') {
            i += 2;
            while (i + 1 < input_.size() && !(input_[i] == '*' && input_[i+1] == '/')) i++;
            if (i + 1 < input_.size()) i += 2;
            continue;
        }

        char c = input_[i];
        Token tok;

        if (c == '{') { tok = { Token::OPEN_CURLY, "{" }; i++; }
        else if (c == '}') { tok = { Token::CLOSE_CURLY, "}" }; i++; }
        else if (c == ',') { tok = { Token::COMMA, "," }; i++; }
        else if (c == '"') {
            size_t start = i;
            i++;
            while (i < input_.size() && input_[i] != '"') {
                if (input_[i] == '\\') i++; // skip escape
                i++;
            }
            if (i < input_.size()) i++;
            tok = { Token::STRING_LITERAL, input_.substr(start, i - start) };
        }
        else if (c == '-' || std::isdigit(c)) {
            size_t start = i;
            if (c == '-') i++;
            while (i < input_.size() && (std::isdigit(input_[i]) || input_[i] == '.')) i++;
            if (i < input_.size() && (input_[i] == 'f' || input_[i] == 'F')) i++;
            std::string numstr = input_.substr(start, i - start);
            tok = { Token::FLOAT, numstr, (float)std::atof(numstr.c_str()) };
        }
        else if (isIdStart(c)) {
            size_t start = i;
            while (i < input_.size() && isIdCont(input_[i])) i++;
            std::string word = input_.substr(start, i - start);
            if (word == "StringList")         tok = { Token::STRING_LIST, word };
            else if (word == "IncludeFile")   tok = { Token::INCLUDE_FILE, word };
            else if (word == "Frame")         tok = { Token::FRAME, word };
            else if (word == "String")        tok = { Token::STRING, word };
            else if (word == "Texture")       tok = { Token::TEXTURE, word };
            else if (word == "Layer")         tok = { Token::LAYER, word };
            else if (word == "INHERITS")      tok = { Token::INHERITS, word };
            else if (word == "WITHCHILDREN")  tok = { Token::WITHCHILDREN, word };
            else if (word == "SetPoint")      tok = { Token::SETPOINT, word };
            else if (word == "Anchor")        tok = { Token::ANCHOR, word };
            else if (word == "MenuItem")      tok = { Token::MENUITEM, word };
            else if (word == "JUSTIFYTOP")    tok = { Token::JUSTIFYTOP, word };
            else if (word == "JUSTIFYMIDDLE") tok = { Token::JUSTIFYMIDDLE, word };
            else if (word == "JUSTIFYBOTTOM") tok = { Token::JUSTIFYBOTTOM, word };
            else if (word == "JUSTIFYLEFT")   tok = { Token::JUSTIFYLEFT, word };
            else if (word == "JUSTIFYCENTER") tok = { Token::JUSTIFYCENTER, word };
            else if (word == "JUSTIFYRIGHT")  tok = { Token::JUSTIFYRIGHT, word };
            else if (word == "TOPLEFT")       tok = { Token::FRAMEPOINT_TOPLEFT, word };
            else if (word == "TOP")           tok = { Token::FRAMEPOINT_TOP, word };
            else if (word == "TOPRIGHT")      tok = { Token::FRAMEPOINT_TOPRIGHT, word };
            else if (word == "LEFT")          tok = { Token::FRAMEPOINT_LEFT, word };
            else if (word == "CENTER")        tok = { Token::FRAMEPOINT_CENTER, word };
            else if (word == "RIGHT")         tok = { Token::FRAMEPOINT_RIGHT, word };
            else if (word == "BOTTOMLEFT")    tok = { Token::FRAMEPOINT_BOTTOMLEFT, word };
            else if (word == "BOTTOM")        tok = { Token::FRAMEPOINT_BOTTOM, word };
            else if (word == "BOTTOMRIGHT")   tok = { Token::FRAMEPOINT_BOTTOMRIGHT, word };
            else                              tok = { Token::ID, word };
        }
        else {
            i++;
            continue;
        }
        tokens_.push_back(tok);
    }
    tokens_.push_back({ Token::EOF_TOKEN, "" });
}

FdfParser::Token FdfParser::Peek() const {
    return tokens_[tokenPos_];
}

FdfParser::Token FdfParser::Advance() {
    return tokens_[tokenPos_++];
}

FdfParser::Token FdfParser::Expect(Token::Type type) {
    Token t = Advance();
    if (t.type != type) {
        throw std::runtime_error("FDF: expected token type " + std::to_string(type) +
                                 " but got '" + t.text + "' at pos " + std::to_string(tokenPos_));
    }
    return t;
}

bool FdfParser::Match(Token::Type type) {
    if (Peek().type == type) {
        Advance();
        return true;
    }
    return false;
}

float FdfParser::ParseFloat() {
    return Expect(Token::FLOAT).floatVal;
}

std::string FdfParser::ParseString() {
    return Unquote(Expect(Token::STRING_LITERAL).text);
}

FramePoint FdfParser::ParseFramePoint() {
    Token t = Advance();
    switch (t.type) {
        case Token::FRAMEPOINT_TOPLEFT:     return FramePoint::TOPLEFT;
        case Token::FRAMEPOINT_TOP:         return FramePoint::TOP;
        case Token::FRAMEPOINT_TOPRIGHT:    return FramePoint::TOPRIGHT;
        case Token::FRAMEPOINT_LEFT:        return FramePoint::LEFT;
        case Token::FRAMEPOINT_CENTER:      return FramePoint::CENTER;
        case Token::FRAMEPOINT_RIGHT:       return FramePoint::RIGHT;
        case Token::FRAMEPOINT_BOTTOMLEFT:  return FramePoint::BOTTOMLEFT;
        case Token::FRAMEPOINT_BOTTOM:      return FramePoint::BOTTOM;
        case Token::FRAMEPOINT_BOTTOMRIGHT: return FramePoint::BOTTOMRIGHT;
        default:
            throw std::runtime_error("FDF: expected frame point, got '" + t.text + "'");
    }
}

void FdfParser::ParseStringList() {
    Expect(Token::OPEN_CURLY);
    while (Peek().type == Token::ID) {
        std::string id = Advance().text;
        std::string value = ParseString();
        Expect(Token::COMMA);
        decoratedStrings_[id] = value;
    }
    Expect(Token::CLOSE_CURLY);
}

void FdfParser::ParseInclude() {
    std::string path = ParseString();
    Expect(Token::COMMA);
    // File include - would need external file loading
    // For now, just record the path
    (void)path;
}

FrameDefinition FdfParser::ParseFrame() {
    // Determine frame class from type qualifier
    Token qualTok = Peek();
    FrameClass frameClass;
    bool isFrameKeyword = false;

    if (qualTok.type == Token::FRAME) {
        frameClass = FrameClass::Frame;
        isFrameKeyword = true;
        Advance();
    } else if (qualTok.type == Token::STRING) {
        frameClass = FrameClass::String;
        Advance();
    } else if (qualTok.type == Token::TEXTURE) {
        frameClass = FrameClass::Texture;
        Advance();
    } else if (qualTok.type == Token::LAYER) {
        frameClass = FrameClass::Layer;
        Advance();
    } else {
        throw std::runtime_error("FDF: expected frame type qualifier, got '" + qualTok.text + "'");
    }

    // Check for inheritance pattern
    bool inherits = false;
    bool withChildren = false;
    std::string name;

    if (isFrameKeyword) {
        // Frame "type" "name" ...
        std::string type = ParseString();
        name = ParseString();
        FrameDefinition def(frameClass, type, name);

        if (Match(Token::INHERITS)) {
            inherits = true;
            if (Match(Token::WITHCHILDREN)) {
                withChildren = true;
            }
            std::string parentName = ParseString();
            auto it = templates_.find(parentName);
            if (it != templates_.end()) {
                def.InheritFrom(it->second, withChildren);
            } else {
                std::cerr << "FDF: template '" << parentName << "' not found for inheritance" << std::endl;
            }
        }

        Expect(Token::OPEN_CURLY);
        while (Peek().type != Token::CLOSE_CURLY && Peek().type != Token::EOF_TOKEN) {
            Token t = Peek();

            if (t.type == Token::STRING || t.type == Token::TEXTURE || t.type == Token::LAYER ||
                t.type == Token::FRAME) {
                def.innerFrames.push_back(ParseFrame());
            }
            else if (t.type == Token::SETPOINT) {
                Advance();
                FramePoint myPoint = ParseFramePoint();
                Expect(Token::COMMA);
                std::string other = ParseString();
                Expect(Token::COMMA);
                FramePoint otherPoint = ParseFramePoint();
                Expect(Token::COMMA);
                float x = ParseFloat();
                Expect(Token::COMMA);
                float y = ParseFloat();
                Expect(Token::COMMA);
                def.setPoints.push_back({ myPoint, other, otherPoint, x, y });
            }
            else if (t.type == Token::ANCHOR) {
                Advance();
                FramePoint pt = ParseFramePoint();
                Expect(Token::COMMA);
                float x = ParseFloat();
                Expect(Token::COMMA);
                float y = ParseFloat();
                Expect(Token::COMMA);
                def.anchors.push_back({ pt, x, y });
            }
            else if (t.type == Token::ID) {
                std::string id = Advance().text;
                Token next = Peek();

                if (next.type == Token::FLOAT) {
                    // float or vector or font
                    float v1 = ParseFloat();
                    if (Match(Token::COMMA)) {
                        // Single float value
                        def.floatFields[id] = v1;
                    } else {
                        // Need more tokens but next is comma already consumed
                        // Vector2: ID FLOAT FLOAT COMMA
                        if (Peek().type == Token::FLOAT) {
                            float v2 = ParseFloat();
                            if (Match(Token::COMMA)) {
                                def.vector2Fields[id] = { v1, v2 };
                            }
                            // Could be font: ID STRING_LITERAL COMMA FLOAT COMMA
                        }
                        else if (Peek().type == Token::STRING_LITERAL) {
                            // Font: ID STRING_LITERAL COMMA FLOAT COMMA
                            std::string fontName = ParseString();
                            Expect(Token::COMMA);
                            // Actually: ID FLOAT COMMA STRING_LITERAL COMMA - need reparse
                            // This was parsed as: ID FLOAT, then we're here
                        }
                    }
                }
                else if (next.type == Token::STRING_LITERAL) {
                    // String or string-pair
                    std::string s1 = ParseString();
                    if (Match(Token::COMMA)) {
                        if (Peek().type == Token::STRING_LITERAL) {
                            std::string s2 = ParseString();
                            Expect(Token::COMMA);
                            def.stringFields[id] = s1; // store first, the pair would need special handling
                        } else {
                            // Actually, the string already consumed the comma? No, ParseString reads the string literal.
                            // After ParseString returns, we need to check if there's a second string.
                            // Wait, the issue: ID STRING_LITERAL COMMA -> single string
                            // ID STRING_LITERAL STRING_LITERAL COMMA -> string pair
                            // After ParseString() (which consumed a STRING_LITERAL)
                            def.stringFields[id] = s1;
                        }
                    }
                }
                else if (next.type == Token::COMMA) {
                    // Flag
                    Advance(); // consume comma
                    def.flags.insert(id);
                }
                else {
                    throw std::runtime_error("FDF: unexpected token after ID '" + id + "': " + next.text);
                }
            }
            else {
                break;
            }
        }
        Expect(Token::CLOSE_CURLY);
        templates_[name] = def;
        return def;
    }
    else {
        // String/Texture/Layer "name"? { ... }
        if (Peek().type == Token::STRING_LITERAL) {
            name = ParseString();
        }
        FrameDefinition def(frameClass, "", name);

        if (Match(Token::INHERITS)) {
            inherits = true;
            if (Match(Token::WITHCHILDREN)) {
                withChildren = true;
            }
            std::string parentName = ParseString();
            auto it = templates_.find(parentName);
            if (it != templates_.end()) {
                def.InheritFrom(it->second, withChildren);
            }
        }

        Expect(Token::OPEN_CURLY);
        while (Peek().type != Token::CLOSE_CURLY && Peek().type != Token::EOF_TOKEN) {
            // Similar element parsing as above
            Token t = Peek();
            if (t.type == Token::STRING || t.type == Token::TEXTURE || t.type == Token::LAYER ||
                t.type == Token::FRAME) {
                def.innerFrames.push_back(ParseFrame());
            }
            else if (t.type == Token::SETPOINT) {
                Advance();
                FramePoint myPoint = ParseFramePoint();
                Expect(Token::COMMA);
                std::string other = ParseString();
                Expect(Token::COMMA);
                FramePoint otherPoint = ParseFramePoint();
                Expect(Token::COMMA);
                float x = ParseFloat();
                Expect(Token::COMMA);
                float y = ParseFloat();
                Expect(Token::COMMA);
                def.setPoints.push_back({ myPoint, other, otherPoint, x, y });
            }
            else if (t.type == Token::ANCHOR) {
                Advance();
                FramePoint pt = ParseFramePoint();
                Expect(Token::COMMA);
                float x = ParseFloat();
                Expect(Token::COMMA);
                float y = ParseFloat();
                Expect(Token::COMMA);
                def.anchors.push_back({ pt, x, y });
            }
            else if (t.type == Token::ID) {
                std::string id = Advance().text;
                Token next = Peek();
                if (next.type == Token::FLOAT) {
                    float val = ParseFloat();
                    Match(Token::COMMA);
                    def.floatFields[id] = val;
                }
                else if (next.type == Token::STRING_LITERAL) {
                    std::string val = ParseString();
                    Match(Token::COMMA);
                    def.stringFields[id] = val;
                }
                else if (next.type == Token::COMMA) {
                    Advance();
                    def.flags.insert(id);
                }
                else {
                    throw std::runtime_error("FDF: unexpected after ID: " + next.text);
                }
            }
            else {
                break;
            }
        }
        Expect(Token::CLOSE_CURLY);
        if (!name.empty()) {
            templates_[name] = def;
        }
        return def;
    }
}

FrameDefinition FdfParser::Parse() {
    Lex();

    while (Peek().type != Token::EOF_TOKEN) {
        Token t = Peek();
        if (t.type == Token::STRING_LIST) {
            Advance();
            ParseStringList();
        } else if (t.type == Token::INCLUDE_FILE) {
            Advance();
            ParseInclude();
        } else if (t.type == Token::FRAME || t.type == Token::STRING ||
                   t.type == Token::TEXTURE || t.type == Token::LAYER) {
            ParseFrame();
        } else {
            break;
        }
    }

    return FrameDefinition();
}
