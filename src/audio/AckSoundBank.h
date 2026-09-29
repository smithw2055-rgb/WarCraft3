#pragma once
// Response-sound bank (ref UnitSoundset usage in RenderUnit): loads
// Units\UnitAckSounds.slk once, then resolves `<SoundSet><Kind>` rows and
// plays them at the unit's position via AudioSystem::Play3DPath.
//
// Playback never touches the sim RNG (sound choice uses rand()); a missing
// bank, unknown set, or headless audio all degrade to a -1 no-op so script
// call sites stay unconditional.
#include <string>

#include "audio/UnitSoundset.h"
#include "resource/slk.h"

class AudioSystem;
class CSimulation;
class CUnit;
class MpqManager;

class AckSoundBank {
public:
    static constexpr const char* kAckSlkPath = "Units\\UnitAckSounds.slk";

    bool LoadFromMpq(const MpqManager& mpq, const std::string& slkPath = kAckSlkPath);
    void LoadFromSlk(const SlkTable& slk);

    bool HasData() const { return loaded_; }
    UnitAckSoundDesc Resolve(const std::string& soundName, const std::string& kind) const;

    // Resolve + play one response file at the unit. Returns the audio id,
    // or -1 when there is nothing to play (no bank / unknown set / no
    // audio backend). Extension fallback (.wav -> .mp3 -> .flac) mirrors
    // ref UnitSound.createSound.
    int PlayResponse(CSimulation& sim, CUnit* unit, const std::string& kind);

private:
    SlkTable ackSlk_;
    bool loaded_ = false;
};
