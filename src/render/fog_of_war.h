#pragma once
#include "third_party/sokol/sokol_gfx.h"

class FogOfWar {
public:
    FogOfWar();
    ~FogOfWar();

    void Init(int width, int height);
    void SetData(const unsigned char* data);
    void ReloadToGPU();

    sg_image GetTexture() const { return texture_; }
    sg_sampler GetSampler() const { return sampler_; }
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }

private:
    sg_image texture_ = {0};
    sg_sampler sampler_ = {0};
    int width_ = 0, height_ = 0;
    unsigned char* data_ = nullptr;
};
