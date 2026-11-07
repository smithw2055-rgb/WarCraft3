#pragma once
// ============================================================================
// Stage T: BlpMipTextureResource Pipeline & 2D TextureAtlasPacker
//          (Inspired by Godot 4 AtlasTexture & Unity SpriteAtlas)
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "asset/BLP.h"
#include "asset/DDS.h"
#include "asset/MipChain.h"
#include "resource/resource_server.h"

namespace render {

class BlpMipTextureResource final : public resource::Resource {
public:
    BlpMipTextureResource() { kind_ = resource::ResourceKind::BlpImage; }

    int width = 0;
    int height = 0;
    bool loadedFromDds = false;
    MipChain mipChain;

    const std::vector<uint8_t>& GetBasePixels() const {
        static const std::vector<uint8_t> kEmpty;
        return mipChain.levels.empty() ? kEmpty : mipChain.levels[0].pixels;
    }

    size_t GetMipLevelCount() const { return mipChain.levels.size(); }

    size_t EstimateMemoryBytes() const override {
        size_t total = sizeof(*this);
        for (const MipLevel& lvl : mipChain.levels) {
            total += sizeof(MipLevel) + lvl.pixels.capacity();
        }
        return total;
    }

    // Load and decode a .blp or .dds texture through ResourceServer VFS
    // (with automatic .blp <-> .dds sibling fallback) and build full MipChain.
    static std::shared_ptr<BlpMipTextureResource> LoadFromVfs(
        resource::ResourceServer& server,
        const std::string& uriOrPath,
        bool buildFullMips = true);
};

struct AtlasRegionUV {
    std::string name;
    int pageX = 0;
    int pageY = 0;
    int width = 0;
    int height = 0;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
};

struct TextureAtlasStats {
    int atlasWidth = 0;
    int atlasHeight = 0;
    int borderPadding = 1;
    int packedSpriteCount = 0;
    int failedPackCount = 0;
    size_t usedPixelArea = 0;
    float occupancyRatio = 0.0f;
};

class TextureAtlasPacker {
public:
    explicit TextureAtlasPacker(int atlasWidth = 512,
                                int atlasHeight = 512,
                                int borderPadding = 1);

    void Clear();

    // Pack an RGBA8 sprite (`pixels.size() == width * height * 4`) into the atlas
    // with automatic 1px edge-extrusion border padding to prevent bilinear bleeding.
    bool PackSprite(const std::string& name,
                    int width,
                    int height,
                    const uint8_t* rgbaPixels,
                    AtlasRegionUV* outRegion = nullptr);

    const AtlasRegionUV* FindRegion(const std::string& name) const;

    // Sample an RGBA pixel at (x, y) on the base atlas page
    bool SamplePixel(int x, int y, uint8_t outRgba[4]) const;

    // Build a full MipChain down to 1x1 for the composited atlas page
    MipChain BuildAtlasMipChain() const;

    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    const std::vector<uint8_t>& GetPixels() const { return pixels_; }
    const TextureAtlasStats& GetStats() const { return stats_; }

private:
    void BlitWithExtrudedBorder(int dstX, int dstY, int w, int h, const uint8_t* srcRgba);

    int width_ = 512;
    int height_ = 512;
    int padding_ = 1;
    int cursorX_ = 0;
    int cursorY_ = 0;
    int currentShelfHeight_ = 0;
    std::vector<uint8_t> pixels_;
    std::unordered_map<std::string, AtlasRegionUV> regions_;
    TextureAtlasStats stats_{};
};

} // namespace render
