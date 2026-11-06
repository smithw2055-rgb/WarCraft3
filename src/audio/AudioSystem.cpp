#include "AudioSystem.h"
#include "audio/MidiFile.h"
#include "audio/MidiSynth.h"
#include "../resource/mpq_storm.h"
#include "../resource/resource_server.h"

#define MINIAUDIO_IMPLEMENTATION
#include "third_party/miniaudio/miniaudio.h"

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
    StopMidiAmbient();
    soundCache_.clear();

    if (engine_) {
        ma_engine_uninit(engine_);
        delete engine_;
        engine_ = nullptr;
    }
    initialized_ = false;
}

bool AudioSystem::LoadSound(const std::string& name, const std::string& mpqPath, MpqManager* mpq) {
    if (soundCache_.find(name) != soundCache_.end()) return true;

    std::vector<uint8_t> buf;
    bool loaded = resource::ResourceServer::Instance().ReadRawBytes(mpqPath, buf);
    if (!loaded && mpq) {
        loaded = mpq->ReadFile(mpqPath, buf);
    }
    if (!loaded || buf.empty()) {
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
    if (mpqPath.empty()) return -1;
    if (!engine_ && !headlessSimulationEnabled_) return -1;

    audio::AudioVoiceRequest req;
    req.clipKey = mpqPath;
    req.bus = (mpqPath.find("Units") != std::string::npos) ? audio::AudioBusId::UnitVoice : audio::AudioBusId::SFX;
    req.baseVolume = volume;
    req.pitch = (pitch > 0.0f) ? pitch : 1.0f;
    req.priority = (req.bus == audio::AudioBusId::UnitVoice) ? 80 : 50;
    req.spatial.is3D = true;
    req.spatial.position = Vec3(x, y, z);
    req.spatial.minDistance = (minDist > 0.0f) ? minDist : 300.0f;
    req.spatial.maxDistance = (maxDist > 0.0f) ? maxDist : 2400.0f;
    req.spatial.distanceCutoff = (maxDist > 0.0f) ? maxDist : 3000.0f;

    const int voiceId = audioServer_.SubmitVoice(req);
    if (voiceId < 0) return -1;
    if (!engine_) return voiceId;
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
    audioServer_.SetListenerPosition(Vec3(x, y, z));
    if (!engine_) return;
    ma_engine_listener_set_position(engine_, 0, x, y, z);
}

void AudioSystem::SetListenerOrientation(float forwardX, float forwardY, float forwardZ,
                                           float upX, float upY, float upZ) {
    audioServer_.SetListenerOrientation(Vec3(forwardX, forwardY, forwardZ), Vec3(upX, upY, upZ));
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

bool AudioSystem::PlayMidiAmbient(const std::string& mpqPath, MpqManager* mpq, float volume) {
    if (!engine_ || !mpq || mpqPath.empty()) return false;
    if (ambientSound_ && ambientPath_ == mpqPath) {
        if (!ma_sound_is_playing(ambientSound_)) ma_sound_start(ambientSound_);
        return true; // already looping this track
    }
    // Render once per path; day/night flips reuse the cache.
    auto cached = midiCache_.find(mpqPath);
    if (cached == midiCache_.end()) {
        std::vector<uint8_t> bytes;
        if (!mpq->ReadFile(mpqPath, bytes) || bytes.empty()) return false;
        MidiSong song;
        if (!ParseMidiFile(bytes.data(), bytes.size(), song)) {
            std::cerr << "AudioSystem: MIDI parse failed '" << mpqPath << "'" << std::endl;
            return false;
        }
        MidiPcm pcm = RenderMidiSong(song, MidiRenderOptions{});
        if (pcm.samples.empty() || pcm.sampleRate <= 0) return false;
        // Wrap as 16-bit WAV so the decoder pipeline (with true rate)
        // handles any engine sample rate correctly.
        std::vector<uint8_t> wav;
        const uint32_t frames = (uint32_t)(pcm.samples.size() / 2);
        const uint32_t dataBytes = frames * 2 * 2;
        wav.reserve(44 + dataBytes);
        auto le32 = [&](uint32_t v) {
            wav.push_back(v & 0xFF);
            wav.push_back((v >> 8) & 0xFF);
            wav.push_back((v >> 16) & 0xFF);
            wav.push_back((v >> 24) & 0xFF);
        };
        auto le16 = [&](uint16_t v) {
            wav.push_back(v & 0xFF);
            wav.push_back((v >> 8) & 0xFF);
        };
        wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
        le32(36 + dataBytes);
        wav.insert(wav.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
        le32(16);
        le16(1);
        le16(2);
        le32((uint32_t)pcm.sampleRate);
        le32((uint32_t)pcm.sampleRate * 2 * 2);
        le16(4);
        le16(16);
        wav.insert(wav.end(), {'d', 'a', 't', 'a'});
        le32(dataBytes);
        for (float s : pcm.samples) {
            float c = s < -1.0f ? -1.0f : (s > 1.0f ? 1.0f : s);
            le16((uint16_t)(int)(c * 32767.0f));
        }
        if (midiCache_.size() >= 4) midiCache_.clear();
        cached = midiCache_.emplace(mpqPath, std::move(wav)).first;
        std::cout << "AudioSystem: rendered MIDI ambient '" << mpqPath
                  << "' (" << frames << " frames)" << std::endl;
    }
    StopMidiAmbient();
    ambientWav_ = cached->second; // decoder references this memory
    ambientDecoder_ = new ma_decoder;
    ma_decoder_config decoderConfig = ma_decoder_config_init_default();
    if (ma_decoder_init_memory(ambientWav_.data(), ambientWav_.size(),
                               &decoderConfig, ambientDecoder_) != MA_SUCCESS) {
        delete ambientDecoder_;
        ambientDecoder_ = nullptr;
        ambientWav_.clear();
        return false;
    }
    ambientSound_ = new ma_sound;
    if (ma_sound_init_from_data_source(engine_, (ma_data_source*)ambientDecoder_,
                                       MA_SOUND_FLAG_NO_SPATIALIZATION,
                                       nullptr, ambientSound_) != MA_SUCCESS) {
        ma_decoder_uninit(ambientDecoder_);
        delete ambientDecoder_;
        ambientDecoder_ = nullptr;
        delete ambientSound_;
        ambientSound_ = nullptr;
        ambientWav_.clear();
        return false;
    }
    ambientPath_ = mpqPath;
    ambientVolume_ = volume;
    ma_sound_set_volume(ambientSound_, volume * masterVolume_);
    ma_sound_set_looping(ambientSound_, true);
    ma_sound_start(ambientSound_);
    ambientSoundId_ = nextSoundId_++;
    activeSounds_.push_back({ambientSoundId_, ambientSound_, ambientDecoder_, true});
    return true;
}

void AudioSystem::StopMidiAmbient() {
    if (!ambientSound_ && !ambientDecoder_) {
        ambientPath_.clear();
        ambientWav_.clear();
        return;
    }
    for (size_t i = 0; i < activeSounds_.size(); i++) {
        if (activeSounds_[i].id == ambientSoundId_) {
            activeSounds_.erase(activeSounds_.begin() + i);
            break;
        }
    }
    ambientSoundId_ = -1;
    if (ambientSound_) {
        ma_sound_stop(ambientSound_);
        ma_sound_uninit(ambientSound_);
        delete ambientSound_;
        ambientSound_ = nullptr;
    }
    if (ambientDecoder_) {
        ma_decoder_uninit(ambientDecoder_);
        delete ambientDecoder_;
        ambientDecoder_ = nullptr;
    }
    ambientPath_.clear();
    ambientWav_.clear();
}

bool AudioSystem::IsMidiAmbientPlaying(const std::string& mpqPath) const {
    return ambientSound_ && ambientPath_ == mpqPath &&
           ma_sound_is_playing(ambientSound_);
}

void AudioSystem::SetMasterVolume(float vol) {
    masterVolume_ = std::clamp(vol, 0.0f, 1.0f);
    audioServer_.SetBusVolumeLinear(audio::AudioBusId::Master, masterVolume_);
    if (engine_) ma_engine_set_volume(engine_, masterVolume_);
}

void AudioSystem::Update(float dt) {
    audioServer_.Update(dt);
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
