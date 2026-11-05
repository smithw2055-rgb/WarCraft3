// Scene instance-matrix convention tests: MakeSceneInstanceMatrix is the
// shared definition (render path refactored onto it); attached effects
// compose through it. Header-only, no GL needed.
#include <cmath>
#include <iostream>

#include "render/camera.h"
#include "render/scene.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) return false; } while (0)

namespace {
constexpr float kHalfPi = 1.57079632679f;
}

bool TestInstanceMatrixIdentity() {
    const Mat4 m = MakeSceneInstanceMatrix(10, 20, 30, 0.0f, 1, 1, 1);
    TEST_NEAR(m.m[0], 1.0f, 1e-6f);
    TEST_NEAR(m.m[5], 1.0f, 1e-6f);
    TEST_NEAR(m.m[10], 1.0f, 1e-6f);
    TEST_NEAR(m.m[15], 1.0f, 1e-6f);
    TEST_NEAR(m.m[12], 10.0f, 1e-6f);
    TEST_NEAR(m.m[13], 20.0f, 1e-6f);
    TEST_NEAR(m.m[14], 30.0f, 1e-6f);
    TEST_NEAR(m.m[1], 0.0f, 1e-6f);
    TEST_NEAR(m.m[2], 0.0f, 1e-6f);
    TEST_NEAR(m.m[8], 0.0f, 1e-6f);
    return true;
}

bool TestInstanceMatrixRotation() {
    // Instance angles are degrees (unit facing / map object angles); the
    // matrix boundary converts to radians.
    const Mat4 m = MakeSceneInstanceMatrix(0, 0, 0, 90.0f, 1, 1, 1);
    // Same entries the render path produced inline before the refactor.
    TEST_NEAR(m.m[0], 0.0f, 1e-6f);
    TEST_NEAR(m.m[2], -1.0f, 1e-6f);
    TEST_NEAR(m.m[5], 1.0f, 1e-6f);
    TEST_NEAR(m.m[8], 1.0f, 1e-6f);
    TEST_NEAR(m.m[10], 0.0f, 1e-6f);
    return true;
}

bool TestInstanceMatrixPerAxisScale() {
    const Mat4 m = MakeSceneInstanceMatrix(1, 2, 3, 0.0f, 2, 3, 4);
    TEST_NEAR(m.m[0], 2.0f, 1e-6f);
    TEST_NEAR(m.m[5], 3.0f, 1e-6f);
    TEST_NEAR(m.m[10], 4.0f, 1e-6f);
    TEST_NEAR(m.m[12], 1.0f, 1e-6f);
    TEST_NEAR(m.m[13], 2.0f, 1e-6f);
    TEST_NEAR(m.m[14], 3.0f, 1e-6f);
    return true;
}

bool TestCameraRotateAroundPoint() {
    OrbitCamera cam;
    cam.SetTarget(100.0f, 0.0f, 0.0f);
    cam.SetAngles(0.0f, 0.0f);
    cam.SetDistance(1000.0f);
    float radians = kHalfPi;
    float duration = 1.0f;
    TEST_ASSERT(cam.StepRotateAround(0.0f, 0.0f, radians, duration, 0.5f));
    TEST_NEAR(radians, kHalfPi * 0.5f, 1e-4f);
    TEST_NEAR(duration, 0.5f, 1e-4f);
    TEST_NEAR(cam.GetYaw(), kHalfPi * 0.5f, 1e-4f);
    const float mid = std::cos(kHalfPi * 0.5f) * 100.0f;
    TEST_NEAR(cam.GetTarget().x, mid, 1e-3f);
    TEST_NEAR(cam.GetTarget().z, mid, 1e-3f);

    TEST_ASSERT(!cam.StepRotateAround(0.0f, 0.0f, radians, duration, 0.5f));
    TEST_NEAR(cam.GetTarget().x, 0.0f, 1e-3f);
    TEST_NEAR(cam.GetTarget().z, 100.0f, 1e-3f);
    TEST_NEAR(cam.GetYaw(), kHalfPi, 1e-3f);

    OrbitCamera look;
    look.SetTarget(0.0f, 50.0f, 0.0f);
    look.SetAngles(0.0f, 0.0f);
    look.SetDistance(1000.0f);
    float turn = kHalfPi;
    float instant = 0.0f;
    TEST_ASSERT(!look.StepRotateAround(0.0f, 0.0f, turn, instant, 0.0f));
    TEST_NEAR(look.GetTarget().x, 0.0f, 1e-3f);
    TEST_NEAR(look.GetTarget().z, 0.0f, 1e-3f);
    TEST_NEAR(look.GetYaw(), kHalfPi, 1e-3f);
    const Vec3 eye = look.GetPosition();
    TEST_NEAR(eye.x, 1000.0f, 1e-2f);
    TEST_NEAR(eye.z, 0.0f, 1e-2f);
    return true;
}
