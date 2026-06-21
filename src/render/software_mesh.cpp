#include "software_mesh.h"
#include <algorithm>
#include <cmath>

SoftwareGroundMesh::SoftwareGroundMesh() = default;
SoftwareGroundMesh::~SoftwareGroundMesh() = default;

void SoftwareGroundMesh::Init(const float* groundHeights, const float* cornerHeights,
                               float centerOffsetX, float centerOffsetY, int width, int height) {
    groundHeights_ = groundHeights;
    cornerHeights_ = cornerHeights;
    centerOffsetX_ = centerOffsetX;
    centerOffsetY_ = centerOffsetY;
    width_ = width;
    height_ = height;
}

float SoftwareGroundMesh::GetHeightAt(float worldX, float worldY) const {
    if (!cornerHeights_ || width_ < 1 || height_ < 1) return 0.0f;

    float gx = worldX - centerOffsetX_;
    float gy = worldY - centerOffsetY_;

    float cx = gx / 128.0f;
    float cy = gy / 128.0f;

    int ix = (int)std::floor(cx);
    int iy = (int)std::floor(cy);

    float fx = cx - ix;
    float fy = cy - iy;

    ix = std::max(0, std::min(ix, width_ - 2));
    iy = std::max(0, std::min(iy, height_ - 2));

    int idx = iy * width_ + ix;
    float h00 = cornerHeights_[idx];
    float h10 = cornerHeights_[idx + 1];
    float h01 = cornerHeights_[idx + width_];
    float h11 = cornerHeights_[idx + width_ + 1];

    float h0 = h00 + (h10 - h00) * fx;
    float h1 = h01 + (h11 - h01) * fx;
    return h0 + (h1 - h0) * fy;
}

SoftwareWaterAndGroundMesh::SoftwareWaterAndGroundMesh() = default;
SoftwareWaterAndGroundMesh::~SoftwareWaterAndGroundMesh() = default;

void SoftwareWaterAndGroundMesh::Init(const float* cornerHeights, const float* waterHeights,
                                       const unsigned char* waterExists, float waterHeightOffset,
                                       float centerOffsetX, float centerOffsetY, int width, int height) {
    cornerHeights_ = cornerHeights;
    waterHeights_ = waterHeights;
    waterExists_ = waterExists;
    waterHeightOffset_ = waterHeightOffset;
    centerOffsetX_ = centerOffsetX;
    centerOffsetY_ = centerOffsetY;
    width_ = width;
    height_ = height;
}

float SoftwareWaterAndGroundMesh::GetHeightAt(float worldX, float worldY) const {
    if (!cornerHeights_ || width_ < 1 || height_ < 1) return 0.0f;

    float gx = worldX - centerOffsetX_;
    float gy = worldY - centerOffsetY_;

    float cx = gx / 128.0f;
    float cy = gy / 128.0f;

    int ix = (int)std::floor(cx);
    int iy = (int)std::floor(cy);

    float fx = cx - ix;
    float fy = cy - iy;

    ix = std::max(0, std::min(ix, width_ - 2));
    iy = std::max(0, std::min(iy, height_ - 2));

    int idx = iy * width_ + ix;
    float h00 = cornerHeights_[idx];
    float h10 = cornerHeights_[idx + 1];
    float h01 = cornerHeights_[idx + width_];
    float h11 = cornerHeights_[idx + width_ + 1];

    float h0 = h00 + (h10 - h00) * fx;
    float h1 = h01 + (h11 - h01) * fx;
    return h0 + (h1 - h0) * fy;
}

float SoftwareWaterAndGroundMesh::GetWaterHeightAt(float worldX, float worldY) const {
    if (!waterHeights_ || width_ < 1 || height_ < 1) return 0.0f;

    float gx = worldX - centerOffsetX_;
    float gy = worldY - centerOffsetY_;

    float cx = gx / 128.0f;
    float cy = gy / 128.0f;

    int ix = (int)std::floor(cx);
    int iy = (int)std::floor(cy);

    float fx = cx - ix;
    float fy = cy - iy;

    ix = std::max(0, std::min(ix, width_ - 2));
    iy = std::max(0, std::min(iy, height_ - 2));

    int idx = iy * width_ + ix;
    float h00 = waterHeights_[idx];
    float h10 = waterHeights_[idx + 1];
    float h01 = waterHeights_[idx + width_];
    float h11 = waterHeights_[idx + width_ + 1];

    float h0 = h00 + (h10 - h00) * fx;
    float h1 = h01 + (h11 - h01) * fx;
    return h0 + (h1 - h0) * fy + waterHeightOffset_;
}

bool SoftwareWaterAndGroundMesh::HasWaterAt(float worldX, float worldY) const {
    if (!waterExists_ || width_ < 1 || height_ < 1) return false;

    float gx = worldX - centerOffsetX_;
    float gy = worldY - centerOffsetY_;

    float cx = gx / 128.0f;
    float cy = gy / 128.0f;

    int ix = (int)std::floor(cx);
    int iy = (int)std::floor(cy);

    ix = std::max(0, std::min(ix, width_ - 1));
    iy = std::max(0, std::min(iy, height_ - 1));

    return waterExists_[iy * width_ + ix] != 0;
}
