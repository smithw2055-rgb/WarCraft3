#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include "math.h"
#include <vector>
#include <cstdint>

// Light types matching Java reference shader:
//   lightExtra.x <= 0.5     → OMNIDIRECTIONAL (point light)
//   0.5 < lightExtra.x <= 1.5 → DIRECTIONAL (sun)
//   lightExtra.x > 1.5      → AMBIENT (ambient sphere)
enum LightType {
    LightType_Omni = 0,
    LightType_Directional = 1,
    LightType_Ambient = 2,
};

struct LightInstance {
    int type = LightType_Directional;
    float position[3] = {};   // for omni/ambient: world position; for directional: direction
    float color[3] = {1,1,1};
    float intensity = 1.0f;
    float ambientColor[3] = {0,0,0};
    float ambientIntensity = 0.3f;
    float attenuationStart = 0;
    float attenuationEnd = 0;
};

class LightManager {
public:
    LightManager();
    ~LightManager();

    void Init();
    void Update();

    // Access lights
    void SetLights(const std::vector<LightInstance>& lights);
    void SetDefaultLights(); // ambient + directional sun

    // GPU resources
    sg_image GetLightTexture() const { return lightTexture_; }
    sg_sampler GetLightSampler() const { return lightSampler_; }
    int GetLightCount() const { return (int)lights_.size(); }
    float GetLightTextureHeight() const { return (float)lights_.size(); }

    // Bind to shader uniform slots (terrain: unit 3/21, ground VS: unit 3)
    void Bind(sg_shader shader);

private:
    std::vector<LightInstance> lights_;
    sg_image lightTexture_ = {0};
    sg_sampler lightSampler_ = {0};
    bool needsUpload_ = false;
};
