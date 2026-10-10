#include "AudioSystem.h"
#include "../resource/mpq_storm.h"

#define MINIAUDIO_IMPLEMENTATION
#include "../third_party/miniaudio/miniaudio.h"

#include <iostream>
#include <algorithm>
#include <cstring>

AudioSystem::AudioSystem() {}
AudioSystem::~AudioSystem() { Shutdown(); }

bool AudioSystem::Init() {
    if (initialized_) return true;

    engine_ = new ma_engine;
    ma_engine_config config = ma_engine_config_init();
    ma_result result = ma_engine_init(&config, engine_);
    if (result != MA_SUCCESS) {
        std::cerr << "AudioSystem: ma_engine_init failed (" << result << ")" << std::endl;
        delete engine_;
        engine_ = nullptr;
        return false;
    }

    ma_engine_listener_set_position(engine_, 0, 0.0f, 0.0f, 0.0f);
    ma_engine_listener_set_world_up(engine_, 0, 0.0f, 1.0f, 0.0f);

    initialized_ = true;
    std::cout << "AudioSystem: initialized" << std::endl;
    return true;
}

void AudioSystem::Shutdown() {
    StopAll();
    StopMusic();
    soundCache_.clear();

    if (engine_) {
        ma_engine_uninit(engine_);
        delete engine_;
        engine_ = nullptr;
    }
    initialized_ = false;
}

bool AudioSystem::LoadSound(const std::string& name, const std::string& mpqPath, MpqManager* mpq) {
    if (!mpq) return false;
    if (soundCache_.find(name) != soundCache_.end()) return true;

    std::vector<uint8_t> buf;
    if (!mpq->ReadFile(mpqPath, buf) || buf.empty()) {
        std::cerr << "AudioSystem: failed to load '" << mpqPath << "'" << std::endl;
        return false;
    }

    SoundSource src;
    src.data = std::move(buf);
    src.path = mpqPath;
    soundCache_[name] = std::move(src);
    return true;
}

bool AudioSystem::LoadSoundFromFile(const std::string& name, const std::string& filePath) {
    if (soundCache_.find(name) != soundCache_.end()) return true;

    FILE* f = std::fopen(filePath.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0) { std::fclose(f); return false; }

    std::vector<uint8_t> buf((size_t)sz);
    if (std::fread(buf.data(), 1, (size_t)sz, f) != (size_t)sz) {
        std::fclose(f);
        return false;
    }
    std::fclose(f);

    SoundSource src;
    src.data = std::move(buf);
    src.path = filePath;
    soundCache_[name] = std::move(src);
    return true;
}

int AudioSystem::playFromSource(SoundSource& src, float volume, float x, float y, float z, bool is3D) {
    return playFromSourceEx(src, volume, x, y, z, is3D, 1.0f, 0.0f, 0.0f);
}

int AudioSystem::playFromSourceEx(SoundSource& src, float volume, float x, float y, float z,
                                  bool is3D, float pitch, float minDist, float maxDist) {
    ma_decoder* decoder = new ma_decoder;
    ma_decoder_config decoderConfig = ma_decoder_config_init_default();
    ma_result result = ma_decoder_init_memory(src.data.data(), src.data.size(),
                                               &decoderConfig, decoder);
    if (result != MA_SUCCESS) {
        delete decoder;
        return -1;
    }

    ma_sound* sound = new ma_sound;
    ma_uint32 flags = is3D ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;
    result = ma_sound_init_from_data_source(engine_, (ma_data_source*)decoder,
                                             flags, nullptr, sound);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(decoder);
        delete decoder;
        delete sound;
        return -1;
    }

    if (is3D) ma_sound_set_position(sound, x, y, z);
    if (pitch > 0.0f && pitch != 1.0f) ma_sound_set_pitch(sound, pitch);
    if (is3D) {
        if (minDist > 0.0f) ma_sound_set_min_distance(sound, minDist);
        if (maxDist > 0.0f) ma_sound_set_max_distance(sound, maxDist);
    }
    ma_sound_set_volume(sound, volume * masterVolume_);
    ma_sound_start(sound);

    int id = nextSoundId_++;
    activeSounds_.push_back({ id, sound, decoder, false });
    return id;
}

int AudioSystem::Play2D(const std::string& name, float volume) {
    if (!engine_) return -1;
    auto it = soundCache_.find(name);
    if (it == soundCache_.end()) {
        // Auto-load from MPQ if manager is set
        if (mpq_) {
            std::string mpqPath = "Sound\\Units\\" + name + ".wav";
            if (!LoadSound(name, mpqPath, mpq_)) {
                mpqPath = "Sound\\Interface\\" + name + ".wav";
                LoadSound(name, mpqPath, mpq_);
            }
        }
        it = soundCache_.find(name);
        if (it == soundCache_.end()) return -1;
    }
    return playFromSource(it->second, volume, 0, 0, 0, false);
}

int AudioSystem::Play3D(const std::string& name, float x, float y, float z, float volume) {
    if (!engine_) return -1;
    auto it = soundCache_.find(name);
    if (it == soundCache_.end()) {
        if (mpq_) {
            std::string mpqPath = "Sound\\Units\\" + name + ".wav";
            if (!LoadSound(name, mpqPath, mpq_)) {
                mpqPath = "Sound\\Interface\\" + name + ".wav";
                LoadSound(name, mpqPath, mpq_);
            }
        }
        it = soundCache_.find(name);
        if (it == soundCache_.end()) return -1;
    }
    return playFromSource(it->second, volume, x, y, z, true);
}

int AudioSystem::Play3DPath(const std::string& mpqPath, float x, float y, float z, float volume) {
    if (!engine_ || mpqPath.empty()) return -1;
    auto it = soundCache_.find(mpqPath);
    if (it == soundCache_.end()) {
        if (!LoadSound(mpqPath, mpqPath, mpq_)) {
            return -1;
        }
        it = soundCache_.find(mpqPath);
        if (it == soundCache_.end()) return -1;
    }
    return playFromSource(it->second, volume, x, y, z, true);
}

int AudioSystem::Play3DEx(const std::string& name, float x, float y, float z, float volume,
                           float pitch, float minDist, float maxDist) {
    if (!engine_) return -1;
    auto it = soundCache_.find(name);
    if (it == soundCache_.end()) {
        if (mpq_) {
            std::string mpqPath = "Sound\\Units\\" + name + ".wav";
            if (!LoadSound(name, mpqPath, mpq_)) {
                mpqPath = "Sound\\Interface\\" + name + ".wav";
                LoadSound(name, mpqPath, mpq_);
            }
        }
        it = soundCache_.find(name);
        if (it == soundCache_.end()) return -1;
    }
    return playFromSourceEx(it->second, volume, x, y, z, true, pitch, minDist, maxDist);
}

int AudioSystem::Play3DPathEx(const std::string& mpqPath, float x, float y, float z, float volume,
                               float pitch, float minDist, float maxDist) {
    if (!engine_ || mpqPath.empty()) return -1;
    auto it = soundCache_.find(mpqPath);
    if (it == soundCache_.end()) {
        if (!LoadSound(mpqPath, mpqPath, mpq_)) return -1;
        it = soundCache_.find(mpqPath);
        if (it == soundCache_.end()) return -1;
    }
    return playFromSourceEx(it->second, volume, x, y, z, true, pitch, minDist, maxDist);
}

void AudioSystem::Stop(int soundId) {
    for (size_t i = 0; i < activeSounds_.size(); i++) {
        if (activeSounds_[i].id == soundId) {
            ma_sound_stop(activeSounds_[i].sound);
            ma_sound_uninit(activeSounds_[i].sound);
            ma_decoder_uninit(activeSounds_[i].decoder);
            delete activeSounds_[i].sound;
            delete activeSounds_[i].decoder;
            activeSounds_.erase(activeSounds_.begin() + i);
            return;
        }
    }
}

void AudioSystem::StopAll() {
    for (auto& as : activeSounds_) {
        if (as.isMusic) continue;
        ma_sound_stop(as.sound);
        ma_sound_uninit(as.sound);
        ma_decoder_uninit(as.decoder);
        delete as.sound;
        delete as.decoder;
    }
    activeSounds_.clear();
}

void AudioSystem::SetListenerPosition(float x, float y, float z) {
    if (!engine_) return;
    ma_engine_listener_set_position(engine_, 0, x, y, z);
}

void AudioSystem::SetListenerOrientation(float forwardX, float forwardY, float forwardZ,
                                           float upX, float upY, float upZ) {
    if (!engine_) return;
    ma_engine_listener_set_direction(engine_, 0, forwardX, forwardY, forwardZ);
    ma_engine_listener_set_world_up(engine_, 0, upX, upY, upZ);
}

bool AudioSystem::PlayMusic(const std::string& mpqPath, MpqManager* mpq, float volume, bool loop) {
    if (!engine_ || !mpq) return false;
    StopMusic();

    std::vector<uint8_t> buf;
    if (!mpq->ReadFile(mpqPath, buf) || buf.empty()) return false;
    musicData_ = std::move(buf);
    return PlayMusicFromFile("", volume, loop); // use the stored data
}

bool AudioSystem::PlayMusicFromFile(const std::string& filePath, float volume, bool loop) {
    if (!engine_) return false;
    StopMusic();

    if (!filePath.empty()) {
        FILE* f = std::fopen(filePath.c_str(), "rb");
        if (!f) return false;
        std::fseek(f, 0, SEEK_END);
        long sz = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (sz <= 0) { std::fclose(f); return false; }
        musicData_.resize((size_t)sz);
        if (std::fread(musicData_.data(), 1, (size_t)sz, f) != (size_t)sz) {
            std::fclose(f); return false;
        }
        std::fclose(f);
    }

    if (musicData_.empty()) return false;

    musicDecoder_ = new ma_decoder;
    ma_decoder_config decoderConfig = ma_decoder_config_init_default();
    ma_result result = ma_decoder_init_memory(musicData_.data(), musicData_.size(),
                                               &decoderConfig, musicDecoder_);
    if (result != MA_SUCCESS) {
        delete musicDecoder_;
        musicDecoder_ = nullptr;
        return false;
    }

    musicSound_ = new ma_sound;
    result = ma_sound_init_from_data_source(engine_, (ma_data_source*)musicDecoder_,
                                             MA_SOUND_FLAG_NO_SPATIALIZATION,
                                             nullptr, musicSound_);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(musicDecoder_);
        delete musicDecoder_;
        delete musicSound_;
        musicSound_ = nullptr;
        musicDecoder_ = nullptr;
        return false;
    }

    ma_sound_set_volume(musicSound_, volume * masterVolume_);
    ma_sound_set_looping(musicSound_, loop);
    ma_sound_start(musicSound_);

    musicSoundId_ = nextSoundId_++;
    activeSounds_.push_back({ musicSoundId_, musicSound_, musicDecoder_, true });
    return true;
}

void AudioSystem::StopMusic() {
    if (musicSound_) {
        ma_sound_stop(musicSound_);
        ma_sound_uninit(musicSound_);
        delete musicSound_;
        musicSound_ = nullptr;
    }
    if (musicDecoder_) {
        ma_decoder_uninit(musicDecoder_);
        delete musicDecoder_;
        musicDecoder_ = nullptr;
    }
    musicSoundId_ = -1;
    musicData_.clear();

    for (size_t i = 0; i < activeSounds_.size(); i++) {
        if (activeSounds_[i].isMusic) {
            activeSounds_.erase(activeSounds_.begin() + i);
            break;
        }
    }
}

void AudioSystem::SetMusicVolume(float vol) {
    if (musicSound_) ma_sound_set_volume(musicSound_, vol * masterVolume_);
}

bool AudioSystem::IsMusicPlaying() const {
    return musicSound_ && ma_sound_is_playing(musicSound_);
}

void AudioSystem::SetMasterVolume(float vol) {
    masterVolume_ = std::clamp(vol, 0.0f, 1.0f);
    if (engine_) ma_engine_set_volume(engine_, masterVolume_);
}

void AudioSystem::Update() {
    for (int i = (int)activeSounds_.size() - 1; i >= 0; i--) {
        if (activeSounds_[i].isMusic) continue;
        if (!ma_sound_is_playing(activeSounds_[i].sound)) {
            ma_sound_uninit(activeSounds_[i].sound);
            ma_decoder_uninit(activeSounds_[i].decoder);
            delete activeSounds_[i].sound;
            delete activeSounds_[i].decoder;
            activeSounds_.erase(activeSounds_.begin() + i);
        }
    }
}
