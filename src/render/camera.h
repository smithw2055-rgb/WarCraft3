#pragma once
#include "math.h"

class OrbitCamera {
public:
    OrbitCamera();

    void SetTarget(float tx, float ty, float tz);
    void SetDistance(float dist) { distance_ = dist; targetDistance_ = dist; }
    void SetMaxDistance(float dist) { maxDistance_ = dist; }
    // Playable camera bounds (ref w3i cameraBounds): clamps pan target in
    // XZ. Margin expands the box so edge scrolling can reveal border tiles.
    void SetBounds(float minX, float maxX, float minZ, float maxZ, float margin = 0.0f) {
        boundsMinX_ = minX - margin; boundsMaxX_ = maxX + margin;
        boundsMinZ_ = minZ - margin; boundsMaxZ_ = maxZ + margin;
        hasBounds_ = true;
        ClampTarget();
    }
    void ClearBounds() { hasBounds_ = false; }
    bool HasBounds() const { return hasBounds_; }
    void SetAngles(float yaw, float pitch) { yaw_ = yaw; targetYaw_ = yaw; pitch_ = pitch; targetPitch_ = pitch; }
    void Orbit(float deltaYaw, float deltaPitch);
    void Zoom(float delta);
    void Pan(float dx, float dz);

    Mat4 GetViewMatrix() const;
    Mat4 GetProjectionMatrix(float aspect) const;

    Vec3 GetPosition() const;
    Vec3 GetTarget() const { return target_; }
    float GetYaw() const { return yaw_; }
    float GetPitch() const { return pitch_; }

    void SaveHomeState();
    void ResetToHome();

    // Input helpers (for use in ProcessInput)
    void OnMouseMove(float dx, float dy, bool leftDown, bool rightDown);
    void OnMouseWheel(float delta);
    void SetNoise(float magnitude, float velocity, bool vertOnly, bool horizOnly);
    Vec3 NoiseOffset() const { return noiseOffset_; }

    // Orbit the rig around a ground point (JASS SetCameraRotateMode).
    // pivotZ is world Z. Consumes radiansLeft over durationLeft seconds.
    // Returns false once the turn is finished.
    bool StepRotateAround(float pivotX, float pivotZ, float& radiansLeft, float& durationLeft, float dt);

    // Call this each frame with dt in seconds
    void Update(float dt);

private:
    Vec3 target_;
    float yaw_ = 0.8f;
    float pitch_ = 0.6f;
    float distance_ = 2000.0f;
    float minDistance_ = 100.0f;
    float maxDistance_ = 2000.0f;
    float fov_ = 45.0f;

    // Home state (initial camera setup, restored when zooming out)
    Vec3 homeTarget_ = {0,0,0};
    float homeYaw_ = 0.8f;
    float homePitch_ = 0.6f;
    float homeDistance_ = 2000.0f;

    // Smooth interpolation targets
    float targetPitch_ = 0.6f;
    float targetDistance_ = 2000.0f;
    float targetYaw_ = 0.8f;
    float smoothSpeed_ = 4.0f;

    float mouseSensitivity_ = 0.005f;
    float zoomSpeed_ = 50.0f;
    float noiseMag_ = 0.0f;
    float noiseVel_ = 0.0f;
    bool noiseVertOnly_ = false;
    bool noiseHorizOnly_ = false;
    float noiseClock_ = 0.0f;
    Vec3 noiseOffset_ = {0, 0, 0};

    void RotateAround(float pivotX, float pivotZ, float deltaRadians);

    bool dragging_ = false;
    float dragYaw_ = 0.0f;
    float dragPitch_ = 0.0f;

    // Optional world bounds for the pan target (unset = free camera).
    bool hasBounds_ = false;
    float boundsMinX_ = 0.0f, boundsMaxX_ = 0.0f;
    float boundsMinZ_ = 0.0f, boundsMaxZ_ = 0.0f;
    void ClampTarget();
};
