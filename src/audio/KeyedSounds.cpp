#include "KeyedSounds.h"
#include "audio/AudioSystem.h"
#include "resource/mpq_storm.h"

bool KeyedSounds::LoadFromMpq(const MpqManager& mpq, const std::string& slkPath) {
    std::vector<uint8_t> buf;
    if (!mpq.ReadFile(slkPath, buf) || buf.empty()) return false;
    SlkTable slk;
    if (!slk.Load(buf.data(), buf.size())) return false;
    slk_ = std::move(slk);
    loaded_ = slk_.Rows() > 0;
    cache_.clear();
    return loaded_;
}

std::string KeyedSounds::ResolveFile(const std::string& key) const {
    if (!loaded_ || key.empty()) return {};
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    // UISounds.slk: first column is the key, file path in a File/Sound column.
    const int keyCol = 0;
    const int row = slk_.FindRow(key, keyCol);
    if (row < 0) return {};
    int fileCol = slk_.FindColumn("File");
    if (fileCol < 0) fileCol = slk_.FindColumn("Sound");
    if (fileCol < 0) fileCol = slk_.FindColumn("SoundFile");
    if (fileCol < 0) fileCol = 1;
    return slk_.GetString(row, fileCol);
}

int KeyedSounds::Play(AudioSystem* audio, const MpqManager* mpq, const std::string& key, float volume) {
    if (!audio || !audio->IsValid()) return -1;
    std::string file = ResolveFile(key);
    if (file.empty()) return -1;
    // Cache per key so repeated clicks don't re-decode.
    const std::string name = "ui:" + key;
    int id = audio->Play2D(name, volume);
    if (id >= 0) return id;
    // Not cached yet: load from MPQ then play.
    if (mpq && audio->LoadSound(name, file, const_cast<MpqManager*>(mpq))) {
        return audio->Play2D(name, volume);
    }
    return -1;
}
