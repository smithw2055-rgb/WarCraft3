#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <functional>

// ---- Interpolation types ----
enum class MdxInterpolation : uint32_t {
    None = 0,
    Linear = 1,
    Hermite = 2,
    Bezier = 3,
};

// ---- A single timeline channel attached to a node or geoset animation ----
struct MdxTimeline {
    uint32_t tag = 0;               // e.g. KGTR, KGRT, KGSC
    MdxInterpolation interpolationType = MdxInterpolation::None;
    int globalSequenceId = -1;
    std::vector<uint32_t> frames;   // keyframe frame indices
    std::vector<float> values;      // raw keyframe values
    std::vector<float> inTans;      // incoming tangents (Hermite/Bezier)
    std::vector<float> outTans;     // outgoing tangents (Hermite/Bezier)
};

// ---- Sequence ----
struct MdxSequence {
    std::string name;
    uint32_t startFrame = 0;
    uint32_t endFrame = 0;
    float moveSpeed = 0;
    uint32_t flags = 0;             // 0x1 = non-looping
    float rarity = 0;
    uint32_t syncPoint = 0;
    float boundsMin[3] = {};
    float boundsMax[3] = {};
    float boundsRadius = 0;
};

// ---- Base for any animated node (bone, helper, light, etc.) ----
struct MdxNode {
    std::string name;
    int objectId = -1;
    int parentId = -1;
    uint32_t flags = 0;
    int geosetId = -1;
    int geosetAnimationId = -1;
    std::vector<MdxTimeline> timelines;
};

// ---- Geoset animation ----
struct MdxGeosetAnim {
    int geosetId = -1;
    float alpha = 1.0f;
    std::array<float,3> color = {1,1,1};
    uint32_t flags = 0;
    std::vector<MdxTimeline> timelines;  // KGAL, KGAC (color)
};

// ---- ParticleEmitter1 (old) ----
struct MdxParticleEmitter1 {
    std::string name;
    int objectId = -1;
    int parentId = -1;
    uint32_t flags = 0;
    float emissionRate = 0;
    float gravity = 0;
    float longitude = 0;
    float latitude = 0;
    float lifeSpan = 0;
    float speed = 0;
    float variation = 0;
    int textureId = -1;
    int filterMode = 0;
    int priorityPlane = 0;
    int replaceableId = 0;
    std::string internalModelPath;
    std::vector<MdxTimeline> timelines;
};

struct MdxParticleEmitter1Sample {
    float emissionRate = 0.0f;
    float gravity = 0.0f;
    float longitude = 0.0f;
    float latitude = 0.0f;
    float lifeSpan = 0.0f;
    float speed = 0.0f;
    float visibility = 1.0f;
};

// ---- ParticleEmitter2 ----
struct MdxParticleEmitter2 {
    std::string name;
    int objectId = -1;
    int parentId = -1;
    int flags = 0;

    float speed = 0;
    float variation = 0;
    float latitude = 0;
    float gravity = 0;
    float lifeSpan = 0;
    float emissionRate = 0;
    float length = 0;
    float width = 0;
    int filterMode = 0;
    unsigned rows = 0;
    unsigned columns = 0;
    int headOrTail = 0;
    float tailLength = 0;
    float timeMiddle = 0;
    float segmentColors[3][3] = {};
    unsigned char segmentAlphas[3] = {};
    float segmentScaling[3] = {};
    unsigned headIntervals[2][3] = {};
    unsigned tailIntervals[2][3] = {};
    int textureId = -1;
    unsigned squirt = 0;
    int priorityPlane = 0;
    unsigned replaceableId = 0;

    std::vector<MdxTimeline> timelines;
};

struct MdxParticleEmitter2Sample {
    float speed = 0.0f;
    float variation = 0.0f;
    float latitude = 0.0f;
    float gravity = 0.0f;
    float emissionRate = 0.0f;
    float length = 0.0f;
    float width = 0.0f;
    float visibility = 1.0f;
    int emissionRateKey = -1;
};

// ---- Ribbon emitter ----
struct MdxRibbonEmitter {
    std::string name;
    int objectId = -1;
    int parentId = -1;
    uint32_t flags = 0;

    float heightAbove = 0;
    float heightBelow = 0;
    float alpha = 1.0f;
    float color[3] = {1,1,1};
    float lifeSpan = 0;
    float emissionRate = 0;
    int textureSlot = 0;
    float gravity = 0.0f;
    unsigned rows = 1;
    unsigned columns = 1;
    int priorityPlane = 0;
    int replaceableId = 0;

    std::vector<MdxTimeline> timelines;
};

// ---- Event object (SPL/SPN/UBR/SND) ----
struct MdxEventObject {
    std::string name;
    int objectId = -1;
    int parentId = -1;
    uint32_t flags = 0;
    std::string type; // "SPL", "SPN", "UBR", "SND", "FPT"
    std::string id;   // lookup key for SLK tables
    std::vector<MdxTimeline> timelines; // KEVT (keyframe trigger)
};

// ---- Light (MDX light node) ----
struct MdxLight {
    std::string name;
    int objectId = -1;
    int parentId = -1;
    uint32_t flags = 0;
    int textureId = -1;
    float attenuationStart = 0;
    float attenuationEnd = 0;
    float intensity = 1.0f;
    float color[3] = {1,1,1};
    float ambientIntensity = 0;
    float ambientColor[3] = {0,0,0};
    std::vector<MdxTimeline> timelines; // KLAS, KLAE, KLBC, KABP, KPLA, KPLS
};

// ---- Geoset (extended with skinning data) ----
struct MdxGeoset {
    std::vector<float> vertices;   // [x y z] triples
    std::vector<float> normals;    // [nx ny nz] triples
    std::vector<float> uvs;        // [u v] pairs
    std::vector<std::vector<float>> uvSets; // multiple UV coordinate sets
    std::vector<unsigned> indices; // triangle indices

    // Skin data (classic vertex groups)
    std::vector<uint8_t> vertexGroups;    // per-vertex group index
    std::vector<uint32_t> matrixGroups;   // per-group size
    std::vector<uint32_t> matrixIndices;  // group contents (bone refs)

    // Skin data (Reforged compact skin)
    std::vector<uint16_t> skin;          // (boneIndex<<8)|weight per vertex

    int materialId = 0;
};

// ---- Material layer ----
struct MdxLayer {
    int textureId = -1;
    int filterMode = 0;
    int flags = 0;
    int textureAnimationId = -1;
    int coordId = 0;
    float alpha = 1.0f;
    std::vector<MdxTimeline> timelines; // KMLT (alpha), KMTF (texture id)
};

// ---- Material ----
struct MdxMaterial {
    int priorityPlane = 0;
    int flags = 0;
    std::vector<MdxLayer> layers;
};

// ---- Texture reference ----
struct MdxTextureRef {
    int replaceableId = 0;
    std::string path;
};

// ---- Texture animation (TXAN) ----
struct MdxTexAnim {
    std::vector<MdxTimeline> timelines; // KTAT (translation), KTAR (rotation), KTAS (scale)
};

// ---- Camera ----
struct MdxCamera {
    std::string name;
    float pos[3] = {};
    float target[3] = {};
    float fov = 0;
    float farClip = 0;
    float nearClip = 0;
    std::vector<MdxTimeline> timelines; // KCTR (translation), KTTR (target)
};

// ---- Attachment ----
struct MdxAttachment {
    std::string name;
    int objectId = -1;
    int parentId = -1;
    uint32_t flags = 0;
    std::string path;
    int attachmentId = 0;
    std::vector<MdxTimeline> timelines;
};

// ---- Top-level model ----
struct MdxModel {
    std::string name;
    int version = 800;

    // Geometry
    std::vector<MdxGeoset> geosets;
    std::vector<MdxMaterial> materials;
    std::vector<MdxTextureRef> textures;
    std::vector<MdxTexAnim> texAnims;

    // Animation data
    std::vector<MdxSequence> sequences;
    std::vector<std::array<float,3>> pivotPoints;

    // Nodes with timelines (ordered by objectId)
    std::vector<MdxNode> bones;           // BONE
    std::vector<MdxNode> helpers;         // HELP
    std::vector<MdxAttachment> attachments; // ATCH
    std::vector<MdxLight> lights;         // LITE
    std::vector<MdxCamera> cameras;       // CAMS

    // Emitters
    std::vector<MdxParticleEmitter1> particleEmitters1; // PREM
    std::vector<MdxParticleEmitter2> particleEmitters2; // PRE2
    std::vector<MdxRibbonEmitter> ribbonEmitters;       // RIBB
    std::vector<MdxEventObject> eventObjects;           // EVTS

    // Geoset animations
    std::vector<MdxGeosetAnim> geosetAnims;

    // Global sequences
    std::vector<uint32_t> globalSequences;

    // DNC collision shapes (CLID)
    std::vector<uint32_t> collisionShapes;

    // Bounds
    float boundsMin[3] = {};
    float boundsMax[3] = {};
    float boundsRadius = 0;
};

// ---- Keyframe sampling helpers ----
// Evaluate a timeline at a given frame (normalized to [0,1] between surrounding keys)
// Returns true if data exists and writes to `out`; false if timeline is empty.
bool MdxSampleTimeline(const MdxTimeline& tl, uint32_t frame, float* out, int elemCount, float defaultVal = 0.0f);
bool MdxSampleParticleEmitter1(const MdxParticleEmitter1& emitter,
                               const MdxModel* model,
                               uint32_t frame,
                               float elapsedMs,
                               MdxParticleEmitter1Sample& out,
                               int sequenceIndex = -1);
bool MdxSampleParticleEmitter2(const MdxParticleEmitter2& emitter,
                               const MdxModel* model,
                               uint32_t frame,
                               float elapsedMs,
                               MdxParticleEmitter2Sample& out,
                               int sequenceIndex = -1);

// Geoset animation color timelines (KGAC) are stored as BGR; convert them for
// renderer-facing RGB color uniforms.
void MdxConvertAnimatedColorBgrToRgb(const float bgr[3], float rgb[3]);

// ---- MDX file loading ----
bool MdxLoad(const uint8_t* data, size_t size, MdxModel& out);
