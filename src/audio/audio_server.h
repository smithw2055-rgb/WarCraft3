#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "render/math.h"

class CSimulation;

namespace audio {

// ============================================================================
// 1. Hierarchical Mixing Bus Architecture
//    (Inspired by Godot 4 AudioServer & Unity AudioMixer)
// ============================================================================

enum class AudioBusId : uint8_t {
    Master    = 0,
    Music     = 1,
    Ambient   = 2,
    SFX       = 3,
    UnitVoice = 4,
    UI        = 5,
    Count     = 6
};

struct AudioBusState {
    AudioBusId id = AudioBusId::Master;
    AudioBusId parent = AudioBusId::Master;
    std::string name = "Master";
    float volumeLinear = 1.0f;
    bool muted = false;
    bool solo = false;
};

// ============================================================================
// 2. 3D Spatial Audio Listener & Emitter Parameters
// ============================================================================

struct AudioListener3D {
    Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
    Vec3 forward  = Vec3(0.0f, 0.0f, -1.0f);
    Vec3 up       = Vec3(0.0f, 1.0f, 0.0f);
    Vec3 right    = Vec3(1.0f, 0.0f, 0.0f);
};

struct AudioSpatialConfig {
    bool is3D = false;
    Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
    float minDistance = 300.0f;
    float maxDistance = 2400.0f;
    float distanceCutoff = 3000.0f;
    float rolloffFactor = 1.0f;
};

struct SpatialEvalResult {
    float distance = 0.0f;
    float attenuationGain = 1.0f;
    float stereoPan = 0.0f; // [-1.0 = full left, 0.0 = center, +1.0 = full right]
    bool culledByDistance = false;
};

// ============================================================================
// 3. Polyphony Voice Descriptor & Telemetry Stats
// ============================================================================

struct AudioVoiceRequest {
    std::string clipKey;
    AudioBusId bus = AudioBusId::SFX;
    float baseVolume = 1.0f;
    float pitch = 1.0f;
    int priority = 50; // Higher value = higher priority (UI=90, UnitVoice=80, SFX=50, Ambient=30)
    float durationSec = 1.2f;
    bool looping = false;
    AudioSpatialConfig spatial;
};

struct ActiveAudioVoice {
    int voiceId = 0;
    std::string clipKey;
    AudioBusId bus = AudioBusId::SFX;
    float baseVolume = 1.0f;
    float finalGain = 1.0f;
    float stereoPan = 0.0f;
    float pitch = 1.0f;
    int priority = 50;
    float elapsedSec = 0.0f;
    float durationSec = 1.2f;
    bool looping = false;
    AudioSpatialConfig spatial;
};

struct AudioServerStats {
    int totalSubmitted = 0;
    int totalAccepted = 0;
    int culledByDistance = 0;
    int culledByMutedBus = 0;
    int throttledByCooldownOrConcurrency = 0;
    int voicesStolen = 0;
    int droppedByPolyphony = 0;
    int simDamageSoundsTriggered = 0;
    int simDeathSoundsTriggered = 0;
};

// ============================================================================
// 4. AudioServer — Mixing Buses, 3D Spatialization, Voice Stealing & Sim Bridge
// ============================================================================

class AudioServer {
public:
    static AudioServer& Instance();

    AudioServer();
    ~AudioServer() = default;

    // Decibel <-> Linear conversion helpers
    static float LinearToDb(float linear);
    static float DbToLinear(float db);

    void Reset();

    // Bus control
    void SetBusVolumeLinear(AudioBusId bus, float linear);
    float GetBusVolumeLinear(AudioBusId bus) const;
    void SetBusVolumeDb(AudioBusId bus, float db);
    float GetBusVolumeDb(AudioBusId bus) const;

    void SetBusMuted(AudioBusId bus, bool muted);
    bool IsBusMuted(AudioBusId bus) const;

    void SetBusSolo(AudioBusId bus, bool solo);
    bool IsBusSolo(AudioBusId bus) const;

    // Sidechain ducking: when UnitVoice has active voices, attenuate SFX & Music by duckingGain_
    void SetVoiceDuckingEnabled(bool enabled, float duckingGain = 0.65f) {
        duckingEnabled_ = enabled;
        duckingGain_ = std::clamp(duckingGain, 0.1f, 1.0f);
    }
    bool IsVoiceDuckingEnabled() const { return duckingEnabled_; }

    // Computes final effective linear gain for a bus (accounting for Master, Mute, Solo & Ducking)
    float ComputeEffectiveBusGain(AudioBusId bus) const;

    // 3D Listener control
    void SetListenerPosition(const Vec3& pos);
    void SetListenerOrientation(const Vec3& forward, const Vec3& up);
    const AudioListener3D& GetListener() const { return listener_; }

    // Analytical 3D spatial evaluation (distance rolloff + cutoff + stereo pan)
    SpatialEvalResult EvaluateSpatial3D(const AudioSpatialConfig& spatial) const;

    // Polyphony & throttling configuration
    void SetMaxActiveVoices(int maxVoices) { maxActiveVoices_ = std::max(1, maxVoices); }
    int GetMaxActiveVoices() const { return maxActiveVoices_; }
    void SetMaxIdenticalClipConcurrency(int maxConcurrency) {
        maxIdenticalClipConcurrency_ = std::max(1, maxConcurrency);
    }
    void SetMinClipCooldownSec(float cooldownSec) {
        minClipCooldownSec_ = std::max(0.0f, cooldownSec);
    }

    // Submit a voice request; returns positive voiceId if accepted, or -1 if culled/throttled/dropped
    int SubmitVoice(const AudioVoiceRequest& req);

    bool StopVoice(int voiceId);
    void StopAllVoices();

    // Per-frame update: advances voice timers, removes expired non-looping voices,
    // and refreshes 3D spatial gain/pan & bus ducking for active voices
    void Update(float dt);

    const ActiveAudioVoice* FindActiveVoice(int voiceId) const;
    const std::vector<ActiveAudioVoice>& GetActiveVoices() const { return activeVoices_; }
    size_t GetActiveVoiceCount() const { return activeVoices_.size(); }
    int GetActiveVoiceCountOnBus(AudioBusId bus) const;

    // Event-driven bridge: subscribes to CSimulation::GetEventBus() (onUnitDamaged & onUnitDied)
    // to automatically emit spatialized 3D combat hit & unit death voices
    void BindSimulationEvents(CSimulation& sim);
    void UnbindSimulationEvents(CSimulation& sim);

    const AudioServerStats& GetStats() const { return stats_; }
    void ResetStats() { stats_ = AudioServerStats{}; }

private:
    static constexpr size_t kBusCount = static_cast<size_t>(AudioBusId::Count);

    std::array<AudioBusState, kBusCount> buses_;
    AudioListener3D listener_;

    bool duckingEnabled_ = true;
    float duckingGain_ = 0.65f;

    int maxActiveVoices_ = 32;
    int maxIdenticalClipConcurrency_ = 4;
    float minClipCooldownSec_ = 0.035f;
    float currentTimeSec_ = 0.0f;
    int nextVoiceId_ = 1;

    std::vector<ActiveAudioVoice> activeVoices_;
    std::unordered_map<std::string, float> lastClipPlayTimeSec_;
    AudioServerStats stats_;

    uint64_t damagedSubId_ = 0;
    uint64_t diedSubId_ = 0;
};

} // namespace audio
