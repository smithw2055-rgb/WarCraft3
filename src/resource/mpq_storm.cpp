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
    if (const MpqArchive* top = FindFileInTopArchives(filename)) return top;
    return FindFileInGameArchives(filename);
}

const MpqArchive* MpqManager::FindFileInTopArchives(const std::string& filename) const {
    const std::string key = ToArchivePath(filename);
    // Map mounts shadow game archives (last mounted wins).
    for (auto it = topArchives_.rbegin(); it != topArchives_.rend(); ++it) {
        if (SFileHasFile(it->handle, key.c_str())) return &(*it);
    }
    return nullptr;
}

const MpqArchive* MpqManager::FindFileInGameArchives(const std::string& filename) const {
    const std::string key = ToArchivePath(filename);
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
    if (FindFileInTopArchives(filename) != nullptr) return true;
    if (FindFileInCasc(filename) != nullptr) return true;
    if (FindFileInGameArchives(filename) != nullptr) return true;
    const MpqArchive* impArchive = nullptr;
    return FindMapImport(filename, folderPath, &impArchive);
}

namespace {
bool ReadFromArchive(const MpqArchive* archive, const std::string& filename,
                     std::vector<uint8_t>& out) {
    if (!archive) return false;
    const std::string key = ToArchivePath(filename);
    HANDLE hFile = nullptr;
    if (!SFileOpenFileEx(archive->handle, key.c_str(), SFILE_OPEN_BASE_FILE, &hFile))
        return false;
    DWORD fileSize = SFileGetFileSize(hFile, nullptr);
    if (fileSize == 0xFFFFFFFFu) {
        SFileCloseFile(hFile);
        return false;
    }
    if (fileSize == 0) {
        out.clear();
        SFileCloseFile(hFile);
        return true;
    }
    out.resize(fileSize);
    DWORD bytesRead = 0;
    const bool ok = SFileReadFile(hFile, out.data(), fileSize, &bytesRead, nullptr);
    SFileCloseFile(hFile);
    if (!ok) out.clear();
    return ok;
}
} // namespace

bool MpqManager::ReadFile(const std::string& filename, std::vector<uint8_t>& out) const {
    // Lookup order (matches header + ref CompoundDataSource newest-first):
    // folders → map mounts → CASC → game MPQs → map-import fallback.
    // CASC shadows MPQ game archives (1.30+ live data beats stale MPQs)
    // but never folders or map mounts.
    std::string folderPath;
    if (FindFileInFolders(filename, folderPath)) {
        if (ReadWholeFile(folderPath, out)) return true;
        out.clear();
    }
    if (ReadFromArchive(FindFileInTopArchives(filename), filename, out)) return true;
    // CASC storages shadow MPQ game archives but never folders or maps.
    const auto* casc = FindFileInCasc(ToArchivePath(filename));
    if (casc) return casc->ReadFile(ToArchivePath(filename), out);
    if (ReadFromArchive(FindFileInGameArchives(filename), filename, out)) return true;
    // Map import fallback (war3mapImported\<path>).
    std::string impFolder;
    const MpqArchive* impArchive = nullptr;
    if (FindMapImport(filename, impFolder, &impArchive)) {
        if (!impFolder.empty() && ReadWholeFile(impFolder, out)) return true;
        if (impArchive) {
            const std::string key = ToArchivePath(std::string("war3mapImported\\") + ToArchivePath(filename));
            HANDLE hFile = nullptr;
            if (SFileOpenFileEx(impArchive->handle, key.c_str(), SFILE_OPEN_BASE_FILE, &hFile)) {
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
    }
    return false;
}

bool MpqManager::FindMapImport(const std::string& filename, std::string& folderOut,
                              const MpqArchive** archiveOut) const {
    const std::string key = ToArchivePath(filename);
    // Already prefixed: direct map-mount hit counts, no recursion.
    if (key.size() >= 17 && (key.compare(0, 17, "war3mapImported\\") == 0 ||
                             key.compare(0, 17, "war3mapImported/") == 0)) {
        if (archiveOut) *archiveOut = nullptr;
        return false;
    }
    const std::string prefixed = std::string("war3mapImported\\") + key;
    const std::string rel = ToNativePath(prefixed);
    for (auto it = folders_.rbegin(); it != folders_.rend(); ++it) {
        fs::path candidate = fs::path(*it) / rel;
        std::error_code ec;
        if (fs::is_regular_file(candidate, ec)) {
            folderOut = candidate.string();
            if (archiveOut) *archiveOut = nullptr;
            return true;
        }
    }
    for (auto it = topArchives_.rbegin(); it != topArchives_.rend(); ++it) {
        if (SFileHasFile(it->handle, prefixed.c_str())) {
            folderOut.clear();
            if (archiveOut) *archiveOut = &(*it);
            return true;
        }
    }
    return false;
}

std::vector<std::string> MpqManager::ListFiles(const std::string& archivePath) const {
    std::vector<std::string> result;
    auto listArchive = [&](const MpqArchive& a) {
        SFILE_FIND_DATA fd;
        HANDLE hFind = SFileFindFirstFile(a.handle, "*", &fd, nullptr);
        if (!hFind) return;

        do { result.push_back(fd.cFileName); }
        while (SFileFindNextFile(hFind, &fd));
        SFileFindClose(hFind);
    };
    // Map mounts first (they shadow game archives), then game archives.
    for (const auto& a : topArchives_) {
        if (!archivePath.empty() && a.path != archivePath) continue;
        listArchive(a);
    }
    for (const auto& a : archives_) {
        if (!archivePath.empty() && a.path != archivePath) continue;
        listArchive(a);
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
