// Animator attachment-point tests: JASS attach names ("hand", "HEAD"..)
// resolve to attachment nodes (case-insensitive substring), with world
// positions from the skeleton. Synthetic model, no assets needed.
#include <cmath>
#include <iostream>

#include "model/mdx.h"
#include "render/math.h"
#include "render/mdx_animator.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) return false; } while (0)

namespace {

MdxModel MakeAttachModel() {
    MdxModel model;
    model.name = "AttachTest";
    model.pivotPoints.push_back({0.0f, 0.0f, 0.0f});
    model.pivotPoints.push_back({10.0f, 20.0f, 30.0f});
    model.pivotPoints.push_back({0.0f, 0.0f, 100.0f});

    MdxNode bone;
    bone.name = "Bone_Root";
    bone.objectId = 0;
    bone.parentId = -1;
    model.bones.push_back(bone);

    MdxAttachment hand;
    hand.name = "Hand, Right";
    hand.objectId = 1;
    hand.parentId = 0;
    model.attachments.push_back(hand);

    MdxAttachment head;
    head.name = "Head";
    head.objectId = 2;
    head.parentId = 0;
    model.attachments.push_back(head);
    return model;
}

} // namespace

bool TestAttachFindIsCaseInsensitive() {
    MdxModel model = MakeAttachModel();
    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.FindAttachmentObjectId("hand") == 1);
    TEST_ASSERT(animator.FindAttachmentObjectId("HAND") == 1);
    TEST_ASSERT(animator.FindAttachmentObjectId("Hand, Right") == 1);
    TEST_ASSERT(animator.FindAttachmentObjectId("head") == 2);
    TEST_ASSERT(animator.FindAttachmentObjectId("HEAD") == 2);
    return true;
}

bool TestAttachFindFallbacks() {
    MdxModel model = MakeAttachModel();
    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.FindAttachmentObjectId("") == -1);
    TEST_ASSERT(animator.FindAttachmentObjectId("origin") == -1);
    TEST_ASSERT(animator.FindAttachmentObjectId("ORIGIN") == -1);
    TEST_ASSERT(animator.FindAttachmentObjectId("wing") == -1);
    Vec3 pos;
    TEST_ASSERT(!animator.GetAttachmentWorldPositionYUp("wing", pos));
    TEST_ASSERT(!animator.GetAttachmentWorldPositionYUp("origin", pos));
    return true;
}

bool TestAttachWorldPosition() {    MdxModel model = MakeAttachModel();
    MdxAnimator animator;
    animator.Init(model);
    Vec3 pos;
    TEST_ASSERT(animator.GetAttachmentWorldPositionYUp("hand", pos));
    // MDX (10,20,30) under an identity bone maps Y-up to (10,30,-20).
    TEST_NEAR(pos.x, 10.0f, 1e-3f);
    TEST_NEAR(pos.y, 30.0f, 1e-3f);
    TEST_NEAR(pos.z, -20.0f, 1e-3f);
    Vec3 head;
    TEST_ASSERT(animator.GetAttachmentWorldPositionYUp("head", head));
    TEST_NEAR(head.x, 0.0f, 1e-3f);
    TEST_NEAR(head.y, 100.0f, 1e-3f);
    TEST_NEAR(head.z, 0.0f, 1e-3f);
    return true;
}

namespace {

float ColLen(const Mat4& m, int col) {
    return std::sqrt(m.m[col] * m.m[col] + m.m[col + 4] * m.m[col + 4] +
                     m.m[col + 8] * m.m[col + 8]);
}

float ColDot(const Mat4& m, int a, int b) {
    return m.m[a] * m.m[b] + m.m[a + 4] * m.m[b + 4] + m.m[a + 8] * m.m[b + 8];
}

MdxModel MakeSpinningAttachModel() {
    MdxModel model = MakeAttachModel();
    MdxSequence stand;
    stand.name = "Stand";
    stand.startFrame = 0;
    stand.endFrame = 1000;
    model.sequences.push_back(stand);
    // Clockwise 90-degree rotation about MDX Z on the root bone
    // (quaternion 0,0,s,c; MDX is left-handed, so this turns -90).
    MdxTimeline spin;
    spin.tag = (uint32_t)('K' << 24 | 'G' << 16 | 'R' << 8 | 'T');
    spin.interpolationType = MdxInterpolation::None;
    spin.globalSequenceId = -1;
    spin.frames = {0};
    spin.values = {0.0f, 0.0f, 0.70710678f, 0.70710678f};
    model.bones[0].timelines.push_back(spin);
    return model;
}

} // namespace

bool TestAttachMatrixMatchesPosition() {
    MdxModel model = MakeAttachModel();
    MdxAnimator animator;
    animator.Init(model);
    Mat4 mat;
    Vec3 loc, pos;
    TEST_ASSERT(animator.GetAttachmentWorldMatrixYUp("hand", mat, loc));
    TEST_ASSERT(animator.GetAttachmentWorldPositionYUp("hand", pos));
    TEST_NEAR(loc.x, pos.x, 1e-5f);
    TEST_NEAR(loc.y, pos.y, 1e-5f);
    TEST_NEAR(loc.z, pos.z, 1e-5f);
    // Unrotated: rotation part is identity (orthonormal columns).
    TEST_NEAR(ColLen(mat, 0), 1.0f, 1e-4f);
    TEST_NEAR(ColLen(mat, 1), 1.0f, 1e-4f);
    TEST_NEAR(ColLen(mat, 2), 1.0f, 1e-4f);
    TEST_NEAR(ColDot(mat, 0, 1), 0.0f, 1e-4f);
    TEST_NEAR(mat.m[0], 1.0f, 1e-4f);
    TEST_NEAR(mat.m[5], 1.0f, 1e-4f);
    TEST_NEAR(mat.m[10], 1.0f, 1e-4f);
    Mat4 bad;
    Vec3 nowhere;
    TEST_ASSERT(!animator.GetAttachmentWorldMatrixYUp("wing", bad, nowhere));
    return true;
}

bool TestAttachMatrixFollowsBoneRotation() {
    MdxModel model = MakeSpinningAttachModel();
    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.SetSequence(0, true, 1.0f));
    animator.Update(0.0f);
    Mat4 mat;
    Vec3 loc;
    TEST_ASSERT(animator.GetAttachmentWorldMatrixYUp("hand", mat, loc));
    // MDX pivot (10,20,30) rotated +90 deg about Z ->
    // (-20,10,30), Y-up mapped to (-20,30,-10).
    TEST_NEAR(loc.x, -20.0f, 1e-2f);
    TEST_NEAR(loc.y, 30.0f, 1e-2f);
    TEST_NEAR(loc.z, -10.0f, 1e-2f);
    // Still a pure rotation: orthonormal columns...
    TEST_NEAR(ColLen(mat, 0), 1.0f, 1e-3f);
    TEST_NEAR(ColLen(mat, 1), 1.0f, 1e-3f);
    TEST_NEAR(ColLen(mat, 2), 1.0f, 1e-3f);
    TEST_NEAR(ColDot(mat, 0, 1), 0.0f, 1e-3f);
    TEST_NEAR(ColDot(mat, 0, 2), 0.0f, 1e-3f);
    // ...turned with the bone
    TEST_NEAR(mat.m[0], 0.0f, 1e-3f);
    TEST_NEAR(mat.m[1], 0.0f, 1e-3f);
    TEST_NEAR(mat.m[2], -1.0f, 1e-3f);
    TEST_NEAR(mat.m[4], 0.0f, 1e-3f);
    TEST_NEAR(mat.m[5], 1.0f, 1e-3f);
    TEST_NEAR(mat.m[6], 0.0f, 1e-3f);
    TEST_NEAR(mat.m[8], 1.0f, 1e-3f);
    TEST_NEAR(mat.m[9], 0.0f, 1e-3f);
    TEST_NEAR(mat.m[10], 0.0f, 1e-3f);
    return true;
}
