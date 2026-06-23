#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include "model/mdx.h"
#include "render/math.h"
#include "render/mdx_animator.h"
#include "render/mdx_particles.h"
#include "resource/mpq_storm.h"
#include <string>
#include <vector>

class MdxViewer {
public:
    MdxViewer();
    ~MdxViewer();

    bool Load(const MdxModel& model, MpqManager* mpq, bool cliffMode = false);
    bool ShareModelResourcesFrom(const MdxViewer& other);

    // Per-frame update (animation, particles, etc.)
    void Update(float dt);
    void Update(float dt, const Mat4& view);

    // Render
    void Render(const Mat4& view, const Mat4& proj);
    void RenderAt(const Vec3& pos, const Mat4& view, const Mat4& proj, float scale, int player,
                  float yawRadians, float alpha);
    void RenderAt(const Vec3& pos, const Mat4& view, const Mat4& proj, float scale = 1.0f, int player = 0,
                  float yawRadians = 0.0f, float alpha = 1.0f, bool depthTest = true);

    const std::string& GetName() const { return name_; }
    bool IsValid() const { return !batches_.empty(); }
    const Vec3& GetBoundsCenter() const { return center_; }
    float GetBoundsRadius() const { return radius_; }
    bool PlayStandSequence();
    bool PlayBirthThenStandSequence();
    void DebugLogState(const char* tag) const;

    // Animation control
    MdxAnimator* GetAnimator() { return hasAnimations_ ? &animator_ : nullptr; }
    bool HasAnimations() const { return hasAnimations_; }

    static void InitSharedShader();
    static void ReleaseSharedShader();

private:
    struct VertexStatic {
        float px, py, pz;
        float nx, ny, nz;
        float tu, tv;
    };

    struct VertexSkinned {
        float px, py, pz;
        float nx, ny, nz;
        float tu, tv;
        uint8_t boneIndices[4];
        float boneWeights[4];
        float boneCount;
    };

    struct Batch {
        sg_buffer vbo = {0};
        sg_buffer ebo = {0};
        int indexCount = 0;
        sg_image textureHandle = {0};
        sg_view textureView = {0};
        sg_sampler sampler = {0};
        bool hasTexture = false;
        bool skinned = false; // uses bone skinning
        int replaceableId = 0;
        std::string replaceablePath;
        sg_image playerTexHandle = {0};
        sg_view playerTexView = {0};
        int cachedPlayer = -1;
        // For skinned batches: bone palette indices
        int materialId = -1;
        int geosetIndex = -1;
        int materialLayerIndex = -1;
        int filterMode = 0;
        int layerFlags = 0;
        float layerAlpha = 1.0f;
        float geosetAlpha = 1.0f;
        float geosetColor[3] = {1.0f, 1.0f, 1.0f};
    };

    struct AnimatedGeosetState {
        float alpha = 1.0f;
        float color[3] = {1.0f, 1.0f, 1.0f};
    };

    sg_image GetPlayerTexture(Batch& batch, int player);
    void Release();
    void InitializeAnimatedStates();
    void UpdateAnimatedStates();

    // Build skinned vertex buffer from geoset data
    void BuildSkinnedBatch(const MdxGeoset& geo, int geosetIdx,
                           const std::vector<float>* uvSource,
                           const MdxModel& model, MpqManager* mpq);

    static sg_shader sharedShader_;
    static sg_pipeline sharedPipeline_;
    static sg_pipeline sharedNoDepthWritePipeline_;
    static sg_pipeline sharedBlendPipeline_;
    static sg_pipeline sharedOverlayPipeline_;
    static sg_pipeline sharedOverlayBlendPipeline_;
    static sg_shader sharedSkinnedShader_;
    static sg_pipeline sharedSkinnedPipeline_;
    static sg_pipeline sharedSkinnedNoDepthWritePipeline_;
    static sg_pipeline sharedSkinnedBlendPipeline_;
    static sg_pipeline sharedOverlaySkinnedPipeline_;
    static sg_pipeline sharedOverlaySkinnedBlendPipeline_;
    static sg_sampler sharedSampler_;
    static sg_image sharedWhiteTexture_;
    static sg_view sharedWhiteTextureView_;
    sg_shader shader_ = {0};
    sg_pipeline pipeline_ = {0};
    std::vector<Batch> batches_;
    std::string name_;
    MpqManager* mpq_ = nullptr;
    Vec3 center_ = {0, 0, 0};
    float radius_ = 0;

    // Stored model data for animation
    MdxModel modelData_;
    bool ownsModelData_ = false;
    bool ownsBatchGpuResources_ = true;
    const MdxModel* modelSource_ = nullptr;

    // Animation
    MdxAnimator animator_;
    bool hasAnimations_ = false;
    bool hasAnimatedMaterials_ = false;
    bool initializedAnimator_ = false;
    std::vector<AnimatedGeosetState> animatedGeosetStates_;
    std::vector<std::vector<float>> animatedLayerAlphas_;

    // Cached bone texture view (recreated when bone texture changes)
    sg_view cachedBoneView_ = {0};
    uint32_t cachedBoneTexId_ = 0;

    // Particle system
    MdxParticleSystem particles_;
    bool hasParticles_ = false;
    bool pendingStandSequenceAfterCurrent_ = false;
    MdxViewer* sharedSource_ = nullptr;
};
