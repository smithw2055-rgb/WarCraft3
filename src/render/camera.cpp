#include "camera.h"
#include <algorithm>
#include <cmath>

OrbitCamera::OrbitCamera() {
    target_ = {0, 0, 0};
}

void OrbitCamera::SetTarget(float tx, float ty, float tz) {
    target_ = {tx, ty, tz};
    ClampTarget();
}

void OrbitCamera::Orbit(float deltaYaw, float deltaPitch) {
    targetYaw_ += deltaYaw;
    targetPitch_ += deltaPitch;
    targetPitch_ = std::max(0.0f, std::min(1.4f, targetPitch_));
}

void OrbitCamera::Zoom(float delta) {
    targetDistance_ = std::max(minDistance_, std::min(maxDistance_, targetDistance_ + delta));
    // If zoomed out past home distance, restore home yaw/pitch
    if (targetDistance_ >= homeDistance_) {
        targetDistance_ = homeDistance_;
        targetYaw_ = homeYaw_;
        targetPitch_ = homePitch_;
        target_ = homeTarget_;
    }
}

void OrbitCamera::SaveHomeState() {
    homeTarget_ = target_;
    homeYaw_ = targetYaw_;
    homePitch_ = targetPitch_;
    homeDistance_ = distance_;
    maxDistance_ = homeDistance_ * 1.2f;
}

void OrbitCamera::ResetToHome() {
    target_ = homeTarget_;
    targetYaw_ = homeYaw_;
    targetPitch_ = homePitch_;
    targetDistance_ = homeDistance_;
}

void OrbitCamera::Pan(float dx, float dz) {
    // Compute camera right and forward directions in XZ plane
    float fy = std::sin(yaw_);
    float fx = std::cos(yaw_);
    // forwardDir (from camera toward target) = (-sin(yaw), -cos(yaw))
    // rightDir = (cos(yaw), -sin(yaw))
    float fwdX = -fy, fwdZ = -fx;
    float rightX = fx, rightZ = -fy;
    target_.x += rightX * dx + fwdX * dz;
    target_.z += rightZ * dx + fwdZ * dz;
    ClampTarget();
}

Vec3 OrbitCamera::GetPosition() const {
    float cx = distance_ * std::cos(pitch_) * std::sin(yaw_);
    float cy = distance_ * std::sin(pitch_);
    float cz = distance_ * std::cos(pitch_) * std::cos(yaw_);
    return {target_.x + cx + noiseOffset_.x, target_.y + cy + noiseOffset_.y,
            target_.z + cz + noiseOffset_.z};
}

Mat4 OrbitCamera::GetViewMatrix() const {
    Vec3 eye = GetPosition();
    Vec3 look = {target_.x + noiseOffset_.x, target_.y + noiseOffset_.y, target_.z + noiseOffset_.z};
    return LookAt(eye, look, {0, 1, 0});
}

Mat4 OrbitCamera::GetProjectionMatrix(float aspect) const {
    return Perspective(fov_, aspect, 8.0f, 50000.0f);
}

void OrbitCamera::OnMouseMove(float dx, float dy, bool leftDown, bool rightDown) {
    if (leftDown || rightDown) {
        Orbit(dx * mouseSensitivity_, dy * mouseSensitivity_);
    }
}

void OrbitCamera::OnMouseWheel(float delta) {
    Zoom(-delta * zoomSpeed_);
}

void OrbitCamera::RotateAround(float pivotX, float pivotZ, float deltaRadians) {
    const float dx = target_.x - pivotX;
    const float dz = target_.z - pivotZ;
    const float c = std::cos(deltaRadians);
    const float s = std::sin(deltaRadians);
    target_.x = pivotX + dx * c - dz * s;
    target_.z = pivotZ + dx * s + dz * c;
    yaw_ += deltaRadians;
    targetYaw_ = yaw_;
}

bool OrbitCamera::StepRotateAround(float pivotX, float pivotZ,
                                   float& radiansLeft, float& durationLeft, float dt) {
    if (durationLeft <= 1.0e-4f) {
        RotateAround(pivotX, pivotZ, radiansLeft);
        radiansLeft = 0.0f;
        durationLeft = 0.0f;
        return false;
    }
    if (dt <= 0.0f) return true;
    const float step = std::min(dt, durationLeft);
    const float delta = radiansLeft * (step / durationLeft);
    RotateAround(pivotX, pivotZ, delta);
    radiansLeft -= delta;
    durationLeft -= step;
    if (durationLeft <= 1.0e-4f) {
        durationLeft = 0.0f;
        radiansLeft = 0.0f;
        return false;
    }
    return true;
}

void OrbitCamera::SetNoise(float magnitude, float velocity, bool vertOnly, bool horizOnly) {
    noiseMag_ = magnitude;
    noiseVel_ = velocity;
    noiseVertOnly_ = vertOnly;
    noiseHorizOnly_ = horizOnly;
}

void OrbitCamera::Update(float dt) {
    // Smooth interpolation toward targets
    float t = std::min(1.0f, dt * smoothSpeed_);
    yaw_ += (targetYaw_ - yaw_) * t;
    pitch_ += (targetPitch_ - pitch_) * t;
    distance_ += (targetDistance_ - distance_) * t;
    ClampTarget();
    noiseClock_ += dt * (noiseVel_ > 0.0f ? noiseVel_ : 1.0f);
    if (noiseMag_ <= 0.0f) {
        noiseOffset_ = {0, 0, 0};
    } else {
        const float s = std::sin(noiseClock_) * noiseMag_;
        const float c = std::cos(noiseClock_ * 1.3f) * noiseMag_;
        const bool any = noiseVertOnly_ || noiseHorizOnly_;
        const bool vert = any ? noiseVertOnly_ : true;
        const bool horiz = any ? noiseHorizOnly_ : true;
        noiseOffset_ = {horiz ? s : 0.0f, vert ? c * 0.35f : 0.0f, horiz ? c : 0.0f};
    }
}

void OrbitCamera::ClampTarget() {
    if (!hasBounds_) return;
    target_.x = std::max(boundsMinX_, std::min(boundsMaxX_, target_.x));
    target_.z = std::max(boundsMinZ_, std::min(boundsMaxZ_, target_.z));
}
