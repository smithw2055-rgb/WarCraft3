#pragma once
#ifndef __STORMLIB_NO_STATIC_LINK__
#define __STORMLIB_NO_STATIC_LINK__
#endif
#include <StormLib.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

struct MpqArchive {
    std::string path;
    HANDLE handle = nullptr;
};

class MpqManager {
public:
    MpqManager() = default;
    ~MpqManager();

    bool Open(const std::string& path);
    void CloseAll();

    // Map mount (ref CompoundDataSource game+map): the map MPQ shadows every
    // game archive regardless of open order. Game archives keep
    // first-opened-wins so patches (opened first) beat base data.
    bool OpenOnTop(const std::string& path);
    void CloseTop();

    // Loose-folder overlay (ref FolderDataSource): dev overrides and
    // extracted assets shadow archive content. Last added wins.
    bool AddFolder(const std::string& path);
    int FolderCount() const { return (int)folders_.size(); }

    bool FileExists(const std::string& filename) const;
    bool ReadFile(const std::string& filename, std::vector<uint8_t>& out) const;
    std::vector<std::string> ListFiles(const std::string& archivePath) const;
    std::vector<std::string> ListAllFiles(const std::string& pattern = "*") const;

    int ArchiveCount() const { return (int)archives_.size(); }
    HANDLE GetArchiveHandle(int index) const {
        if (index < 0 || index >= (int)archives_.size()) return nullptr;
        return archives_[index].handle;
    }

private:
    // Lookup order: folders, last added first; then map mounts, last
    // mounted first; then game archives in open order (patches first).
    const MpqArchive* FindFileInArchives(const std::string& filename) const;
    bool FindFileInFolders(const std::string& filename, std::string& outPath) const;
    std::vector<MpqArchive> archives_;
    std::vector<MpqArchive> topArchives_; // map mounts shadow everything
    std::vector<std::string> folders_;
};
