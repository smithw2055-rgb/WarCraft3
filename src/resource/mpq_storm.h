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
    const MpqArchive* FindFileInArchives(const std::string& filename) const;
    std::vector<MpqArchive> archives_;
};
