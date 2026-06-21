#pragma once

class SoftwareGroundMesh {
public:
    SoftwareGroundMesh();
    ~SoftwareGroundMesh();

    void Init(const float* groundHeights, const float* cornerHeights,
              float centerOffsetX, float centerOffsetY, int width, int height);

    float GetHeightAt(float worldX, float worldY) const;

private:
    const float* groundHeights_ = nullptr;
    const float* cornerHeights_ = nullptr;
    float centerOffsetX_ = 0, centerOffsetY_ = 0;
    int width_ = 0, height_ = 0;
};

class SoftwareWaterAndGroundMesh {
public:
    SoftwareWaterAndGroundMesh();
    ~SoftwareWaterAndGroundMesh();

    void Init(const float* cornerHeights, const float* waterHeights,
              const unsigned char* waterExists, float waterHeightOffset,
              float centerOffsetX, float centerOffsetY, int width, int height);

    float GetHeightAt(float worldX, float worldY) const;
    float GetWaterHeightAt(float worldX, float worldY) const;
    bool HasWaterAt(float worldX, float worldY) const;

private:
    const float* cornerHeights_ = nullptr;
    const float* waterHeights_ = nullptr;
    const unsigned char* waterExists_ = nullptr;
    float waterHeightOffset_ = 0;
    float centerOffsetX_ = 0, centerOffsetY_ = 0;
    int width_ = 0, height_ = 0;
};
