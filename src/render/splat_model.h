#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include <string>
#include <vector>
#include "math.h"

class SplatModel {
public:
    enum class BlendMode {
        Blend,
        AdditiveAlpha,
        Modulate,
        Modulate2x
    };

    struct MeshVertex {
        float worldX = 0.0f;
        float worldY = 0.0f;
        float u = 0.0f;
        float v = 0.0f;
        float zOffset = 0.0f;
        bool absoluteHeight = false;
        float absoluteHeightValue = 0.0f;
        bool clampHeightToRect = false;
        float clampMinTileX = 0.0f;
        float clampMinTileY = 0.0f;
        float clampMaxTileX = 0.0f;
        float clampMaxTileY = 0.0f;
        bool useCornerHeightOverride = false;
        float cornerHeightBL = 0.0f;
        float cornerHeightBR = 0.0f;
        float cornerHeightTL = 0.0f;
        float cornerHeightTR = 0.0f;
        Vec4 tint = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    };

    SplatModel();
    ~SplatModel();

    bool LoadTexture(const std::string& path, sg_image tex);

    void AddSplat(float centerX, float centerY, float scale, float rotation);
    void AddSplatRect(float x0, float y0, float x1, float y1, float zOffset,
                      bool absoluteHeight = false, float absoluteHeightValue = 0.0f,
                      bool clampHeightToRect = false,
                      float clampMinTileX = 0.0f, float clampMinTileY = 0.0f,
                      float clampMaxTileX = 0.0f, float clampMaxTileY = 0.0f,
                      bool useCornerHeightOverride = false,
                      float cornerHeightBL = 0.0f, float cornerHeightBR = 0.0f,
                      float cornerHeightTL = 0.0f, float cornerHeightTR = 0.0f,
                      const Vec4& tint = Vec4(1.0f, 1.0f, 1.0f, 1.0f),
                      const Vec4& uvRect = Vec4(0.0f, 0.0f, 1.0f, 1.0f));
    void AddMesh(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices);
    void RemoveSplat(int index);
    void ClearGeometry();

    void Render(sg_shader uberSplatShader, const Mat4& mvp, float centerOffset[2],
                int mapCols, int mapRows, sg_view heightMapView, sg_view shadowMapView,
                sg_view waterHeightMapView, sg_view fogOfWarMapView, float waterHeightOffset,
                sg_view terrainTextureListView, const sg_view* groundTextureViews,
                int groundTextureCount, sg_sampler terrainTextureListSampler,
                sg_sampler groundTextureSampler);

    bool IsHighPriority() const { return highPriority_; }
    void SetHighPriority(bool v) { highPriority_ = v; }
    void SetUseTerrainColorMask(bool v) { terrainColorMask_ = v; }
    void SetNoDepthTest(bool v);
    void SetBlendMode(BlendMode mode);
    void SetUnshaded(bool v) { unshaded_ = v; }

private:
    struct SplatInstance {
        float x0 = 0.0f;
        float y0 = 0.0f;
        float x1 = 0.0f;
        float y1 = 0.0f;
        float zOffset = 0.0f;
        bool absoluteHeight = false;
        float absoluteHeightValue = 0.0f;
        bool clampHeightToRect = false;
        float clampMinTileX = 0.0f;
        float clampMinTileY = 0.0f;
        float clampMaxTileX = 0.0f;
        float clampMaxTileY = 0.0f;
        bool useCornerHeightOverride = false;
        float cornerHeightBL = 0.0f;
        float cornerHeightBR = 0.0f;
        float cornerHeightTL = 0.0f;
        float cornerHeightTR = 0.0f;
        Vec4 tint = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        Vec4 uvRect = Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    };

    struct BatchGPU {
        sg_buffer vbo = {0};
        sg_buffer ibo = {0};
        int indexCount = 0;
    };

    struct MeshInstance {
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;
    };

    void ClearBatches();
    void RebuildBatches(float centerOffset[2], int mapCols, int mapRows);

    sg_image texture_ = {0};
    sg_view textureView_ = {0};
    sg_sampler sampler_ = {0};
    bool highPriority_ = false;
    bool terrainColorMask_ = false;
    bool noDepthTest_ = false;
    bool unshaded_ = false;
    BlendMode blendMode_ = BlendMode::Blend;
    std::vector<SplatInstance> instances_;
    std::vector<MeshInstance> meshInstances_;
    std::vector<BatchGPU> batches_;
    float lastCenterOffset_[2] = {0.0f, 0.0f};
    int lastMapCols_ = 0;
    int lastMapRows_ = 0;
    bool dirty_ = false;
    sg_pipeline pipeline_ = {0};
};
