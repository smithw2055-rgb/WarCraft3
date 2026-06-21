#include "UiTypes.h"

void FrameDefinition::InheritFrom(const FrameDefinition& other, bool withChildren) {
    flags.insert(other.flags.begin(), other.flags.end());
    for (auto& kv : other.stringFields) {
        if (!stringFields.count(kv.first)) stringFields[kv.first] = kv.second;
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
    if (withChildren) {
        innerFrames.insert(innerFrames.end(), other.innerFrames.begin(), other.innerFrames.end());
    }
}
