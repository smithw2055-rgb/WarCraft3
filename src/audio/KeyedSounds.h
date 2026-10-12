#pragma once
#include <string>
#include <unordered_map>
#include "resource/slk.h"

class AudioSystem;
class MpqManager;

// KeyedSounds - UI sound effects from UI\SoundInfo\UISounds.slk.
// Ref Java handlers/w3x/ui/sound/KeyedSounds (MenuUI/War3MapViewer).
class KeyedSounds {
public:
    static constexpr const char* kSlkPath = "UI\\SoundInfo\\UISounds.slk";

    bool LoadFromMpq(const MpqManager& mpq, const std::string& slkPath = kSlkPath);
    // Play a keyed UI sound (e.g. "Click", "Error", "Build"). No-op when
    // the table or audio system is missing. Returns sound id or -1.
    int Play(AudioSystem* audio, const MpqManager* mpq, const std::string& key,
             float volume = 1.0f);

private:
    std::string ResolveFile(const std::string& key) const;

    SlkTable slk_;
    bool loaded_ = false;
    std::unordered_map<std::string, std::string> cache_;
};
