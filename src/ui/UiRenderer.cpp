#include "UiRenderer.h"
#include "render/rhi_device.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

UiRenderer::UiRenderer() {
    verts_.resize(MAX_VERTS);
    indices_.resize(MAX_INDICES);
}

UiRenderer::~UiRenderer() {
    if (!Renderer::IsActive()) return;
    if (pipeline_.id) sg_destroy_pipeline(pipeline_);
    if (shader_.id) sg_destroy_shader(shader_);
    if (vbo_.id) sg_destroy_buffer(vbo_);
    if (ibo_.id) sg_destroy_buffer(ibo_);
    if (sampler_.id) sg_destroy_sampler(sampler_);
    if (whiteTex_.id) sg_destroy_image(whiteTex_);
    if (whiteView_.id) sg_destroy_view(whiteView_);
    for (auto& e : viewCache_) sg_destroy_view(e.view);
}

static sg_image make_white_tex() {
    uint32_t white = 0xFFFFFFFF;
    sg_image_desc desc = {};
    desc.type = SG_IMAGETYPE_2D;
    desc.usage.immutable = true;
    desc.width = 1;
    desc.height = 1;
    desc.num_slices = 1;
    desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    desc.data.mip_levels[0] = { &white, 4 };
    return sg_make_image(&desc);
}

sg_view UiRenderer::GetOrCreateView(sg_image img) {
    if (!img.id) {
        return whiteView_;
    }
    if (img.id == whiteTex_.id) return whiteView_;
    for (auto& e : viewCache_) {
        if (e.img.id == img.id) return e.view;
    }
    if (sg_query_image_state(img) != SG_RESOURCESTATE_VALID) {
        std::fprintf(stderr, "UiRenderer: rejected invalid image handle 0x%08x for view creation\n", img.id);
        return whiteView_;
    }
    sg_view_desc vd = {};
    vd.texture.image = img;
    vd.texture.mip_levels.base = 0;
    vd.texture.mip_levels.count = 1;
    vd.texture.slices.base = 0;
    vd.texture.slices.count = 1;
    sg_view view = sg_make_view(&vd);
    viewCache_.push_back({ img, view });
    return view;
}

bool UiRenderer::Init(int /*screenW*/, int /*screenH*/) {
    // Multiple UiRenderer instances may coexist (MainUI overlay renderer
    // plus the FDF HUD host renderer); each owns its GL objects.
    whiteTex_ = make_white_tex();
    if (!sg_query_image_state(whiteTex_)) return false;

    {
        sg_view_desc vd = {};
        vd.texture.image = whiteTex_;
        vd.texture.mip_levels.base = 0;
        vd.texture.mip_levels.count = 1;
        vd.texture.slices.base = 0;
        vd.texture.slices.count = 1;
        whiteView_ = sg_make_view(&vd);
    }

    // Sampler
    {
        sg_sampler_desc desc = {};
        desc.min_filter = SG_FILTER_LINEAR;
        desc.mag_filter = SG_FILTER_LINEAR;
        desc.mipmap_filter = SG_FILTER_LINEAR;
        desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        sampler_ = sg_make_sampler(&desc);
    }

    // Shader
    {
        const char* vs_src = R"(
            #version 410 core
            in vec2 aPos;
            in vec2 aUV;
            in vec4 aColor;
            out vec2 vUV;
            out vec4 vColor;
            uniform vec2 uScreenSize;
            void main() {
                vec2 p = aPos / uScreenSize * 2.0 - 1.0;
                p.y = -p.y;
                gl_Position = vec4(p, 0.0, 1.0);
                vUV = aUV;
                vColor = aColor;
            }
        )";
        const char* fs_src = R"(
            #version 410 core
            in vec2 vUV;
            in vec4 vColor;
            out vec4 FragColor;
            uniform sampler2D uTex;
            void main() {
                FragColor = texture(uTex, vUV) * vColor;
            }
        )";

        sg_shader_desc sd = {};
        sd.vertex_func.source = vs_src;
        sd.fragment_func.source = fs_src;

        sd.attrs[0].glsl_name = "aPos";
        sd.attrs[0].base_type = SG_SHADERATTRBASETYPE_FLOAT;
        sd.attrs[1].glsl_name = "aUV";
        sd.attrs[1].base_type = SG_SHADERATTRBASETYPE_FLOAT;
        sd.attrs[2].glsl_name = "aColor";
        sd.attrs[2].base_type = SG_SHADERATTRBASETYPE_FLOAT;

        sd.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
        sd.uniform_blocks[0].size = 8;
        sd.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_NATIVE;
        sd.uniform_blocks[0].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT2;
        sd.uniform_blocks[0].glsl_uniforms[0].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[0].glsl_name = "uScreenSize";

        sd.views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT;
        sd.views[0].texture.image_type = SG_IMAGETYPE_2D;
        sd.views[0].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;

        sd.samplers[0].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;

        sd.texture_sampler_pairs[0].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.texture_sampler_pairs[0].glsl_name = "uTex";
        sd.texture_sampler_pairs[0].view_slot = 0;
        sd.texture_sampler_pairs[0].sampler_slot = 0;

        shader_ = sg_make_shader(&sd);
        if (!sg_query_shader_state(shader_)) return false;
    }

    // Pipeline
    {
        sg_pipeline_desc pd = {};
        pd.shader = shader_;
        pd.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
        pd.layout.attrs[0].buffer_index = 0;
        pd.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
        pd.layout.attrs[1].buffer_index = 0;
        pd.layout.attrs[2].format = SG_VERTEXFORMAT_UBYTE4N;
        pd.layout.attrs[2].buffer_index = 0;
        pd.layout.buffers[0].stride = sizeof(UiVertex);
        pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
        pd.index_type = SG_INDEXTYPE_UINT16;
        pd.colors[0].blend.enabled = true;
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        pd.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
        pd.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        pd.colors[0].write_mask = SG_COLORMASK_RGBA;
        pipeline_ = sg_make_pipeline(&pd);
        if (!sg_query_pipeline_state(pipeline_)) return false;
    }

    // Pre-fill index buffer (static indices for quads)
    for (int qi = 0; qi < MAX_QUADS; qi++) {
        int base = qi * 4;
        indices_[qi * 6 + 0] = (uint16_t)(base + 0);
        indices_[qi * 6 + 1] = (uint16_t)(base + 1);
        indices_[qi * 6 + 2] = (uint16_t)(base + 2);
        indices_[qi * 6 + 3] = (uint16_t)(base + 1);
        indices_[qi * 6 + 4] = (uint16_t)(base + 3);
        indices_[qi * 6 + 5] = (uint16_t)(base + 2);
    }
    {
        sg_buffer_desc desc = {};
        desc.usage.index_buffer = true;
        desc.usage.immutable = true;
        desc.data.ptr = indices_.data();
        desc.data.size = indices_.size() * sizeof(uint16_t);
        desc.size = desc.data.size;
        ibo_ = sg_make_buffer(&desc);
    }

    // Vertex buffer (streaming)
    {
        sg_buffer_desc desc = {};
        desc.usage.vertex_buffer = true;
        desc.usage.stream_update = true;
        desc.size = MAX_VERTS * sizeof(UiVertex);
        vbo_ = sg_make_buffer(&desc);
    }

    return true;
}

void UiRenderer::BeginFrame(int screenW, int screenH) {
    screenW_ = screenW;
    screenH_ = screenH;
    quadCount_ = 0;
    currentTex_ = whiteTex_;
}

void UiRenderer::Flush() {
    if (quadCount_ == 0) return;

    size_t vertBytes = (size_t)quadCount_ * 4 * sizeof(UiVertex);
    sg_range data = { verts_.data(), vertBytes };
    int appendOffset = sg_append_buffer(vbo_, &data);

    sg_bindings bind = {};
    bind.vertex_buffers[0] = vbo_;
    bind.vertex_buffer_offsets[0] = (uint32_t)appendOffset;
    bind.index_buffer = ibo_;
    bind.views[0] = GetOrCreateView(currentTex_);
    bind.samplers[0] = sampler_;

    if (bind.views[0].id && ((bind.views[0].id & 0xFFFFu) >= 4096u)) {
        std::fprintf(stderr, "UiRenderer suspicious bind view: 0x%08x tex=0x%08x\n",
            bind.views[0].id, currentTex_.id);
        quadCount_ = 0;
        currentTex_ = whiteTex_;
        return;
    }

    sg_apply_pipeline(pipeline_);
    sg_apply_bindings(&bind);

    float uScreenSize[2] = { (float)screenW_, (float)screenH_ };
    sg_range unif = { uScreenSize, 8 };
    sg_apply_uniforms(0, &unif);

    sg_draw(0, quadCount_ * 6, 1);

    quadCount_ = 0;
    currentTex_ = whiteTex_;
}

void UiRenderer::DrawQuad(float x, float y, float w, float h, uint32_t color) {
    DrawTexturedQuad(x, y, w, h, whiteTex_, 0, 0, 1, 1, color);
}

void UiRenderer::DrawTexturedQuad(float x, float y, float w, float h,
                                   sg_image tex, float u0, float v0,
                                   float u1, float v1, uint32_t color) {
    if (tex.id != currentTex_.id) {
        Flush();
        currentTex_ = tex;
    }

    if (quadCount_ >= MAX_QUADS) {
        Flush();
    }

    int vi = quadCount_ * 4;
    verts_[vi + 0] = { x,     y + h, u0, v1, color };
    verts_[vi + 1] = { x,     y,     u0, v0, color };
    verts_[vi + 2] = { x + w, y + h, u1, v1, color };
    verts_[vi + 3] = { x + w, y,     u1, v0, color };

    quadCount_++;
}

void UiRenderer::DrawTransposedQuad(float x, float y, float w, float h,
                                     sg_image tex, float su0, float sv0,
                                     float su1, float sv1, uint32_t color) {
    if (tex.id != currentTex_.id) {
        Flush();
        currentTex_ = tex;
    }

    if (quadCount_ >= MAX_QUADS) {
        Flush();
    }

    // U runs across the edge thickness (screen Y in [su0, su1]),
    // V runs along edge length (screen X in [sv0, sv1]).
    int vi = quadCount_ * 4;
    verts_[vi + 0] = { x,     y + h, su1, sv0, color };
    verts_[vi + 1] = { x,     y,     su0, sv0, color };
    verts_[vi + 2] = { x + w, y + h, su1, sv1, color };
    verts_[vi + 3] = { x + w, y,     su0, sv1, color };

    quadCount_++;
}

void UiRenderer::DrawCooldownSweep(float x, float y, float w, float h, float progress, uint32_t color) {
    if (progress <= 0.001f) return;
    progress = std::min(1.0f, progress);

    if (whiteTex_.id != currentTex_.id) {
        Flush();
        currentTex_ = whiteTex_;
    }

    float cx = x + w * 0.5f;
    float cy = y + h * 0.5f;
    float r = std::sqrt(w * w + h * h) * 0.55f;

    const float startAngle = -1.5707963f; // 12 o'clock
    const float sweepAngle = progress * 6.2831853f; // clockwise
    const int segments = std::max(6, (int)(progress * 32.0f));

    for (int i = 0; i < segments; ++i) {
        if (quadCount_ >= MAX_QUADS) {
            Flush();
        }
        float a0 = startAngle + (float)i / (float)segments * sweepAngle;
        float a1 = startAngle + (float)(i + 1) / (float)segments * sweepAngle;

        float p0x = std::clamp(cx + std::cos(a0) * r, x, x + w);
        float p0y = std::clamp(cy + std::sin(a0) * r, y, y + h);
        float p1x = std::clamp(cx + std::cos(a1) * r, x, x + w);
        float p1y = std::clamp(cy + std::sin(a1) * r, y, y + h);

        int vi = quadCount_ * 4;
        verts_[vi + 0] = { cx,  cy,  0.5f, 0.5f, color };
        verts_[vi + 1] = { p0x, p0y, 0.5f, 0.5f, color };
        verts_[vi + 2] = { p1x, p1y, 0.5f, 0.5f, color };
        verts_[vi + 3] = { p1x, p1y, 0.5f, 0.5f, color };
        quadCount_++;
    }
}

void UiRenderer::EndFrame() {
    Flush();
}
