#pragma once

#include "model/mdx.h"
#include "render/mdx_animator.h"
#include "resource/slk.h"

#include <array>
#include <string>
#include <vector>

class MpqManager;

struct MdxEventEffect {
    bool resolved = false;
    std::string type;
    std::string id;
    std::string texturePath;
    std::string modelPath;
    std::vector<std::string> soundPaths;
    float scale = 0.0f;
    float lifeSpan = 0.0f;
    float volume = 1.0f;
    int columns = 1;
    int rows = 1;
    int blendMode = 0;
    bool footprint = false;
    bool unshaded = false;
    std::array<std::array<float, 4>, 3> colors = {{
        {{255.0f, 255.0f, 255.0f, 255.0f}},
        {{255.0f, 255.0f, 255.0f, 255.0f}},
        {{255.0f, 255.0f, 255.0f, 0.0f}}
    }};
    std::array<float, 3> intervalTimes = {{0.0f, 0.0f, 0.0f}};
    std::array<std::array<float, 3>, 2> uvIntervals = {{
        {{0.0f, 0.0f, 0.0f}},
        {{0.0f, 0.0f, 0.0f}}
    }};
    int intervalCount = 0;
    bool hasLocalTransform = false;
    Mat4 localTransform = Mat4::Identity();
    Vec3 localPosition = {0,0,0};
    Vec3 localScale = {1,1,1};
};

class MdxEventEffectResolver {
public:
    bool Load(MpqManager& mpq);

    MdxEventEffect Resolve(const MdxEventObject& eventObject) const;
    MdxEventEffect Resolve(const MdxEventTrigger& trigger) const;

    bool HasSplatData() const { return hasSplatData_; }
    bool HasUberSplatData() const { return hasUberSplatData_; }
    bool HasSpawnData() const { return hasSpawnData_; }
    bool HasSoundData() const { return hasAnimSounds_; }

private:
    MdxEventEffect ResolveByTypeAndId(const std::string& type, const std::string& id) const;

    SlkTable splatData_;
    SlkTable uberSplatData_;
    SlkTable spawnData_;
    SlkTable animLookups_;
    SlkTable animSounds_;
    bool hasSplatData_ = false;
    bool hasUberSplatData_ = false;
    bool hasSpawnData_ = false;
    bool hasAnimLookups_ = false;
    bool hasAnimSounds_ = false;
};
