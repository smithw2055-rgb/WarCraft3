#include "camera.h"
#include <algorithm>
#include <cmath>

OrbitCamera::OrbitCamera() {
    target_ = {0, 0, 0};
}

void OrbitCamera::SetTarget(float tx, float ty, float tz) {
    target_ = {tx, ty, tz};
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
}

Vec3 OrbitCamera::GetPosition() const {
    float cx = distance_ * std::cos(pitch_) * std::sin(yaw_);
    float cy = distance_ * std::sin(pitch_);
    float cz = distance_ * std::cos(pitch_) * std::cos(yaw_);
    return {target_.x + cx, target_.y + cy + 800.0f, target_.z + cz};
}

Mat4 OrbitCamera::GetViewMatrix() const {
    Vec3 eye = GetPosition();
    return LookAt(eye, target_, {0, 1, 0});
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

void OrbitCamera::Update(float dt) {
    // Smooth interpolation toward targets
    float t = std::min(1.0f, dt * smoothSpeed_);
    yaw_ += (targetYaw_ - yaw_) * t;
    pitch_ += (targetPitch_ - pitch_) * t;
    distance_ += (targetDistance_ - distance_) * t;
}
