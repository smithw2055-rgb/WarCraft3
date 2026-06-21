#include "splat_model.h"
#include "shader.h"
#include "renderer.h"
#include <cstring>

SplatModel::SplatModel() = default;

SplatModel::~SplatModel() {
    if (!Renderer::IsActive()) {
        texture_ = {};
        sampler_ = {};
        vbo_ = {};
        pipeline_ = {};
        return;
    }
    if (sg_query_image_state(texture_) == SG_RESOURCESTATE_VALID)
        sg_destroy_image(texture_);
    if (sg_query_sampler_state(sampler_) == SG_RESOURCESTATE_VALID)
        sg_destroy_sampler(sampler_);
    if (sg_query_buffer_state(vbo_) == SG_RESOURCESTATE_VALID)
        sg_destroy_buffer(vbo_);
    if (sg_query_pipeline_state(pipeline_) == SG_RESOURCESTATE_VALID)
        sg_destroy_pipeline(pipeline_);
}

bool SplatModel::LoadTexture(const std::string& path, sg_image tex) {
    (void)path;
    texture_ = tex;
    sg_sampler_desc sd = {};
    sd.min_filter = SG_FILTER_LINEAR;
    sd.mag_filter = SG_FILTER_LINEAR;
    sd.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    sd.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    sampler_ = sg_make_sampler(&sd);
    return true;
}

void SplatModel::AddSplat(float centerX, float centerY, float scale, float rotation) {
    instances_.push_back({centerX, centerY, scale, rotation});
}

void SplatModel::RemoveSplat(int index) {
    if (index >= 0 && index < (int)instances_.size())
        instances_.erase(instances_.begin() + index);
}

void SplatModel::Render(sg_shader uberSplatShader, const Mat4& mvp, float centerOffset[2],
                         int mapCols, int mapRows, sg_image heightMap, sg_image shadowMap,
                         sg_image waterHeightMap, sg_image fogOfWarMap, float waterHeightOffset) {
    if (instances_.empty() || !uberSplatShader.id || !texture_.id) return;

    if (!vbo_.id) {
        float quad[] = {
            -0.5f, 0.0f, -0.5f,  0.0f, 0.0f,
             0.5f, 0.0f, -0.5f,  1.0f, 0.0f,
            -0.5f, 0.0f,  0.5f,  0.0f, 1.0f,
             0.5f, 0.0f,  0.5f,  1.0f, 1.0f
        };
        sg_buffer_desc bd = {};
        bd.usage.vertex_buffer = true;
        bd.usage.immutable = true;
        bd.data.ptr = quad;
        bd.data.size = sizeof(quad);
        vbo_ = sg_make_buffer(&bd);

        sg_pipeline_desc pd = {};
        pd.shader = uberSplatShader;
        pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
        pd.index_type = SG_INDEXTYPE_NONE;
        pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
        pd.depth.write_enabled = true;
        pd.cull_mode = SG_CULLMODE_NONE;
        pd.colors[0].blend.enabled = true;
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        pd.layout.buffers[0].stride = 20;
        pd.layout.attrs[0].buffer_index = 0; pd.layout.attrs[0].offset = 0;  pd.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3;
        pd.layout.attrs[1].buffer_index = 0; pd.layout.attrs[1].offset = 12; pd.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
        pipeline_ = sg_make_pipeline(&pd);
    }

    sg_apply_pipeline(pipeline_);
    sg_bindings bind = {};
    bind.vertex_buffers[0] = vbo_;

    // VS image bindings
    bind.views[0].id = heightMap.id;
    bind.views[1].id = waterHeightMap.id;
    bind.samplers[0] = sampler_;
    bind.samplers[1] = sampler_;
    // FS image bindings
    bind.views[2].id = texture_.id;
    bind.views[3].id = shadowMap.id;
    bind.views[4].id = fogOfWarMap.id;
    bind.samplers[2] = sampler_;
    bind.samplers[3] = sampler_;
    bind.samplers[4] = sampler_;
    sg_apply_bindings(&bind);

    // VS uniforms
    struct VSBlock {
        float MVP[16];
        float centerOffset[2], mapSize[2];
        float splatCenter[2];
        float splatScale, splatRotation, waterHeightOffset;
    };
    // FS uniforms
    struct FSBlock {
        float shadowScale[2];
        float fogColor[4], fogParams[4];
    };

    for (auto& inst : instances_) {
        VSBlock vsU;
        memcpy(vsU.MVP, mvp.Data(), 64);
        vsU.centerOffset[0] = centerOffset[0]; vsU.centerOffset[1] = centerOffset[1];
        vsU.mapSize[0] = (float)mapCols; vsU.mapSize[1] = (float)mapRows;
        vsU.splatCenter[0] = inst.x; vsU.splatCenter[1] = inst.y;
        vsU.splatScale = inst.scale * 128.0f;
        vsU.splatRotation = inst.rotation;
        vsU.waterHeightOffset = waterHeightOffset;
        sg_apply_uniforms(0, sg_rng(vsU));

        FSBlock fsU = {};
        fsU.shadowScale[0] = 1.0f; fsU.shadowScale[1] = 1.0f;
        sg_apply_uniforms(1, sg_rng(fsU));

        sg_draw(0, 4, 1);
    }
}
