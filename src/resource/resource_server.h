#pragma once
// ============================================================================
// ResourceServer & VFS URI Pipeline (Phase 6 Engine Modernization - Stage E & Stage J)
//
// Inspired by Godot 4 ResourceLoader / RenderingServer RID & Unity Addressables:
// 1. Strongly-typed 64-bit RID (Resource ID) + intrusive/shared ResourceRef<T>.
// 2. Unified Virtual File System (VFS) URI resolution & canonical path key:
//    - Supports `mpq://`, `map://`, `local://`, `res://`, and raw Warcraft 3 paths.
//    - Canonicalizes slashes (`/` -> `\`) and ASCII case for O(1) cache hits.
// 3. Central reference-counted cache for:
//    - `RawBinaryResource` (raw archive/file bytes)
//    - `SlkTableResource` (parsed SLK spreadsheet tables)
//    - `TxtTableResource` (parsed INI/TXT key-value tables)
//    - `BlpImageResource` (decoded RGBA8 mipmaps in CPU memory)
//    - `MdxModelResource` (parsed MDX binary or MDL text model AST)
//    - `AudioClipResource` (Stage J: validated WAV PCM / MP3 / FLAC / OGG audio buffer & metadata)
// 4. Background Async Preload Queue (`RequestAsyncPreload` / `PollAsyncCompletions`)
//    so heavy CPU decoding (MDX/MDL/BLP/SLK/Audio) runs off the main thread while GPU
//    upload (`sokol_gfx`) stays cleanly on the main thread.
// ============================================================================
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "asset/BLP.h"
#include "model/mdl.h"
#include "model/mdx.h"
#include "resource/mpq_storm.h"
#include "resource/slk.h"
#include "engine/core/project_manifest.h"
#include "engine/resource/format_loader.h"
#include "engine/resource/vfs_archive.h"

namespace resource {

enum class VfsScheme : uint8_t {
    Auto = 0,   // Default overlay order (Folder -> Map -> CASC -> Base MPQ)
    Mpq  = 1,   // mpq://
    Map  = 2,   // map://
    Local = 3,  // local://
    Res  = 4    // res://
};

enum class ResourceKind : uint8_t {
    Unknown = 0,
    RawBinary = 1,
    SlkTable = 2,
    TxtTable = 3,
    BlpImage = 4,
    MdxModel = 5,
    AudioClip = 6
};

enum class AudioFormat : uint8_t {
    Unknown = 0,
    WavPcm = 1,
    Mp3 = 2,
    Flac = 3,
    Ogg = 4,
    SilenceOverride = 5
};

enum class ResourceLoadState : uint8_t {
    Unloaded = 0,
    Loading  = 1,
    Loaded   = 2,
    Failed   = 3
};

// 64-bit Resource ID (upper 32 bits = generation/kind, lower 32 bits = slot id)
struct RID {
    uint64_t id = 0;

    constexpr RID() = default;
    constexpr explicit RID(uint64_t v) : id(v) {}
    constexpr RID(uint32_t index, uint32_t generation, ResourceKind kind)
        : id((static_cast<uint64_t>(kind) << 56) |
             (static_cast<uint64_t>(generation & 0x00FFFFFFu) << 32) |
             static_cast<uint64_t>(index)) {}

    constexpr bool IsValid() const { return id != 0; }
    constexpr uint32_t Index() const { return static_cast<uint32_t>(id & 0xFFFFFFFFu); }
    constexpr uint32_t Generation() const { return static_cast<uint32_t>((id >> 32) & 0x00FFFFFFu); }
    constexpr ResourceKind Kind() const { return static_cast<ResourceKind>((id >> 56) & 0xFFu); }

    constexpr bool operator==(const RID& other) const { return id == other.id; }
    constexpr bool operator!=(const RID& other) const { return id != other.id; }
    constexpr bool operator<(const RID& other) const { return id < other.id; }
};

struct RIDHasher {
    size_t operator()(const RID& r) const noexcept {
        return std::hash<uint64_t>{}(r.id);
    }
};

struct ParsedVfsUri {
    VfsScheme scheme = VfsScheme::Auto;
    std::string rawArchivePath;     // Normalized with backslashes for MPQ/filesystem lookup
    std::string canonicalCacheKey;  // Lowercase canonical key with backslashes for deduplication
};

// Base class for all managed engine resources
class Resource {
public:
    virtual ~Resource() = default;

    RID GetRid() const { return rid_; }
    ResourceKind GetKind() const { return kind_; }
    ResourceLoadState GetState() const { return state_; }
    const std::string& GetPath() const { return path_; }
    const std::string& GetCanonicalKey() const { return canonicalKey_; }
    virtual size_t EstimateMemoryBytes() const = 0;

protected:
    friend class ResourceServer;
    RID rid_{};
    ResourceKind kind_ = ResourceKind::Unknown;
    ResourceLoadState state_ = ResourceLoadState::Unloaded;
    std::string path_;
    std::string canonicalKey_;
};

template <typename T>
using ResourceRef = std::shared_ptr<T>;

class RawBinaryResource final : public Resource {
public:
    static constexpr ResourceKind kStaticKind = ResourceKind::RawBinary;
    RawBinaryResource() { kind_ = ResourceKind::RawBinary; }
    std::vector<uint8_t> bytes;
    size_t EstimateMemoryBytes() const override { return sizeof(*this) + bytes.capacity(); }
};

class SlkTableResource final : public Resource {
public:
    static constexpr ResourceKind kStaticKind = ResourceKind::SlkTable;
    SlkTableResource() { kind_ = ResourceKind::SlkTable; }
    SlkTable table;
    size_t byteSize = 0;
    size_t EstimateMemoryBytes() const override { return sizeof(*this) + byteSize; }
};

class TxtTableResource final : public Resource {
public:
    static constexpr ResourceKind kStaticKind = ResourceKind::TxtTable;
    TxtTableResource() { kind_ = ResourceKind::TxtTable; }
    TxtTable table;
    size_t byteSize = 0;
    size_t EstimateMemoryBytes() const override { return sizeof(*this) + byteSize; }
};

class BlpImageResource final : public Resource {
public:
    static constexpr ResourceKind kStaticKind = ResourceKind::BlpImage;
    BlpImageResource() { kind_ = ResourceKind::BlpImage; }
    BLPImage baseImage{};
    std::vector<BLPImage> mipChain;
    size_t EstimateMemoryBytes() const override {
        size_t total = sizeof(*this) + baseImage.pixels.capacity();
        for (const auto& mip : mipChain) total += mip.pixels.capacity();
        return total;
    }
};

class MdxModelResource final : public Resource {
public:
    static constexpr ResourceKind kStaticKind = ResourceKind::MdxModel;
    MdxModelResource() { kind_ = ResourceKind::MdxModel; }
    MdxModel model;
    size_t sourceByteSize = 0;
    bool loadedFromMdlText = false;
    size_t EstimateMemoryBytes() const override {
        return sizeof(*this) + sourceByteSize +
               model.geosets.size() * 256 +
               model.bones.size() * 128 +
               model.sequences.size() * 64;
    }
};

// Stage J: Managed Audio Clip Resource (WAV RIFF/WAVE chunk parser + MP3/FLAC/OGG container support)
class AudioClipResource final : public Resource {
public:
    static constexpr ResourceKind kStaticKind = ResourceKind::AudioClip;
    AudioClipResource() { kind_ = ResourceKind::AudioClip; }

    std::vector<uint8_t> bytes;
    AudioFormat format = AudioFormat::Unknown;
    uint16_t audioFormatTag = 0; // 1 = PCM
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint32_t byteRate = 0;
    uint16_t bitsPerSample = 0;
    uint32_t pcmDataOffset = 0;
    uint32_t pcmDataSize = 0;
    float durationSeconds = 0.0f;
    bool isSilenceOverride = false;

    static bool ParseAudioContainer(const std::string& pathHint,
                                    const std::vector<uint8_t>& rawBytes,
                                    AudioClipResource& outMeta);

    size_t EstimateMemoryBytes() const override { return sizeof(*this) + bytes.capacity(); }
};

struct ResourceServerStats {
    uint64_t cacheHits = 0;
    uint64_t cacheMisses = 0;
    uint64_t loadFailures = 0;
    uint64_t asyncJobsSubmitted = 0;
    uint64_t asyncJobsCompleted = 0;
    size_t activeResourceCount = 0;
    size_t totalEstimatedBytes = 0;
};

class ResourceServer {
public:
    using ByteReaderFn = std::function<bool(const std::string& archivePath, std::vector<uint8_t>& outBytes)>;
    using AsyncCompletionCallback = std::function<void(RID rid, ResourceRef<Resource> resource, bool success)>;

    ResourceServer();
    ~ResourceServer();

    static ResourceServer& Instance();

    // Parse and canonicalize a VFS URI (`mpq://Units/Human/Footman/Footman.mdx` ->
    // archivePath: `Units\Human\Footman\Footman.mdx`, canonicalKey: `units\human\footman\footman.mdx`)
    static ParsedVfsUri ParseUri(const std::string& uriOrPath);
    static std::string SiblingModelExtensionPath(const std::string& archivePath);

    // Bind an MpqManager data source or a custom virtual byte provider (for tests / memory VFS)
    void SetMpqManager(const MpqManager* mpq);
    bool MountProjectManifest(const engine::ProjectManifest& manifest,
                              const std::string& baseDir = ".");
    engine::vfs::VfsMountRegistry& Vfs() { return vfsRegistry_; }
    const engine::vfs::VfsMountRegistry& Vfs() const { return vfsRegistry_; }
    engine::resource::FormatLoaderRegistry& FormatLoaders() { return formatRegistry_; }
    const engine::resource::FormatLoaderRegistry& FormatLoaders() const { return formatRegistry_; }
    const MpqManager* GetMpqManager() const { return mpq_; }

    void SetCustomByteReader(ByteReaderFn reader);
    void MountMemoryFile(const std::string& virtualPath, std::vector<uint8_t> bytes);
    void UnmountMemoryFiles();

    // Phase 29: VFS Path Aliasing / Redirection (e.g. HD asset pack or module override)
    void RegisterPathAlias(const std::string& fromUriOrPath, const std::string& toUriOrPath);
    bool UnregisterPathAlias(const std::string& fromUriOrPath);
    void ClearPathAliases();
    std::string ResolvePathAlias(const std::string& uriOrPath) const;
    size_t GetPathAliasCount() const;

    // Read raw file bytes through the VFS layer (Memory Overlay -> Custom Reader -> MpqManager -> Local Disk)
    bool ReadRawBytes(const std::string& uriOrPath, std::vector<uint8_t>& outBytes) const;
    bool FileExists(const std::string& uriOrPath) const;

    // Synchronous typed resource loaders (with automatic canonical-path deduplication & caching)
    ResourceRef<RawBinaryResource> LoadRawBinary(const std::string& uriOrPath);
    ResourceRef<SlkTableResource> LoadSlkTable(const std::string& uriOrPath);
    ResourceRef<TxtTableResource> LoadTxtTable(const std::string& uriOrPath);
    ResourceRef<BlpImageResource> LoadBlpImage(const std::string& uriOrPath, bool decodeAllMips = false);
    ResourceRef<MdxModelResource> LoadMdxModel(const std::string& uriOrPath);
    ResourceRef<AudioClipResource> LoadAudioClip(const std::string& uriOrPath);

    // Generic template helper
    template <typename T>
    ResourceRef<T> Load(const std::string& uriOrPath);

    // Lookup existing resource by RID or path without triggering a load
    ResourceRef<Resource> GetByRid(RID rid) const;
    ResourceRef<Resource> FindCached(const std::string& uriOrPath, ResourceKind kind) const;

    template <typename T>
    ResourceRef<T> GetTypedByRid(RID rid) const {
        auto base = GetByRid(rid);
        if (base && base->GetKind() == T::kStaticKind) {
            return std::static_pointer_cast<T>(base);
        }
        return nullptr;
    }

    // Asynchronous preload queue: decodes resource on background worker thread and invokes
    // optional `onComplete` callback on the main thread when `PollAsyncCompletions()` is called.
    void StartAsyncWorker();
    void StopAsyncWorker();
    void RequestAsyncPreload(const std::string& uriOrPath,
                             ResourceKind kind,
                             AsyncCompletionCallback onComplete = nullptr);
    // Dispatches completed async callbacks on the calling (main) thread; returns number of completed items.
    size_t PollAsyncCompletions();
    // Blocks until all currently queued async preload requests finish and dispatches their callbacks.
    void FlushAsyncPreloads();

    // Garbage collection: removes cached resources whose only remaining reference is the cache itself (`use_count() == 1`).
    // Returns the number of evicted resources.
    size_t PruneUnusedResources();
    void ClearAll();

    ResourceServerStats GetStats() const;

private:
    RID AllocateRidLocked(ResourceKind kind);
    std::string MakeTypedCacheKey(const std::string& canonicalPath, ResourceKind kind) const;
    ResourceRef<Resource> LoadByKindInternal(const std::string& uriOrPath, ResourceKind kind);
    void WorkerLoop();

    mutable std::mutex mutex_;
    const MpqManager* mpq_ = nullptr;
    std::weak_ptr<const int> mpqLifetime_;
    ByteReaderFn customReader_;
    std::unordered_map<std::string, std::vector<uint8_t>> memoryFiles_;

    uint32_t nextIndex_ = 1;
    uint32_t generation_ = 1;
    std::unordered_map<RID, ResourceRef<Resource>, RIDHasher> resourcesByRid_;
    std::unordered_map<std::string, RID> ridByTypedKey_;

    mutable uint64_t cacheHits_ = 0;
    mutable uint64_t cacheMisses_ = 0;
    mutable uint64_t loadFailures_ = 0;
    uint64_t asyncJobsSubmitted_ = 0;
    uint64_t asyncJobsCompleted_ = 0;

    struct AsyncJob {
        std::string uri;
        ResourceKind kind = ResourceKind::RawBinary;
        AsyncCompletionCallback callback;
    };
    struct AsyncCompletedJob {
        RID rid;
        ResourceRef<Resource> resource;
        bool success = false;
        AsyncCompletionCallback callback;
    };

    std::mutex asyncMutex_;
    std::condition_variable asyncCv_;
    std::condition_variable asyncDoneCv_;
    std::deque<AsyncJob> asyncPending_;
    std::deque<AsyncCompletedJob> asyncCompleted_;
    bool workerRunning_ = false;
    bool workerBusy_ = false;
    std::thread workerThread_;
    std::unordered_map<std::string, std::string> pathAliases_;
    engine::vfs::VfsMountRegistry vfsRegistry_;
    engine::resource::FormatLoaderRegistry formatRegistry_;

    template <typename T, typename BuilderFn>
    ResourceRef<T> LoadOrInsertCached(const std::string& typedKey,
                                      ResourceKind kind,
                                      BuilderFn&& buildUnlocked);
};

template <>
inline ResourceRef<RawBinaryResource> ResourceServer::Load<RawBinaryResource>(const std::string& uriOrPath) {
    return LoadRawBinary(uriOrPath);
}

template <>
inline ResourceRef<SlkTableResource> ResourceServer::Load<SlkTableResource>(const std::string& uriOrPath) {
    return LoadSlkTable(uriOrPath);
}

template <>
inline ResourceRef<TxtTableResource> ResourceServer::Load<TxtTableResource>(const std::string& uriOrPath) {
    return LoadTxtTable(uriOrPath);
}

template <>
inline ResourceRef<BlpImageResource> ResourceServer::Load<BlpImageResource>(const std::string& uriOrPath) {
    return LoadBlpImage(uriOrPath, false);
}

template <>
inline ResourceRef<MdxModelResource> ResourceServer::Load<MdxModelResource>(const std::string& uriOrPath) {
    return LoadMdxModel(uriOrPath);
}

template <>
inline ResourceRef<AudioClipResource> ResourceServer::Load<AudioClipResource>(const std::string& uriOrPath) {
    return LoadAudioClip(uriOrPath);
}

} // namespace resource
