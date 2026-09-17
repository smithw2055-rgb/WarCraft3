#include "mdx_prem_particles.h"

#include "render/mdx_animator.h"

#include <algorithm>
#include <cmath>

namespace {

static float HashUnit(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return (float)(value & 0x00ffffffu) / (float)0x01000000u;
}

static Vec3 TransformVector(const Mat4& matrix, const Vec3& vector) {
    return Vec3{
        matrix.m[0] * vector.x + matrix.m[4] * vector.y + matrix.m[8] * vector.z,
        matrix.m[1] * vector.x + matrix.m[5] * vector.y + matrix.m[9] * vector.z,
        matrix.m[2] * vector.x + matrix.m[6] * vector.y + matrix.m[10] * vector.z
    };
}

static Mat4 RotateY(float radians) {
    Mat4 r = Mat4::Identity();
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

static Vec3 MdxScaleToYUp(const Vec3& scale) {
    return {scale.x, scale.z, scale.y};
}

static Mat4 BuildInternalModelTransform(const Vec3& position,
                                        const Vec3& mdxScale,
                                        uint32_t spawnSerial) {
    const float pi = 3.14159265358979323846f;
    const float yaw = HashUnit(spawnSerial * 1103515245u + 12345u) * 2.0f * pi;
    const Vec3 yUpScale = MdxScaleToYUp(mdxScale);
    return Mat4::Translate(position.x, position.y, position.z)
         * RotateY(yaw)
         * Mat4::Scale(yUpScale.x, yUpScale.y, yUpScale.z);
}

static Vec3 BuildEmitterVelocity(const MdxParticleEmitter1& emitter,
                                 const MdxParticleEmitter1Sample& sample,
                                 const Mat4& localTransform,
                                 const Vec3& mdxScale,
                                 uint32_t spawnSerial) {
    const float pi = 3.14159265358979323846f;
    const float theta = (HashUnit((uint32_t)emitter.objectId * 73856093u + spawnSerial * 19349663u) * 2.0f - 1.0f) * pi;
    const float latitude = std::max(0.0f, sample.latitude);
    const float phi = (HashUnit((uint32_t)emitter.objectId * 83492791u + spawnSerial * 2971215073u) * 2.0f - 1.0f) * latitude;

    const float sinPhi = std::sin(phi);
    Vec3 localDirection{
        std::cos(theta) * sinPhi,
        std::cos(phi),
        std::sin(theta) * sinPhi
    };
    Vec3 worldDirection = TransformVector(localTransform, localDirection);
    const float lenSq = Dot(worldDirection, worldDirection);
    if (lenSq > 1e-8f) {
        worldDirection = worldDirection / std::sqrt(lenSq);
    }
    Vec3 velocity = worldDirection * sample.speed;
    const Vec3 yUpScale = MdxScaleToYUp(mdxScale);
    velocity.x *= yUpScale.x;
    velocity.y *= yUpScale.y;
    velocity.z *= yUpScale.z;
    return velocity;
}

} // namespace

void MdxResetParticleEmitter1RuntimeStates(const MdxModel& model,
                                           std::vector<MdxParticleEmitter1RuntimeState>& states) {
    states.assign(model.particleEmitters1.size(), MdxParticleEmitter1RuntimeState{});
}

int MdxCollectParticleEmitter1Spawns(const MdxModel& model,
                                     const MdxAnimator* animator,
                                     uint32_t frame,
                                     float elapsedMs,
                                     float dt,
                                     std::vector<MdxParticleEmitter1RuntimeState>& states,
                                     std::vector<MdxParticleEmitter1Spawn>& outSpawns) {
    if (states.size() != model.particleEmitters1.size()) {
        MdxResetParticleEmitter1RuntimeStates(model, states);
    }

    const float clampedDt = std::max(0.0f, dt);
    int emitted = 0;

    for (size_t i = 0; i < model.particleEmitters1.size(); ++i) {
        const MdxParticleEmitter1& emitter = model.particleEmitters1[i];
        if (emitter.internalModelPath.empty()) {
            states[i].emissionAccum = 0.0f;
            continue;
        }

        MdxParticleEmitter1Sample sample;
        MdxSampleParticleEmitter1(emitter, &model, frame, elapsedMs, sample,
                                  animator ? animator->GetCurrentSequence() : -1);

        const bool emitterVisible = !animator || animator->IsObjectVisible(emitter.objectId);
        if (emitterVisible && sample.visibility > 0.0f && sample.emissionRate > 0.0f && clampedDt > 0.0f) {
            states[i].emissionAccum += sample.emissionRate * clampedDt;
        }

        const int emitCount = (int)states[i].emissionAccum;
        if (emitCount <= 0) {
            continue;
        }
        states[i].emissionAccum -= (float)emitCount;

        Mat4 localTransform = Mat4::Identity();
        Vec3 emitterScale = {1, 1, 1};
        if (animator) {
            Mat4 animatedTransform;
            Vec3 animatedScale;
            if (animator->GetObjectWorldTransformYUp(emitter.objectId, animatedTransform, &animatedScale)) {
                localTransform = animatedTransform;
                emitterScale = animatedScale;
            }
        }

        for (int count = 0; count < emitCount; ++count) {
            const uint32_t spawnSerial = states[i].spawnSerial++;
            MdxParticleEmitter1Spawn spawn;
            spawn.objectId = emitter.objectId;
            spawn.modelPath = emitter.internalModelPath;
            Vec3 localPosition = {localTransform.m[12], localTransform.m[13], localTransform.m[14]};
            if (animator) {
                Vec3 worldLocation;
                if (animator->GetObjectWorldLocationYUp(emitter.objectId, worldLocation)) {
                    localPosition = worldLocation;
                }
            }
            spawn.localTransform = BuildInternalModelTransform(localPosition, emitterScale, spawnSerial);
            spawn.localPosition = localPosition;
            spawn.velocity = BuildEmitterVelocity(emitter, sample, localTransform, emitterScale, spawnSerial);
            spawn.gravity = sample.gravity * emitterScale.z;
            spawn.lifeSpan = sample.lifeSpan;
            outSpawns.push_back(std::move(spawn));
            ++emitted;
        }
    }

    return emitted;
}

void MdxAdvanceParticleEmitter1SpawnTransform(Mat4& transform,
                                              Vec3& velocity,
                                              float gravity,
                                              float dt) {
    const float clampedDt = std::max(0.0f, dt);
    if (clampedDt <= 0.0f) {
        return;
    }

    velocity.y -= gravity * clampedDt;
    transform.m[12] += velocity.x * clampedDt;
    transform.m[13] += velocity.y * clampedDt;
    transform.m[14] += velocity.z * clampedDt;
}
