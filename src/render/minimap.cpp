#include "minimap.h"
#include "renderer.h"
#include "textured_terrain.h"
#include "math.h"
#include "asset/TileSet.h"
#include "asset/BLP.h"
#include "asset/MipChain.h"
#include "asset/stb_image.h"
#include "map/w3e.h"
#include "map/w3i.h"
#include "resource/mpq_storm.h"
#include "../third_party/sokol/sokol_gfx.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace {
static uint32_t HashColor(int index) {
    uint32_t x = (uint32_t)(index + 1) * 1103515245u + 12345u;
    uint8_t r = (uint8_t)(70 + (x & 0x7F));
    uint8_t g = (uint8_t)(80 + ((x >> 8) & 0x7F));
    uint8_t b = (uint8_t)(60 + ((x >> 16) & 0x7F));
    return (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

static bool DecodeImageFile(const std::vector<uint8_t>& fileData, int& outW, int& outH, std::vector<uint8_t>& outPixels) {
    if (fileData.size() >= 4 &&
        (std::memcmp(fileData.data(), "BLP1", 4) == 0 || std::memcmp(fileData.data(), "BLP2", 4) == 0)) {
        BLPImage img;
        if (!DecodeBLP(fileData.data(), fileData.size(), img)) {
            return false;
        }
        outW = img.width;
        outH = img.height;
        outPixels = img.pixels;
        return !outPixels.empty();
    }

    int w = 0, h = 0, ch = 0;
    stbi_uc* pixels = stbi_load_from_memory(fileData.data(), (int)fileData.size(), &w, &h, &ch, STBI_rgb_alpha);
    if (!pixels) {
        return false;
    }
    outW = w;
    outH = h;
    outPixels.assign(pixels, pixels + (w * h * 4));
    stbi_image_free(pixels);
    return true;
}

static bool UploadTexture(sg_image& texture, sg_view& textureView, int width, int height,
                          const std::vector<uint8_t>& pixels) {
    if (width <= 0 || height <= 0 || pixels.size() < (size_t)width * height * 4) return false;
    if (texture.id) {
        sg_destroy_image(texture);
        texture = {0};
    }
    if (textureView.id) {
        sg_destroy_view(textureView);
        textureView = {0};
    }

    sg_image_desc imgd = {};
    imgd.type = SG_IMAGETYPE_2D;
    imgd.width = width;
    imgd.height = height;
    imgd.pixel_format = SG_PIXELFORMAT_RGBA8;
    imgd.num_mipmaps = 1;
    imgd.data.mip_levels[0].ptr = pixels.data();
    imgd.data.mip_levels[0].size = (size_t)width * height * 4;
    texture = sg_make_image(&imgd);
    if (sg_query_image_state(texture) != SG_RESOURCESTATE_VALID) {
        texture = {0};
        return false;
    }

    sg_view_desc vd = {};
    vd.texture.image = texture;
    textureView = sg_make_view(&vd);
    return textureView.id != 0;
}
}

Minimap::~Minimap() {
    if (!Renderer::IsActive()) return;
    if (texture_.id) sg_destroy_image(texture_);
    if (textureView_.id) sg_destroy_view(textureView_);
    if (fogTexture_.id) sg_destroy_image(fogTexture_);
    if (fogView_.id) sg_destroy_view(fogView_);
    if (shader_.id) sg_destroy_shader(shader_);
    if (pipeline_.id) sg_destroy_pipeline(pipeline_);
    if (linePipeline_.id) sg_destroy_pipeline(linePipeline_);
    if (vbo_.id) sg_destroy_buffer(vbo_);
    if (lineVbo_.id) sg_destroy_buffer(lineVbo_);
    if (ibo_.id) sg_destroy_buffer(ibo_);
    if (sampler_.id) sg_destroy_sampler(sampler_);
}

void Minimap::InitShader() {
    if (shader_.id) return;
    static bool attempted = false;
    if (attempted) return;
    attempted = true;

    const char* vs = R"(
        #version 410 core
        in vec2 aPos;
        in vec2 aUV;
        out vec2 vUV;
        uniform vec2 uScreenSize;
        uniform vec2 uOffset;
        uniform vec2 uScale;
        void main() {
            vec2 pos = (aPos * uScale + uOffset) / uScreenSize * 2.0 - 1.0;
            pos.y = -pos.y;
            gl_Position = vec4(pos, 0.0, 1.0);
            vUV = aUV;
        }
    )";
    const char* fs = R"(
        #version 410 core
        in vec2 vUV;
        out vec4 FragColor;
        uniform sampler2D uTex;
        uniform vec4 uColor;
        uniform float uUseTexture;
        void main() {
            FragColor = (uUseTexture > 0.5) ? texture(uTex, vUV) * uColor : uColor;
        }
    )";

    sg_shader_desc sd = {};
    sd.vertex_func.source = vs;
    sd.fragment_func.source = fs;
    sd.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
    sd.uniform_blocks[0].size = 24;
    sd.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_NATIVE;
    sd.uniform_blocks[0].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT2;
    sd.uniform_blocks[0].glsl_uniforms[0].array_count = 1;
    sd.uniform_blocks[0].glsl_uniforms[0].glsl_name = "uScreenSize";
    sd.uniform_blocks[0].glsl_uniforms[1].type = SG_UNIFORMTYPE_FLOAT2;
    sd.uniform_blocks[0].glsl_uniforms[1].array_count = 1;
    sd.uniform_blocks[0].glsl_uniforms[1].glsl_name = "uOffset";
    sd.uniform_blocks[0].glsl_uniforms[2].type = SG_UNIFORMTYPE_FLOAT2;
    sd.uniform_blocks[0].glsl_uniforms[2].array_count = 1;
    sd.uniform_blocks[0].glsl_uniforms[2].glsl_name = "uScale";
    sd.views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT;
    sd.views[0].texture.image_type = SG_IMAGETYPE_2D;
    sd.views[0].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
    sd.uniform_blocks[1].stage = SG_SHADERSTAGE_FRAGMENT;
    sd.uniform_blocks[1].size = 20;
    sd.uniform_blocks[1].layout = SG_UNIFORMLAYOUT_NATIVE;
    sd.uniform_blocks[1].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
    sd.uniform_blocks[1].glsl_uniforms[0].array_count = 1;
    sd.uniform_blocks[1].glsl_uniforms[0].glsl_name = "uColor";
    sd.uniform_blocks[1].glsl_uniforms[1].type = SG_UNIFORMTYPE_FLOAT;
    sd.uniform_blocks[1].glsl_uniforms[1].array_count = 1;
    sd.uniform_blocks[1].glsl_uniforms[1].glsl_name = "uUseTexture";
    sd.samplers[0].stage = SG_SHADERSTAGE_FRAGMENT;
    sd.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;
    sd.texture_sampler_pairs[0].stage = SG_SHADERSTAGE_FRAGMENT;
    sd.texture_sampler_pairs[0].glsl_name = "uTex";
    sd.texture_sampler_pairs[0].view_slot = 0;
    sd.texture_sampler_pairs[0].sampler_slot = 0;
    shader_ = sg_make_shader(&sd);
    if (sg_query_shader_state(shader_) != SG_RESOURCESTATE_VALID) {
        std::cerr << "Minimap: shader creation failed" << std::endl;
        shader_ = {0};
        return;
    }

    sg_pipeline_desc pd = {};
    pd.shader = shader_;
    pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pd.index_type = SG_INDEXTYPE_UINT32;
    pd.depth.compare = SG_COMPAREFUNC_ALWAYS;
    pd.depth.write_enabled = false;
    pd.cull_mode = SG_CULLMODE_NONE;
    pd.colors[0].blend.enabled = true;
    pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pd.layout.buffers[0].stride = 16;
    pd.layout.attrs[0] = {0, 0, SG_VERTEXFORMAT_FLOAT2};
    pd.layout.attrs[1] = {0, 8, SG_VERTEXFORMAT_FLOAT2};
    pipeline_ = sg_make_pipeline(&pd);
    if (sg_query_pipeline_state(pipeline_) != SG_RESOURCESTATE_VALID) {
        std::cerr << "Minimap: pipeline creation failed" << std::endl;
        pipeline_ = {0};
        return;
    }

    sg_sampler_desc smp = {};
    smp.min_filter = SG_FILTER_LINEAR;
    smp.mag_filter = SG_FILTER_LINEAR;
    smp.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smp.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    sampler_ = sg_make_sampler(&smp);

    // Quad VBO: position (screen space) + UV
    float quadData[] = {
        0,0, 0,0,   // bottom-left
        1,0, 1,0,   // bottom-right
        0,1, 0,1,   // top-left
        1,1, 1,1,   // top-right
    };
    sg_buffer_desc vbd = {};
    vbd.usage.vertex_buffer = true;
    vbd.data.ptr = quadData;
    vbd.data.size = sizeof(quadData);
    vbo_ = sg_make_buffer(&vbd);

    unsigned idxData[] = {0,1,2, 1,3,2};
    sg_buffer_desc ibd = {};
    ibd.usage.index_buffer = true;
    ibd.data.ptr = idxData;
    ibd.data.size = sizeof(idxData);
    ibo_ = sg_make_buffer(&ibd);

    // Line pipeline and VBO for frustum trapezoid
    sg_pipeline_desc lpd = pd;
    lpd.primitive_type = SG_PRIMITIVETYPE_LINES;
    lpd.index_type = SG_INDEXTYPE_NONE;
    linePipeline_ = sg_make_pipeline(&lpd);

    sg_buffer_desc lbd = {};
    lbd.usage.vertex_buffer = true;
    lbd.usage.stream_update = true;
    lbd.size = 8 * 4 * sizeof(float);
    lineVbo_ = sg_make_buffer(&lbd);
}

void Minimap::Generate(const TexturedTerrain& terrain, const TileSet& tileset,
                       const W3EMap& w3e, const W3iInfo& w3i, const MpqManager* mpq) {
    // Use actual W3E terrain extents as minimap bounds; W3I camera bounds are for camera limits,
    // and using them here can collapse sampling to the wrong area.
    worldMinX_ = w3e.offsetX;
    worldMinY_ = w3e.offsetY;
    worldMaxX_ = w3e.offsetX + std::max(1, w3e.tileWidth - 1) * 128.0f;
    worldMaxY_ = w3e.offsetY + std::max(1, w3e.tileHeight - 1) * 128.0f;
    if (!(worldMaxX_ > worldMinX_) || !(worldMaxY_ > worldMinY_)) {
        worldMinX_ = w3i.cameraBounds[0] * 128.0f;
        worldMinY_ = w3i.cameraBounds[2] * 128.0f;
        worldMaxX_ = w3i.cameraBounds[1] * 128.0f;
        worldMaxY_ = w3i.cameraBounds[3] * 128.0f;
    }

    if (mpq) {
        std::vector<uint8_t> fileData;
        int imageW = 0, imageH = 0;
        std::vector<uint8_t> imagePixels;
        if ((mpq->ReadFile("war3mapMap.tga", fileData) || mpq->ReadFile("war3mapMap.blp", fileData))
            && DecodeImageFile(fileData, imageW, imageH, imagePixels)) {
            for (size_t i = 3; i < imagePixels.size(); i += 4) {
                imagePixels[i] = 255;
            }
            if (UploadTexture(texture_, textureView_, imageW, imageH, imagePixels)) {
                texW_ = (float)imageW;
                texH_ = (float)imageH;
                if (screenW_ > 0.0f && screenH_ > 0.0f) {
                    SetScreenRect(screenX_, screenY_, screenW_, screenH_);
                }
                return;
            }
        }
    }

    int texW = std::max(64, std::min(512, (int)((worldMaxX_ - worldMinX_) / 128.0f * 2)));
    int texH = std::max(64, std::min(512, (int)((worldMaxY_ - worldMinY_) / 128.0f * 2)));
    texW_ = (float)texW; texH_ = (float)texH;

    std::vector<uint8_t> pixels(texW * texH * 4, 0);
    int texCount = tileset.TextureCount();
    std::vector<uint32_t> tileColors(texCount, 0);
    for (int i = 0; i < texCount; i++) {
        const auto& gt = tileset.GetTile(i);
        if (!gt.firstLayerData.empty() && gt.tileSize > 0) {
            const uint8_t* src = gt.firstLayerData.data();
            const int tileSz = gt.tileSize;
            const int sampleStep = std::max(1, tileSz / 8);
            uint64_t sumR = 0, sumG = 0, sumB = 0, samples = 0;
            for (int y = sampleStep / 2; y < tileSz; y += sampleStep) {
                for (int x = sampleStep / 2; x < tileSz; x += sampleStep) {
                    const int si = (y * tileSz + x) * 4;
                    sumR += src[si + 0];
                    sumG += src[si + 1];
                    sumB += src[si + 2];
                    ++samples;
                }
            }
            uint32_t hashed = HashColor(i);
            if (samples > 0) {
                const uint8_t ar = (uint8_t)(sumR / samples);
                const uint8_t ag = (uint8_t)(sumG / samples);
                const uint8_t ab = (uint8_t)(sumB / samples);
                const uint8_t hr = (uint8_t)((hashed >> 16) & 0xFF);
                const uint8_t hg = (uint8_t)((hashed >> 8) & 0xFF);
                const uint8_t hb = (uint8_t)(hashed & 0xFF);
                tileColors[i] =
                    (uint32_t((ar * 3 + hr * 2) / 5) << 16) |
                    (uint32_t((ag * 3 + hg * 2) / 5) << 8) |
                    uint32_t((ab * 3 + hb * 2) / 5);
            }
        }
        if (tileColors[i] == 0) tileColors[i] = HashColor(i);
    }

    int cols = w3e.tileWidth, rows = w3e.tileHeight;
    float invStepX = texW / (worldMaxX_ - worldMinX_);
    float invStepY = texH / (worldMaxY_ - worldMinY_);
    float minH = 1e30f, maxH = -1e30f;
    for (int i = 0; i < cols * rows; ++i) {
        const float h = w3e.tiles[i].FinalGroundHeight();
        minH = std::min(minH, h);
        maxH = std::max(maxH, h);
    }
    const float heightRange = std::max(1.0f, maxH - minH);

    for (int ty = 0; ty < texH; ty++) {
        for (int tx = 0; tx < texW; tx++) {
            float wx = worldMinX_ + (tx + 0.5f) / invStepX;
            float wy = worldMinY_ + (ty + 0.5f) / invStepY;

            // Find nearest corner
            int cx = std::clamp((int)((wx - w3e.offsetX) / 128.0f), 0, cols - 2);
            int cy = std::clamp((int)((wy - w3e.offsetY) / 128.0f), 0, rows - 2);
            int idx = cy * cols + cx;
            int texIdx = w3e.tiles[idx].GroundTexture();
            bool isWater = w3e.tiles[idx].IsWater();
            float h = w3e.tiles[idx].FinalGroundHeight();

            int pi = (ty * texW + tx) * 4;
            if (isWater) {
                pixels[pi + 0] = 54;
                pixels[pi + 1] = 95;
                pixels[pi + 2] = 168;
                pixels[pi + 3] = 220;
            } else if (texIdx >= 0 && texIdx < texCount) {
                const uint32_t color = tileColors[texIdx];
                pixels[pi + 0] = (uint8_t)((color >> 16) & 0xFF);
                pixels[pi + 1] = (uint8_t)((color >> 8) & 0xFF);
                pixels[pi + 2] = (uint8_t)(color & 0xFF);
                pixels[pi + 3] = 255;
            } else {
                pixels[pi + 0] = 96;
                pixels[pi + 1] = 86;
                pixels[pi + 2] = 72;
                pixels[pi + 3] = 255;
            }

            const float t = std::clamp((h - minH) / heightRange, 0.0f, 1.0f);
            const float shade = 0.70f + t * 0.45f;
            pixels[pi + 0] = (uint8_t)std::clamp((int)(pixels[pi + 0] * shade), 0, 255);
            pixels[pi + 1] = (uint8_t)std::clamp((int)(pixels[pi + 1] * shade), 0, 255);
            pixels[pi + 2] = (uint8_t)std::clamp((int)(pixels[pi + 2] * shade), 0, 255);
        }
    }

    UploadTexture(texture_, textureView_, texW, texH, pixels);
    if (screenW_ > 0.0f && screenH_ > 0.0f) {
        SetScreenRect(screenX_, screenY_, screenW_, screenH_);
    }
}

void Minimap::SetScreenRect(float x, float y, float w, float h) {
    screenX_ = x;
    screenY_ = y;
    screenW_ = std::max(0.0f, w);
    screenH_ = std::max(0.0f, h);
    const float worldWidth = std::max(1.0f, worldMaxX_ - worldMinX_);
    const float worldHeight = std::max(1.0f, worldMaxY_ - worldMinY_);
    const float worldSize = std::max(worldWidth, worldHeight);
    filledW_ = screenW_ * (worldWidth / worldSize);
    filledH_ = screenH_ * (worldHeight / worldSize);
    filledX_ = screenX_ + (screenW_ - filledW_) * 0.5f;
    filledY_ = screenY_ + (screenH_ - filledH_) * 0.5f;
}

void Minimap::ClearMarkers() {
    markers_.clear();
}

void Minimap::AddMarker(float wx, float wy, uint32_t rgba, float sizePixels) {
    markers_.push_back({wx, wy, rgba, std::max(2.0f, sizePixels)});
}

void Minimap::ClearScriptMarkers() { scriptMarkers_.clear(); }

void Minimap::AddScriptMarker(float wx, float wy, uint32_t rgba, float sizePixels) {
    scriptMarkers_.push_back({wx, wy, rgba, std::max(2.0f, sizePixels)});
}

void Minimap::SetUnitDots(const std::vector<UnitDot>& dots) {
    unitDots_.clear();
    unitDots_.reserve(dots.size());
    for (const auto& d : dots) {
        UnitDot dot = d;
        dot.sizePixels = std::max(2.0f, d.sizePixels);
        unitDots_.push_back(dot);
    }
}

void Minimap::ClearUnitDots() { unitDots_.clear(); }

void Minimap::SetFogOverlay(int w, int h, const std::vector<uint8_t>& rgba) {
    if (w <= 0 || h <= 0 || rgba.size() < (size_t)w * h * 4) {
        ClearFogOverlay();
        return;
    }
    UploadTexture(fogTexture_, fogView_, w, h, rgba);
}

void Minimap::ClearFogOverlay() {
    if (!Renderer::IsActive()) {
        fogTexture_ = {0};
        fogView_ = {0};
        return;
    }
    if (fogTexture_.id) sg_destroy_image(fogTexture_);
    fogTexture_ = {0};
    if (fogView_.id) sg_destroy_view(fogView_);
    fogView_ = {0};
}

void Minimap::GetWorldBounds(float& minX, float& minY, float& maxX, float& maxY) const {
    minX = worldMinX_;
    minY = worldMinY_;
    maxX = worldMaxX_;
    maxY = worldMaxY_;
}

void Minimap::Render(int winW, int winH, float camX, float camZ,
                     float camYaw, float camPitch, float camDist, float fov, float aspect) {
    if (!texture_.id || !textureView_.id) return;
    if (!pipeline_.id || !shader_.id || !vbo_.id || !ibo_.id || !sampler_.id) {
        InitShader();
    }
    if (!pipeline_.id || !shader_.id || !vbo_.id || !ibo_.id || !sampler_.id) return;
    if (screenW_ <= 0.0f || screenH_ <= 0.0f) return;

    (void)camYaw;
    (void)camPitch;
    (void)camDist;
    (void)fov;
    (void)aspect;

    struct VSUniforms {
        float screenSize[2];
        float offset[2];
        float scale[2];
    };
    VSUniforms vsUniforms = {};
    vsUniforms.screenSize[0] = (float)winW;
    vsUniforms.screenSize[1] = (float)winH;
    vsUniforms.offset[0] = (filledW_ > 0.0f) ? filledX_ : screenX_;
    vsUniforms.offset[1] = (filledH_ > 0.0f) ? filledY_ : screenY_;
    vsUniforms.scale[0] = (filledW_ > 0.0f) ? filledW_ : screenW_;
    vsUniforms.scale[1] = (filledH_ > 0.0f) ? filledH_ : screenH_;

    float fsUniforms[5] = {
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f
    };

    sg_apply_pipeline(pipeline_);
    sg_bindings bind = {};
    bind.vertex_buffers[0] = vbo_;
    bind.index_buffer = ibo_;
    bind.views[0] = textureView_;
    bind.samplers[0] = sampler_;
    if (bind.views[0].id && ((bind.views[0].id & 0xFFFFu) >= 4096u)) {
        std::fprintf(stderr, "Minimap suspicious view: 0x%08x\n", bind.views[0].id);
        return;
    }
    sg_apply_bindings(&bind);
    sg_apply_uniforms(0, SG_RANGE(vsUniforms));
    sg_apply_uniforms(1, SG_RANGE(fsUniforms));
    sg_draw(0, 6, 1);

    // Fog overlay over the filled (world-mapped) area, matching
    // MeleeUIMinimap: MASKED = opaque black, FOGGED = 50% black.
    if (fogTexture_.id && fogView_.id && filledW_ > 0.0f && filledH_ > 0.0f) {
        sg_bindings fogBind = {};
        fogBind.vertex_buffers[0] = vbo_;
        fogBind.index_buffer = ibo_;
        fogBind.views[0] = fogView_;
        fogBind.samplers[0] = sampler_;
        sg_apply_bindings(&fogBind);
        VSUniforms fogVs = vsUniforms;
        fogVs.offset[0] = filledX_;
        fogVs.offset[1] = filledY_;
        fogVs.scale[0] = filledW_;
        fogVs.scale[1] = filledH_;
        sg_apply_uniforms(0, SG_RANGE(fogVs));
        sg_apply_uniforms(1, SG_RANGE(fsUniforms));
        sg_draw(0, 6, 1);
    }

    auto drawMarker = [&](float screenX, float screenY, float sizePixels, uint32_t rgba) {
        if (screenX < filledX_ - sizePixels || screenX > filledX_ + filledW_ + sizePixels ||
            screenY < filledY_ - sizePixels || screenY > filledY_ + filledH_ + sizePixels) {
            return;
        }
        VSUniforms markerVs = vsUniforms;
        markerVs.offset[0] = screenX - sizePixels * 0.5f;
        markerVs.offset[1] = screenY - sizePixels * 0.5f;
        markerVs.scale[0] = sizePixels;
        markerVs.scale[1] = sizePixels;

        float markerFs[5] = {
            float((rgba >> 24) & 0xFF) / 255.0f,
            float((rgba >> 16) & 0xFF) / 255.0f,
            float((rgba >> 8) & 0xFF) / 255.0f,
            float(rgba & 0xFF) / 255.0f,
            0.0f
        };

        sg_apply_uniforms(0, SG_RANGE(markerVs));
        sg_apply_uniforms(1, SG_RANGE(markerFs));
        sg_draw(0, 6, 1);
    };

    // Unit dots under the camera/ping markers (warsmash draws units
    // right after fog, pings on top).
    for (const auto& dot : unitDots_) {
        float sx = 0.0f;
        float sy = 0.0f;
        WorldToScreen(dot.worldX, dot.worldY, winW, winH, &sx, &sy);
        drawMarker(sx, sy, dot.sizePixels, dot.rgba);
    }

    float camSX = 0.0f;
    float camSY = 0.0f;
    WorldToScreen(camX, camZ, winW, winH, &camSX, &camSY);

    // Ground-projected camera view frustum trapezoid (matching Warcraft 3 retail minimap)
    if (camDist > 10.0f && fov > 1.0f) {
        float cx = camDist * std::cos(camPitch) * std::sin(camYaw);
        float cy = camDist * std::sin(camPitch);
        float cz = camDist * std::cos(camPitch) * std::cos(camYaw);
        Vec3 eye = {camX + cx, std::max(50.0f, cy), camZ + cz};
        Vec3 target = {camX, 0.0f, camZ};
        Vec3 fwd = Normalize(target - eye);
        Vec3 right = Normalize(Cross(fwd, Vec3{0, 1, 0}));
        Vec3 up = Cross(right, fwd);

        float fovRad = fov * 3.14159265f / 180.0f;
        float tanHalfY = std::tan(fovRad * 0.5f);
        float tanHalfX = tanHalfY * (aspect > 0.0f ? aspect : 1.333f);

        Vec3 rays[4] = {
            Normalize(fwd + up * tanHalfY - right * tanHalfX), // 0: Far-Left
            Normalize(fwd + up * tanHalfY + right * tanHalfX), // 1: Far-Right
            Normalize(fwd - up * tanHalfY + right * tanHalfX), // 2: Near-Right
            Normalize(fwd - up * tanHalfY - right * tanHalfX)  // 3: Near-Left
        };

        float cornersSX[4] = {};
        float cornersSY[4] = {};
        const float maxGroundDist = camDist * 2.5f;

        for (int i = 0; i < 4; ++i) {
            float t = maxGroundDist;
            if (rays[i].y < -0.01f) {
                t = std::min(maxGroundDist, -eye.y / rays[i].y);
            }
            float gx = eye.x + rays[i].x * t;
            float gz = eye.z + rays[i].z * t;
            WorldToScreen(gx, gz, winW, winH, &cornersSX[i], &cornersSY[i]);
            cornersSX[i] = std::clamp(cornersSX[i], filledX_, filledX_ + filledW_);
            cornersSY[i] = std::clamp(cornersSY[i], filledY_, filledY_ + filledH_);
        }

        // Single draw call for camera view frustum trapezoid (4 line segments = 8 vertices)
        if (lineVbo_.id && linePipeline_.id) {
            float lineVerts[32] = {
                cornersSX[0], cornersSY[0], 0.0f, 0.0f,
                cornersSX[1], cornersSY[1], 0.0f, 0.0f,
                cornersSX[1], cornersSY[1], 0.0f, 0.0f,
                cornersSX[2], cornersSY[2], 0.0f, 0.0f,
                cornersSX[2], cornersSY[2], 0.0f, 0.0f,
                cornersSX[3], cornersSY[3], 0.0f, 0.0f,
                cornersSX[3], cornersSY[3], 0.0f, 0.0f,
                cornersSX[0], cornersSY[0], 0.0f, 0.0f,
            };
            sg_update_buffer(lineVbo_, sg_range{lineVerts, sizeof(lineVerts)});

            sg_apply_pipeline(linePipeline_);
            sg_bindings lbind = {};
            lbind.vertex_buffers[0] = lineVbo_;
            lbind.views[0] = textureView_;
            lbind.samplers[0] = sampler_;
            sg_apply_bindings(&lbind);

            VSUniforms lvs = vsUniforms;
            lvs.offset[0] = 0.0f;
            lvs.offset[1] = 0.0f;
            lvs.scale[0] = 1.0f;
            lvs.scale[1] = 1.0f;
            sg_apply_uniforms(0, SG_RANGE(lvs));

            float lfs[5] = {1.0f, 1.0f, 1.0f, 1.0f, 0.0f};
            sg_apply_uniforms(1, SG_RANGE(lfs));
            sg_draw(0, 8, 1);

            // Re-bind quad pipeline for marker drawing
            sg_apply_pipeline(pipeline_);
            sg_apply_bindings(&bind);
        }
    }

    drawMarker(camSX, camSY, 4.0f, 0xFFFFFFFFu);

    for (const auto& marker : markers_) {
        float sx = 0.0f;
        float sy = 0.0f;
        WorldToScreen(marker.worldX, marker.worldY, winW, winH, &sx, &sy);
        drawMarker(sx, sy, marker.sizePixels, marker.rgba);
    }
    for (const auto& marker : scriptMarkers_) {
        float sx = 0.0f;
        float sy = 0.0f;
        WorldToScreen(marker.worldX, marker.worldY, winW, winH, &sx, &sy);
        drawMarker(sx, sy, marker.sizePixels, marker.rgba);
    }
}

bool Minimap::HitTest(float mouseX, float mouseY, int winW, int winH) const {
    return mouseX >= filledX_ && mouseX <= filledX_ + filledW_ &&
           mouseY >= filledY_ && mouseY <= filledY_ + filledH_;
}

void Minimap::ScreenToWorld(float mx, float my, int winW, int winH, float* wx, float* wy) const {
    float u = (mx - filledX_) / std::max(1.0f, filledW_);
    float v = (my - filledY_) / std::max(1.0f, filledH_);
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    *wx = worldMinX_ + u * (worldMaxX_ - worldMinX_);
    *wy = worldMinY_ + v * (worldMaxY_ - worldMinY_);
}

void Minimap::WorldToScreen(float wx, float wy, int winW, int winH, float* sx, float* sy) const {
    float u = (wx - worldMinX_) / (worldMaxX_ - worldMinX_);
    float v = (wy - worldMinY_) / (worldMaxY_ - worldMinY_);
    *sx = filledX_ + u * filledW_;
    *sy = filledY_ + v * filledH_;
}
