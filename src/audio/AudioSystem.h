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
    MpqManager* mpq_ = nullptr;
    bool initialized_ = false;

    int playFromSource(SoundSource& src, float volume, float x, float y, float z, bool is3D);
};
