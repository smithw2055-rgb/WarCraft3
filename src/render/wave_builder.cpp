#include "wave_builder.h"
// Temporary rebuild anchor for MdxViewer layout debugging.
#include "renderer.h"
#include "resource/slk.h"
#include "resource/mpq_storm.h"
#include "mdx_viewer.h"
#include "shapes.h"
#include "asset/BLP.h"
#include "model/mdx.h"
#include <map>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <cstdlib>

static constexpr float WAVE_TILE_SIZE = 128.0f;
static constexpr float PI = 3.14159265358979323846f;
static constexpr float MODEL_DETAIL_PARTICLE_FACTOR = 0.75f;
static constexpr float MODEL_DETAIL_PARTICLE_FACTOR_INVERSE = 1.0f / 0.75f;

static float RandomRange(float a, float b) {
    return a + (float(std::rand()) / float(RAND_MAX)) * (b - a);
}

static sg_image CreateSGImage(int w, int h, const uint8_t* pixels) {
    sg_image_desc d = {};
    d.type = SG_IMAGETYPE_2D;
    d.width = w;
    d.height = h;
    d.pixel_format = SG_PIXELFORMAT_RGBA8;
    d.num_mipmaps = 0;
    d.data.mip_levels[0].ptr = pixels;
    d.data.mip_levels[0].size = (size_t)(w * h * 4);
    sg_image img = sg_make_image(&d);
    if (sg_query_image_state(img) != SG_RESOURCESTATE_VALID) img = {0};
    return img;
}

static sg_image LoadBLPTexture(MpqManager* mpq, const std::string& path) {
    if (path.empty()) return {0};
    std::vector<uint8_t> data;
    if (!mpq->ReadFile(path, data)) return {0};
    BLPImage img;
    if (!DecodeBLP(data.data(), data.size(), img)) return {0};
    return CreateSGImage(img.width, img.height, img.pixels.data());
}

sg_buffer WaveBuilder::particleVBO_ = {0};
sg_image WaveBuilder::whiteTexture_ = {0};
sg_view WaveBuilder::whiteTextureView_ = {0};
sg_sampler WaveBuilder::particleSampler_ = {0};
sg_shader WaveBuilder::particleShader_ = {0};
sg_pipeline WaveBuilder::particlePipeline_ = {0};
int WaveBuilder::particleInitCount_ = 0;

WaveBuilder::WaveBuilder() = default;

WaveBuilder::~WaveBuilder() { Clear(); }

void WaveBuilder::Clear() {
    if (!Renderer::IsActive()) { emitters_.clear(); waves_.clear(); viewerCache_.clear(); textureImageCache_.clear(); modelDataCache_.clear(); return; }
    for (auto& em : emitters_)
        if (em.textureView.id) sg_destroy_view(em.textureView);
    emitters_.clear();
    waves_.clear();
    for (auto& [path, viewer] : viewerCache_) delete viewer;
    viewerCache_.clear();
    for (auto& [path, img] : textureImageCache_)
        if (img.id) sg_destroy_image(img);
    textureImageCache_.clear();
    modelDataCache_.clear();
}

void WaveBuilder::InitParticleShader() {
    if (particleInitCount_++ > 0) return;

    const char* vs = R"(
        #version 410 core
        in vec2 aPos;
        out vec2 vUV;
        uniform mat4 uMVP;
        void main() {
            gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
            vUV = aPos;
        }
    )";
    const char* fs = R"(
        #version 410 core
        in vec2 vUV;
        out vec4 FragColor;
        uniform vec4 uColor;
        uniform sampler2D uTex;
        void main() {
            vec4 t = texture(uTex, vUV);
            float d = length(vUV - 0.5) * 2.0;
            float mask = smoothstep(1.0, 0.3, d);
            float alpha = mask * uColor.a * t.a;
            FragColor = vec4(t.rgb, alpha);
            if (FragColor.a < 0.01) discard;
        }
    )";

    sg_shader_desc sd = {};
    sd.vertex_func.source = vs;
    sd.fragment_func.source = fs;
    sd.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
    sd.uniform_blocks[0].size = 64;
    sd.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_NATIVE;
    sd.uniform_blocks[0].glsl_uniforms[0].type = SG_UNIFORMTYPE_MAT4;
    sd.uniform_blocks[0].glsl_uniforms[0].array_count = 1;
    sd.uniform_blocks[0].glsl_uniforms[0].glsl_name = "uMVP";
    sd.uniform_blocks[1].stage = SG_SHADERSTAGE_FRAGMENT;
    sd.uniform_blocks[1].size = 16;
    sd.uniform_blocks[1].layout = SG_UNIFORMLAYOUT_NATIVE;
    sd.uniform_blocks[1].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
    sd.uniform_blocks[1].glsl_uniforms[0].array_count = 1;
    sd.uniform_blocks[1].glsl_uniforms[0].glsl_name = "uColor";
    sd.views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT;
    sd.views[0].texture.image_type = SG_IMAGETYPE_2D;
    sd.views[0].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
    sd.samplers[0].stage = SG_SHADERSTAGE_FRAGMENT;
    sd.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;
    sd.texture_sampler_pairs[0].stage = SG_SHADERSTAGE_FRAGMENT;
    sd.texture_sampler_pairs[0].glsl_name = "uTex";
    sd.texture_sampler_pairs[0].view_slot = 0;
    sd.texture_sampler_pairs[0].sampler_slot = 0;
    particleShader_ = sg_make_shader(&sd);
    if (sg_query_shader_state(particleShader_) != SG_RESOURCESTATE_VALID) {
        particleShader_ = {0}; return;
    }

    sg_pipeline_desc pd = {};
    pd.shader = particleShader_;
    pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pd.index_type = SG_INDEXTYPE_UINT32;
    pd.depth.compare = SG_COMPAREFUNC_ALWAYS;  // TEMP: was LESS_EQUAL, testing
    pd.depth.write_enabled = false;
    pd.cull_mode = SG_CULLMODE_NONE;
    pd.colors[0].blend.enabled = true;
    pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
    pd.layout.buffers[0].stride = 8;
    pd.layout.attrs[0] = {0, 0, SG_VERTEXFORMAT_FLOAT2};
    particlePipeline_ = sg_make_pipeline(&pd);

    sg_sampler_desc smp = {};
    smp.min_filter = SG_FILTER_LINEAR;
    smp.mag_filter = SG_FILTER_LINEAR;
    smp.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smp.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    particleSampler_ = sg_make_sampler(&smp);

    // 1x1 white fallback texture
    if (!whiteTexture_.id) {
        uint8_t white[4] = {255,255,255,255};
        sg_image_desc wd = {};
        wd.type = SG_IMAGETYPE_2D;
        wd.width = 1; wd.height = 1;
        wd.pixel_format = SG_PIXELFORMAT_RGBA8;
        wd.num_mipmaps = 0;
        wd.data.mip_levels[0].ptr = white;
        wd.data.mip_levels[0].size = 4;
        whiteTexture_ = sg_make_image(&wd);
        if (whiteTexture_.id) {
            sg_view_desc vd = {}; vd.texture.image = whiteTexture_;
            whiteTextureView_ = sg_make_view(&vd);
        }
    }
}

static MdxViewer* LoadMdxModel(MpqManager* mpq, const std::string& path,
                                std::unordered_map<std::string, MdxViewer*>& cache,
                                std::unordered_map<std::string, MdxModel>& modelCache,
                                std::unordered_map<std::string, sg_image>& texCache) {
    if (!mpq) return nullptr;
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;

    std::vector<uint8_t> data;
    if (!mpq->ReadFile(path, data)) {
        std::cout << "  LoadMdxModel: NOT FOUND " << path << std::endl;
        return nullptr;
    }
    std::cout << "  LoadMdxModel: loading " << path << " (" << data.size() << " bytes)" << std::endl;
    MdxModel model;
    if (!MdxLoad(data.data(), data.size(), model)) {
        std::cout << "  LoadMdxModel: MdxLoad FAILED" << std::endl;
        return nullptr;
    }
    std::cout << "  LoadMdxModel: MdxLoad OK, geosets=" << model.geosets.size()
              << " particleEmitters2=" << model.particleEmitters2.size()
              << " textures=" << model.textures.size() << std::endl;

    // Store model data (for PRE2 extraction) regardless of viewer success
    modelCache[path] = std::move(model);

    // Load textures referenced by PRE2 emitters
    auto& storedModel = modelCache[path];
    for (auto& texRef : storedModel.textures) {
        if (texRef.path.empty()) continue;
        auto it = texCache.find(texRef.path);
        if (it != texCache.end()) continue;
        sg_image img = LoadBLPTexture(mpq, texRef.path);
        if (img.id) {
            texCache[texRef.path] = img;
            std::cout << "    loaded texture: " << texRef.path << std::endl;
        }
    }

    // Try to create a geometry viewer; it's ok if it fails (particle-only models)
    MdxViewer* viewer = new MdxViewer();
    if (viewer->Load(modelCache[path], mpq) && viewer->IsValid()) {
        cache[path] = viewer;
        std::cout << "  LoadMdxModel: loaded OK" << std::endl;
        return viewer;
    }
    delete viewer;
    std::cout << "  LoadMdxModel: no geometry (PRE2 only)" << std::endl;
    return nullptr;
}

void WaveBuilder::Build(MpqManager* mpq, const std::string& tileset,
                         int mapWidth, int mapHeight, float centerOffset[2],
                         const float* cornerHeights, const float* waterHeights,
                         const unsigned char* waterExists, int cornerStride,
                         float waterHeightOffset,
                         const std::string& shoreDir,
                         const std::string& shoreSFile,
                         const std::string& shoreOCFile,
                         const std::string& shoreICFile) {
    Clear();
    if (!mpq || !cornerHeights || !waterHeights || !waterExists || mapWidth < 2 || mapHeight < 2 || tileset.empty())
        return;

    InitParticleShader();

    auto addEmitter = [&](float wx, float wz, float wy, float yaw, const MdxParticleEmitter2* pre2, const MdxModel* model) {
        WaveEmitter em;
        em.posX = wx; em.posY = wz; em.posZ = wy;
        em.yaw = yaw;
        em.currentEmission = 0.5f;
        if (pre2) {
            // Read pivot from model (MDX objectId indexes into pivotPoints)
            if (model && pre2->objectId >= 0 && (size_t)pre2->objectId < model->pivotPoints.size()) {
                auto& pp = model->pivotPoints[pre2->objectId];
                em.pivotX = pp[0]; em.pivotY = pp[1]; em.pivotZ = pp[2];
            }
            em.speed = pre2->speed;
            em.variation = pre2->variation;
            em.latitude = pre2->latitude;
            em.gravity = pre2->gravity;
            em.lifeSpan = pre2->lifeSpan;
            em.emissionRate = pre2->emissionRate;
            em.width = pre2->width;
            em.length = pre2->length;
            em.filterMode = pre2->filterMode;
            em.textureId = pre2->textureId;
            em.headOrTail = pre2->headOrTail;
            em.tailLength = pre2->tailLength;
            em.timeMiddle = pre2->timeMiddle;
            em.columns = (int)pre2->columns;
            em.rows = (int)pre2->rows;
            for (int i = 0; i < 2; i++) {
                for (int j = 0; j < 3; j++) {
                    em.headIntervals[i][j] = pre2->headIntervals[i][j];
                    em.tailIntervals[i][j] = pre2->tailIntervals[i][j];
                }
            }
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++)
                    em.segmentColors[i][j] = pre2->segmentColors[i][j];
                em.segmentAlphas[i] = (pre2->segmentAlphas[i] / 255.0f) * MODEL_DETAIL_PARTICLE_FACTOR_INVERSE;
                em.segmentScaling[i] = pre2->segmentScaling[i]; // raw values, Java uses them as-is
            }
            // Load texture from model's texture table
            if (pre2->textureId >= 0 && model && (size_t)pre2->textureId < model->textures.size()) {
                auto& texRef = model->textures[pre2->textureId];
                auto tit = textureImageCache_.find(texRef.path);
                if (tit != textureImageCache_.end()) {
                    em.textureImage = tit->second;
                    sg_view_desc vd = {}; vd.texture.image = em.textureImage;
                    em.textureView = sg_make_view(&vd);
                }
            }
        }
        emitters_.push_back(em);
    };

    // Java-style tile-center shoreline wave placement
    // Iterates tiles (not edges), places at tile center, one per shoreline tile.
    if (!shoreDir.empty() && !shoreSFile.empty()) {
        auto makePath = [&](const std::string& file) {
            return shoreDir + "\\" + file + "\\" + file + "0.mdx";
        };
        std::string shorelinePath = makePath(shoreSFile);
        MdxViewer* shorelineViewer = LoadMdxModel(mpq, shorelinePath, viewerCache_, modelDataCache_, textureImageCache_);

        auto mcIt = modelDataCache_.find(shorelinePath);
        const auto* pre2Model = (mcIt != modelDataCache_.end()) ? &mcIt->second : nullptr;
        const auto* pre2Vec = pre2Model ? &pre2Model->particleEmitters2 : nullptr;

        auto addTileEmitter = [&](float tx, float ty, float tz, float yaw) {
            if (pre2Vec && !pre2Vec->empty()) {
                for (const auto& pre2 : *pre2Vec)
                    addEmitter(tx, ty, tz, yaw, &pre2, pre2Model);
            } else {
                addEmitter(tx, ty, tz, yaw, nullptr, pre2Model);
            }
        };

        int nShoreline = 0, nInside = 0, nOutside = 0;

        for (int y = 0; y < mapHeight - 1; ++y) {
            for (int x = 0; x < mapWidth - 1; ++x) {
                int a = y * cornerStride + x;
                int b = y * cornerStride + (x + 1);
                int c = (y + 1) * cornerStride + (x + 1);
                int d = (y + 1) * cornerStride + x;

                bool aw = waterExists[a] != 0;
                bool bw = waterExists[b] != 0;
                bool cw = waterExists[c] != 0;
                bool dw = waterExists[d] != 0;
                if (!aw && !bw && !cw && !dw) continue;

                int count = (aw ? 1 : 0) + (bw ? 1 : 0) + (cw ? 1 : 0) + (dw ? 1 : 0);
                if (count == 0 || count == 4) continue;

                float tx = (x * WAVE_TILE_SIZE) + centerOffset[0] + 64.0f;
                float ty = (y * WAVE_TILE_SIZE) + centerOffset[1] + 64.0f;
                // Use WATER height only (matching Java), not max(water, ground)
                float avgWh = (waterHeights[a] + waterHeights[b] + waterHeights[c] + waterHeights[d]) * 0.25f + waterHeightOffset;
                float tz = avgWh * WAVE_TILE_SIZE + 1.0f;

                double rot = 0;
                if (count == 2) {
                    // Shoreline: 2 adjacent water corners (matching Java rotation2)
                    if (aw && bw)      { rot = -PI * 0.5; nShoreline++; }
                    else if (bw && cw) { rot = 0;         nShoreline++; }
                    else if (cw && dw) { rot = PI * 0.5;  nShoreline++; }
                    else if (aw && dw) { rot = PI;        nShoreline++; }
                    else continue; // opposite corners (diagonal), skip
                } else if (count == 1) {
                    // Inside corner: 1 water corner (matching Java rotation - 3PI/4)
                    if (aw)      rot = -PI * 0.75;
                    else if (bw) rot = -PI * 0.25;
                    else if (cw) rot =  PI * 0.25;
                    else         rot =  PI * 0.75;
                    rot -= PI * 0.75;
                    nInside++;
                } else { // count == 3
                    // Outside corner: 3 water = 1 land. Invert to find land corner, match Java rotation + 5PI/4
                    int la = aw ? 0 : 1, lb = bw ? 0 : 1, lc = cw ? 0 : 1;
                    if (la)      rot = -PI * 0.75;
                    else if (lb) rot = -PI * 0.25;
                    else if (lc) rot =  PI * 0.25;
                    else         rot =  PI * 0.75;
                    rot += PI * 1.25;
                    nOutside++;
                }

                addTileEmitter(tx, ty, tz, (float)rot);
            }
        }

        std::cout << "WaveBuilder: shoreline=" << nShoreline
                  << " inside=" << nInside << " outside=" << nOutside
                  << " total=" << (nShoreline + nInside + nOutside)
                  << std::endl;
    }

    PrintCount();
}

void WaveBuilder::PrintCount() {
    std::cout << "WaveBuilder: " << emitters_.size() << " emitters, "
              << waves_.size() << " fallback models" << std::endl;
}

void WaveBuilder::Render(const Mat4& view, const Mat4& proj) {
    // Temporarily disable shoreline particle rendering while scene init/state is stabilized.
    (void)view;
    (void)proj;
    return;
    RenderParticles(view, proj);
}

void WaveBuilder::EmitParticle(WaveEmitter& em) {
    WaveParticle p;
    float cy = std::cos(em.yaw), sy = std::sin(em.yaw);

    // Java Particle2.bind(): spawn position uses node.pivot + random spread
    // Then transforms to world via node.worldMatrix (approximated here with yaw rotation)
    float lx = RandomRange(-em.width * 0.5f, em.width * 0.5f);
    float lz = RandomRange(-em.length * 0.5f, em.length * 0.5f);

    // Apply pivot offset (MDX pivot) + spread, rotated by yaw
    float px = em.pivotX + lx;
    float py = em.pivotY + lz;
    float pz = em.pivotZ;

    // Rotate pivot+spread by emitter yaw around Z (up), then add emitter position
    p.location.x = em.posX + px * cy - py * sy;
    p.location.y = em.posY + px * sy + py * cy;
    p.location.z = em.posZ + pz;

    // Java Particle2.bind(): velocity starts as VEC3_UNIT_Z = (0,0,1) = UP
    // MDX coords: X=right, Y=forward, Z=up — same as our world coords.
    // Spread: random X-axis rotation then random Y-axis rotation → 2D cone.
    // Then emitter yaw (world rotation) around Z.
    float latRad = em.latitude * PI / 180.0f;
    float xAngle = RandomRange(-latRad, latRad);
    float yAngle = RandomRange(-latRad, latRad);

    float vx = 0.0f, vy = 0.0f, vz = 1.0f;

    // X-axis spread: rotate around X, tilting vz into vy
    float cx = std::cos(xAngle), sx = std::sin(xAngle);
    float ny = vy * cx - vz * sx;
    float nz = vy * sx + vz * cx;
    vy = ny; vz = nz;

    // Y-axis spread: rotate around Y, tilting vz into vx
    float cy2 = std::cos(yAngle), sy2 = std::sin(yAngle);
    float nx = vx * cy2 + vz * sy2;
    nz = -vx * sy2 + vz * cy2;
    vx = nx; vz = nz;

    // Emitter yaw (world rotation): rotate around Z
    float tx2 = vx * cy - vy * sy;
    float ty2 = vx * sy + vy * cy;
    vx = tx2; vy = ty2;

    float spd = em.speed + RandomRange(-em.variation, em.variation);
    p.velocity = {vx * spd, vy * spd, vz * spd};
    p.health = em.lifeSpan;
    p.gravity = em.gravity;
    p.scale = {1, 1, 1};
    p.tail = 0;

    em.particles.push_back(p);
}

void WaveBuilder::RenderParticles(const Mat4& view, const Mat4& proj) {
    if (emitters_.empty() || !particleShader_.id || !particlePipeline_.id) return;

    // One-time: show emitter positions relative to camera
    static bool posDiag = false;
    if (!posDiag && emitters_.size() >= 8) {
        posDiag = true;
        float camX = -(view.m[0]*view.m[12] + view.m[1]*view.m[13] + view.m[2]*view.m[14]);
        float camY = -(view.m[4]*view.m[12] + view.m[5]*view.m[13] + view.m[6]*view.m[14]);
        float camZ = -(view.m[8]*view.m[12] + view.m[9]*view.m[13] + view.m[10]*view.m[14]);
        float fwdX = -view.m[8], fwdY = -view.m[9], fwdZ = -view.m[10];
        float fwdLen = std::sqrt(fwdX*fwdX + fwdY*fwdY + fwdZ*fwdZ);
        if (fwdLen > 0.0001f) { fwdX /= fwdLen; fwdY /= fwdLen; fwdZ /= fwdLen; }
        std::cout << "DIAG: cam=(" << camX << "," << camY << "," << camZ
                  << ") fwd=(" << fwdX << "," << fwdY << "," << fwdZ << ")" << std::endl;
        // Show first 8 emitter positions and whether in front of camera
        for (int i = 0; i < 8; i++) {
            auto& e = emitters_[i];
            float dx = e.posX - camX, dy = e.posY - camY, dz = e.posZ - camZ;
            float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            float dotFwd = dx*fwdX + dy*fwdY + dz*fwdZ;
            std::cout << "DIAG em[" << i << "]: pos=(" << e.posX << "," << e.posY << "," << e.posZ
                      << ") dist=" << (int)dist << " front=" << (dotFwd > 0 ? "YES" : "NO") << std::endl;
        }
    }

    Shapes& shapes = Shapes::Instance();
    shapes.Bind();

    float dt = 0.016f;
    if (lastFrameTime_ >= 0.0f) dt = std::max(0.001f, time_ - lastFrameTime_);
    lastFrameTime_ = time_;

    // --- Emission + Update ---
    for (auto& em : emitters_) {
        em.currentEmission += em.emissionRate * dt * MODEL_DETAIL_PARTICLE_FACTOR;
        while (em.currentEmission >= 1.0f) {
            EmitParticle(em);
            em.currentEmission -= 1.0f;
        }
        auto& parts = em.particles;
        for (int i = (int)parts.size() - 1; i >= 0; --i) {
            auto& p = parts[i];
            p.health -= dt;
            if (p.health <= 0) {
                parts[i] = parts.back();
                parts.pop_back();
                continue;
            }
            p.velocity.z -= p.gravity * dt;
            p.location.x += p.velocity.x * dt;
            p.location.y += p.velocity.y * dt;
            p.location.z += p.velocity.z * dt;
        }
    }

    // --- Render phase ---
    if (!particlePipeline_.id) return;
    sg_apply_pipeline(particlePipeline_);

    for (auto& em : emitters_) {
        const float (*segColors)[3] = em.segmentColors[0][0] || em.segmentColors[1][0] || em.segmentColors[2][0]
            ? (const float(*)[3])em.segmentColors
            : (const float(*)[3])nullptr;
        static const float DEF_COLORS[3][3] = {{0.3f,0.6f,1.0f},{0.6f,0.8f,1.0f},{0.8f,0.9f,1.0f}};
        static const float DEF_ALPHAS[3] = {0.6f, 0.3f, 0.05f};
        static const float DEF_SCALES[3] = {1.0f, 0.7f, 0.3f};

        const float* segAlphas = em.segmentAlphas[0] ? em.segmentAlphas : DEF_ALPHAS;
        const float* segScales = em.segmentScaling[0] ? em.segmentScaling : DEF_SCALES;
        const auto& cols = segColors ? segColors : DEF_COLORS;
        float tm = (em.timeMiddle > 0.001f && em.timeMiddle < 0.999f) ? em.timeMiddle : 0.5f;

        for (auto& p : em.particles) {
            float age = 1.0f - p.health / em.lifeSpan;
            if (age < 0) age = 0;
            if (age > 1) age = 1;

            float cr, cg, cb, alpha, psize;
            if (age < tm) {
                float t = (tm > 0.001f) ? age / tm : 0.0f;
                t = t < 1.0f ? t : 1.0f;
                cr = cols[0][0] + (cols[1][0] - cols[0][0]) * t;
                cg = cols[0][1] + (cols[1][1] - cols[0][1]) * t;
                cb = cols[0][2] + (cols[1][2] - cols[0][2]) * t;
                alpha = segAlphas[0] + (segAlphas[1] - segAlphas[0]) * t;
                psize = (segScales[0] + (segScales[1] - segScales[0]) * t) * 50.0f;
            } else {
                float t = (tm < 0.999f) ? (age - tm) / (1.0f - tm) : 0.0f;
                t = t < 1.0f ? t : 1.0f;
                cr = cols[1][0] + (cols[2][0] - cols[1][0]) * t;
                cg = cols[1][1] + (cols[2][1] - cols[1][1]) * t;
                cb = cols[1][2] + (cols[2][2] - cols[1][2]) * t;
                alpha = segAlphas[1] + (segAlphas[2] - segAlphas[1]) * t;
                psize = (segScales[1] + (segScales[2] - segScales[1]) * t) * 50.0f;
            }

            // Simple axis-aligned quad (no xYQuad for now, keep it simple)
            float px = p.location.x;
            float py = p.location.y;
            float pz = p.location.z;
            float hs = psize * 0.5f;

            Mat4 model = Mat4::Identity();
            model.m[0] = psize; model.m[5] = psize;
            model.m[12] = px - hs; model.m[13] = py - hs; model.m[14] = pz;
            Mat4 mvp = proj * view * model;

            sg_bindings bind = {};
            bind.vertex_buffers[0] = shapes.GetVertexBuffer();
            bind.index_buffer = shapes.GetIndexBuffer();
            bind.views[0] = em.textureView.id ? em.textureView : whiteTextureView_;
            bind.samplers[0] = particleSampler_;
            sg_apply_bindings(&bind);

            float color[4] = {cr, cg, cb, alpha};
            sg_apply_uniforms(0, SG_RANGE(mvp));
            sg_apply_uniforms(1, SG_RANGE(color));
            sg_draw(0, shapes.IndexCount(), 1);
        }
    }
}
