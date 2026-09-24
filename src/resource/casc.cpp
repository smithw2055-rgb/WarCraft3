#include "resource/casc.h"

#ifdef WITHOUT_CASCLIB
// Header-only fallback; nothing to compile.
#else // WITH_CASCLIB

#include <algorithm>
#include <cstring>
#include <iostream>

#include "CascLib.h"

CascStorage::CascStorage(CascStorage&& other) noexcept
    : storage_(other.storage_), path_(std::move(other.path_)) {
    other.storage_ = nullptr;
}

CascStorage& CascStorage::operator=(CascStorage&& other) noexcept {
    if (this != &other) {
        Close();
        storage_ = other.storage_;
        path_ = std::move(other.path_);
        other.storage_ = nullptr;
    }
    return *this;
}

bool CascStorage::Open(const std::string& storagePath) {
    Close();
    HANDLE hStorage = nullptr;
    if (!CascOpenStorage(storagePath.c_str(), CASC_LOCALE_ALL, &hStorage) ||
        !hStorage) {
        return false;
    }
    storage_ = hStorage;
    path_ = storagePath;
    std::cout << "CascStorage: opened " << storagePath << std::endl;
    return true;
}

void CascStorage::Close() {
    if (storage_) {
        CascCloseStorage((HANDLE)storage_);
        storage_ = nullptr;
    }
    path_.clear();
}

bool CascStorage::FileExists(const std::string& filename) const {
    if (!storage_ || filename.empty()) return false;
    HANDLE hFile = nullptr;
    if (!CascOpenFile((HANDLE)storage_, filename.c_str(), 0, 0, &hFile) || !hFile) {
        return false;
    }
    CascCloseFile(hFile);
    return true;
}

bool CascStorage::ReadFile(const std::string& filename,
                           std::vector<uint8_t>& out) const {
    out.clear();
    if (!storage_ || filename.empty()) return false;
    HANDLE hFile = nullptr;
    if (!CascOpenFile((HANDLE)storage_, filename.c_str(), 0, 0, &hFile) || !hFile) {
        return false;
    }
    ULONGLONG fileSize = 0;
    bool ok = false;
    if (CascGetFileSize64(hFile, &fileSize) && fileSize > 0 &&
        fileSize <= 512ULL * 1024ULL * 1024ULL) {
        out.resize((size_t)fileSize);
        size_t total = 0;
        ok = true;
        while (total < (size_t)fileSize) {
            DWORD chunk =
                (DWORD)std::min<ULONGLONG>((ULONGLONG)(size_t)fileSize - total, 16ULL * 1024ULL * 1024ULL);
            DWORD bytesRead = 0;
            if (!CascReadFile(hFile, out.data() + total, chunk, &bytesRead) ||
                bytesRead == 0) {
                ok = false;
                break;
            }
            total += bytesRead;
        }
        if (!ok || total != (size_t)fileSize) {
            ok = false;
        }
    }
    CascCloseFile(hFile);
    if (!ok) out.clear();
    return ok;
}

std::vector<std::string> CascStorage::ListFiles(const std::string& mask) const {
    std::vector<std::string> result;
    if (!storage_) return result;
    CASC_FIND_DATA findData;
    std::memset(&findData, 0, sizeof(findData));
    HANDLE hFind =
        CascFindFirstFile((HANDLE)storage_, mask.c_str(), &findData, nullptr);
    if (!hFind) return result;
    do {
        result.emplace_back(findData.szFileName);
    } while (CascFindNextFile(hFind, &findData));
    CascFindClose(hFind);
    return result;
}

#endif // WITH_CASCLIB
