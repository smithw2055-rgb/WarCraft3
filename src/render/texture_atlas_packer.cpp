#include "render/texture_atlas_packer.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace render {

namespace {

std::string SiblingTextureExtensionPath(const std::string& path) {
    if (path.size() < 4) return {};
    const size_t dot = path.rfind('.');
    if (dot == std::string::npos) return {};
    std::string ext = path.substr(dot);
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (ext == ".blp") return path.substr(0, dot) + ".dds";
    if (ext == ".dds") return path.substr(0, dot) + ".blp";
    return {};
}

} // namespace

std::shared_ptr<BlpMipTextureResource> BlpMipTextureResource::LoadFromVfs(
    resource::ResourceServer& server,
    const std::string& uriOrPath,
    bool buildFullMips) {
    resource::ParsedVfsUri parsed = resource::ResourceServer::ParseUri(uriOrPath);
    if (parsed.rawArchivePath.empty()) return nullptr;

    std::vector<uint8_t> bytes;
    std::string resolvedPath = parsed.rawArchivePath;
    if (!server.ReadRawBytes(resolvedPath, bytes) || bytes.empty()) {
        const std::string sibling = SiblingTextureExtensionPath(resolvedPath);
        if (sibling.empty() || !server.ReadRawBytes(sibling, bytes) || bytes.empty()) {
            return nullptr;
        }
        resolvedPath = sibling;
    }

    auto tex = std::make_shared<BlpMipTextureResource>();
    tex->path_ = resolvedPath;
    tex->canonicalKey_ = parsed.canonicalCacheKey;

    std::vector<BLPImage> blpLevels;
    if (DecodeBLPLevels(bytes.data(), bytes.size(), blpLevels) && !blpLevels.empty()) {
        tex->width = blpLevels[0].width;
        tex->height = blpLevels[0].height;
        tex->loadedFromDds = false;

        if (buildFullMips) {
            std::vector<uint8_t> basePixels = std::move(blpLevels[0].pixels);
            std::vector<MipLevel> embedded;
            for (size_t i = 1; i < blpLevels.size(); ++i) {
                MipLevel lvl;
                lvl.width = blpLevels[i].width;
                lvl.height = blpLevels[i].height;
                lvl.pixels = std::move(blpLevels[i].pixels);
                embedded.push_back(std::move(lvl));
            }
            tex->mipChain = BuildMipChain(tex->width, tex->height,
                                          basePixels.data(),
                                          std::move(embedded));
        } else {
            MipLevel base;
            base.width = blpLevels[0].width;
            base.height = blpLevels[0].height;
            base.pixels = std::move(blpLevels[0].pixels);
            tex->mipChain.levels.push_back(std::move(base));
        }
        tex->state_ = resource::ResourceLoadState::Loaded;
        return tex;
    }

    std::vector<DdsImage> ddsLevels;
    if (DecodeDDSLevels(bytes.data(), bytes.size(), ddsLevels) && !ddsLevels.empty()) {
        tex->width = ddsLevels[0].width;
        tex->height = ddsLevels[0].height;
        tex->loadedFromDds = true;

        if (buildFullMips) {
            std::vector<uint8_t> basePixels = std::move(ddsLevels[0].pixels);
            std::vector<MipLevel> embedded;
            for (size_t i = 1; i < ddsLevels.size(); ++i) {
                MipLevel lvl;
                lvl.width = ddsLevels[i].width;
                lvl.height = ddsLevels[i].height;
                lvl.pixels = std::move(ddsLevels[i].pixels);
                embedded.push_back(std::move(lvl));
            }
            tex->mipChain = BuildMipChain(tex->width, tex->height,
                                          basePixels.data(),
                                          std::move(embedded));
        } else {
            MipLevel base;
            base.width = ddsLevels[0].width;
            base.height = ddsLevels[0].height;
            base.pixels = std::move(ddsLevels[0].pixels);
            tex->mipChain.levels.push_back(std::move(base));
        }
        tex->state_ = resource::ResourceLoadState::Loaded;
        return tex;
    }

    return nullptr;
}

TextureAtlasPacker::TextureAtlasPacker(int atlasWidth, int atlasHeight, int borderPadding)
    : width_(std::max(16, atlasWidth)),
      height_(std::max(16, atlasHeight)),
      padding_(std::max(0, borderPadding)) {
    Clear();
}

void TextureAtlasPacker::Clear() {
    cursorX_ = 0;
    cursorY_ = 0;
    currentShelfHeight_ = 0;
    pixels_.assign(static_cast<size_t>(width_) * static_cast<size_t>(height_) * 4u, 0u);
    regions_.clear();
    stats_ = TextureAtlasStats{};
    stats_.atlasWidth = width_;
    stats_.atlasHeight = height_;
    stats_.borderPadding = padding_;
}

bool TextureAtlasPacker::PackSprite(const std::string& name,
                                    int width,
                                    int height,
                                    const uint8_t* rgbaPixels,
                                    AtlasRegionUV* outRegion) {
    if (name.empty() || width <= 0 || height <= 0 || !rgbaPixels) {
        stats_.failedPackCount++;
        return false;
    }

    // If already packed, return existing region
    auto existing = regions_.find(name);
    if (existing != regions_.end()) {
        if (outRegion) *outRegion = existing->second;
        return true;
    }

    const int paddedW = width + padding_ * 2;
    const int paddedH = height + padding_ * 2;
    if (paddedW > width_ || paddedH > height_) {
        stats_.failedPackCount++;
        return false;
    }

    // Advance to next shelf row if current row lacks horizontal space
    if (cursorX_ + paddedW > width_) {
        cursorX_ = 0;
        cursorY_ += currentShelfHeight_;
        currentShelfHeight_ = 0;
    }

    if (cursorY_ + paddedH > height_) {
        stats_.failedPackCount++;
        return false;
    }

    const int innerX = cursorX_ + padding_;
    const int innerY = cursorY_ + padding_;
    BlitWithExtrudedBorder(innerX, innerY, width, height, rgbaPixels);

    cursorX_ += paddedW;
    currentShelfHeight_ = std::max(currentShelfHeight_, paddedH);

    AtlasRegionUV reg;
    reg.name = name;
    reg.pageX = innerX;
    reg.pageY = innerY;
    reg.width = width;
    reg.height = height;
    reg.u0 = static_cast<float>(innerX) / static_cast<float>(width_);
    reg.v0 = static_cast<float>(innerY) / static_cast<float>(height_);
    reg.u1 = static_cast<float>(innerX + width) / static_cast<float>(width_);
    reg.v1 = static_cast<float>(innerY + height) / static_cast<float>(height_);

    regions_[name] = reg;
    stats_.packedSpriteCount++;
    stats_.usedPixelArea += static_cast<size_t>(paddedW) * static_cast<size_t>(paddedH);
    const float totalArea = static_cast<float>(width_) * static_cast<float>(height_);
    stats_.occupancyRatio = (totalArea > 0.0f)
        ? (static_cast<float>(stats_.usedPixelArea) / totalArea)
        : 0.0f;

    if (outRegion) *outRegion = reg;
    return true;
}

void TextureAtlasPacker::BlitWithExtrudedBorder(int dstX, int dstY, int w, int h, const uint8_t* srcRgba) {
    for (int ry = -padding_; ry < h + padding_; ++ry) {
        const int sy = std::clamp(ry, 0, h - 1);
        const int py = dstY + ry;
        if (py < 0 || py >= height_) continue;

        for (int rx = -padding_; rx < w + padding_; ++rx) {
            const int sx = std::clamp(rx, 0, w - 1);
            const int px = dstX + rx;
            if (px < 0 || px >= width_) continue;

            const size_t srcOff = (static_cast<size_t>(sy) * static_cast<size_t>(w) + static_cast<size_t>(sx)) * 4u;
            const size_t dstOff = (static_cast<size_t>(py) * static_cast<size_t>(width_) + static_cast<size_t>(px)) * 4u;
            std::memcpy(&pixels_[dstOff], &srcRgba[srcOff], 4u);
        }
    }
}

const AtlasRegionUV* TextureAtlasPacker::FindRegion(const std::string& name) const {
    auto it = regions_.find(name);
    return (it != regions_.end()) ? &it->second : nullptr;
}

bool TextureAtlasPacker::SamplePixel(int x, int y, uint8_t outRgba[4]) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return false;
    const size_t off = (static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)) * 4u;
    std::memcpy(outRgba, &pixels_[off], 4u);
    return true;
}

MipChain TextureAtlasPacker::BuildAtlasMipChain() const {
    return BuildMipChain(width_, height_, pixels_.data());
}

} // namespace render
