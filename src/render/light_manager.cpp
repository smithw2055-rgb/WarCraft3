#include "light_manager.h"
#include "render/rhi_device.h"
#include <cstring>
#include <iostream>
#include <algorithm>

// RGBA32F texture: 4 columns, N rows. One row per light.
// Column layout:
//   0 (x=0.125): position.xyz / direction.xyz + padding
//   1 (x=0.375): extra = (type, attenuationStart, attenuationEnd, 0)
//   2 (x=0.625): color.rgb + intensity
//   3 (x=0.875): ambientColor.rgb + ambientIntensity
static const int kTexWidth = 4;
static const int kFloatsPerLight = kTexWidth * 4; // 16 floats per light

LightManager::LightManager() = default;

LightManager::~LightManager() {
    if (!Renderer::IsActive()) return;
    if (lightTexture_.id && sg_query_image_state(lightTexture_) == SG_RESOURCESTATE_VALID)
        sg_destroy_image(lightTexture_);
    if (lightSampler_.id && sg_query_sampler_state(lightSampler_) == SG_RESOURCESTATE_VALID)
        sg_destroy_sampler(lightSampler_);
}

void LightManager::Init() {
    SetDefaultLights();

    sg_sampler_desc smp = {};
    smp.min_filter = SG_FILTER_NEAREST;
    smp.mag_filter = SG_FILTER_NEAREST;
    smp.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smp.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    lightSampler_ = sg_make_sampler(&smp);

    needsUpload_ = true;
}

void LightManager::SetDefaultLights() {
    lights_.clear();

    // Light 0: Directional sun (from upper-right, warm color)
    {
        LightInstance sun;
        sun.type = LightType_Directional;
        // Direction from upper-right-front
        sun.position[0] = 0.5f;
        sun.position[1] = 1.0f;
        sun.position[2] = 0.3f;
        sun.color[0] = 1.0f; sun.color[1] = 0.9f; sun.color[2] = 0.7f;
        sun.intensity = 1.0f;
        sun.ambientColor[0] = 0.4f; sun.ambientColor[1] = 0.4f; sun.ambientColor[2] = 0.5f;
        sun.ambientIntensity = 0.5f;
        lights_.push_back(sun);
    }

    // Light 1: Fill light (from left, cool color)
    {
        LightInstance fill;
        fill.type = LightType_Directional;
        fill.position[0] = -0.3f;
        fill.position[1] = 0.5f;
        fill.position[2] = 0.5f;
        fill.color[0] = 0.6f; fill.color[1] = 0.7f; fill.color[2] = 1.0f;
        fill.intensity = 0.4f;
        fill.ambientColor[0] = 0.2f; fill.ambientColor[1] = 0.2f; fill.ambientColor[2] = 0.3f;
        fill.ambientIntensity = 0.2f;
        lights_.push_back(fill);
    }

    // Light 2: Ambient light (omnidirectional ambient)
    {
        LightInstance amb;
        amb.type = LightType_Ambient;
        amb.position[0] = 0; amb.position[1] = 1; amb.position[2] = 0;
        amb.color[0] = 0.5f; amb.color[1] = 0.5f; amb.color[2] = 0.5f;
        amb.intensity = 0.3f;
        amb.ambientColor[0] = 0.3f; amb.ambientColor[1] = 0.3f; amb.ambientColor[2] = 0.3f;
        amb.ambientIntensity = 0.3f;
        amb.attenuationStart = 0;
        amb.attenuationEnd = 100000; // covers entire scene
        lights_.push_back(amb);
    }

    needsUpload_ = true;
    std::cout << "LightManager: " << lights_.size() << " default lights initialized" << std::endl;
}

void LightManager::SetLights(const std::vector<LightInstance>& lights) {
    lights_ = lights;
    needsUpload_ = true;
}

void LightManager::Update() {
    if (!needsUpload_) return;
    needsUpload_ = false;

    if (lights_.empty()) return;

    int height = (int)lights_.size();
    std::vector<float> texData(kTexWidth * 4 * height, 0.0f);

    for (int i = 0; i < height; i++) {
        const auto& light = lights_[i];
        float* row = texData.data() + i * kFloatsPerLight;

        // Column 0: position/direction
        row[0] = light.position[0];
        row[1] = light.position[1];
        row[2] = light.position[2];
        row[3] = 0; // padding

        // Column 1: extra (type, attenuationStart, attenuationEnd, padding)
        row[4] = (float)(light.type);
        row[5] = light.attenuationStart;
        row[6] = light.attenuationEnd;
        row[7] = 0;

        // Column 2: color + intensity
        row[8] = light.color[0];
        row[9] = light.color[1];
        row[10] = light.color[2];
        row[11] = light.intensity;

        // Column 3: ambient color + ambient intensity
        row[12] = light.ambientColor[0];
        row[13] = light.ambientColor[1];
        row[14] = light.ambientColor[2];
        row[15] = light.ambientIntensity;
    }

    // The light texture is updated in place (dynamic_update) so the
    // terrain's cached view stays valid across day/night changes. Only
    // re-create when the light count changes.
    const bool canUpdateInPlace =
        uploadedHeight_ == height &&
        sg_query_image_state(lightTexture_) == SG_RESOURCESTATE_VALID;

    sg_image_data data = {};
    data.mip_levels[0].ptr = texData.data();
    data.mip_levels[0].size = texData.size() * sizeof(float);

    if (canUpdateInPlace) {
        sg_update_image(lightTexture_, &data);
        return;
    }

    if (sg_query_image_state(lightTexture_) == SG_RESOURCESTATE_VALID)
        sg_destroy_image(lightTexture_);
    lightTexture_ = {0};

    sg_image_desc desc = {};
    desc.type = SG_IMAGETYPE_2D;
    desc.width = kTexWidth;
    desc.height = height;
    desc.pixel_format = SG_PIXELFORMAT_RGBA32F;
    desc.num_mipmaps = 1;
    desc.data = data;
    desc.usage.immutable = false;
    desc.usage.dynamic_update = true;
    lightTexture_ = sg_make_image(&desc);

    if (sg_query_image_state(lightTexture_) != SG_RESOURCESTATE_VALID) {
        std::cerr << "LightManager: failed to create light texture" << std::endl;
        lightTexture_ = {0};
        uploadedHeight_ = 0;
    } else {
        uploadedHeight_ = height;
        std::cout << "LightManager: uploaded " << height << " lights"
                  << " (tex=" << lightTexture_.id << ")" << std::endl;
    }
}

void LightManager::Bind(sg_shader shader) {
    (void)shader;
    // The shader binding happens in textured_terrain's Render() via explicit
    // sg_bindings setup. This method is provided for future use if we want
    // to bind via shader introspection.
}
