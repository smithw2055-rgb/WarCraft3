#include "mdx_viewer.h"
#include "shader.h"
#include "renderer.h"
#include "replaceable_ids.h"
#include "asset/BLP.h"
#include "asset/DDS.h"
#include "asset/stb_image.h"
#include <iostream>
#include <cstring>
#include <cmath>

MdxViewer::~MdxViewer() { Release(); }

sg_shader MdxViewer::sharedShader_ = {0};
sg_pipeline MdxViewer::sharedPipeline_ = {0};
sg_pipeline MdxViewer::sharedNoDepthWritePipeline_ = {0};
sg_pipeline MdxViewer::sharedBlendPipeline_ = {0};
sg_pipeline MdxViewer::sharedOverlayPipeline_ = {0};
sg_pipeline MdxViewer::sharedOverlayBlendPipeline_ = {0};
sg_shader MdxViewer::sharedSkinnedShader_ = {0};
sg_pipeline MdxViewer::sharedSkinnedPipeline_ = {0};
sg_pipeline MdxViewer::sharedSkinnedNoDepthWritePipeline_ = {0};
sg_pipeline MdxViewer::sharedSkinnedBlendPipeline_ = {0};
sg_pipeline MdxViewer::sharedOverlaySkinnedPipeline_ = {0};
sg_pipeline MdxViewer::sharedOverlaySkinnedBlendPipeline_ = {0};
sg_sampler MdxViewer::sharedSampler_ = {0};

void MdxViewer::InitSharedShader() {
    if (sharedShader_.id) return;

    // Create shared sampler
    sg_sampler_desc smpDesc = {};
    smpDesc.min_filter = SG_FILTER_LINEAR;
    smpDesc.mag_filter = SG_FILTER_LINEAR;
    smpDesc.mipmap_filter = SG_FILTER_LINEAR;
    smpDesc.wrap_u = SG_WRAP_REPEAT;
    smpDesc.wrap_v = SG_WRAP_REPEAT;
    sharedSampler_ = sg_make_sampler(&smpDesc);

    // ---- Static (non-skinned) shader ----
    {
        const char* vertSrc = R"(
            #version 410 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aNorm;
            layout(location = 2) in vec2 aUV;
            out vec2 vUV;
            out vec3 vNorm;
            uniform mat4 uMVP;
            uniform mat4 uModel;
            void main() {
                gl_Position = uMVP * vec4(aPos, 1.0);
                vUV = aUV;
                vNorm = mat3(uModel) * aNorm;
            }
        )";
        const char* fragSrc = R"(
            #version 410 core
            in vec2 vUV;
            in vec3 vNorm;
            out vec4 FragColor;
            uniform sampler2D uTexture;
            uniform float uHasTex;
            uniform float uFilterMode;
            uniform vec4 uDebugColor;
            void main() {
                vec3 light = normalize(vec3(0.5, 1.0, 0.3));
                float diff = max(abs(dot(normalize(vNorm), light)), 0.35);
                vec4 color = uDebugColor;
                if (uHasTex > 0.5) color = texture(uTexture, vUV) * uDebugColor;
                if (uFilterMode == 1.0 && color.a < 0.75) discard;
                if (uFilterMode >= 5.0 && color.a < 0.02) discard;
                FragColor = vec4(color.rgb * diff, color.a);
                if (FragColor.a < 0.01) discard;
            }
        )";

        sg_shader_desc sd = {};
        sd.vertex_func.source = vertSrc;
        sd.fragment_func.source = fragSrc;
        sd.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
        sd.uniform_blocks[0].size = 128;
        sd.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_NATIVE;
        sd.uniform_blocks[0].glsl_uniforms[0].type = SG_UNIFORMTYPE_MAT4;
        sd.uniform_blocks[0].glsl_uniforms[0].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[0].glsl_name = "uMVP";
        sd.uniform_blocks[0].glsl_uniforms[1].type = SG_UNIFORMTYPE_MAT4;
        sd.uniform_blocks[0].glsl_uniforms[1].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[1].glsl_name = "uModel";
        sd.uniform_blocks[1].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.uniform_blocks[1].size = 24;
        sd.uniform_blocks[1].layout = SG_UNIFORMLAYOUT_NATIVE;
        sd.uniform_blocks[1].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
        sd.uniform_blocks[1].glsl_uniforms[0].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[0].glsl_name = "uDebugColor";
        sd.uniform_blocks[1].glsl_uniforms[1].type = SG_UNIFORMTYPE_FLOAT;
        sd.uniform_blocks[1].glsl_uniforms[1].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[1].glsl_name = "uHasTex";
        sd.uniform_blocks[1].glsl_uniforms[2].type = SG_UNIFORMTYPE_FLOAT;
        sd.uniform_blocks[1].glsl_uniforms[2].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[2].glsl_name = "uFilterMode";
        sd.views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT;
        sd.views[0].texture.image_type = SG_IMAGETYPE_2D;
        sd.views[0].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
        sd.samplers[0].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;
        sd.texture_sampler_pairs[0].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.texture_sampler_pairs[0].glsl_name = "uTexture";
        sd.texture_sampler_pairs[0].view_slot = 0;
        sd.texture_sampler_pairs[0].sampler_slot = 0;
        sharedShader_ = sg_make_shader(&sd);
    }

    // ---- Skinned shader ----
    {
        const char* vertSrc = R"(
            #version 410 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aNorm;
            layout(location = 2) in vec2 aUV;
            layout(location = 3) in uvec4 aBoneIndices;
            layout(location = 4) in vec4 aBoneWeights;
            layout(location = 5) in float aBoneCount;

            out vec2 vUV;
            out vec3 vNorm;
            uniform mat4 uMVP;
            uniform mat4 uModel;
            uniform sampler2D uBoneTexture;
            uniform int uBoneCount;

            mat4 getBoneMatrix(int idx) {
                ivec2 texSize = textureSize(uBoneTexture, 0);
                int base = idx * 4;
                vec4 c0 = texelFetch(uBoneTexture, ivec2(base + 0, 0), 0);
                vec4 c1 = texelFetch(uBoneTexture, ivec2(base + 1, 0), 0);
                vec4 c2 = texelFetch(uBoneTexture, ivec2(base + 2, 0), 0);
                vec4 c3 = texelFetch(uBoneTexture, ivec2(base + 3, 0), 0);
                return mat4(c0, c1, c2, c3);
            }

            void main() {
                vec3 pos = vec3(0);
                vec3 norm = vec3(0);
                int numBones = int(aBoneCount);
                for (int i = 0; i < 4 && i < numBones; i++) {
                    int bi = int(aBoneIndices[i]);
                    float w = aBoneWeights[i];
                    if (bi >= 0 && bi < uBoneCount) {
                        mat4 m = getBoneMatrix(bi);
                        pos += (m * vec4(aPos, 1.0)).xyz * w;
                        norm += (mat3(m) * aNorm) * w;
                    }
                }
                gl_Position = uMVP * vec4(pos, 1.0);
                vUV = aUV;
                vNorm = normalize(mat3(uModel) * norm);
            }
        )";
        const char* fragSrc = R"(
            #version 410 core
            in vec2 vUV;
            in vec3 vNorm;
            out vec4 FragColor;
            uniform sampler2D uTexture;
            uniform float uHasTex;
            uniform float uFilterMode;
            uniform vec4 uDebugColor;
            void main() {
                vec3 light = normalize(vec3(0.5, 1.0, 0.3));
                float diff = max(abs(dot(normalize(vNorm), light)), 0.35);
                vec4 color = uDebugColor;
                if (uHasTex > 0.5) color = texture(uTexture, vUV) * uDebugColor;
                if (uFilterMode == 1.0 && color.a < 0.75) discard;
                if (uFilterMode >= 5.0 && color.a < 0.02) discard;
                FragColor = vec4(color.rgb * diff, color.a);
                if (FragColor.a < 0.01) discard;
            }
        )";

        sg_shader_desc sd = {};
        sd.vertex_func.source = vertSrc;
        sd.fragment_func.source = fragSrc;

        // VS uniforms: uMVP(64) + uModel(64) + uBoneCount(4) = 132 bytes
        sd.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
        sd.uniform_blocks[0].size = 132;
        sd.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_NATIVE;
        sd.uniform_blocks[0].glsl_uniforms[0].type = SG_UNIFORMTYPE_MAT4;
        sd.uniform_blocks[0].glsl_uniforms[0].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[0].glsl_name = "uMVP";
        sd.uniform_blocks[0].glsl_uniforms[1].type = SG_UNIFORMTYPE_MAT4;
        sd.uniform_blocks[0].glsl_uniforms[1].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[1].glsl_name = "uModel";
        sd.uniform_blocks[0].glsl_uniforms[2].type = SG_UNIFORMTYPE_INT;
        sd.uniform_blocks[0].glsl_uniforms[2].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[2].glsl_name = "uBoneCount";

        sd.uniform_blocks[1].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.uniform_blocks[1].size = 24;
        sd.uniform_blocks[1].layout = SG_UNIFORMLAYOUT_NATIVE;
        sd.uniform_blocks[1].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
        sd.uniform_blocks[1].glsl_uniforms[0].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[0].glsl_name = "uDebugColor";
        sd.uniform_blocks[1].glsl_uniforms[1].type = SG_UNIFORMTYPE_FLOAT;
        sd.uniform_blocks[1].glsl_uniforms[1].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[1].glsl_name = "uHasTex";
        sd.uniform_blocks[1].glsl_uniforms[2].type = SG_UNIFORMTYPE_FLOAT;
        sd.uniform_blocks[1].glsl_uniforms[2].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[2].glsl_name = "uFilterMode";

        // Bone texture (VS, slot 1)
        sd.views[0].texture.stage = SG_SHADERSTAGE_VERTEX;
        sd.views[0].texture.image_type = SG_IMAGETYPE_2D;
        sd.views[0].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
        sd.samplers[0].stage = SG_SHADERSTAGE_VERTEX;
        sd.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;
        sd.texture_sampler_pairs[0].stage = SG_SHADERSTAGE_VERTEX;
        sd.texture_sampler_pairs[0].glsl_name = "uBoneTexture";
        sd.texture_sampler_pairs[0].view_slot = 0;
        sd.texture_sampler_pairs[0].sampler_slot = 0;

        // Diffuse texture (FS, slot 1)
        sd.views[1].texture.stage = SG_SHADERSTAGE_FRAGMENT;
        sd.views[1].texture.image_type = SG_IMAGETYPE_2D;
        sd.views[1].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
        sd.samplers[1].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.samplers[1].sampler_type = SG_SAMPLERTYPE_FILTERING;
        sd.texture_sampler_pairs[1].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.texture_sampler_pairs[1].glsl_name = "uTexture";
        sd.texture_sampler_pairs[1].view_slot = 1;
        sd.texture_sampler_pairs[1].sampler_slot = 1;

        sharedSkinnedShader_ = sg_make_shader(&sd);
    }

    // ---- Static pipeline ----
    {
        sg_pipeline_desc pd = {};
        pd.shader = sharedShader_;
        pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
        pd.index_type = SG_INDEXTYPE_UINT32;
        pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
        pd.depth.write_enabled = true;
        pd.cull_mode = SG_CULLMODE_NONE;
        pd.colors[0].blend.enabled = false;
        pd.layout.buffers[0].stride = 32; // 8 floats x 4 bytes
        pd.layout.attrs[0] = {0, 0, SG_VERTEXFORMAT_FLOAT3};
        pd.layout.attrs[1] = {0, 12, SG_VERTEXFORMAT_FLOAT3};
        pd.layout.attrs[2] = {0, 24, SG_VERTEXFORMAT_FLOAT2};
        sharedPipeline_ = sg_make_pipeline(&pd);

        pd.depth.write_enabled = false;
        sharedNoDepthWritePipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.enabled = true;
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        pd.depth.write_enabled = false;
        sharedBlendPipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.enabled = false;
        pd.depth.compare = SG_COMPAREFUNC_ALWAYS;
        pd.depth.write_enabled = false;
        sharedOverlayPipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.enabled = true;
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        sharedOverlayBlendPipeline_ = sg_make_pipeline(&pd);
    }

    // ---- Skinned pipeline ----
    {
        sg_pipeline_desc pd = {};
        pd.shader = sharedSkinnedShader_;
        pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
        pd.index_type = SG_INDEXTYPE_UINT32;
        pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
        pd.depth.write_enabled = true;
        pd.cull_mode = SG_CULLMODE_NONE;
        pd.colors[0].blend.enabled = false;
        // Vertex: pos(3f) + norm(3f) + uv(2f) + boneIdx(4ub) + boneWt(4f) + boneCount(1f)
        // Stride = 12 + 12 + 8 + 4 + 16 + 4 = 56 bytes
        pd.layout.buffers[0].stride = 56;
        pd.layout.attrs[0] = {0, 0, SG_VERTEXFORMAT_FLOAT3};          // aPos
        pd.layout.attrs[1] = {0, 12, SG_VERTEXFORMAT_FLOAT3};         // aNorm
        pd.layout.attrs[2] = {0, 24, SG_VERTEXFORMAT_FLOAT2};         // aUV
        pd.layout.attrs[3] = {0, 32, SG_VERTEXFORMAT_UBYTE4};         // aBoneIndices
        pd.layout.attrs[4] = {0, 36, SG_VERTEXFORMAT_FLOAT4};         // aBoneWeights
        pd.layout.attrs[5] = {0, 52, SG_VERTEXFORMAT_FLOAT};          // aBoneCount
        sharedSkinnedPipeline_ = sg_make_pipeline(&pd);

        pd.depth.write_enabled = false;
        sharedSkinnedNoDepthWritePipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.enabled = true;
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        pd.depth.write_enabled = false;
        sharedSkinnedBlendPipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.enabled = false;
        pd.depth.compare = SG_COMPAREFUNC_ALWAYS;
        pd.depth.write_enabled = false;
        sharedOverlaySkinnedPipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.enabled = true;
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        sharedOverlaySkinnedBlendPipeline_ = sg_make_pipeline(&pd);
    }
}

void MdxViewer::ReleaseSharedShader() {
    if (sharedOverlayBlendPipeline_.id) sg_destroy_pipeline(sharedOverlayBlendPipeline_);
    if (sharedOverlayPipeline_.id) sg_destroy_pipeline(sharedOverlayPipeline_);
    if (sharedBlendPipeline_.id) sg_destroy_pipeline(sharedBlendPipeline_);
    if (sharedNoDepthWritePipeline_.id) sg_destroy_pipeline(sharedNoDepthWritePipeline_);
    if (sharedPipeline_.id) sg_destroy_pipeline(sharedPipeline_);
    if (sharedOverlaySkinnedBlendPipeline_.id) sg_destroy_pipeline(sharedOverlaySkinnedBlendPipeline_);
    if (sharedOverlaySkinnedPipeline_.id) sg_destroy_pipeline(sharedOverlaySkinnedPipeline_);
    if (sharedSkinnedBlendPipeline_.id) sg_destroy_pipeline(sharedSkinnedBlendPipeline_);
    if (sharedSkinnedNoDepthWritePipeline_.id) sg_destroy_pipeline(sharedSkinnedNoDepthWritePipeline_);
    if (sharedShader_.id) sg_destroy_shader(sharedShader_);
    if (sharedSkinnedPipeline_.id) sg_destroy_pipeline(sharedSkinnedPipeline_);
    if (sharedSkinnedShader_.id) sg_destroy_shader(sharedSkinnedShader_);
    sharedShader_ = {0}; sharedPipeline_ = {0}; sharedNoDepthWritePipeline_ = {0}; sharedBlendPipeline_ = {0};
    sharedOverlayPipeline_ = {0}; sharedOverlayBlendPipeline_ = {0};
    sharedSkinnedShader_ = {0}; sharedSkinnedPipeline_ = {0}; sharedSkinnedNoDepthWritePipeline_ = {0}; sharedSkinnedBlendPipeline_ = {0};
    sharedOverlaySkinnedPipeline_ = {0}; sharedOverlaySkinnedBlendPipeline_ = {0};
}

void MdxViewer::Release() {
    if (!Renderer::IsActive()) { batches_.clear(); shader_ = {0}; return; }
    for (auto& b : batches_) {
        if (b.vbo.id) sg_destroy_buffer(b.vbo);
        if (b.ebo.id) sg_destroy_buffer(b.ebo);
        if (b.textureHandle.id) sg_destroy_image(b.textureHandle);
        if (b.textureView.id) sg_destroy_view(b.textureView);
        if (b.playerTexHandle.id) sg_destroy_image(b.playerTexHandle);
        if (b.playerTexView.id) sg_destroy_view(b.playerTexView);
        if (b.sampler.id) sg_destroy_sampler(b.sampler);
    }
    if (cachedBoneView_.id) { sg_destroy_view(cachedBoneView_); cachedBoneView_ = {0}; }
    cachedBoneTexId_ = 0;
    batches_.clear();
    shader_ = {0};
    if (hasParticles_) { particles_.Destroy(); hasParticles_ = false; }
}

static sg_image CreateSGImage(int w, int h, const uint8_t* pixels) {
    sg_image_desc d = {};
    d.type = SG_IMAGETYPE_2D;
    d.width = w; d.height = h;
    d.pixel_format = SG_PIXELFORMAT_RGBA8;
    d.num_mipmaps = 0;
    d.data.mip_levels[0].ptr = pixels;
    d.data.mip_levels[0].size = (size_t)(w * h * 4);
    sg_image img = sg_make_image(&d);
    if (sg_query_image_state(img) != SG_RESOURCESTATE_VALID) img = {0};
    return img;
}

static sg_image LoadTextureFromMpq(MpqManager* mpq, const std::string& path) {
    if (!mpq) return {0};
    auto tryLoad = [&](const std::string& p) -> sg_image {
        std::vector<uint8_t> data;
        if (!mpq->ReadFile(p, data)) return {0};
        BLPImage img;
        if (DecodeBLP(data.data(), data.size(), img))
            return CreateSGImage(img.width, img.height, img.pixels.data());
        DdsImage dds;
        if (DecodeDDS(data.data(), data.size(), dds))
            return CreateSGImage(dds.width, dds.height, dds.pixels.data());
        int w, h, ch;
        stbi_uc* pixels = stbi_load_from_memory(data.data(), (int)data.size(), &w, &h, &ch, STBI_rgb_alpha);
        if (!pixels) return {0};
        sg_image tex = CreateSGImage(w, h, pixels);
        stbi_image_free(pixels);
        return tex;
    };
    sg_image tex = tryLoad(path);
    if (tex.id) return tex;
    if (path.size() > 4) {
        std::string base = path.substr(0, path.size() - 4);
        tex = tryLoad(base + ".tga");
        if (tex.id) return tex;
        tex = tryLoad(base + ".dds");
        if (tex.id) return tex;
    }
    return {0};
}

bool MdxViewer::Load(const MdxModel& model, MpqManager* mpq, bool cliffMode) {
    Release();
    name_ = model.name;

    InitSharedShader();
    if (!sharedShader_.id || !sharedPipeline_.id) return false;

    // Many Warcraft III models rely on bones/helpers even without animation sequences.
    // Treat any skeletal hierarchy as needing the animator path so bind-pose transforms
    // are still applied.
    hasAnimations_ = !model.bones.empty() || !model.helpers.empty();

    sg_sampler_desc smp = {};
    smp.min_filter = SG_FILTER_LINEAR;
    smp.mag_filter = SG_FILTER_LINEAR;
    smp.mipmap_filter = SG_FILTER_LINEAR;
    smp.wrap_u = SG_WRAP_REPEAT;
    smp.wrap_v = SG_WRAP_REPEAT;

    if (hasAnimations_) {
        modelData_ = model;
        ownsModelData_ = true;
        animator_.Init(modelData_);
        initializedAnimator_ = true;
        if (animator_.GetSequenceCount() > 0) {
            animator_.SetSequence(0, true, 1.0f);
        }
    }

    // Resolve textures for all materials
    struct TexResolve {
        sg_image handle = {0};
        int replaceableId = 0;
        std::string replaceablePath;
    };
    std::vector<TexResolve> texCache(model.textures.size());
    for (size_t ti = 0; ti < model.textures.size(); ti++) {
        auto& texRef = model.textures[ti];
        auto& cache = texCache[ti];
        if (!texRef.path.empty()) {
            std::string path = texRef.path;
            for (auto& c : path) if (c == '/') c = '\\';
            cache.handle = LoadTextureFromMpq(mpq, path);
        } else if (texRef.replaceableId != 0) {
            cache.replaceableId = texRef.replaceableId;
            const char* rp = ReplaceableIds::GetPathString(texRef.replaceableId);
            if (rp && rp[0]) {
                cache.replaceablePath = rp;
                if (texRef.replaceableId == 1 || texRef.replaceableId == 2) {
                    std::string fp = "ReplaceableTextures\\" + std::string(rp)
                        + ReplaceableIds::GetIdString(0) + ".blp";
                    cache.handle = LoadTextureFromMpq(mpq, fp);
                } else {
                    std::string fp = "ReplaceableTextures\\" + std::string(rp) + ".blp";
                    cache.handle = LoadTextureFromMpq(mpq, fp);
                }
            }
        }
    }

    // Create batches for each geoset
    for (int gi = 0; gi < (int)model.geosets.size(); gi++) {
        const auto& geo = model.geosets[gi];
        if (geo.vertices.empty() || geo.indices.empty()) continue;
        const MdxGeosetAnim* geosetAnim = nullptr;
        for (const auto& ga : model.geosetAnims) {
            if (ga.geosetId == gi) {
                geosetAnim = &ga;
                break;
            }
        }

        // Determine if this geoset needs skinning (has matrix groups)
        bool isSkinned = !geo.vertexGroups.empty() && !geo.matrixGroups.empty();

        const MdxMaterial* material = nullptr;
        if (geo.materialId >= 0 && geo.materialId < (int)model.materials.size()) {
            material = &model.materials[geo.materialId];
        }
        int passCount = (material && !material->layers.empty()) ? (int)material->layers.size() : 1;

        for (int pass = 0; pass < passCount; ++pass) {
            const MdxLayer* layer = (material && !material->layers.empty()) ? &material->layers[pass] : nullptr;

            sg_image texHandle = {0};
            int replaceableId = 0;
            std::string replaceablePath;
            int coordId = layer ? std::max(0, layer->coordId) : 0;
            if (layer) {
                int texId = layer->textureId;
                if (texId >= 0 && texId < (int)texCache.size()) {
                    texHandle = texCache[texId].handle;
                    replaceableId = texCache[texId].replaceableId;
                    replaceablePath = texCache[texId].replaceablePath;
                }
            }
            const std::vector<float>* uvSource = &geo.uvs;
            if (!geo.uvSets.empty() && coordId >= 0 && coordId < (int)geo.uvSets.size()) {
                uvSource = &geo.uvSets[coordId];
            }

            if (isSkinned && hasAnimations_) {
                BuildSkinnedBatch(geo, gi, uvSource, model, mpq);
                if (!batches_.empty()) {
                    auto& batch = batches_.back();
                    batch.textureHandle = texHandle;
                    batch.hasTexture = (texHandle.id != 0);
                    if (texHandle.id) {
                        sg_view_desc vd = {}; vd.texture.image = texHandle;
                        batch.textureView = sg_make_view(&vd);
                    }
                    batch.replaceableId = replaceableId;
                    batch.replaceablePath = replaceablePath;
                    batch.sampler = sg_make_sampler(&smp);
                    batch.materialId = geo.materialId;
                    batch.geosetIndex = gi;
                    if (layer) {
                        batch.filterMode = layer->filterMode;
                        batch.layerFlags = layer->flags;
                        batch.layerAlpha = layer->alpha;
                    }
                    if (geosetAnim) {
                        batch.geosetAlpha = geosetAnim->alpha;
                        batch.geosetColor[0] = geosetAnim->color[0];
                        batch.geosetColor[1] = geosetAnim->color[1];
                        batch.geosetColor[2] = geosetAnim->color[2];
                    }
                }
            } else {
                int vc = (int)geo.vertices.size() / 3;
                std::vector<VertexStatic> verts(vc);
                for (int i = 0; i < vc; i++) {
                    float mx = geo.vertices[i*3];
                    float my = geo.vertices[i*3+1];
                    float mz = geo.vertices[i*3+2];
                    if (cliffMode) {
                        verts[i].px = my;  verts[i].py = mz;  verts[i].pz = -mx;
                        float nnx = geo.normals.empty() ? 0 : geo.normals[i*3];
                        float nny = geo.normals.empty() ? 0 : geo.normals[i*3+1];
                        float nnz = geo.normals.empty() ? 0 : geo.normals[i*3+2];
                        verts[i].nx = nny; verts[i].ny = nnz; verts[i].nz = -nnx;
                    } else {
                        verts[i].px = mx;  verts[i].py = mz;  verts[i].pz = -my;
                        if (!geo.normals.empty()) {
                            verts[i].nx = geo.normals[i*3];
                            verts[i].ny = geo.normals[i*3+2];
                            verts[i].nz = -geo.normals[i*3+1];
                        } else { verts[i].nx = 0; verts[i].ny = 1; verts[i].nz = 0; }
                    }
                    if (uvSource && uvSource->size() >= size_t(i * 2 + 2)) {
                        verts[i].tu = (*uvSource)[i*2];
                        verts[i].tv = (*uvSource)[i*2+1];
                    } else { verts[i].tu = 0; verts[i].tv = 0; }
                }

                Batch batch;
                batch.indexCount = (int)geo.indices.size();
                batch.textureHandle = texHandle;
                batch.hasTexture = (texHandle.id != 0);
                if (texHandle.id) {
                    sg_view_desc vd = {}; vd.texture.image = texHandle;
                    batch.textureView = sg_make_view(&vd);
                }
                batch.replaceableId = replaceableId;
                batch.replaceablePath = replaceablePath;
                batch.sampler = sg_make_sampler(&smp);
                batch.skinned = false;
                batch.materialId = geo.materialId;
                batch.geosetIndex = gi;
                if (layer) {
                    batch.filterMode = layer->filterMode;
                    batch.layerFlags = layer->flags;
                    batch.layerAlpha = layer->alpha;
                }
                if (geosetAnim) {
                    batch.geosetAlpha = geosetAnim->alpha;
                    batch.geosetColor[0] = geosetAnim->color[0];
                    batch.geosetColor[1] = geosetAnim->color[1];
                    batch.geosetColor[2] = geosetAnim->color[2];
                }

                sg_buffer_desc vbd = {};
                vbd.usage.vertex_buffer = true; vbd.usage.immutable = true;
                vbd.data.ptr = verts.data(); vbd.data.size = verts.size() * sizeof(VertexStatic);
                batch.vbo = sg_make_buffer(&vbd);

                sg_buffer_desc ibd = {};
                ibd.usage.index_buffer = true; ibd.usage.immutable = true;
                ibd.data.ptr = geo.indices.data(); ibd.data.size = geo.indices.size() * sizeof(unsigned);
                batch.ebo = sg_make_buffer(&ibd);

                batches_.push_back(batch);
            }
        }
    }

    mpq_ = mpq;
    center_ = {
        (model.boundsMin[0] + model.boundsMax[0]) * 0.5f,
        (model.boundsMin[2] + model.boundsMax[2]) * 0.5f,
        -(model.boundsMin[1] + model.boundsMax[1]) * 0.5f
    };
    radius_ = model.boundsRadius > 0 ? model.boundsRadius : 100.0f;

    // Particle systems are temporarily disabled while stabilizing scene rendering.
    hasParticles_ = false;

    std::cout << "MdxViewer: " << name_ << " loaded (batches=" << batches_.size()
              << ", animated=" << hasAnimations_ << ", particles=" << hasParticles_ << ")" << std::endl;
    return !batches_.empty();
}

void MdxViewer::BuildSkinnedBatch(const MdxGeoset& geo, int geosetIdx,
                                   const std::vector<float>* uvSource,
                                   const MdxModel& model, MpqManager* mpq) {
    (void)model; (void)mpq;

    int vc = (int)geo.vertices.size() / 3;
    if (vc == 0) return;

    // Build per-vertex bone data
    std::vector<VertexSkinned> verts(vc);

    // For each vertex, look up its vertexGroup -> matrixGroup -> matrix indices
    for (int i = 0; i < vc; i++) {
        float mx = geo.vertices[i*3];
        float my = geo.vertices[i*3+1];
        float mz = geo.vertices[i*3+2];
        // Y-up conversion
        verts[i].px = mx;  verts[i].py = mz;  verts[i].pz = -my;
        if (!geo.normals.empty()) {
            verts[i].nx = geo.normals[i*3];
            verts[i].ny = geo.normals[i*3+2];
            verts[i].nz = -geo.normals[i*3+1];
        } else {
            verts[i].nx = 0; verts[i].ny = 1; verts[i].nz = 0;
        }
        if (uvSource && uvSource->size() >= size_t(i * 2 + 2)) {
            verts[i].tu = (*uvSource)[i*2];
            verts[i].tv = (*uvSource)[i*2+1];
        } else {
            verts[i].tu = 0; verts[i].tv = 0;
        }

        // Bone data
        memset(verts[i].boneIndices, 0, 4);
        verts[i].boneWeights[0] = 1.0f;
        verts[i].boneWeights[1] = 0;
        verts[i].boneWeights[2] = 0;
        verts[i].boneWeights[3] = 0;
        verts[i].boneCount = 1;

        if (i < (int)geo.vertexGroups.size()) {
            int groupIdx = geo.vertexGroups[i];
            if (groupIdx >= 0 && groupIdx < (int)geo.matrixGroups.size()) {
                int groupStart = 0;
                for (int mg = 0; mg < groupIdx; mg++)
                    groupStart += (int)geo.matrixGroups[mg];
                int boneCount = (int)geo.matrixGroups[groupIdx];
                boneCount = std::min(boneCount, 4);
                verts[i].boneCount = boneCount;
                float totalWeight = 0;
                for (int b = 0; b < boneCount; b++) {
                    int mi = groupStart + b;
                    if (mi < (int)geo.matrixIndices.size()) {
                        int objectId = (int)geo.matrixIndices[mi];
                        int boneIndex = animator_.GetBoneIndexForObjectId(objectId);
                        if (boneIndex < 0) {
                            boneIndex = objectId;
                        }
                        if (boneIndex >= 0 && boneIndex < 256) {
                            verts[i].boneIndices[b] = (uint8_t)boneIndex;
                            verts[i].boneWeights[b] = 1.0f / (float)boneCount;
                            totalWeight += verts[i].boneWeights[b];
                        }
                    }
                }
                // Normalize
                if (totalWeight > 0) {
                    for (int b = 0; b < boneCount; b++)
                        verts[i].boneWeights[b] /= totalWeight;
                }
            }
        }
    }

    Batch batch;
    batch.indexCount = (int)geo.indices.size();
    batch.skinned = true;
    batch.materialId = geo.materialId;

    sg_buffer_desc vbd = {};
    vbd.usage.vertex_buffer = true; vbd.usage.immutable = true;
    vbd.data.ptr = verts.data(); vbd.data.size = verts.size() * sizeof(VertexSkinned);
    batch.vbo = sg_make_buffer(&vbd);

    sg_buffer_desc ibd = {};
    ibd.usage.index_buffer = true; ibd.usage.immutable = true;
    ibd.data.ptr = geo.indices.data(); ibd.data.size = geo.indices.size() * sizeof(unsigned);
    batch.ebo = sg_make_buffer(&ibd);

    batches_.push_back(batch);
}

void MdxViewer::Update(float dt) {
    if (hasAnimations_)
        animator_.Update(dt);
    if (hasParticles_)
        particles_.Update(dt, particles_.GetWorldOffset());
}

void MdxViewer::Update(float dt, const Mat4& view) {
    if (hasAnimations_) {
        Vec3 cameraForward = {-view.m[2], -view.m[6], -view.m[10]};
        animator_.SetCameraForward(cameraForward);
    }
    if (hasAnimations_)
        animator_.Update(dt);
    if (hasParticles_)
        particles_.Update(dt, particles_.GetWorldOffset());
}

void MdxViewer::Render(const Mat4& view, const Mat4& proj) {
    RenderAt(center_, view, proj, 1.0f);
}

void MdxViewer::RenderAt(const Vec3& pos, const Mat4& view, const Mat4& proj, float scale,
                         int player, float yawRadians, float alpha) {
    RenderAt(pos, view, proj, scale, player, yawRadians, alpha, true);
}

sg_image MdxViewer::GetPlayerTexture(Batch& batch, int player) {
    if (player == batch.cachedPlayer && batch.playerTexHandle.id)
        return batch.playerTexHandle;
    if (batch.replaceablePath.empty()) return {0};
    if (player < 0) player = 0;
    if (player > 23) player = 23;
    if (batch.playerTexView.id) {
        sg_destroy_view(batch.playerTexView);
        batch.playerTexView = {0};
    }
    if (batch.playerTexHandle.id) {
        sg_destroy_image(batch.playerTexHandle);
        batch.playerTexHandle = {0};
    }
    std::string fullPath = "ReplaceableTextures\\" + batch.replaceablePath
        + ReplaceableIds::GetIdString(player) + ".blp";
    sg_image tex = LoadTextureFromMpq(mpq_, fullPath);
    if (tex.id) {
        sg_view_desc vd = {};
        vd.texture.image = tex;
        batch.playerTexView = sg_make_view(&vd);
    }
    batch.playerTexHandle = tex;
    batch.cachedPlayer = player;
    return tex;
}

void MdxViewer::RenderAt(const Vec3& pos, const Mat4& view, const Mat4& proj, float scale,
                         int player, float yawRadians, float alpha, bool depthTest) {
    if (batches_.empty() || (!sharedShader_.id && !sharedSkinnedShader_.id)) return;
    static bool s_loggedFirstOverlayRender = false;
    if (!s_loggedFirstOverlayRender && !depthTest) {
        int skinnedBatches = 0;
        for (const auto& batch : batches_) {
            if (batch.skinned) ++skinnedBatches;
        }
        std::fprintf(stderr,
            "MdxViewer::RenderAt probe name=%s batches=%zu skinned=%d pos=(%.2f,%.2f,%.2f) scale=%.2f alpha=%.2f\n",
            name_.c_str(), batches_.size(), skinnedBatches, pos.x, pos.y, pos.z, scale, alpha);
        s_loggedFirstOverlayRender = true;
    }

    Mat4 model = Mat4::Identity();
    float c = std::cos(yawRadians), s = std::sin(yawRadians);
    model.m[0] = c * scale;  model.m[2] = -s * scale;
    model.m[5] = scale;
    model.m[8] = s * scale;  model.m[10] = c * scale;
    model.m[12] = pos.x; model.m[13] = pos.y; model.m[14] = pos.z;

    Mat4 mvp = proj * view * model;

    struct VSBlock { float MVP[16]; float Model[16]; };
    struct VSBlockSkinned { float MVP[16]; float Model[16]; int boneCount; };
    struct FSBlock { float color[4]; float hasTex; float filterMode; };

    VSBlock vsU; std::memcpy(vsU.MVP, mvp.Data(), 64); std::memcpy(vsU.Model, model.Data(), 64);
    VSBlockSkinned vsUS;
    std::memcpy(vsUS.MVP, mvp.Data(), 64);
    std::memcpy(vsUS.Model, model.Data(), 64);
    vsUS.boneCount = animator_.GetBoneCount();
    FSBlock fsU = {};
    fsU.color[0] = 1.0f; fsU.color[1] = 1.0f; fsU.color[2] = 1.0f; fsU.color[3] = alpha;

    sg_sampler linearSmp = {0};
    sg_sampler_desc smpDesc = {};
    smpDesc.min_filter = SG_FILTER_LINEAR;
    smpDesc.mag_filter = SG_FILTER_LINEAR;

    for (auto& batch : batches_) {
        sg_image tex = batch.textureHandle;
        sg_view texView = batch.textureView;
        if (batch.replaceableId == 1 || batch.replaceableId == 2) {
            sg_image pt = GetPlayerTexture(batch, player);
            if (pt.id) {
                tex = pt;
                if (batch.playerTexView.id) texView = batch.playerTexView;
            }
        }

        fsU.hasTex = tex.id ? 1.0f : 0.0f;
        fsU.filterMode = (float)batch.filterMode;
        fsU.color[0] = batch.geosetColor[0];
        fsU.color[1] = batch.geosetColor[1];
        fsU.color[2] = batch.geosetColor[2];
        fsU.color[3] = alpha * batch.layerAlpha * batch.geosetAlpha;
        const bool blended = batch.filterMode >= 2;
        const bool depthEnabled = depthTest && ((batch.layerFlags & 0x40) == 0);
        const bool noDepthWrite = (batch.layerFlags & 0x80) != 0;

        if (batch.skinned && sharedSkinnedShader_.id && sharedSkinnedPipeline_.id) {
            sg_image boneTex = animator_.GetBoneTexture();
            if (!boneTex.id) continue;

            sg_apply_pipeline(depthEnabled
                ? (blended ? sharedSkinnedBlendPipeline_ : (noDepthWrite ? sharedSkinnedNoDepthWritePipeline_ : sharedSkinnedPipeline_))
                : (blended ? sharedOverlaySkinnedBlendPipeline_ : sharedOverlaySkinnedPipeline_));
            sg_apply_uniforms(0, sg_rng(vsUS));
            sg_apply_uniforms(1, sg_rng(fsU));

            sg_bindings bind = {};
            bind.vertex_buffers[0] = batch.vbo;
            bind.index_buffer = batch.ebo;
            if (!linearSmp.id) linearSmp = sg_make_sampler(&smpDesc);
            if (boneTex.id != cachedBoneTexId_) {
                if (cachedBoneView_.id) sg_destroy_view(cachedBoneView_);
                sg_view_desc vd = {};
                vd.texture.image = boneTex;
                cachedBoneView_ = sg_make_view(&vd);
                cachedBoneTexId_ = boneTex.id;
            }
            bind.views[0] = cachedBoneView_;
            bind.samplers[0] = linearSmp;
            // Diffuse texture (FS slot 1)
            if (tex.id && texView.id) {
                bind.views[1] = texView;
                if (!linearSmp.id) linearSmp = sg_make_sampler(&smpDesc);
                bind.samplers[1] = linearSmp;
            }
            sg_apply_bindings(&bind);
            sg_draw(0, batch.indexCount, 1);
        } else if (sharedShader_.id && sharedPipeline_.id) {
            sg_apply_pipeline(depthEnabled
                ? (blended ? sharedBlendPipeline_ : (noDepthWrite ? sharedNoDepthWritePipeline_ : sharedPipeline_))
                : (blended ? sharedOverlayBlendPipeline_ : sharedOverlayPipeline_));
            sg_apply_uniforms(0, sg_rng(vsU));
            sg_apply_uniforms(1, sg_rng(fsU));

            sg_bindings bind = {};
            bind.vertex_buffers[0] = batch.vbo;
            bind.index_buffer = batch.ebo;
            bind.views[0] = texView;
            bind.samplers[0] = batch.sampler;
            sg_apply_bindings(&bind);
            sg_draw(0, batch.indexCount, 1);
        }
    }

    if (linearSmp.id) sg_destroy_sampler(linearSmp);

    // Particle systems are temporarily disabled while stabilizing scene rendering.
}
