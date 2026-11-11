#include "audio/audio_server.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "audio/MidiFile.h"
#include "audio/MidiSynth.h"
#include "ecs/csimulation.h"
#include "ecs/sim_events.h"
#include "ecs/unit.h"
#include "resource/mpq_storm.h"
#include "resource/resource_server.h"

#define MINIAUDIO_IMPLEMENTATION
#include "third_party/miniaudio/miniaudio.h"

namespace audio {

// ============================================================================
// MiniaudioBackend Hardware Implementation
// ============================================================================

MiniaudioBackend::MiniaudioBackend() = default;

MiniaudioBackend::~MiniaudioBackend() {
    Shutdown();
}

bool MiniaudioBackend::Init() {
    if (engine_) {
        return true;
    }
    auto* eng = new ma_engine;
    ma_engine_config config = ma_engine_config_init();
    const ma_result result = ma_engine_init(&config, eng);
    if (result != MA_SUCCESS) {
        std::cerr << "MiniaudioBackend: ma_engine_init failed (" << result << ")" << std::endl;
        delete eng;
        return false;
    }

    ma_engine_listener_set_position(eng, 0, 0.0f, 0.0f, 0.0f);
    ma_engine_listener_set_world_up(eng, 0, 0.0f, 1.0f, 0.0f);
    engine_ = eng;
    std::cout << "MiniaudioBackend: initialized" << std::endl;
    return true;
}

void MiniaudioBackend::Shutdown() {
    StopAllSounds(/*includeMusic=*/true);
    if (engine_) {
        auto* eng = static_cast<ma_engine*>(engine_);
        ma_engine_uninit(eng);
        delete eng;
        engine_ = nullptr;
    }
}

int MiniaudioBackend::PlayMemorySound(const uint8_t* data,
                                      size_t size,
                                      float volume,
                                      float x,
                                      float y,
                                      float z,
                                      bool is3D,
                                      float pitch,
                                      float minDist,
                                      float maxDist,
                                      bool looping,
                                      bool isMusic) {
    if (!engine_ || !data || size == 0) {
        return -1;
    }
    auto* eng = static_cast<ma_engine*>(engine_);

    auto* decoder = new ma_decoder;
    ma_decoder_config decoderConfig = ma_decoder_config_init_default();
    ma_result result = ma_decoder_init_memory(data, size, &decoderConfig, decoder);
    if (result != MA_SUCCESS) {
        delete decoder;
        return -1;
    }

    auto* sound = new ma_sound;
    const ma_uint32 flags = is3D ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;
    result = ma_sound_init_from_data_source(
        eng, reinterpret_cast<ma_data_source*>(decoder), flags, nullptr, sound);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(decoder);
        delete decoder;
        delete sound;
        return -1;
    }

    if (is3D) {
        ma_sound_set_position(sound, x, y, z);
        if (minDist > 0.0f) {
            ma_sound_set_min_distance(sound, minDist);
        }
        if (maxDist > 0.0f) {
            ma_sound_set_max_distance(sound, maxDist);
        }
    }
    if (pitch > 0.0f && pitch != 1.0f) {
        ma_sound_set_pitch(sound, pitch);
    }
    ma_sound_set_looping(sound, looping ? MA_TRUE : MA_FALSE);
    // `ma_engine_set_volume` in `SetMasterVolume` already applies `masterVolume_` once.
    ma_sound_set_volume(sound, std::max(0.0f, volume));
    ma_sound_start(sound);

    const int id = nextSoundId_++;
    activeSounds_.push_back({id, sound, decoder, isMusic});
    return id;
}

void MiniaudioBackend::StopSound(int backendSoundId) {
    for (size_t i = 0; i < activeSounds_.size(); ++i) {
        if (activeSounds_[i].id == backendSoundId) {
            auto* snd = static_cast<ma_sound*>(activeSounds_[i].sound);
            auto* dec = static_cast<ma_decoder*>(activeSounds_[i].decoder);
            if (snd) {
                ma_sound_stop(snd);
                ma_sound_uninit(snd);
                delete snd;
            }
            if (dec) {
                ma_decoder_uninit(dec);
                delete dec;
            }
            activeSounds_.erase(activeSounds_.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
}

void MiniaudioBackend::StopAllSounds(bool includeMusic) {
    for (int i = static_cast<int>(activeSounds_.size()) - 1; i >= 0; --i) {
        ActiveHwSound& entry = activeSounds_[static_cast<size_t>(i)];
        if (!includeMusic && entry.isMusic) {
            continue;
        }
        auto* snd = static_cast<ma_sound*>(entry.sound);
        auto* dec = static_cast<ma_decoder*>(entry.decoder);
        if (snd) {
            ma_sound_stop(snd);
            ma_sound_uninit(snd);
            delete snd;
        }
        if (dec) {
            ma_decoder_uninit(dec);
            delete dec;
        }
        activeSounds_.erase(activeSounds_.begin() + i);
    }
}

bool MiniaudioBackend::IsSoundPlaying(int backendSoundId) const {
    for (const auto& entry : activeSounds_) {
        if (entry.id == backendSoundId) {
            const auto* snd = static_cast<const ma_sound*>(entry.sound);
            return snd && ma_sound_is_playing(snd);
        }
    }
    return false;
}

void MiniaudioBackend::SetSoundVolume(int backendSoundId, float volume) {
    for (auto& entry : activeSounds_) {
        if (entry.id == backendSoundId) {
            auto* snd = static_cast<ma_sound*>(entry.sound);
            if (snd) {
                ma_sound_set_volume(snd, std::max(0.0f, volume));
            }
            return;
        }
    }
}

void MiniaudioBackend::SetMasterVolume(float volume) {
    masterVolume_ = std::clamp(volume, 0.0f, 1.0f);
    if (engine_) {
        ma_engine_set_volume(static_cast<ma_engine*>(engine_), masterVolume_);
    }
}

void MiniaudioBackend::SetListenerPosition(float x, float y, float z) {
    if (engine_) {
        ma_engine_listener_set_position(static_cast<ma_engine*>(engine_), 0, x, y, z);
    }
}

void MiniaudioBackend::SetListenerOrientation(float fx, float fy, float fz,
                                              float ux, float uy, float uz) {
    if (engine_) {
        auto* eng = static_cast<ma_engine*>(engine_);
        ma_engine_listener_set_direction(eng, 0, fx, fy, fz);
        ma_engine_listener_set_world_up(eng, 0, ux, uy, uz);
    }
}

void MiniaudioBackend::PollGarbageCollect() {
    for (int i = static_cast<int>(activeSounds_.size()) - 1; i >= 0; --i) {
        ActiveHwSound& entry = activeSounds_[static_cast<size_t>(i)];
        if (entry.isMusic) {
            continue;
        }
        auto* snd = static_cast<ma_sound*>(entry.sound);
        if (snd && !ma_sound_is_playing(snd)) {
            auto* dec = static_cast<ma_decoder*>(entry.decoder);
            ma_sound_uninit(snd);
            delete snd;
            if (dec) {
                ma_decoder_uninit(dec);
                delete dec;
            }
            activeSounds_.erase(activeSounds_.begin() + i);
        }
    }
}

// ============================================================================
// AudioServer Implementation
// ============================================================================

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

AudioServer::~AudioServer() {
    ShutdownBackend();
}

void AudioServer::Reset() {
    // Ensure any active hardware/mock sounds, music, and MIDI ambient loops stop cleanly
    StopMusic();
    StopMidiAmbient();
    if (backend_) {
        backend_->StopAllSounds(/*includeMusic=*/true);
        backend_->SetMasterVolume(1.0f);
    }

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

void AudioServer::SetBackend(std::shared_ptr<IAudioBackend> backend) {
    if (backend_ && backend_ != backend) {
        StopMusic();
        StopMidiAmbient();
        StopAllVoices();
        backend_->Shutdown();
    }
    backend_ = std::move(backend);
    if (backend_) {
        backend_->SetMasterVolume(1.0f);
    }
}

bool AudioServer::InitHardwareBackend() {
    if (IsBackendActive()) {
        return true;
    }
    if (!backend_) {
        backend_ = std::make_shared<MiniaudioBackend>();
    }
    if (!backend_->Init()) {
        return false;
    }
    backend_->SetMasterVolume(1.0f);
    return true;
}

void AudioServer::ShutdownBackend() {
    StopAllVoices();
    StopMusic();
    StopMidiAmbient();
    soundCache_.clear();
    midiCache_.clear();
    if (backend_) {
        backend_->Shutdown();
        backend_.reset();
    }
}

bool AudioServer::LoadSound(const std::string& name, const std::string& mpqPath, MpqManager* mpq) {
    if (soundCache_.find(name) != soundCache_.end()) {
        return true;
    }

    std::vector<uint8_t> buf;
    bool loaded = false;
    if (auto clip = resource::ResourceServer::Instance().LoadAudioClip(mpqPath)) {
        if (!clip->bytes.empty()) {
            buf = clip->bytes;
            loaded = true;
        } else if (clip->isSilenceOverride) {
            return false;
        }
    }
    if (!loaded) {
        loaded = resource::ResourceServer::Instance().ReadRawBytes(mpqPath, buf);
    }
    if (!loaded && mpq) {
        loaded = mpq->ReadFile(mpqPath, buf);
    }
    if (!loaded || buf.empty()) {
        return false;
    }

    CachedSoundClip clipData;
    clipData.data = std::move(buf);
    clipData.path = mpqPath;
    soundCache_[name] = std::move(clipData);
    return true;
}

bool AudioServer::LoadSoundFromFile(const std::string& name, const std::string& filePath) {
    if (soundCache_.find(name) != soundCache_.end()) {
        return true;
    }

    FILE* f = std::fopen(filePath.c_str(), "rb");
    if (!f) {
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        std::fclose(f);
        return false;
    }

    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    if (std::fread(buf.data(), 1, static_cast<size_t>(sz), f) != static_cast<size_t>(sz)) {
        std::fclose(f);
        return false;
    }
    std::fclose(f);

    CachedSoundClip clipData;
    clipData.data = std::move(buf);
    clipData.path = filePath;
    soundCache_[name] = std::move(clipData);
    return true;
}

const CachedSoundClip* AudioServer::ResolveOrAutoLoadNamedClip(const std::string& name) {
    auto it = soundCache_.find(name);
    if (it != soundCache_.end()) {
        return &it->second;
    }
    if (mpq_) {
        std::string mpqPath = "Sound\\Units\\" + name + ".wav";
        if (!LoadSound(name, mpqPath, mpq_)) {
            mpqPath = "Sound\\Interface\\" + name + ".wav";
            LoadSound(name, mpqPath, mpq_);
        }
        it = soundCache_.find(name);
        if (it != soundCache_.end()) {
            return &it->second;
        }
    }
    return nullptr;
}

const CachedSoundClip* AudioServer::ResolveOrLoadPathClip(const std::string& mpqPath) {
    auto it = soundCache_.find(mpqPath);
    if (it != soundCache_.end()) {
        return &it->second;
    }
    if (!LoadSound(mpqPath, mpqPath, mpq_)) {
        return nullptr;
    }
    it = soundCache_.find(mpqPath);
    return (it != soundCache_.end()) ? &it->second : nullptr;
}

void AudioServer::BindBackendSoundToVoice(int voiceId, int backendSoundId) {
    for (auto& v : activeVoices_) {
        if (v.voiceId == voiceId) {
            v.backendSoundId = backendSoundId;
            return;
        }
    }
}

int AudioServer::Play2D(const std::string& name, float volume) {
    if (name.empty()) {
        return -1;
    }
    const bool hasBackend = IsBackendActive();
    if (!hasBackend && !headlessSimulationEnabled_) {
        return -1;
    }

    // Load-before-submit when backend is active: prevents ghost voices & false ducking!
    const CachedSoundClip* clip = nullptr;
    if (hasBackend) {
        clip = ResolveOrAutoLoadNamedClip(name);
        if (!clip) {
            if (!headlessSimulationEnabled_) {
                return -1;
            }
        }
    }

    AudioVoiceRequest req;
    req.clipKey = name;
    req.bus = (name.find("Click") != std::string::npos ||
               name.find("Error") != std::string::npos ||
               name.find("Interface") != std::string::npos)
                  ? AudioBusId::UI
                  : AudioBusId::SFX;
    req.baseVolume = volume;
    req.priority = (req.bus == AudioBusId::UI) ? 90 : 50;
    req.spatial.is3D = false;

    const int voiceId = SubmitVoice(req);
    if (voiceId < 0) {
        return -1;
    }

    if (hasBackend && clip) {
        const float effectiveGain = volume * ComputeEffectiveBusGain(req.bus);
        const int backendId = backend_->PlayMemorySound(
            clip->data.data(), clip->data.size(), effectiveGain,
            0.0f, 0.0f, 0.0f, false, 1.0f, 0.0f, 0.0f, false, false);
        if (backendId < 0) {
            StopVoice(voiceId);
            return -1;
        }
        BindBackendSoundToVoice(voiceId, backendId);
    }
    return voiceId;
}

int AudioServer::Play3D(const std::string& name, float x, float y, float z, float volume) {
    return Play3DEx(name, x, y, z, volume, 1.0f, 0.0f, 0.0f);
}

int AudioServer::Play3DPath(const std::string& mpqPath, float x, float y, float z, float volume) {
    return Play3DPathEx(mpqPath, x, y, z, volume, 1.0f, 0.0f, 0.0f);
}

int AudioServer::Play3DEx(const std::string& name,
                          float x,
                          float y,
                          float z,
                          float volume,
                          float pitch,
                          float minDist,
                          float maxDist) {
    if (name.empty()) {
        return -1;
    }
    const bool hasBackend = IsBackendActive();
    if (!hasBackend && !headlessSimulationEnabled_) {
        return -1;
    }

    const CachedSoundClip* clip = nullptr;
    if (hasBackend) {
        clip = ResolveOrAutoLoadNamedClip(name);
        if (!clip) {
            return -1;
        }
    }

    AudioVoiceRequest req;
    req.clipKey = name;
    req.bus = AudioBusId::SFX;
    req.baseVolume = volume;
    req.pitch = (pitch > 0.0f) ? pitch : 1.0f;
    req.priority = 50;
    req.spatial.is3D = true;
    req.spatial.position = Vec3(x, y, z);
    req.spatial.minDistance = (minDist > 0.0f) ? minDist : 300.0f;
    req.spatial.maxDistance = (maxDist > 0.0f) ? maxDist : 2400.0f;
    req.spatial.distanceCutoff = (maxDist > 0.0f) ? maxDist : 3000.0f;

    const int voiceId = SubmitVoice(req);
    if (voiceId < 0) {
        return -1;
    }

    if (hasBackend && clip) {
        const float effectiveGain = volume * ComputeEffectiveBusGain(req.bus);
        const int backendId = backend_->PlayMemorySound(
            clip->data.data(), clip->data.size(), effectiveGain,
            x, y, z, true, req.pitch, minDist, maxDist, false, false);
        if (backendId < 0) {
            StopVoice(voiceId);
            return -1;
        }
        BindBackendSoundToVoice(voiceId, backendId);
    }
    return voiceId;
}

int AudioServer::Play3DPathEx(const std::string& mpqPath,
                              float x,
                              float y,
                              float z,
                              float volume,
                              float pitch,
                              float minDist,
                              float maxDist) {
    if (mpqPath.empty()) {
        return -1;
    }
    const bool hasBackend = IsBackendActive();
    if (!hasBackend && !headlessSimulationEnabled_) {
        return -1;
    }

    // Load-before-submit: if e.g. AckSoundBank probes `.wav` before `.mp3` fallback and
    // `ResolveOrLoadPathClip` fails, return -1 immediately WITHOUT allocating a UnitVoice
    // or triggering ghost sidechain ducking!
    const CachedSoundClip* clip = nullptr;
    if (hasBackend) {
        clip = ResolveOrLoadPathClip(mpqPath);
        if (!clip) {
            return -1;
        }
    }

    AudioVoiceRequest req;
    req.clipKey = mpqPath;
    req.bus = (mpqPath.find("Units") != std::string::npos)
                  ? AudioBusId::UnitVoice
                  : AudioBusId::SFX;
    req.baseVolume = volume;
    req.pitch = (pitch > 0.0f) ? pitch : 1.0f;
    req.priority = (req.bus == AudioBusId::UnitVoice) ? 80 : 50;
    req.spatial.is3D = true;
    req.spatial.position = Vec3(x, y, z);
    req.spatial.minDistance = (minDist > 0.0f) ? minDist : 300.0f;
    req.spatial.maxDistance = (maxDist > 0.0f) ? maxDist : 2400.0f;
    req.spatial.distanceCutoff = (maxDist > 0.0f) ? maxDist : 3000.0f;

    const int voiceId = SubmitVoice(req);
    if (voiceId < 0) {
        return -1;
    }

    if (hasBackend && clip) {
        const float effectiveGain = volume * ComputeEffectiveBusGain(req.bus);
        const int backendId = backend_->PlayMemorySound(
            clip->data.data(), clip->data.size(), effectiveGain,
            x, y, z, true, req.pitch, minDist, maxDist, false, false);
        if (backendId < 0) {
            StopVoice(voiceId);
            return -1;
        }
        BindBackendSoundToVoice(voiceId, backendId);
    }
    return voiceId;
}

bool AudioServer::PlayMusic(const std::string& mpqPath, MpqManager* mpq, float volume, bool loop) {
    if (!IsBackendActive() || !mpq) {
        return false;
    }
    StopMusic();

    std::vector<uint8_t> buf;
    if (!mpq->ReadFile(mpqPath, buf) || buf.empty()) {
        return false;
    }
    musicData_ = std::move(buf);
    return PlayMusicFromFile("", volume, loop);
}

bool AudioServer::PlayMusicFromFile(const std::string& filePath, float volume, bool loop) {
    if (!IsBackendActive()) {
        return false;
    }
    if (!filePath.empty()) {
        StopMusic();
        FILE* f = std::fopen(filePath.c_str(), "rb");
        if (!f) {
            return false;
        }
        std::fseek(f, 0, SEEK_END);
        const long sz = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (sz <= 0) {
            std::fclose(f);
            return false;
        }
        musicData_.resize(static_cast<size_t>(sz));
        if (std::fread(musicData_.data(), 1, static_cast<size_t>(sz), f) != static_cast<size_t>(sz)) {
            std::fclose(f);
            musicData_.clear();
            return false;
        }
        std::fclose(f);
    }
    if (musicData_.empty()) {
        return false;
    }

    musicBaseVolume_ = std::clamp(volume, 0.0f, 2.0f);
    const float effectiveGain = musicBaseVolume_ * ComputeEffectiveBusGain(AudioBusId::Music);
    musicBackendId_ = backend_->PlayMemorySound(
        musicData_.data(), musicData_.size(), effectiveGain,
        0.0f, 0.0f, 0.0f, false, 1.0f, 0.0f, 0.0f, loop, /*isMusic=*/true);
    if (musicBackendId_ < 0) {
        musicData_.clear();
        return false;
    }
    return true;
}

void AudioServer::StopMusic() {
    if (backend_ && musicBackendId_ > 0) {
        backend_->StopSound(musicBackendId_);
    }
    musicBackendId_ = -1;
    musicData_.clear();
}

void AudioServer::SetMusicVolume(float vol) {
    SetBusVolumeLinear(AudioBusId::Music, std::clamp(vol, 0.0f, 1.0f));
    if (backend_ && musicBackendId_ > 0) {
        backend_->SetSoundVolume(musicBackendId_,
                                 musicBaseVolume_ * ComputeEffectiveBusGain(AudioBusId::Music));
    }
}

bool AudioServer::IsMusicPlaying() const {
    return backend_ && musicBackendId_ > 0 && backend_->IsSoundPlaying(musicBackendId_);
}

bool AudioServer::PlayMidiAmbient(const std::string& mpqPath, MpqManager* mpq, float volume) {
    if (!IsBackendActive() || !mpq || mpqPath.empty()) {
        return false;
    }
    if (ambientBackendId_ > 0 && ambientPath_ == mpqPath && backend_->IsSoundPlaying(ambientBackendId_)) {
        return true;
    }

    // Render once per path; day/night flips reuse the cache.
    auto cached = midiCache_.find(mpqPath);
    if (cached == midiCache_.end()) {
        std::vector<uint8_t> bytes;
        if (!mpq->ReadFile(mpqPath, bytes) || bytes.empty()) {
            return false;
        }
        MidiSong song;
        if (!ParseMidiFile(bytes.data(), bytes.size(), song)) {
            std::cerr << "AudioServer: MIDI parse failed '" << mpqPath << "'" << std::endl;
            return false;
        }
        const MidiPcm pcm = RenderMidiSong(song, MidiRenderOptions{});
        if (pcm.samples.empty() || pcm.sampleRate <= 0) {
            return false;
        }

        // Wrap rendered stereo float PCM as 16-bit WAV so miniaudio handles sample-rate resampling.
        std::vector<uint8_t> wav;
        const uint32_t frames = static_cast<uint32_t>(pcm.samples.size() / 2);
        const uint32_t dataBytes = frames * 2u * 2u;
        wav.reserve(44u + dataBytes);

        auto le32 = [&](uint32_t v) {
            wav.push_back(static_cast<uint8_t>(v & 0xFFu));
            wav.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
            wav.push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
            wav.push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
        };
        auto le16 = [&](uint16_t v) {
            wav.push_back(static_cast<uint8_t>(v & 0xFFu));
            wav.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
        };

        wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
        le32(36u + dataBytes);
        wav.insert(wav.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
        le32(16u);
        le16(1u);
        le16(2u);
        le32(static_cast<uint32_t>(pcm.sampleRate));
        le32(static_cast<uint32_t>(pcm.sampleRate) * 2u * 2u);
        le16(4u);
        le16(16u);
        wav.insert(wav.end(), {'d', 'a', 't', 'a'});
        le32(dataBytes);

        for (float s : pcm.samples) {
            const float clamped = std::clamp(s, -1.0f, 1.0f);
            le16(static_cast<uint16_t>(static_cast<int16_t>(clamped * 32767.0f)));
        }

        if (midiCache_.size() >= 4u) {
            midiCache_.clear();
        }
        cached = midiCache_.emplace(mpqPath, std::move(wav)).first;
    }

    StopMidiAmbient();
    ambientWav_ = cached->second;
    ambientVolume_ = volume;
    const float effectiveGain = volume * ComputeEffectiveBusGain(AudioBusId::Ambient);
    ambientBackendId_ = backend_->PlayMemorySound(
        ambientWav_.data(), ambientWav_.size(), effectiveGain,
        0.0f, 0.0f, 0.0f, false, 1.0f, 0.0f, 0.0f, /*looping=*/true, /*isMusic=*/true);
    if (ambientBackendId_ < 0) {
        ambientWav_.clear();
        return false;
    }
    ambientPath_ = mpqPath;
    return true;
}

void AudioServer::StopMidiAmbient() {
    if (backend_ && ambientBackendId_ > 0) {
        backend_->StopSound(ambientBackendId_);
    }
    ambientBackendId_ = -1;
    ambientPath_.clear();
    ambientWav_.clear();
}

bool AudioServer::IsMidiAmbientPlaying(const std::string& mpqPath) const {
    return backend_ &&
           ambientBackendId_ > 0 &&
           ambientPath_ == mpqPath &&
           backend_->IsSoundPlaying(ambientBackendId_);
}

void AudioServer::SetMasterVolume(float vol) {
    // Route solely through `AudioBusId::Master` so effective bus gain applies `vol` once
    // (keeping `backend_->SetMasterVolume(1.0f)` prevents quadratic/cubic gain scaling).
    const float clamped = std::clamp(vol, 0.0f, 1.0f);
    SetBusVolumeLinear(AudioBusId::Master, clamped);
}

void AudioServer::SetBusVolumeLinear(AudioBusId bus, float linear) {
    const size_t idx = static_cast<size_t>(bus);
    if (idx < kBusCount) {
        buses_[idx].volumeLinear = std::clamp(linear, 0.0f, 2.0f);
        RefreshActiveVoiceGains(/*updateStereoPan=*/false);
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
        RefreshActiveVoiceGains(/*updateStereoPan=*/false);
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
        RefreshActiveVoiceGains(/*updateStereoPan=*/false);
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
    if (backend_) {
        backend_->SetListenerPosition(pos.x, pos.y, pos.z);
    }
}

void AudioServer::SetListenerOrientation(const Vec3& forward, const Vec3& up) {
    const float fLen = std::sqrt(Dot(forward, forward));
    listener_.forward = (fLen > 1e-6f) ? (forward * (1.0f / fLen)) : Vec3(0.0f, 0.0f, -1.0f);

    const float uLen = std::sqrt(Dot(up, up));
    listener_.up = (uLen > 1e-6f) ? (up * (1.0f / uLen)) : Vec3(0.0f, 1.0f, 0.0f);

    const Vec3 r = Cross(listener_.forward, listener_.up);
    const float rLen = std::sqrt(Dot(r, r));
    listener_.right = (rLen > 1e-6f) ? (r * (1.0f / rLen)) : Vec3(1.0f, 0.0f, 0.0f);

    if (backend_) {
        backend_->SetListenerOrientation(listener_.forward.x, listener_.forward.y, listener_.forward.z,
                                         listener_.up.x, listener_.up.y, listener_.up.z);
    }
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

    // Polyphony limit & priority-based voice stealing (stops mapped backend sound too!)
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
        if (backend_ && activeVoices_[victimIdx].backendSoundId > 0) {
            backend_->StopSound(activeVoices_[victimIdx].backendSoundId);
        }
        activeVoices_.erase(activeVoices_.begin() + static_cast<std::ptrdiff_t>(victimIdx));
        stats_.voicesStolen++;
    }

    const int id = nextVoiceId_++;
    ActiveAudioVoice voice;
    voice.voiceId = id;
    voice.backendSoundId = -1;
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
            const bool wasUnitVoice = (activeVoices_[i].bus == AudioBusId::UnitVoice);
            const int backendId = activeVoices_[i].backendSoundId;
            if (backend_ && backendId > 0) {
                backend_->StopSound(backendId);
            }
            activeVoices_.erase(activeVoices_.begin() + static_cast<std::ptrdiff_t>(i));
            if (wasUnitVoice && duckingEnabled_) {
                RefreshActiveVoiceGains(/*updateStereoPan=*/false);
            }
            return true;
        }
    }
    return false;
}

void AudioServer::StopAllVoices() {
    const bool hadVoices = !activeVoices_.empty();
    if (backend_) {
        for (const auto& v : activeVoices_) {
            if (v.backendSoundId > 0) {
                backend_->StopSound(v.backendSoundId);
            }
        }
        backend_->StopAllSounds(/*includeMusic=*/false);
    }
    activeVoices_.clear();
    if (hadVoices) {
        RefreshActiveVoiceGains(/*updateStereoPan=*/false);
    }
}

void AudioServer::Update(float dt) {
    const float safeDt = std::max(0.0f, dt);
    currentTimeSec_ += safeDt;

    for (int i = static_cast<int>(activeVoices_.size()) - 1; i >= 0; --i) {
        ActiveAudioVoice& v = activeVoices_[static_cast<size_t>(i)];
        v.elapsedSec += safeDt;
        const bool backendFinished =
            (v.backendSoundId > 0 && backend_ && !backend_->IsSoundPlaying(v.backendSoundId));
        if (backendFinished || (!v.looping && v.backendSoundId <= 0 && v.elapsedSec >= v.durationSec)) {
            if (backend_ && v.backendSoundId > 0 && backend_->IsSoundPlaying(v.backendSoundId)) {
                backend_->StopSound(v.backendSoundId);
            }
            activeVoices_.erase(activeVoices_.begin() + i);
        }
    }

    if (backend_) {
        backend_->PollGarbageCollect();
    }
    RefreshActiveVoiceGains(/*updateStereoPan=*/true);
}

void AudioServer::RefreshActiveVoiceGains(bool updateStereoPan) {
    std::array<float, kBusCount> busGains{};
    for (size_t b = 0; b < kBusCount; ++b) {
        busGains[b] = ComputeEffectiveBusGain(static_cast<AudioBusId>(b));
    }

    if (backend_ && musicBackendId_ > 0) {
        backend_->SetSoundVolume(musicBackendId_,
                                 musicBaseVolume_ * busGains[static_cast<size_t>(AudioBusId::Music)]);
    }
    if (backend_ && ambientBackendId_ > 0) {
        backend_->SetSoundVolume(ambientBackendId_,
                                 ambientVolume_ * busGains[static_cast<size_t>(AudioBusId::Ambient)]);
    }

    if (activeVoices_.empty()) {
        return;
    }

    for (auto& v : activeVoices_) {
        const SpatialEvalResult sp = EvaluateSpatial3D(v.spatial);
        v.finalGain = v.baseVolume * busGains[static_cast<size_t>(v.bus)] * sp.attenuationGain;
        if (updateStereoPan) {
            v.stereoPan = sp.stereoPan;
        }
        if (backend_ && v.backendSoundId > 0) {
            // Hardware/mock 3D voices spatialize internally (`is3D = true`), so pass
            // pre-attenuation bus gain to avoid quadratic `attenuation^2` rolloff.
            const float backendGain = v.spatial.is3D
                ? (v.baseVolume * busGains[static_cast<size_t>(v.bus)])
                : v.finalGain;
            backend_->SetSoundVolume(v.backendSoundId, backendGain);
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
