#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include "map/w3e.h"
#include "map/wpm.h"
#include "map/w3r.h"
#include "resource/mpq_storm.h"
#include "math.h"
#include "wave_builder.h"
#include "light_manager.h"
#include "splat_model.h"
#include "render/render_graph.h"
#include "engine/render/standard_render_passes.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class TileSet;
class MdxViewer;

struct CliffInstance {
    MdxViewer* viewer = nullptr;
    float posX = 0, posY = 0, posZ = 0;
    float scale = 1.0f;
    int cliffTexture = 0;
    int variation = 0;
    float yaw = 0.0f;
    int gridX = 0;
    int gridY = 0;
    float layerBase = 0.0f;
    int topCellX = 0;
    int topCellY = 0;
    int bottomCellX = 0;
    int bottomCellY = 0;
    std::string meshPath;
};

struct TerrainBuildStats {
    int groundTexturesLoaded = 0;
    int blightLoaded = 0;
    int groundSrgbTextures = 0;
    int groundLinearTextures = 0;
    int cliffSrgbTextures = 0;
    int cliffLinearTextures = 0;
    int waterSrgbTextures = 0;
    int waterLinearTextures = 0;
    int cliffMeshesBuilt = 0;
    int cliffModelHits = 0;
    int cliffModelMisses = 0;
    int shorelineInstances = 0;
    int uberSplatLoads = 0;
    int uberSplatMisses = 0;
    int shadowLoads = 0;
    int shadowMisses = 0;
    int groundPaletteMappedCount = 0;
    int groundPaletteFallbackCount = 0;
    std::string fallbackMode;
};

class TexturedTerrain {
public:
    struct CachedImage {
        int width = 0;
        int height = 0;
        std::vector<unsigned char> pixels;
    };
    // Store CPU-side tile texture data for combined texture array creation
    std::vector<CachedImage> tilePixelData_;
    sg_image combinedTex_ = {0};
    sg_view combinedTexView_ = {0};

    TexturedTerrain();
    ~TexturedTerrain();
    TexturedTerrain(const TexturedTerrain&) = delete;
    TexturedTerrain& operator=(const TexturedTerrain&) = delete;

    bool Build(const W3EMap& map, TileSet* tileset);
    void SetMpq(MpqManager* mpq) { mpq_ = mpq; }

    void SetFog(const float color[3], float start, float end);
    void SetWaterDeforms(bool enabled) { waterDeforms_ = enabled; }
    bool GetWaterDeforms() const { return waterDeforms_; }
    void SetPathing(const WpmData* wpm) { pathing_ = wpm; }
    void SetRegions(const W3rData* w3r) { regions_ = w3r; }
    void SetShowPathing(bool v) { showPathing_ = v; }
    void SetShowRegions(bool v) { showRegions_ = v; }
    void SetDebugMode(int mode) { debugMode_ = mode; }
    int DebugMode() const { return debugMode_; }
    void SetMapBounds(float minX, float minZ, float maxX, float maxZ);
    void GetMapBounds(float out[4]) const {
        out[0] = mapBounds_[0]; out[1] = mapBounds_[1];
        out[2] = mapBounds_[2]; out[3] = mapBounds_[3];
    }
    // Fog-texture extents: must match the ground-shader v_suv mapping
    // (v_suv = (vPosition + tileIndex) / gridSize over the instance grid).
    // Rasterizing fog over any other bounds (e.g. camera bounds) shifts the
    // visible blobs away from their units.
    void GetFogMapBounds(float out[4]) const {
        const float spanX = columns_ > 1 ? (float)(columns_ - 1) * 128.0f : 0.0f;
        const float spanY = rows_ > 1 ? (float)(rows_ - 1) * 128.0f : 0.0f;
        out[0] = centerOffsetX_; out[1] = centerOffsetY_;
        out[2] = centerOffsetX_ + spanX; out[3] = centerOffsetY_ + spanY;
    }
    void SetW3i(const float cameraBounds[8]);

    void SetFogOfWarData(const unsigned char* data, int width, int height);
    void SetShadowData(const unsigned char* data, int width, int height);
    LightManager& GetLightManager() { return lightManager_; }
    void InitLights();
    void UpdateGroundHeights(int x, int y, int w, int h);
    void UpdateGroundTextures(int x, int y, int w, int h);

    float GetGroundHeight(float worldX, float worldY) const;
    float GetWaterHeight(float worldX, float worldY) const;
    bool InPlayableArea(float worldX, float worldY) const;

    void AddUberSplat(const std::string& path, float x, float y, float scale, bool highPriority);
    void CreateUnitShadow(float x, float y, const std::string& shadowFile);
    void ClearRuntimeSplats();
    void AddRuntimeSplatRect(const std::string& key, const std::string& texturePath,
                             float x0, float y0, float x1, float y1, float zOffset,
                             bool highPriority, bool noDepthTest,
                             bool absoluteHeight = false, float absoluteHeightValue = 0.0f,
                             const Vec4& tint = Vec4(1.0f, 1.0f, 1.0f, 1.0f),
                             const Vec4& uvRect = Vec4(0.0f, 0.0f, 1.0f, 1.0f),
                             SplatModel::BlendMode blendMode = SplatModel::BlendMode::Blend,
                             bool unshaded = false);
    void AddRuntimeSplatMesh(const std::string& key, const std::string& texturePath,
                             const std::vector<SplatModel::MeshVertex>& vertices,
                             const std::vector<uint32_t>& indices,
                             bool highPriority, bool noDepthTest,
                             SplatModel::BlendMode blendMode = SplatModel::BlendMode::Blend,
                             bool unshaded = false);

    void Render(const Mat4& view, const Mat4& proj, float time = 0);

    bool IsValid() const { return groundShader_.id != 0; }
    Vec3 GetCenter() const { return center_; }
    float GetRadius() const { return radius_; }
    const TerrainBuildStats& GetBuildStats() const { return buildStats_; }
    int GetLastVisibleCliffCount() const { return lastVisibleCliffs_; }
    int GetLastCulledCliffCount() const { return lastCulledCliffs_; }
    const render::TerrainChunkGrid& GetTerrainChunkGrid() const { return terrainChunkGrid_; }
    const render::TerrainCullStats& GetLastChunkCullStats() const { return lastChunkCullStats_; }
    engine::render::TerrainOverlayState& GetOverlayState() { return overlayState_; }
    const engine::render::TerrainOverlayState& GetOverlayState() const { return overlayState_; }

private:
    void Release();
    void BuildGroundGPU(const W3EMap& map);
    void BuildCliffsGPU(const W3EMap& map);
    void BuildWaterGPU(const W3EMap& map);
    void RebuildCornerData(const W3EMap& map);
    MdxViewer* GetOrLoadCliffModel(const std::string& path);
    void BuildCliffsFromMdx(const W3EMap& map);
    void BuildCliffEdgeSplats();
    void BuildPathingOverlay(const W3EMap& map);
    void BuildRegionOverlay(const W3EMap& map);

    void getTextureVariations(const W3EMap& map, int x, int y, short* out, int offset);
    int GetVariation(int groundTexture, int variation);
    bool isCornerRampEntrance(const W3EMap& map, int x, int y) const;
    int RealTileTexture(const W3EMap& map, int x, int y) const;
    void AddUberSplatRect(const std::string& path, float x0, float y0, float x1, float y1,
                          float zOffset, bool highPriority,
                          bool absoluteHeight = false, float absoluteHeightValue = 0.0f,
                          bool clampHeightToRect = false,
                          float clampMinTileX = 0.0f, float clampMinTileY = 0.0f,
                          float clampMaxTileX = 0.0f, float clampMaxTileY = 0.0f,
                          bool useCornerHeightOverride = false,
                          float cornerHeightBL = 0.0f, float cornerHeightBR = 0.0f,
                          float cornerHeightTL = 0.0f, float cornerHeightTR = 0.0f);
    void AddUberSplatMesh(const std::string& path, const std::vector<SplatModel::MeshVertex>& vertices,
                          const std::vector<uint32_t>& indices, bool highPriority);
    SplatModel* GetOrCreateUberSplatModel(const std::string& path, bool highPriority, bool terrainColorMask);
    SplatModel* GetOrCreateUberSplatModelForKey(const std::string& cacheKey, const std::string& texturePath,
                                                bool highPriority, bool terrainColorMask);
    std::string ResolveUberSplatPath(const std::string& path);

    struct CornerExtra {
        float rampAdjust = 0.0f;
        bool romp = false;
        bool cliff = false;
        int cliffTexture = 0;
        float depth = 0;
        bool hideCliff = false;
    };

    // Data textures (GPU)
    sg_image groundTextureData_ = {0};
    sg_image groundHeight_ = {0};
    sg_image groundCornerHeight_ = {0};
    sg_image groundCornerHeightLinear_ = {0};
    sg_image cliffTextureArray_ = {0};
    sg_image waterHeight_ = {0};
    sg_image waterExists_ = {0};
    sg_image waterTextureArray_ = {0};
    sg_image shadowMap_ = {0};
    sg_image fogOfWarMap_ = {0};
    sg_sampler shadowSampler_ = {0};
    sg_sampler fogOfWarSampler_ = {0};
    sg_sampler linearSampler_ = {0};
    sg_sampler nearestSampler_ = {0};
    sg_sampler nonFilterSampler_ = {0};
    sg_sampler repeatSampler_ = {0};
    sg_sampler baseLevelSampler_ = {0};
    sg_view heightTexView_ = {0};
    sg_view cornerHeightView_ = {0};
    sg_view texDataView_ = {0};
    sg_view shadowView_ = {0};
    sg_view fogView_ = {0};
    sg_view lightView_ = {0};
    sg_view waterHeightView_ = {0};
    sg_view waterExistsView_ = {0};
    sg_view waterTexArrayView_ = {0};
    sg_view tileTexViews_[17] = {}; // cached views for texture arrays

    float* groundHeights_ = nullptr;
    float* groundCornerHeights_ = nullptr;
    short* groundTextureList_ = nullptr;
    float* waterHeights_ = nullptr;
    unsigned char* waterExistsData_ = nullptr;
    unsigned char* shadowData_ = nullptr;
    unsigned char* fogOfWarData_ = nullptr;
    int dataWidth_ = 0, dataHeight_ = 0;

    sg_shader groundShader_ = {0};
    sg_shader cliffShader_ = {0};
    sg_shader waterShader_ = {0};
    sg_shader uberSplatShader_ = {0};
    sg_pipeline groundPipeline_ = {0};
    sg_pipeline cliffPipeline_ = {0};
    sg_pipeline waterPipeline_ = {0};
    sg_pipeline uberSplatPipeline_ = {0};

    struct CliffMeshGPU {
        sg_buffer vbo = {0}, ebo = {0};
        sg_view textureView = {0};
        int indexCount = 0;
        float minLocalZ = 0.0f;
        float maxLocalZ = 0.0f;
    };
    struct CliffInstanceGPU {
        int gridX, gridY;
        float layerBase;
        int textureLayer;
        int topCellX, topCellY;
        int bottomCellX, bottomCellY;
        int meshIndex;
    };
    std::vector<CliffMeshGPU> cliffMeshes_;
    std::vector<CliffInstanceGPU> cliffInstanceData_;

    std::unordered_map<std::string, MdxViewer*> cliffViewerCache_;
    std::vector<CliffInstance> cliffInstances_;
    std::unordered_map<std::string, SplatModel*> uberSplatModels_;
    std::unordered_map<std::string, std::string> uberSplatPathCache_;
    std::unordered_set<std::string> runtimeUberSplatKeys_;
    std::vector<SplatModel*> uberSplatModelsList_;
    std::unordered_map<std::string, CachedImage> shadowTextureCache_;
    WaveBuilder waveBuilder_;
    LightManager lightManager_;
    TerrainBuildStats buildStats_;

    std::vector<CornerExtra> cornerExtra_;
    Vec3 center_ = {0, 0, 0};
    float radius_ = 0;
    TileSet* tileset_ = nullptr;
    MpqManager* mpq_ = nullptr;

    float centerOffsetX_ = 0, centerOffsetY_ = 0;
    int columns_ = 0, rows_ = 0;
    int mapTileW_ = 0, mapTileH_ = 0;
    float mapOffsetX_ = 0, mapOffsetY_ = 0;
    char tilesetChar_ = 0;

    bool fogEnabled_ = false;
    float fogColor_[3] = {0.7f, 0.7f, 0.7f};
    float fogStart_ = 8000, fogEnd_ = 20000;
    bool waterDeforms_ = true;

    float mapBounds_[4] = {};
    float shaderMapBounds_[4] = {};

    const WpmData* pathing_ = nullptr;
    const W3rData* regions_ = nullptr;
    bool showPathing_ = false, showRegions_ = false;
    int debugMode_ = 0;
    float cameraBounds_[8] = {};

    float waterIndex_ = 0;
    float waterHeightOffset_ = 0;
    float lastRenderTime_ = -1.0f;

    // Overlay VAOs
    sg_buffer pathingVB_ = {0}, pathingIB_ = {0};
    int pathingIndexCount_ = 0;
    sg_buffer regionVB_ = {0}, regionIB_ = {0};
    int regionIndexCount_ = 0;

    // CPU-generated terrain mesh (fallback when no textures)
    sg_buffer groundVB_ = {0};
    sg_buffer groundIB_ = {0};
    int groundIndexCount_ = 0;
    int lastVisibleCliffs_ = 0;
    int lastCulledCliffs_ = 0;
    render::TerrainChunkGrid terrainChunkGrid_;
    render::TerrainCullStats lastChunkCullStats_{};
    std::vector<int> visibleTerrainChunks_;
    engine::render::TerrainOverlayState overlayState_;
    int nextShadowFootprintId_ = 1;
};
