// MDL text model parser: tokenizes Blizzard MDL and fills the shared
// MdxModel representation (same structs the MDX binary loader targets,
// mirroring ref's mdlx model layer). Unknown blocks and fields are
// skipped so variant files still load; hard syntax errors fail with a
// line-anchored diagnostic.
#include "mdl.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include "model/mdx.h"

namespace {

constexpr uint32_t MakeTag(char a, char b, char c, char d) {
    return ((uint32_t)(uint8_t)a << 24) | ((uint32_t)(uint8_t)b << 16) |
           ((uint32_t)(uint8_t)c << 8) | (uint32_t)(uint8_t)d;
}

// ---- Lexer ----
struct Token {
    enum Kind { End, Word, Number, String, LBrace, RBrace, Comma, Colon } kind = End;
    std::string text;
    double number = 0;
    int line = 1;
};

class Lexer {
public:
    Lexer(const char* text, size_t size) : p_(text), end_(text + size) {}

    Token peek() { return peekAt(0); }
    Token peek2() { return peekAt(1); }

    Token next() {
        if (!lookahead_.empty()) {
            Token t = lookahead_.front();
            lookahead_.erase(lookahead_.begin());
            return t;
        }
        return nextRaw();
    }

    int line() const { return line_; }

private:
    Token peekAt(size_t n) {
        while (lookahead_.size() <= n) lookahead_.push_back(nextRaw());
        return lookahead_[n];
    }
    void skipSpace() {
        while (p_ < end_) {
            if (*p_ == '/' && p_ + 1 < end_ && p_[1] == '/') {
                while (p_ < end_ && *p_ != '\n') p_++;
            } else if (*p_ == '\n') {
                line_++;
                p_++;
            } else if (*p_ == ' ' || *p_ == '\t' || *p_ == '\r' ||
                       *p_ == '\f' || *p_ == '\v') {
                p_++;
            } else {
                break;
            }
        }
    }

    Token nextRaw() {
        skipSpace();
        Token t;
        t.line = line_;
        if (p_ >= end_) return t;
        char c = *p_;
        if (c == '{') { p_++; t.kind = Token::LBrace; return t; }
        if (c == '}') { p_++; t.kind = Token::RBrace; return t; }
        if (c == ',') { p_++; t.kind = Token::Comma; return t; }
        if (c == ':') { p_++; t.kind = Token::Colon; return t; }
        if (c == '"') {
            p_++;
            std::string s;
            while (p_ < end_ && *p_ != '"') {
                if (*p_ == '\\' && p_ + 1 < end_) {
                    p_++;
                    s += *p_++;
                } else {
                    if (*p_ == '\n') line_++;
                    s += *p_++;
                }
            }
            if (p_ < end_) p_++; // closing quote
            t.kind = Token::String;
            t.text = s;
            return t;
        }
        // Number: [+-]? (digits[.digits][e[+-]digits] | .digits)
        const char* start = p_;
        const char* q = p_;
        if (q < end_ && (*q == '+' || *q == '-')) q++;
        bool hasDigits = false;
        while (q < end_ && std::isdigit((unsigned char)*q)) { q++; hasDigits = true; }
        if (q < end_ && *q == '.') {
            q++;
            while (q < end_ && std::isdigit((unsigned char)*q)) { q++; hasDigits = true; }
        }
        if (hasDigits && q < end_ && (*q == 'e' || *q == 'E')) {
            const char* eq = q + 1;
            if (eq < end_ && (*eq == '+' || *eq == '-')) eq++;
            if (eq < end_ && std::isdigit((unsigned char)*eq)) {
                q = eq + 1;
                while (q < end_ && std::isdigit((unsigned char)*q)) q++;
            }
        }
        if (hasDigits && (q > start + (start < end_ && (*start == '+' || *start == '-') ? 1 : 0))) {
            t.kind = Token::Number;
            t.text.assign(start, q);
            t.number = std::strtod(t.text.c_str(), nullptr);
            p_ = q;
            return t;
        }
        // Word: until whitespace or punctuation.
        while (p_ < end_ && *p_ != '{' && *p_ != '}' && *p_ != ',' && *p_ != ':' &&
               *p_ != '"' && *p_ != ' ' && *p_ != '\t' && *p_ != '\r' && *p_ != '\n' &&
               *p_ != '\f' && *p_ != '\v') {
            // A '/' starts a comment only as '//'; single '/' ends words here
            // so comment skipping stays consistent.
            if (*p_ == '/' && p_ + 1 < end_ && p_[1] == '/') break;
            if (*p_ == '/') { p_++; break; }
            p_++;
        }
        // Rewind the peculiar single-'/' advance above (kept simple: words
        // never contain '/'; the char is left for the next token).
        t.kind = Token::Word;
        t.text.assign(start, p_);
        if (!t.text.empty() && t.text.back() == '/') {
            t.text.pop_back();
            p_--;
        }
        return t;
    }

    const char* p_;
    const char* end_;
    int line_ = 1;
    std::vector<Token> lookahead_;
};

// ---- Parser ----
struct Parser {
    Lexer lx;
    std::string* error = nullptr;
    bool failed = false;

    Parser(const char* text, size_t size, std::string* err)
        : lx(text, size), error(err) {}

    bool fail(const std::string& message) {
        if (!failed) {
            failed = true;
            if (error) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "line %d: ", lx.line());
                *error = std::string(buf) + message;
            }
        }
        return false;
    }

    Token peek() { return lx.peek(); }
    Token next() { return lx.next(); }

    bool expect(Token::Kind kind, const char* what) {
        Token t = next();
        if (t.kind != kind) return fail(std::string("expected ") + what);
        return true;
    }

    bool expectWord(const std::string& word) {
        Token t = next();
        if (t.kind != Token::Word || t.text != word) {
            return fail("expected '" + word + "'");
        }
        return true;
    }

    // Reads a number token (int or float text).
    bool number(double& out) {
        Token t = next();
        if (t.kind != Token::Number) return fail("expected number");
        out = t.number;
        return true;
    }

    bool integer(int& out) {
        double v = 0;
        if (!number(v)) return false;
        out = (int)std::llround(v);
        return true;
    }

    bool floatValue(float& out) {
        double v = 0;
        if (!number(v)) return false;
        out = (float)v;
        return true;
    }

    bool comma() { return expect(Token::Comma, "','"); }
    bool lbrace() { return expect(Token::LBrace, "'{'"); }
    bool rbrace() { return expect(Token::RBrace, "'}'"); }

    // Skips one value: a braced block (balanced), a quoted string, or a
    // bare word/number run up to the next comma or closing brace.
    bool skipValue() {
        Token t = peek();
        if (t.kind == Token::LBrace) {
            next();
            int depth = 1;
            while (depth > 0) {
                Token u = next();
                if (u.kind == Token::End) return fail("unexpected end in block");
                if (u.kind == Token::LBrace) depth++;
                if (u.kind == Token::RBrace) depth--;
            }
            if (peek().kind == Token::Comma) next();
            return true;
        }
        // Bare value: consume until comma/RBrace/End (strings already whole).
        while (true) {
            Token u = peek();
            if (u.kind == Token::Comma) { next(); return true; }
            if (u.kind == Token::RBrace || u.kind == Token::End) return true;
            if (u.kind == Token::LBrace) return skipValue(); // nested block value
            next();
        }
    }

    // Skips `{ ... }` plus an optional trailing comma. The opening brace
    // must be the next token.
    bool skipBlock() {
        if (!lbrace()) return false;
        int depth = 1;
        while (depth > 0) {
            Token u = next();
            if (u.kind == Token::End) return fail("unexpected end in block");
            if (u.kind == Token::LBrace) depth++;
            if (u.kind == Token::RBrace) depth--;
        }
        if (peek().kind == Token::Comma) next();
        return true;
    }

    bool parseFloatTriple(float out[3]) {
        if (!lbrace()) return false;
        for (int i = 0; i < 3; i++) {
            if (!floatValue(out[i])) return false;
            if (peek().kind == Token::Comma) next();
            else if (i < 2) return fail("expected ','");
        }
        return rbrace();
    }

    bool parseFloatPair(float out[2]) {
        if (!lbrace()) return false;
        for (int i = 0; i < 2; i++) {
            if (!floatValue(out[i])) return false;
            if (peek().kind == Token::Comma) next();
            else if (i < 1) return fail("expected ','");
        }
        return rbrace();
    }
};

int timelineElemSize(uint32_t tag) {
    switch (tag) {
        case MakeTag('K','G','T','R'):
        case MakeTag('K','C','T','R'):
        case MakeTag('K','T','T','R'):
        case MakeTag('K','G','S','C'):
        case MakeTag('K','T','A','S'):
        case MakeTag('K','T','A','T'):
        case MakeTag('K','G','A','C'):
        case MakeTag('K','L','A','C'):
        case MakeTag('K','L','B','C'):
        case MakeTag('K','P','P','C'):
        case MakeTag('K','R','C','O'):
        case MakeTag('K','F','C','3'):
            return 3;
        case MakeTag('K','G','R','T'):
        case MakeTag('K','T','A','R'):
            return 4;
        default:
            return 1;
    }
}

bool isIntegerTimeline(uint32_t tag) {
    return tag == MakeTag('K','M','T','F') || tag == MakeTag('K','R','T','X') ||
           tag == MakeTag('K','F','T','C');
}

MdxInterpolation parseInterpWord(const std::string& word, bool& known) {
    known = true;
    if (word == "DontInterp") return MdxInterpolation::None;
    if (word == "Linear") return MdxInterpolation::Linear;
    if (word == "Hermite") return MdxInterpolation::Hermite;
    if (word == "Bezier") return MdxInterpolation::Bezier;
    known = false;
    return MdxInterpolation::None;
}

// Parses an animated track body (count + brace already consumed by the
// caller pattern below): [interp,] [GlobalSeqId N,] frame: {...} ...
// Appends one MdxTimeline with the given tag. KEVT carries frames only.
bool parseTrackBody(Parser& p, uint32_t tag, std::vector<MdxTimeline>& out,
                    bool swapRB) {
    MdxTimeline tl;
    tl.tag = tag;
    tl.interpolationType = MdxInterpolation::None;
    tl.globalSequenceId = -1;
    const int elemSize = (tag == MakeTag('K','E','V','T')) ? 1 : timelineElemSize(tag);
    const bool integerTimeline = isIntegerTimeline(tag);

    if (!p.lbrace()) return false;
    // Optional interpolation word and GlobalSeqId, in either order, at most once each.
    for (int i = 0; i < 2; i++) {
        Token t = p.peek();
        if (t.kind != Token::Word) break;
        bool known = false;
        MdxInterpolation interp = parseInterpWord(t.text, known);
        if (known) {
            p.next();
            tl.interpolationType = interp;
            if (p.peek().kind == Token::Comma) p.next();
            continue;
        }
        if (t.text == "GlobalSeqId") {
            p.next();
            int gid = -1;
            if (!p.integer(gid)) return false;
            tl.globalSequenceId = (gid < 0 || (uint32_t)gid >= 0xFFFFFF00u) ? -1 : gid;
            if (p.peek().kind == Token::Comma) p.next();
            continue;
        }
        break;
    }

    if (tag == MakeTag('K','E','V','T')) {
        while (p.peek().kind != Token::RBrace) {
            if (p.peek().kind == Token::End) return p.fail("unexpected end in event track");
            int frame = 0;
            if (!p.integer(frame)) return false;
            tl.frames.push_back((uint32_t)frame);
            if (p.peek().kind == Token::Comma) p.next();
        }
        if (!p.rbrace()) return false;
        out.push_back(std::move(tl));
        return true;
    }

    const bool tangential = tl.interpolationType == MdxInterpolation::Hermite ||
                            tl.interpolationType == MdxInterpolation::Bezier;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in track");
        int frame = 0;
        if (!p.integer(frame)) return false;
        if (!p.expect(Token::Colon, "':'")) return false;
        if (!p.lbrace()) return false;
        tl.frames.push_back((uint32_t)frame);
        for (int e = 0; e < elemSize; e++) {
            double v = 0;
            if (!p.number(v)) return false;
            float f = integerTimeline ? (float)(int)std::llround(v) : (float)v;
            tl.values.push_back(f);
            if (p.peek().kind == Token::Comma) p.next();
        }
        if (tangential) {
            if (!p.expectWord("InTan")) return false;
            if (!p.lbrace()) return false;
            for (int e = 0; e < elemSize; e++) {
                double v = 0;
                if (!p.number(v)) return false;
                tl.inTans.push_back((float)v);
                if (p.peek().kind == Token::Comma) p.next();
            }
            if (!p.rbrace()) return false;
            if (p.peek().kind == Token::Comma) p.next();
            if (!p.expectWord("OutTan")) return false;
            if (!p.lbrace()) return false;
            for (int e = 0; e < elemSize; e++) {
                double v = 0;
                if (!p.number(v)) return false;
                tl.outTans.push_back((float)v);
                if (p.peek().kind == Token::Comma) p.next();
            }
            if (!p.rbrace()) return false;
            if (p.peek().kind == Token::Comma) p.next();
        }
        if (!p.rbrace()) return false;
        if (p.peek().kind == Token::Comma) p.next();
    }
    if (!p.rbrace()) return false;
    if (swapRB && elemSize == 3) {
        auto swapTriples = [](std::vector<float>& v) {
            for (size_t i = 0; i + 2 < v.size(); i += 3) std::swap(v[i], v[i + 2]);
        };
        swapTriples(tl.values);
        swapTriples(tl.inTans);
        swapTriples(tl.outTans);
    }
    out.push_back(std::move(tl));
    return true;
}

// ---- Shared node header ----
// Consumes: "Name" already read by the caller is passed in; parses
// ObjectId/Parent plus optional GeosetId/GeosetAnimId and flag words.
// Unknown words are left for the caller's track/static dispatch.
struct NodeHead {
    std::string name;
    int objectId = -1;
    int parentId = -1;
    int geosetId = -1;
    int geosetAnimId = -1;
    uint32_t flags = 0;
};

constexpr uint32_t NODE_DONT_INHERIT_TRANSLATION = 0x1;
constexpr uint32_t NODE_DONT_INHERIT_SCALING = 0x2;
constexpr uint32_t NODE_DONT_INHERIT_ROTATION = 0x4;
constexpr uint32_t NODE_BILLBOARDED = 0x8;
constexpr uint32_t NODE_BILLBOARDED_LOCK_X = 0x10;
constexpr uint32_t NODE_BILLBOARDED_LOCK_Y = 0x20;
constexpr uint32_t NODE_BILLBOARDED_LOCK_Z = 0x40;

bool parseNodeFlagWord(const std::string& word, uint32_t& flags) {
    if (word == "DontInheritTranslation") { flags |= NODE_DONT_INHERIT_TRANSLATION; return true; }
    if (word == "DontInheritScaling") { flags |= NODE_DONT_INHERIT_SCALING; return true; }
    if (word == "DontInheritRotation") { flags |= NODE_DONT_INHERIT_ROTATION; return true; }
    if (word == "Billboarded") { flags |= NODE_BILLBOARDED; return true; }
    if (word == "BillboardedLockX") { flags |= NODE_BILLBOARDED_LOCK_X; return true; }
    if (word == "BillboardedLockY") { flags |= NODE_BILLBOARDED_LOCK_Y; return true; }
    if (word == "BillboardedLockZ") { flags |= NODE_BILLBOARDED_LOCK_Z; return true; }
    return false;
}

bool parseNodeHead(Parser& p, NodeHead& head, bool withGeosetIds) {
    // ObjectId <int>, Parent <int>,
    if (!p.expectWord("ObjectId")) return false;
    if (!p.integer(head.objectId)) return false;
    if (!p.comma()) return false;
    if (!p.expectWord("Parent")) return false;
    if (!p.integer(head.parentId)) return false;
    if (!p.comma()) return false;
    if (withGeosetIds) {
        // Optional GeosetId / GeosetAnimId (bone entries carry them).
        for (int i = 0; i < 2; i++) {
            Token t = p.peek();
            if (t.kind != Token::Word) break;
            if (t.text == "GeosetId") {
                p.next();
                if (!p.integer(head.geosetId)) return false;
                if (!p.comma()) return false;
            } else if (t.text == "GeosetAnimId") {
                p.next();
                if (!p.integer(head.geosetAnimId)) return false;
                if (!p.comma()) return false;
            } else {
                break;
            }
        }
    }
    // Flag words.
    while (p.peek().kind == Token::Word && parseNodeFlagWord(p.peek().text, head.flags)) {
        p.next();
        if (p.peek().kind == Token::Comma) p.next();
    }
    return true;
}

void applyNodeHead(const NodeHead& head, MdxNode& node) {
    node.name = head.name;
    node.objectId = head.objectId;
    node.parentId = head.parentId;
    node.flags = head.flags;
    node.geosetId = head.geosetId;
    node.geosetAnimationId = head.geosetAnimId;
}

// Peek helpers for the `static X ...` vs `X N {...}` dispatch used by
// nearly every animated field. Neither consumes on mismatch.
bool peekWord(Parser& p, const std::string& word) {
    Token t = p.peek();
    return t.kind == Token::Word && t.text == word;
}

// Effective field keyword: `static X` reports X so `static`-prefixed
// statics dispatch to the same branch (probeField consumes the prefix).
std::string fieldKeyword(Parser& p) {
    Token t = p.peek();
    if (t.kind != Token::Word) return "";
    if (t.text != "static") return t.text;
    Token t2 = p.lx.peek2();
    if (t2.kind == Token::Word) return t2.text;
    return "static";
}


// Parses `Keyword count { ... }` animated-track form into out with the
// given tag. Consumes the count token. swapRB swaps R/B triples (KGAC).
bool parseCountedTrack(Parser& p, const std::string& keyword, uint32_t tag,
                       std::vector<MdxTimeline>& out, bool swapRB = false) {
    if (!p.expectWord(keyword)) return false;
    double count = 0;
    if (!p.number(count)) return false;
    (void)count;
    const int elemSize = (tag == MakeTag('K','E','V','T')) ? 1 : timelineElemSize(tag);
    (void)elemSize;
    return parseTrackBody(p, tag, out, swapRB);
}

// Translation/Rotation/Scaling node tracks (KGTR/KGRT/KGSC). A `static`
// triple degrades to a 1-key track at frame 0 (nodes carry no base
// transform; the pivot derivation below reads frame 0 the same way).
bool parseNodeTransformTracks(Parser& p, const std::string& keyword,
                              std::vector<MdxTimeline>& out) {
    uint32_t tag = 0;
    int elemSize = 3;
    if (keyword == "Translation") tag = MakeTag('K','G','T','R');
    else if (keyword == "Rotation") {
        tag = MakeTag('K','G','R','T');
        elemSize = 4;
    } else if (keyword == "Scaling") tag = MakeTag('K','G','S','C');
    else return false;
    if (peekWord(p, "static")) {
        p.next();
        if (!p.expectWord(keyword)) return false;
        if (!p.lbrace()) return false;
        MdxTimeline tl;
        tl.tag = tag;
        tl.frames.push_back(0);
        for (int e = 0; e < elemSize; e++) {
            double v = 0;
            if (!p.number(v)) return false;
            tl.values.push_back((float)v);
            if (p.peek().kind == Token::Comma) p.next();
        }
        if (!p.rbrace()) return false;
        if (p.peek().kind == Token::Comma) p.next();
        out.push_back(std::move(tl));
        return true;
    }
    return parseCountedTrack(p, keyword, tag, out);
}

// ---- Field dispatch ----
// Returns 1 when `static <keyword>` or `<keyword>` was consumed, 0 when
// neither is next (field absent), -1 on malformed present data.
int probeField(Parser& p, const std::string& keyword) {
    if (peekWord(p, "static")) {
        p.next();
        if (!peekWord(p, keyword)) {
            p.fail("expected '" + keyword + "' after 'static'");
            return -1;
        }
        p.next();
        return 1;
    }
    if (peekWord(p, keyword)) {
        p.next();
        return 1;
    }
    return 0;
}

// Skips a `{ ... }` track body after keyword+count were consumed (for
// animated forms of fields without a timeline tag).
bool skipTrackBody(Parser& p) { return p.skipBlock(); }

// `Keyword { a, b, c, },` triple (after probeField consumed the keyword).
bool parseTripleField(Parser& p, float out[3]) { return p.parseFloatTriple(out); }

// `Keyword v,` scalar, or `Keyword count { ... }` track. Handles the base
// value into `base` and appends the timeline with `tag` when animated.
bool parseScalarField(Parser& p, const std::string& keyword, uint32_t tag,
                      float& base, std::vector<MdxTimeline>& timelines) {
    int r = probeField(p, keyword);
    if (r < 0) return false;
    if (r == 0) return true;
    if (p.peek().kind == Token::LBrace) {
        // `static Keyword { v },` degenerate single-value form.
        float v = 0;
        if (!p.lbrace()) return false;
        double d = 0;
        if (!p.number(d)) return false;
        v = (float)d;
        if (p.peek().kind == Token::Comma) p.next();
        if (!p.rbrace()) return false;
        if (p.peek().kind == Token::Comma) p.next();
        base = v;
        return true;
    }
    double n = 0;
    if (!p.number(n)) return false;
    if (p.peek().kind == Token::LBrace) {
        (void)n; // keyframe count hint
        return parseTrackBody(p, tag, timelines, false);
    }
    base = (float)n;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

// Integer variant (static TextureID / ObjectId style fields).
bool parseIntField(Parser& p, const std::string& keyword, int& out) {
    int r = probeField(p, keyword);
    if (r < 0) return false;
    if (r == 0) return true;
    int v = 0;
    if (!p.integer(v)) return false;
    out = v;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

// `Keyword "string",` form.
bool parseStringField(Parser& p, const std::string& keyword, std::string& out) {
    int r = probeField(p, keyword);
    if (r < 0) return false;
    if (r == 0) return true;
    Token t = p.next();
    if (t.kind != Token::String) return p.fail("expected string");
    out = t.text;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}


// Visibility: `static`/plain scalar or counted track. Emitters and
// attachments have no base visibility field, so a scalar becomes a 1-key
// track at frame 0.
bool parseVisibilityField(Parser& p, uint32_t tag, std::vector<MdxTimeline>& out,
                          float* base = nullptr) {
    auto emitScalar = [&](float v) {
        if (base) {
            *base = v;
        } else {
            MdxTimeline tl;
            tl.tag = tag;
            tl.frames.push_back(0);
            tl.values.push_back(v);
            out.push_back(std::move(tl));
        }
    };
    int r = probeField(p, "Visibility");
    if (r < 0) return false;
    if (r == 0) return true;
    if (p.peek().kind == Token::LBrace) {
        // `static Visibility { v },` degenerate form.
        if (!p.lbrace()) return false;
        double d = 0;
        if (!p.number(d)) return false;
        if (p.peek().kind == Token::Comma) p.next();
        if (!p.rbrace()) return false;
        if (p.peek().kind == Token::Comma) p.next();
        emitScalar((float)d);
        return true;
    }
    double n = 0;
    if (!p.number(n)) return false;
    if (p.peek().kind == Token::LBrace) {
        (void)n; // keyframe count hint
        return parseTrackBody(p, tag, out, false);
    }
    emitScalar((float)n);
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

// ---- Materials ----
int filterModeFromWord(const std::string& word, bool& known) {
    known = true;
    if (word == "None") return 0;
    if (word == "Transparent") return 1;
    if (word == "Blend") return 2;
    if (word == "Additive") return 3;
    if (word == "AddAlpha") return 4;
    if (word == "Modulate") return 5;
    if (word == "Modulate2x") return 6;
    known = false;
    return 0;
}

bool parseMaterialLayer(Parser& p, MdxLayer& layer) {
    if (!p.lbrace()) return false;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in Layer");
        if (p.peek().kind != Token::Word) return p.fail("expected layer field");
        // Raw peek here: only TextureID/Alpha accept a `static` prefix
        // (handled explicitly below); anything else static is skipped.
        std::string kw = p.peek().text;
        if (kw == "FilterMode") {
            p.next();
            Token v = p.next();
            if (v.kind != Token::Word) return p.fail("expected FilterMode word");
            bool known = false;
            layer.filterMode = filterModeFromWord(v.text, known);
            if (!known) return p.fail("unknown FilterMode '" + v.text + "'");
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "Shading") {
            p.next();
            Token v = p.next();
            if (v.kind != Token::Word || v.text != "Unshaded") {
                return p.fail("expected Unshaded shading");
            }
            layer.flags |= 0x1;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "TwoSided") {
            p.next();
            layer.flags |= 0x10;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "Unfogged") {
            p.next();
            layer.flags |= 0x20;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "NoDepthTest") {
            p.next();
            layer.flags |= 0x40;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "NoDepthSet") {
            p.next();
            layer.flags |= 0x80;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "DepthTest") {
            p.next();
            layer.flags &= ~0x40;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "DepthWrite") {
            p.next();
            layer.flags &= ~0x80;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "SphereEnvMap") {
            p.next();
            layer.flags |= 0x2;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "TextureID") {
            // Static int or KMTF integer track.
            p.next();
            double n = 0;
            if (!p.number(n)) return false;
            if (p.peek().kind == Token::LBrace) {
                if (!parseTrackBody(p, MakeTag('K','M','T','F'), layer.timelines, false)) {
                    return false;
                }
            } else {
                layer.textureId = (int)std::llround(n);
                if (p.peek().kind == Token::Comma) p.next();
            }
        } else if (kw == "Alpha") {
            if (!parseScalarField(p, "Alpha", MakeTag('K','M','L','T'), layer.alpha,
                                 layer.timelines)) {
                return false;
            }
        } else if (kw == "TextureAnimId") {
            p.next();
            int v = 0;
            if (!p.integer(v)) return false;
            layer.textureAnimationId = v;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "CoordId") {
            p.next();
            int v = 0;
            if (!p.integer(v)) return false;
            layer.coordId = v;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "static") {
            // `static TextureID N` / `static Alpha F` forms.
            p.next();
            Token k2 = p.peek();
            if (k2.kind != Token::Word) return p.fail("expected field after 'static'");
            if (k2.text == "TextureID") {
                p.next();
                int v = 0;
                if (!p.integer(v)) return false;
                layer.textureId = v;
                if (p.peek().kind == Token::Comma) p.next();
            } else if (k2.text == "Alpha") {
                p.next();
                float v = 0;
                double d = 0;
                if (!p.number(d)) return false;
                v = (float)d;
                layer.alpha = v;
                if (p.peek().kind == Token::Comma) p.next();
            } else {
                if (!p.skipValue()) return false;
            }
        } else {
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

bool parseMaterial(Parser& p, MdxMaterial& mat) {
    if (!p.lbrace()) return false;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in Material");
        if (!peekWord(p, "Layer")) {
            if (!p.skipValue()) return false;
            continue;
        }
        p.next(); // 'Layer'
        MdxLayer layer;
        if (!parseMaterialLayer(p, layer)) return false;
        mat.layers.push_back(std::move(layer));
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

// ---- Geoset ----
bool parseIntList(Parser& p, std::vector<unsigned>& out) {
    // Collects flat integers, transparent to nested `{...}` groupings
    // (MDL Triangles blocks group triples, but indices stay flat like the
    // MDX path keeps raw face order).
    while (true) {
        Token t = p.peek();
        if (t.kind == Token::RBrace || t.kind == Token::End) return true;
        if (t.kind == Token::LBrace) {
            p.next();
            if (!parseIntList(p, out)) return false;
            if (!p.rbrace()) return false;
            if (p.peek().kind == Token::Comma) p.next();
            continue;
        }
        if (t.kind == Token::Comma) {
            p.next();
            continue;
        }
        double v = 0;
        if (!p.number(v)) return p.fail("expected index integer");
        if (v < 0 || v > 65535) return p.fail("face index out of range");
        out.push_back((unsigned)std::llround(v));
    }
}

bool parseGeoset(Parser& p, MdxGeoset& geo) {
    if (!p.lbrace()) return false;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in Geoset");
        Token t = p.peek();
        if (t.kind != Token::Word) return p.fail("expected geoset field");
        std::string kw = t.text;
        if (kw == "Vertices" || kw == "Normals") {
            p.next();
            double count = 0;
            if (!p.number(count)) return false;
            if (!p.lbrace()) return false;
            std::vector<float>& dst = (kw == "Vertices") ? geo.vertices : geo.normals;
            dst.reserve(dst.size() + (size_t)count * 3);
            while (p.peek().kind != Token::RBrace) {
                if (p.peek().kind == Token::End) return p.fail("unexpected end in vertices");
                if (!p.lbrace()) return false;
                for (int i = 0; i < 3; i++) {
                    double v = 0;
                    if (!p.number(v)) return false;
                    dst.push_back((float)v);
                    if (p.peek().kind == Token::Comma) p.next();
                    else if (i < 2) return p.fail("expected ','");
                }
                if (!p.rbrace()) return false;
                if (p.peek().kind == Token::Comma) p.next();
            }
            if (!p.rbrace()) return false;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "TVertices") {
            p.next();
            double count = 0;
            if (!p.number(count)) return false;
            if (!p.lbrace()) return false;
            std::vector<float> set;
            set.reserve((size_t)count * 2);
            while (p.peek().kind != Token::RBrace) {
                if (p.peek().kind == Token::End) return p.fail("unexpected end in uvs");
                if (!p.lbrace()) return false;
                for (int i = 0; i < 2; i++) {
                    double v = 0;
                    if (!p.number(v)) return false;
                    set.push_back((float)v);
                    if (p.peek().kind == Token::Comma) p.next();
                    else if (i < 1) return p.fail("expected ','");
                }
                if (!p.rbrace()) return false;
                if (p.peek().kind == Token::Comma) p.next();
            }
            if (!p.rbrace()) return false;
            if (p.peek().kind == Token::Comma) p.next();
            geo.uvSets.push_back(std::move(set));
        } else if (kw == "VertexGroup") {
            p.next();
            if (!p.lbrace()) return false;
            while (p.peek().kind != Token::RBrace) {
                if (p.peek().kind == Token::End) return p.fail("unexpected end in VertexGroup");
                if (p.peek().kind == Token::Comma) { p.next(); continue; }
                double v = 0;
                if (!p.number(v)) return false;
                if (v < 0 || v > 255) return p.fail("vertex group out of range");
                geo.vertexGroups.push_back((uint8_t)std::llround(v));
            }
            if (!p.rbrace()) return false;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "Faces") {
            p.next();
            double groups = 0, total = 0;
            if (!p.number(groups)) return false;
            if (!p.number(total)) return false;
            (void)groups;
            (void)total;
            if (!p.lbrace()) return false;
            while (p.peek().kind != Token::RBrace) {
                if (p.peek().kind == Token::End) return p.fail("unexpected end in Faces");
                Token u = p.peek();
                if (u.kind == Token::Comma) { p.next(); continue; }
                if (u.kind != Token::Word) return p.fail("expected Triangles block");
                p.next(); // block word ('Triangles' and friends stay flat)
                if (!p.lbrace()) return false;
                if (!parseIntList(p, geo.indices)) return false;
                if (!p.rbrace()) return false;
                if (p.peek().kind == Token::Comma) p.next();
            }
            if (!p.rbrace()) return false;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "Groups") {
            p.next();
            double groupCount = 0, matrixTotal = 0;
            if (!p.number(groupCount)) return false;
            if (!p.number(matrixTotal)) return false;
            (void)matrixTotal;
            if (!p.lbrace()) return false;
            for (int g = 0; g < (int)groupCount; g++) {
                if (!p.expectWord("Matrices")) return false;
                if (!p.lbrace()) return false;
                uint32_t count = 0;
                while (p.peek().kind != Token::RBrace) {
                    if (p.peek().kind == Token::End) {
                        return p.fail("unexpected end in Matrices");
                    }
                    if (p.peek().kind == Token::Comma) { p.next(); continue; }
                    double v = 0;
                    if (!p.number(v)) return false;
                    geo.matrixIndices.push_back((uint32_t)std::llround(v));
                    count++;
                }
                if (!p.rbrace()) return false;
                if (p.peek().kind == Token::Comma) p.next();
                geo.matrixGroups.push_back(count);
            }
            if (!p.rbrace()) return false;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "MaterialID") {
            p.next();
            int v = 0;
            if (!p.integer(v)) return false;
            geo.materialId = v;
            if (p.peek().kind == Token::Comma) p.next();
        } else {
            // MinimumExtent/MaximumExtent/BoundsRadius/Anim extents,
            // SelectionGroup/Unselectable and future fields: parsed over.
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    if (!geo.uvSets.empty()) geo.uvs = geo.uvSets[0];
    // Rigid fallback: no per-vertex groups means group 0 for all.
    if (geo.vertexGroups.empty() && !geo.vertices.empty()) {
        geo.vertexGroups.assign(geo.vertices.size() / 3, 0);
    }
    return true;
}

// ---- GeosetAnim / TextureAnims / Sequences / Textures ----
bool parseGeosetAnim(Parser& p, MdxGeosetAnim& ga) {
    if (!p.lbrace()) return false;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in GeosetAnim");
        std::string kw = fieldKeyword(p);
        if (kw != "Alpha" && kw != "Color" && kw != "GeosetId") {
            if (!p.skipValue()) return false;
            continue;
        }
        if (kw == "Alpha") {
            if (!parseScalarField(p, "Alpha", MakeTag('K','G','A','O'), ga.alpha,
                                 ga.timelines)) {
                return false;
            }
        } else if (kw == "Color") {
            // KGAC values (and the base color) are BGR on disk; MDL text is
            // RGB, so swap on the way in like the binary layout expects.
            int r = probeField(p, "Color");
            if (r < 0) return false;
            if (r > 0) {
                if (p.peek().kind == Token::LBrace) {
                    float c[3] = {1, 1, 1};
                    if (!p.parseFloatTriple(c)) return false;
                    if (p.peek().kind == Token::Comma) p.next();
                    ga.color = {c[2], c[1], c[0]};
                } else {
                    double n = 0;
                    if (!p.number(n)) return false;
                    (void)n;
                    if (!parseTrackBody(p, MakeTag('K','G','A','C'), ga.timelines, true)) {
                        return false;
                    }
                }
            }
        } else if (kw == "GeosetId") {
            if (!parseIntField(p, "GeosetId", ga.geosetId)) return false;
        } else {
            return p.fail("unreachable geosetanim branch");
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

bool parseTexAnim(Parser& p, MdxTexAnim& ta) {
    if (!p.expectWord("TVertexAnim")) return false;
    if (!p.lbrace()) return false;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in TVertexAnim");
        if (peekWord(p, "Translation")) {
            if (!parseCountedTrack(p, "Translation", MakeTag('K','T','A','T'), ta.timelines)) {
                return false;
            }
        } else if (peekWord(p, "Rotation")) {
            if (!parseCountedTrack(p, "Rotation", MakeTag('K','T','A','R'), ta.timelines)) {
                return false;
            }
        } else if (peekWord(p, "Scaling")) {
            if (!parseCountedTrack(p, "Scaling", MakeTag('K','T','A','S'), ta.timelines)) {
                return false;
            }
        } else {
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

bool parseSequenceAnim(Parser& p, MdxSequence& seq, const std::string& name) {
    seq.name = name;
    if (!p.lbrace()) return false;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in Anim");
        if (peekWord(p, "Interval")) {
            p.next();
            if (!p.lbrace()) return false;
            int a = 0, b = 0;
            if (!p.integer(a)) return false;
            if (!p.comma()) return false;
            if (!p.integer(b)) return false;
            if (p.peek().kind == Token::Comma) p.next();
            if (!p.rbrace()) return false;
            if (p.peek().kind == Token::Comma) p.next();
            seq.startFrame = (uint32_t)(a < 0 ? 0 : a);
            seq.endFrame = (uint32_t)(b < 0 ? 0 : b);
        } else if (peekWord(p, "MoveSpeed")) {
            p.next();
            float v = 0;
            double d = 0;
            if (!p.number(d)) return false;
            v = (float)d;
            seq.moveSpeed = v;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (peekWord(p, "NonLooping")) {
            p.next();
            seq.flags |= 0x1;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (peekWord(p, "Rarity")) {
            p.next();
            double d = 0;
            if (!p.number(d)) return false;
            seq.rarity = (float)d;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (peekWord(p, "MinimumExtent")) {
            p.next();
            if (!p.parseFloatTriple(seq.boundsMin)) return false;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (peekWord(p, "MaximumExtent")) {
            p.next();
            if (!p.parseFloatTriple(seq.boundsMax)) return false;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (peekWord(p, "BoundsRadius")) {
            p.next();
            double d = 0;
            if (!p.number(d)) return false;
            seq.boundsRadius = (float)d;
            if (p.peek().kind == Token::Comma) p.next();
        } else {
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}


// Bone / Helper bodies. Bone entries may carry GeosetId/GeosetAnimId.
bool parseBasicNodeBody(Parser& p, MdxNode& node, bool withGeosetIds) {
    NodeHead head;
    head.name = node.name;
    if (!p.lbrace()) return false;
    if (!parseNodeHead(p, head, withGeosetIds)) return false;
    applyNodeHead(head, node);
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in node");
        if (p.peek().kind != Token::Word) return p.fail("expected node field");
        std::string kw = fieldKeyword(p);
        if (kw == "Translation" || kw == "Rotation" || kw == "Scaling") {
            if (!parseNodeTransformTracks(p, kw, node.timelines)) return false;
        } else if (kw == "Visibility") {
            // Generic-object visibility (renderer ignores bone visibility,
            // stored for MDX parity).
            if (!parseVisibilityField(p, MakeTag('K','G','B','V'), node.timelines)) {
                return false;
            }
        } else {
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

bool parseAttachmentBody(Parser& p, MdxAttachment& att, const std::string& name) {
    MdxNode node;
    node.name = name;
    NodeHead head;
    head.name = name;
    if (!p.lbrace()) return false;
    if (!parseNodeHead(p, head, false)) return false;
    applyNodeHead(head, node);
    att.name = node.name;
    att.objectId = node.objectId;
    att.parentId = node.parentId;
    att.flags = node.flags;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in Attachment");
        if (p.peek().kind != Token::Word) return p.fail("expected attachment field");
        std::string kw = fieldKeyword(p);
        if (kw == "Translation" || kw == "Rotation" || kw == "Scaling") {
            if (!parseNodeTransformTracks(p, kw, att.timelines)) return false;
        } else if (kw == "Visibility") {
            if (!parseVisibilityField(p, MakeTag('K','A','T','V'), att.timelines)) {
                return false;
            }
        } else if (kw == "Path") {
            if (!parseStringField(p, "Path", att.path)) return false;
        } else if (kw == "AttachmentID" || kw == "AttachmentId") {
            p.next();
            int v = 0;
            if (!p.integer(v)) return false;
            att.attachmentId = v;
            if (p.peek().kind == Token::Comma) p.next();
        } else {
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

int lightTypeFromWord(const std::string& word, bool& known) {
    known = true;
    if (word == "Omnidirectional" || word == "Point") return 0;
    if (word == "Directional") return 1;
    if (word == "Ambient") return 2;
    known = false;
    return 0;
}

bool parseLightBody(Parser& p, MdxLight& lt, const std::string& name) {
    NodeHead head;
    head.name = name;
    if (!p.lbrace()) return false;
    if (!parseNodeHead(p, head, false)) return false;
    lt.name = head.name;
    lt.objectId = head.objectId;
    lt.parentId = head.parentId;
    lt.flags = head.flags;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in Light");
        if (p.peek().kind != Token::Word) return p.fail("expected light field");
        std::string kw = fieldKeyword(p);
        if (kw == "Translation" || kw == "Rotation" || kw == "Scaling") {
            if (!parseNodeTransformTracks(p, kw, lt.timelines)) return false;
        } else if (kw == "Visibility") {
            if (!parseVisibilityField(p, MakeTag('K','L','A','V'), lt.timelines)) {
                return false;
            }
        } else if (kw == "Omnidirectional" || kw == "Point" || kw == "Directional" ||
                   kw == "Ambient") {
            bool known = false;
            int t = lightTypeFromWord(kw, known);
            (void)known;
            p.next();
            lt.type = (MdxLightType)t;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "Color") {
            int r = probeField(p, "Color");
            if (r < 0) return false;
            if (r == 0) {
                if (!p.skipValue()) return false;
            } else if (p.peek().kind == Token::LBrace) {
                if (!p.parseFloatTriple(lt.color)) return false;
                if (p.peek().kind == Token::Comma) p.next();
            } else {
                double n = 0;
                if (!p.number(n)) return false;
                (void)n;
                if (!parseTrackBody(p, MakeTag('K','L','A','C'), lt.timelines, false)) {
                    return false;
                }
            }
        } else if (kw == "Intensity") {
            if (!parseScalarField(p, "Intensity", MakeTag('K','L','A','I'), lt.intensity,
                                 lt.timelines)) {
                return false;
            }
        } else if (kw == "AmbientColor" || kw == "AmbColor") {
            std::string key = kw;
            int r = probeField(p, key);
            if (r < 0) return false;
            if (r == 0) {
                if (!p.skipValue()) return false;
            } else if (p.peek().kind == Token::LBrace) {
                if (!p.parseFloatTriple(lt.ambientColor)) return false;
                if (p.peek().kind == Token::Comma) p.next();
            } else {
                double n = 0;
                if (!p.number(n)) return false;
                (void)n;
                if (!parseTrackBody(p, MakeTag('K','L','B','C'), lt.timelines, false)) {
                    return false;
                }
            }
        } else if (kw == "AmbientIntensity" || kw == "AmbIntensity") {
            // Static scalar or KLBI track (probeField eats `static` too).
            std::string key = kw;
            int r = probeField(p, key);
            if (r < 0) return false;
            if (r == 0) {
                if (!p.skipValue()) return false;
            } else if (p.peek().kind == Token::LBrace) {
                double d = 0;
                if (!p.lbrace()) return false;
                if (!p.number(d)) return false;
                lt.ambientIntensity = (float)d;
                if (p.peek().kind == Token::Comma) p.next();
                if (!p.rbrace()) return false;
                if (p.peek().kind == Token::Comma) p.next();
            } else {
                double n = 0;
                if (!p.number(n)) return false;
                if (p.peek().kind == Token::LBrace) {
                    if (!parseTrackBody(p, MakeTag('K','L','B','I'), lt.timelines,
                                       false)) {
                        return false;
                    }
                } else {
                    lt.ambientIntensity = (float)n;
                    if (p.peek().kind == Token::Comma) p.next();
                }
            }
        } else if (kw == "AttenuationStart") {
            if (!parseScalarField(p, "AttenuationStart", MakeTag('K','L','A','S'),
                                 lt.attenuationStart, lt.timelines)) {
                return false;
            }
        } else if (kw == "AttenuationEnd") {
            if (!parseScalarField(p, "AttenuationEnd", MakeTag('K','L','A','E'),
                                 lt.attenuationEnd, lt.timelines)) {
                return false;
            }
        } else {
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

bool parseParticleEmitter1Body(Parser& p, MdxParticleEmitter1& em,
                               const std::string& name) {
    NodeHead head;
    head.name = name;
    if (!p.lbrace()) return false;
    if (!parseNodeHead(p, head, false)) return false;
    em.name = head.name;
    em.objectId = head.objectId;
    em.parentId = head.parentId;
    em.flags = head.flags;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in ParticleEmitter");
        if (p.peek().kind != Token::Word) return p.fail("expected emitter field");
        std::string kw = fieldKeyword(p);
        if (kw == "Translation" || kw == "Rotation" || kw == "Scaling") {
            if (!parseNodeTransformTracks(p, kw, em.timelines)) return false;
        } else if (kw == "Visibility") {
            if (!parseVisibilityField(p, MakeTag('K','P','E','V'), em.timelines)) {
                return false;
            }
        } else if (kw == "EmissionRate") {
            if (!parseScalarField(p, "EmissionRate", MakeTag('K','P','E','E'),
                                 em.emissionRate, em.timelines)) {
                return false;
            }
        } else if (kw == "Gravity") {
            if (!parseScalarField(p, "Gravity", MakeTag('K','P','E','G'), em.gravity,
                                 em.timelines)) {
                return false;
            }
        } else if (kw == "Longitude") {
            if (!parseScalarField(p, "Longitude", MakeTag('K','P','L','N'), em.longitude,
                                 em.timelines)) {
                return false;
            }
        } else if (kw == "Latitude") {
            if (!parseScalarField(p, "Latitude", MakeTag('K','P','L','T'), em.latitude,
                                 em.timelines)) {
                return false;
            }
        } else if (kw == "LifeSpan") {
            if (!parseScalarField(p, "LifeSpan", MakeTag('K','P','E','L'), em.lifeSpan,
                                 em.timelines)) {
                return false;
            }
        } else if (kw == "Speed") {
            if (!parseScalarField(p, "Speed", MakeTag('K','P','E','S'), em.speed,
                                 em.timelines)) {
                return false;
            }
        } else if (kw == "Path" || kw == "Model") {
            std::string key = kw;
            if (!parseStringField(p, key, em.internalModelPath)) return false;
        } else if (kw == "TextureID") {
            p.next();
            int v = 0;
            if (!p.integer(v)) return false;
            em.textureId = v;
            if (p.peek().kind == Token::Comma) p.next();
        } else {
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

int pre2FilterMode(Parser& p, bool& ok) {
    ok = true;
    Token v = p.peek();
    if (v.kind == Token::Word) {
        p.next();
        bool known = false;
        int mode = filterModeFromWord(v.text, known);
        if (!known) {
            p.fail("unknown FilterMode '" + v.text + "'");
            ok = false;
        }
        if (p.peek().kind == Token::Comma) p.next();
        return mode;
    }
    int iv = 0;
    if (!p.integer(iv)) {
        ok = false;
        return 0;
    }
    if (p.peek().kind == Token::Comma) p.next();
    return iv;
}

bool parseParticleEmitter2Body(Parser& p, MdxParticleEmitter2& em,
                               const std::string& name) {
    NodeHead head;
    head.name = name;
    if (!p.lbrace()) return false;
    if (!parseNodeHead(p, head, false)) return false;
    em.name = head.name;
    em.objectId = head.objectId;
    em.parentId = head.parentId;
    em.flags = (int)head.flags;
    int segment = 0;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in ParticleEmitter2");
        if (p.peek().kind != Token::Word) return p.fail("expected emitter2 field");
        std::string kw = fieldKeyword(p);
        if (kw == "Translation" || kw == "Rotation" || kw == "Scaling") {
            if (!parseNodeTransformTracks(p, kw, em.timelines)) return false;
        } else if (kw == "Visibility") {
            if (!parseVisibilityField(p, MakeTag('K','P','2','V'), em.timelines)) {
                return false;
            }
        } else if (kw == "Speed") {
            if (!parseScalarField(p, "Speed", MakeTag('K','P','2','S'), em.speed,
                                 em.timelines)) {
                return false;
            }
        } else if (kw == "Variation") {
            if (!parseScalarField(p, "Variation", MakeTag('K','P','2','R'), em.variation,
                                 em.timelines)) {
                return false;
            }
        } else if (kw == "Latitude") {
            if (!parseScalarField(p, "Latitude", MakeTag('K','P','2','L'), em.latitude,
                                 em.timelines)) {
                return false;
            }
        } else if (kw == "Gravity") {
            if (!parseScalarField(p, "Gravity", MakeTag('K','P','2','G'), em.gravity,
                                 em.timelines)) {
                return false;
            }
        } else if (kw == "LifeSpan") {
            // No KP2 lifespan tag exists in this port; static only, with
            // graceful skip for animated forms.
            p.next();
            double d = 0;
            if (!p.number(d)) return false;
            if (p.peek().kind == Token::LBrace) {
                if (!p.skipBlock()) return false;
            } else {
                em.lifeSpan = (float)d;
                if (p.peek().kind == Token::Comma) p.next();
            }
        } else if (kw == "EmissionRate") {
            if (!parseScalarField(p, "EmissionRate", MakeTag('K','P','2','E'),
                                 em.emissionRate, em.timelines)) {
                return false;
            }
        } else if (kw == "Length") {
            if (!parseScalarField(p, "Length", MakeTag('K','P','2','N'), em.length,
                                 em.timelines)) {
                return false;
            }
        } else if (kw == "Width") {
            if (!parseScalarField(p, "Width", MakeTag('K','P','2','W'), em.width,
                                 em.timelines)) {
                return false;
            }
        } else if (kw == "FilterMode") {
            p.next();
            bool ok = true;
            em.filterMode = pre2FilterMode(p, ok);
            if (!ok) return false;
        } else if (kw == "Rows" || kw == "Columns" || kw == "TextureID" ||
                   kw == "Squirt" || kw == "PriorityPlane" || kw == "ReplaceableId") {
            p.next();
            int v = 0;
            if (!p.integer(v)) return false;
            if (kw == "Rows") em.rows = (unsigned)v;
            else if (kw == "Columns") em.columns = (unsigned)v;
            else if (kw == "TextureID") em.textureId = v;
            else if (kw == "Squirt") em.squirt = (unsigned)v;
            else if (kw == "PriorityPlane") em.priorityPlane = v;
            else em.replaceableId = (unsigned)v;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "Head") {
            p.next();
            em.headOrTail = 0;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "Tail") {
            p.next();
            em.headOrTail = 2;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "HeadOrTail") {
            p.next();
            int v = 0;
            if (!p.integer(v)) return false;
            em.headOrTail = v;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "TailLength" || kw == "Time") {
            p.next();
            double d = 0;
            if (!p.number(d)) return false;
            if (kw == "TailLength") em.tailLength = (float)d;
            else em.timeMiddle = (float)d;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "SegmentColor") {
            p.next();
            if (segment >= 3) {
                if (!p.skipBlock()) return false;
                continue;
            }
            if (!p.lbrace()) return false;
            while (p.peek().kind != Token::RBrace) {
                if (p.peek().kind == Token::End) {
                    return p.fail("unexpected end in SegmentColor");
                }
                if (peekWord(p, "Color")) {
                    p.next();
                    float c[3] = {1, 1, 1};
                    if (!p.parseFloatTriple(c)) return false;
                    if (p.peek().kind == Token::Comma) p.next();
                    em.segmentColors[segment][0] = c[0];
                    em.segmentColors[segment][1] = c[1];
                    em.segmentColors[segment][2] = c[2];
                } else if (peekWord(p, "Alpha")) {
                    p.next();
                    int a = 0;
                    if (!p.integer(a)) return false;
                    if (a < 0) a = 0;
                    if (a > 255) a = 255;
                    em.segmentAlphas[segment] = (unsigned char)a;
                    if (p.peek().kind == Token::Comma) p.next();
                } else {
                    if (!p.skipValue()) return false;
                }
            }
            if (!p.rbrace()) return false;
            if (p.peek().kind == Token::Comma) p.next();
            segment++;
        } else if (kw == "ParticleScaling") {
            p.next();
            if (!p.parseFloatTriple(em.segmentScaling)) return false;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "HeadIntervals" || kw == "TailIntervals") {
            bool isHead = (kw == "HeadIntervals");
            p.next();
            if (!p.lbrace()) return false;
            // Two triples of uints; tolerate flat or nested layout by
            // tracking brace depth (a bare RBrace check would stop at
            // the first inner triple's close).
            unsigned vals[6] = {};
            int got = 0;
            int depth = 0;
            while (true) {
                Token u = p.peek();
                if (u.kind == Token::End) {
                    return p.fail("unexpected end in intervals");
                }
                if (u.kind == Token::LBrace) {
                    p.next();
                    depth++;
                    continue;
                }
                if (u.kind == Token::RBrace) {
                    p.next();
                    if (depth == 0) break;
                    depth--;
                    if (p.peek().kind == Token::Comma) p.next();
                    continue;
                }
                if (u.kind == Token::Comma) { p.next(); continue; }
                double d = 0;
                if (!p.number(d)) return false;
                if (got < 6) vals[got++] = (unsigned)std::llround(d);
            }
            if (p.peek().kind == Token::Comma) p.next();
            unsigned(*dst)[3] = isHead ? em.headIntervals : em.tailIntervals;
            dst[0][0] = vals[0];
            dst[0][1] = vals[1];
            dst[0][2] = vals[2];
            dst[1][0] = vals[3];
            dst[1][1] = vals[4];
            dst[1][2] = vals[5];
        } else {
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

bool parseRibbonBody(Parser& p, MdxRibbonEmitter& rb, const std::string& name) {
    NodeHead head;
    head.name = name;
    if (!p.lbrace()) return false;
    if (!parseNodeHead(p, head, false)) return false;
    rb.name = head.name;
    rb.objectId = head.objectId;
    rb.parentId = head.parentId;
    rb.flags = head.flags;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in RibbonEmitter");
        if (p.peek().kind != Token::Word) return p.fail("expected ribbon field");
        std::string kw = fieldKeyword(p);
        if (kw == "Translation" || kw == "Rotation" || kw == "Scaling") {
            if (!parseNodeTransformTracks(p, kw, rb.timelines)) return false;
        } else if (kw == "Visibility") {
            if (!parseVisibilityField(p, MakeTag('K','R','V','S'), rb.timelines)) {
                return false;
            }
        } else if (kw == "HeightAbove") {
            if (!parseScalarField(p, "HeightAbove", MakeTag('K','R','H','A'),
                                 rb.heightAbove, rb.timelines)) {
                return false;
            }
        } else if (kw == "HeightBelow") {
            if (!parseScalarField(p, "HeightBelow", MakeTag('K','R','H','B'),
                                 rb.heightBelow, rb.timelines)) {
                return false;
            }
        } else if (kw == "Alpha") {
            if (!parseScalarField(p, "Alpha", MakeTag('K','R','A','L'), rb.alpha,
                                 rb.timelines)) {
                return false;
            }
        } else if (kw == "Color") {
            int r = probeField(p, "Color");
            if (r < 0) return false;
            if (r == 0) {
                if (!p.skipValue()) return false;
            } else if (p.peek().kind == Token::LBrace) {
                if (!p.parseFloatTriple(rb.color)) return false;
                if (p.peek().kind == Token::Comma) p.next();
            } else {
                double n = 0;
                if (!p.number(n)) return false;
                (void)n;
                if (!parseTrackBody(p, MakeTag('K','R','C','O'), rb.timelines, false)) {
                    return false;
                }
            }
        } else if (kw == "LifeSpan") {
            p.next();
            double d = 0;
            if (!p.number(d)) return false;
            rb.lifeSpan = (float)d;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "TextureSlot") {
            p.next();
            int v = 0;
            if (p.peek().kind == Token::LBrace) {
                // Animated form without a timeline tag in this port; skip.
                if (!p.skipBlock()) return false;
            } else {
                if (!p.integer(v)) return false;
                rb.textureSlot = v;
                if (p.peek().kind == Token::Comma) p.next();
            }
        } else if (kw == "EmissionRate") {
            p.next();
            double d = 0;
            if (!p.number(d)) return false;
            if (p.peek().kind == Token::LBrace) {
                // No emission-rate timeline tag in this port; skip.
                if (!p.skipBlock()) return false;
            } else {
                rb.emissionRate = (float)d;
                if (p.peek().kind == Token::Comma) p.next();
            }
        } else if (kw == "Rows" || kw == "Columns" || kw == "MaterialID") {
            p.next();
            int v = 0;
            if (!p.integer(v)) return false;
            if (kw == "Rows") rb.rows = (unsigned)v;
            else if (kw == "Columns") rb.columns = (unsigned)v;
            else {
                rb.materialId = v;
                rb.priorityPlane = v;
            }
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "Gravity") {
            p.next();
            double d = 0;
            if (!p.number(d)) return false;
            rb.gravity = (float)d;
            if (p.peek().kind == Token::Comma) p.next();
        } else {
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

bool parsePopcornBody(Parser& p, MdxParticleEmitterPopcorn& em,
                      const std::string& name) {
    NodeHead head;
    head.name = name;
    if (!p.lbrace()) return false;
    if (!parseNodeHead(p, head, false)) return false;
    em.name = head.name;
    em.objectId = head.objectId;
    em.parentId = head.parentId;
    em.flags = head.flags;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) {
            return p.fail("unexpected end in ParticleEmitterPopcorn");
        }
        if (p.peek().kind != Token::Word) return p.fail("expected popcorn field");
        std::string kw = fieldKeyword(p);
        if (kw == "Translation" || kw == "Rotation" || kw == "Scaling") {
            if (!parseNodeTransformTracks(p, kw, em.timelines)) return false;
        } else if (kw == "Visibility") {
            if (!parseVisibilityField(p, MakeTag('K','P','P','V'), em.timelines)) {
                return false;
            }
        } else if (kw == "LifeSpan" || kw == "EmissionRate" || kw == "Speed" ||
                   kw == "Alpha") {
            float* dst = nullptr;
            if (kw == "LifeSpan") dst = &em.lifeSpan;
            else if (kw == "EmissionRate") dst = &em.emissionRate;
            else if (kw == "Speed") dst = &em.speed;
            else dst = &em.alpha;
            int r = probeField(p, kw);
            if (r < 0) return false;
            if (r == 0) {
                if (!p.skipValue()) return false;
            } else {
                double d = 0;
                if (!p.number(d)) return false;
                if (p.peek().kind == Token::LBrace) {
                    if (!p.skipBlock()) return false;
                } else {
                    *dst = (float)d;
                    if (p.peek().kind == Token::Comma) p.next();
                }
            }
        } else if (kw == "Color") {
            int r = probeField(p, "Color");
            if (r < 0) return false;
            if (r == 0) {
                if (!p.skipValue()) return false;
            } else if (p.peek().kind == Token::LBrace) {
                if (!p.parseFloatTriple(em.color)) return false;
                if (p.peek().kind == Token::Comma) p.next();
            } else {
                double n = 0;
                if (!p.number(n)) return false;
                (void)n;
                if (!parseTrackBody(p, MakeTag('K','P','P','C'), em.timelines, false)) {
                    return false;
                }
            }
        } else if (kw == "ReplaceableId") {
            p.next();
            int v = 0;
            if (!p.integer(v)) return false;
            em.replaceableId = v;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "Path" || kw == "AnimationVisibilityGuide") {
            std::string key = kw;
            std::string s;
            // Optional `static` already consumed by fieldKeyword? No:
            // fieldKeyword only peeks. Consume via probe pattern manually.
            int r = probeField(p, key);
            if (r < 0) return false;
            if (r == 0) {
                if (!p.skipValue()) return false;
            } else {
                Token t = p.next();
                if (t.kind != Token::String) return p.fail("expected string");
                s = t.text;
                if (p.peek().kind == Token::Comma) p.next();
                if (key == "Path") em.path = s;
                else em.animationVisibilityGuide = s;
            }
        } else {
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

bool parseEventBody(Parser& p, MdxEventObject& ev, const std::string& name) {
    NodeHead head;
    head.name = name;
    if (!p.lbrace()) return false;
    if (!parseNodeHead(p, head, false)) return false;
    ev.name = head.name;
    ev.objectId = head.objectId;
    ev.parentId = head.parentId;
    ev.flags = head.flags;
    // Type/id derive from the name prefix like the binary path.
    if (ev.name.size() >= 3) {
        ev.type = ev.name.substr(0, 3);
        if (ev.name.size() > 4) ev.id = ev.name.substr(4);
    }
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in EventObject");
        if (p.peek().kind != Token::Word) return p.fail("expected event field");
        std::string kw = fieldKeyword(p);
        if (kw == "Translation" || kw == "Rotation" || kw == "Scaling") {
            if (!parseNodeTransformTracks(p, kw, ev.timelines)) return false;
        } else if (kw == "EventTrack") {
            p.next();
            double n = 0;
            if (!p.number(n)) return false;
            (void)n;
            if (!parseTrackBody(p, MakeTag('K','E','V','T'), ev.timelines, false)) {
                return false;
            }
        } else {
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

int collisionShapeFromWord(const std::string& word, bool& known) {
    known = true;
    if (word == "Box") return (int)MdxCollisionShapeType::Box;
    if (word == "Plane") return (int)MdxCollisionShapeType::Plane;
    if (word == "Sphere") return (int)MdxCollisionShapeType::Sphere;
    if (word == "Cylinder") return (int)MdxCollisionShapeType::Cylinder;
    known = false;
    return 0;
}

bool parseCollisionBody(Parser& p, MdxCollisionShape& shape, const std::string& name) {
    NodeHead head;
    head.name = name;
    if (!p.lbrace()) return false;
    if (!parseNodeHead(p, head, false)) return false;
    shape.name = head.name;
    shape.objectId = head.objectId;
    shape.parentId = head.parentId;
    shape.flags = head.flags;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) {
            return p.fail("unexpected end in CollisionShape");
        }
        if (p.peek().kind != Token::Word) return p.fail("expected collision field");
        std::string kw = fieldKeyword(p);
        if (kw == "Box" || kw == "Plane" || kw == "Sphere" || kw == "Cylinder") {
            bool known = false;
            int t = collisionShapeFromWord(kw, known);
            (void)known;
            p.next();
            shape.type = (MdxCollisionShapeType)t;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "Vertices") {
            p.next();
            if (!p.lbrace()) return false;
            for (int v = 0; v < 2; v++) {
                if (p.peek().kind != Token::LBrace) break;
                if (!p.parseFloatTriple(shape.vertices[v])) return false;
                if (p.peek().kind == Token::Comma) p.next();
            }
            if (!p.rbrace()) return false;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "BoundsRadius") {
            p.next();
            double d = 0;
            if (!p.number(d)) return false;
            shape.boundsRadius = (float)d;
            if (p.peek().kind == Token::Comma) p.next();
        } else {
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    // Mirror the binary vertex rules: spheres carry one vertex, the rest two.
    if (shape.type == MdxCollisionShapeType::Sphere) {
        shape.vertices[1][0] = shape.vertices[0][0];
        shape.vertices[1][1] = shape.vertices[0][1];
        shape.vertices[1][2] = shape.vertices[0][2];
    }
    return true;
}

bool parseCameraBody(Parser& p, MdxCamera& cam, const std::string& name) {
    cam.name = name;
    if (!p.lbrace()) return false;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in Camera");
        if (p.peek().kind != Token::Word) return p.fail("expected camera field");
        std::string kw = fieldKeyword(p);
        if (kw == "Position") {
            p.next();
            if (!p.parseFloatTriple(cam.pos)) return false;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "TargetPosition") {
            p.next();
            if (!p.parseFloatTriple(cam.target)) return false;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "Translation") {
            if (!parseCountedTrack(p, "Translation", MakeTag('K','C','T','R'),
                                  cam.timelines)) {
                return false;
            }
        } else if (kw == "Rotation") {
            if (!parseCountedTrack(p, "Rotation", MakeTag('K','T','T','R'), cam.timelines)) {
                return false;
            }
        } else if (kw == "FieldOfView" || kw == "fieldOfView") {
            p.next();
            double d = 0;
            if (!p.number(d)) return false;
            cam.fov = (float)d;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "FarClip" || kw == "farClip") {
            p.next();
            double d = 0;
            if (!p.number(d)) return false;
            cam.farClip = (float)d;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "NearClip" || kw == "nearClip") {
            p.next();
            double d = 0;
            if (!p.number(d)) return false;
            cam.nearClip = (float)d;
            if (p.peek().kind == Token::Comma) p.next();
        } else {
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

// ---- Top-level blocks ----
bool parseVersionBlock(Parser& p, MdxModel& model) {
    if (!p.lbrace()) return false;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in Version");
        if (peekWord(p, "FormatVersion")) {
            p.next();
            int v = 800;
            if (!p.integer(v)) return false;
            model.version = v;
            if (p.peek().kind == Token::Comma) p.next();
        } else {
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

bool parseModelHead(Parser& p, MdxModel& model, const std::string& name) {
    model.name = name;
    if (!p.lbrace()) return false;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in Model");
        if (p.peek().kind != Token::Word) return p.fail("expected model field");
        std::string kw = p.peek().text;
        if (kw == "MinimumExtent") {
            p.next();
            if (!p.parseFloatTriple(model.boundsMin)) return false;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "MaximumExtent") {
            p.next();
            if (!p.parseFloatTriple(model.boundsMax)) return false;
            if (p.peek().kind == Token::Comma) p.next();
        } else if (kw == "BoundsRadius") {
            p.next();
            double d = 0;
            if (!p.number(d)) return false;
            model.boundsRadius = (float)d;
            if (p.peek().kind == Token::Comma) p.next();
        } else {
            // NumGeosets/NumBones/BlendTime and future counts: ignored,
            // vectors size themselves from parsed content.
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

bool parseSequencesBlock(Parser& p, MdxModel& model) {
    double count = 0;
    if (!p.number(count)) return false;
    (void)count;
    if (!p.lbrace()) return false;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in Sequences");
        if (!peekWord(p, "Anim")) {
            if (!p.skipValue()) return false;
            continue;
        }
        p.next();
        Token name = p.next();
        if (name.kind != Token::String) return p.fail("expected anim name");
        MdxSequence seq;
        if (!parseSequenceAnim(p, seq, name.text)) return false;
        model.sequences.push_back(std::move(seq));
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

bool parseGlobalSequencesBlock(Parser& p, MdxModel& model) {
    double count = 0;
    if (!p.number(count)) return false;
    (void)count;
    if (!p.lbrace()) return false;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) {
            return p.fail("unexpected end in GlobalSequences");
        }
        if (peekWord(p, "Duration")) {
            p.next();
            int d = 0;
            if (!p.integer(d)) return false;
            if (d < 0) d = 0;
            model.globalSequences.push_back((uint32_t)d);
            if (p.peek().kind == Token::Comma) p.next();
        } else {
            if (!p.skipValue()) return false;
        }
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

bool parseTexturesBlock(Parser& p, MdxModel& model) {
    double count = 0;
    if (!p.number(count)) return false;
    (void)count;
    if (!p.lbrace()) return false;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in Textures");
        if (!peekWord(p, "Bitmap")) {
            if (!p.skipValue()) return false;
            continue;
        }
        p.next();
        if (!p.lbrace()) return false;
        MdxTextureRef tex;
        while (p.peek().kind != Token::RBrace) {
            if (p.peek().kind == Token::End) return p.fail("unexpected end in Bitmap");
            if (peekWord(p, "Image")) {
                if (!parseStringField(p, "Image", tex.path)) return false;
            } else if (peekWord(p, "ReplaceableId")) {
                p.next();
                int v = 0;
                if (!p.integer(v)) return false;
                tex.replaceableId = v;
                if (p.peek().kind == Token::Comma) p.next();
            } else {
                if (!p.skipValue()) return false;
            }
        }
        if (!p.rbrace()) return false;
        if (p.peek().kind == Token::Comma) p.next();
        model.textures.push_back(std::move(tex));
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

bool parseMaterialsBlock(Parser& p, MdxModel& model) {
    double count = 0;
    if (!p.number(count)) return false;
    (void)count;
    if (!p.lbrace()) return false;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) return p.fail("unexpected end in Materials");
        if (!peekWord(p, "Material")) {
            if (!p.skipValue()) return false;
            continue;
        }
        p.next();
        MdxMaterial mat;
        if (!parseMaterial(p, mat)) return false;
        model.materials.push_back(std::move(mat));
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

bool parseTextureAnimsBlock(Parser& p, MdxModel& model) {
    double count = 0;
    if (!p.number(count)) return false;
    (void)count;
    if (!p.lbrace()) return false;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) {
            return p.fail("unexpected end in TextureAnims");
        }
        if (!peekWord(p, "TVertexAnim")) {
            if (!p.skipValue()) return false;
            continue;
        }
        MdxTexAnim ta;
        if (!parseTexAnim(p, ta)) return false;
        model.texAnims.push_back(std::move(ta));
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

bool parsePivotPointsBlock(Parser& p, MdxModel& model) {
    double count = 0;
    if (!p.number(count)) return false;
    (void)count;
    if (!p.lbrace()) return false;
    while (p.peek().kind != Token::RBrace) {
        if (p.peek().kind == Token::End) {
            return p.fail("unexpected end in PivotPoints");
        }
        if (p.peek().kind != Token::LBrace) return p.fail("expected pivot triple");
        std::array<float, 3> pivot = {0, 0, 0};
        if (!p.lbrace()) return false;
        for (int i = 0; i < 3; i++) {
            double v = 0;
            if (!p.number(v)) return false;
            pivot[i] = (float)v;
            if (p.peek().kind == Token::Comma) p.next();
            else if (i < 2) return p.fail("expected ','");
        }
        if (!p.rbrace()) return false;
        if (p.peek().kind == Token::Comma) p.next();
        model.pivotPoints.push_back(pivot);
    }
    if (!p.rbrace()) return false;
    if (p.peek().kind == Token::Comma) p.next();
    return true;
}

// Pivot derivation: MDL carries no PIVT chunk, so pivots come from each
// node's rest Translation (static value, else frame-0 key, else origin),
// sized to the largest object id. Matches exporter behavior closely
// enough for skinning, and the animator tolerates missing entries.
void derivePivotPoints(MdxModel& model) {
    auto noteTranslation = [&](const std::vector<MdxTimeline>& timelines, int objectId,
                               std::vector<std::array<float, 3>>& pivots,
                               std::vector<char>& have) {
        if (objectId < 0) return;
        if ((size_t)objectId >= pivots.size()) {
            pivots.resize((size_t)objectId + 1, {0, 0, 0});
            have.resize((size_t)objectId + 1, 0);
        }
        if (have[(size_t)objectId]) return;
        for (const auto& tl : timelines) {
            if (tl.tag == MakeTag('K','G','T','R') && !tl.frames.empty() &&
                tl.values.size() >= 3 && tl.frames[0] == 0) {
                pivots[(size_t)objectId] = {tl.values[0], tl.values[1], tl.values[2]};
                have[(size_t)objectId] = 1;
                return;
            }
        }
    };
    // Seed pivots from static translations first (frame-0 keys also match
    // the frames[0]==0 rule above, so one pass suffices).
    std::vector<char> have;
    for (const auto& b : model.bones) noteTranslation(b.timelines, b.objectId,
                                                      model.pivotPoints, have);
    for (const auto& h : model.helpers) noteTranslation(h.timelines, h.objectId,
                                                        model.pivotPoints, have);
}

} // namespace

bool MdlLoad(const char* text, size_t size, MdxModel& out, std::string* error) {
    if (!text || size == 0) {
        if (error) *error = "empty input";
        return false;
    }
    out = MdxModel();
    Parser p(text, size, error);
    bool havePivots = false;
    while (p.peek().kind != Token::End) {
        Token t = p.peek();
        if (t.kind != Token::Word) return p.fail("expected top-level block");
        std::string kw = t.text;
        if (kw == "Version") {
            p.next();
            if (!parseVersionBlock(p, out)) return false;
        } else if (kw == "Model") {
            p.next();
            Token name = p.next();
            if (name.kind != Token::String) return p.fail("expected model name");
            if (!parseModelHead(p, out, name.text)) return false;
        } else if (kw == "Sequences") {
            p.next();
            if (!parseSequencesBlock(p, out)) return false;
        } else if (kw == "GlobalSequences") {
            p.next();
            if (!parseGlobalSequencesBlock(p, out)) return false;
        } else if (kw == "Textures") {
            p.next();
            if (!parseTexturesBlock(p, out)) return false;
        } else if (kw == "Materials") {
            p.next();
            if (!parseMaterialsBlock(p, out)) return false;
        } else if (kw == "TextureAnims") {
            p.next();
            if (!parseTextureAnimsBlock(p, out)) return false;
        } else if (kw == "Geoset") {
            p.next();
            MdxGeoset geo;
            if (!parseGeoset(p, geo)) return false;
            if (!geo.vertices.empty() && !geo.indices.empty()) {
                out.geosets.push_back(std::move(geo));
            }
        } else if (kw == "GeosetAnim") {
            p.next();
            MdxGeosetAnim ga;
            if (!parseGeosetAnim(p, ga)) return false;
            out.geosetAnims.push_back(std::move(ga));
        } else if (kw == "Bone" || kw == "Helper") {
            p.next();
            Token name = p.next();
            if (name.kind != Token::String) return p.fail("expected node name");
            MdxNode node;
            node.name = name.text;
            if (!parseBasicNodeBody(p, node, kw == "Bone")) return false;
            if (kw == "Bone") out.bones.push_back(std::move(node));
            else out.helpers.push_back(std::move(node));
        } else if (kw == "Attachment") {
            p.next();
            Token name = p.next();
            if (name.kind != Token::String) return p.fail("expected node name");
            MdxAttachment att;
            if (!parseAttachmentBody(p, att, name.text)) return false;
            out.attachments.push_back(std::move(att));
        } else if (kw == "Light") {
            p.next();
            Token name = p.next();
            if (name.kind != Token::String) return p.fail("expected node name");
            MdxLight lt;
            if (!parseLightBody(p, lt, name.text)) return false;
            out.lights.push_back(std::move(lt));
        } else if (kw == "ParticleEmitter") {
            p.next();
            Token name = p.peek();
            std::string nodeName;
            if (name.kind == Token::String) {
                p.next();
                nodeName = name.text;
            }
            MdxParticleEmitter1 em;
            if (!parseParticleEmitter1Body(p, em, nodeName)) return false;
            out.particleEmitters1.push_back(std::move(em));
        } else if (kw == "ParticleEmitter2") {
            p.next();
            Token name = p.peek();
            std::string nodeName;
            if (name.kind == Token::String) {
                p.next();
                nodeName = name.text;
            }
            MdxParticleEmitter2 em;
            if (!parseParticleEmitter2Body(p, em, nodeName)) return false;
            out.particleEmitters2.push_back(std::move(em));
        } else if (kw == "ParticleEmitterPopcorn") {
            p.next();
            Token name = p.peek();
            std::string nodeName;
            if (name.kind == Token::String) {
                p.next();
                nodeName = name.text;
            }
            MdxParticleEmitterPopcorn em;
            if (!parsePopcornBody(p, em, nodeName)) return false;
            out.particleEmittersPopcorn.push_back(std::move(em));
        } else if (kw == "RibbonEmitter") {
            p.next();
            Token name = p.peek();
            std::string nodeName;
            if (name.kind == Token::String) {
                p.next();
                nodeName = name.text;
            }
            MdxRibbonEmitter rb;
            if (!parseRibbonBody(p, rb, nodeName)) return false;
            out.ribbonEmitters.push_back(std::move(rb));
        } else if (kw == "EventObject") {
            p.next();
            Token name = p.next();
            if (name.kind != Token::String) return p.fail("expected node name");
            MdxEventObject ev;
            if (!parseEventBody(p, ev, name.text)) return false;
            out.eventObjects.push_back(std::move(ev));
        } else if (kw == "CollisionShape") {
            p.next();
            Token name = p.next();
            if (name.kind != Token::String) return p.fail("expected node name");
            MdxCollisionShape shape;
            if (!parseCollisionBody(p, shape, name.text)) return false;
            out.collisionShapes.push_back(std::move(shape));
        } else if (kw == "Camera") {
            p.next();
            Token name = p.next();
            if (name.kind != Token::String) return p.fail("expected camera name");
            MdxCamera cam;
            if (!parseCameraBody(p, cam, name.text)) return false;
            out.cameras.push_back(std::move(cam));
        } else if (kw == "PivotPoints") {
            p.next();
            if (!parsePivotPointsBlock(p, out)) return false;
            havePivots = true;
        } else {
            // Unknown top-level block: skip name/count prelude, then body.
            p.next();
            while (true) {
                Token u = p.peek();
                if (u.kind == Token::String || u.kind == Token::Number) {
                    p.next();
                    continue;
                }
                break;
            }
            if (p.peek().kind != Token::LBrace) {
                return p.fail("expected block body for '" + kw + "'");
            }
            if (!p.skipBlock()) return false;
        }
    }
    if (!havePivots) derivePivotPoints(out);
    return true;
}

