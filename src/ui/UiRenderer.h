#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include <cstdint>
#include <vector>

struct UiVertex {
    float x, y, u, v;
    uint32_t color;
};

class UiRenderer {
public:
    UiRenderer();
    ~UiRenderer();

    bool Init(int screenW, int screenH);

    void BeginFrame(int screenW, int screenH);
    void EndFrame();

    void DrawQuad(float x, float y, float w, float h, uint32_t color);
    void DrawTexturedQuad(float x, float y, float w, float h,
                          sg_image tex, float u0=0, float v0=0,
                          float u1=1, float v1=1,
                          uint32_t color=0xFFFFFFFF);
    // Transposed quad: the texture U axis runs along screen Y and V along
    // screen X. Used for retail backdrop B/T edges, whose strip slices are
    // authored vertically but laid horizontally (ref BackdropFrame, where
    // Java rotates the tiles by 270 degrees).
    void DrawTransposedQuad(float x, float y, float w, float h,
                            sg_image tex, float su0=0, float sv0=0,
                            float su1=1, float sv1=1,
                            uint32_t color=0xFFFFFFFF);

    sg_image GetWhiteTexture() const { return whiteTex_; }
    sg_view GetOrCreateView(sg_image img);

private:
    void Flush();

    struct ViewEntry { sg_image img = {}; sg_view view = {}; };
    std::vector<ViewEntry> viewCache_;

    sg_pipeline pipeline_ = {};
    sg_shader shader_ = {};
    sg_buffer vbo_ = {};
    sg_buffer ibo_ = {};
    sg_sampler sampler_ = {};
    sg_image whiteTex_ = {};
    sg_view whiteView_ = {};

    static const int MAX_QUADS = 4096;
    static const int MAX_VERTS = MAX_QUADS * 4;
    static const int MAX_INDICES = MAX_QUADS * 6;

    std::vector<UiVertex> verts_;
    std::vector<uint16_t> indices_;
    int quadCount_ = 0;

    int screenW_ = 1280;
    int screenH_ = 720;
    sg_image currentTex_ = {};
};
