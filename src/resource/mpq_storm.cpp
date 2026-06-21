#include "mpq_storm.h"
#include <iostream>
#include <cstring>

MpqManager::~MpqManager() { CloseAll(); }

bool MpqManager::Open(const std::string& path) {
    for (auto& a : archives_) {
        if (a.path == path) return true;
    }
    HANDLE h = nullptr;
    if (!SFileOpenArchive(path.c_str(), 0, MPQ_OPEN_READ_ONLY, &h)) {
        std::cerr << "MpqStorm: failed to open " << path << std::endl;
        return false;
    }
    archives_.push_back({path, h});
    std::cout << "MpqStorm: opened " << path << std::endl;
    return true;
}

void MpqManager::CloseAll() {
    for (auto& a : archives_) {
        if (a.handle) SFileCloseArchive(a.handle);
    }
    archives_.clear();
}

const MpqArchive* MpqManager::FindFileInArchives(const std::string& filename) const {
    for (const auto& a : archives_) {
        if (SFileHasFile(a.handle, filename.c_str())) return &a;
    }
    return nullptr;
}

bool MpqManager::FileExists(const std::string& filename) const {
    return FindFileInArchives(filename) != nullptr;
}

bool MpqManager::ReadFile(const std::string& filename, std::vector<uint8_t>& out) const {
    const auto* archive = FindFileInArchives(filename);
    if (!archive) return false;

    HANDLE hFile = nullptr;
    if (!SFileOpenFileEx(archive->handle, filename.c_str(), SFILE_OPEN_BASE_FILE, &hFile)) return false;

    DWORD fileSize = SFileGetFileSize(hFile, nullptr);
    out.resize(fileSize);
    DWORD bytesRead = 0;
    bool ok = SFileReadFile(hFile, out.data(), fileSize, &bytesRead, nullptr);
    SFileCloseFile(hFile);

    if (!ok) out.clear();
    return ok;
}

std::vector<std::string> MpqManager::ListFiles(const std::string& archivePath) const {
    std::vector<std::string> result;
    for (const auto& a : archives_) {
        if (!archivePath.empty() && a.path != archivePath) continue;

        SFILE_FIND_DATA fd;
        HANDLE hFind = SFileFindFirstFile(a.handle, "*", &fd, nullptr);
        if (!hFind) continue;

        do { result.push_back(fd.cFileName); }
        while (SFileFindNextFile(hFind, &fd));
        SFileFindClose(hFind);
    }
    return result;
}

std::vector<std::string> MpqManager::ListAllFiles(const std::string& pattern) const {
    std::vector<std::string> result;
    for (const auto& a : archives_) {
        SFILE_FIND_DATA fd;
        HANDLE hFind = SFileFindFirstFile(a.handle, pattern.c_str(), &fd, nullptr);
        if (!hFind) continue;
        do { result.push_back(fd.cFileName); }
        while (SFileFindNextFile(hFind, &fd));
        SFileFindClose(hFind);
    }
    return result;
}
