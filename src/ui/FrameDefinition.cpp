#include "UiTypes.h"
#include <functional>

void FrameDefinition::InheritFrom(const FrameDefinition& other, bool withChildren) {
    if (frameType.empty() && !other.frameType.empty()) frameType = other.frameType;
    flags.insert(other.flags.begin(), other.flags.end());
    for (auto& kv : other.stringFields) {
        if (!stringFields.count(kv.first)) stringFields[kv.first] = kv.second;
    }
    for (auto& kv : other.stringPairFields) {
        if (!stringPairFields.count(kv.first)) {
            stringPairFields[kv.first] = kv.second;
            if (!stringFields.count(kv.first)) stringFields[kv.first] = kv.second.first;
        }
    }
    for (auto& kv : other.floatFields) {
        if (!floatFields.count(kv.first)) floatFields[kv.first] = kv.second;
    }
    for (auto& kv : other.vector4Fields) {
        if (!vector4Fields.count(kv.first)) vector4Fields[kv.first] = kv.second;
    }
    for (auto& kv : other.vector2Fields) {
        if (!vector2Fields.count(kv.first)) vector2Fields[kv.first] = kv.second;
    }
    for (auto& kv : other.vector3Fields) {
        if (!vector3Fields.count(kv.first)) vector3Fields[kv.first] = kv.second;
    }
    for (auto& kv : other.fontFields) {
        if (!fontFields.count(kv.first)) fontFields[kv.first] = kv.second;
    }
    if (setPoints.empty()) {
        setPoints = other.setPoints;
    }
    if (anchors.empty()) {
        anchors = other.anchors;
    }
    if (menuItems.empty()) {
        menuItems = other.menuItems;
    }
    if (withChildren) {
        innerFrames.insert(innerFrames.end(), other.innerFrames.begin(), other.innerFrames.end());
    }
}

static bool ResolveDefRecursively(FrameDefinition& def, const std::map<std::string, FrameDefinition>& templates) {
    bool progress = false;
    if (!def.inheritsFrom.empty()) {
        auto it = templates.find(def.inheritsFrom);
        if (it != templates.end() && it->first != def.name && it->second.inheritsFrom.empty()) {
            def.InheritFrom(it->second, def.inheritsWithChildren);
            def.inheritsFrom.clear();
            progress = true;
        }
    }
    for (auto& inner : def.innerFrames) {
        if (ResolveDefRecursively(inner, templates)) {
            progress = true;
        }
    }
    return progress;
}

int ResolveFdfInheritance(std::map<std::string, FrameDefinition>& templates) {
    // Fixpoint: apply a parent only once IT has no pending parent, so
    // grandchildren observe fully-resolved fields regardless of file order.
    // Recursively resolves all innerFrames so nested frames and templates
    // inherit their sizes, anchors, and backdrops.
    for (int pass = 0; pass < 64; ++pass) {
        bool progress = false;
        for (auto& [name, def] : templates) {
            if (ResolveDefRecursively(def, templates)) {
                progress = true;
            }
        }
        if (!progress) break;
    }
    int unresolved = 0;
    std::function<void(const FrameDefinition&)> countUnresolved = [&](const FrameDefinition& d) {
        if (!d.inheritsFrom.empty()) unresolved++;
        for (const auto& inner : d.innerFrames) countUnresolved(inner);
    };
    for (const auto& [name, def] : templates) {
        countUnresolved(def);
    }
    return unresolved;
}
