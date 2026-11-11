#pragma once
// ============================================================================
// AudioServer & Pluggable IAudioBackend Architecture
// (Inspired by Godot 4 AudioServer & Unity AudioMixer)
//
// Unifies logical mixing buses, 3D spatial attenuation/panning, sidechain voice
// ducking, priority voice stealing, and hardware/headless audio backends under
// a single canonical `voiceId` handle space (`voiceId -> backendSoundId`).
// ============================================================================
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "render/math.h"

class CSimulation;
class MpqManager;

namespace audio {

// ============================================================================
// 1. Hierarchical Mixing Bus Architecture
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
    int backendSoundId = -1; // Mapped hardware/mock backend sound handle (-1 when pure virtual)
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

struct CachedSoundClip {
    std::vector<uint8_t> data;
    std::string path;
};

// ============================================================================
// 4. Pluggable Hardware / Headless Audio Backend Contract
// ============================================================================
class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    virtual const char* BackendName() const = 0;
    virtual bool Init() = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    virtual int PlayMemorySound(const uint8_t* data,
                                size_t size,
                                float volume,
                                float x,
                                float y,
                                float z,
                                bool is3D,
                                float pitch,
                                float minDist,
                                float maxDist,
                                bool looping = false,
                                bool isMusic = false) = 0;

    virtual void StopSound(int backendSoundId) = 0;
    virtual void StopAllSounds(bool includeMusic = false) = 0;
    virtual bool IsSoundPlaying(int backendSoundId) const = 0;
    virtual void SetSoundVolume(int backendSoundId, float volume) = 0;
    virtual void SetMasterVolume(float volume) = 0;
    virtual void SetListenerPosition(float x, float y, float z) = 0;
    virtual void SetListenerOrientation(float fx, float fy, float fz, float ux, float uy, float uz) = 0;
    virtual void PollGarbageCollect() = 0;
    virtual size_t GetActiveSoundCount() const = 0;
};

class NullAudioBackend final : public IAudioBackend {
public:
    const char* BackendName() const override { return "NullAudioBackend"; }

    bool Init() override {
        initialized_ = true;
        return true;
    }

    void Shutdown() override {
        activeSounds_.clear();
        initialized_ = false;
    }

    bool IsInitialized() const override { return initialized_; }

    int PlayMemorySound(const uint8_t* data,
                        size_t size,
                        float volume,
                        float /*x*/,
                        float /*y*/,
                        float /*z*/,
                        bool /*is3D*/,
                        float /*pitch*/,
                        float /*minDist*/,
                        float /*maxDist*/,
                        bool /*looping*/,
                        bool isMusic) override {
        if (!initialized_ || (!data && size > 0)) {
            return -1;
        }
        const int id = nextId_++;
        activeSounds_[id] = {volume * masterVol_, isMusic};
        return id;
    }

    void StopSound(int backendSoundId) override {
        activeSounds_.erase(backendSoundId);
    }

    void StopAllSounds(bool includeMusic = false) override {
        if (includeMusic) {
            activeSounds_.clear();
            return;
        }
        for (auto it = activeSounds_.begin(); it != activeSounds_.end(); ) {
            if (!it->second.isMusic) {
                it = activeSounds_.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool IsSoundPlaying(int backendSoundId) const override {
        return activeSounds_.find(backendSoundId) != activeSounds_.end();
    }

    void SetSoundVolume(int backendSoundId, float volume) override {
        auto it = activeSounds_.find(backendSoundId);
        if (it != activeSounds_.end()) {
            it->second.volume = volume * masterVol_;
        }
    }

    void SetMasterVolume(float volume) override {
        masterVol_ = std::clamp(volume, 0.0f, 1.0f);
    }

    void SetListenerPosition(float x, float y, float z) override {
        listenerPos_ = Vec3(x, y, z);
    }

    void SetListenerOrientation(float fx, float fy, float fz, float ux, float uy, float uz) override {
        listenerForward_ = Vec3(fx, fy, fz);
        listenerUp_ = Vec3(ux, uy, uz);
    }

    void PollGarbageCollect() override {}

    size_t GetActiveSoundCount() const override { return activeSounds_.size(); }
    int GetLastSoundId() const { return nextId_ - 1; }
    float GetSoundVolume(int backendSoundId) const {
        auto it = activeSounds_.find(backendSoundId);
        return (it != activeSounds_.end()) ? it->second.volume : -1.0f;
    }

private:
    struct NullSoundSlot {
        float volume = 1.0f;
        bool isMusic = false;
    };

    bool initialized_ = false;
    int nextId_ = 1;
    float masterVol_ = 1.0f;
    Vec3 listenerPos_ = Vec3(0.0f, 0.0f, 0.0f);
    Vec3 listenerForward_ = Vec3(0.0f, 0.0f, -1.0f);
    Vec3 listenerUp_ = Vec3(0.0f, 1.0f, 0.0f);
    std::unordered_map<int, NullSoundSlot> activeSounds_;
};

class MiniaudioBackend final : public IAudioBackend {
public:
    MiniaudioBackend();
    ~MiniaudioBackend() override;

    const char* BackendName() const override { return "MiniaudioBackend"; }
    bool Init() override;
    void Shutdown() override;
    bool IsInitialized() const override { return engine_ != nullptr; }

    int PlayMemorySound(const uint8_t* data,
                        size_t size,
                        float volume,
                        float x,
                        float y,
                        float z,
                        bool is3D,
                        float pitch,
                        float minDist,
                        float maxDist,
                        bool looping = false,
                        bool isMusic = false) override;

    void StopSound(int backendSoundId) override;
    void StopAllSounds(bool includeMusic = false) override;
    bool IsSoundPlaying(int backendSoundId) const override;
    void SetSoundVolume(int backendSoundId, float volume) override;
    void SetMasterVolume(float volume) override;
    void SetListenerPosition(float x, float y, float z) override;
    void SetListenerOrientation(float fx, float fy, float fz, float ux, float uy, float uz) override;
    void PollGarbageCollect() override;
    size_t GetActiveSoundCount() const override { return activeSounds_.size(); }

private:
    struct ActiveHwSound {
        int id = 0;
        void* sound = nullptr;
        void* decoder = nullptr;
        bool isMusic = false;
    };

    void* engine_ = nullptr;
    std::vector<ActiveHwSound> activeSounds_;
    int nextSoundId_ = 1;
    float masterVolume_ = 1.0f;
};

// ============================================================================
// 5. AudioServer — Mixing Buses, 3D Spatialization, Voice Stealing & Playback
// ============================================================================
class AudioServer {
public:
    static AudioServer& Instance();

    AudioServer();
    ~AudioServer();

    // Decibel <-> Linear conversion helpers
    static float LinearToDb(float linear);
    static float DbToLinear(float db);

    void Reset();

    // Backend & VFS attachment
    void SetBackend(std::shared_ptr<IAudioBackend> backend);
    std::shared_ptr<IAudioBackend> GetBackend() const { return backend_; }
    const char* GetBackendName() const { return backend_ ? backend_->BackendName() : "None"; }
    bool InitHardwareBackend();
    void ShutdownBackend();
    bool IsBackendActive() const { return backend_ && backend_->IsInitialized(); }

    void SetMpqManager(MpqManager* mpq) { mpq_ = mpq; }
    MpqManager* GetMpqManager() const { return mpq_; }

    void SetHeadlessSimulationEnabled(bool enabled) { headlessSimulationEnabled_ = enabled; }
    bool IsHeadlessSimulationEnabled() const { return headlessSimulationEnabled_; }

    // Clip cache & playback API (always returns canonical AudioServer `voiceId`!)
    bool LoadSound(const std::string& name, const std::string& mpqPath, MpqManager* mpq);
    bool LoadSoundFromFile(const std::string& name, const std::string& filePath);

    int Play2D(const std::string& name, float volume = 1.0f);
    int Play3D(const std::string& name, float x, float y, float z, float volume = 1.0f);
    int Play3DPath(const std::string& mpqPath, float x, float y, float z, float volume = 1.0f);
    int Play3DEx(const std::string& name,
                 float x,
                 float y,
                 float z,
                 float volume = 1.0f,
                 float pitch = 1.0f,
                 float minDist = 0.0f,
                 float maxDist = 0.0f);
    int Play3DPathEx(const std::string& mpqPath,
                     float x,
                     float y,
                     float z,
                     float volume = 1.0f,
                     float pitch = 1.0f,
                     float minDist = 0.0f,
                     float maxDist = 0.0f);

    // Streaming music & synthesized MIDI day/night ambient loops
    bool PlayMusic(const std::string& mpqPath, MpqManager* mpq, float volume = 1.0f, bool loop = true);
    bool PlayMusicFromFile(const std::string& filePath, float volume = 1.0f, bool loop = true);
    void StopMusic();
    void SetMusicVolume(float vol);
    bool IsMusicPlaying() const;

    bool PlayMidiAmbient(const std::string& mpqPath, MpqManager* mpq, float volume = 0.5f);
    void StopMidiAmbient();
    bool IsMidiAmbientPlaying(const std::string& mpqPath) const;

    void SetMasterVolume(float vol);
    float GetMasterVolume() const { return GetBusVolumeLinear(AudioBusId::Master); }

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

    float ComputeEffectiveBusGain(AudioBusId bus) const;

    // 3D Listener control
    void SetListenerPosition(const Vec3& pos);
    void SetListenerOrientation(const Vec3& forward, const Vec3& up);
    const AudioListener3D& GetListener() const { return listener_; }

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

    // Submit a voice request; returns positive canonical voiceId if accepted, or -1
    int SubmitVoice(const AudioVoiceRequest& req);

    bool StopVoice(int voiceId);
    void StopAllVoices();

    void Update(float dt);

    const ActiveAudioVoice* FindActiveVoice(int voiceId) const;
    const std::vector<ActiveAudioVoice>& GetActiveVoices() const { return activeVoices_; }
    size_t GetActiveVoiceCount() const { return activeVoices_.size(); }
    int GetActiveVoiceCountOnBus(AudioBusId bus) const;

    void BindSimulationEvents(CSimulation& sim);
    void UnbindSimulationEvents(CSimulation& sim);

    const AudioServerStats& GetStats() const { return stats_; }
    void ResetStats() { stats_ = AudioServerStats{}; }

private:
    static constexpr size_t kBusCount = static_cast<size_t>(AudioBusId::Count);

    const CachedSoundClip* ResolveOrAutoLoadNamedClip(const std::string& name);
    const CachedSoundClip* ResolveOrLoadPathClip(const std::string& mpqPath);
    void BindBackendSoundToVoice(int voiceId, int backendSoundId);
    void RefreshActiveVoiceGains(bool updateStereoPan);

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

    std::shared_ptr<IAudioBackend> backend_;
    MpqManager* mpq_ = nullptr;
    bool headlessSimulationEnabled_ = false;
    std::unordered_map<std::string, CachedSoundClip> soundCache_;

    std::vector<uint8_t> musicData_;
    int musicBackendId_ = -1;
    float musicBaseVolume_ = 1.0f;

    std::vector<uint8_t> ambientWav_;
    int ambientBackendId_ = -1;
    std::string ambientPath_;
    float ambientVolume_ = 0.5f;
    std::unordered_map<std::string, std::vector<uint8_t>> midiCache_;
};

} // namespace audio
