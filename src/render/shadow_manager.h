#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include <string>
#include <vector>
#include <unordered_map>

class DynamicShadowManager {
public:
    DynamicShadowManager();
    ~DynamicShadowManager();

    void Init(int columns, int rows);
    void SetupFrame();

    sg_image GetShadowMap() const { return shadowMap_; }
    int GetShadowWidth() const { return shadowWidth_; }
    int GetShadowHeight() const { return shadowHeight_; }

    void LoadStaticShadows(const unsigned char* data, int size);

    void AddShadow(float worldX, float worldY, int width, int height, const unsigned char* alphaData);
    void RemoveShadow(int index);
    void ReloadToGPU();

    void BlitShadowData(float worldX, float worldY, int texWidth, int texHeight,
                        const unsigned char* rgbaData, const float* centerOffset);

    unsigned char* GetShadowData() { return shadowData_; }

private:
    sg_image shadowMap_ = {0};
    sg_sampler shadowSampler_ = {0};
    int columns_ = 0, rows_ = 0;
    int shadowWidth_ = 0, shadowHeight_ = 0;
    unsigned char* shadowData_ = nullptr;
    unsigned char* staticShadowData_ = nullptr;
    float depthBiasMVP_[16] = {};
    float centerOffset_[2] = {};
};
