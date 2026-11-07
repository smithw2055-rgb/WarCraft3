#include "fog_of_war.h"
#include "renderer.h"
#include <algorithm>
#include <cmath>
#include <cstring>

FogOfWar::FogOfWar() = default;

FogOfWar::~FogOfWar() {
    delete[] data_;
    delete[] targetData_;
    if (!Renderer::IsActive()) return;
    if (texture_.id && sg_query_image_state(texture_) == SG_RESOURCESTATE_VALID)
        sg_destroy_image(texture_);
    if (sampler_.id && sg_query_sampler_state(sampler_) == SG_RESOURCESTATE_VALID)
        sg_destroy_sampler(sampler_);
}

void FogOfWar::Init(int width, int height) {
    delete[] data_;
    delete[] targetData_;
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    tilesX_ = (width_ + kTileSize - 1) / kTileSize;
    tilesY_ = (height_ + kTileSize - 1) / kTileSize;
    data_ = new unsigned char[(size_t)width_ * height_]();
    targetData_ = new unsigned char[(size_t)width_ * height_]();
    dirtyTiles_.assign((size_t)tilesX_ * tilesY_, 1);

    if (!Renderer::IsActive()) return;
    if (texture_.id && sg_query_image_state(texture_) == SG_RESOURCESTATE_VALID)
        sg_destroy_image(texture_);
    if (sampler_.id && sg_query_sampler_state(sampler_) == SG_RESOURCESTATE_VALID)
        sg_destroy_sampler(sampler_);

    sg_image_desc imgDesc = {};
    imgDesc.type = SG_IMAGETYPE_2D;
    imgDesc.usage.immutable = false;
    imgDesc.usage.dynamic_update = true;
    imgDesc.width = width_;
    imgDesc.height = height_;
    imgDesc.pixel_format = SG_PIXELFORMAT_R8;
    imgDesc.num_mipmaps = 1;
    imgDesc.data.mip_levels[0].size = 0;
    imgDesc.data.mip_levels[0].ptr = nullptr;
    texture_ = sg_make_image(&imgDesc);

    sg_sampler_desc smpDesc = {};
    smpDesc.min_filter = SG_FILTER_LINEAR;
    smpDesc.mag_filter = SG_FILTER_LINEAR;
    smpDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smpDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    sampler_ = sg_make_sampler(&smpDesc);
}

void FogOfWar::SetData(const unsigned char* data) {
    if (!data || !data_ || !targetData_) return;
    const size_t total = (size_t)width_ * height_;
    std::memcpy(data_, data, total);
    std::memcpy(targetData_, data, total);
    std::fill(dirtyTiles_.begin(), dirtyTiles_.end(), 1);
}

void FogOfWar::SetTargetData(const unsigned char* targetData) {
    if (!targetData || !targetData_) return;
    std::memcpy(targetData_, targetData, (size_t)width_ * height_);
}

int FogOfWar::StepSmoothTransition(float dt, float speedPerSec) {
    if (!data_ || !targetData_ || dt <= 0.0f || speedPerSec <= 0.0f) return 0;
    const int maxStep = std::max(1, (int)std::lround(dt * speedPerSec));
    int modifiedTexels = 0;

    for (int row = 0; row < height_; ++row) {
        const size_t rowOffset = (size_t)row * width_;
        const int ty = row / kTileSize;
        for (int col = 0; col < width_; ++col) {
            const size_t idx = rowOffset + col;
            const int cur = (int)data_[idx];
            const int tgt = (int)targetData_[idx];
            if (cur == tgt) continue;

            int nextVal = cur;
            if (cur < tgt) {
                nextVal = std::min(tgt, cur + maxStep);
            } else {
                nextVal = std::max(tgt, cur - maxStep);
            }
            data_[idx] = static_cast<unsigned char>(nextVal);
            dirtyTiles_[(size_t)ty * tilesX_ + (col / kTileSize)] = 1;
            ++modifiedTexels;
        }
    }
    return modifiedTexels;
}

int FogOfWar::GetDirtyTileCount() const {
    int count = 0;
    for (uint8_t d : dirtyTiles_) {
        if (d != 0) ++count;
    }
    return count;
}

void FogOfWar::ClearDirtyTiles() {
    std::fill(dirtyTiles_.begin(), dirtyTiles_.end(), 0);
}

void FogOfWar::ReloadToGPU() {
    if (!Renderer::IsActive() || !texture_.id || !data_) return;
    sg_image_data imgData = {};
    imgData.mip_levels[0].ptr = data_;
    imgData.mip_levels[0].size = (size_t)(width_ * height_);
    sg_update_image(texture_, &imgData);
    ClearDirtyTiles();
}
