#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include "model/mdx.h"
#include "render/math.h"
#include "render/mdx_animator.h"
#include "render/mdx_event_effects.h"
#include "render/mdx_particles.h"
#include "render/mdx_skinning.h"
#include "resource/mpq_storm.h"
#include <string>
#include <vector>

class MdxViewer {
public:
    MdxViewer();
    ~MdxViewer();

    bool Load(const MdxModel& model, MpqManager* mpq, bool cliffMode = false, bool enableParticles = false);
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
    void RenderAtScale(const Vec3& pos, const Mat4& view, const Mat4& proj, const Vec3& scale, int player = 0,
                       float yawRadians = 0.0f, float alpha = 1.0f, bool depthTest = true);
    void RenderWithModelMatrix(const Mat4& model, const Mat4& view, const Mat4& proj, int player = 0,
                               float alpha = 1.0f, bool depthTest = true);

    const std::string& GetName() const { return name_; }
    bool IsValid() const { return !batches_.empty(); }
    const Vec3& GetBoundsCenter() const { return center_; }
    float GetBoundsRadius() const { return radius_; }
    bool PlayStandSequence();
    bool PlayStandSequenceWithTags(const std::vector<std::string>& secondaryTags);
    bool PlayStandSequenceWithTag(const char* secondaryTag);
    bool PlayWalkSequence();
    bool PlayWalkSequenceWithTags(const std::vector<std::string>& secondaryTags);
    bool PlayWalkSequenceWithTag(const char* secondaryTag);
    bool PlayWalkSequenceAtSpeed(float currentMovementSpeed);
    bool PlayWalkSequenceWithTagsAtSpeed(const std::vector<std::string>& secondaryTags, float currentMovementSpeed);
    bool PlayWalkSequenceWithTagAtSpeed(const char* secondaryTag, float currentMovementSpeed);
    bool PlayWalkSequenceAtSpeedRatio(float speedRatio);
    bool PlayWalkSequenceWithTagsAtSpeedRatio(const std::vector<std::string>& secondaryTags, float speedRatio);
    bool PlayWalkSequenceWithTagAtSpeedRatio(const char* secondaryTag, float speedRatio);
    bool PlayAttackSequence();
    bool PlayAttackSequenceWithTags(const std::vector<std::string>& secondaryTags);
    bool PlayAttackSequenceWithDuration(float durationSeconds);
    bool PlayAttackSequenceWithTagsAndDuration(const std::vector<std::string>& secondaryTags, float durationSeconds);
    bool PlaySpellSequence(float durationSeconds = 0.0f);
    bool PlaySpellSequenceWithTags(const std::vector<std::string>& secondaryTags, float durationSeconds = 0.0f);
    bool PlaySpellSequenceWithTag(const char* secondaryTag, float durationSeconds = 0.0f);
    bool PlayChannelSequence(float durationSeconds = 0.0f);
    bool PlayChannelSequenceWithTags(const std::vector<std::string>& secondaryTags, float durationSeconds = 0.0f);
    bool PlayChannelSequenceWithTag(const char* secondaryTag, float durationSeconds = 0.0f);
    bool PlayBirthThenStandSequence();
    bool PlayDeathSequence();
    bool PlayDecaySequenceWithTags(const std::vector<std::string>& secondaryTags, float durationSeconds);
    bool PlayDecaySequenceWithTag(const char* secondaryTag, float durationSeconds);
    bool PlayDissipateSequence(float durationSeconds);
    bool PlaySequenceByName(const char* sequenceName, bool loop, float speed = 1.0f,
                            bool queueStandAfterCurrent = false);
    bool PlaySequenceByIndex(int index, bool loop, float speed = 1.0f,
                             bool queueStandAfterCurrent = false);
    bool PlaySequenceByPrimaryAndTags(const char* primaryTag,
                                      const std::vector<std::string>& secondaryTags,
                                      bool loop,
                                      float speed = 1.0f,
                                      bool queueStandAfterCurrent = false,
                                      bool restartIfSame = true);
    bool IsPlayingSequenceByPrimaryAndTags(const char* primaryTag,
                                           const std::vector<std::string>& secondaryTags) const;
    bool SetCurrentSequenceFrameByRatio(float ratio);
    bool IsPlayingSequenceWithToken(const char* token) const;
    bool IsCurrentSequenceFinished() const;
    bool IsCurrentSequenceLooping() const;
    bool GetSequenceName(int index, std::string& outName) const;
    float GetAnimationSpeedScale() const { return animationSpeedScale_; }
    int GetTriggeredEventCount() const { return triggeredEventCount_; }
    int GetResolvedEventEffectCount() const { return resolvedEventEffectCount_; }
    const std::vector<MdxEventEffect>& GetResolvedEventEffects() const { return resolvedEventEffects_; }
    void DebugLogState(const char* tag) const;
    void SetFog(const float color[3], float start, float end);
    void DisableFog();

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

    struct Batch {
        sg_buffer vbo = {0};
        sg_buffer ebo = {0};
        int indexCount = 0;
        sg_image textureHandle = {0};
        sg_view textureView = {0};
        sg_sampler sampler = {0};
        bool hasTexture = false;
        int textureId = -1;
        int textureAnimationId = -1;
        bool hasTextureIdTimeline = false;
        MdxTimeline textureIdTimeline;
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
        uint32_t sequenceHintMask = 0;
        float layerAlpha = 1.0f;
        float geosetAlpha = 1.0f;
        float geosetColor[3] = {1.0f, 1.0f, 1.0f};
    };

    struct AnimatedGeosetState {
        float alpha = 1.0f;
        float color[3] = {1.0f, 1.0f, 1.0f};
    };

    struct ModelTextureSlot {
        sg_image handle = {0};
        sg_view view = {0};
        int replaceableId = 0;
        std::string replaceablePath;
    };

    sg_image GetPlayerTexture(Batch& batch, int player);
    void Release();
    void InitializeAnimatedStates();
    void UpdateAnimatedStates();
    void ProcessTriggeredEvents();
    bool SetSequencePreservingWalk(int sequenceIndex, bool loop, float speed);

    // Build skinned vertex buffer from geoset data
    void BuildSkinnedBatch(const MdxGeoset& geo, int geosetIdx,
                           const std::vector<float>* uvSource,
                           const MdxModel& model, MpqManager* mpq);

    static sg_shader sharedShader_;
    static sg_pipeline sharedPipeline_;
    static sg_pipeline sharedNoDepthWritePipeline_;
    static sg_pipeline sharedBlendPipeline_;
    static sg_pipeline sharedAdditivePipeline_;
    static sg_pipeline sharedAddAlphaPipeline_;
    static sg_pipeline sharedOverlayPipeline_;
    static sg_pipeline sharedOverlayBlendPipeline_;
    static sg_pipeline sharedOverlayAdditivePipeline_;
    static sg_pipeline sharedOverlayAddAlphaPipeline_;
    static sg_shader sharedSkinnedShader_;
    static sg_pipeline sharedSkinnedPipeline_;
    static sg_pipeline sharedSkinnedNoDepthWritePipeline_;
    static sg_pipeline sharedSkinnedBlendPipeline_;
    static sg_pipeline sharedSkinnedAdditivePipeline_;
    static sg_pipeline sharedSkinnedAddAlphaPipeline_;
    static sg_pipeline sharedOverlaySkinnedPipeline_;
    static sg_pipeline sharedOverlaySkinnedBlendPipeline_;
    static sg_pipeline sharedOverlaySkinnedAdditivePipeline_;
    static sg_pipeline sharedOverlaySkinnedAddAlphaPipeline_;
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
    std::vector<ModelTextureSlot> modelTextures_;

    // Cached bone texture view (recreated when bone texture changes)
    sg_view cachedBoneView_ = {0};
    uint32_t cachedBoneTexId_ = 0;

    // Particle system
    MdxParticleSystem particles_;
    bool hasParticles_ = false;
    MdxEventEffectResolver eventEffectResolver_;
    bool eventEffectResolverLoaded_ = false;
    std::vector<MdxEventEffect> resolvedEventEffects_;
    int triggeredEventCount_ = 0;
    int resolvedEventEffectCount_ = 0;
    bool pendingStandSequenceAfterCurrent_ = false;
    bool currentSequenceIsWalk_ = false;
    float lastWalkSequenceProgress_ = -1.0f;
    float animationSpeedScale_ = 1.0f;
    float elapsedMs_ = 0.0f;
    bool fogEnabled_ = false;
    float fogColor_[3] = {0.78f, 0.84f, 0.94f};
    float fogStart_ = 12000.0f;
    float fogEnd_ = 65000.0f;
    MdxViewer* sharedSource_ = nullptr;
};
