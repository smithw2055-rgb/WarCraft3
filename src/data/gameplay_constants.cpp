#include "gameplay_constants.h"
#include "resource/slk.h"

CGameplayConstants::CGameplayConstants()
    : gameDayLength(24.0f), gameDayHours(24.0f),
      dawnTimeGameHours(6.0f), duskTimeGameHours(18.0f),
      maxCollisionRadius(512.0f), deathTimeDefault(1.0f) {}

CGameplayConstants::CGameplayConstants(const TxtTable& miscData) {
    gameDayLength = miscData.GetFloat("GameConstants", "GameDayLength", 24.0f);
    gameDayHours = miscData.GetFloat("GameConstants", "GameDayHours", 24.0f);
    dawnTimeGameHours = miscData.GetFloat("GameConstants", "DawnTime", 6.0f);
    duskTimeGameHours = miscData.GetFloat("GameConstants", "DuskTime", 18.0f);
    maxCollisionRadius = miscData.GetFloat("GameConstants", "MaxCollisionRadius", 512.0f);
    deathTimeDefault = miscData.GetFloat("GameConstants", "DeathTime", 1.0f);
}
