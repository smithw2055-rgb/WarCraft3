#include "resource/resource_server.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <utility>

namespace resource {

namespace {

std::string ToLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

uint16_t ReadLE16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t ReadLE32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

} // namespace

bool AudioClipResource::ParseAudioContainer(const std::string& pathHint,
                                            const std::vector<uint8_t>& rawBytes,
                                            AudioClipResource& outMeta) {
    outMeta.format = AudioFormat::Unknown;
    outMeta.audioFormatTag = 0;
    outMeta.channels = 0;
    outMeta.sampleRate = 0;
    outMeta.byteRate = 0;
    outMeta.bitsPerSample = 0;
    outMeta.pcmDataOffset = 0;
    outMeta.pcmDataSize = 0;
    outMeta.durationSeconds = 0.0f;
    outMeta.isSilenceOverride = false;

    if (rawBytes.empty()) {
        // Retail custom maps (e.g. AzerothGrandPrix.w3x) embed 0-byte WAV overrides to mute default unit sounds
        outMeta.format = AudioFormat::SilenceOverride;
        outMeta.isSilenceOverride = true;
        return true;
    }

    // 1. Check RIFF/WAVE container (handles arbitrary JUNK/fact/LIST chunks before data)
    if (rawBytes.size() >= 12 &&
        std::memcmp(rawBytes.data(), "RIFF", 4) == 0 &&
        std::memcmp(rawBytes.data() + 8, "WAVE", 4) == 0) {
        outMeta.format = AudioFormat::WavPcm;
        size_t pos = 12;
        bool foundFmt = false;
        while (pos + 8 <= rawBytes.size()) {
            const uint8_t* chunkHdr = rawBytes.data() + pos;
            uint32_t chunkSize = ReadLE32(chunkHdr + 4);
            pos += 8;
            size_t avail = rawBytes.size() - pos;
            size_t safeStep = std::min<size_t>(chunkSize, avail);

            if (std::memcmp(chunkHdr, "fmt ", 4) == 0 && safeStep >= 16) {
                outMeta.audioFormatTag = ReadLE16(rawBytes.data() + pos);
                outMeta.channels = ReadLE16(rawBytes.data() + pos + 2);
                outMeta.sampleRate = ReadLE32(rawBytes.data() + pos + 4);
                outMeta.byteRate = ReadLE32(rawBytes.data() + pos + 8);
                outMeta.bitsPerSample = ReadLE16(rawBytes.data() + pos + 14);
                foundFmt = true;
            } else if (std::memcmp(chunkHdr, "data", 4) == 0) {
                outMeta.pcmDataOffset = static_cast<uint32_t>(pos);
                outMeta.pcmDataSize = static_cast<uint32_t>(safeStep);
                break;
            }

            pos += safeStep;
            // RIFF chunks are word-aligned (padded to even byte boundary)
            if ((chunkSize & 1u) != 0 && pos < rawBytes.size()) {
                pos += 1;
            }
        }
        if (foundFmt && outMeta.byteRate > 0 && outMeta.pcmDataSize > 0) {
            outMeta.durationSeconds = static_cast<float>(outMeta.pcmDataSize) /
                                      static_cast<float>(outMeta.byteRate);
        }
        return foundFmt;
    }

    // 2. Check FLAC ("fLaC"), OGG ("OggS"), or MP3 ("ID3" / 0xFF 0xFB..0xFF)
    if (rawBytes.size() >= 4 && std::memcmp(rawBytes.data(), "fLaC", 4) == 0) {
        outMeta.format = AudioFormat::Flac;
        return true;
    }
    if (rawBytes.size() >= 4 && std::memcmp(rawBytes.data(), "OggS", 4) == 0) {
        outMeta.format = AudioFormat::Ogg;
        return true;
    }
    if ((rawBytes.size() >= 3 && std::memcmp(rawBytes.data(), "ID3", 3) == 0) ||
        (rawBytes.size() >= 2 && rawBytes[0] == 0xFF && (rawBytes[1] & 0xE0) == 0xE0)) {
        outMeta.format = AudioFormat::Mp3;
        return true;
    }

    const std::string lowerPath = ToLowerAscii(pathHint);
    if (lowerPath.size() >= 4 && lowerPath.substr(lowerPath.size() - 4) == ".mp3") {
        outMeta.format = AudioFormat::Mp3;
        return true;
    }
    return false;
}

ResourceServer::ResourceServer() = default;

ResourceServer::~ResourceServer() {
    StopAsyncWorker();
}

ResourceServer& ResourceServer::Instance() {
    static ResourceServer instance;
    return instance;
}

ParsedVfsUri ResourceServer::ParseUri(const std::string& uriOrPath) {
    ParsedVfsUri out;
    std::string body = uriOrPath;

    // Trim leading/trailing whitespace
    while (!body.empty() && (body.front() == ' ' || body.front() == '\t')) body.erase(body.begin());
    while (!body.empty() && (body.back() == ' ' || body.back() == '\t')) body.pop_back();

    // Detect scheme prefix (`mpq://`, `map://`, `local://`, `res://`)
    const size_t schemePos = body.find("://");
    if (schemePos != std::string::npos) {
        const std::string schemeStr = ToLowerAscii(body.substr(0, schemePos));
        body = body.substr(schemePos + 3);
        if (schemeStr == "mpq") out.scheme = VfsScheme::Mpq;
        else if (schemeStr == "map") out.scheme = VfsScheme::Map;
        else if (schemeStr == "local" || schemeStr == "file") out.scheme = VfsScheme::Local;
        else if (schemeStr == "res") out.scheme = VfsScheme::Res;
        else out.scheme = VfsScheme::Auto;
    }

    // Strip leading slashes (`mpq:///Units/...` -> `Units/...`) unless it is an absolute POSIX local path
    if (out.scheme != VfsScheme::Local) {
        while (!body.empty() && (body.front() == '/' || body.front() == '\\')) {
            body.erase(body.begin());
        }
    }

    // Normalize path separators to Warcraft 3 backslash convention and collapse duplicate slashes
    std::string normalized;
    normalized.reserve(body.size());
    for (char c : body) {
        char norm = (c == '/') ? '\\' : c;
        if (norm == '\\' && !normalized.empty() && normalized.back() == '\\') {
            continue;
        }
        normalized.push_back(norm);
    }

    out.rawArchivePath = normalized;
    out.canonicalCacheKey = ToLowerAscii(normalized);
    return out;
}

std::string ResourceServer::SiblingModelExtensionPath(const std::string& archivePath) {
    if (archivePath.size() < 4) return {};
    const std::string ext = ToLowerAscii(archivePath.substr(archivePath.size() - 4));
    if (ext == ".mdx") {
        return archivePath.substr(0, archivePath.size() - 4) + ".mdl";
    }
    if (ext == ".mdl") {
        return archivePath.substr(0, archivePath.size() - 4) + ".mdx";
    }
    return {};
}

void ResourceServer::SetMpqManager(const MpqManager* mpq) {
    std::lock_guard<std::mutex> lock(mutex_);
    mpq_ = mpq;
}

void ResourceServer::SetCustomByteReader(ByteReaderFn reader) {
    std::lock_guard<std::mutex> lock(mutex_);
    customReader_ = std::move(reader);
}

void ResourceServer::MountMemoryFile(const std::string& virtualPath, std::vector<uint8_t> bytes) {
    const ParsedVfsUri parsed = ParseUri(virtualPath);
    std::lock_guard<std::mutex> lock(mutex_);
    memoryFiles_[parsed.canonicalCacheKey] = std::move(bytes);
}

void ResourceServer::UnmountMemoryFiles() {
    std::lock_guard<std::mutex> lock(mutex_);
    memoryFiles_.clear();
}

bool ResourceServer::ReadRawBytes(const std::string& uriOrPath, std::vector<uint8_t>& outBytes) const {
    const ParsedVfsUri parsed = ParseUri(uriOrPath);
    if (parsed.rawArchivePath.empty()) return false;

    const MpqManager* mpq = nullptr;
    ByteReaderFn customReader;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto memIt = memoryFiles_.find(parsed.canonicalCacheKey);
        if (memIt != memoryFiles_.end()) {
            outBytes = memIt->second;
            return true;
        }
        mpq = mpq_;
        customReader = customReader_;
    }

    if (customReader && customReader(parsed.rawArchivePath, outBytes)) {
        return true;
    }

    if (mpq && parsed.scheme != VfsScheme::Local) {
        if (mpq->ReadFile(parsed.rawArchivePath, outBytes)) {
            return true;
        }
    }

    // Fallback: direct local filesystem read (with `/` separator support on Linux/macOS)
    std::string fsPath = parsed.rawArchivePath;
    for (char& c : fsPath) {
        if (c == '\\') c = '/';
    }
    std::ifstream ifs(fsPath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) return false;
    const std::streamsize sz = ifs.tellg();
    if (sz < 0) return false;
    outBytes.resize(static_cast<size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    return sz == 0 || static_cast<bool>(ifs.read(reinterpret_cast<char*>(outBytes.data()), sz));
}

bool ResourceServer::FileExists(const std::string& uriOrPath) const {
    const ParsedVfsUri parsed = ParseUri(uriOrPath);
    if (parsed.rawArchivePath.empty()) return false;

    const MpqManager* mpq = nullptr;
    bool hasCustom = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (memoryFiles_.count(parsed.canonicalCacheKey) > 0) return true;
        mpq = mpq_;
        hasCustom = static_cast<bool>(customReader_);
    }

    if (hasCustom) {
        std::vector<uint8_t> tmp;
        if (ReadRawBytes(uriOrPath, tmp)) return true;
    }

    if (mpq && parsed.scheme != VfsScheme::Local && mpq->FileExists(parsed.rawArchivePath)) {
        return true;
    }

    std::string fsPath = parsed.rawArchivePath;
    for (char& c : fsPath) {
        if (c == '\\') c = '/';
    }
    std::ifstream ifs(fsPath, std::ios::binary);
    return ifs.good();
}

RID ResourceServer::AllocateRidLocked(ResourceKind kind) {
    return RID(nextIndex_++, generation_, kind);
}

std::string ResourceServer::MakeTypedCacheKey(const std::string& canonicalPath, ResourceKind kind) const {
    return std::to_string(static_cast<int>(kind)) + ":" + canonicalPath;
}

ResourceRef<Resource> ResourceServer::GetByRid(RID rid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = resourcesByRid_.find(rid);
    return (it != resourcesByRid_.end()) ? it->second : nullptr;
}

ResourceRef<Resource> ResourceServer::FindCached(const std::string& uriOrPath, ResourceKind kind) const {
    const ParsedVfsUri parsed = ParseUri(uriOrPath);
    const std::string typedKey = MakeTypedCacheKey(parsed.canonicalCacheKey, kind);
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = ridByTypedKey_.find(typedKey);
    if (it == ridByTypedKey_.end()) return nullptr;
    auto rit = resourcesByRid_.find(it->second);
    return (rit != resourcesByRid_.end()) ? rit->second : nullptr;
}

ResourceRef<RawBinaryResource> ResourceServer::LoadRawBinary(const std::string& uriOrPath) {
    const ParsedVfsUri parsed = ParseUri(uriOrPath);
    const std::string typedKey = MakeTypedCacheKey(parsed.canonicalCacheKey, ResourceKind::RawBinary);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = ridByTypedKey_.find(typedKey);
        if (it != ridByTypedKey_.end()) {
            auto rit = resourcesByRid_.find(it->second);
            if (rit != resourcesByRid_.end()) {
                cacheHits_++;
                return std::static_pointer_cast<RawBinaryResource>(rit->second);
            }
        }
        cacheMisses_++;
    }

    std::vector<uint8_t> bytes;
    if (!ReadRawBytes(uriOrPath, bytes)) {
        std::lock_guard<std::mutex> lock(mutex_);
        loadFailures_++;
        return nullptr;
    }

    auto res = std::make_shared<RawBinaryResource>();
    res->path_ = parsed.rawArchivePath;
    res->canonicalKey_ = parsed.canonicalCacheKey;
    res->state_ = ResourceLoadState::Loaded;
    res->bytes = std::move(bytes);

    std::lock_guard<std::mutex> lock(mutex_);
    auto existingIt = ridByTypedKey_.find(typedKey);
    if (existingIt != ridByTypedKey_.end()) {
        auto rit = resourcesByRid_.find(existingIt->second);
        if (rit != resourcesByRid_.end()) {
            return std::static_pointer_cast<RawBinaryResource>(rit->second);
        }
    }
    res->rid_ = AllocateRidLocked(ResourceKind::RawBinary);
    ridByTypedKey_[typedKey] = res->rid_;
    resourcesByRid_[res->rid_] = res;
    return res;
}

ResourceRef<SlkTableResource> ResourceServer::LoadSlkTable(const std::string& uriOrPath) {
    const ParsedVfsUri parsed = ParseUri(uriOrPath);
    const std::string typedKey = MakeTypedCacheKey(parsed.canonicalCacheKey, ResourceKind::SlkTable);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = ridByTypedKey_.find(typedKey);
        if (it != ridByTypedKey_.end()) {
            auto rit = resourcesByRid_.find(it->second);
            if (rit != resourcesByRid_.end()) {
                cacheHits_++;
                return std::static_pointer_cast<SlkTableResource>(rit->second);
            }
        }
        cacheMisses_++;
    }

    std::vector<uint8_t> bytes;
    if (!ReadRawBytes(uriOrPath, bytes) || bytes.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        loadFailures_++;
        return nullptr;
    }

    auto res = std::make_shared<SlkTableResource>();
    if (!res->table.Load(bytes.data(), bytes.size())) {
        std::lock_guard<std::mutex> lock(mutex_);
        loadFailures_++;
        return nullptr;
    }
    res->path_ = parsed.rawArchivePath;
    res->canonicalKey_ = parsed.canonicalCacheKey;
    res->byteSize = bytes.size();
    res->state_ = ResourceLoadState::Loaded;

    std::lock_guard<std::mutex> lock(mutex_);
    auto existingIt = ridByTypedKey_.find(typedKey);
    if (existingIt != ridByTypedKey_.end()) {
        auto rit = resourcesByRid_.find(existingIt->second);
        if (rit != resourcesByRid_.end()) {
            return std::static_pointer_cast<SlkTableResource>(rit->second);
        }
    }
    res->rid_ = AllocateRidLocked(ResourceKind::SlkTable);
    ridByTypedKey_[typedKey] = res->rid_;
    resourcesByRid_[res->rid_] = res;
    return res;
}

ResourceRef<TxtTableResource> ResourceServer::LoadTxtTable(const std::string& uriOrPath) {
    const ParsedVfsUri parsed = ParseUri(uriOrPath);
    const std::string typedKey = MakeTypedCacheKey(parsed.canonicalCacheKey, ResourceKind::TxtTable);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = ridByTypedKey_.find(typedKey);
        if (it != ridByTypedKey_.end()) {
            auto rit = resourcesByRid_.find(it->second);
            if (rit != resourcesByRid_.end()) {
                cacheHits_++;
                return std::static_pointer_cast<TxtTableResource>(rit->second);
            }
        }
        cacheMisses_++;
    }

    std::vector<uint8_t> bytes;
    if (!ReadRawBytes(uriOrPath, bytes) || bytes.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        loadFailures_++;
        return nullptr;
    }

    auto res = std::make_shared<TxtTableResource>();
    if (!res->table.Load(bytes.data(), bytes.size())) {
        std::lock_guard<std::mutex> lock(mutex_);
        loadFailures_++;
        return nullptr;
    }
    res->path_ = parsed.rawArchivePath;
    res->canonicalKey_ = parsed.canonicalCacheKey;
    res->byteSize = bytes.size();
    res->state_ = ResourceLoadState::Loaded;

    std::lock_guard<std::mutex> lock(mutex_);
    auto existingIt = ridByTypedKey_.find(typedKey);
    if (existingIt != ridByTypedKey_.end()) {
        auto rit = resourcesByRid_.find(existingIt->second);
        if (rit != resourcesByRid_.end()) {
            return std::static_pointer_cast<TxtTableResource>(rit->second);
        }
    }
    res->rid_ = AllocateRidLocked(ResourceKind::TxtTable);
    ridByTypedKey_[typedKey] = res->rid_;
    resourcesByRid_[res->rid_] = res;
    return res;
}

ResourceRef<BlpImageResource> ResourceServer::LoadBlpImage(const std::string& uriOrPath, bool decodeAllMips) {
    const ParsedVfsUri parsed = ParseUri(uriOrPath);
    const std::string typedKey = MakeTypedCacheKey(parsed.canonicalCacheKey, ResourceKind::BlpImage);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = ridByTypedKey_.find(typedKey);
        if (it != ridByTypedKey_.end()) {
            auto rit = resourcesByRid_.find(it->second);
            if (rit != resourcesByRid_.end()) {
                cacheHits_++;
                return std::static_pointer_cast<BlpImageResource>(rit->second);
            }
        }
        cacheMisses_++;
    }

    std::vector<uint8_t> bytes;
    if (!ReadRawBytes(uriOrPath, bytes) || bytes.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        loadFailures_++;
        return nullptr;
    }

    auto res = std::make_shared<BlpImageResource>();
    if (decodeAllMips) {
        if (!DecodeBLPLevels(bytes.data(), bytes.size(), res->mipChain) || res->mipChain.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            loadFailures_++;
            return nullptr;
        }
        res->baseImage = res->mipChain.front();
    } else {
        if (!DecodeBLP(bytes.data(), bytes.size(), res->baseImage, 0)) {
            std::lock_guard<std::mutex> lock(mutex_);
            loadFailures_++;
            return nullptr;
        }
    }

    res->path_ = parsed.rawArchivePath;
    res->canonicalKey_ = parsed.canonicalCacheKey;
    res->state_ = ResourceLoadState::Loaded;

    std::lock_guard<std::mutex> lock(mutex_);
    auto existingIt = ridByTypedKey_.find(typedKey);
    if (existingIt != ridByTypedKey_.end()) {
        auto rit = resourcesByRid_.find(existingIt->second);
        if (rit != resourcesByRid_.end()) {
            return std::static_pointer_cast<BlpImageResource>(rit->second);
        }
    }
    res->rid_ = AllocateRidLocked(ResourceKind::BlpImage);
    ridByTypedKey_[typedKey] = res->rid_;
    resourcesByRid_[res->rid_] = res;
    return res;
}

ResourceRef<MdxModelResource> ResourceServer::LoadMdxModel(const std::string& uriOrPath) {
    ParsedVfsUri parsed = ParseUri(uriOrPath);
    if (parsed.rawArchivePath.size() <= 4) {
        parsed = ParseUri(uriOrPath + ".mdx");
    } else {
        const std::string ext = ToLowerAscii(parsed.rawArchivePath.substr(parsed.rawArchivePath.size() - 4));
        if (ext != ".mdx" && ext != ".mdl") {
            parsed = ParseUri(uriOrPath + ".mdx");
        }
    }

    // Unify .mdx and .mdl under the `.mdx` canonical cache key so sibling fallbacks share the same slot
    std::string canonicalModelKey = parsed.canonicalCacheKey;
    if (canonicalModelKey.size() >= 4 &&
        canonicalModelKey.substr(canonicalModelKey.size() - 4) == ".mdl") {
        canonicalModelKey = canonicalModelKey.substr(0, canonicalModelKey.size() - 4) + ".mdx";
    }
    const std::string typedKey = MakeTypedCacheKey(canonicalModelKey, ResourceKind::MdxModel);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = ridByTypedKey_.find(typedKey);
        if (it != ridByTypedKey_.end()) {
            auto rit = resourcesByRid_.find(it->second);
            if (rit != resourcesByRid_.end()) {
                cacheHits_++;
                return std::static_pointer_cast<MdxModelResource>(rit->second);
            }
        }
        cacheMisses_++;
    }

    auto tryParseModelBytes = [](const std::string& filePath,
                                 const std::vector<uint8_t>& bytes,
                                 MdxModel& outModel,
                                 bool& outIsMdl) -> bool {
        if (bytes.empty()) return false;
        std::string ext;
        if (filePath.size() >= 4) {
            ext = ToLowerAscii(filePath.substr(filePath.size() - 4));
        }
        if (ext == ".mdl") {
            outIsMdl = true;
            std::string text(bytes.begin(), bytes.end());
            return MdlLoad(text.c_str(), text.size(), outModel);
        }
        outIsMdl = false;
        if (MdxLoad(bytes.data(), bytes.size(), outModel)) {
            return true;
        }
        // Fallback: if a file named .mdx actually contains MDL text
        std::string text(bytes.begin(), bytes.end());
        if (MdlLoad(text.c_str(), text.size(), outModel)) {
            outIsMdl = true;
            return true;
        }
        return false;
    };

    std::vector<uint8_t> bytes;
    MdxModel model;
    bool isMdl = false;
    bool loaded = false;

    if (ReadRawBytes(parsed.rawArchivePath, bytes)) {
        loaded = tryParseModelBytes(parsed.rawArchivePath, bytes, model, isMdl);
    }

    if (!loaded) {
        const std::string sibling = SiblingModelExtensionPath(parsed.rawArchivePath);
        if (!sibling.empty() && ReadRawBytes(sibling, bytes)) {
            loaded = tryParseModelBytes(sibling, bytes, model, isMdl);
        }
    }

    if (!loaded) {
        std::lock_guard<std::mutex> lock(mutex_);
        loadFailures_++;
        return nullptr;
    }

    auto res = std::make_shared<MdxModelResource>();
    res->path_ = parsed.rawArchivePath;
    res->canonicalKey_ = canonicalModelKey;
    res->sourceByteSize = bytes.size();
    res->loadedFromMdlText = isMdl;
    res->model = std::move(model);
    res->state_ = ResourceLoadState::Loaded;

    std::lock_guard<std::mutex> lock(mutex_);
    auto existingIt = ridByTypedKey_.find(typedKey);
    if (existingIt != ridByTypedKey_.end()) {
        auto rit = resourcesByRid_.find(existingIt->second);
        if (rit != resourcesByRid_.end()) {
            return std::static_pointer_cast<MdxModelResource>(rit->second);
        }
    }
    res->rid_ = AllocateRidLocked(ResourceKind::MdxModel);
    ridByTypedKey_[typedKey] = res->rid_;
    resourcesByRid_[res->rid_] = res;
    return res;
}

ResourceRef<AudioClipResource> ResourceServer::LoadAudioClip(const std::string& uriOrPath) {
    const ParsedVfsUri parsed = ParseUri(uriOrPath);
    const std::string typedKey = MakeTypedCacheKey(parsed.canonicalCacheKey, ResourceKind::AudioClip);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = ridByTypedKey_.find(typedKey);
        if (it != ridByTypedKey_.end()) {
            auto rit = resourcesByRid_.find(it->second);
            if (rit != resourcesByRid_.end()) {
                cacheHits_++;
                return std::static_pointer_cast<AudioClipResource>(rit->second);
            }
        }
        cacheMisses_++;
    }

    std::vector<uint8_t> bytes;
    if (!ReadRawBytes(uriOrPath, bytes)) {
        std::lock_guard<std::mutex> lock(mutex_);
        loadFailures_++;
        return nullptr;
    }

    auto res = std::make_shared<AudioClipResource>();
    if (!AudioClipResource::ParseAudioContainer(parsed.rawArchivePath, bytes, *res)) {
        std::lock_guard<std::mutex> lock(mutex_);
        loadFailures_++;
        return nullptr;
    }
    res->path_ = parsed.rawArchivePath;
    res->canonicalKey_ = parsed.canonicalCacheKey;
    res->bytes = std::move(bytes);
    res->state_ = ResourceLoadState::Loaded;

    std::lock_guard<std::mutex> lock(mutex_);
    auto existingIt = ridByTypedKey_.find(typedKey);
    if (existingIt != ridByTypedKey_.end()) {
        auto rit = resourcesByRid_.find(existingIt->second);
        if (rit != resourcesByRid_.end()) {
            return std::static_pointer_cast<AudioClipResource>(rit->second);
        }
    }
    res->rid_ = AllocateRidLocked(ResourceKind::AudioClip);
    ridByTypedKey_[typedKey] = res->rid_;
    resourcesByRid_[res->rid_] = res;
    return res;
}

ResourceRef<Resource> ResourceServer::LoadByKindInternal(const std::string& uriOrPath, ResourceKind kind) {
    switch (kind) {
        case ResourceKind::RawBinary: return LoadRawBinary(uriOrPath);
        case ResourceKind::SlkTable:  return LoadSlkTable(uriOrPath);
        case ResourceKind::TxtTable:  return LoadTxtTable(uriOrPath);
        case ResourceKind::BlpImage:  return LoadBlpImage(uriOrPath, false);
        case ResourceKind::MdxModel:  return LoadMdxModel(uriOrPath);
        case ResourceKind::AudioClip: return LoadAudioClip(uriOrPath);
        default: return nullptr;
    }
}

void ResourceServer::StartAsyncWorker() {
    std::lock_guard<std::mutex> lock(asyncMutex_);
    if (workerRunning_) return;
    workerRunning_ = true;
    workerThread_ = std::thread(&ResourceServer::WorkerLoop, this);
}

void ResourceServer::StopAsyncWorker() {
    {
        std::lock_guard<std::mutex> lock(asyncMutex_);
        if (!workerRunning_) return;
        workerRunning_ = false;
    }
    asyncCv_.notify_all();
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

void ResourceServer::WorkerLoop() {
    while (true) {
        AsyncJob job;
        {
            std::unique_lock<std::mutex> lock(asyncMutex_);
            asyncCv_.wait(lock, [this]() {
                return !workerRunning_ || !asyncPending_.empty();
            });
            if (!workerRunning_ && asyncPending_.empty()) {
                return;
            }
            job = std::move(asyncPending_.front());
            asyncPending_.pop_front();
            workerBusy_ = true;
        }

        ResourceRef<Resource> loaded = LoadByKindInternal(job.uri, job.kind);
        const RID rid = loaded ? loaded->GetRid() : RID{};
        const bool ok = (loaded != nullptr);

        {
            std::lock_guard<std::mutex> lock(asyncMutex_);
            asyncCompleted_.push_back(AsyncCompletedJob{
                rid, std::move(loaded), ok, std::move(job.callback)
            });
            workerBusy_ = false;
        }
        asyncDoneCv_.notify_all();
    }
}

void ResourceServer::RequestAsyncPreload(const std::string& uriOrPath,
                                         ResourceKind kind,
                                         AsyncCompletionCallback onComplete) {
    StartAsyncWorker();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        asyncJobsSubmitted_++;
    }
    {
        std::lock_guard<std::mutex> lock(asyncMutex_);
        asyncPending_.push_back(AsyncJob{uriOrPath, kind, std::move(onComplete)});
    }
    asyncCv_.notify_one();
}

size_t ResourceServer::PollAsyncCompletions() {
    std::deque<AsyncCompletedJob> finished;
    {
        std::lock_guard<std::mutex> lock(asyncMutex_);
        finished.swap(asyncCompleted_);
    }

    size_t count = 0;
    for (auto& item : finished) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            asyncJobsCompleted_++;
        }
        if (item.callback) {
            item.callback(item.rid, item.resource, item.success);
        }
        count++;
    }
    return count;
}

void ResourceServer::FlushAsyncPreloads() {
    {
        std::unique_lock<std::mutex> lock(asyncMutex_);
        if (!workerRunning_) return;
        asyncDoneCv_.wait(lock, [this]() {
            return asyncPending_.empty() && !workerBusy_;
        });
    }
    PollAsyncCompletions();
}

size_t ResourceServer::PruneUnusedResources() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t pruned = 0;
    for (auto it = ridByTypedKey_.begin(); it != ridByTypedKey_.end();) {
        auto rit = resourcesByRid_.find(it->second);
        if (rit == resourcesByRid_.end() || rit->second.use_count() <= 1) {
            if (rit != resourcesByRid_.end()) {
                resourcesByRid_.erase(rit);
            }
            it = ridByTypedKey_.erase(it);
            pruned++;
        } else {
            ++it;
        }
    }
    generation_++;
    return pruned;
}

void ResourceServer::ClearAll() {
    FlushAsyncPreloads();
    std::lock_guard<std::mutex> lock(mutex_);
    resourcesByRid_.clear();
    ridByTypedKey_.clear();
    cacheHits_ = 0;
    cacheMisses_ = 0;
    loadFailures_ = 0;
    asyncJobsSubmitted_ = 0;
    asyncJobsCompleted_ = 0;
    generation_++;
}

ResourceServerStats ResourceServer::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    ResourceServerStats s;
    s.cacheHits = cacheHits_;
    s.cacheMisses = cacheMisses_;
    s.loadFailures = loadFailures_;
    s.asyncJobsSubmitted = asyncJobsSubmitted_;
    s.asyncJobsCompleted = asyncJobsCompleted_;
    s.activeResourceCount = resourcesByRid_.size();
    for (const auto& [rid, res] : resourcesByRid_) {
        (void)rid;
        if (res) s.totalEstimatedBytes += res->EstimateMemoryBytes();
    }
    return s;
}

} // namespace resource
