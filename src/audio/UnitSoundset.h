#pragma once
// Unit response sound sets (ref UnitSound / UnitSoundset).
//
// Retail data: Units\UnitAckSounds.slk rows keyed `<SoundSet><Kind>` where
// Kind is What|Pissed|YesAttack|Yes|Ready|Warcry. Each row carries
// FileNames (comma list), DirectoryBase, Volume (0..127), Pitch,
// PitchVariance (1.0 means 0), MinDistance/MaxDistance/DistanceCutoff and
// Flags (LOOPING). This header resolves those rows to plain data; the audio
// backend picks an entry to play (random response like ref
// playUnitResponse).
#include <cstdlib>
#include <string>
#include <vector>

class SlkTable;

struct UnitAckSoundDesc {
    bool found = false;
    std::vector<std::string> files; // DirectoryBase + file name each
    float volume = 1.0f;            // 0..1 (Volume/127)
    float pitch = 0.0f;
    float pitchVariance = 0.0f;
    float minDistance = 0.0f;
    float maxDistance = 0.0f;
    float distanceCutoff = 0.0f;
    bool looping = false;
};

class UnitSoundset {
public:
    static const char* Kinds[6];

    // Resolve one `<soundName><kind>` row. Layout-agnostic: the key may sit
    // in any column (retail SLKs vary); field columns are found by header
    // name via FindColumn.
    static UnitAckSoundDesc Resolve(const SlkTable& slk,
                                    const std::string& soundName,
                                    const std::string& kind);

    // All six kinds for a sound set name (What/Pissed/YesAttack/Yes/Ready/
    // Warcry order matching Kinds).
    static std::vector<UnitAckSoundDesc> ResolveAll(const SlkTable& slk,
                                                    const std::string& soundName);

    // Deterministic pick (index % files), mirroring ref random response
    // without dragging RNG into data code. Empty when nothing resolved.
    static std::string PickFile(const UnitAckSoundDesc& desc, int index);

private:
    static std::vector<std::string> SplitList(const std::string& text);
};
