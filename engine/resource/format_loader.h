#pragma once
// ============================================================================
// Engine Resource: IFormatLoader & FormatLoaderRegistry
//
// Pluggable asset decoder pipeline (Godot ResourceFormatLoader equivalent).
// Dispatches decoders by magic bytes first, then by lowercase file extension.
// ============================================================================
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "engine/core/tag_id.h"

namespace engine::resource {

struct LoadedAssetPayload {
    TagId formatTag;
    std::string sourcePath;
    std::vector<uint8_t> decodedBytes;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    uint32_t sampleRate = 0;
    uint32_t subAssetCount = 0;
};

class IFormatLoader {
public:
    virtual ~IFormatLoader() = default;
    virtual TagId GetFormatTag() const = 0;
    virtual bool CanRecognize(std::string_view extensionLower,
                              const uint8_t* headerBytes,
                              size_t headerLen) const = 0;
    virtual bool Decode(std::string_view virtualPath,
                        const uint8_t* rawBytes,
                        size_t rawLen,
                        LoadedAssetPayload& outPayload) const = 0;
};

class RawBytesFormatLoader final : public IFormatLoader {
public:
    TagId GetFormatTag() const override { return TagId("RAWB"); }

    bool CanRecognize(std::string_view /*extensionLower*/,
                      const uint8_t* /*headerBytes*/,
                      size_t /*headerLen*/) const override {
        return true;
    }

    bool Decode(std::string_view virtualPath,
                const uint8_t* rawBytes,
                size_t rawLen,
                LoadedAssetPayload& outPayload) const override {
        outPayload = LoadedAssetPayload{};
        outPayload.formatTag = GetFormatTag();
        outPayload.sourcePath = std::string(virtualPath);
        if (rawBytes && rawLen > 0) {
            outPayload.decodedBytes.assign(rawBytes, rawBytes + rawLen);
        }
        return true;
    }
};

class FormatLoaderRegistry {
public:
    static std::string ExtractLowerExtension(std::string_view path) {
        const size_t slash = path.find_last_of("/\\");
        const size_t dot = path.find_last_of('.');
        if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) {
            return "";
        }
        std::string ext(path.substr(dot));
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return ext;
    }

    void RegisterLoader(std::shared_ptr<IFormatLoader> loader, int priority = 0) {
        if (!loader) return;
        entries_.push_back({priority, std::move(loader)});
        std::stable_sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
            return a.priority > b.priority;
        });
    }

    const IFormatLoader* FindLoader(std::string_view virtualPath,
                                    const uint8_t* bytes,
                                    size_t len) const {
        const std::string ext = ExtractLowerExtension(virtualPath);
        const size_t headerLen = std::min<size_t>(len, 16u);
        for (const auto& e : entries_) {
            if (e.loader->CanRecognize(ext, bytes, headerLen)) {
                return e.loader.get();
            }
        }
        return nullptr;
    }

    bool DecodeBuffer(std::string_view virtualPath,
                      const uint8_t* bytes,
                      size_t len,
                      LoadedAssetPayload& outPayload) const {
        const IFormatLoader* loader = FindLoader(virtualPath, bytes, len);
        if (!loader) return false;
        ++totalDecoded_;
        return loader->Decode(virtualPath, bytes, len, outPayload);
    }

    size_t LoaderCount() const { return entries_.size(); }
    uint64_t TotalDecodedCount() const { return totalDecoded_; }

private:
    struct Entry {
        int priority = 0;
        std::shared_ptr<IFormatLoader> loader;
    };
    std::vector<Entry> entries_;
    mutable uint64_t totalDecoded_ = 0;
};

} // namespace engine::resource
