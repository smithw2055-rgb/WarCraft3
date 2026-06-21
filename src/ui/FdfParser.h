#pragma once
#include "UiTypes.h"
#include <string>
#include <vector>
#include <memory>

class FdfParser {
public:
    FdfParser(const std::string& content);

    FrameDefinition Parse();

    const std::map<std::string, std::string>& GetDecoratedStrings() const { return decoratedStrings_; }
    const std::map<std::string, FrameDefinition>& GetTemplates() const { return templates_; }

private:
    struct Token {
        enum Type {
            STRING_LIST, INCLUDE_FILE, FRAME, STRING, TEXTURE, LAYER,
            INHERITS, WITHCHILDREN, SETPOINT, ANCHOR, MENUITEM,
            JUSTIFYTOP, JUSTIFYMIDDLE, JUSTIFYBOTTOM,
            JUSTIFYLEFT, JUSTIFYCENTER, JUSTIFYRIGHT,
            FRAMEPOINT_TOPLEFT, FRAMEPOINT_TOP, FRAMEPOINT_TOPRIGHT,
            FRAMEPOINT_LEFT, FRAMEPOINT_CENTER, FRAMEPOINT_RIGHT,
            FRAMEPOINT_BOTTOMLEFT, FRAMEPOINT_BOTTOM, FRAMEPOINT_BOTTOMRIGHT,
            ID, STRING_LITERAL, FLOAT, COMMA, OPEN_CURLY, CLOSE_CURLY,
            EOF_TOKEN, FLAG
        };
        Type type;
        std::string text;
        float floatVal = 0;
    };

    void Lex();
    Token Peek() const;
    Token Advance();
    Token Expect(Token::Type type);
    bool Match(Token::Type type);
    float ParseFloat();
    std::string ParseString();
    FramePoint ParseFramePoint();
    void ParseStringList();
    void ParseInclude();
    FrameDefinition ParseFrame();

    std::string input_;
    size_t pos_ = 0;
    std::vector<Token> tokens_;
    size_t tokenPos_ = 0;

    std::map<std::string, std::string> decoratedStrings_;
    std::map<std::string, FrameDefinition> templates_;

    static std::string Unquote(const std::string& s);
};
