#include "gameplay_constants.h"
#include "resource/slk.h"

CGameplayConstants::CGameplayConstants()
    : gameDayLength(24.0f), gameDayHours(24.0f),
      dawnTimeGameHours(6.0f), duskTimeGameHours(18.0f),
      maxCollisionRadius(512.0f), deathTimeDefault(1.0f),
      decayTime(8.0f), boneDecayTime(8.0f),
      dissipateTime(2.0f), structureDecayTime(8.0f) {}

CGameplayConstants::CGameplayConstants(const TxtTable& miscData) {
    auto readFloat = [&](const char* key, float def) {
        if (miscData.HasSection("Misc")) {
            return miscData.GetFloat("Misc", key, def);
        }
        return miscData.GetFloat("GameConstants", key, def);
    };

    gameDayLength = readFloat("GameDayLength", 24.0f);
    gameDayHours = readFloat("GameDayHours", 24.0f);
    dawnTimeGameHours = readFloat("DawnTime", readFloat("Dawn", 6.0f));
    duskTimeGameHours = readFloat("DuskTime", readFloat("Dusk", 18.0f));
    maxCollisionRadius = readFloat("MaxCollisionRadius", 512.0f);
    deathTimeDefault = readFloat("DeathTime", 1.0f);
    decayTime = readFloat("DecayTime", 8.0f);
    boneDecayTime = readFloat("BoneDecayTime", 8.0f);
    dissipateTime = readFloat("DissipateTime", 2.0f);
    structureDecayTime = readFloat("StructureDecayTime", 8.0f);
}
