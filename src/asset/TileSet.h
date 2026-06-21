#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "map/w3e.h"
#include "resource/mpq_storm.h"
#include "third_party/sokol/sokol_gfx.h"

struct GroundTextureTile {
    sg_image glTexture = {0};        // sg_image (2D array texture)
    int tileSize = 0;
    bool extended = false;
    int tileCount = 16;
    int id = 0;
    std::string sourceFormat = "unknown";
    bool needsSRGBFix = false;
    bool srgbUpload = false;
    int sourceWidth = 0;
    int sourceHeight = 0;
    // First tile's RGBA8 pixel data (64x64) for combined texture creation
    std::vector<uint8_t> firstLayerData;
};

struct CliffTextureInfo {
    sg_image glTexture = {0};        // sg_image (2D array texture)
    std::string texDir;
    std::string texFile;
    std::string cliffModelDir;
    std::string rampModelDir;
    std::string sourceFormat = "unknown";
    bool needsSRGBFix = false;
    bool srgbUpload = false;
    int sourceWidth = 0;
    int sourceHeight = 0;
};

struct WaterInfo {
    float heightOffset = 0;
    int textureCount = 0;
    float texRate = 0;
    std::string texFile;
    float minShallow[4] = {0, 0.3f, 0.5f, 0.8f};
    float maxShallow[4] = {0, 0.5f, 0.8f, 1.0f};
    float minDeep[4] = {0, 0.1f, 0.2f, 0.6f};
    float maxDeep[4] = {0, 0.3f, 0.5f, 0.8f};
    // Shoreline wave model fields
    std::string shoreDir;
    std::string shoreSFile;
    std::string shoreOCFile;
    std::string shoreICFile;
    int shoreSVar = 0;
    int shoreOCVar = 0;
    int shoreICVar = 0;
};

class TileSet {
public:
    TileSet() = default;
    ~TileSet();

    bool LoadFromMpq(const MpqManager& mpq, const W3EMap& w3e);
    bool LoadFromWorldEditData(const MpqManager& mpq, char tileset, int paletteSize);
    bool LoadCliffTextures(const MpqManager& mpq, const W3EMap& w3e);
    bool LoadCliffModelInfo(const MpqManager& mpq);
    bool LoadWaterInfo(const MpqManager& mpq, const W3EMap& w3e);

    int TextureCount() const { return (int)tiles_.size(); }
    const GroundTextureTile& GetTile(int index) const { return tiles_[index]; }
    bool HasBlightTexture() const { return blightTextureIndex_ >= 0 && blightTextureIndex_ < (int)tiles_.size(); }
    int BlightTextureIndex() const { return blightTextureIndex_; }

    int PaletteToTexIndex(int paletteIdx) const;
    const std::string& FallbackMode() const { return fallbackMode_; }
    int GroundPaletteMappedCount() const { return groundPaletteMappedCount_; }
    int GroundPaletteFallbackCount() const { return groundPaletteFallbackCount_; }
    int GroundSrgbTextureCount() const { return groundSrgbTextureCount_; }
    int GroundLinearTextureCount() const { return groundLinearTextureCount_; }

    int CliffTextureCount() const { return (int)cliffTiles_.size(); }
    const CliffTextureInfo& GetCliffTile(int index) const { return cliffTiles_[index]; }
    int CliffSrgbTextureCount() const { return cliffSrgbTextureCount_; }
    int CliffLinearTextureCount() const { return cliffLinearTextureCount_; }

    int CliffPaletteToTexIndex(int paletteIdx) const;

    bool HasCliffTextureArray() const { return cliffTextureArray_.id != 0; }
    sg_image GetCliffTextureArray() const { return cliffTextureArray_; }
    sg_view GetCliffTextureArrayView() const { return cliffTextureArrayView_; }
    int CliffTextureArraySize() const { return cliffTextureArraySize_; }

    int CliffToGroundPalIdx(int cliffPalIdx) const {
        if (cliffPalIdx < 0 || cliffPalIdx >= (int)cliffToGroundPal_.size()) return -1;
        return cliffToGroundPal_[cliffPalIdx];
    }

    const WaterInfo& GetWaterInfo() const { return waterInfo_; }
    bool HasWaterInfo() const { return waterLoaded_; }
    bool HasWaterTexture() const { return waterTextureArray_.id != 0; }
    sg_image GetWaterTextureArray() const { return waterTextureArray_; }
    int WaterFrameCount() const { return waterTextureCount_; }
    float WaterTexRate() const { return waterInfo_.texRate; }
    bool WaterNeedsSRGBFix() const { return waterNeedsSRGBFix_; }
    bool WaterSrgbUpload() const { return waterSrgbUpload_; }
    const std::string& WaterSourceFormat() const { return waterSourceFormat_; }
    int WaterSourceWidth() const { return waterSourceWidth_; }
    int WaterSourceHeight() const { return waterSourceHeight_; }
    int WaterSrgbTextureCount() const { return waterSrgbTextureCount_; }
    int WaterLinearTextureCount() const { return waterLinearTextureCount_; }

    bool LoadWaterTextures(const MpqManager& mpq);

private:
    sg_image waterTextureArray_ = {0};   // SG_IMAGETYPE_ARRAY for water frames

    std::vector<GroundTextureTile> tiles_;
    std::vector<CliffTextureInfo> cliffTiles_;
    sg_image cliffTextureArray_ = {0};
    sg_view cliffTextureArrayView_ = {0};
    int cliffTextureArraySize_ = 0;
    WaterInfo waterInfo_;
    bool waterLoaded_ = false;
    int waterTextureCount_ = 0;
    std::vector<int> paletteToTex_;
    std::vector<int> paletteToCliffTex_;
    std::vector<int> cliffToGroundPal_;
    std::unordered_map<uint32_t, int> war3idToIndex_;
    int blightTextureIndex_ = -1;
    std::string fallbackMode_;
    int groundPaletteMappedCount_ = 0;
    int groundPaletteFallbackCount_ = 0;
    int groundSrgbTextureCount_ = 0;
    int groundLinearTextureCount_ = 0;
    int cliffSrgbTextureCount_ = 0;
    int cliffLinearTextureCount_ = 0;
    bool waterNeedsSRGBFix_ = false;
    bool waterSrgbUpload_ = false;
    std::string waterSourceFormat_ = "unknown";
    int waterSourceWidth_ = 0;
    int waterSourceHeight_ = 0;
    int waterSrgbTextureCount_ = 0;
    int waterLinearTextureCount_ = 0;
};
