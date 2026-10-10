#pragma once
#ifndef __STORMLIB_NO_STATIC_LINK__
#define __STORMLIB_NO_STATIC_LINK__
#endif
#include <StormLib.h>
#ifdef far
#undef far
#endif
#ifdef near
#undef near
#endif
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "casc.h"

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

    // CASC storage (1.30+ installs): behaves like a game archive in the
    // lookup order below, shadowing MPQs but never folders or map mounts.
    bool OpenCasc(const std::string& storagePath);
    int CascCount() const { return (int)cascStorages_.size(); }

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
    // mounted first; then CASC storages in open order; then game archives
    // in open order (patches first).
    const MpqArchive* FindFileInArchives(const std::string& filename) const;
    const MpqArchive* FindFileInTopArchives(const std::string& filename) const;
    const MpqArchive* FindFileInGameArchives(const std::string& filename) const;
    const CascStorage* FindFileInCasc(const std::string& filename) const;
    bool FindFileInFolders(const std::string& filename, std::string& outPath) const;
    // Map import fallback (ref SubdirDataSource(map, "war3mapImported\")):
    // map editors store imports under war3mapImported\<path> but game code
    // requests <path>; retry with the prefix inside map mounts/folders.
    bool FindMapImport(const std::string& filename, std::string& folderOut,
                       const MpqArchive** archiveOut) const;
    std::vector<MpqArchive> archives_;
    std::vector<MpqArchive> topArchives_; // map mounts shadow everything
    std::vector<CascStorage> cascStorages_;
    std::vector<std::string> folders_;
};
