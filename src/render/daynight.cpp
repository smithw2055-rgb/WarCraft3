#include "daynight.h"
#include "textured_terrain.h"
#include "renderer.h"
#include <cmath>
#include <algorithm>
#include <iostream>

DayNightCycle::DayNightCycle() {
    Compute(0.5f);
}

void DayNightCycle::Update(float dt) {
    if (!enabled_) return;
    timeOfDay_ += dt / gameDayLength_;
    if (timeOfDay_ >= 1.0f) timeOfDay_ -= 1.0f;
    Compute(timeOfDay_);
}

void DayNightCycle::Compute(float time) {
    // Sun angle: 0 at midnight, rotates 360° per day
    // At noon (0.5): sun is directly overhead or slightly angled
    float angle = time * 2.0f * 3.14159265f;
    float sinAngle = std::sin(angle);
    float cosAngle = std::cos(angle);

    // Sun elevation: -1 at midnight (below horizon), +1 at noon (overhead)
    // Use cos(angle) with offset: midday is peak
    float elevation = std::cos(angle); // 1 at noon, -1 at midnight
    elevation = std::clamp(elevation, -0.3f, 1.0f); // sun doesn't go too far below

    // Sun position in sky (Y-up)
    // Rotates around Y axis, with elevation
    float yaw = angle + 3.14159265f; // sun rises in east (negative X), sets in west (positive X)
    sunDirX_ = std::sin(yaw) * std::sqrt(1.0f - elevation * elevation);
    sunDirY_ = elevation;
    sunDirZ_ = std::cos(yaw) * std::sqrt(1.0f - elevation * elevation);

    // Normalize
    float len = std::sqrt(sunDirX_ * sunDirX_ + sunDirY_ * sunDirY_ + sunDirZ_ * sunDirZ_);
    if (len > 0.0001f) {
        sunDirX_ /= len;
        sunDirY_ /= len;
        sunDirZ_ /= len;
    }

    // Sun intensity: full when above horizon, dim below
    float aboveHorizon = std::clamp((elevation + 0.2f) / 1.2f, 0.0f, 1.0f);

    // Sun color: warm white at noon, orange at sunrise/sunset
    float noonFactor = std::clamp(1.0f - std::abs(elevation - 1.0f) * 2.0f, 0.0f, 1.0f);
    float sunriseFactor = 1.0f - noonFactor;

    sunColor_[0] = (1.0f * noonFactor + 1.0f * sunriseFactor) * aboveHorizon;
    sunColor_[1] = (1.0f * noonFactor + 0.6f * sunriseFactor) * aboveHorizon;
    sunColor_[2] = (0.9f * noonFactor + 0.3f * sunriseFactor) * aboveHorizon;

    sunIntensity_ = 1.0f * aboveHorizon;

    // Ambient light: brighter during day, dimmer at night
    ambientIntensity_ = 0.2f + 0.4f * aboveHorizon;

    // Ambient color: warm during day, cool twilight, dark blue at night
    if (aboveHorizon > 0.5f) {
        // Day
        ambientColor_[0] = 0.4f; ambientColor_[1] = 0.4f; ambientColor_[2] = 0.5f;
    } else if (aboveHorizon > 0.1f) {
        // Twilight
        float twilight = (aboveHorizon - 0.1f) / 0.4f;
        ambientColor_[0] = 0.2f + 0.2f * twilight;
        ambientColor_[1] = 0.15f + 0.25f * twilight;
        ambientColor_[2] = 0.3f + 0.2f * twilight;
    } else {
        // Night
        ambientColor_[0] = 0.05f; ambientColor_[1] = 0.05f; ambientColor_[2] = 0.15f;
    }

    // Fog color: matches sky
    if (aboveHorizon > 0.5f) {
        fogColor_[0] = 0.5f + 0.3f * (1.0f - aboveHorizon);
        fogColor_[1] = 0.6f + 0.3f * (1.0f - aboveHorizon);
        fogColor_[2] = 0.7f + 0.2f * (1.0f - aboveHorizon);
    } else if (aboveHorizon > 0.1f) {
        float t = (aboveHorizon - 0.1f) / 0.4f;
        fogColor_[0] = 0.15f + 0.35f * t;
        fogColor_[1] = 0.1f + 0.5f * t;
        fogColor_[2] = 0.2f + 0.5f * t;
    } else {
        fogColor_[0] = 0.05f; fogColor_[1] = 0.03f; fogColor_[2] = 0.1f;
    }

    // Clear color (sky)
    clearColor_[0] = fogColor_[0] * 0.8f;
    clearColor_[1] = fogColor_[1] * 0.8f;
    clearColor_[2] = fogColor_[2] * 0.8f;
    clearColor_[3] = 1.0f;
}

void DayNightCycle::Apply(TexturedTerrain& terrain) {
    // Update LightManager
    LightManager& lm = terrain.GetLightManager();

    std::vector<LightInstance> lights;

    // Sun
    {
        LightInstance sun;
        sun.type = LightType_Directional;
        sun.position[0] = sunDirX_;
        sun.position[1] = sunDirY_;
        sun.position[2] = sunDirZ_;
        sun.color[0] = sunColor_[0];
        sun.color[1] = sunColor_[1];
        sun.color[2] = sunColor_[2];
        sun.intensity = sunIntensity_;
        sun.ambientColor[0] = ambientColor_[0];
        sun.ambientColor[1] = ambientColor_[1];
        sun.ambientColor[2] = ambientColor_[2];
        sun.ambientIntensity = ambientIntensity_;
        lights.push_back(sun);
    }

    // Fill light (cool, from left)
    {
        LightInstance fill;
        fill.type = LightType_Directional;
        fill.position[0] = -0.3f;
        fill.position[1] = 0.3f;
        fill.position[2] = 0.5f;
        fill.color[0] = 0.4f; fill.color[1] = 0.5f; fill.color[2] = 0.8f;
        fill.intensity = sunIntensity_ * 0.3f;
        fill.ambientColor[0] = 0.1f; fill.ambientColor[1] = 0.1f; fill.ambientColor[2] = 0.2f;
        fill.ambientIntensity = ambientIntensity_ * 0.3f;
        lights.push_back(fill);
    }

    // Ambient
    {
        LightInstance amb;
        amb.type = LightType_Ambient;
        amb.ambientColor[0] = ambientColor_[0];
        amb.ambientColor[1] = ambientColor_[1];
        amb.ambientColor[2] = ambientColor_[2];
        amb.ambientIntensity = ambientIntensity_;
        amb.attenuationEnd = 100000.0f;
        lights.push_back(amb);
    }

    lm.SetLights(lights);

    // Update terrain fog color
    terrain.SetFog(fogColor_, 4000.0f, 12000.0f);

    // Update renderer clear color (sky)
    Renderer::SetClearColor(clearColor_);
}
