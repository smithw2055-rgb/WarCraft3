#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include "model/mdx.h"
#include "render/math.h"
#include <vector>
#include <random>
#include <memory>

class MdxAnimator;
class MpqManager;

struct Particle {
    Vec3 position;
    Vec3 velocity;
    float gravity;
    float life;
    float maxLife;
    float size;
    Vec4 color;
    float texCol;
    float texRow;
};

struct ParticleEmitterState {
    const MdxParticleEmitter2* emitterData = nullptr;
    MdxParticleEmitter2Sample sample;
    int lastEmissionKey = -1;
    float emissionAccum = 0.0f;
    std::vector<Particle> particles;
    sg_buffer instanceBuffer = {0};
    int instanceCapacity = 0;
    sg_image texture = {0};
    sg_view textureView = {0};
    sg_sampler sampler = {0};
    int replaceableId = 0;
    std::string replaceablePath;
};

class MdxParticleSystem {
public:
    MdxParticleSystem();
    ~MdxParticleSystem();

    struct InstanceData {
        float posX, posY, posZ;
        float size;
        float colorR, colorG, colorB, colorA;
        float texCol, texRow;
    };

    void Init(const MdxModel& model, class MpqManager* mpq);
    void Update(float dt, const Vec3& worldOffset = Vec3{0,0,0});
    void Update(float dt, const Vec3& worldOffset, const MdxModel* model, const MdxAnimator* animator,
                uint32_t frame, float elapsedMs);
    void Render(const Mat4& view, const Mat4& proj);
    void SetWorldOffset(const Vec3& offset) { worldOffset_ = offset; }
    const Vec3& GetWorldOffset() const { return worldOffset_; }
    void SetInstanceMatrix(const Mat4& m) {
        instanceMatrix_ = m;
        hasInstanceMatrix_ = true;
        worldOffset_ = Vec3{m.m[12], m.m[13], m.m[14]};
    }
    const Mat4& GetInstanceMatrix() const { return instanceMatrix_; }
    void Destroy();

    int GetParticleCount() const;

    // Shared particle shader/pipeline
    static sg_shader ParticleShader();
    static sg_pipeline ParticlePipeline();
    static void InitSharedParticleResources();
    static void ReleaseSharedParticleResources();

private:
    void EmitParticle(ParticleEmitterState& state, const MdxParticleEmitter2Sample& sample, int count);
    void UpdateParticle(Particle& p, float dt, const MdxParticleEmitter2& data);
    Vec3 RandomDirectionInCone(float latitude, std::mt19937& rng, std::uniform_real_distribution<float>& dist01);

    std::vector<ParticleEmitterState> emitters_;
    // Synthesized Emitter2 views of CORN popcorn emitters (ref
    // MdlxParticleEmitterPopcorn has no dedicated runtime here; mapped to
    // billboard particles so campaign/Reforged effects are visible).
    std::vector<std::unique_ptr<MdxParticleEmitter2>> popcornOwned_;
    sg_buffer quadBuffer_ = {0};
    sg_buffer indexBuffer_ = {0};

    // Static shared resources
    static sg_shader sharedParticleShader_;
    static sg_pipeline sharedParticlePipeline_;
    static sg_pipeline sharedAdditivePipeline_;
    static sg_pipeline sharedModulatePipeline_;
    static sg_buffer sharedQuadBuffer_;
    static sg_buffer sharedIndexBuffer_;

    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist01_;
    Vec3 worldOffset_ = {0, 0, 0};
    Mat4 instanceMatrix_ = Mat4::Identity();
    bool hasInstanceMatrix_ = false;
};
