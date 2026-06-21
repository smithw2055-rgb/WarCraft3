#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include "math.h"
#include <string>
#include "model/mdx.h"
#include <unordered_map>
#include <vector>

class MpqManager;
class MdxViewer;

struct WaveParticle {
    Vec3 location;
    Vec3 velocity;
    Vec3 scale = {1, 1, 1};
    float health = 0;
    float gravity = 0;
    int tail = 0;
};

struct WaveEmitter {
    float posX, posY, posZ;
    float yaw = 0.0f;
    float currentEmission = 0.0f;
    std::vector<WaveParticle> particles;
    float speed = 15.0f;
    float variation = 5.0f;
    float latitude = 15.0f;
    float gravity = 3.0f;
    float lifeSpan = 1.5f;
    float emissionRate = 20.0f;
    float width = 32.0f;
    float length = 4.0f;
    int filterMode = 1; // 0=Blend 1=Additive
    int textureId = -1;
    sg_image textureImage = {0};
    sg_view textureView = {0};
    float segmentColors[3][3] = {};
    float segmentAlphas[3] = {};
    float segmentScaling[3] = {};
    int headOrTail = 0;
    float tailLength = 0;
    float timeMiddle = 0.5f;
    int columns = 1;
    int rows = 1;
    unsigned headIntervals[2][3] = {};
    unsigned tailIntervals[2][3] = {};
    float pivotX = 0, pivotY = 0, pivotZ = 0;
};

struct WaveInstance {
    float posX, posY, posZ;
    float scale = 4.0f;
    float yaw = 0.0f;
    float phase = 0.0f;
    MdxViewer* viewer = nullptr;
};

class WaveBuilder {
public:
    WaveBuilder();
    ~WaveBuilder();

    void Build(MpqManager* mpq, const std::string& tileset,
               int mapWidth, int mapHeight, float centerOffset[2],
               const float* cornerHeights, const float* waterHeights,
               const unsigned char* waterExists, int cornerStride,
               float waterHeightOffset,
               const std::string& shoreDir = "",
               const std::string& shoreSFile = "",
               const std::string& shoreOCFile = "",
               const std::string& shoreICFile = "");

    void Render(const Mat4& view, const Mat4& proj);
    int InstanceCount() const { return (int)(waves_.size() + emitters_.size()); }
    void SetTime(float t) { time_ = t; }

private:
    void Clear();
    void InitParticleShader();
    void EmitParticle(WaveEmitter& em);
    void RenderParticles(const Mat4& view, const Mat4& proj);
    void RenderFallback(const Mat4& view, const Mat4& proj);
    void PrintCount();

    std::vector<WaveEmitter> emitters_;
    std::unordered_map<std::string, MdxModel> modelDataCache_;
    std::unordered_map<std::string, sg_image> textureImageCache_;
    float lastFrameTime_ = -1.0f;

    std::vector<WaveInstance> waves_;
    std::unordered_map<std::string, MdxViewer*> viewerCache_;

    static sg_buffer particleVBO_;
    static sg_image particleTexture_;
    static sg_image whiteTexture_;
    static sg_view whiteTextureView_;
    static sg_sampler particleSampler_;
    static sg_shader particleShader_;
    static sg_pipeline particlePipeline_;
    static int particleInitCount_;

    float time_ = 0.0f;
};
