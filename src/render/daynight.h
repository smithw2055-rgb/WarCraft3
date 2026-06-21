#pragma once
#include "light_manager.h"

class TexturedTerrain;

class DayNightCycle {
public:
    DayNightCycle();

    // Time: 0.0 = midnight, 0.25 = dawn, 0.5 = noon, 0.75 = dusk
    void SetTimeOfDay(float t) { timeOfDay_ = t; }
    float GetTimeOfDay() const { return timeOfDay_; }

    // Advance time by dt (seconds). A full day takes gameDayLength_ seconds.
    void Update(float dt);

    // Apply lighting to LightManager and fog to TexturedTerrain
    void Apply(TexturedTerrain& terrain);

    // Config
    void SetDayLength(float seconds) { gameDayLength_ = seconds; }
    void SetEnabled(bool en) { enabled_ = en; }
    bool IsEnabled() const { return enabled_; }

    // Get current sky/fog color
    const float* GetFogColor() const { return fogColor_; }
    const float* GetClearColor() const { return clearColor_; }

    // Sun direction for external use
    float GetSunDirectionX() const { return sunDirX_; }
    float GetSunDirectionY() const { return sunDirY_; }
    float GetSunDirectionZ() const { return sunDirZ_; }
    float GetSunIntensity() const { return sunIntensity_; }

private:
    void Compute(float time);

    float timeOfDay_ = 0.5f; // start at noon
    float gameDayLength_ = 480.0f; // 8 real minutes = 1 game day
    bool enabled_ = true;

    // Computed values
    float sunDirX_ = 0, sunDirY_ = 1, sunDirZ_ = 0;
    float sunColor_[3] = {1, 1, 1};
    float sunIntensity_ = 1.0f;
    float ambientIntensity_ = 0.5f;
    float ambientColor_[3] = {0.4f, 0.4f, 0.5f};
    float fogColor_[3] = {0.7f, 0.7f, 0.7f};
    float clearColor_[4] = {0.3f, 0.5f, 0.7f, 1.0f};
};
