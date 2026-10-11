#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

struct ma_engine;
struct ma_sound;
struct ma_decoder;
class MpqManager;

struct SoundSource {
    std::vector<uint8_t> data;
    std::string path;
};

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    bool Init();
    void Shutdown();
    bool IsValid() const { return engine_ != nullptr; }

    bool LoadSound(const std::string& name, const std::string& mpqPath, MpqManager* mpq);
    bool LoadSoundFromFile(const std::string& name, const std::string& filePath);
    void SetMpqManager(MpqManager* mpq) { mpq_ = mpq; }
    MpqManager* GetMpqManager() const { return mpq_; }

    int Play2D(const std::string& name, float volume = 1.0f);
    int Play3D(const std::string& name, float x, float y, float z, float volume = 1.0f);
    int Play3DPath(const std::string& mpqPath, float x, float y, float z, float volume = 1.0f);
    // Full 3D voice with retail-style distance cutoff + pitch variance (ref
    // UnitAckSoundDesc min/max distance, CSound pitch/pitchVariance).
    // Defaults preserve legacy behavior (inverse attenuation, no cutoff).
    int Play3DEx(const std::string& name, float x, float y, float z, float volume = 1.0f,
                 float pitch = 1.0f, float minDist = 0.0f, float maxDist = 0.0f);
    int Play3DPathEx(const std::string& mpqPath, float x, float y, float z, float volume = 1.0f,
                     float pitch = 1.0f, float minDist = 0.0f, float maxDist = 0.0f);

    void Stop(int soundId);
    void StopAll();

    void SetListenerPosition(float x, float y, float z);
    void SetListenerOrientation(float forwardX, float forwardY, float forwardZ,
                                float upX, float upY, float upZ);

    bool PlayMusic(const std::string& mpqPath, MpqManager* mpq, float volume = 1.0f, bool loop = true);
    bool PlayMusicFromFile(const std::string& filePath, float volume = 1.0f, bool loop = true);
    void StopMusic();
    void SetMusicVolume(float vol);
    bool IsMusicPlaying() const;

    // MIDI ambient loop (retail Sound\Ambient day/night tracks): parsed and
    // rendered by the built-in ambient-grade synth (see MidiSynth), looped
    // from a cached PCM buffer. No-op when the same path already plays.
    bool PlayMidiAmbient(const std::string& mpqPath, MpqManager* mpq, float volume = 0.5f);
    void StopMidiAmbient();
    bool IsMidiAmbientPlaying(const std::string& mpqPath) const;

    void SetMasterVolume(float vol);
    float GetMasterVolume() const { return masterVolume_; }

    void Update();

private:
    struct ActiveSound {
        int id;
        ma_sound* sound;
        ma_decoder* decoder;
        bool isMusic;
    };

    ma_engine* engine_ = nullptr;
    std::unordered_map<std::string, SoundSource> soundCache_;
    std::vector<ActiveSound> activeSounds_;
    int nextSoundId_ = 1;
    float masterVolume_ = 1.0f;
    ma_sound* musicSound_ = nullptr;
    ma_decoder* musicDecoder_ = nullptr;
    int musicSoundId_ = -1;
    std::vector<uint8_t> musicData_;
    // MIDI ambient loop state: rendered PCM wrapped as WAV bytes and
    // played through the proven decoder pipeline (carries the true sample
    // rate, unlike ma_audio_buffer_ref). Cache keyed by MPQ path.
    ma_decoder* ambientDecoder_ = nullptr;
    ma_sound* ambientSound_ = nullptr;
    int ambientSoundId_ = -1;
    std::string ambientPath_;
    float ambientVolume_ = 0.5f;
    std::vector<uint8_t> ambientWav_;
    std::unordered_map<std::string, std::vector<uint8_t>> midiCache_;
    MpqManager* mpq_ = nullptr;
    bool initialized_ = false;

    int playFromSource(SoundSource& src, float volume, float x, float y, float z, bool is3D);
    int playFromSourceEx(SoundSource& src, float volume, float x, float y, float z, bool is3D,
                         float pitch, float minDist, float maxDist);
};
