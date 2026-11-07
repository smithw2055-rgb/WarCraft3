#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include <cstdint>
#include <vector>

class FogOfWar {
public:
    static constexpr int kTileSize = 16;

    FogOfWar();
    ~FogOfWar();

    void Init(int width, int height);
    // Sets both display and target fog immediately (instant snap).
    void SetData(const unsigned char* data);
    // Stage X2: Sets target fog buffer for temporal smooth interpolation.
    void SetTargetData(const unsigned char* targetData);
    // Advances display fog (data_) smoothly toward targetData_ at speedPerSec gray levels/sec.
    // Returns the number of texels modified during this step.
    int StepSmoothTransition(float dt, float speedPerSec = 512.0f);
    void ReloadToGPU();

    sg_image GetTexture() const { return texture_; }
    sg_sampler GetSampler() const { return sampler_; }
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    const unsigned char* GetData() const { return data_; }
    const unsigned char* GetTargetData() const { return targetData_; }
    int GetDirtyTileCount() const;
    void ClearDirtyTiles();

private:
    sg_image texture_ = {0};
    sg_sampler sampler_ = {0};
    int width_ = 0, height_ = 0;
    int tilesX_ = 0, tilesY_ = 0;
    unsigned char* data_ = nullptr;
    unsigned char* targetData_ = nullptr;
    std::vector<uint8_t> dirtyTiles_;
};
