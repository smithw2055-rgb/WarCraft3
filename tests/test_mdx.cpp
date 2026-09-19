#include "model/mdx.h"
#include "render/mdx_animator.h"
#include "render/mdx_collision.h"
#include "render/mdx_event_splat.h"
#include "render/mdx_prem_particles.h"
#include "render/mdx_sequence_select.h"

#include <cmath>
#include <iostream>

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "test_mdx assertion failed at line " << __LINE__ << ": " #cond << std::endl; return false; } } while (0)

static bool Near(float a, float b, float epsilon = 0.0001f) {
    return std::fabs(a - b) <= epsilon;
}

static float ColumnLength(const Mat4& matrix, int column) {
    const int offset = column * 4;
    return std::sqrt(matrix.m[offset] * matrix.m[offset]
        + matrix.m[offset + 1] * matrix.m[offset + 1]
        + matrix.m[offset + 2] * matrix.m[offset + 2]);
}

static constexpr uint32_t TestMdxTag(char a, char b, char c, char d) {
    return (uint32_t(uint8_t(a)) << 24) | (uint32_t(uint8_t(b)) << 16)
         | (uint32_t(uint8_t(c)) << 8) | uint32_t(uint8_t(d));
}

static void PushMdxTag(std::vector<uint8_t>& out, char a, char b, char c, char d) {
    out.push_back(uint8_t(a));
    out.push_back(uint8_t(b));
    out.push_back(uint8_t(c));
    out.push_back(uint8_t(d));
}

static void PushLE32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(uint8_t(value & 0xffu));
    out.push_back(uint8_t((value >> 8) & 0xffu));
    out.push_back(uint8_t((value >> 16) & 0xffu));
    out.push_back(uint8_t((value >> 24) & 0xffu));
}

static void PushFixedString(std::vector<uint8_t>& out, const char* text, size_t fixedSize) {
    size_t written = 0;
    while (text[written] && written < fixedSize) {
        out.push_back(uint8_t(text[written]));
        ++written;
    }
    while (written < fixedSize) {
        out.push_back(0);
        ++written;
    }
}

bool TestMdxTimelineSamplesLinearValues() {
    MdxTimeline timeline;
    timeline.interpolationType = MdxInterpolation::Linear;
    timeline.frames = {0, 1000};
    timeline.values = {
        0.0f, 10.0f, 20.0f,
        100.0f, 110.0f, 120.0f
    };

    float out[3] = {};
    TEST_ASSERT(MdxSampleTimeline(timeline, 500, out, 3, -1.0f));
    TEST_ASSERT(Near(out[0], 50.0f));
    TEST_ASSERT(Near(out[1], 60.0f));
    TEST_ASSERT(Near(out[2], 70.0f));

    TEST_ASSERT(MdxSampleTimeline(timeline, 1500, out, 3, -1.0f));
    TEST_ASSERT(Near(out[0], 100.0f));
    TEST_ASSERT(Near(out[1], 110.0f));
    TEST_ASSERT(Near(out[2], 120.0f));
    return true;
}

bool TestMdxTimelineRejectsShortValues() {
    MdxTimeline timeline;
    timeline.interpolationType = MdxInterpolation::Linear;
    timeline.frames = {0, 1000};
    timeline.values = {
        0.0f, 10.0f, 20.0f,
        100.0f
    };

    float out[3] = {1.0f, 2.0f, 3.0f};
    TEST_ASSERT(!MdxSampleTimeline(timeline, 500, out, 3, -7.0f));
    TEST_ASSERT(Near(out[0], -7.0f));
    TEST_ASSERT(Near(out[1], -7.0f));
    TEST_ASSERT(Near(out[2], -7.0f));
    return true;
}

bool TestMdxTimelineFallsBackWhenTangentsAreMissing() {
    MdxTimeline timeline;
    timeline.interpolationType = MdxInterpolation::Hermite;
    timeline.frames = {0, 1000};
    timeline.values = {0.0f, 100.0f};

    float out = -1.0f;
    TEST_ASSERT(MdxSampleTimeline(timeline, 250, &out, 1, -7.0f));
    TEST_ASSERT(Near(out, 25.0f));
    return true;
}

bool TestMdxTimelineUsesBezierTangents() {
    MdxTimeline timeline;
    timeline.interpolationType = MdxInterpolation::Bezier;
    timeline.frames = {0, 1000};
    timeline.values = {0.0f, 100.0f};
    timeline.outTans = {100.0f, 100.0f};
    timeline.inTans = {0.0f, 0.0f};

    float out = -1.0f;
    TEST_ASSERT(MdxSampleTimeline(timeline, 500, &out, 1, -7.0f));
    TEST_ASSERT(Near(out, 50.0f));
    return true;
}

bool TestMdxModelTimelineSamplingIgnoresOtherSequences() {
    MdxModel model;

    MdxSequence stand;
    stand.name = "Stand";
    stand.startFrame = 0;
    stand.endFrame = 100;
    model.sequences.push_back(stand);

    MdxSequence attack;
    attack.name = "Attack";
    attack.startFrame = 1000;
    attack.endFrame = 1100;
    model.sequences.push_back(attack);

    MdxTimeline alpha;
    alpha.tag = TestMdxTag('K', 'M', 'T', 'A');
    alpha.interpolationType = MdxInterpolation::Linear;
    alpha.frames = {1000, 1100};
    alpha.values = {0.25f, 0.75f};

    float sampled = -1.0f;
    TEST_ASSERT(!MdxSampleTimelineForModel(alpha, &model, 50, 50.0f, &sampled, 1, 1.0f, 0));
    TEST_ASSERT(Near(sampled, 1.0f));

    TEST_ASSERT(MdxSampleTimelineForModel(alpha, &model, 1050, 1050.0f, &sampled, 1, 1.0f, 1));
    TEST_ASSERT(Near(sampled, 0.5f));
    return true;
}

bool TestMdxAnimatedGeosetColorConvertsBgrToRgb() {
    const float bgr[3] = {0.25f, 0.5f, 0.75f};
    float rgb[3] = {};
    MdxConvertAnimatedColorBgrToRgb(bgr, rgb);
    TEST_ASSERT(Near(rgb[0], 0.75f));
    TEST_ASSERT(Near(rgb[1], 0.5f));
    TEST_ASSERT(Near(rgb[2], 0.25f));
    return true;
}

bool TestMdxParsesLayerTextureIdTimelineAsInteger() {
    std::vector<uint8_t> data;
    PushMdxTag(data, 'M', 'D', 'L', 'X');
    PushMdxTag(data, 'M', 'T', 'L', 'S');
    PushLE32(data, 80); // one material.

    PushLE32(data, 80); // material byte length, including this size field.
    PushLE32(data, 0);  // priority plane.
    PushLE32(data, 0);  // material flags.
    PushMdxTag(data, 'L', 'A', 'Y', 'S');
    PushLE32(data, 1);  // layer count.

    PushLE32(data, 60); // layer byte length, including this size field.
    PushLE32(data, 0);  // filter mode.
    PushLE32(data, 0);  // layer flags.
    PushLE32(data, 0);  // static texture id.
    PushLE32(data, 0xffffffffu); // texture animation id.
    PushLE32(data, 0);  // coord id.
    PushLE32(data, 0x3f800000u); // alpha = 1.0f.

    PushMdxTag(data, 'K', 'M', 'T', 'F');
    PushLE32(data, 2);            // keyframe count.
    PushLE32(data, 0);            // interpolation type: none.
    PushLE32(data, 0xffffffffu);  // global sequence id.
    PushLE32(data, 0);
    PushLE32(data, 0);
    PushLE32(data, 500);
    PushLE32(data, 2);

    MdxModel model;
    TEST_ASSERT(MdxLoad(data.data(), data.size(), model));
    TEST_ASSERT(model.materials.size() == 1);
    TEST_ASSERT(model.materials[0].layers.size() == 1);
    const MdxLayer& layer = model.materials[0].layers[0];
    TEST_ASSERT(layer.timelines.size() == 1);
    const MdxTimeline& textureIdTimeline = layer.timelines[0];
    TEST_ASSERT(textureIdTimeline.tag == TestMdxTag('K', 'M', 'T', 'F'));
    TEST_ASSERT(textureIdTimeline.frames.size() == 2);
    TEST_ASSERT(textureIdTimeline.values.size() == 2);
    TEST_ASSERT(Near(textureIdTimeline.values[0], 0.0f));
    TEST_ASSERT(Near(textureIdTimeline.values[1], 2.0f));
    return true;
}

bool TestMdxParsesLightTypeAndTimelines() {
    std::vector<uint8_t> data;
    PushMdxTag(data, 'M', 'D', 'L', 'X');
    PushMdxTag(data, 'L', 'I', 'T', 'E');
    PushLE32(data, 200); // light object byte length, including this size field.

    PushLE32(data, 200); // object byte length
    PushLE32(data, 96);  // generic object byte length
    PushFixedString(data, "AmbientGlow", 80);
    PushLE32(data, 5);            // object id
    PushLE32(data, 0xffffffffu);  // parent id
    PushLE32(data, 0x200);        // light flags

    PushLE32(data, 2);            // ambient light type
    PushLE32(data, 0x3f800000u);  // attenuation start = 1.0
    PushLE32(data, 0x40000000u);  // attenuation end = 2.0
    PushLE32(data, 0x3e800000u);  // color b = 0.25
    PushLE32(data, 0x3f000000u);  // color g = 0.5
    PushLE32(data, 0x3f400000u);  // color r = 0.75
    PushLE32(data, 0x40400000u);  // intensity = 3.0
    PushLE32(data, 0x3dcccccdU);  // ambient color b = 0.1
    PushLE32(data, 0x3e4ccccdU);  // ambient color g = 0.2
    PushLE32(data, 0x3e99999aU);  // ambient color r = 0.3
    PushLE32(data, 0x40800000u);  // ambient intensity = 4.0

    PushMdxTag(data, 'K', 'L', 'A', 'C');
    PushLE32(data, 1);
    PushLE32(data, 0);            // interpolation: none
    PushLE32(data, 0xffffffffu);  // global sequence id
    PushLE32(data, 100);
    PushLE32(data, 0x3f800000u);
    PushLE32(data, 0x40000000u);
    PushLE32(data, 0x40400000u);

    PushMdxTag(data, 'K', 'L', 'A', 'V');
    PushLE32(data, 1);
    PushLE32(data, 0);            // interpolation: none
    PushLE32(data, 0xffffffffu);  // global sequence id
    PushLE32(data, 100);
    PushLE32(data, 0x00000000u);

    MdxModel model;
    TEST_ASSERT(MdxLoad(data.data(), data.size(), model));
    TEST_ASSERT(model.lights.size() == 1);
    const MdxLight& light = model.lights[0];
    TEST_ASSERT(light.name == "AmbientGlow");
    TEST_ASSERT(light.objectId == 5);
    TEST_ASSERT(light.parentId == -1);
    TEST_ASSERT(light.flags == 0x200);
    TEST_ASSERT(light.type == MdxLightType::Ambient);
    TEST_ASSERT(Near(light.attenuationStart, 1.0f));
    TEST_ASSERT(Near(light.attenuationEnd, 2.0f));
    TEST_ASSERT(Near(light.color[0], 0.25f));
    TEST_ASSERT(Near(light.color[1], 0.5f));
    TEST_ASSERT(Near(light.color[2], 0.75f));
    TEST_ASSERT(Near(light.intensity, 3.0f));
    TEST_ASSERT(Near(light.ambientColor[0], 0.1f));
    TEST_ASSERT(Near(light.ambientColor[1], 0.2f));
    TEST_ASSERT(Near(light.ambientColor[2], 0.3f));
    TEST_ASSERT(Near(light.ambientIntensity, 4.0f));
    TEST_ASSERT(light.timelines.size() == 2);
    TEST_ASSERT(light.timelines[0].tag == TestMdxTag('K', 'L', 'A', 'C'));
    TEST_ASSERT(light.timelines[0].values.size() == 3);
    TEST_ASSERT(light.timelines[1].tag == TestMdxTag('K', 'L', 'A', 'V'));
    TEST_ASSERT(light.timelines[1].values.size() == 1);

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.GetBoneIndexForObjectId(5) == 5);
    TEST_ASSERT(!animator.IsObjectVisible(5));
    return true;
}

bool TestMdxSamplesCameraTimelinesOnlyInsideCurrentSequence() {
    MdxModel model;
    MdxSequence stand;
    stand.name = "Stand";
    stand.startFrame = 0;
    stand.endFrame = 1000;
    model.sequences.push_back(stand);

    MdxSequence cinematic;
    cinematic.name = "Cinematic";
    cinematic.startFrame = 1000;
    cinematic.endFrame = 2000;
    model.sequences.push_back(cinematic);

    MdxCamera camera;
    camera.name = "Camera01";
    camera.pos[0] = 10.0f;
    camera.pos[1] = 20.0f;
    camera.pos[2] = 30.0f;
    camera.target[0] = 1.0f;
    camera.target[1] = 2.0f;
    camera.target[2] = 3.0f;
    camera.fov = 0.75f;
    camera.farClip = 5000.0f;
    camera.nearClip = 8.0f;

    MdxTimeline position;
    position.tag = TestMdxTag('K', 'C', 'T', 'R');
    position.interpolationType = MdxInterpolation::Linear;
    position.frames = {1100, 1900};
    position.values = {
        100.0f, 0.0f, 0.0f,
        200.0f, 20.0f, 40.0f
    };
    camera.timelines.push_back(position);

    MdxTimeline target;
    target.tag = TestMdxTag('K', 'T', 'T', 'R');
    target.interpolationType = MdxInterpolation::Linear;
    target.frames = {1100, 1900};
    target.values = {
        0.0f, 10.0f, 0.0f,
        0.0f, 20.0f, 30.0f
    };
    camera.timelines.push_back(target);

    MdxTimeline roll;
    roll.tag = TestMdxTag('K', 'C', 'R', 'L');
    roll.interpolationType = MdxInterpolation::Linear;
    roll.frames = {1100, 1900};
    roll.values = {0.0f, 1.0f};
    camera.timelines.push_back(roll);

    MdxCameraSample sample;
    TEST_ASSERT(!MdxSampleCamera(camera, &model, 500, 500.0f, sample, 0));
    TEST_ASSERT(Near(sample.pos[0], 10.0f));
    TEST_ASSERT(Near(sample.target[1], 2.0f));
    TEST_ASSERT(Near(sample.roll, 0.0f));

    TEST_ASSERT(MdxSampleCamera(camera, &model, 1500, 1500.0f, sample, 1));
    TEST_ASSERT(Near(sample.pos[0], 160.0f));
    TEST_ASSERT(Near(sample.pos[1], 30.0f));
    TEST_ASSERT(Near(sample.pos[2], 50.0f));
    TEST_ASSERT(Near(sample.target[0], 1.0f));
    TEST_ASSERT(Near(sample.target[1], 17.0f));
    TEST_ASSERT(Near(sample.target[2], 18.0f));
    TEST_ASSERT(Near(sample.roll, 0.5f));
    TEST_ASSERT(Near(sample.fov, 0.75f));
    TEST_ASSERT(Near(sample.farClip, 5000.0f));
    TEST_ASSERT(Near(sample.nearClip, 8.0f));
    return true;
}

bool TestMdxParsesRibbonColorAndTextureSlotTimelines() {
    std::vector<uint8_t> data;
    PushMdxTag(data, 'M', 'D', 'L', 'X');
    PushMdxTag(data, 'R', 'I', 'B', 'B');
    PushLE32(data, 232);

    PushLE32(data, 232); // ribbon emitter byte length, including this size field.
    PushLE32(data, 96);  // generic object byte length, including this size field.
    PushFixedString(data, "RibbonTrail", 80);
    PushLE32(data, 3);           // object id
    PushLE32(data, 0xffffffffu); // parent id
    PushLE32(data, 0x4000);      // ribbon emitter flag

    PushLE32(data, 0x3fc00000u); // height above = 1.5
    PushLE32(data, 0x3f000000u); // height below = 0.5
    PushLE32(data, 0x3f800000u); // alpha = 1.0
    PushLE32(data, 0x3e800000u); // color b/r component = 0.25
    PushLE32(data, 0x3f000000u); // color = 0.5
    PushLE32(data, 0x3f400000u); // color = 0.75
    PushLE32(data, 0x40000000u); // life span = 2.0
    PushLE32(data, 0);           // texture slot
    PushLE32(data, 30);          // emission rate
    PushLE32(data, 1);           // rows
    PushLE32(data, 2);           // columns
    PushLE32(data, 7);           // material id
    PushLE32(data, 0x40600000u); // gravity = 3.5

    PushMdxTag(data, 'K', 'R', 'C', 'O');
    PushLE32(data, 2);
    PushLE32(data, 1);           // linear
    PushLE32(data, 0xffffffffu);
    PushLE32(data, 0);
    PushLE32(data, 0x00000000u);
    PushLE32(data, 0x3f000000u);
    PushLE32(data, 0x3f800000u);
    PushLE32(data, 100);
    PushLE32(data, 0x3f800000u);
    PushLE32(data, 0x3f000000u);
    PushLE32(data, 0x00000000u);

    PushMdxTag(data, 'K', 'R', 'T', 'X');
    PushLE32(data, 2);
    PushLE32(data, 0);           // none
    PushLE32(data, 0xffffffffu);
    PushLE32(data, 0);
    PushLE32(data, 0);
    PushLE32(data, 100);
    PushLE32(data, 2);

    MdxModel model;
    TEST_ASSERT(MdxLoad(data.data(), data.size(), model));
    TEST_ASSERT(model.ribbonEmitters.size() == 1);
    const MdxRibbonEmitter& ribbon = model.ribbonEmitters[0];
    TEST_ASSERT(ribbon.name == "RibbonTrail");
    TEST_ASSERT(ribbon.objectId == 3);
    TEST_ASSERT(Near(ribbon.heightAbove, 1.5f));
    TEST_ASSERT(Near(ribbon.heightBelow, 0.5f));
    TEST_ASSERT(Near(ribbon.gravity, 3.5f));
    TEST_ASSERT(ribbon.timelines.size() == 2);

    const MdxTimeline& color = ribbon.timelines[0];
    TEST_ASSERT(color.tag == TestMdxTag('K', 'R', 'C', 'O'));
    TEST_ASSERT(color.frames.size() == 2);
    TEST_ASSERT(color.values.size() == 6);
    TEST_ASSERT(Near(color.values[0], 0.0f));
    TEST_ASSERT(Near(color.values[1], 0.5f));
    TEST_ASSERT(Near(color.values[2], 1.0f));
    TEST_ASSERT(Near(color.values[3], 1.0f));
    TEST_ASSERT(Near(color.values[4], 0.5f));
    TEST_ASSERT(Near(color.values[5], 0.0f));

    const MdxTimeline& textureSlot = ribbon.timelines[1];
    TEST_ASSERT(textureSlot.tag == TestMdxTag('K', 'R', 'T', 'X'));
    TEST_ASSERT(textureSlot.frames.size() == 2);
    TEST_ASSERT(textureSlot.values.size() == 2);
    TEST_ASSERT(Near(textureSlot.values[0], 0.0f));
    TEST_ASSERT(Near(textureSlot.values[1], 2.0f));
    return true;
}

bool TestMdxSamplesParticleEmitter2RuntimeTimelines() {
    MdxModel model;
    model.globalSequences = {1000};

    MdxParticleEmitter2 emitter;
    emitter.speed = 10.0f;
    emitter.variation = 0.25f;
    emitter.latitude = 15.0f;
    emitter.gravity = 3.0f;
    emitter.emissionRate = 1.0f;
    emitter.length = 20.0f;
    emitter.width = 8.0f;

    MdxTimeline emission;
    emission.tag = TestMdxTag('K', 'P', '2', 'E');
    emission.interpolationType = MdxInterpolation::Linear;
    emission.globalSequenceId = 0;
    emission.frames = {0, 500};
    emission.values = {2.0f, 8.0f};
    emitter.timelines.push_back(emission);

    MdxTimeline visibility;
    visibility.tag = TestMdxTag('K', 'P', '2', 'V');
    visibility.interpolationType = MdxInterpolation::Linear;
    visibility.frames = {0, 100};
    visibility.values = {0.0f, 1.0f};
    emitter.timelines.push_back(visibility);

    MdxTimeline width;
    width.tag = TestMdxTag('K', 'P', '2', 'W');
    width.interpolationType = MdxInterpolation::Linear;
    width.frames = {0, 200};
    width.values = {8.0f, 16.0f};
    emitter.timelines.push_back(width);

    MdxParticleEmitter2Sample sample;
    TEST_ASSERT(MdxSampleParticleEmitter2(emitter, &model, 50, 750.0f, sample));
    TEST_ASSERT(Near(sample.emissionRate, 5.0f));
    TEST_ASSERT(sample.emissionRateKey == 1);
    TEST_ASSERT(Near(sample.visibility, 0.0f));
    TEST_ASSERT(Near(sample.width, 10.0f));

    TEST_ASSERT(MdxSampleParticleEmitter2(emitter, &model, 150, 250.0f, sample));
    TEST_ASSERT(Near(sample.emissionRate, 5.0f));
    TEST_ASSERT(sample.emissionRateKey == 0);
    TEST_ASSERT(Near(sample.visibility, 1.0f));
    TEST_ASSERT(Near(sample.width, 14.0f));
    return true;
}

bool TestMdxSamplesParticleEmitter2TimelinesOnlyInsideCurrentSequence() {
    MdxModel model;

    MdxSequence stand;
    stand.name = "Stand";
    stand.startFrame = 0;
    stand.endFrame = 100;
    model.sequences.push_back(stand);

    MdxSequence attack;
    attack.name = "Attack";
    attack.startFrame = 1000;
    attack.endFrame = 1100;
    model.sequences.push_back(attack);

    MdxParticleEmitter2 emitter;
    emitter.emissionRate = 1.0f;
    emitter.width = 8.0f;

    MdxTimeline attackEmission;
    attackEmission.tag = TestMdxTag('K', 'P', '2', 'E');
    attackEmission.interpolationType = MdxInterpolation::Linear;
    attackEmission.frames = {1000, 1100};
    attackEmission.values = {10.0f, 20.0f};
    emitter.timelines.push_back(attackEmission);

    MdxTimeline attackWidth;
    attackWidth.tag = TestMdxTag('K', 'P', '2', 'W');
    attackWidth.interpolationType = MdxInterpolation::Linear;
    attackWidth.frames = {1000, 1100};
    attackWidth.values = {16.0f, 32.0f};
    emitter.timelines.push_back(attackWidth);

    MdxParticleEmitter2Sample sample;
    TEST_ASSERT(!MdxSampleParticleEmitter2(emitter, &model, 50, 50.0f, sample, 0));
    TEST_ASSERT(Near(sample.emissionRate, 1.0f));
    TEST_ASSERT(Near(sample.width, 8.0f));
    TEST_ASSERT(sample.emissionRateKey == -1);

    TEST_ASSERT(MdxSampleParticleEmitter2(emitter, &model, 1050, 1050.0f, sample, 1));
    TEST_ASSERT(Near(sample.emissionRate, 15.0f));
    TEST_ASSERT(Near(sample.width, 24.0f));
    TEST_ASSERT(sample.emissionRateKey == 0);
    return true;
}

bool TestMdxSamplesParticleEmitter1TimelinesOnlyInsideCurrentSequence() {
    MdxModel model;

    MdxSequence stand;
    stand.name = "Stand";
    stand.startFrame = 0;
    stand.endFrame = 100;
    model.sequences.push_back(stand);

    MdxSequence attack;
    attack.name = "Attack";
    attack.startFrame = 1000;
    attack.endFrame = 1100;
    model.sequences.push_back(attack);

    MdxParticleEmitter1 emitter;
    emitter.emissionRate = 1.0f;
    emitter.gravity = 2.0f;
    emitter.longitude = 3.0f;
    emitter.latitude = 4.0f;
    emitter.lifeSpan = 5.0f;
    emitter.speed = 6.0f;

    MdxTimeline attackEmission;
    attackEmission.tag = TestMdxTag('K', 'P', 'E', 'E');
    attackEmission.interpolationType = MdxInterpolation::Linear;
    attackEmission.frames = {1000, 1100};
    attackEmission.values = {10.0f, 20.0f};
    emitter.timelines.push_back(attackEmission);

    MdxTimeline attackSpeed;
    attackSpeed.tag = TestMdxTag('K', 'P', 'E', 'S');
    attackSpeed.interpolationType = MdxInterpolation::Linear;
    attackSpeed.frames = {1000, 1100};
    attackSpeed.values = {50.0f, 70.0f};
    emitter.timelines.push_back(attackSpeed);

    MdxTimeline attackVisibility;
    attackVisibility.tag = TestMdxTag('K', 'P', 'E', 'V');
    attackVisibility.interpolationType = MdxInterpolation::Linear;
    attackVisibility.frames = {1000, 1100};
    attackVisibility.values = {0.0f, 1.0f};
    emitter.timelines.push_back(attackVisibility);

    MdxParticleEmitter1Sample sample;
    TEST_ASSERT(!MdxSampleParticleEmitter1(emitter, &model, 50, 50.0f, sample, 0));
    TEST_ASSERT(Near(sample.emissionRate, 1.0f));
    TEST_ASSERT(Near(sample.speed, 6.0f));
    TEST_ASSERT(Near(sample.visibility, 1.0f));

    TEST_ASSERT(MdxSampleParticleEmitter1(emitter, &model, 1050, 1050.0f, sample, 1));
    TEST_ASSERT(Near(sample.emissionRate, 15.0f));
    TEST_ASSERT(Near(sample.speed, 60.0f));
    TEST_ASSERT(Near(sample.visibility, 0.0f));
    return true;
}

bool TestMdxCollectsParticleEmitter1ModelSpawns() {
    MdxModel model;

    MdxParticleEmitter1 emitter;
    emitter.objectId = 0;
    emitter.internalModelPath = "Abilities\\Spells\\Other\\TalkToMe\\TalkToMe.mdx";
    emitter.emissionRate = 2.0f;
    emitter.gravity = 2.0f;
    emitter.lifeSpan = 3.0f;
    emitter.speed = 10.0f;
    model.particleEmitters1.push_back(emitter);

    std::vector<MdxParticleEmitter1RuntimeState> states;
    std::vector<MdxParticleEmitter1Spawn> spawns;
    MdxResetParticleEmitter1RuntimeStates(model, states);

    TEST_ASSERT(MdxCollectParticleEmitter1Spawns(model, nullptr, 0, 0.0f, 0.25f, states, spawns) == 0);
    TEST_ASSERT(spawns.empty());

    TEST_ASSERT(MdxCollectParticleEmitter1Spawns(model, nullptr, 0, 250.0f, 0.25f, states, spawns) == 1);
    TEST_ASSERT(spawns.size() == 1);
    TEST_ASSERT(spawns[0].objectId == 0);
    TEST_ASSERT(spawns[0].modelPath == emitter.internalModelPath);
    TEST_ASSERT(Near(spawns[0].lifeSpan, 3.0f));
    TEST_ASSERT(Near(spawns[0].localPosition.x, 0.0f));
    TEST_ASSERT(Near(spawns[0].localPosition.y, 0.0f));
    TEST_ASSERT(Near(spawns[0].localPosition.z, 0.0f));
    TEST_ASSERT(Near(spawns[0].localTransform.m[12], 0.0f));
    TEST_ASSERT(Near(spawns[0].localTransform.m[13], 0.0f));
    TEST_ASSERT(Near(spawns[0].localTransform.m[14], 0.0f));
    TEST_ASSERT(Near(spawns[0].velocity.x, 0.0f));
    TEST_ASSERT(Near(spawns[0].velocity.y, 10.0f));
    TEST_ASSERT(Near(spawns[0].velocity.z, 0.0f));
    TEST_ASSERT(Near(spawns[0].gravity, 2.0f));
    return true;
}

bool TestMdxAdvancesParticleEmitter1SpawnWithGravity() {
    Mat4 transform = Mat4::Identity();
    Vec3 velocity = {2.0f, 10.0f, -4.0f};

    MdxAdvanceParticleEmitter1SpawnTransform(transform, velocity, 6.0f, 0.5f);

    TEST_ASSERT(Near(velocity.x, 2.0f));
    TEST_ASSERT(Near(velocity.y, 7.0f));
    TEST_ASSERT(Near(velocity.z, -4.0f));
    TEST_ASSERT(Near(transform.m[12], 1.0f));
    TEST_ASSERT(Near(transform.m[13], 3.5f));
    TEST_ASSERT(Near(transform.m[14], -2.0f));
    return true;
}

bool TestMdxCollectsParticleEmitter1OnlyWhenVisibleInCurrentSequence() {
    MdxModel model;
    model.pivotPoints.push_back({2.0f, 3.0f, 4.0f});

    MdxSequence stand;
    stand.name = "Stand";
    stand.startFrame = 0;
    stand.endFrame = 100;
    model.sequences.push_back(stand);

    MdxSequence attack;
    attack.name = "Attack";
    attack.startFrame = 1000;
    attack.endFrame = 1100;
    model.sequences.push_back(attack);

    MdxParticleEmitter1 emitter;
    emitter.objectId = 0;
    emitter.parentId = -1;
    emitter.internalModelPath = "Abilities\\Weapons\\FireBallMissile\\FireBallMissile.mdx";
    emitter.emissionRate = 0.0f;
    emitter.gravity = 3.0f;
    emitter.lifeSpan = 1.0f;
    emitter.speed = 8.0f;

    MdxTimeline attackEmission;
    attackEmission.tag = TestMdxTag('K', 'P', 'E', 'E');
    attackEmission.interpolationType = MdxInterpolation::Linear;
    attackEmission.frames = {1000, 1100};
    attackEmission.values = {4.0f, 4.0f};
    emitter.timelines.push_back(attackEmission);

    MdxTimeline attackVisibility;
    attackVisibility.tag = TestMdxTag('K', 'P', 'E', 'V');
    attackVisibility.interpolationType = MdxInterpolation::None;
    attackVisibility.frames = {1000, 1050};
    attackVisibility.values = {0.0f, 1.0f};
    emitter.timelines.push_back(attackVisibility);

    MdxTimeline attackScale;
    attackScale.tag = TestMdxTag('K', 'G', 'S', 'C');
    attackScale.interpolationType = MdxInterpolation::None;
    attackScale.frames = {1000};
    attackScale.values = {2.0f, 3.0f, 4.0f};
    emitter.timelines.push_back(attackScale);

    model.particleEmitters1.push_back(emitter);

    MdxAnimator animator;
    animator.Init(model);
    std::vector<MdxParticleEmitter1RuntimeState> states;
    std::vector<MdxParticleEmitter1Spawn> spawns;
    MdxResetParticleEmitter1RuntimeStates(model, states);

    TEST_ASSERT(animator.SetSequence(0, true, 1.0f));
    TEST_ASSERT(!MdxCollectParticleEmitter1Spawns(model, &animator, 50, 50.0f, 1.0f, states, spawns));
    TEST_ASSERT(spawns.empty());

    TEST_ASSERT(animator.SetSequence(1, true, 1.0f));
    TEST_ASSERT(animator.SetFrameByRatio(0.25f));
    TEST_ASSERT(!MdxCollectParticleEmitter1Spawns(model, &animator, animator.GetCurrentFrame(),
                                                 1025.0f, 1.0f, states, spawns));
    TEST_ASSERT(spawns.empty());

    TEST_ASSERT(animator.SetFrameByRatio(0.75f));
    TEST_ASSERT(MdxCollectParticleEmitter1Spawns(model, &animator, animator.GetCurrentFrame(),
                                                1075.0f, 0.25f, states, spawns) == 1);
    TEST_ASSERT(spawns.size() == 1);
    TEST_ASSERT(spawns[0].modelPath == emitter.internalModelPath);
    TEST_ASSERT(Near(spawns[0].lifeSpan, 1.0f));
    TEST_ASSERT(Near(spawns[0].localPosition.x, 2.0f));
    TEST_ASSERT(Near(spawns[0].localPosition.y, 4.0f));
    TEST_ASSERT(Near(spawns[0].localPosition.z, -3.0f));
    TEST_ASSERT(Near(spawns[0].localTransform.m[12], 2.0f));
    TEST_ASSERT(Near(spawns[0].localTransform.m[13], 4.0f));
    TEST_ASSERT(Near(spawns[0].localTransform.m[14], -3.0f));
    TEST_ASSERT(Near(ColumnLength(spawns[0].localTransform, 0), 2.0f));
    TEST_ASSERT(Near(ColumnLength(spawns[0].localTransform, 1), 4.0f));
    TEST_ASSERT(Near(ColumnLength(spawns[0].localTransform, 2), 3.0f));
    TEST_ASSERT(Near(spawns[0].velocity.x, 0.0f));
    TEST_ASSERT(Near(spawns[0].velocity.y, 32.0f));
    TEST_ASSERT(Near(spawns[0].velocity.z, 0.0f));
    TEST_ASSERT(Near(spawns[0].gravity, 12.0f));
    return true;
}

bool TestMdxAnimatorSamplesParticleEmitter2Visibility() {
    MdxModel model;
    model.pivotPoints.push_back({0.0f, 0.0f, 0.0f});

    MdxSequence sequence;
    sequence.name = "Stand";
    sequence.startFrame = 0;
    sequence.endFrame = 1000;
    model.sequences.push_back(sequence);

    MdxParticleEmitter2 emitter;
    emitter.name = "Dust";
    emitter.objectId = 0;
    emitter.parentId = -1;

    MdxTimeline visibility;
    visibility.tag = TestMdxTag('K', 'P', '2', 'V');
    visibility.interpolationType = MdxInterpolation::None;
    visibility.frames = {0, 500};
    visibility.values = {0.0f, 1.0f};
    emitter.timelines.push_back(visibility);
    model.particleEmitters2.push_back(emitter);

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.SetSequence(0, true, 1.0f));
    TEST_ASSERT(!animator.IsObjectVisible(0));

    TEST_ASSERT(animator.SetFrameByRatio(0.25f));
    TEST_ASSERT(!animator.IsObjectVisible(0));

    TEST_ASSERT(animator.SetFrameByRatio(0.75f));
    TEST_ASSERT(animator.IsObjectVisible(0));
    return true;
}

bool TestMdxParsesEventObjectKeyframes() {
    std::vector<uint8_t> data;
    PushMdxTag(data, 'M', 'D', 'L', 'X');
    PushMdxTag(data, 'E', 'V', 'T', 'S');
    PushLE32(data, 116); // generic object (96) + KEVT header (12) + two frames.

    PushLE32(data, 96); // generic object byte length, including this size field.
    PushFixedString(data, "SPL Footstep", 80);
    PushLE32(data, 7);            // object id
    PushLE32(data, 0xffffffffu);  // parent id
    PushLE32(data, 0x400);        // event object flags

    PushMdxTag(data, 'K', 'E', 'V', 'T');
    PushLE32(data, 2);            // keyframe count
    PushLE32(data, 0xffffffffu);  // global sequence id
    PushLE32(data, 123);
    PushLE32(data, 456);

    MdxModel model;
    TEST_ASSERT(MdxLoad(data.data(), data.size(), model));
    TEST_ASSERT(model.eventObjects.size() == 1);
    const MdxEventObject& eventObject = model.eventObjects[0];
    TEST_ASSERT(eventObject.name == "SPL Footstep");
    TEST_ASSERT(eventObject.objectId == 7);
    TEST_ASSERT(eventObject.parentId == -1);
    TEST_ASSERT(eventObject.type == "SPL");
    TEST_ASSERT(eventObject.id == "Footstep");
    TEST_ASSERT(eventObject.timelines.size() == 1);

    const MdxTimeline& eventTrack = eventObject.timelines[0];
    TEST_ASSERT(eventTrack.tag == TestMdxTag('K', 'E', 'V', 'T'));
    TEST_ASSERT(eventTrack.interpolationType == MdxInterpolation::None);
    TEST_ASSERT(eventTrack.globalSequenceId == -1);
    TEST_ASSERT(eventTrack.frames.size() == 2);
    TEST_ASSERT(eventTrack.frames[0] == 123);
    TEST_ASSERT(eventTrack.frames[1] == 456);
    TEST_ASSERT(eventTrack.values.empty());
    TEST_ASSERT(eventTrack.inTans.empty());
    TEST_ASSERT(eventTrack.outTans.empty());
    return true;
}

bool TestMdxParsesCollisionShapeAndAnimatorNode() {
    std::vector<uint8_t> data;
    PushMdxTag(data, 'M', 'D', 'L', 'X');
    PushMdxTag(data, 'C', 'L', 'I', 'D');
    PushLE32(data, 116); // generic object (96) + type (4) + sphere vertex (12) + radius (4).

    PushLE32(data, 96); // generic object byte length, including this size field.
    PushFixedString(data, "CollisionShape", 80);
    PushLE32(data, 7);            // object id
    PushLE32(data, 0xffffffffu);  // parent id
    PushLE32(data, 0x2000);       // collision shape flags

    PushLE32(data, 2);            // sphere
    PushLE32(data, 0x3f800000u);  // vertex x = 1.0
    PushLE32(data, 0x40000000u);  // vertex y = 2.0
    PushLE32(data, 0x40400000u);  // vertex z = 3.0
    PushLE32(data, 0x42800000u);  // radius = 64.0

    MdxModel model;
    TEST_ASSERT(MdxLoad(data.data(), data.size(), model));
    TEST_ASSERT(model.collisionShapes.size() == 1);
    const MdxCollisionShape& shape = model.collisionShapes[0];
    TEST_ASSERT(shape.name == "CollisionShape");
    TEST_ASSERT(shape.objectId == 7);
    TEST_ASSERT(shape.parentId == -1);
    TEST_ASSERT(shape.flags == 0x2000);
    TEST_ASSERT(shape.type == MdxCollisionShapeType::Sphere);
    TEST_ASSERT(Near(shape.vertices[0][0], 1.0f));
    TEST_ASSERT(Near(shape.vertices[0][1], 2.0f));
    TEST_ASSERT(Near(shape.vertices[0][2], 3.0f));
    TEST_ASSERT(Near(shape.boundsRadius, 64.0f));

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.GetBoneIndexForObjectId(7) == 7);
    TEST_ASSERT(animator.GetBoneCount() == 8);
    return true;
}

bool TestMdxParsesPopcornEmitterAndAnimatorNode() {
    std::vector<uint8_t> data;
    PushMdxTag(data, 'M', 'D', 'L', 'X');
    PushMdxTag(data, 'C', 'O', 'R', 'N');
    PushLE32(data, 684); // outer size (4) + generic object (96) + static fields (552) + KPPV timeline (32).

    PushLE32(data, 684); // ParticleEmitterPopcorn byte length, including this size field.
    PushLE32(data, 96);  // generic object byte length, including this size field.
    PushFixedString(data, "PopcornEmitter", 80);
    PushLE32(data, 9);            // object id
    PushLE32(data, 0xffffffffu);  // parent id
    PushLE32(data, 0x8000);       // emitter flags

    PushLE32(data, 0x3fc00000u);  // life span = 1.5
    PushLE32(data, 0x40000000u);  // emission rate = 2.0
    PushLE32(data, 0x40400000u);  // speed = 3.0
    PushLE32(data, 0x3e800000u);  // color r = 0.25
    PushLE32(data, 0x3f000000u);  // color g = 0.5
    PushLE32(data, 0x3f400000u);  // color b = 0.75
    PushLE32(data, 0x3f800000u);  // alpha = 1.0
    PushLE32(data, 31);           // replaceable id
    PushFixedString(data, "Particles\\Popcorn.mdl", 260);
    PushFixedString(data, "Stand", 260);

    PushMdxTag(data, 'K', 'P', 'P', 'V');
    PushLE32(data, 2);            // keyframe count
    PushLE32(data, 0);            // interpolation: none
    PushLE32(data, 0xffffffffu);  // global sequence id
    PushLE32(data, 0);
    PushLE32(data, 0x00000000u);  // invisible at frame 0
    PushLE32(data, 100);
    PushLE32(data, 0x3f800000u);  // visible at frame 100

    MdxModel model;
    TEST_ASSERT(MdxLoad(data.data(), data.size(), model));
    TEST_ASSERT(model.particleEmittersPopcorn.size() == 1);
    const MdxParticleEmitterPopcorn& emitter = model.particleEmittersPopcorn[0];
    TEST_ASSERT(emitter.name == "PopcornEmitter");
    TEST_ASSERT(emitter.objectId == 9);
    TEST_ASSERT(emitter.parentId == -1);
    TEST_ASSERT(emitter.flags == 0x8000);
    TEST_ASSERT(Near(emitter.lifeSpan, 1.5f));
    TEST_ASSERT(Near(emitter.emissionRate, 2.0f));
    TEST_ASSERT(Near(emitter.speed, 3.0f));
    TEST_ASSERT(Near(emitter.color[0], 0.25f));
    TEST_ASSERT(Near(emitter.color[1], 0.5f));
    TEST_ASSERT(Near(emitter.color[2], 0.75f));
    TEST_ASSERT(Near(emitter.alpha, 1.0f));
    TEST_ASSERT(emitter.replaceableId == 31);
    TEST_ASSERT(emitter.path == "Particles\\Popcorn.mdl");
    TEST_ASSERT(emitter.animationVisibilityGuide == "Stand");
    TEST_ASSERT(emitter.timelines.size() == 1);
    TEST_ASSERT(emitter.timelines[0].tag == TestMdxTag('K', 'P', 'P', 'V'));
    TEST_ASSERT(emitter.timelines[0].frames.size() == 2);
    TEST_ASSERT(Near(emitter.timelines[0].values[0], 0.0f));
    TEST_ASSERT(Near(emitter.timelines[0].values[1], 1.0f));

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.GetBoneIndexForObjectId(9) == 9);
    TEST_ASSERT(animator.GetBoneCount() == 10);
    TEST_ASSERT(!animator.IsObjectVisible(9));
    return true;
}

bool TestMdxCollisionSphereRayUsesInstanceTransform() {
    MdxModel model;
    model.pivotPoints.push_back({0.0f, 0.0f, 0.0f});

    MdxCollisionShape shape;
    shape.name = "Sphere";
    shape.objectId = 0;
    shape.parentId = -1;
    shape.type = MdxCollisionShapeType::Sphere;
    shape.vertices[0][0] = 0.0f;
    shape.vertices[0][1] = 0.0f;
    shape.vertices[0][2] = 50.0f;
    shape.boundsRadius = 20.0f;
    model.collisionShapes.push_back(shape);

    MdxAnimator animator;
    animator.Init(model);

    Mat4 instance = Mat4::Identity();
    instance.m[0] = 2.0f;
    instance.m[5] = 2.0f;
    instance.m[10] = 2.0f;
    instance.m[12] = 10.0f;

    picking::PickRay ray;
    ray.origin = {10.0f, 100.0f, -100.0f};
    ray.direction = {0.0f, 0.0f, 1.0f};

    float hitT = 0.0f;
    TEST_ASSERT(MdxIntersectCollisionShapes(model, animator, ray, instance, hitT));
    TEST_ASSERT(Near(hitT, 60.0f));
    return true;
}

bool TestMdxCollisionBoxRayUsesCollisionExtents() {
    MdxModel model;
    model.pivotPoints.push_back({0.0f, 0.0f, 0.0f});

    MdxCollisionShape shape;
    shape.name = "Box";
    shape.objectId = 0;
    shape.parentId = -1;
    shape.type = MdxCollisionShapeType::Box;
    shape.vertices[0][0] = -10.0f;
    shape.vertices[0][1] = -10.0f;
    shape.vertices[0][2] = 0.0f;
    shape.vertices[1][0] = 10.0f;
    shape.vertices[1][1] = 10.0f;
    shape.vertices[1][2] = 20.0f;
    model.collisionShapes.push_back(shape);

    MdxAnimator animator;
    animator.Init(model);

    picking::PickRay ray;
    ray.origin = {0.0f, 10.0f, -100.0f};
    ray.direction = {0.0f, 0.0f, 1.0f};

    float hitT = 0.0f;
    TEST_ASSERT(MdxIntersectCollisionShapes(model, animator, ray, Mat4::Identity(), hitT));
    TEST_ASSERT(Near(hitT, 90.0f));

    ray.origin = {30.0f, 10.0f, -100.0f};
    TEST_ASSERT(!MdxIntersectCollisionShapes(model, animator, ray, Mat4::Identity(), hitT));
    return true;
}

bool TestMdxAnimatorSamplesGlobalSequenceBones() {
    MdxModel model;
    model.globalSequences = {1000};
    model.pivotPoints.push_back({0.0f, 0.0f, 0.0f});

    MdxSequence sequence;
    sequence.name = "Stand";
    sequence.startFrame = 100;
    sequence.endFrame = 200;
    model.sequences.push_back(sequence);

    MdxNode bone;
    bone.name = "GlobalMover";
    bone.objectId = 0;
    bone.parentId = -1;

    MdxTimeline translation;
    translation.tag = TestMdxTag('K', 'G', 'T', 'R');
    translation.interpolationType = MdxInterpolation::Linear;
    translation.globalSequenceId = 0;
    translation.frames = {0, 1000};
    translation.values = {
        0.0f, 0.0f, 0.0f,
        100.0f, 0.0f, 0.0f
    };
    bone.timelines.push_back(translation);
    model.bones.push_back(bone);

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.SetSequence(0, true, 1.0f));
    TEST_ASSERT(animator.GetBoneMatrices().size() == 1);
    TEST_ASSERT(Near(animator.GetBoneMatrices()[0].m[12], 0.0f));

    animator.Update(0.25f);
    TEST_ASSERT(Near(animator.GetBoneMatrices()[0].m[12], 25.0f));

    animator.Update(1.0f);
    TEST_ASSERT(Near(animator.GetBoneMatrices()[0].m[12], 25.0f));
    return true;
}

bool TestMdxAnimatorLoopRestartMatchesReferenceEndFrame() {
    MdxModel model;
    model.pivotPoints.push_back({0.0f, 0.0f, 0.0f});

    MdxSequence sequence;
    sequence.name = "Stand";
    sequence.startFrame = 100;
    sequence.endFrame = 200;
    model.sequences.push_back(sequence);

    MdxNode bone;
    bone.name = "LoopingBone";
    bone.objectId = 0;
    bone.parentId = -1;

    MdxTimeline translation;
    translation.tag = TestMdxTag('K', 'G', 'T', 'R');
    translation.interpolationType = MdxInterpolation::Linear;
    translation.frames = {100, 199};
    translation.values = {
        5.0f, 0.0f, 0.0f,
        99.0f, 0.0f, 0.0f
    };
    bone.timelines.push_back(translation);
    model.bones.push_back(bone);

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.SetSequence(0, true, 1.0f));
    TEST_ASSERT(animator.GetCurrentFrame() == 100);
    TEST_ASSERT(Near(animator.GetBoneMatrices()[0].m[12], 5.0f));

    animator.Update(0.120f);
    TEST_ASSERT(animator.GetCurrentFrame() == 100);
    TEST_ASSERT(Near(animator.GetBoneMatrices()[0].m[12], 5.0f));
    TEST_ASSERT(!animator.IsSequenceFinished());
    return true;
}

bool TestMdxAnimatorIgnoresSequenceLocalBoneKeysOutsideCurrentSequence() {
    MdxModel model;
    model.pivotPoints.push_back({0.0f, 0.0f, 0.0f});

    MdxSequence stand;
    stand.name = "Stand";
    stand.startFrame = 0;
    stand.endFrame = 100;
    model.sequences.push_back(stand);

    MdxSequence attack;
    attack.name = "Attack";
    attack.startFrame = 1000;
    attack.endFrame = 1100;
    model.sequences.push_back(attack);

    MdxNode bone;
    bone.name = "Weapon";
    bone.objectId = 0;
    bone.parentId = -1;

    MdxTimeline attackOnlyTranslation;
    attackOnlyTranslation.tag = TestMdxTag('K', 'G', 'T', 'R');
    attackOnlyTranslation.interpolationType = MdxInterpolation::Linear;
    attackOnlyTranslation.frames = {1000, 1100};
    attackOnlyTranslation.values = {
        50.0f, 0.0f, 0.0f,
        70.0f, 0.0f, 0.0f
    };
    bone.timelines.push_back(attackOnlyTranslation);
    model.bones.push_back(bone);

    MdxAnimator animator;
    animator.Init(model);

    TEST_ASSERT(animator.SetSequence(0, true, 1.0f));
    TEST_ASSERT(animator.GetBoneMatrices().size() == 1);
    TEST_ASSERT(Near(animator.GetBoneMatrices()[0].m[12], 0.0f));
    animator.Update(0.050f);
    TEST_ASSERT(Near(animator.GetBoneMatrices()[0].m[12], 0.0f));

    TEST_ASSERT(animator.SetSequence(1, true, 1.0f));
    TEST_ASSERT(Near(animator.GetBoneMatrices()[0].m[12], 50.0f));
    animator.Update(0.050f);
    TEST_ASSERT(Near(animator.GetBoneMatrices()[0].m[12], 60.0f));

    TEST_ASSERT(animator.SetSequence(0, true, 1.0f));
    TEST_ASSERT(Near(animator.GetBoneMatrices()[0].m[12], 0.0f));
    return true;
}

bool TestMdxAnimatorSamplesAttachmentVisibilityAsStep() {
    MdxModel model;
    model.pivotPoints.push_back({0.0f, 0.0f, 0.0f});

    MdxSequence sequence;
    sequence.name = "Stand";
    sequence.startFrame = 0;
    sequence.endFrame = 1000;
    model.sequences.push_back(sequence);

    MdxAttachment attachment;
    attachment.name = "Weapon";
    attachment.objectId = 0;
    attachment.parentId = -1;

    MdxTimeline visibility;
    visibility.tag = TestMdxTag('K', 'A', 'T', 'V');
    visibility.interpolationType = MdxInterpolation::Linear;
    visibility.frames = {0, 500};
    visibility.values = {0.0f, 1.0f};
    attachment.timelines.push_back(visibility);
    model.attachments.push_back(attachment);

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.SetSequence(0, true, 1.0f));
    TEST_ASSERT(!animator.IsObjectVisible(0));

    TEST_ASSERT(animator.SetFrameByRatio(0.25f));
    TEST_ASSERT(!animator.IsObjectVisible(0));

    TEST_ASSERT(animator.SetFrameByRatio(0.75f));
    TEST_ASSERT(animator.IsObjectVisible(0));
    return true;
}

bool TestMdxAnimatorSamplesLightVisibilityAsStep() {
    MdxModel model;
    model.pivotPoints.push_back({0.0f, 0.0f, 0.0f});

    MdxSequence sequence;
    sequence.name = "Stand";
    sequence.startFrame = 0;
    sequence.endFrame = 1000;
    model.sequences.push_back(sequence);

    MdxLight light;
    light.name = "Glow";
    light.objectId = 0;
    light.parentId = -1;

    MdxTimeline visibility;
    visibility.tag = TestMdxTag('K', 'L', 'A', 'V');
    visibility.interpolationType = MdxInterpolation::Linear;
    visibility.frames = {0, 500};
    visibility.values = {0.0f, 1.0f};
    light.timelines.push_back(visibility);
    model.lights.push_back(light);

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.SetSequence(0, true, 1.0f));
    TEST_ASSERT(!animator.IsObjectVisible(0));

    TEST_ASSERT(animator.SetFrameByRatio(0.25f));
    TEST_ASSERT(!animator.IsObjectVisible(0));

    TEST_ASSERT(animator.SetFrameByRatio(0.75f));
    TEST_ASSERT(animator.IsObjectVisible(0));
    return true;
}

bool TestMdxAnimatorUsesBoneGeosetAnimationVisibility() {
    MdxModel model;
    model.pivotPoints.push_back({0.0f, 0.0f, 0.0f});

    MdxSequence sequence;
    sequence.name = "Stand";
    sequence.startFrame = 0;
    sequence.endFrame = 1000;
    model.sequences.push_back(sequence);

    MdxNode bone;
    bone.name = "DissolveBone";
    bone.objectId = 0;
    bone.parentId = -1;
    bone.geosetId = 0;
    bone.geosetAnimationId = 0;
    model.bones.push_back(bone);

    MdxGeosetAnim geosetAnim;
    geosetAnim.geosetId = 0;
    geosetAnim.alpha = 1.0f;

    MdxTimeline alpha;
    alpha.tag = TestMdxTag('K', 'G', 'A', 'O');
    alpha.interpolationType = MdxInterpolation::None;
    alpha.frames = {0, 500};
    alpha.values = {0.0f, 1.0f};
    geosetAnim.timelines.push_back(alpha);
    model.geosetAnims.push_back(geosetAnim);

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.SetSequence(0, true, 1.0f));
    TEST_ASSERT(!animator.IsObjectVisible(0));

    TEST_ASSERT(animator.SetFrameByRatio(0.25f));
    TEST_ASSERT(!animator.IsObjectVisible(0));

    TEST_ASSERT(animator.SetFrameByRatio(0.75f));
    TEST_ASSERT(animator.IsObjectVisible(0));
    return true;
}

bool TestMdxAnimatorTriggersEventKeyframes() {
    MdxModel model;

    MdxSequence sequence;
    sequence.name = "Spell";
    sequence.startFrame = 100;
    sequence.endFrame = 300;
    model.sequences.push_back(sequence);

    MdxEventObject eventObject;
    eventObject.name = "SPL Footstep";
    eventObject.type = "SPL";
    eventObject.id = "Footstep";
    eventObject.objectId = 4;
    eventObject.parentId = -1;

    MdxTimeline eventTrack;
    eventTrack.tag = TestMdxTag('K', 'E', 'V', 'T');
    eventTrack.interpolationType = MdxInterpolation::None;
    eventTrack.globalSequenceId = -1;
    eventTrack.frames = {150, 250};
    eventObject.timelines.push_back(eventTrack);
    model.eventObjects.push_back(eventObject);

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.SetSequence(0, true, 1.0f));
    TEST_ASSERT(animator.GetTriggeredEvents().empty());

    animator.Update(0.049f);
    TEST_ASSERT(animator.GetTriggeredEvents().empty());

    animator.Update(0.001f);
    TEST_ASSERT(animator.GetTriggeredEvents().size() == 1);
    TEST_ASSERT(animator.GetTriggeredEvents()[0].objectId == 4);
    TEST_ASSERT(animator.GetTriggeredEvents()[0].type == "SPL");
    TEST_ASSERT(animator.GetTriggeredEvents()[0].id == "Footstep");
    TEST_ASSERT(animator.GetTriggeredEvents()[0].keyframeIndex == 0);
    TEST_ASSERT(animator.GetTriggeredEvents()[0].frame == 150);

    animator.Update(0.050f);
    TEST_ASSERT(animator.GetTriggeredEvents().empty());

    animator.Update(0.050f);
    TEST_ASSERT(animator.GetTriggeredEvents().size() == 1);
    TEST_ASSERT(animator.GetTriggeredEvents()[0].keyframeIndex == 1);
    TEST_ASSERT(animator.GetTriggeredEvents()[0].frame == 250);
    return true;
}

bool TestMdxAnimatorResetsEventKeyframesOnLoop() {
    MdxModel model;

    MdxSequence sequence;
    sequence.name = "Stand";
    sequence.startFrame = 0;
    sequence.endFrame = 100;
    model.sequences.push_back(sequence);

    MdxEventObject eventObject;
    eventObject.name = "SND Loop";
    eventObject.type = "SND";
    eventObject.id = "Loop";
    eventObject.objectId = 8;
    eventObject.parentId = -1;

    MdxTimeline eventTrack;
    eventTrack.tag = TestMdxTag('K', 'E', 'V', 'T');
    eventTrack.frames = {20, 80};
    eventObject.timelines.push_back(eventTrack);
    model.eventObjects.push_back(eventObject);

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.SetSequence(0, true, 1.0f));

    animator.Update(0.080f);
    TEST_ASSERT(animator.GetTriggeredEvents().size() == 1);
    TEST_ASSERT(animator.GetTriggeredEvents()[0].keyframeIndex == 1);

    animator.Update(0.040f);
    TEST_ASSERT(animator.GetCurrentFrame() == 0);
    TEST_ASSERT(animator.GetTriggeredEvents().empty());

    animator.Update(0.020f);
    TEST_ASSERT(animator.GetCurrentFrame() == 20);
    TEST_ASSERT(animator.GetTriggeredEvents().size() == 1);
    TEST_ASSERT(animator.GetTriggeredEvents()[0].keyframeIndex == 0);
    TEST_ASSERT(animator.GetTriggeredEvents()[0].frame == 20);
    return true;
}

bool TestMdxAnimatorTriggersGlobalSequenceEvents() {
    MdxModel model;
    model.globalSequences = {100};

    MdxSequence sequence;
    sequence.name = "Stand";
    sequence.startFrame = 200;
    sequence.endFrame = 400;
    model.sequences.push_back(sequence);

    MdxEventObject eventObject;
    eventObject.name = "SPN Spark";
    eventObject.type = "SPN";
    eventObject.id = "Spark";
    eventObject.objectId = 12;
    eventObject.parentId = -1;

    MdxTimeline eventTrack;
    eventTrack.tag = TestMdxTag('K', 'E', 'V', 'T');
    eventTrack.globalSequenceId = 0;
    eventTrack.frames = {50};
    eventObject.timelines.push_back(eventTrack);
    model.eventObjects.push_back(eventObject);

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.SetSequence(0, true, 1.0f));

    animator.Update(0.049f);
    TEST_ASSERT(animator.GetTriggeredEvents().empty());

    animator.Update(0.001f);
    TEST_ASSERT(animator.GetTriggeredEvents().size() == 1);
    TEST_ASSERT(animator.GetTriggeredEvents()[0].type == "SPN");
    TEST_ASSERT(animator.GetTriggeredEvents()[0].id == "Spark");
    TEST_ASSERT(animator.GetTriggeredEvents()[0].keyframeIndex == 0);
    TEST_ASSERT(animator.GetTriggeredEvents()[0].frame == 50);
    return true;
}

bool TestMdxAnimatorEventTriggersCarryNodeTransform() {
    MdxModel model;
    model.pivotPoints.push_back({0.0f, 0.0f, 0.0f});
    model.pivotPoints.push_back({0.0f, 0.0f, 0.0f});

    MdxSequence sequence;
    sequence.name = "Spell";
    sequence.startFrame = 0;
    sequence.endFrame = 100;
    model.sequences.push_back(sequence);

    MdxNode helper;
    helper.name = "EmitterParent";
    helper.objectId = 0;
    helper.parentId = -1;

    MdxTimeline parentMove;
    parentMove.tag = TestMdxTag('K', 'G', 'T', 'R');
    parentMove.interpolationType = MdxInterpolation::Linear;
    parentMove.frames = {0, 100};
    parentMove.values = {
        10.0f, 0.0f, 0.0f,
        20.0f, 0.0f, 0.0f
    };
    helper.timelines.push_back(parentMove);
    model.helpers.push_back(helper);

    MdxEventObject eventObject;
    eventObject.name = "SPN Spark";
    eventObject.type = "SPN";
    eventObject.id = "Spark";
    eventObject.objectId = 1;
    eventObject.parentId = 0;

    MdxTimeline eventMove;
    eventMove.tag = TestMdxTag('K', 'G', 'T', 'R');
    eventMove.interpolationType = MdxInterpolation::Linear;
    eventMove.frames = {0, 100};
    eventMove.values = {
        5.0f, 0.0f, 0.0f,
        5.0f, 0.0f, 0.0f
    };
    eventObject.timelines.push_back(eventMove);

    MdxTimeline eventRotate;
    eventRotate.tag = TestMdxTag('K', 'G', 'R', 'T');
    eventRotate.interpolationType = MdxInterpolation::None;
    eventRotate.frames = {0};
    constexpr float kPi = 3.14159265358979323846f;
    const float quarterTurnSin = std::sin(kPi * 0.25f);
    const float quarterTurnCos = std::cos(kPi * 0.25f);
    eventRotate.values = {
        0.0f, 0.0f, quarterTurnSin, quarterTurnCos
    };
    eventObject.timelines.push_back(eventRotate);

    MdxTimeline eventTrack;
    eventTrack.tag = TestMdxTag('K', 'E', 'V', 'T');
    eventTrack.frames = {50};
    eventObject.timelines.push_back(eventTrack);
    model.eventObjects.push_back(eventObject);

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.SetSequence(0, false, 1.0f));
    animator.Update(0.050f);
    TEST_ASSERT(animator.GetTriggeredEvents().size() == 1);
    const MdxEventTrigger& trigger = animator.GetTriggeredEvents()[0];
    TEST_ASSERT(trigger.hasLocalTransform);
    TEST_ASSERT(Near(trigger.localPosition.x, 20.0f));
    TEST_ASSERT(Near(trigger.localPosition.y, 0.0f));
    TEST_ASSERT(Near(trigger.localPosition.z, 0.0f));
    TEST_ASSERT(std::fabs(trigger.localTransform.m[2]) > 0.9f
        || std::fabs(trigger.localTransform.m[8]) > 0.9f);

    Vec3 queriedPosition;
    TEST_ASSERT(animator.GetObjectWorldPositionYUp(1, queriedPosition));
    TEST_ASSERT(Near(queriedPosition.x, trigger.localPosition.x));
    return true;
}

bool TestMdxAnimatorEventTriggerUsesPivotWorldLocation() {
    MdxModel model;
    model.pivotPoints.push_back({2.0f, 3.0f, 4.0f});

    MdxSequence sequence;
    sequence.name = "Spell";
    sequence.startFrame = 0;
    sequence.endFrame = 100;
    model.sequences.push_back(sequence);

    MdxEventObject eventObject;
    eventObject.name = "SPN Spark";
    eventObject.type = "SPN";
    eventObject.id = "Spark";
    eventObject.objectId = 0;
    eventObject.parentId = -1;

    MdxTimeline scale;
    scale.tag = TestMdxTag('K', 'G', 'S', 'C');
    scale.interpolationType = MdxInterpolation::None;
    scale.frames = {0};
    scale.values = {2.0f, 3.0f, 4.0f};
    eventObject.timelines.push_back(scale);

    MdxTimeline eventTrack;
    eventTrack.tag = TestMdxTag('K', 'E', 'V', 'T');
    eventTrack.frames = {50};
    eventObject.timelines.push_back(eventTrack);
    model.eventObjects.push_back(eventObject);

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.SetSequence(0, false, 1.0f));
    animator.Update(0.050f);

    TEST_ASSERT(animator.GetTriggeredEvents().size() == 1);
    const MdxEventTrigger& trigger = animator.GetTriggeredEvents()[0];
    TEST_ASSERT(trigger.hasLocalTransform);
    TEST_ASSERT(Near(trigger.localPosition.x, 2.0f));
    TEST_ASSERT(Near(trigger.localPosition.y, 4.0f));
    TEST_ASSERT(Near(trigger.localPosition.z, -3.0f));
    TEST_ASSERT(!Near(trigger.localTransform.m[12], trigger.localPosition.x));
    return true;
}

bool TestMdxEventSplatMeshUsesWorldTransform() {
    constexpr float kPi = 3.14159265358979323846f;
    const float c = std::cos(kPi * 0.25f);
    const float s = std::sin(kPi * 0.25f);

    Mat4 transform = Mat4::Identity();
    transform.m[0] = c;
    transform.m[2] = -s;
    transform.m[5] = 1.0f;
    transform.m[8] = s;
    transform.m[10] = c;
    transform.m[12] = 100.0f;
    transform.m[13] = 25.0f;
    transform.m[14] = 200.0f;

    const MdxEventSplatMesh mesh = BuildMdxEventSplatMeshFromTransform(transform, 10.0f, 8.0f);
    TEST_ASSERT(mesh.vertices.size() == 4);
    TEST_ASSERT(mesh.indices.size() == 6);
    TEST_ASSERT(Near(mesh.vertices[0].worldX, 100.0f + (c + s) * 10.0f));
    TEST_ASSERT(Near(mesh.vertices[0].worldY, 200.0f + (-s + c) * 10.0f));
    TEST_ASSERT(Near(mesh.vertices[0].zOffset, 8.0f));
    TEST_ASSERT(!Near(mesh.vertices[0].worldX, mesh.vertices[3].worldX));
    TEST_ASSERT(!Near(mesh.vertices[0].worldY, mesh.vertices[1].worldY));
    TEST_ASSERT(mesh.indices[0] == 0 && mesh.indices[5] == 3);
    return true;
}

bool TestMdxEventSplatMeshCancelsNodeScale() {
    const Mat4 transform = Mat4::Scale(2.0f, 1.0f, 3.0f);
    const MdxEventSplatMesh mesh = BuildMdxEventSplatMeshFromTransform(
        transform, 10.0f, 8.0f, Vec3(2.0f, 3.0f, 1.0f));

    TEST_ASSERT(mesh.vertices.size() == 4);
    TEST_ASSERT(mesh.indices.size() == 6);
    TEST_ASSERT(Near(mesh.vertices[0].worldX, 10.0f));
    TEST_ASSERT(Near(mesh.vertices[0].worldY, 10.0f));
    TEST_ASSERT(Near(mesh.vertices[1].worldX, -10.0f));
    TEST_ASSERT(Near(mesh.vertices[1].worldY, 10.0f));
    TEST_ASSERT(Near(mesh.vertices[2].worldX, -10.0f));
    TEST_ASSERT(Near(mesh.vertices[2].worldY, -10.0f));
    TEST_ASSERT(Near(mesh.vertices[3].worldX, 10.0f));
    TEST_ASSERT(Near(mesh.vertices[3].worldY, -10.0f));
    return true;
}

bool TestMdxEventSplatScaleCompensationMatchesEventType() {
    const Mat4 transform = Mat4::Scale(2.0f, 1.0f, 3.0f);
    const Vec3 nodeScale(2.0f, 3.0f, 1.0f);

    const MdxEventSplatMesh spl = BuildMdxEventSplatMeshFromTransform(
        transform, 10.0f, 8.0f, MdxEventSplatScaleCompensationForType("SPL", nodeScale));
    TEST_ASSERT(spl.vertices.size() == 4);
    TEST_ASSERT(Near(spl.vertices[0].worldX, 10.0f));
    TEST_ASSERT(Near(spl.vertices[0].worldY, 10.0f));

    const MdxEventSplatMesh fpt = BuildMdxEventSplatMeshFromTransform(
        transform, 10.0f, 8.0f, MdxEventSplatScaleCompensationForType("FPT", nodeScale));
    TEST_ASSERT(fpt.vertices.size() == 4);
    TEST_ASSERT(Near(fpt.vertices[0].worldX, 10.0f));
    TEST_ASSERT(Near(fpt.vertices[0].worldY, 10.0f));

    const MdxEventSplatMesh ubr = BuildMdxEventSplatMeshFromTransform(
        transform, 10.0f, 8.0f, MdxEventSplatScaleCompensationForType("UBR", nodeScale));
    TEST_ASSERT(ubr.vertices.size() == 4);
    TEST_ASSERT(Near(ubr.vertices[0].worldX, 20.0f));
    TEST_ASSERT(Near(ubr.vertices[0].worldY, 30.0f));
    return true;
}

static MdxModel MakeSequenceSelectionModel(std::initializer_list<const char*> names) {
    MdxModel model;
    uint32_t frame = 0;
    for (const char* name : names) {
        MdxSequence sequence;
        sequence.name = name;
        sequence.startFrame = frame;
        sequence.endFrame = frame + 1000;
        frame += 1000;
        model.sequences.push_back(std::move(sequence));
    }
    return model;
}

bool TestMdxSequenceSelectsExactSecondaryTags() {
    MdxModel model = MakeSequenceSelectionModel({
        "Stand",
        "Stand Ready",
        "Stand Work",
        "Stand Ready Work"
    });

    TEST_ASSERT(MdxSelectSequenceIndexByPrimaryAndTags(model, "stand", {"work"}) == 2);
    TEST_ASSERT(MdxSelectSequenceIndexByPrimaryAndTags(model, "stand", {"ready", "work"}) == 3);
    TEST_ASSERT(MdxSelectSequenceIndexByPrimaryAndTags(model, "stand", {"ready", "gold"}) == 1);
    return true;
}

bool TestMdxSequenceSelectsSpellSecondaryFallback() {
    MdxModel fallbackModel = MakeSequenceSelectionModel({
        "Stand",
        "Stand Spell",
        "Stand Channel"
    });

    TEST_ASSERT(MdxSelectSpellSequenceIndex(fallbackModel, {}) == 1);
    TEST_ASSERT(MdxSelectSpellSequenceIndex(fallbackModel, {"spell"}) == 1);

    MdxModel preferredModel = MakeSequenceSelectionModel({
        "Stand Spell",
        "Spell Channel",
        "Spell"
    });
    TEST_ASSERT(MdxSelectSpellSequenceIndex(preferredModel, {}) == 2);
    return true;
}

bool TestMdxSequenceDurationSeconds() {
    MdxModel model;
    MdxSequence sequence;
    sequence.startFrame = 250;
    sequence.endFrame = 1750;
    model.sequences.push_back(sequence);

    MdxSequence zeroLength;
    zeroLength.startFrame = 1000;
    zeroLength.endFrame = 1000;
    model.sequences.push_back(zeroLength);

    TEST_ASSERT(Near(MdxSequenceDurationSeconds(model, 0), 1.5f));
    TEST_ASSERT(Near(MdxSequenceDurationSeconds(model, 1), 0.0f));
    TEST_ASSERT(Near(MdxSequenceDurationSeconds(model, -1), 0.0f));
    TEST_ASSERT(Near(MdxSequenceDurationSeconds(model, 2), 0.0f));
    return true;
}

bool TestMdxSkipsFaceFxAndBindPose() {
    std::vector<uint8_t> data;
    PushMdxTag(data, 'M', 'D', 'L', 'X');
    // VERS chunk: size 4 + version 800.
    PushMdxTag(data, 'V', 'E', 'R', 'S');
    PushLE32(data, 4);
    PushLE32(data, 800);
    // FAFX chunk with opaque payload: must be skipped, not fail.
    PushMdxTag(data, 'F', 'A', 'F', 'X');
    PushLE32(data, 16);
    for (int i = 0; i < 16; i++) data.push_back(uint8_t(i));
    // BPOS chunk with opaque payload: must be skipped, not fail.
    PushMdxTag(data, 'B', 'P', 'O', 'S');
    PushLE32(data, 12);
    for (int i = 0; i < 12; i++) data.push_back(uint8_t(0xAB));
    // A sequence after the skipped chunks proves parsing continues.
    PushMdxTag(data, 'S', 'E', 'Q', 'S');
    PushLE32(data, 132);
    PushFixedString(data, "Stand", 80);
    PushLE32(data, 0);     // start frame
    PushLE32(data, 1000);  // end frame
    PushLE32(data, 0x3f800000u); // move speed = 1.0
    PushLE32(data, 0);     // flags
    PushLE32(data, 0x3f800000u); // rarity = 1.0
    PushLE32(data, 0);     // sync point
    for (int i = 0; i < 6; i++) PushLE32(data, 0); // bounds min/max
    PushLE32(data, 0x40000000u); // bounds radius = 2.0

    MdxModel model;
    TEST_ASSERT(MdxLoad(data.data(), data.size(), model));
    TEST_ASSERT(model.version == 800);
    TEST_ASSERT(model.sequences.size() == 1);
    TEST_ASSERT(model.sequences[0].name == "Stand");
    return true;
}

bool TestMdxParsesRibbonEmitter() {
    std::vector<uint8_t> data;
    PushMdxTag(data, 'M', 'D', 'L', 'X');
    PushMdxTag(data, 'R', 'I', 'B', 'B');
    // Entry: 4 (size) + 96 (generic, no timelines) + 44 (fields) + 24 (KRAL timeline) = 168.
    PushLE32(data, 168); // chunk size
    PushLE32(data, 168); // entry size (includes this field)
    PushLE32(data, 96);  // generic object size
    PushFixedString(data, "Ribbon01", 80);
    PushLE32(data, 7);            // object id
    PushLE32(data, 0xffffffffu);  // parent id
    PushLE32(data, 0);            // flags
    PushLE32(data, 0x40a00000u);  // heightAbove = 5.0
    PushLE32(data, 0x40800000u);  // heightBelow = 4.0
    PushLE32(data, 0x3f000000u);  // alpha = 0.5
    PushLE32(data, 0x3f800000u);  // color r = 1.0
    PushLE32(data, 0x3f000000u);  // color g = 0.5
    PushLE32(data, 0x00000000u);  // color b = 0.0
    PushLE32(data, 0x3f800000u);  // lifeSpan = 1.0
    PushLE32(data, 2);            // textureSlot
    PushLE32(data, 10);           // emissionRate (uint)
    PushLE32(data, 2);            // rows
    PushLE32(data, 4);            // columns
    PushLE32(data, 3);            // materialId
    PushLE32(data, 0x41200000u);  // gravity = 10.0

    PushMdxTag(data, 'K', 'R', 'A', 'L');
    PushLE32(data, 1);            // keyframe count
    PushLE32(data, 0);            // interpolation: none
    PushLE32(data, 0xffffffffu);  // global sequence id
    PushLE32(data, 0);            // frame
    PushLE32(data, 0x3f400000u);  // value = 0.75

    MdxModel model;
    TEST_ASSERT(MdxLoad(data.data(), data.size(), model));
    TEST_ASSERT(model.ribbonEmitters.size() == 1);
    const MdxRibbonEmitter& rb = model.ribbonEmitters[0];
    TEST_ASSERT(rb.name == "Ribbon01");
    TEST_ASSERT(rb.objectId == 7);
    TEST_ASSERT(Near(rb.heightAbove, 5.0f));
    TEST_ASSERT(Near(rb.heightBelow, 4.0f));
    TEST_ASSERT(Near(rb.alpha, 0.5f));
    TEST_ASSERT(Near(rb.color[0], 1.0f));
    TEST_ASSERT(Near(rb.color[1], 0.5f));
    TEST_ASSERT(Near(rb.color[2], 0.0f));
    TEST_ASSERT(Near(rb.lifeSpan, 1.0f));
    TEST_ASSERT(rb.textureSlot == 2);
    TEST_ASSERT(Near(rb.emissionRate, 10.0f));
    TEST_ASSERT(rb.rows == 2);
    TEST_ASSERT(rb.columns == 4);
    TEST_ASSERT(rb.materialId == 3);
    TEST_ASSERT(Near(rb.gravity, 10.0f));
    TEST_ASSERT(rb.timelines.size() == 1);
    TEST_ASSERT(rb.timelines[0].tag == TestMdxTag('K', 'R', 'A', 'L'));
    return true;
}
