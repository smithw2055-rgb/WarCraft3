#include "AckSoundBank.h"

#include <cstdlib>

#include "audio/AudioSystem.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "resource/mpq_storm.h"

bool AckSoundBank::LoadFromMpq(const MpqManager& mpq, const std::string& slkPath) {
    std::vector<uint8_t> buf;
    if (!mpq.ReadFile(slkPath, buf) || buf.empty()) return false;
    SlkTable slk;
    if (!slk.Load(buf.data(), buf.size())) return false;
    ackSlk_ = std::move(slk);
    loaded_ = true;
    return true;
}

void AckSoundBank::LoadFromSlk(const SlkTable& slk) {
    ackSlk_ = slk;
    loaded_ = slk.Rows() > 0;
}

UnitAckSoundDesc AckSoundBank::Resolve(const std::string& soundName,
                                       const std::string& kind) const {
    if (!loaded_) return UnitAckSoundDesc{};
    return UnitSoundset::Resolve(ackSlk_, soundName, kind);
}

namespace {

std::string SwapExtension(const std::string& path, const char* ext) {
    size_t dot = path.find_last_of('.');
    std::string base = (dot == std::string::npos) ? path : path.substr(0, dot);
    return base + ext;
}

} // namespace

int AckSoundBank::PlayResponse(CSimulation& sim, CUnit* unit, const std::string& kind) {
    if (!loaded_ || !unit || kind.empty()) return -1;
    AudioSystem* audio = sim.GetAudioSystem();
    if (!audio) return -1;
    const CUnitType* type = unit->GetUnitType();
    const std::string soundName = type ? type->GetUnitSound() : std::string();
    if (soundName.empty()) return -1;
    UnitAckSoundDesc desc = UnitSoundset::Resolve(ackSlk_, soundName, kind);
    if (desc.files.empty()) return -1;
    const std::string file = UnitSoundset::PickFile(desc, std::rand());
    const float volume = desc.volume <= 0.0f ? 1.0f : desc.volume;
    int id = audio->Play3DPath(file, unit->GetX(), unit->GetY(), 0.0f, volume);
    if (id < 0) {
        // Retail rows usually name .wav; fall back like ref createSound.
        id = audio->Play3DPath(SwapExtension(file, ".mp3"), unit->GetX(), unit->GetY(), 0.0f, volume);
    }
    if (id < 0) {
        id = audio->Play3DPath(SwapExtension(file, ".flac"), unit->GetX(), unit->GetY(), 0.0f, volume);
    }
    return id;
}
