#pragma once
// ============================================================================
// Engine Resource: IVfsArchiveProvider & VfsMountRegistry
//
// Pluggable priority-sorted Virtual File System (Godot ProjectSettings/DirAccess
// & Quake/Source VFS overlay style). Engine code queries `VfsMountRegistry`
// without knowing whether bytes come from loose folders, memory packs, or
// game-specific archive plugins (such as War3's `MpqArchiveProvider`).
// ============================================================================
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "engine/core/project_manifest.h"

namespace engine::vfs {

inline std::string NormalizeVfsKey(std::string_view rawPath) {
    std::string out;
    out.reserve(rawPath.size());
    size_t i = 0;
    while (i < rawPath.size() && (rawPath[i] == '/' || rawPath[i] == '\\')) {
        ++i;
    }
    for (; i < rawPath.size(); ++i) {
        char c = rawPath[i];
        if (c == '\\') c = '/';
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (c == '/' && !out.empty() && out.back() == '/') continue;
        out.push_back(c);
    }
    return out;
}

inline bool MatchGlobPattern(std::string_view pattern, std::string_view text) {
    if (pattern.empty() || pattern == "*") return true;
    size_t p = 0;
    size_t t = 0;
    size_t starIdx = std::string_view::npos;
    size_t matchIdx = 0;

    auto normChar = [](char ch) -> char {
        if (ch == '\\') ch = '/';
        return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    };

    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || normChar(pattern[p]) == normChar(text[t]))) {
            ++p;
            ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            starIdx = p++;
            matchIdx = t;
        } else if (starIdx != std::string_view::npos) {
            p = starIdx + 1;
            t = ++matchIdx;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') {
        ++p;
    }
    return p == pattern.size();
}

class IVfsArchiveProvider {
public:
    virtual ~IVfsArchiveProvider() = default;
    virtual const char* GetProviderType() const = 0;
    virtual const std::string& GetSourcePath() const = 0;
    virtual bool Contains(std::string_view virtualPath) const = 0;
    virtual bool ReadAllBytes(std::string_view virtualPath, std::vector<uint8_t>& outBytes) const = 0;
    virtual void EnumerateFiles(std::string_view globPattern, std::vector<std::string>& outFiles) const = 0;
};

class DirectoryArchiveProvider final : public IVfsArchiveProvider {
public:
    explicit DirectoryArchiveProvider(std::string rootDir)
        : rootDir_(std::move(rootDir)) {}

    bool IsDirectoryValid() const {
        std::error_code ec;
        return std::filesystem::is_directory(rootDir_, ec);
    }

    const char* GetProviderType() const override { return "folder"; }
    const std::string& GetSourcePath() const override { return rootDir_; }

    bool Contains(std::string_view virtualPath) const override {
        std::filesystem::path resolved;
        return ResolveFile(virtualPath, resolved);
    }

    bool ReadAllBytes(std::string_view virtualPath, std::vector<uint8_t>& outBytes) const override {
        std::filesystem::path resolved;
        if (!ResolveFile(virtualPath, resolved)) return false;
        std::ifstream ifs(resolved, std::ios::binary | std::ios::ate);
        if (!ifs) return false;
        const std::streamsize sz = ifs.tellg();
        if (sz < 0) return false;
        ifs.seekg(0, std::ios::beg);
        outBytes.resize(static_cast<size_t>(sz));
        return sz == 0 || static_cast<bool>(ifs.read(reinterpret_cast<char*>(outBytes.data()), sz));
    }

    void EnumerateFiles(std::string_view globPattern, std::vector<std::string>& outFiles) const override {
        std::error_code ec;
        if (!std::filesystem::is_directory(rootDir_, ec)) return;
        const std::filesystem::path base(rootDir_);
        for (const auto& entry : std::filesystem::recursive_directory_iterator(base, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            const std::string rel = std::filesystem::relative(entry.path(), base, ec).generic_string();
            if (!ec && MatchGlobPattern(globPattern, rel)) {
                outFiles.push_back(rel);
            }
        }
    }

private:
    bool ResolveFile(std::string_view virtualPath, std::filesystem::path& outPath) const {
        std::string rel(virtualPath);
        while (!rel.empty() && (rel.front() == '/' || rel.front() == '\\')) {
            rel.erase(rel.begin());
        }
        for (char& c : rel) {
            if (c == '\\' || c == '/') c = std::filesystem::path::preferred_separator;
        }
        if (rel.find("..") != std::string::npos) return false;
        std::error_code ec;
        const std::filesystem::path candidate = std::filesystem::path(rootDir_) / rel;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            outPath = candidate;
            return true;
        }
        return false;
    }

    std::string rootDir_;
};

class MemoryArchiveProvider final : public IVfsArchiveProvider {
public:
    explicit MemoryArchiveProvider(std::string label = "memory://overlay")
        : label_(std::move(label)) {}

    void PutFile(std::string_view virtualPath, std::vector<uint8_t> bytes) {
        const std::string key = NormalizeVfsKey(virtualPath);
        originalNames_[key] = std::string(virtualPath);
        files_[key] = std::move(bytes);
    }

    void PutText(std::string_view virtualPath, std::string_view text) {
        PutFile(virtualPath, std::vector<uint8_t>(text.begin(), text.end()));
    }

    void Clear() {
        files_.clear();
        originalNames_.clear();
    }

    const char* GetProviderType() const override { return "memory"; }
    const std::string& GetSourcePath() const override { return label_; }

    bool Contains(std::string_view virtualPath) const override {
        return files_.find(NormalizeVfsKey(virtualPath)) != files_.end();
    }

    bool ReadAllBytes(std::string_view virtualPath, std::vector<uint8_t>& outBytes) const override {
        auto it = files_.find(NormalizeVfsKey(virtualPath));
        if (it == files_.end()) return false;
        outBytes = it->second;
        return true;
    }

    void EnumerateFiles(std::string_view globPattern, std::vector<std::string>& outFiles) const override {
        for (const auto& kv : originalNames_) {
            if (MatchGlobPattern(globPattern, kv.second)) {
                outFiles.push_back(kv.second);
            }
        }
    }

private:
    std::string label_;
    std::unordered_map<std::string, std::vector<uint8_t>> files_;
    std::unordered_map<std::string, std::string> originalNames_;
};

class VfsMountRegistry {
public:
    using ProviderFactoryFn = std::function<std::shared_ptr<IVfsArchiveProvider>(const std::string& path)>;

    struct MountEntry {
        int priority = 0;
        uint64_t mountSequence = 0;
        std::string mountPoint = "/";
        std::shared_ptr<IVfsArchiveProvider> provider;
    };

    VfsMountRegistry() {
        RegisterProviderFactory("folder", [](const std::string& path) -> std::shared_ptr<IVfsArchiveProvider> {
            auto dir = std::make_shared<DirectoryArchiveProvider>(path);
            return dir->IsDirectoryValid() ? dir : nullptr;
        });
    }

    void RegisterProviderFactory(const std::string& typeName, ProviderFactoryFn factory) {
        if (!typeName.empty() && factory) {
            factories_[typeName] = std::move(factory);
        }
    }

    void Mount(std::shared_ptr<IVfsArchiveProvider> provider,
               int priority = 0,
               std::string mountPoint = "/") {
        if (!provider) return;
        mounts_.push_back({priority, ++nextSeq_, std::move(mountPoint), std::move(provider)});
        // Higher priority wins; on equal priority, more recently mounted wins
        std::sort(mounts_.begin(), mounts_.end(), [](const MountEntry& a, const MountEntry& b) {
            if (a.priority != b.priority) return a.priority > b.priority;
            return a.mountSequence > b.mountSequence;
        });
    }

    void UnmountAll() {
        mounts_.clear();
    }

    size_t UnmountBySource(std::string_view sourcePath) {
        const size_t before = mounts_.size();
        mounts_.erase(
            std::remove_if(mounts_.begin(), mounts_.end(), [&](const MountEntry& m) {
                return m.provider && m.provider->GetSourcePath() == sourcePath;
            }),
            mounts_.end());
        return before - mounts_.size();
    }

    // Mounts all archives declared in a ProjectManifest, returning count of active mounts
    // and failing only if a non-optional mount fails to open.
    bool MountFromManifest(const ProjectManifest& manifest,
                           const std::string& baseDir = "",
                           size_t* outMountedCount = nullptr,
                           std::string* outError = nullptr) {
        size_t mounted = 0;
        for (const auto& spec : manifest.vfsMounts) {
            auto fIt = factories_.find(spec.type);
            if (fIt == factories_.end()) {
                if (!spec.optional) {
                    if (outError) *outError = "No VFS provider registered for type: " + spec.type;
                    return false;
                }
                continue;
            }
            std::shared_ptr<IVfsArchiveProvider> prov = fIt->second(spec.path);
            if (!prov && !baseDir.empty()) {
                const std::string relCandidate =
                    (std::filesystem::path(baseDir) / spec.path).generic_string();
                prov = fIt->second(relCandidate);
            }
            if (!prov) {
                if (!spec.optional) {
                    if (outError) *outError = "Required VFS mount missing: " + spec.path;
                    return false;
                }
                continue;
            }
            Mount(std::move(prov), spec.priority, spec.mountPoint);
            ++mounted;
        }
        if (outMountedCount) *outMountedCount = mounted;
        return true;
    }

    bool Exists(std::string_view virtualPath) const {
        for (const auto& m : mounts_) {
            std::string subPath;
            if (StripMountPoint(virtualPath, m.mountPoint, subPath) &&
                m.provider->Contains(subPath)) {
                return true;
            }
        }
        return false;
    }

    bool ReadFile(std::string_view virtualPath, std::vector<uint8_t>& outBytes) const {
        for (const auto& m : mounts_) {
            std::string subPath;
            if (StripMountPoint(virtualPath, m.mountPoint, subPath) &&
                m.provider->ReadAllBytes(subPath, outBytes)) {
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> ListFiles(std::string_view globPattern = "*") const {
        std::vector<std::string> results;
        std::unordered_set<std::string> seenKeys;
        for (const auto& m : mounts_) {
            if (!m.provider) continue;
            const std::string normMount = NormalizeVfsKey(m.mountPoint);
            std::vector<std::string> mountFiles;
            m.provider->EnumerateFiles(normMount.empty() ? globPattern : std::string_view("*"), mountFiles);
            for (const std::string& rawFile : mountFiles) {
                std::string rel = rawFile;
                while (!rel.empty() && (rel.front() == '/' || rel.front() == '\\')) {
                    rel.erase(rel.begin());
                }
                const std::string fullVirtualPath =
                    normMount.empty() ? rel : (normMount + "/" + rel);
                if (!MatchGlobPattern(globPattern, fullVirtualPath)) {
                    continue;
                }
                if (seenKeys.insert(NormalizeVfsKey(fullVirtualPath)).second) {
                    results.push_back(fullVirtualPath);
                }
            }
        }
        return results;
    }

    size_t MountCount() const { return mounts_.size(); }
    const std::vector<MountEntry>& Mounts() const { return mounts_; }

private:
    static bool StripMountPoint(std::string_view reqPath,
                                std::string_view mountPoint,
                                std::string& outSubPath) {
        const std::string normReq = NormalizeVfsKey(reqPath);
        const std::string normMount = NormalizeVfsKey(mountPoint);
        if (normMount.empty()) {
            outSubPath = std::string(reqPath);
            return true;
        }
        if (normReq == normMount) {
            outSubPath.clear();
            return true;
        }
        if (normReq.size() > normMount.size() &&
            normReq.compare(0, normMount.size(), normMount) == 0 &&
            normReq[normMount.size()] == '/') {
            outSubPath = normReq.substr(normMount.size() + 1);
            return true;
        }
        return false;
    }

    uint64_t nextSeq_ = 0;
    std::vector<MountEntry> mounts_;
    std::unordered_map<std::string, ProviderFactoryFn> factories_;
};

} // namespace engine::vfs
