#pragma once

class TxtTable;

struct CGameplayConstants {
    float gameDayLength = 24.0f;
    float gameDayHours = 24.0f;
    float dawnTimeGameHours = 6.0f;
    float duskTimeGameHours = 18.0f;
    float maxCollisionRadius = 512.0f;
    float deathTimeDefault = 1.0f;
    float decayTime = 8.0f;
    float boneDecayTime = 8.0f;
    float dissipateTime = 2.0f;
    float structureDecayTime = 8.0f;

    CGameplayConstants();
    explicit CGameplayConstants(const TxtTable& miscData);
};
