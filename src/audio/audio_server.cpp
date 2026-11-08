#include "audio/audio_server.h"

#include <algorithm>
#include <cmath>

#include "ecs/csimulation.h"
#include "ecs/sim_events.h"
#include "ecs/unit.h"

namespace audio {

AudioServer& AudioServer::Instance() {
    static AudioServer instance;
    return instance;
}

float AudioServer::LinearToDb(float linear) {
    if (linear <= 0.00001f) {
        return -100.0f;
    }
    return 20.0f * std::log10(linear);
}

float AudioServer::DbToLinear(float db) {
    if (db <= -100.0f) {
        return 0.0f;
    }
    return std::pow(10.0f, db / 20.0f);
}

AudioServer::AudioServer() {
    Reset();
}

void AudioServer::Reset() {
    buses_[static_cast<size_t>(AudioBusId::Master)]    = {AudioBusId::Master,    AudioBusId::Master, "Master",    1.0f, false, false};
    buses_[static_cast<size_t>(AudioBusId::Music)]     = {AudioBusId::Music,     AudioBusId::Master, "Music",     0.8f, false, false};
    buses_[static_cast<size_t>(AudioBusId::Ambient)]   = {AudioBusId::Ambient,   AudioBusId::Master, "Ambient",   0.6f, false, false};
    buses_[static_cast<size_t>(AudioBusId::SFX)]       = {AudioBusId::SFX,       AudioBusId::Master, "SFX",       1.0f, false, false};
    buses_[static_cast<size_t>(AudioBusId::UnitVoice)] = {AudioBusId::UnitVoice, AudioBusId::Master, "UnitVoice", 1.0f, false, false};
    buses_[static_cast<size_t>(AudioBusId::UI)]        = {AudioBusId::UI,        AudioBusId::Master, "UI",        1.0f, false, false};

    listener_ = AudioListener3D{};
    duckingEnabled_ = true;
    duckingGain_ = 0.65f;
    maxActiveVoices_ = 32;
    maxIdenticalClipConcurrency_ = 4;
    minClipCooldownSec_ = 0.035f;
    currentTimeSec_ = 0.0f;
    nextVoiceId_ = 1;
    activeVoices_.clear();
    lastClipPlayTimeSec_.clear();
    stats_ = AudioServerStats{};
}

void AudioServer::SetBusVolumeLinear(AudioBusId bus, float linear) {
    const size_t idx = static_cast<size_t>(bus);
    if (idx < kBusCount) {
        buses_[idx].volumeLinear = std::clamp(linear, 0.0f, 2.0f);
    }
}

float AudioServer::GetBusVolumeLinear(AudioBusId bus) const {
    const size_t idx = static_cast<size_t>(bus);
    return (idx < kBusCount) ? buses_[idx].volumeLinear : 0.0f;
}

void AudioServer::SetBusVolumeDb(AudioBusId bus, float db) {
    SetBusVolumeLinear(bus, DbToLinear(db));
}

float AudioServer::GetBusVolumeDb(AudioBusId bus) const {
    return LinearToDb(GetBusVolumeLinear(bus));
}

void AudioServer::SetBusMuted(AudioBusId bus, bool muted) {
    const size_t idx = static_cast<size_t>(bus);
    if (idx < kBusCount) {
        buses_[idx].muted = muted;
    }
}

bool AudioServer::IsBusMuted(AudioBusId bus) const {
    const size_t idx = static_cast<size_t>(bus);
    return (idx < kBusCount) ? buses_[idx].muted : true;
}

void AudioServer::SetBusSolo(AudioBusId bus, bool solo) {
    const size_t idx = static_cast<size_t>(bus);
    if (idx < kBusCount && bus != AudioBusId::Master) {
        buses_[idx].solo = solo;
    }
}

bool AudioServer::IsBusSolo(AudioBusId bus) const {
    const size_t idx = static_cast<size_t>(bus);
    return (idx < kBusCount) ? buses_[idx].solo : false;
}

int AudioServer::GetActiveVoiceCountOnBus(AudioBusId bus) const {
    int count = 0;
    for (const auto& v : activeVoices_) {
        if (v.bus == bus) {
            ++count;
        }
    }
    return count;
}

float AudioServer::ComputeEffectiveBusGain(AudioBusId bus) const {
    const size_t idx = static_cast<size_t>(bus);
    if (idx >= kBusCount) {
        return 0.0f;
    }
    const AudioBusState& master = buses_[static_cast<size_t>(AudioBusId::Master)];
    if (master.muted) {
        return 0.0f;
    }
    if (bus == AudioBusId::Master) {
        return master.volumeLinear;
    }

    const AudioBusState& state = buses_[idx];
    if (state.muted) {
        return 0.0f;
    }

    // Check if any non-Master bus has Solo enabled
    bool anySolo = false;
    for (size_t i = 1; i < kBusCount; ++i) {
        if (buses_[i].solo) {
            anySolo = true;
            break;
        }
    }
    if (anySolo && !state.solo) {
        return 0.0f;
    }

    float gain = master.volumeLinear * state.volumeLinear;

    // Sidechain ducking: if UnitVoice currently has active voices, duck SFX and Music
    if (duckingEnabled_ && (bus == AudioBusId::SFX || bus == AudioBusId::Music)) {
        if (GetActiveVoiceCountOnBus(AudioBusId::UnitVoice) > 0) {
            gain *= duckingGain_;
        }
    }
    return gain;
}

void AudioServer::SetListenerPosition(const Vec3& pos) {
    listener_.position = pos;
}

void AudioServer::SetListenerOrientation(const Vec3& forward, const Vec3& up) {
    const float fLen = std::sqrt(Dot(forward, forward));
    listener_.forward = (fLen > 1e-6f) ? (forward * (1.0f / fLen)) : Vec3(0.0f, 0.0f, -1.0f);

    const float uLen = std::sqrt(Dot(up, up));
    listener_.up = (uLen > 1e-6f) ? (up * (1.0f / uLen)) : Vec3(0.0f, 1.0f, 0.0f);

    const Vec3 r = Cross(listener_.forward, listener_.up);
    const float rLen = std::sqrt(Dot(r, r));
    listener_.right = (rLen > 1e-6f) ? (r * (1.0f / rLen)) : Vec3(1.0f, 0.0f, 0.0f);
}

SpatialEvalResult AudioServer::EvaluateSpatial3D(const AudioSpatialConfig& spatial) const {
    SpatialEvalResult res;
    if (!spatial.is3D) {
        res.distance = 0.0f;
        res.attenuationGain = 1.0f;
        res.stereoPan = 0.0f;
        res.culledByDistance = false;
        return res;
    }

    const Vec3 delta = spatial.position - listener_.position;
    const float dist = std::sqrt(Dot(delta, delta));
    res.distance = dist;

    const float cutoff = (spatial.distanceCutoff > 0.0f)
        ? spatial.distanceCutoff
        : ((spatial.maxDistance > 0.0f) ? spatial.maxDistance : 3000.0f);
    if (cutoff > 0.0f && dist > cutoff) {
        res.attenuationGain = 0.0f;
        res.culledByDistance = true;
        return res;
    }

    const float minD = std::max(0.0f, spatial.minDistance);
    const float maxD = std::max(minD + 1.0f, (spatial.maxDistance > 0.0f) ? spatial.maxDistance : cutoff);

    if (dist <= minD) {
        res.attenuationGain = 1.0f;
    } else if (dist >= maxD) {
        res.attenuationGain = 0.0f;
        res.culledByDistance = true;
        return res;
    } else {
        const float t = (dist - minD) / (maxD - minD);
        const float rolloff = std::max(0.1f, spatial.rolloffFactor);
        res.attenuationGain = std::pow(std::clamp(1.0f - t, 0.0f, 1.0f), rolloff);
    }

    if (dist > 1e-4f) {
        const Vec3 dir = delta * (1.0f / dist);
        res.stereoPan = std::clamp(Dot(dir, listener_.right), -1.0f, 1.0f);
    }
    return res;
}

int AudioServer::SubmitVoice(const AudioVoiceRequest& req) {
    stats_.totalSubmitted++;

    const float busGain = ComputeEffectiveBusGain(req.bus);
    if (busGain <= 0.0001f) {
        stats_.culledByMutedBus++;
        return -1;
    }

    const SpatialEvalResult spatialRes = EvaluateSpatial3D(req.spatial);
    if (spatialRes.culledByDistance || spatialRes.attenuationGain <= 0.0001f) {
        stats_.culledByDistance++;
        return -1;
    }

    // Per-clip cooldown & identical clip concurrency check (prevents comb-filtering)
    if (!req.clipKey.empty()) {
        auto lastIt = lastClipPlayTimeSec_.find(req.clipKey);
        if (lastIt != lastClipPlayTimeSec_.end() &&
            (currentTimeSec_ - lastIt->second) < minClipCooldownSec_) {
            stats_.throttledByCooldownOrConcurrency++;
            return -1;
        }

        int sameClipCount = 0;
        for (const auto& v : activeVoices_) {
            if (v.clipKey == req.clipKey) {
                ++sameClipCount;
            }
        }
        if (sameClipCount >= maxIdenticalClipConcurrency_) {
            stats_.throttledByCooldownOrConcurrency++;
            return -1;
        }
    }

    const float candidateGain = std::clamp(req.baseVolume, 0.0f, 2.0f) * busGain * spatialRes.attenuationGain;

    // Polyphony limit & priority-based voice stealing
    if (static_cast<int>(activeVoices_.size()) >= maxActiveVoices_) {
        size_t victimIdx = activeVoices_.size();
        int lowestPriority = req.priority;
        float lowestGain = candidateGain;

        for (size_t i = 0; i < activeVoices_.size(); ++i) {
            const auto& v = activeVoices_[i];
            if (v.priority < lowestPriority ||
                (v.priority == lowestPriority && v.finalGain < lowestGain)) {
                victimIdx = i;
                lowestPriority = v.priority;
                lowestGain = v.finalGain;
            }
        }

        if (victimIdx >= activeVoices_.size()) {
            stats_.droppedByPolyphony++;
            return -1;
        }
        activeVoices_.erase(activeVoices_.begin() + static_cast<std::ptrdiff_t>(victimIdx));
        stats_.voicesStolen++;
    }

    const int id = nextVoiceId_++;
    ActiveAudioVoice voice;
    voice.voiceId = id;
    voice.clipKey = req.clipKey;
    voice.bus = req.bus;
    voice.baseVolume = req.baseVolume;
    voice.finalGain = candidateGain;
    voice.stereoPan = spatialRes.stereoPan;
    voice.pitch = req.pitch;
    voice.priority = req.priority;
    voice.elapsedSec = 0.0f;
    voice.durationSec = std::max(0.05f, req.durationSec);
    voice.looping = req.looping;
    voice.spatial = req.spatial;

    activeVoices_.push_back(std::move(voice));
    if (!req.clipKey.empty()) {
        lastClipPlayTimeSec_[req.clipKey] = currentTimeSec_;
    }

    // If a UnitVoice started and ducking is enabled, immediately refresh active SFX/Music gains
    if (req.bus == AudioBusId::UnitVoice && duckingEnabled_) {
        RefreshActiveVoiceGains(/*updateStereoPan=*/false);
    }

    stats_.totalAccepted++;
    return id;
}

bool AudioServer::StopVoice(int voiceId) {
    for (size_t i = 0; i < activeVoices_.size(); ++i) {
        if (activeVoices_[i].voiceId == voiceId) {
            activeVoices_.erase(activeVoices_.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

void AudioServer::StopAllVoices() {
    activeVoices_.clear();
}

void AudioServer::Update(float dt) {
    const float safeDt = std::max(0.0f, dt);
    currentTimeSec_ += safeDt;

    for (int i = static_cast<int>(activeVoices_.size()) - 1; i >= 0; --i) {
        ActiveAudioVoice& v = activeVoices_[static_cast<size_t>(i)];
        v.elapsedSec += safeDt;
        if (!v.looping && v.elapsedSec >= v.durationSec) {
            activeVoices_.erase(activeVoices_.begin() + i);
            continue;
        }
    }

    RefreshActiveVoiceGains(/*updateStereoPan=*/true);
}

void AudioServer::RefreshActiveVoiceGains(bool updateStereoPan) {
    if (activeVoices_.empty()) return;
    std::array<float, kBusCount> busGains{};
    for (size_t b = 0; b < kBusCount; ++b) {
        busGains[b] = ComputeEffectiveBusGain(static_cast<AudioBusId>(b));
    }
    for (auto& v : activeVoices_) {
        const SpatialEvalResult sp = EvaluateSpatial3D(v.spatial);
        v.finalGain = v.baseVolume * busGains[static_cast<size_t>(v.bus)] * sp.attenuationGain;
        if (updateStereoPan) {
            v.stereoPan = sp.stereoPan;
        }
    }
}

const ActiveAudioVoice* AudioServer::FindActiveVoice(int voiceId) const {
    for (const auto& v : activeVoices_) {
        if (v.voiceId == voiceId) {
            return &v;
        }
    }
    return nullptr;
}

void AudioServer::BindSimulationEvents(CSimulation& sim) {
    UnbindSimulationEvents(sim);

    damagedSubId_ = sim.Events().onUnitDamage.Connect(
        [this, &sim](int unitHandle, int sourceHandle, float damageAmount) {
            if (damageAmount <= 0.0f) {
                return;
            }
            const CUnit* target = sim.GetUnit(unitHandle);
            AudioVoiceRequest req;
            std::string resolvedKey;
            if (const auto* mod = sim.GetSimulationModule()) {
                mod->OnResolveUnitCombatSound(sim, unitHandle, sourceHandle, /*isDeath=*/false, resolvedKey);
            }
            req.clipKey = !resolvedKey.empty()
                ? resolvedKey
                : ("Sound/Units/Combat/MetalMediumSlice_" + std::to_string(unitHandle));
            req.bus = AudioBusId::SFX;
            req.baseVolume = 0.85f;
            req.priority = 50;
            req.durationSec = 0.45f;
            req.spatial.is3D = true;
            if (target) {
                req.spatial.position = Vec3(target->GetX(), target->GetFlyHeight(), target->GetY());
            }
            req.spatial.minDistance = 300.0f;
            req.spatial.maxDistance = 2400.0f;
            req.spatial.distanceCutoff = 2800.0f;
            if (SubmitVoice(req) > 0) {
                stats_.simDamageSoundsTriggered++;
            }
        });

    diedSubId_ = sim.Events().onUnitDeath.Connect(
        [this, &sim](int unitHandle, int killerHandle, int /*ownerPlayerIndex*/) {
            const CUnit* dyingUnit = sim.GetUnit(unitHandle);
            AudioVoiceRequest req;
            std::string resolvedKey;
            if (const auto* mod = sim.GetSimulationModule()) {
                mod->OnResolveUnitCombatSound(sim, unitHandle, killerHandle, /*isDeath=*/true, resolvedKey);
            }
            req.clipKey = !resolvedKey.empty()
                ? resolvedKey
                : ("Sound/Units/Death/" +
                   (dyingUnit ? dyingUnit->GetTypeId().ToString() : std::string("unit")) +
                   "_" + std::to_string(unitHandle));
            req.bus = AudioBusId::UnitVoice;
            req.baseVolume = 1.0f;
            req.priority = 80;
            req.durationSec = 1.25f;
            req.spatial.is3D = true;
            if (dyingUnit) {
                req.spatial.position = Vec3(dyingUnit->GetX(), dyingUnit->GetFlyHeight(), dyingUnit->GetY());
            }
            req.spatial.minDistance = 400.0f;
            req.spatial.maxDistance = 2800.0f;
            req.spatial.distanceCutoff = 3200.0f;
            if (SubmitVoice(req) > 0) {
                stats_.simDeathSoundsTriggered++;
            }
        });
}

void AudioServer::UnbindSimulationEvents(CSimulation& sim) {
    if (damagedSubId_ != 0) {
        sim.Events().onUnitDamage.Disconnect(damagedSubId_);
        damagedSubId_ = 0;
    }
    if (diedSubId_ != 0) {
        sim.Events().onUnitDeath.Disconnect(diedSubId_);
        diedSubId_ = 0;
    }
}

} // namespace audio
