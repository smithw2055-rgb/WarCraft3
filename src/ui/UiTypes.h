#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstdint>

enum class FrameClass {
    Frame,
    String,
    Texture,
    Layer
};

enum class FramePoint {
    TOPLEFT,
    TOP,
    TOPRIGHT,
    LEFT,
    CENTER,
    RIGHT,
    BOTTOMLEFT,
    BOTTOM,
    BOTTOMRIGHT
};

struct SetPointDef {
    FramePoint myPoint;
    std::string otherFrame;
    FramePoint otherPoint;
    float x = 0, y = 0;
};

struct AnchorDef {
    FramePoint point;
    float x = 0, y = 0;
};

struct FontDef {
    std::string name;
    float size = 12;
    std::string path;
    std::string flags;
};

struct Vector4Def {
    float x = 0, y = 0, z = 0, w = 0;
};

struct Vector2Def {
    float x = 0, y = 0;
};

struct Vector3Def {
    float x = 0, y = 0, z = 0;
};

class FrameDefinition {
public:
    FrameClass frameClass = FrameClass::Frame;
    std::string frameType;
    std::string name;

    std::set<std::string> flags;
    std::map<std::string, std::string> stringFields;
    std::map<std::string, std::pair<std::string, std::string>> stringPairFields;
    std::map<std::string, float> floatFields;
    std::map<std::string, Vector4Def> vector4Fields;
    std::map<std::string, Vector2Def> vector2Fields;
    std::map<std::string, Vector3Def> vector3Fields;
    std::map<std::string, FontDef> fontFields;
    std::vector<std::pair<std::string, int>> menuItems;
    std::vector<SetPointDef> setPoints;
    std::vector<AnchorDef> anchors;
    std::vector<FrameDefinition> innerFrames;

    // Deferred cross-file inheritance (ref multi-file FrameDef.toc loads):
    // the parser fills these only when the parent is missing or itself
    // unresolved in the same file; ResolveFdfInheritance applies them in
    // dependency order and clears them, so nothing merges twice.
    std::string inheritsFrom;
    bool inheritsWithChildren = false;

    FrameDefinition() = default;
    FrameDefinition(FrameClass fc, const std::string& type, const std::string& name_)
        : frameClass(fc), frameType(type), name(name_) {}

    bool HasFlag(const std::string& flag) const { return flags.find(flag) != flags.end(); }

    std::string GetString(const std::string& key, const std::string& def = "") const {
        auto it = stringFields.find(key);
        return it != stringFields.end() ? it->second : def;
    }

    float GetFloat(const std::string& key, float def = 0) const {
        auto it = floatFields.find(key);
        return it != floatFields.end() ? it->second : def;
    }

    Vector4Def GetVector4(const std::string& key) const {
        auto it = vector4Fields.find(key);
        return it != vector4Fields.end() ? it->second : Vector4Def{};
    }

    bool HasField(const std::string& key) const {
        return stringFields.count(key) || stringPairFields.count(key) ||
               floatFields.count(key) ||
               vector4Fields.count(key) || vector2Fields.count(key) ||
               fontFields.count(key);
    }

    void InheritFrom(const FrameDefinition& other, bool withChildren);
};

// Resolve deferred INHERITS links across a merged template map (fixpoint,
// cycle-safe). Returns the number of still-unresolved parents.
int ResolveFdfInheritance(std::map<std::string, FrameDefinition>& templates);
