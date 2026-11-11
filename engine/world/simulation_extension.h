#pragma once
// ============================================================================
// Engine World: ISimulationExtension & Deterministic Save/Load Archive ("ESAV")
//
// Provides:
// 1. Overflow-safe, Little-Endian canonical `BinaryWriter` & `BinaryReader`
//    (`len > size_ - cursor_` prevents unsigned wrap-around on malformed streams).
// 2. `ISaveChunkContributor` & `DeterministicSaveArchive` (`"ESAV"` chunked
//    container with FNV-1a integrity verification).
// 3. `ISimulationExtension` & `ProjectExtensionRegistry` for zero-coupling
//    project simulation boot in `engine_player` and tests.
// ============================================================================
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "engine/core/tag_id.h"

namespace engine::world {

namespace detail {

inline bool IsHostBigEndian() noexcept {
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return true;
#else
    const uint16_t probe = 0x0102u;
    uint8_t firstByte = 0;
    std::memcpy(&firstByte, &probe, 1);
    return firstByte == 0x01u;
#endif
}

template <typename T>
inline void SwapByteOrderInPlace(uint8_t* bytes) noexcept {
    if constexpr (sizeof(T) > 1) {
        for (size_t i = 0; i < sizeof(T) / 2; ++i) {
            std::swap(bytes[i], bytes[sizeof(T) - 1 - i]);
        }
    }
}

} // namespace detail

class BinaryWriter {
public:
    void Reserve(size_t cap) { buffer_.reserve(cap); }

    // Canonical Little-Endian arithmetic/enum serialization for cross-architecture determinism
    template <typename T>
    void WritePod(const T& val) {
        static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>,
                      "WritePod only accepts arithmetic scalars or enums for deterministic LE layout");
        uint8_t raw[sizeof(T)];
        std::memcpy(raw, &val, sizeof(T));
        if (detail::IsHostBigEndian()) {
            detail::SwapByteOrderInPlace<T>(raw);
        }
        WriteBytes(raw, sizeof(T));
    }

    void WriteU8(uint8_t v)   { buffer_.push_back(v); }
    void WriteU16(uint16_t v) { WritePod(v); }
    void WriteU32(uint32_t v) { WritePod(v); }
    void WriteI32(int32_t v)  { WritePod(v); }
    void WriteU64(uint64_t v) { WritePod(v); }
    void WriteF32(float v)    { WritePod(v); }

    void WriteBytes(const void* src, size_t len) {
        if (!src || len == 0) return;
        const size_t oldSize = buffer_.size();
        buffer_.resize(oldSize + len);
        std::memcpy(buffer_.data() + oldSize, src, len);
    }

    void WriteString(std::string_view sv) {
        const uint32_t len = static_cast<uint32_t>(sv.size());
        WriteU32(len);
        if (len > 0) {
            WriteBytes(sv.data(), len);
        }
    }

    const std::vector<uint8_t>& Data() const { return buffer_; }
    std::vector<uint8_t> TakeData() { return std::move(buffer_); }
    size_t Size() const { return buffer_.size(); }

private:
    std::vector<uint8_t> buffer_;
};

class BinaryReader {
public:
    BinaryReader(const uint8_t* data, size_t size)
        : data_(data), size_(data ? size : 0) {}

    explicit BinaryReader(const std::vector<uint8_t>& bytes)
        : data_(bytes.data()), size_(bytes.size()) {}

    template <typename T>
    bool ReadPod(T& out) {
        static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>,
                      "ReadPod only accepts arithmetic scalars or enums for deterministic LE layout");
        uint8_t raw[sizeof(T)];
        if (!ReadBytes(raw, sizeof(T))) return false;
        if (detail::IsHostBigEndian()) {
            detail::SwapByteOrderInPlace<T>(raw);
        }
        std::memcpy(&out, raw, sizeof(T));
        return true;
    }

    bool ReadU8(uint8_t& v)   { return ReadPod(v); }
    bool ReadU16(uint16_t& v) { return ReadPod(v); }
    bool ReadU32(uint32_t& v) { return ReadPod(v); }
    bool ReadI32(int32_t& v)  { return ReadPod(v); }
    bool ReadU64(uint64_t& v) { return ReadPod(v); }
    bool ReadF32(float& v)    { return ReadPod(v); }

    // Overflow-safe subtraction check (`len > size_ - cursor_`) prevents size_t wrap-around
    bool ReadBytes(void* dst, size_t len) {
        if (len == 0) return true;
        if (!data_ || !dst || cursor_ > size_ || len > size_ - cursor_) {
            return false;
        }
        std::memcpy(dst, data_ + cursor_, len);
        cursor_ += len;
        return true;
    }

    bool ReadString(std::string& out) {
        uint32_t len = 0;
        if (!ReadU32(len)) return false;
        if (cursor_ > size_ || static_cast<size_t>(len) > size_ - cursor_) {
            return false;
        }
        out.assign(reinterpret_cast<const char*>(data_ + cursor_), static_cast<size_t>(len));
        cursor_ += static_cast<size_t>(len);
        return true;
    }

    size_t Remaining() const { return (cursor_ <= size_) ? (size_ - cursor_) : 0u; }
    size_t Cursor() const { return cursor_; }
    bool AtEnd() const { return cursor_ == size_; }

private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t cursor_ = 0;
};

class ISaveChunkContributor {
public:
    virtual ~ISaveChunkContributor() = default;
    virtual TagId GetChunkTag() const = 0;
    virtual uint32_t GetChunkVersion() const { return 1u; }
    virtual void WriteChunk(BinaryWriter& writer) const = 0;
    virtual bool ReadChunk(BinaryReader& reader, uint32_t version) = 0;
};

class DeterministicSaveArchive {
public:
    static constexpr uint32_t kMagicEsav = 0x56415345u; // "ESAV" in Little-Endian wire order ('E','S','A','V')
    static constexpr uint32_t kArchiveFormatVersion = 1u;

    void RegisterContributor(ISaveChunkContributor* contributor) {
        if (!contributor) return;
        for (auto* existing : contributors_) {
            if (existing->GetChunkTag() == contributor->GetChunkTag()) {
                return;
            }
        }
        contributors_.push_back(contributor);
    }

    void ClearContributors() { contributors_.clear(); }
    size_t ContributorCount() const { return contributors_.size(); }

    static uint64_t ComputeFnv1a64(const uint8_t* data, size_t len) {
        uint64_t h = 14695981039346656037ULL;
        for (size_t i = 0; i < len; ++i) {
            h ^= static_cast<uint64_t>(data[i]);
            h *= 1099511628211ULL;
        }
        return h;
    }

    std::vector<uint8_t> SaveAll(uint64_t tickNumber = 0) const {
        BinaryWriter body;
        body.WriteU64(tickNumber);
        body.WriteU32(static_cast<uint32_t>(contributors_.size()));

        for (const auto* c : contributors_) {
            BinaryWriter chunkPayload;
            c->WriteChunk(chunkPayload);
            body.WriteU32(c->GetChunkTag().Value());
            body.WriteU32(c->GetChunkVersion());
            body.WriteU32(static_cast<uint32_t>(chunkPayload.Size()));
            if (chunkPayload.Size() > 0) {
                body.WriteBytes(chunkPayload.Data().data(), chunkPayload.Size());
            }
        }

        const uint64_t checksum = ComputeFnv1a64(body.Data().data(), body.Size());

        BinaryWriter out;
        out.Reserve(20 + body.Size());
        out.WriteU32(kMagicEsav);
        out.WriteU32(kArchiveFormatVersion);
        out.WriteU64(checksum);
        out.WriteU32(static_cast<uint32_t>(body.Size()));
        out.WriteBytes(body.Data().data(), body.Size());
        return out.TakeData();
    }

    bool LoadAll(const uint8_t* bytes, size_t size, uint64_t* outTickNumber = nullptr) {
        BinaryReader outer(bytes, size);
        uint32_t magic = 0;
        uint32_t archiveVer = 0;
        uint64_t expectedChecksum = 0;
        uint32_t bodySize = 0;

        if (!outer.ReadU32(magic) || magic != kMagicEsav) return false;
        if (!outer.ReadU32(archiveVer) || archiveVer != kArchiveFormatVersion) return false;
        if (!outer.ReadU64(expectedChecksum)) return false;
        if (!outer.ReadU32(bodySize)) return false;
        if (outer.Remaining() != bodySize) return false;

        const uint8_t* bodyPtr = bytes + outer.Cursor();
        if (ComputeFnv1a64(bodyPtr, bodySize) != expectedChecksum) {
            return false;
        }

        BinaryReader body(bodyPtr, bodySize);
        uint64_t tickNumber = 0;
        uint32_t chunkCount = 0;
        if (!body.ReadU64(tickNumber) || !body.ReadU32(chunkCount)) {
            return false;
        }
        if (outTickNumber) *outTickNumber = tickNumber;

        for (uint32_t i = 0; i < chunkCount; ++i) {
            uint32_t tagRaw = 0;
            uint32_t chunkVer = 0;
            uint32_t payloadLen = 0;
            if (!body.ReadU32(tagRaw) || !body.ReadU32(chunkVer) || !body.ReadU32(payloadLen)) {
                return false;
            }
            if (payloadLen > body.Remaining()) {
                return false;
            }
            std::vector<uint8_t> chunkBuf(payloadLen);
            if (payloadLen > 0 && !body.ReadBytes(chunkBuf.data(), payloadLen)) {
                return false;
            }
            const TagId tag(tagRaw);
            for (auto* c : contributors_) {
                if (c->GetChunkTag() == tag) {
                    BinaryReader chunkReader(chunkBuf);
                    if (!c->ReadChunk(chunkReader, chunkVer)) {
                        return false;
                    }
                    break;
                }
            }
        }
        return body.AtEnd();
    }

    bool LoadAll(const std::vector<uint8_t>& bytes, uint64_t* outTickNumber = nullptr) {
        return LoadAll(bytes.data(), bytes.size(), outTickNumber);
    }

private:
    std::vector<ISaveChunkContributor*> contributors_;
};

// ============================================================================
// Generic 5-Phase Deterministic Simulation Extension Contract
// ============================================================================
enum class TickPhase : uint8_t {
    CommandIngest = 0,
    OrderBehavior = 1,
    MovementSteering = 2,
    CombatProjectiles = 3,
    PostTickCleanup = 4
};

class ISimulationExtension : public ISaveChunkContributor {
public:
    ~ISimulationExtension() override = default;

    virtual const char* GetExtensionId() const = 0;
    virtual bool ConfigureFromProjectData(std::string_view /*sceneOrDataJson*/) { return true; }
    virtual void OnInitializeWorld() = 0;
    virtual void OnTickPhase(TickPhase phase, float fixedDt) = 0;
    virtual uint64_t ComputeDeterministicHash() const = 0;
    virtual void OnShutdownWorld() {}
};

// Generic factory registry so `engine_player` never hardcodes concrete game headers
class ProjectExtensionRegistry {
public:
    using ExtensionFactoryFn = std::function<std::unique_ptr<ISimulationExtension>()>;

    static ProjectExtensionRegistry& Instance() {
        static ProjectExtensionRegistry inst;
        return inst;
    }

    void RegisterFactory(const std::string& projectOrModuleId, ExtensionFactoryFn fn) {
        if (!projectOrModuleId.empty() && fn) {
            factories_[projectOrModuleId] = std::move(fn);
        }
    }

    std::unique_ptr<ISimulationExtension> Create(const std::string& projectOrModuleId) const {
        auto it = factories_.find(projectOrModuleId);
        return (it != factories_.end() && it->second) ? it->second() : nullptr;
    }

    bool HasFactory(const std::string& projectOrModuleId) const {
        return factories_.find(projectOrModuleId) != factories_.end();
    }

private:
    std::unordered_map<std::string, ExtensionFactoryFn> factories_;
};

} // namespace engine::world
