// MDL text parser tests: hand-authored fixture covering every block the
// parser supports, plus malformed-input rejection. MDL maps onto the same
// MdxModel the MDX binary loader fills (single internal representation).
#include <cmath>
#include <iostream>
#include <string>

#include "model/mdl.h"
#include "model/mdx.h"

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace {

bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

const MdxTimeline* FindTag(const std::vector<MdxTimeline>& tls, uint32_t tag) {
    for (const auto& tl : tls) {
        if (tl.tag == tag) return &tl;
    }
    return nullptr;
}

constexpr uint32_t Tag(char a, char b, char c, char d) {
    return ((uint32_t)(uint8_t)a << 24) | ((uint32_t)(uint8_t)b << 16) |
           ((uint32_t)(uint8_t)c << 8) | (uint32_t)(uint8_t)d;
}

const char* kFixture = R"MDL(
Version {
    FormatVersion 800,
}
Model "TestCube" {
    NumGeosets 1,
    NumBones 2,
    BlendTime 150,
    MinimumExtent { -10, -10, -10 },
    MaximumExtent { 10, 10, 10 },
    BoundsRadius 20,
}
Sequences 2 {
    Anim "Stand" {
        Interval { 0, 100 },
        MoveSpeed 0,
        Rarity 0,
    }
    Anim "Death" {
        Interval { 110, 200 },
        MoveSpeed 0,
        NonLooping,
        Rarity 5,
        MinimumExtent { -9, -9, -9 },
        MaximumExtent { 9, 9, 9 },
        BoundsRadius 18,
    }
}
GlobalSequences 2 {
    Duration 100,
    Duration 200,
}
Textures 2 {
    Bitmap {
        Image "Textures\\White.blp",
    }
    Bitmap {
        Image "",
        ReplaceableId 1,
    }
}
Materials 1 {
    Material {
        Layer {
            FilterMode Blend,
            static TextureID 0,
            static Alpha 0.5,
            TwoSided,
        }
        Layer {
            FilterMode Additive,
            TextureID 1 {
                DontInterp,
                0: { 0 },
                50: { 1 },
            }
            Alpha 2 {
                Linear,
                0: { 1.0 },
                100: { 0.0 },
            }
            NoDepthTest,
            TextureAnimId 0,
            CoordId 1,
        }
    }
}
TextureAnims 1 {
    TVertexAnim {
        Translation 1 {
            Linear,
            0: { 0, 0, 0 },
        }
        Rotation 1 {
            DontInterp,
            0: { 0, 0, 0, 1 },
        }
        Scaling 1 {
            DontInterp,
            0: { 1, 1, 1 },
        }
    }
}
Geoset {
    Vertices 4 {
        { -1, -1, 0 },
        { 1, -1, 0 },
        { 1, 1, 0 },
        { -1, 1, 0 },
    }
    Normals 4 {
        { 0, 0, 1 },
        { 0, 0, 1 },
        { 0, 0, 1 },
        { 0, 0, 1 },
    }
    TVertices 4 {
        { 0, 0 },
        { 1, 0 },
        { 1, 1 },
        { 0, 1 },
    }
    TVertices 4 {
        { 0, 1 },
        { 1, 1 },
        { 1, 0 },
        { 0, 0 },
    }
    VertexGroup {
        0,
        0,
        1,
        1,
    }
    Faces 1 6 {
        Triangles {
            { 0, 1, 2, 2, 3, 0 },
        }
    }
    Groups 2 3 {
        Matrices { 0 },
        Matrices { 1, 2 },
    }
    MinimumExtent { -1, -1, 0 },
    MaximumExtent { 1, 1, 0 },
    BoundsRadius 2,
    MaterialID 0,
    SelectionGroup 0,
}
GeosetAnim {
    Alpha 2 {
        Linear,
        0: { 1.0 },
        100: { 0.5 },
    }
    Color { 0.1, 0.2, 0.3 },
    GeosetId 0,
}
Bone "Bone_root" {
    ObjectId 0,
    Parent -1,
    Translation 2 {
        DontInterp,
        0: { 10, 20, 30 },
        100: { 10, 20, 40 },
    }
    Rotation 1 {
        Linear,
        0: { 0, 0, 0, 1 },
    }
    Scaling 1 {
        Hermite,
        0: { 1, 1, 1, InTan { 0, 0, 0, }, OutTan { 0, 0, 0, }, },
    }
    Visibility 1 {
        DontInterp,
        0: { 1 },
    }
}
Bone "Bone_child" {
    ObjectId 1,
    Parent 0,
    GeosetId 0,
    GeosetAnimId -1,
}
Helper "Helper_0" {
    ObjectId 2,
    Parent 0,
    Billboarded,
}
Attachment "Attachment_0" {
    ObjectId 3,
    Parent 1,
    Path "Abilities\\Weapons\\Test.mdx",
    AttachmentID 5,
    Visibility 1 {
        DontInterp,
        0: { 1 },
    }
}
Light "Light_0" {
    ObjectId 4,
    Parent -1,
    Directional,
    Color { 1.0, 0.5, 0.25 },
    Intensity 1.2,
    AmbientColor { 0.1, 0.2, 0.3 },
    AmbientIntensity 0.4,
    AttenuationStart 100,
    AttenuationEnd 300,
    Visibility 1 {
        DontInterp,
        0: { 1 },
    }
}
ParticleEmitter "Emitter_0" {
    ObjectId 5,
    Parent 0,
    EmissionRate 50,
    Gravity 9.8,
    Longitude 45,
    Latitude 30,
    Path "Abilities\\Weapons\\Lit.mdx",
    LifeSpan 2.5,
    Speed 120,
    TextureID 0,
    EmissionRate 1 {
        Linear,
        0: { 10.0 },
        100: { 20.0 },
    }
}
ParticleEmitter2 "Emitter2_0" {
    ObjectId 6,
    Parent 0,
    Speed 90,
    Variation 0.2,
    Latitude 15,
    Gravity 100,
    LifeSpan 3,
    EmissionRate 40,
    Length 50,
    Width 25,
    FilterMode Blend,
    Rows 1,
    Columns 1,
    Head,
    TailLength 60,
    Time 0.5,
    SegmentColor {
        Color { 255, 0, 0 },
        Alpha 255,
    }
    SegmentColor {
        Color { 0, 255, 0 },
        Alpha 128,
    }
    ParticleScaling { 1.0, 2.0, 3.0 },
    HeadIntervals {
        { 0, 100, 200 },
        { 300, 400, 500 },
    }
    TextureID 1,
    Squirt 0,
    PriorityPlane 1,
    ReplaceableId 0,
    Visibility 1 {
        DontInterp,
        0: { 1 },
    }
    EmissionRate 2 {
        Linear,
        0: { 40.0 },
        100: { 80.0 },
    }
}
RibbonEmitter "Ribbon_0" {
    ObjectId 7,
    Parent 1,
    HeightAbove 10,
    HeightBelow 5,
    Alpha 0.8,
    Color { 1.0, 0.0, 0.0 },
    LifeSpan 2,
    TextureSlot 0,
    EmissionRate 30,
    Rows 1,
    Columns 1,
    MaterialID 0,
    Gravity 50,
    HeightAbove 1 {
        Linear,
        0: { 10.0 },
        100: { 20.0 },
    }
    Visibility 1 {
        DontInterp,
        0: { 1 },
    }
}
EventObject "SPN test01" {
    ObjectId 8,
    Parent 0,
    EventTrack 3 {
        0,
        100,
        200,
    }
}
CollisionShape "Shape_box" {
    ObjectId 9,
    Parent -1,
    Box,
    Vertices {
        { -5, -5, -5 },
        { 5, 5, 5 },
    }
    BoundsRadius 9,
}
CollisionShape "Shape_ball" {
    ObjectId 10,
    Parent -1,
    Sphere,
    Vertices {
        { 1, 2, 3 },
    }
    BoundsRadius 4,
}
Camera "Camera_0" {
    Position { 100, 200, 300 },
    TargetPosition { 0, 0, 50 },
    FieldOfView 0.5,
    FarClip 3000,
    NearClip 10,
    Translation 1 {
        Linear,
        0: { 100, 200, 300 },
    }
}
ParticleEmitterPopcorn "Pop_0" {
    ObjectId 11,
    Parent 0,
    LifeSpan 4,
    EmissionRate 12,
    Speed 60,
    Color { 0.5, 0.5, 0.5 },
    Alpha 0.9,
    ReplaceableId 2,
    Color 1 {
        Linear,
        0: { 0.5, 0.5, 0.5 },
    }
}
)MDL";

} // namespace

bool TestMdlFixtureParses() {
    MdxModel model;
    std::string error;
    TEST_ASSERT(MdlLoad(kFixture, std::string(kFixture).size(), model, &error));
    TEST_ASSERT(error.empty());

    TEST_ASSERT(model.version == 800);
    TEST_ASSERT(model.name == "TestCube");
    TEST_ASSERT(Near(model.boundsMin[0], -10.0f));
    TEST_ASSERT(Near(model.boundsRadius, 20.0f));

    // Sequences + global sequences.
    TEST_ASSERT(model.sequences.size() == 2);
    TEST_ASSERT(model.sequences[0].name == "Stand");
    TEST_ASSERT(model.sequences[0].startFrame == 0);
    TEST_ASSERT(model.sequences[0].endFrame == 100);
    TEST_ASSERT(model.sequences[1].name == "Death");
    TEST_ASSERT((model.sequences[1].flags & 0x1) != 0);
    TEST_ASSERT(Near(model.sequences[1].rarity, 5.0f));
    TEST_ASSERT(Near(model.sequences[1].boundsRadius, 18.0f));
    TEST_ASSERT(model.globalSequences.size() == 2);
    TEST_ASSERT(model.globalSequences[0] == 100);
    TEST_ASSERT(model.globalSequences[1] == 200);

    // Textures.
    TEST_ASSERT(model.textures.size() == 2);
    TEST_ASSERT(model.textures[0].path == "Textures\\White.blp");
    TEST_ASSERT(model.textures[0].replaceableId == 0);
    TEST_ASSERT(model.textures[1].replaceableId == 1);

    // Materials / layers.
    TEST_ASSERT(model.materials.size() == 1);
    TEST_ASSERT(model.materials[0].layers.size() == 2);
    const MdxLayer& l0 = model.materials[0].layers[0];
    TEST_ASSERT(l0.filterMode == 2);
    TEST_ASSERT(l0.textureId == 0);
    TEST_ASSERT(Near(l0.alpha, 0.5f));
    TEST_ASSERT((l0.flags & 0x10) != 0); // TwoSided
    const MdxLayer& l1 = model.materials[0].layers[1];
    TEST_ASSERT(l1.filterMode == 3);
    TEST_ASSERT((l1.flags & 0x40) != 0); // NoDepthTest
    TEST_ASSERT(l1.textureAnimationId == 0);
    TEST_ASSERT(l1.coordId == 1);
    const MdxTimeline* kmtf = FindTag(l1.timelines, Tag('K','M','T','F'));
    TEST_ASSERT(kmtf != nullptr && kmtf->frames.size() == 2);
    TEST_ASSERT(Near(kmtf->values[0], 0.0f) && Near(kmtf->values[1], 1.0f));
    const MdxTimeline* kmlt = FindTag(l1.timelines, Tag('K','M','L','T'));
    TEST_ASSERT(kmlt != nullptr && kmlt->frames[1] == 100);
    TEST_ASSERT(Near(kmlt->values[1], 0.0f));

    // Texture anims.
    TEST_ASSERT(model.texAnims.size() == 1);
    TEST_ASSERT(FindTag(model.texAnims[0].timelines, Tag('K','T','A','T')) != nullptr);
    TEST_ASSERT(FindTag(model.texAnims[0].timelines, Tag('K','T','A','R')) != nullptr);
    TEST_ASSERT(FindTag(model.texAnims[0].timelines, Tag('K','T','A','S')) != nullptr);

    // Geoset.
    TEST_ASSERT(model.geosets.size() == 1);
    const MdxGeoset& geo = model.geosets[0];
    TEST_ASSERT(geo.vertices.size() == 12);
    TEST_ASSERT(Near(geo.vertices[0], -1.0f));
    TEST_ASSERT(geo.normals.size() == 12);
    TEST_ASSERT(geo.uvSets.size() == 2);
    TEST_ASSERT(geo.uvs.size() == 8 && Near(geo.uvs[0], 0.0f));
    TEST_ASSERT(geo.vertexGroups.size() == 4);
    TEST_ASSERT(geo.vertexGroups[0] == 0 && geo.vertexGroups[2] == 1);
    TEST_ASSERT(geo.indices.size() == 6);
    TEST_ASSERT(geo.indices[0] == 0 && geo.indices[5] == 0);
    TEST_ASSERT(geo.matrixGroups.size() == 2);
    TEST_ASSERT(geo.matrixGroups[0] == 1 && geo.matrixGroups[1] == 2);
    TEST_ASSERT(geo.matrixIndices.size() == 3);
    TEST_ASSERT(geo.matrixIndices[2] == 2);
    TEST_ASSERT(geo.materialId == 0);

    // Geoset anim (Color stored BGR like binary).
    TEST_ASSERT(model.geosetAnims.size() == 1);
    TEST_ASSERT(model.geosetAnims[0].geosetId == 0);
    const MdxTimeline* kgal = FindTag(model.geosetAnims[0].timelines, Tag('K','G','A','O'));
    TEST_ASSERT(kgal != nullptr && Near(kgal->values[1], 0.5f));
    const MdxTimeline* kgac = FindTag(model.geosetAnims[0].timelines, Tag('K','G','A','C'));
    TEST_ASSERT(kgac == nullptr); // static color: no track
    TEST_ASSERT(Near(model.geosetAnims[0].color[0], 0.3f)); // BGR-swapped
    TEST_ASSERT(Near(model.geosetAnims[0].color[2], 0.1f));

    // Bones / helpers.
    TEST_ASSERT(model.bones.size() == 2);
    TEST_ASSERT(model.bones[0].name == "Bone_root");
    TEST_ASSERT(model.bones[0].objectId == 0 && model.bones[0].parentId == -1);
    const MdxTimeline* kgtr = FindTag(model.bones[0].timelines, Tag('K','G','T','R'));
    TEST_ASSERT(kgtr != nullptr && kgtr->frames.size() == 2);
    TEST_ASSERT(Near(kgtr->values[0], 10.0f));
    const MdxTimeline* kgrt = FindTag(model.bones[0].timelines, Tag('K','G','R','T'));
    TEST_ASSERT(kgrt != nullptr && kgrt->values.size() == 4);
    TEST_ASSERT(Near(kgrt->values[3], 1.0f));
    const MdxTimeline* kgsc = FindTag(model.bones[0].timelines, Tag('K','G','S','C'));
    TEST_ASSERT(kgsc != nullptr);
    TEST_ASSERT(kgsc->interpolationType == MdxInterpolation::Hermite);
    TEST_ASSERT(kgsc->inTans.size() == 3 && kgsc->outTans.size() == 3);
    TEST_ASSERT(FindTag(model.bones[0].timelines, Tag('K','G','B','V')) != nullptr);
    TEST_ASSERT(model.bones[1].geosetId == 0);
    TEST_ASSERT(model.bones[1].geosetAnimationId == -1);
    TEST_ASSERT(model.helpers.size() == 1);
    TEST_ASSERT((model.helpers[0].flags & 0x8) != 0); // Billboarded

    // Pivots derived from frame-0 translations.
    TEST_ASSERT(model.pivotPoints.size() >= 1);
    TEST_ASSERT(Near(model.pivotPoints[0][0], 10.0f));
    TEST_ASSERT(Near(model.pivotPoints[0][2], 30.0f));

    // Attachment / light.
    TEST_ASSERT(model.attachments.size() == 1);
    TEST_ASSERT(model.attachments[0].path == "Abilities\\Weapons\\Test.mdx");
    TEST_ASSERT(model.attachments[0].attachmentId == 5);
    TEST_ASSERT(FindTag(model.attachments[0].timelines, Tag('K','A','T','V')) != nullptr);
    TEST_ASSERT(model.lights.size() == 1);
    TEST_ASSERT(model.lights[0].type == MdxLightType::Directional);
    TEST_ASSERT(Near(model.lights[0].intensity, 1.2f));
    TEST_ASSERT(Near(model.lights[0].ambientColor[2], 0.3f));
    TEST_ASSERT(Near(model.lights[0].attenuationEnd, 300.0f));

    // Particle emitters.
    TEST_ASSERT(model.particleEmitters1.size() == 1);
    TEST_ASSERT(Near(model.particleEmitters1[0].emissionRate, 50.0f));
    TEST_ASSERT(model.particleEmitters1[0].internalModelPath == "Abilities\\Weapons\\Lit.mdx");
    TEST_ASSERT(FindTag(model.particleEmitters1[0].timelines, Tag('K','P','E','E')) != nullptr);
    TEST_ASSERT(model.particleEmitters2.size() == 1);
    const MdxParticleEmitter2& pre2 = model.particleEmitters2[0];
    TEST_ASSERT(Near(pre2.speed, 90.0f) && Near(pre2.variation, 0.2f));
    TEST_ASSERT(Near(pre2.length, 50.0f) && Near(pre2.width, 25.0f));
    TEST_ASSERT(pre2.filterMode == 2 && pre2.rows == 1 && pre2.columns == 1);
    TEST_ASSERT(pre2.headOrTail == 0);
    TEST_ASSERT(Near(pre2.tailLength, 60.0f) && Near(pre2.timeMiddle, 0.5f));
    TEST_ASSERT(Near(pre2.segmentColors[0][0], 255.0f));
    TEST_ASSERT(pre2.segmentAlphas[0] == 255 && pre2.segmentAlphas[1] == 128);
    TEST_ASSERT(Near(pre2.segmentScaling[2], 3.0f));
    TEST_ASSERT(pre2.headIntervals[1][2] == 500);
    TEST_ASSERT(pre2.textureId == 1 && pre2.priorityPlane == 1);
    TEST_ASSERT(FindTag(pre2.timelines, Tag('K','P','2','E')) != nullptr);
    TEST_ASSERT(FindTag(pre2.timelines, Tag('K','P','2','V')) != nullptr);

    // Ribbon.
    TEST_ASSERT(model.ribbonEmitters.size() == 1);
    const MdxRibbonEmitter& rib = model.ribbonEmitters[0];
    TEST_ASSERT(Near(rib.heightAbove, 10.0f) && Near(rib.heightBelow, 5.0f));
    TEST_ASSERT(Near(rib.alpha, 0.8f) && Near(rib.color[0], 1.0f));
    TEST_ASSERT(Near(rib.lifeSpan, 2.0f) && rib.textureSlot == 0);
    TEST_ASSERT(Near(rib.emissionRate, 30.0f) && Near(rib.gravity, 50.0f));
    TEST_ASSERT(FindTag(rib.timelines, Tag('K','R','H','A')) != nullptr);
    TEST_ASSERT(FindTag(rib.timelines, Tag('K','R','V','S')) != nullptr);

    // Event / collision / camera / popcorn.
    TEST_ASSERT(model.eventObjects.size() == 1);
    TEST_ASSERT(model.eventObjects[0].type == "SPN");
    TEST_ASSERT(model.eventObjects[0].id == "test01");
    const MdxTimeline* kevt = FindTag(model.eventObjects[0].timelines, Tag('K','E','V','T'));
    TEST_ASSERT(kevt != nullptr && kevt->frames.size() == 3);
    TEST_ASSERT(kevt->frames[2] == 200);
    TEST_ASSERT(model.collisionShapes.size() == 2);
    TEST_ASSERT(model.collisionShapes[0].type == MdxCollisionShapeType::Box);
    TEST_ASSERT(Near(model.collisionShapes[0].vertices[1][0], 5.0f));
    TEST_ASSERT(model.collisionShapes[1].type == MdxCollisionShapeType::Sphere);
    TEST_ASSERT(Near(model.collisionShapes[1].boundsRadius, 4.0f));
    TEST_ASSERT(model.cameras.size() == 1);
    TEST_ASSERT(Near(model.cameras[0].pos[0], 100.0f));
    TEST_ASSERT(Near(model.cameras[0].target[2], 50.0f));
    TEST_ASSERT(Near(model.cameras[0].fov, 0.5f));
    TEST_ASSERT(FindTag(model.cameras[0].timelines, Tag('K','C','T','R')) != nullptr);
    TEST_ASSERT(model.particleEmittersPopcorn.size() == 1);
    TEST_ASSERT(Near(model.particleEmittersPopcorn[0].lifeSpan, 4.0f));
    TEST_ASSERT(model.particleEmittersPopcorn[0].replaceableId == 2);
    TEST_ASSERT(FindTag(model.particleEmittersPopcorn[0].timelines, Tag('K','P','P','C')) != nullptr);
    return true;
}

bool TestMdlRejectsGarbage() {
    MdxModel model;
    std::string error;
    const std::string bad = "Geoset { Vertices 2 { { 1, 2, 3 }, ";
    TEST_ASSERT(!MdlLoad(bad.c_str(), bad.size(), model, &error));
    TEST_ASSERT(!error.empty());
    TEST_ASSERT(!MdlLoad(nullptr, 0, model, nullptr));
    const std::string badNum = "Model \"x\" { BoundsRadius banana, }";
    TEST_ASSERT(!MdlLoad(badNum.c_str(), badNum.size(), model, nullptr));
    return true;
}
