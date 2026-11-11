#pragma once
// ============================================================================
// AudioSystem — Thin Compatibility Facade Over Unified `audio::AudioServer`
//
// Preserves 100% of existing game/UI/JASS AudioSystem call sites while routing
// all clip loading, 2D/3D playback, canonical `voiceId -> backendSoundId`
// tracking, music, and MIDI day/night ambient loops through `AudioServer`.
// ============================================================================
#include <memory>
#include <string>
#include <vector>

#include "audio/audio_server.h"

class MpqManager;

class AudioSystem {
public:
    AudioSystem() = default;
    ~AudioSystem() {
        Shutdown();
    }

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    bool Init() {
        return audioServer_.InitHardwareBackend();
    }

    void Shutdown() {
        audioServer_.ShutdownBackend();
    }

    bool IsValid() const {
        return audioServer_.IsBackendActive();
    }

    bool LoadSound(const std::string& name, const std::string& mpqPath, MpqManager* mpq) {
        return audioServer_.LoadSound(name, mpqPath, mpq);
    }

    bool LoadSoundFromFile(const std::string& name, const std::string& filePath) {
        return audioServer_.LoadSoundFromFile(name, filePath);
    }

    void SetMpqManager(MpqManager* mpq) {
        audioServer_.SetMpqManager(mpq);
    }

    MpqManager* GetMpqManager() const {
        return audioServer_.GetMpqManager();
    }

    int Play2D(const std::string& name, float volume = 1.0f) {
        return audioServer_.Play2D(name, volume);
    }

    int Play3D(const std::string& name, float x, float y, float z, float volume = 1.0f) {
        return audioServer_.Play3D(name, x, y, z, volume);
    }

    int Play3DPath(const std::string& mpqPath, float x, float y, float z, float volume = 1.0f) {
        return audioServer_.Play3DPath(mpqPath, x, y, z, volume);
    }

    int Play3DEx(const std::string& name,
                 float x,
                 float y,
                 float z,
                 float volume = 1.0f,
                 float pitch = 1.0f,
                 float minDist = 0.0f,
                 float maxDist = 0.0f) {
        return audioServer_.Play3DEx(name, x, y, z, volume, pitch, minDist, maxDist);
    }

    int Play3DPathEx(const std::string& mpqPath,
                     float x,
                     float y,
                     float z,
                     float volume = 1.0f,
                     float pitch = 1.0f,
                     float minDist = 0.0f,
                     float maxDist = 0.0f) {
        return audioServer_.Play3DPathEx(mpqPath, x, y, z, volume, pitch, minDist, maxDist);
    }

    void Stop(int soundId) {
        audioServer_.StopVoice(soundId);
    }

    void StopAll() {
        audioServer_.StopAllVoices();
    }

    void SetListenerPosition(float x, float y, float z) {
        audioServer_.SetListenerPosition(Vec3(x, y, z));
    }

    void SetListenerOrientation(float forwardX,
                                float forwardY,
                                float forwardZ,
                                float upX,
                                float upY,
                                float upZ) {
        audioServer_.SetListenerOrientation(
            Vec3(forwardX, forwardY, forwardZ),
            Vec3(upX, upY, upZ));
    }

    bool PlayMusic(const std::string& mpqPath,
                   MpqManager* mpq,
                   float volume = 1.0f,
                   bool loop = true) {
        return audioServer_.PlayMusic(mpqPath, mpq, volume, loop);
    }

    bool PlayMusicFromFile(const std::string& filePath,
                           float volume = 1.0f,
                           bool loop = true) {
        return audioServer_.PlayMusicFromFile(filePath, volume, loop);
    }

    void StopMusic() {
        audioServer_.StopMusic();
    }

    void SetMusicVolume(float vol) {
        audioServer_.SetMusicVolume(vol);
    }

    bool IsMusicPlaying() const {
        return audioServer_.IsMusicPlaying();
    }

    bool PlayMidiAmbient(const std::string& mpqPath, MpqManager* mpq, float volume = 0.5f) {
        return audioServer_.PlayMidiAmbient(mpqPath, mpq, volume);
    }

    void StopMidiAmbient() {
        audioServer_.StopMidiAmbient();
    }

    bool IsMidiAmbientPlaying(const std::string& mpqPath) const {
        return audioServer_.IsMidiAmbientPlaying(mpqPath);
    }

    void SetMasterVolume(float vol) {
        audioServer_.SetMasterVolume(vol);
    }

    float GetMasterVolume() const {
        return audioServer_.GetMasterVolume();
    }

    audio::AudioServer& GetAudioServer() {
        return audioServer_;
    }

    const audio::AudioServer& GetAudioServer() const {
        return audioServer_;
    }

    void SetHeadlessSimulationEnabled(bool enabled) {
        audioServer_.SetHeadlessSimulationEnabled(enabled);
    }

    bool IsHeadlessSimulationEnabled() const {
        return audioServer_.IsHeadlessSimulationEnabled();
    }

    void Update(float dt = 0.016f) {
        audioServer_.Update(dt);
    }

private:
    audio::AudioServer audioServer_;
};
