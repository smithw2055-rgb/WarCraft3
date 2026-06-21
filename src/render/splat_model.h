#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include <string>
#include <vector>
#include "math.h"

class SplatModel {
public:
    SplatModel();
    ~SplatModel();

    bool LoadTexture(const std::string& path, sg_image tex);

    void AddSplat(float centerX, float centerY, float scale, float rotation);
    void RemoveSplat(int index);

    void Render(sg_shader uberSplatShader, const Mat4& mvp, float centerOffset[2],
                int mapCols, int mapRows, sg_image heightMap, sg_image shadowMap,
                sg_image waterHeightMap, sg_image fogOfWarMap, float waterHeightOffset);

    bool IsHighPriority() const { return highPriority_; }
    void SetHighPriority(bool v) { highPriority_ = v; }

private:
    sg_image texture_ = {0};
    sg_sampler sampler_ = {0};
    bool highPriority_ = false;

    struct SplatInstance {
        float x, y;
        float scale;
        float rotation;
    };
    std::vector<SplatInstance> instances_;
    sg_buffer vbo_ = {0};
    sg_pipeline pipeline_ = {0};
};
