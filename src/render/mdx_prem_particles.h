#pragma once

#include "model/mdx.h"
#include "render/math.h"

#include <string>
#include <vector>

class MdxAnimator;

struct MdxParticleEmitter1RuntimeState {
    float emissionAccum = 0.0f;
    uint32_t spawnSerial = 0;
};

struct MdxParticleEmitter1Spawn {
    int objectId = -1;
    std::string modelPath;
    Mat4 localTransform = Mat4::Identity();
    Vec3 localPosition = {0, 0, 0};
    Vec3 velocity = {0, 0, 0};
    float gravity = 0.0f;
    float lifeSpan = 0.0f;
};

void MdxResetParticleEmitter1RuntimeStates(const MdxModel& model,
                                           std::vector<MdxParticleEmitter1RuntimeState>& states);

int MdxCollectParticleEmitter1Spawns(const MdxModel& model,
                                     const MdxAnimator* animator,
                                     uint32_t frame,
                                     float elapsedMs,
                                     float dt,
                                     std::vector<MdxParticleEmitter1RuntimeState>& states,
                                     std::vector<MdxParticleEmitter1Spawn>& outSpawns);

void MdxAdvanceParticleEmitter1SpawnTransform(Mat4& transform,
                                              Vec3& velocity,
                                              float gravity,
                                              float dt);
