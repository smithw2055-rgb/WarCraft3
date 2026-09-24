#include "mpq_storm.h"
#include <iostream>
#include <cstring>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

// Archives canonically use backslashes; folders use native separators.
std::string ToArchivePath(const std::string& p) {
    std::string out = p;
    for (auto& c : out) if (c == '/') c = '\\';
    return out;
}

std::string ToNativePath(const std::string& p) {
    std::string out = p;
    for (auto& c : out) if (c == '\\' || c == '/') c = fs::path::preferred_separator;
    return out;
}

bool ReadWholeFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    std::streamsize size = f.tellg();
    if (size < 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize((size_t)size);
    return size == 0 || (bool)f.read(reinterpret_cast<char*>(out.data()), size);
}

} // namespace

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

bool MpqManager::OpenCasc(const std::string& storagePath) {
    for (auto& c : cascStorages_) {
        if (c.Path() == storagePath) return true;
    }
    CascStorage storage;
    if (!storage.Open(storagePath)) {
        std::cerr << "MpqStorm: failed to open CASC storage " << storagePath << std::endl;
        return false;
    }
    cascStorages_.push_back(std::move(storage));
    return true;
}

const CascStorage* MpqManager::FindFileInCasc(const std::string& filename) const {
    const std::string key = ToArchivePath(filename);
    for (const auto& c : cascStorages_) {
        if (c.FileExists(key)) return &c;
    }
    return nullptr;
}

void MpqManager::CloseAll() {
    for (auto& a : archives_) {
        if (a.handle) SFileCloseArchive(a.handle);
    }
    archives_.clear();
    for (auto& c : cascStorages_) c.Close();
    cascStorages_.clear();
    CloseTop();
}

bool MpqManager::OpenOnTop(const std::string& path) {
    for (const auto& a : topArchives_) {
        if (a.path == path) return true;
    }
    HANDLE h = nullptr;
    if (!SFileOpenArchive(path.c_str(), 0, MPQ_OPEN_READ_ONLY, &h)) {
        std::cerr << "MpqStorm: failed to mount " << path << std::endl;
        return false;
    }
    topArchives_.push_back({path, h});
    std::cout << "MpqStorm: mounted on top " << path << std::endl;
    return true;
}

void MpqManager::CloseTop() {
    for (auto& a : topArchives_) {
        if (a.handle) SFileCloseArchive(a.handle);
    }
    topArchives_.clear();
}

const MpqArchive* MpqManager::FindFileInArchives(const std::string& filename) const {
    const std::string key = ToArchivePath(filename);
    // Map mounts shadow game archives (last mounted wins).
    for (auto it = topArchives_.rbegin(); it != topArchives_.rend(); ++it) {
        if (SFileHasFile(it->handle, key.c_str())) return &(*it);
    }
    // Game archives: first opened wins so patches beat base data.
    for (const auto& a : archives_) {
        if (SFileHasFile(a.handle, key.c_str())) return &a;
    }
    return nullptr;
}

bool MpqManager::AddFolder(const std::string& path) {
    for (const auto& f : folders_) {
        if (f == path) return true;
    }
    std::error_code ec;
    if (!fs::is_directory(path, ec)) {
        std::cerr << "MpqStorm: folder overlay not a directory: " << path << std::endl;
        return false;
    }
    folders_.push_back(path);
    std::cout << "MpqStorm: folder overlay " << path << std::endl;
    return true;
}

bool MpqManager::FindFileInFolders(const std::string& filename, std::string& outPath) const {
    const std::string rel = ToNativePath(filename);
    for (auto it = folders_.rbegin(); it != folders_.rend(); ++it) {
        fs::path candidate = fs::path(*it) / rel;
        std::error_code ec;
        if (fs::is_regular_file(candidate, ec)) {
            outPath = candidate.string();
            return true;
        }
    }
    return false;
}

bool MpqManager::FileExists(const std::string& filename) const {
    std::string folderPath;
    if (FindFileInFolders(filename, folderPath)) return true;
    if (FindFileInArchives(filename) != nullptr) return true;
    return FindFileInCasc(filename) != nullptr;
}

bool MpqManager::ReadFile(const std::string& filename, std::vector<uint8_t>& out) const {
    // Folders shadow archives (ref FolderDataSource over MpqDataSource).
    std::string folderPath;
    if (FindFileInFolders(filename, folderPath)) {
        if (ReadWholeFile(folderPath, out)) return true;
        out.clear();
    }
    const auto* archive = FindFileInArchives(filename);
    if (archive) {
        const std::string key = ToArchivePath(filename);
        HANDLE hFile = nullptr;
        if (SFileOpenFileEx(archive->handle, key.c_str(), SFILE_OPEN_BASE_FILE, &hFile)) {
            DWORD fileSize = SFileGetFileSize(hFile, nullptr);
            out.resize(fileSize);
            DWORD bytesRead = 0;
            const bool ok =
                SFileReadFile(hFile, out.data(), fileSize, &bytesRead, nullptr);
            SFileCloseFile(hFile);
            if (ok) return true;
            out.clear();
        }
    }
    // CASC storages shadow MPQ game archives but never folders or maps.
    const auto* casc = FindFileInCasc(ToArchivePath(filename));
    if (casc) return casc->ReadFile(ToArchivePath(filename), out);
    return false;
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
    for (const auto& c : cascStorages_) {
        for (auto& name : c.ListFiles(pattern)) result.push_back(std::move(name));
    }
    return result;
}
