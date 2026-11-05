#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include "math.h"
#include <cstdint>
#include <vector>

class TexturedTerrain;
class TileSet;
class MpqManager;
struct W3EMap;
struct W3iInfo;

class Minimap {
public:
    Minimap() = default;
    ~Minimap();

    void Generate(const TexturedTerrain& terrain, const TileSet& tileset,
                  const W3EMap& w3e, const W3iInfo& w3i, const MpqManager* mpq = nullptr);
    void SetScreenRect(float x, float y, float w, float h);
    void Render(int winW, int winH, float camX, float camZ,
                float camYaw, float camPitch, float camDist, float fov, float aspect);
    void ClearMarkers();
    void AddMarker(float wx, float wy, uint32_t rgba, float sizePixels = 8.0f);
    void ClearScriptMarkers();
    void AddScriptMarker(float wx, float wy, uint32_t rgba, float sizePixels = 8.0f);

    // Warsmash MeleeUIMinimap parity: fog overlay + per-unit dots.
    // rgba format for dots matches markers (0xRRGGBBAA).
    struct UnitDot {
        float worldX = 0.0f;
        float worldY = 0.0f;
        uint32_t rgba = 0xFFFFFFFFu;
        float sizePixels = 4.0f;
    };
    void SetUnitDots(const std::vector<UnitDot>& dots);
    void ClearUnitDots();
    // Fog overlay image (RGBA, row 0 = worldMinY to match Generate()).
    // w/h <= 0 clears the overlay.
    void SetFogOverlay(int w, int h, const std::vector<uint8_t>& rgba);
    void ClearFogOverlay();
    void GetWorldBounds(float& minX, float& minY, float& maxX, float& maxY) const;

    bool HitTest(float mouseX, float mouseY, int winW, int winH) const;
    void ScreenToWorld(float mx, float my, int winW, int winH, float* wx, float* wy) const;
    void WorldToScreen(float wx, float wy, int winW, int winH, float* sx, float* sy) const;

    bool IsValid() const { return texture_.id != 0; }

private:
    struct Marker {
        float worldX = 0.0f;
        float worldY = 0.0f;
        uint32_t rgba = 0xFFFFFFFFu;
        float sizePixels = 8.0f;
    };

    void InitShader();

    sg_image texture_ = {0};
    sg_view textureView_ = {0};
    // Fog-of-war overlay (black mask / grey fog over the filled area).
    sg_image fogTexture_ = {0};
    sg_view fogView_ = {0};
    sg_shader shader_ = {0};
    sg_pipeline pipeline_ = {0};
    sg_pipeline linePipeline_ = {0};
    sg_buffer vbo_ = {0}, ibo_ = {0};
    sg_buffer lineVbo_ = {0};
    sg_sampler sampler_ = {0};

    // Screen position (bottom-left, pixels)
    float screenX_ = 0, screenY_ = 0;
    float screenW_ = 0, screenH_ = 0;
    float filledX_ = 0, filledY_ = 0;
    float filledW_ = 0, filledH_ = 0;

    // World bounds
    float worldMinX_ = 0, worldMinY_ = 0;
    float worldMaxX_ = 0, worldMaxY_ = 0;

    float texW_ = 0, texH_ = 0;
    std::vector<Marker> markers_;
    std::vector<Marker> scriptMarkers_;
    std::vector<UnitDot> unitDots_;
};
