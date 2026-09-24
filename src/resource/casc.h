#pragma once
// Thin RAII wrapper over CascLib ( Blizzard CASC storage, used by
// 1.30+ installs instead of MPQs). Offline local storages only; no CDN.
// Encrypted files needing TACT keys are out of scope and read as missing.
// The HANDLE stays behind void* so this header needs no CascLib include.
#include <cstdint>
#include <string>
#include <vector>

#ifdef WITHOUT_CASCLIB
// No-op fallback so all translation units keep compiling without CascLib.
class CascStorage {
public:
    CascStorage() = default;
    ~CascStorage() = default;
    CascStorage(const CascStorage&) = delete;
    CascStorage& operator=(const CascStorage&) = delete;
    CascStorage(CascStorage&& other) noexcept = default;
    CascStorage& operator=(CascStorage&& other) noexcept = default;

    bool Open(const std::string&) { return false; }
    void Close() {}
    bool IsOpen() const { return false; }
    const std::string& Path() const { return path_; }

    bool FileExists(const std::string&) const { return false; }
    bool ReadFile(const std::string&, std::vector<uint8_t>&) const { return false; }
    std::vector<std::string> ListFiles(const std::string& = "*") const { return {}; }

private:
    void* storage_ = nullptr;
    std::string path_;
};
#else // WITH_CASCLIB

class CascStorage {
public:
    CascStorage() = default;
    ~CascStorage() { Close(); }

    CascStorage(const CascStorage&) = delete;
    CascStorage& operator=(const CascStorage&) = delete;
    CascStorage(CascStorage&& other) noexcept;
    CascStorage& operator=(CascStorage&& other) noexcept;

    // Opens a local storage directory (where .build.info lives).
    // Returns false (no throw, no crash) when the layout is absent.
    bool Open(const std::string& storagePath);
    void Close();

    bool IsOpen() const { return storage_ != nullptr; }
    const std::string& Path() const { return path_; }

    bool FileExists(const std::string& filename) const;
    bool ReadFile(const std::string& filename, std::vector<uint8_t>& out) const;
    std::vector<std::string> ListFiles(const std::string& mask = "*") const;

private:
    void* storage_ = nullptr;
    std::string path_;
};

#endif // WITH_CASCLIB
