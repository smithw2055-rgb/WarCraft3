#include "asset/BLP.h"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "model/mdx.h"
#include "resource/mpq_storm.h"
#include "resource/slk.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>
#include <string_view>

namespace fs = std::filesystem;

namespace {

struct AtlasInfo {
    std::string key;
    std::string resourcePath;
    int width = 0;
    int height = 0;
};

struct FdfTextureCrop {
    std::string textureName;
    std::string fileKey;
    std::string sourceName;
    float u0 = 0.0f;
    float u1 = 1.0f;
    float v0 = 0.0f;
    float v1 = 1.0f;
};

struct BrushInfo {
    std::string key;
    std::string imageSource;
    std::string skinField;
    std::string sourcePath;
    int tileViewport = 0;
    int rotateDegrees = 0;
};

struct PackedCropInfo {
    std::string key;
    std::string imageSource;
    std::string sourcePath;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct DerivedSegmentInfo {
    std::string key;
    std::string sourceKey;
    int segmentIndex = 0;
    int segmentCount = 1;
    int rotateDegrees = 0;
    bool trimAlpha = false;
    int alignVisibleY = 0;
};

BLPImage CropImageSegment(const BLPImage& image, int x, int y, int width, int height) {
    BLPImage cropped;
    cropped.width = width;
    cropped.height = height;
    cropped.pixels.assign((size_t)width * (size_t)height * 4, 0);
    for (int row = 0; row < height; ++row) {
        const size_t src = ((size_t)(y + row) * (size_t)image.width + (size_t)x) * 4;
        const size_t dst = (size_t)row * (size_t)width * 4;
        std::copy(image.pixels.begin() + src,
                  image.pixels.begin() + src + (size_t)width * 4,
                  cropped.pixels.begin() + dst);
    }
    return cropped;
}

BLPImage RotateImage270(const BLPImage& image) {
    BLPImage rotated;
    rotated.width = image.height;
    rotated.height = image.width;
    rotated.pixels.assign((size_t)rotated.width * (size_t)rotated.height * 4, 0);
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const int dstX = y;
            const int dstY = image.width - 1 - x;
            const size_t src = ((size_t)y * (size_t)image.width + (size_t)x) * 4;
            const size_t dst = ((size_t)dstY * (size_t)rotated.width + (size_t)dstX) * 4;
            std::copy(image.pixels.begin() + src, image.pixels.begin() + src + 4, rotated.pixels.begin() + dst);
        }
    }
    return rotated;
}

BLPImage RotateImage90(const BLPImage& image) {
    BLPImage rotated;
    rotated.width = image.height;
    rotated.height = image.width;
    rotated.pixels.assign((size_t)rotated.width * (size_t)rotated.height * 4, 0);
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const int dstX = image.height - 1 - y;
            const int dstY = x;
            const size_t src = ((size_t)y * (size_t)image.width + (size_t)x) * 4;
            const size_t dst = ((size_t)dstY * (size_t)rotated.width + (size_t)dstX) * 4;
            std::copy(image.pixels.begin() + src, image.pixels.begin() + src + 4, rotated.pixels.begin() + dst);
        }
    }
    return rotated;
}

BLPImage TrimImageAlpha(const BLPImage& image) {
    int minX = image.width;
    int minY = image.height;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const uint8_t alpha = image.pixels[((size_t)y * (size_t)image.width + (size_t)x) * 4 + 3];
            if (alpha == 0) {
                continue;
            }
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }
    if (maxX < minX || maxY < minY) {
        return image;
    }
    return CropImageSegment(image, minX, minY, maxX - minX + 1, maxY - minY + 1);
}

BLPImage AlignVisibleAlphaY(const BLPImage& image, int direction) {
    if (direction == 0 || image.width <= 0 || image.height <= 0) {
        return image;
    }

    int minY = image.height;
    int maxY = -1;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const size_t a = ((size_t)y * (size_t)image.width + (size_t)x) * 4 + 3;
            if (image.pixels[a] != 0) {
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }
        }
    }

    if (maxY < minY) {
        return image;
    }

    const int offset = direction < 0 ? -minY : (image.height - 1 - maxY);
    if (offset == 0) {
        return image;
    }

    BLPImage aligned;
    aligned.width = image.width;
    aligned.height = image.height;
    aligned.pixels.assign((size_t)aligned.width * (size_t)aligned.height * 4, 0);
    for (int y = 0; y < image.height; ++y) {
        const int dstY = y + offset;
        if (dstY < 0 || dstY >= image.height) {
            continue;
        }
        for (int x = 0; x < image.width; ++x) {
            const size_t src = ((size_t)y * (size_t)image.width + (size_t)x) * 4;
            const size_t dst = ((size_t)dstY * (size_t)aligned.width + (size_t)x) * 4;
            std::copy(image.pixels.begin() + src, image.pixels.begin() + src + 4,
                      aligned.pixels.begin() + dst);
        }
    }
    return aligned;
}

void AppendHorizontalSegmentCrops(std::vector<PackedCropInfo>& crops,
                                  const std::string& sourceCropKey,
                                  const std::vector<std::string>& segmentKeys) {
    auto sourceIt = std::find_if(crops.begin(), crops.end(), [&](const PackedCropInfo& crop) {
        return crop.key == sourceCropKey;
    });
    if (sourceIt == crops.end() || sourceIt->width <= 0 || sourceIt->height <= 0 || segmentKeys.empty()) {
        return;
    }

    const PackedCropInfo source = *sourceIt;
    const int segmentWidth = source.width / (int)segmentKeys.size();
    if (segmentWidth <= 0) {
        return;
    }
    for (int i = 0; i < (int)segmentKeys.size(); ++i) {
        crops.push_back(PackedCropInfo{
            segmentKeys[i],
            source.imageSource,
            source.sourcePath,
            source.x + (segmentWidth * i),
            source.y,
            segmentWidth,
            source.height
        });
    }
}

int FdfBackdropTileViewport(float backgroundSize, float leftInset, float rightInset,
	                            float topInset, float bottomInset) {
    constexpr float kFdfPixelScale = 1920.0f;
    const float adjustedSize = backgroundSize - ((leftInset + rightInset + topInset + bottomInset) / 2.0f);
    return std::max(1, (int)std::lround(adjustedSize * kFdfPixelScale));
}

bool OpenDefaultArchives(MpqManager& mpq, const fs::path& root) {
    const fs::path resourcesDir = root / "resources";
    for (const auto& name : {"War3Patch.mpq", "War3xLocal.mpq", "war3x.mpq", "war3.mpq"}) {
        const fs::path path = resourcesDir / name;
        if (fs::exists(path)) {
            mpq.Open(path.string());
        }
    }
    return mpq.ArchiveCount() > 0;
}

std::string ToLower(std::string value);

std::vector<std::string> CandidatePaths(const std::string& fileStem) {
    std::vector<std::string> paths;
    const char* races[] = {"Human", "Orc", "Undead", "NightElf", "Naga"};
    for (const char* race : races) {
        paths.push_back("UI\\Console\\" + std::string(race) + "\\" + fileStem + ".blp");
        paths.push_back("UI\\Console\\" + std::string(race) + "\\" + fileStem + ".dds");
    }
    paths.push_back("UI\\Console\\" + fileStem + ".blp");
    paths.push_back("UI\\Widgets\\Console\\" + fileStem + ".blp");
    paths.push_back(fileStem + ".blp");
    return paths;
}

std::string Trim(std::string value) {
    while (!value.empty() && std::isspace((unsigned char)value.front())) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace((unsigned char)value.back())) {
        value.pop_back();
    }
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

void AddBlpCandidates(std::vector<std::string>& paths, const std::string& value) {
    if (value.empty()) {
        return;
    }
    paths.push_back(value);
    const std::string lower = ToLower(value);
    if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".blp") {
        paths.push_back(value + ".blp");
    }
}

std::vector<std::string> SkinFieldPaths(MpqManager& mpq, const std::string& fileStem,
                                        const std::string& skin) {
    std::vector<uint8_t> data;
    if (!mpq.ReadFile("UI\\war3skins.txt", data)) {
        return {};
    }

    std::vector<std::string> skinPaths;
    std::vector<std::string> defaultPaths;
    std::vector<std::string> fallbackPaths;
    std::string text(data.begin(), data.end());
    std::istringstream stream(text);
    std::string line;
    std::string section;
    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty() || line.front() == '/') {
            continue;
        }
        if (line.front() == '[') {
            const size_t end = line.find(']');
            section = end == std::string::npos ? std::string() : line.substr(1, end - 1);
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = Trim(line.substr(0, eq));
        if (key != fileStem) {
            continue;
        }
        std::string value = Trim(line.substr(eq + 1));
        const size_t comment = value.find("//");
        if (comment != std::string::npos) {
            value = Trim(value.substr(0, comment));
        }
        if (value.empty()) {
            continue;
        }
        if (section == skin) {
            AddBlpCandidates(skinPaths, value);
        } else if (section == "Default") {
            AddBlpCandidates(defaultPaths, value);
        } else {
            AddBlpCandidates(fallbackPaths, value);
        }
    }
    skinPaths.insert(skinPaths.end(), defaultPaths.begin(), defaultPaths.end());
    skinPaths.insert(skinPaths.end(), fallbackPaths.begin(), fallbackPaths.end());
    return skinPaths;
}

std::vector<std::string> SkinFieldPaths(MpqManager& mpq, const std::string& fileStem) {
    std::vector<std::string> paths = SkinFieldPaths(mpq, fileStem, "Orc");
    if (paths.empty()) {
        paths = SkinFieldPaths(mpq, fileStem, "Default");
    }
    return paths;
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return value;
}

std::string NormalizeBlpPath(std::string value) {
    value = Trim(value);
    const size_t separator = value.find_first_of(",;");
    if (separator != std::string::npos) {
        value = Trim(value.substr(0, separator));
    }
    std::replace(value.begin(), value.end(), '/', '\\');
    const std::string lower = ToLower(value);
    if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".blp") {
        value += ".blp";
    }
    return value;
}

std::string SanitizeAssetStem(const std::string& path) {
    std::string result;
    result.reserve(path.size());
    for (unsigned char c : path) {
        if (std::isalnum(c)) {
            result.push_back((char)std::tolower(c));
        } else {
            result.push_back('_');
        }
    }
    return result.empty() ? std::string("icon") : result;
}

bool ReadBySearch(MpqManager& mpq, const std::string& fileStem,
                  std::vector<uint8_t>& data, std::string& sourcePath) {
    const std::string needle = ToLower(fileStem);
    const std::vector<std::string> files = mpq.ListAllFiles("*");
    for (const std::string& file : files) {
        const std::string lower = ToLower(file);
        if (lower.find(needle) == std::string::npos) {
            continue;
        }
        if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".blp") {
            continue;
        }
        if (mpq.ReadFile(file, data)) {
            sourcePath = file;
            return true;
        }
    }
    return false;
}

bool WritePng(const fs::path& outputPath, int width, int height, const std::vector<uint8_t>& pixels) {
    return stbi_write_png(outputPath.string().c_str(), width, height, 4, pixels.data(), width * 4) != 0;
}

bool ReadTextFile(const fs::path& path, std::string& text) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream out;
    out << in.rdbuf();
    text = out.str();
    return true;
}

bool ExtractBlpToPng(MpqManager& mpq, const std::string& fileStem, const fs::path& outputDir,
                     std::vector<AtlasInfo>& atlases) {
    std::vector<uint8_t> data;
    std::string sourcePath;
    std::vector<std::string> candidates = SkinFieldPaths(mpq, fileStem);
    const std::vector<std::string> fallbackCandidates = CandidatePaths(fileStem);
    candidates.insert(candidates.end(), fallbackCandidates.begin(), fallbackCandidates.end());
    for (const std::string& candidate : candidates) {
        if (mpq.ReadFile(candidate, data)) {
            sourcePath = candidate;
            break;
        }
    }
    if (sourcePath.empty()) {
        ReadBySearch(mpq, fileStem, data, sourcePath);
    }

    if (sourcePath.empty()) {
        std::cerr << "missing: " << fileStem << std::endl;
        return false;
    }

    BLPImage image;
    if (!DecodeBLP(data.data(), data.size(), image)) {
        std::cerr << "decode failed: " << sourcePath << std::endl;
        return false;
    }

    std::error_code ec;
    fs::create_directories(outputDir, ec);
    const fs::path outputPath = outputDir / (fileStem + ".png");
    if (!WritePng(outputPath, image.width, image.height, image.pixels)) {
        std::cerr << "write failed: " << outputPath.string() << std::endl;
        return false;
    }

    std::cout << "extracted: " << sourcePath << " -> " << outputPath.string()
              << " (" << image.width << "x" << image.height << ")" << std::endl;
    atlases.push_back(AtlasInfo{fileStem, "console/" + fileStem + ".png", image.width, image.height});
    return true;
}

bool ExtractPathToPng(MpqManager& mpq, const std::string& sourcePath, const fs::path& outputPath) {
    std::vector<uint8_t> data;
    if (!mpq.ReadFile(sourcePath, data)) {
        std::cerr << "missing: " << sourcePath << std::endl;
        return false;
    }

    BLPImage image;
    if (!DecodeBLP(data.data(), data.size(), image)) {
        std::cerr << "decode failed: " << sourcePath << std::endl;
        return false;
    }

    std::error_code ec;
    fs::create_directories(outputPath.parent_path(), ec);
    if (!WritePng(outputPath, image.width, image.height, image.pixels)) {
        std::cerr << "write failed: " << outputPath.string() << std::endl;
        return false;
    }

    std::cout << "extracted: " << sourcePath << " -> " << outputPath.string()
              << " (" << image.width << "x" << image.height << ")" << std::endl;
    return true;
}

bool DecodeImageToRGBA(const std::vector<uint8_t>& data, int& width, int& height, std::vector<uint8_t>& pixels) {
    BLPImage blp;
    if (DecodeBLP(data.data(), data.size(), blp)) {
        width = blp.width;
        height = blp.height;
        pixels = std::move(blp.pixels);
        return width > 0 && height > 0 && !pixels.empty();
    }

    int channels = 0;
    stbi_uc* loaded = stbi_load_from_memory(data.data(), (int)data.size(), &width, &height, &channels, STBI_rgb_alpha);
    if (!loaded) {
        return false;
    }

    pixels.assign(loaded, loaded + (size_t)width * (size_t)height * 4);
    stbi_image_free(loaded);
    return width > 0 && height > 0 && !pixels.empty();
}

bool ExtractArchiveImageToPng(const fs::path& archivePath, const std::vector<std::string>& candidates,
                              const fs::path& outputPath) {
    MpqManager archive;
    if (!archive.Open(archivePath.string())) {
        return false;
    }

    for (const std::string& candidate : candidates) {
        std::vector<uint8_t> data;
        if (!archive.ReadFile(candidate, data)) {
            continue;
        }

        int width = 0;
        int height = 0;
        std::vector<uint8_t> pixels;
        if (!DecodeImageToRGBA(data, width, height, pixels)) {
            std::cerr << "decode failed: " << archivePath.string() << ":" << candidate << std::endl;
            continue;
        }

        std::error_code ec;
        fs::create_directories(outputPath.parent_path(), ec);
        if (!WritePng(outputPath, width, height, pixels)) {
            std::cerr << "write failed: " << outputPath.string() << std::endl;
            return false;
        }

        std::cout << "extracted: " << archivePath.string() << ":" << candidate
                  << " -> " << outputPath.string()
                  << " (" << width << "x" << height << ")" << std::endl;
        return true;
    }

    std::cerr << "missing preview image in: " << archivePath.string() << std::endl;
    return false;
}

bool ExtractBuffIcons(MpqManager& mpq, const fs::path& projectRoot) {
    std::vector<uint8_t> data;
    if (!mpq.ReadFile("Units\\AbilityBuffData.slk", data)) {
        std::cerr << "missing: Units\\AbilityBuffData.slk" << std::endl;
        return false;
    }

    SlkTable slk;
    if (!slk.Load(data.data(), data.size())) {
        std::cerr << "decode failed: Units\\AbilityBuffData.slk" << std::endl;
        return false;
    }

    const int colAlias = slk.FindColumn("alias");
    if (colAlias < 0) {
        std::cerr << "AbilityBuffData.slk missing alias column" << std::endl;
        return false;
    }

    TxtTable profile;
    const char* profileFiles[] = {
        "Units\\CommonAbilityFunc.txt",
        "Units\\HumanAbilityFunc.txt",
        "Units\\OrcAbilityFunc.txt",
        "Units\\NightElfAbilityFunc.txt",
        "Units\\UndeadAbilityFunc.txt",
        "Units\\NeutralAbilityFunc.txt",
        "Units\\CampaignAbilityFunc.txt",
        "Units\\ItemAbilityFunc.txt",
    };
    for (const char* file : profileFiles) {
        std::vector<uint8_t> profileData;
        if (mpq.ReadFile(file, profileData)) {
            profile.Load(profileData.data(), profileData.size());
        }
    }

    std::set<std::string> exportedSources;
    int extracted = 0;
    int missing = 0;
    const fs::path outputDir = projectRoot / "assets" / "ui" / "cache" / "icons";
    for (int row = 0; row < slk.Rows(); ++row) {
        const std::string alias = slk.GetString(row, colAlias, "");
        const std::string art = NormalizeBlpPath(profile.GetString(alias, "Buffart", ""));
        if (alias.empty() || art.empty()) {
            continue;
        }
        if (!exportedSources.insert(art).second) {
            continue;
        }
        if (!mpq.FileExists(art)) {
            ++missing;
            continue;
        }

        const fs::path outputPath = outputDir / (SanitizeAssetStem(art) + ".png");
        if (ExtractPathToPng(mpq, art, outputPath)) {
            ++extracted;
        }
    }

    std::cout << "buff icons: extracted=" << extracted
              << " uniqueSources=" << exportedSources.size()
              << " missing=" << missing << std::endl;
    return extracted > 0;
}

bool ExtractSkinFieldToPng(MpqManager& mpq, const std::string& skinField, const std::string& skin,
                           const fs::path& outputPath, std::string& sourcePath,
                           int& outWidth, int& outHeight) {
    std::vector<uint8_t> data;
    for (const std::string& candidate : SkinFieldPaths(mpq, skinField, skin)) {
        if (mpq.ReadFile(candidate, data)) {
            sourcePath = candidate;
            break;
        }
    }
    if (sourcePath.empty() && skinField.size() > 7 && skinField.substr(skinField.size() - 7) == "Neutral") {
        const std::string baseSkinField = skinField.substr(0, skinField.size() - 7);
        for (const std::string& candidate : SkinFieldPaths(mpq, baseSkinField, skin)) {
            if (mpq.ReadFile(candidate, data)) {
                sourcePath = candidate;
                break;
            }
        }
    }
    if (sourcePath.empty()) {
        std::cerr << "missing skin field: " << skinField << std::endl;
        return false;
    }

    BLPImage image;
    if (!DecodeBLP(data.data(), data.size(), image)) {
        std::cerr << "decode failed: " << sourcePath << std::endl;
        return false;
    }
    outWidth = image.width;
    outHeight = image.height;

    std::error_code ec;
    fs::create_directories(outputPath.parent_path(), ec);
    if (!WritePng(outputPath, image.width, image.height, image.pixels)) {
        std::cerr << "write failed: " << outputPath.string() << std::endl;
        return false;
    }

    std::cout << "extracted: " << skinField << "=" << sourcePath << " -> "
              << outputPath.string() << " (" << image.width << "x" << image.height << ")" << std::endl;
    return true;
}

bool ExtractSkinFieldToPng(MpqManager& mpq, const std::string& skinField, const std::string& skin,
                           const fs::path& outputPath, std::string& sourcePath) {
    int ignoredWidth = 0;
    int ignoredHeight = 0;
    return ExtractSkinFieldToPng(mpq, skinField, skin, outputPath, sourcePath, ignoredWidth, ignoredHeight);
}

bool ReadSkinFieldImage(MpqManager& mpq, const std::string& skinField, const std::string& skin,
                        BLPImage& image, std::string& sourcePath) {
    std::vector<uint8_t> data;
    for (const std::string& candidate : SkinFieldPaths(mpq, skinField, skin)) {
        if (mpq.ReadFile(candidate, data)) {
            sourcePath = candidate;
            break;
        }
    }
    if (sourcePath.empty() && skinField.size() > 7 && skinField.substr(skinField.size() - 7) == "Neutral") {
        const std::string baseSkinField = skinField.substr(0, skinField.size() - 7);
        for (const std::string& candidate : SkinFieldPaths(mpq, baseSkinField, skin)) {
            if (mpq.ReadFile(candidate, data)) {
                sourcePath = candidate;
                break;
            }
        }
    }
    if (sourcePath.empty()) {
        std::cerr << "missing skin field: " << skinField << std::endl;
        return false;
    }
    if (!DecodeBLP(data.data(), data.size(), image)) {
        std::cerr << "decode failed: " << sourcePath << std::endl;
        return false;
    }
    return true;
}

bool ExtractSkinFieldsToAtlas(MpqManager& mpq, const std::vector<std::pair<std::string, std::string>>& fields,
                              const std::string& skin, const fs::path& outputPath,
                              const std::string& imageSource, std::vector<PackedCropInfo>& packedCrops,
                              const std::vector<DerivedSegmentInfo>& derivedSegments = {}) {
    struct DecodedAtlasEntry {
        std::string key;
        std::string sourcePath;
        BLPImage image;
    };

    std::vector<DecodedAtlasEntry> entries;
    bool ok = true;
    for (const auto& field : fields) {
        DecodedAtlasEntry entry;
        entry.key = field.second;
        ok = ReadSkinFieldImage(mpq, field.first, skin, entry.image, entry.sourcePath) && ok;
        if (!entry.sourcePath.empty()) {
            entries.push_back(std::move(entry));
        }
    }

    for (const DerivedSegmentInfo& derived : derivedSegments) {
        auto sourceIt = std::find_if(entries.begin(), entries.end(), [&](const DecodedAtlasEntry& entry) {
            return entry.key == derived.sourceKey;
        });
        if (sourceIt == entries.end() || derived.segmentCount <= 0) {
            ok = false;
            continue;
        }

        const int segmentWidth = sourceIt->image.width / derived.segmentCount;
        if (segmentWidth <= 0 || derived.segmentIndex < 0 || derived.segmentIndex >= derived.segmentCount) {
            ok = false;
            continue;
        }

        BLPImage segment = CropImageSegment(sourceIt->image, segmentWidth * derived.segmentIndex, 0,
                                            segmentWidth, sourceIt->image.height);
        if (derived.rotateDegrees == 90) {
            segment = RotateImage90(segment);
        } else if (derived.rotateDegrees == 270) {
            segment = RotateImage270(segment);
        }
        if (derived.trimAlpha) {
            segment = TrimImageAlpha(segment);
        }
        if (derived.alignVisibleY != 0) {
            segment = AlignVisibleAlphaY(segment, derived.alignVisibleY);
        }

        entries.push_back(DecodedAtlasEntry{
            derived.key,
            sourceIt->sourcePath + " derived",
            std::move(segment)
        });
    }

    int atlasWidth = 0;
    int atlasHeight = 0;
    for (const DecodedAtlasEntry& entry : entries) {
        atlasWidth += entry.image.width;
        atlasHeight = std::max(atlasHeight, entry.image.height);
    }

    if (entries.empty() || atlasWidth <= 0 || atlasHeight <= 0) {
        return false;
    }

    std::vector<uint8_t> pixels((size_t)atlasWidth * (size_t)atlasHeight * 4, 0);
    int x = 0;
    for (const DecodedAtlasEntry& entry : entries) {
        for (int row = 0; row < entry.image.height; ++row) {
            const size_t dst = ((size_t)row * (size_t)atlasWidth + (size_t)x) * 4;
            const size_t src = (size_t)row * (size_t)entry.image.width * 4;
            std::copy(entry.image.pixels.begin() + src,
                      entry.image.pixels.begin() + src + (size_t)entry.image.width * 4,
                      pixels.begin() + dst);
        }
        packedCrops.push_back(PackedCropInfo{entry.key, imageSource, entry.sourcePath,
                                             x, 0, entry.image.width, entry.image.height});
        x += entry.image.width;
    }

    std::error_code ec;
    fs::create_directories(outputPath.parent_path(), ec);
    if (!WritePng(outputPath, atlasWidth, atlasHeight, pixels)) {
        std::cerr << "write failed: " << outputPath.string() << std::endl;
        return false;
    }
    std::cout << "packed atlas: " << outputPath.string() << " (" << atlasWidth << "x"
              << atlasHeight << ", entries=" << entries.size() << ")" << std::endl;
    return ok;
}

int FindResources(MpqManager& mpq, const std::string& needle) {
    const std::string lowerNeedle = ToLower(needle);
    int count = 0;
    for (const std::string& file : mpq.ListAllFiles("*")) {
        if (ToLower(file).find(lowerNeedle) == std::string::npos) {
            continue;
        }
        std::cout << file << std::endl;
        ++count;
    }
    return count;
}

int DumpResource(MpqManager& mpq, const std::string& path) {
    std::vector<uint8_t> data;
    if (!mpq.ReadFile(path, data)) {
        std::cerr << "missing: " << path << std::endl;
        return 1;
    }
    std::cout.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
    return 0;
}

int ListMdxTextures(MpqManager& mpq, const std::string& path) {
    std::vector<uint8_t> data;
    if (!mpq.ReadFile(path, data)) {
        std::cerr << "missing: " << path << std::endl;
        return 1;
    }

    MdxModel model;
    if (!MdxLoad(data.data(), data.size(), model)) {
        std::cerr << "mdx parse failed: " << path << std::endl;
        return 1;
    }

    std::set<std::string> uniquePaths;
    for (const MdxTextureRef& texture : model.textures) {
        if (!texture.path.empty()) {
            uniquePaths.insert(texture.path);
        } else if (texture.replaceableId != 0) {
            uniquePaths.insert("ReplaceableId:" + std::to_string(texture.replaceableId));
        }
    }

    for (const std::string& texturePath : uniquePaths) {
        std::cout << texturePath << std::endl;
    }
    return uniquePaths.empty() ? 1 : 0;
}

int InspectMdx(MpqManager& mpq, const std::string& path) {
    std::vector<uint8_t> data;
    if (!mpq.ReadFile(path, data)) {
        std::cerr << "missing: " << path << std::endl;
        return 1;
    }

    MdxModel model;
    if (!MdxLoad(data.data(), data.size(), model)) {
        std::cerr << "mdx parse failed: " << path << std::endl;
        return 1;
    }

    std::cout << "model=" << model.name
              << " version=" << model.version
              << " geosets=" << model.geosets.size()
              << " materials=" << model.materials.size()
              << " textures=" << model.textures.size()
              << " sequences=" << model.sequences.size()
              << " cameras=" << model.cameras.size()
              << " pre2=" << model.particleEmitters2.size()
              << "\n";
    std::cout << "bounds min=(" << model.boundsMin[0] << "," << model.boundsMin[1] << "," << model.boundsMin[2]
              << ") max=(" << model.boundsMax[0] << "," << model.boundsMax[1] << "," << model.boundsMax[2]
              << ") radius=" << model.boundsRadius << "\n";

    for (size_t i = 0; i < model.sequences.size(); ++i) {
        const MdxSequence& seq = model.sequences[i];
        std::cout << "sequence[" << i << "] name=\"" << seq.name << "\" frames="
                  << seq.startFrame << ".." << seq.endFrame
                  << " flags=" << seq.flags
                  << " moveSpeed=" << seq.moveSpeed << "\n";
    }

    for (size_t i = 0; i < model.cameras.size(); ++i) {
        const MdxCamera& cam = model.cameras[i];
        std::cout << "camera[" << i << "] name=\"" << cam.name << "\" pos=("
                  << cam.pos[0] << "," << cam.pos[1] << "," << cam.pos[2]
                  << ") target=(" << cam.target[0] << "," << cam.target[1] << "," << cam.target[2]
                  << ") fov=" << cam.fov
                  << " near=" << cam.nearClip
                  << " far=" << cam.farClip
                  << " timelines=" << cam.timelines.size() << "\n";
    }

    for (size_t i = 0; i < model.textures.size(); ++i) {
        const MdxTextureRef& texture = model.textures[i];
        if (!texture.path.empty()) {
            std::cout << "texture[" << i << "] path=\"" << texture.path
                      << "\" exists=" << (mpq.FileExists(texture.path) ? "yes" : "no") << "\n";
        } else {
            std::cout << "texture[" << i << "] replaceableId=" << texture.replaceableId << "\n";
        }
    }

    for (size_t gi = 0; gi < model.geosets.size(); ++gi) {
        const MdxGeoset& geoset = model.geosets[gi];
        std::cout << "geoset[" << gi << "] materialId=" << geoset.materialId
                  << " vertices=" << (geoset.vertices.size() / 3)
                  << " indices=" << geoset.indices.size() << "\n";
    }

    auto tagToString = [](uint32_t tag) {
        std::string s;
        s.push_back((char)((tag >> 24) & 0xFF));
        s.push_back((char)((tag >> 16) & 0xFF));
        s.push_back((char)((tag >> 8) & 0xFF));
        s.push_back((char)(tag & 0xFF));
        return s;
    };

    auto printTimeline = [&](const MdxTimeline& timeline, const char* prefix) {
        std::cout << prefix << " timeline=" << tagToString(timeline.tag)
                  << " keys=" << timeline.frames.size()
                  << " interp=" << (int)timeline.interpolationType
                  << " globalSeq=" << timeline.globalSequenceId;
        if (!timeline.frames.empty()) {
            std::cout << " first=(" << timeline.frames.front();
            const int elemCount = timeline.frames.empty() ? 0 : (int)(timeline.values.size() / timeline.frames.size());
            for (int elem = 0; elem < elemCount && elem < 4; ++elem) {
                std::cout << "," << timeline.values[elem];
            }
            std::cout << ") last=(" << timeline.frames.back();
            const size_t lastBase = (timeline.frames.size() - 1) * (size_t)std::max(1, elemCount);
            for (int elem = 0; elem < elemCount && elem < 4; ++elem) {
                std::cout << "," << timeline.values[lastBase + elem];
            }
            std::cout << ")";
        }
        std::cout << "\n";
    };

    for (size_t mi = 0; mi < model.materials.size(); ++mi) {
        const MdxMaterial& material = model.materials[mi];
        std::cout << "material[" << mi << "] layers=" << material.layers.size()
                  << " flags=" << material.flags
                  << " priority=" << material.priorityPlane << "\n";
        for (size_t li = 0; li < material.layers.size(); ++li) {
            const MdxLayer& layer = material.layers[li];
            std::cout << "  layer[" << li << "] filter=" << layer.filterMode
                      << " flags=" << layer.flags
                      << " textureId=" << layer.textureId
                      << " texAnimId=" << layer.textureAnimationId
                      << " coordId=" << layer.coordId
                      << " alpha=" << layer.alpha;
            if (layer.textureId >= 0 && layer.textureId < (int)model.textures.size()) {
                std::cout << " path=\"" << model.textures[(size_t)layer.textureId].path << "\"";
            }
            std::cout << " timelines=" << layer.timelines.size() << "\n";
            for (const MdxTimeline& timeline : layer.timelines) {
                printTimeline(timeline, "    ");
            }
        }
    }

    for (size_t gi = 0; gi < model.geosetAnims.size(); ++gi) {
        const MdxGeosetAnim& geosetAnim = model.geosetAnims[gi];
        std::cout << "geosetAnim[" << gi << "] geosetId=" << geosetAnim.geosetId
                  << " flags=" << geosetAnim.flags
                  << " alpha=" << geosetAnim.alpha
                  << " color=(" << geosetAnim.color[0]
                  << "," << geosetAnim.color[1]
                  << "," << geosetAnim.color[2]
                  << ") timelines=" << geosetAnim.timelines.size() << "\n";
        for (const MdxTimeline& timeline : geosetAnim.timelines) {
            printTimeline(timeline, "  ");
        }
    }

    for (size_t ti = 0; ti < model.texAnims.size(); ++ti) {
        const MdxTexAnim& texAnim = model.texAnims[ti];
        std::cout << "texAnim[" << ti << "] timelines=" << texAnim.timelines.size() << "\n";
        for (const MdxTimeline& timeline : texAnim.timelines) {
            printTimeline(timeline, "  ");
        }
    }
    return 0;
}

std::vector<float> ParseFloatList(std::string value) {
    for (char& c : value) {
        if (c == ',') {
            c = ' ';
        }
    }
    std::istringstream stream(value);
    std::vector<float> values;
    float parsed = 0.0f;
    while (stream >> parsed) {
        values.push_back(parsed);
    }
    return values;
}

std::vector<FdfTextureCrop> ParseTextureCropsFromFdfText(const std::string& text,
                                                         const std::string& sourceName) {
    std::vector<FdfTextureCrop> crops;
    std::istringstream stream(text);
    std::string line;
    bool inTexture = false;
    FdfTextureCrop current;
    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty() || line.rfind("//", 0) == 0) {
            continue;
        }
        if (line.find("Texture") == 0 && line.find('{') != std::string::npos) {
            inTexture = true;
            current = FdfTextureCrop{};
            current.sourceName = sourceName;
            const size_t first = line.find('"');
            const size_t last = line.find_last_of('"');
            if (first != std::string::npos && last != std::string::npos && last > first) {
                current.textureName = line.substr(first + 1, last - first - 1);
            }
            continue;
        }
        if (!inTexture) {
            continue;
        }
        if (line.find('}') != std::string::npos) {
            if (!current.fileKey.empty()) {
                crops.push_back(current);
            }
            inTexture = false;
            continue;
        }
        if (line.rfind("File", 0) == 0) {
            size_t first = line.find('"');
            size_t last = line.find_last_of('"');
            if (first != std::string::npos && last != std::string::npos && last > first) {
                current.fileKey = line.substr(first + 1, last - first - 1);
            }
            continue;
        }
        if (line.rfind("TexCoord", 0) == 0) {
            std::string valuesText = line.substr(std::string("TexCoord").size());
            if (!valuesText.empty() && valuesText.back() == ',') {
                valuesText.pop_back();
            }
            const std::vector<float> values = ParseFloatList(valuesText);
            if (values.size() >= 4) {
                current.u0 = values[0];
                current.u1 = values[1];
                current.v0 = values[2];
                current.v1 = values[3];
            }
        }
    }
    return crops;
}

std::vector<FdfTextureCrop> ParseConsoleUiTextureCrops(MpqManager& mpq, const fs::path& root) {
    std::vector<FdfTextureCrop> crops;
    const fs::path refUiDir = root / "warsmash_ref" / "resources" / "UI" / "FrameDef" / "SmashUI";
    const fs::path refFiles[] = {
        refUiDir / "SmashConsoleUI.fdf",
        refUiDir / "InventoryCover.fdf",
    };
    for (const fs::path& refFile : refFiles) {
        std::string text;
        if (ReadTextFile(refFile, text)) {
            std::vector<FdfTextureCrop> parsed = ParseTextureCropsFromFdfText(
                text, "warsmash_ref/" + refFile.filename().string());
            crops.insert(crops.end(), parsed.begin(), parsed.end());
        }
    }
    std::vector<uint8_t> data;
    if (mpq.ReadFile("UI\\FrameDef\\UI\\UpperButtonBar.fdf", data)) {
        std::vector<FdfTextureCrop> parsed = ParseTextureCropsFromFdfText(
            std::string(data.begin(), data.end()), "UI\\FrameDef\\UI\\UpperButtonBar.fdf");
        crops.insert(crops.end(), parsed.begin(), parsed.end());
    }
    if (crops.empty() && mpq.ReadFile("UI\\FrameDef\\UI\\ConsoleUI.fdf", data)) {
        return ParseTextureCropsFromFdfText(std::string(data.begin(), data.end()),
                                            "UI\\FrameDef\\UI\\ConsoleUI.fdf");
    }
    return crops;
}

bool NearlyEqual(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

std::string CropKeyFor(const FdfTextureCrop& crop) {
    if (!crop.textureName.empty()) {
        if (crop.textureName == "ConsoleTexture02Top" && crop.fileKey == "ConsoleTexture02") {
            if (NearlyEqual(crop.u0, 0.0f)) {
                return "ConsoleTexture02TopLeftCrop";
            }
            if (NearlyEqual(crop.u1, 1.0f)) {
                return "ConsoleTexture02TopRightCrop";
            }
        }
        return crop.textureName + "Crop";
    }
    if (crop.fileKey == "ConsoleInventoryCoverTexture") {
        return "ConsoleInventoryCoverBottomCrop";
    }
    if (NearlyEqual(crop.v0, 0.0f) && NearlyEqual(crop.v1, 0.125f)) {
        if (crop.fileKey == "ConsoleTexture02" && NearlyEqual(crop.u0, 0.0f)) {
            return crop.fileKey + "TopLeftCrop";
        }
        if (crop.fileKey == "ConsoleTexture02" && NearlyEqual(crop.u1, 1.0f)) {
            return crop.fileKey + "TopRightCrop";
        }
        return crop.fileKey + "TopCrop";
    }
    if (NearlyEqual(crop.v1, 1.0f)) {
        return crop.fileKey + "BottomCrop";
    }
    return crop.fileKey + "Crop";
}

void WriteCropResource(std::ostream& out, const FdfTextureCrop& crop, const AtlasInfo& atlas) {
    const int x = std::clamp((int)std::lround(crop.u0 * atlas.width), 0, atlas.width);
    const int x1 = std::clamp((int)std::lround(crop.u1 * atlas.width), 0, atlas.width);
    const int y = std::clamp((int)std::lround(crop.v0 * atlas.height), 0, atlas.height);
    const int y1 = std::clamp((int)std::lround(crop.v1 * atlas.height), 0, atlas.height);
    const int w = std::max(0, x1 - x);
    const int h = std::max(0, y1 - y);
    if (w <= 0 || h <= 0) {
        return;
    }
    out << "  <!-- FDF ConsoleUI File " << crop.fileKey
        << (crop.textureName.empty() ? "" : " Texture " + crop.textureName)
        << (crop.sourceName.empty() ? "" : " from " + crop.sourceName)
        << ": uv=(" << crop.u0 << "," << crop.v0 << ")-(" << crop.u1 << "," << crop.v1
        << "), source=" << atlas.width << "x" << atlas.height << " -->\n"
        << "  <CroppedBitmap x:Key=\"" << CropKeyFor(crop) << "\" Source=\""
        << atlas.resourcePath << "\" SourceRect=\"" << x << "," << y << "," << w << "," << h << "\"/>\n";
}

bool WriteUiResourceXaml(MpqManager& mpq, const fs::path& outputPath,
                         const std::vector<AtlasInfo>& atlases,
                         const std::vector<PackedCropInfo>& packedCrops,
                         const std::vector<BrushInfo>& brushes) {
    std::error_code ec;
    fs::create_directories(outputPath.parent_path(), ec);

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        std::cerr << "write failed: " << outputPath.string() << std::endl;
        return false;
    }

    std::map<std::string, AtlasInfo> atlasByKey;
    for (const AtlasInfo& atlas : atlases) {
        atlasByKey[atlas.key] = atlas;
    }

    out << "<ResourceDictionary\n"
        << "  xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\"\n"
        << "  xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">\n\n";

    const std::vector<FdfTextureCrop> crops = ParseConsoleUiTextureCrops(mpq, outputPath.parent_path().parent_path().parent_path());
    for (const FdfTextureCrop& crop : crops) {
        auto it = atlasByKey.find(crop.fileKey);
        if (it != atlasByKey.end()) {
            WriteCropResource(out, crop, it->second);
        }
    }

    for (const PackedCropInfo& crop : packedCrops) {
        out << "  <!-- packed atlas " << crop.sourcePath << " -->\n"
            << "  <CroppedBitmap x:Key=\"" << crop.key << "\" Source=\""
            << crop.imageSource << "\" SourceRect=\"" << crop.x << "," << crop.y << ","
            << crop.width << "," << crop.height << "\"/>\n";
    }

    bool wroteInventoryCover = false;
    for (const FdfTextureCrop& crop : crops) {
        if (crop.fileKey == "ConsoleInventoryCoverTexture") {
            wroteInventoryCover = true;
            break;
        }
    }
    auto coverIt = atlasByKey.find("ConsoleInventoryCoverTexture");
    if (!wroteInventoryCover && coverIt != atlasByKey.end()) {
        WriteCropResource(out, FdfTextureCrop{"ConsoleInventoryCoverTexture", "ConsoleInventoryCoverTexture", "fallback", 0.0f, 1.0f, 0.3125f, 1.0f}, coverIt->second);
    }

    out << "\n";
	    for (const BrushInfo& brush : brushes) {
	        out << "  <!-- war3skins " << brush.skinField << "=" << brush.sourcePath << " -->\n"
	            << "  <ImageBrush x:Key=\"" << brush.key << "\" ImageSource=\"" << brush.imageSource << "\"";
	        if (brush.tileViewport > 0) {
	            out << " Stretch=\"Fill\" TileMode=\"Tile\" ViewportUnits=\"Absolute\" Viewport=\"0,0,"
	                << brush.tileViewport << "," << brush.tileViewport << "\"";
	        } else {
	            out << " Stretch=\"Fill\"";
	        }
	        if (brush.rotateDegrees != 0) {
	            out << ">\n"
	                << "    <ImageBrush.RelativeTransform>\n"
	                << "      <RotateTransform Angle=\"" << brush.rotateDegrees << "\" CenterX=\"0.5\" CenterY=\"0.5\"/>\n"
	                << "    </ImageBrush.RelativeTransform>\n"
	                << "  </ImageBrush>\n";
	        } else {
	            out << "/>\n";
	        }
	    }
    out << "</ResourceDictionary>\n";

    std::cout << "generated: " << outputPath.string() << std::endl;
    return true;
}

} // namespace

static void PrintUsage(const char* exe) {
    std::cerr
        << "Usage:\n"
        << "  " << exe << " [output-dir]\n"
        << "  " << exe << " --find <pattern>\n"
        << "  " << exe << " --dump <mpq-resource-path>\n"
        << "  " << exe << " --list-mdx-textures <mpq-resource-path>\n"
        << "  " << exe << " --inspect-mdx <mpq-resource-path>\n"
        << "  " << exe << " --extract-png <mpq-resource-path> <output-path>\n"
        << "  " << exe << " --extract-map-preview <map-archive> <output-path>\n";
}

int main(int argc, char** argv) {
    fs::path root = PROJECT_SOURCE_DIR;
    fs::path outputDir = root / "assets" / "ui" / "console";

    MpqManager mpq;
    if (!OpenDefaultArchives(mpq, root)) {
        std::cerr << "No MPQ archives opened from " << (root / "resources").string() << std::endl;
        return 2;
    }

    if (argc > 1) {
        const std::string command = argv[1];
        if (command == "--help" || command == "-h") {
            PrintUsage(argv[0]);
            return 0;
        }
        if (command == "--find") {
            if (argc <= 2) {
                std::cerr << "--find requires a search pattern\n";
                PrintUsage(argv[0]);
                return 2;
            }
            return FindResources(mpq, argv[2]) > 0 ? 0 : 1;
        }
        if (command == "--dump") {
            if (argc <= 2) {
                std::cerr << "--dump requires an MPQ resource path\n";
                PrintUsage(argv[0]);
                return 2;
            }
            return DumpResource(mpq, argv[2]);
        }
        if (command == "--list-mdx-textures") {
            if (argc <= 2) {
                std::cerr << "--list-mdx-textures requires an MPQ model path\n";
                PrintUsage(argv[0]);
                return 2;
            }
            return ListMdxTextures(mpq, argv[2]);
        }
        if (command == "--inspect-mdx") {
            if (argc <= 2) {
                std::cerr << "--inspect-mdx requires an MPQ model path\n";
                PrintUsage(argv[0]);
                return 2;
            }
            return InspectMdx(mpq, argv[2]);
        }
        if (command == "--extract-png") {
            if (argc <= 3) {
                std::cerr << "--extract-png requires an MPQ resource path and output path\n";
                PrintUsage(argv[0]);
                return 2;
            }
            return ExtractPathToPng(mpq, argv[2], fs::path(argv[3])) ? 0 : 1;
        }
        if (command == "--extract-map-preview") {
            if (argc <= 3) {
                std::cerr << "--extract-map-preview requires a map archive path and output path\n";
                PrintUsage(argv[0]);
                return 2;
            }
            const std::vector<std::string> candidates = {
                "war3mapPreview.blp",
                "war3mapPreview.tga",
                "war3mapMap.blp",
                "war3mapMap.tga",
            };
            return ExtractArchiveImageToPng(fs::path(argv[2]), candidates, fs::path(argv[3])) ? 0 : 1;
        }
        if (!command.empty() && command[0] == '-') {
            std::cerr << "Unknown option: " << command << "\n";
            PrintUsage(argv[0]);
            return 2;
        }
        outputDir = argv[1];
    }

    if (argc > 2) {
        std::cerr << "Unexpected extra argument: " << argv[2] << "\n";
        PrintUsage(argv[0]);
        return 2;
    }

    const char* assets[] = {
        "ConsoleTexture01",
        "ConsoleTexture02",
        "ConsoleTexture03",
        "ConsoleTexture04",
        "ConsoleInventoryCoverTexture",
    };

    bool ok = true;
    std::vector<AtlasInfo> atlases;
    for (const char* asset : assets) {
        ok = ExtractBlpToPng(mpq, asset, outputDir, atlases) && ok;
    }

    struct ManifestAsset {
        const char* skinField;
        const char* outputPath;
        const char* brushKey;
    };

    const ManifestAsset manifest[] = {
        {"ConsoleInventoryNoCapacity", "assets\\ui\\widgets\\console\\orc\\ConsoleInventoryNoCapacity.png", "InventorySlotFillerBrush"},
        {"UpperMenuButtonTexture", "assets\\ui\\widgets\\console\\orc\\UpperMenuButtonTexture.png", "UpperMenuButtonBrush"},
        {"DefaultButton", "assets\\ui\\widgets\\console\\human\\DefaultButton.png", "DefaultCommandButtonBrush"},
        {"BuildQueueBackdrop", "assets\\ui\\widgets\\console\\human\\BuildQueueBackdrop.png", "BuildQueueBackdropBrush"},
        {"CargoBackdrop", "assets\\ui\\widgets\\console\\human\\CargoBackdrop.png", "CargoBackdropBrush"},
    };

    const fs::path projectRoot = PROJECT_SOURCE_DIR;
    std::vector<BrushInfo> brushes;
    for (const ManifestAsset& asset : manifest) {
        std::string sourcePath;
        const fs::path outputPath = projectRoot / asset.outputPath;
        int width = 0;
        int height = 0;
        const bool extracted = ExtractSkinFieldToPng(mpq, asset.skinField, "Orc", outputPath,
                                                     sourcePath, width, height);
        if (extracted) {
            std::string imageSource = asset.outputPath;
            const std::string prefix = "assets\\ui\\";
            if (imageSource.rfind(prefix, 0) == 0) {
                imageSource = imageSource.substr(prefix.size());
            }
            std::replace(imageSource.begin(), imageSource.end(), '\\', '/');
            if (std::string(asset.skinField) == "UpperMenuButtonTexture") {
                atlases.push_back(AtlasInfo{asset.skinField, imageSource, width, height});
            }
            brushes.push_back(BrushInfo{asset.brushKey, imageSource, asset.skinField, sourcePath});
        }
        ok = extracted && ok;
    }

    ok = ExtractBuffIcons(mpq, projectRoot) && ok;

    std::vector<PackedCropInfo> packedCrops;
    const std::vector<std::pair<std::string, std::string>> feedbackBarFields = {
        {"SimpleManaBarConsole", "SimpleHealthBarCrop"},
        {"SimpleXpBarConsole", "SimpleXpBarCrop"},
        {"SimpleXpBarBorder", "SimpleXpBarBorderCrop"},
        {"SimpleProgressBarConsole", "SimpleProgressBarCrop"},
        {"SimpleProgressBarBorder", "SimpleProgressBarBorderCrop"},
        {"SimpleBuildTimeIndicator", "SimpleBuildTimeIndicatorCrop"},
        {"SimpleBuildTimeIndicatorBorder", "SimpleBuildTimeIndicatorBorderCrop"},
    };
    std::vector<PackedCropInfo> feedbackBarCrops;
    const bool packedFeedbackBars = ExtractSkinFieldsToAtlas(
        mpq, feedbackBarFields, "Orc",
        projectRoot / "assets" / "ui" / "widgets" / "feedback" / "bars" / "FeedbackBars.png",
        "widgets/feedback/bars/FeedbackBars.png", feedbackBarCrops);
    ok = packedFeedbackBars && ok;
    packedCrops.insert(packedCrops.end(), feedbackBarCrops.begin(), feedbackBarCrops.end());
    if (packedFeedbackBars) {
        const std::pair<const char*, const char*> feedbackBarBrushes[] = {
            {"SimpleHealthBarBrush", "SimpleHealthBarCrop"},
            {"SimpleXpBarBrush", "SimpleXpBarCrop"},
            {"SimpleXpBarBorderBrush", "SimpleXpBarBorderCrop"},
            {"SimpleProgressBarBrush", "SimpleProgressBarCrop"},
            {"SimpleProgressBarBorderBrush", "SimpleProgressBarBorderCrop"},
            {"SimpleBuildTimeIndicatorBrush", "SimpleBuildTimeIndicatorCrop"},
            {"SimpleBuildTimeIndicatorBorderBrush", "SimpleBuildTimeIndicatorBorderCrop"},
        };
        for (const auto& brush : feedbackBarBrushes) {
            brushes.push_back(BrushInfo{
                brush.first,
                std::string("{StaticResource ") + brush.second + "}",
                "FeedbackBars atlas",
                "assets\\ui\\widgets\\feedback\\bars\\FeedbackBars.png"
            });
        }
    }

    const std::vector<std::pair<std::string, std::string>> tooltipResourceFields = {
        {"ToolTipBackground", "TooltipBackgroundCrop"},
        {"ToolTipBorder", "TooltipBorderCrop"},
        {"ToolTipGoldIcon", "TooltipGoldIconCrop"},
        {"ToolTipLumberIcon", "TooltipLumberIconCrop"},
        {"ToolTipStonesIcon", "TooltipStonesIconCrop"},
        {"ToolTipManaIcon", "TooltipManaIconCrop"},
        {"ToolTipSupplyIcon", "TooltipSupplyIconCrop"},
        {"GoldIcon", "ResourceGoldIconCrop"},
        {"LumberIcon", "ResourceLumberIconCrop"},
        {"SupplyIcon", "ResourceSupplyIconCrop"},
        {"UpkeepIcon", "ResourceUpkeepIconCrop"},
    };
    std::vector<PackedCropInfo> tooltipResourceCrops;
    const bool packedTooltipResources = ExtractSkinFieldsToAtlas(
        mpq, tooltipResourceFields, "Orc",
        projectRoot / "assets" / "ui" / "widgets" / "feedback" / "resources" / "TooltipResourceIcons.png",
        "widgets/feedback/resources/TooltipResourceIcons.png", tooltipResourceCrops);
    ok = packedTooltipResources && ok;
    packedCrops.insert(packedCrops.end(), tooltipResourceCrops.begin(), tooltipResourceCrops.end());
    if (packedTooltipResources) {
        const std::pair<const char*, const char*> tooltipResourceBrushes[] = {
            {"TooltipBackgroundBrush", "TooltipBackgroundCrop"},
            {"TooltipBorderBrush", "TooltipBorderCrop"},
            {"TooltipGoldIconBrush", "TooltipGoldIconCrop"},
            {"TooltipLumberIconBrush", "TooltipLumberIconCrop"},
            {"TooltipStonesIconBrush", "TooltipStonesIconCrop"},
            {"TooltipManaIconBrush", "TooltipManaIconCrop"},
            {"TooltipSupplyIconBrush", "TooltipSupplyIconCrop"},
            {"ResourceGoldIconBrush", "ResourceGoldIconCrop"},
            {"ResourceLumberIconBrush", "ResourceLumberIconCrop"},
            {"ResourceSupplyIconBrush", "ResourceSupplyIconCrop"},
            {"ResourceUpkeepIconBrush", "ResourceUpkeepIconCrop"},
        };
        for (const auto& brush : tooltipResourceBrushes) {
            brushes.push_back(BrushInfo{
                brush.first,
                std::string("{StaticResource ") + brush.second + "}",
                "TooltipResourceIcons atlas",
                "assets\\ui\\widgets\\feedback\\resources\\TooltipResourceIcons.png"
            });
        }
    }

    const std::vector<std::pair<std::string, std::string>> commandButtonChromeFields = {
        {"ConsoleButtonBackground", "ConsoleButtonBackgroundCrop"},
        {"ConsoleButtonBorder", "ConsoleButtonBorderCrop"},
        {"ConsoleButtonPushedBorder", "ConsoleButtonPushedBorderCrop"},
        {"ConsoleButtonMouseOverHighlight", "ConsoleButtonHighlightCrop"},
        {"ConsoleButtonDisabledBackground", "ConsoleButtonDisabledBackgroundCrop"},
        {"CommandButtonActiveHighlight", "CommandButtonActiveHighlightCrop"},
        {"CommandButtonNumberOverlay", "CommandButtonNumberOverlayCrop"},
        {"CommandButtonHeroGlow", "CommandButtonHeroGlowCrop"},
        {"SelectedSubgroupHighlight", "SelectedSubgroupHighlightCrop"},
    };
    std::vector<PackedCropInfo> commandButtonChromeCrops;
    const bool packedCommandButtonChrome = ExtractSkinFieldsToAtlas(
        mpq, commandButtonChromeFields, "Orc",
        projectRoot / "assets" / "ui" / "widgets" / "console" / "human" / "CommandButtonChrome.png",
        "widgets/console/human/CommandButtonChrome.png", commandButtonChromeCrops);
    ok = packedCommandButtonChrome && ok;
    packedCrops.insert(packedCrops.end(), commandButtonChromeCrops.begin(), commandButtonChromeCrops.end());
    if (packedCommandButtonChrome) {
        const std::pair<const char*, const char*> commandButtonChromeBrushes[] = {
            {"ConsoleButtonBackgroundBrush", "ConsoleButtonBackgroundCrop"},
            {"ConsoleButtonBorderBrush", "ConsoleButtonBorderCrop"},
            {"ConsoleButtonPushedBorderBrush", "ConsoleButtonPushedBorderCrop"},
            {"ConsoleButtonHighlightBrush", "ConsoleButtonHighlightCrop"},
            {"ConsoleButtonDisabledBackgroundBrush", "ConsoleButtonDisabledBackgroundCrop"},
            {"CommandButtonActiveHighlightBrush", "CommandButtonActiveHighlightCrop"},
            {"CommandButtonNumberOverlayBrush", "CommandButtonNumberOverlayCrop"},
            {"CommandButtonHeroGlowBrush", "CommandButtonHeroGlowCrop"},
            {"SelectedSubgroupHighlightBrush", "SelectedSubgroupHighlightCrop"},
        };
        for (const auto& brush : commandButtonChromeBrushes) {
            brushes.push_back(BrushInfo{
                brush.first,
                std::string("{StaticResource ") + brush.second + "}",
                "CommandButtonChrome atlas",
                "assets\\ui\\widgets\\console\\human\\CommandButtonChrome.png"
            });
        }
    }

    const std::vector<std::pair<std::string, std::string>> escMenuChromeFields = {
        {"EscMenuBackground", "EscMenuBackgroundCrop"},
        {"EscMenuBorder", "EscMenuBorderCrop"},
        {"EscMenuButtonBackground", "EscMenuButtonBackgroundCrop"},
        {"EscMenuButtonBorder", "EscMenuButtonBorderCrop"},
        {"EscMenuButtonPushedBackground", "EscMenuButtonPushedBackgroundCrop"},
        {"EscMenuButtonPushedBorder", "EscMenuButtonPushedBorderCrop"},
        {"EscMenuButtonDisabledBackground", "EscMenuButtonDisabledBackgroundCrop"},
        {"EscMenuButtonDisabledBorder", "EscMenuButtonDisabledBorderCrop"},
        {"EscMenuButtonDisabledPushedBackground", "EscMenuButtonDisabledPushedBackgroundCrop"},
        {"EscMenuButtonDisabledPushedBorder", "EscMenuButtonDisabledPushedBorderCrop"},
        {"EscMenuButtonMouseOverHighlight", "EscMenuButtonHighlightCrop"},
        {"EscMenuSliderBackground", "EscMenuSliderBackgroundCrop"},
        {"EscMenuSliderBorder", "EscMenuSliderBorderCrop"},
        {"EscMenuSliderDisabledBorder", "EscMenuSliderDisabledBorderCrop"},
        {"EscMenuSliderThumbButton", "EscMenuSliderThumbButtonCrop"},
        {"EscMenuSliderDisabledThumbButton", "EscMenuSliderDisabledThumbButtonCrop"},
        {"EscMenuCheckBoxBackground", "EscMenuCheckBoxBackgroundCrop"},
        {"EscMenuDisabledCheckBoxBackground", "EscMenuDisabledCheckBoxBackgroundCrop"},
        {"EscMenuCheckBoxPushedBackground", "EscMenuCheckBoxPushedBackgroundCrop"},
        {"EscMenuCheckBoxCheckHighlight", "EscMenuCheckBoxCheckHighlightCrop"},
        {"EscMenuDisabledCheckHighlight", "EscMenuDisabledCheckHighlightCrop"},
        {"EscMenuRadioButtonBackground", "EscMenuRadioButtonBackgroundCrop"},
        {"EscMenuRadioButtonDisabledBackground", "EscMenuRadioButtonDisabledBackgroundCrop"},
        {"EscMenuRadioButtonPushedBackground", "EscMenuRadioButtonPushedBackgroundCrop"},
        {"EscMenuRadioButtonSelectedHighlight", "EscMenuRadioButtonSelectedHighlightCrop"},
        {"EscMenuDisabledRadioButtonSelectedHighlight", "EscMenuDisabledRadioButtonSelectedHighlightCrop"},
        {"EscMenuEditBoxBackground", "EscMenuEditBoxBackgroundCrop"},
        {"EscMenuEditBoxBorder", "EscMenuEditBoxBorderCrop"},
        {"EscMenuPopupMenuMenuBackground", "EscMenuPopupMenuMenuBackgroundCrop"},
        {"EscMenuPopupMenuArrow", "EscMenuPopupMenuArrowCrop"},
    };
    std::vector<PackedCropInfo> escMenuChromeCrops;
	    const bool packedEscMenuChrome = ExtractSkinFieldsToAtlas(
	        mpq, escMenuChromeFields, "Orc",
	        projectRoot / "assets" / "ui" / "widgets" / "escmenu" / "orc" / "EscMenuChrome.png",
	        "widgets/escmenu/orc/EscMenuChrome.png", escMenuChromeCrops,
	        {
	            {"EscMenuBorderTopTileCrop", "EscMenuBorderCrop", 2, 8, 270, false, -1},
	            {"EscMenuBorderBottomTileCrop", "EscMenuBorderCrop", 3, 8, 270, false, 1},
	        });
	    AppendHorizontalSegmentCrops(escMenuChromeCrops, "EscMenuBorderCrop", {
	        "EscMenuBorderLeftCrop",
	        "EscMenuBorderRightCrop",
	        "EscMenuBorderTopSourceCrop",
	        "EscMenuBorderBottomSourceCrop",
	        "EscMenuBorderUpperLeftCrop",
	        "EscMenuBorderUpperRightCrop",
	        "EscMenuBorderBottomLeftCrop",
	        "EscMenuBorderBottomRightCrop",
	    });
	    const auto appendButtonBorderCrops = [&](const char* sourceKey, const char* prefix) {
	        AppendHorizontalSegmentCrops(escMenuChromeCrops, sourceKey, {
	            std::string(prefix) + "LeftCrop",
	            std::string(prefix) + "RightCrop",
	            std::string(prefix) + "TopSourceCrop",
	            std::string(prefix) + "BottomSourceCrop",
	            std::string(prefix) + "UpperLeftCrop",
	            std::string(prefix) + "UpperRightCrop",
	            std::string(prefix) + "BottomLeftCrop",
	            std::string(prefix) + "BottomRightCrop",
	        });
	    };
	    appendButtonBorderCrops("EscMenuButtonBorderCrop", "EscMenuButtonBorder");
	    appendButtonBorderCrops("EscMenuButtonPushedBorderCrop", "EscMenuButtonPushedBorder");
	    appendButtonBorderCrops("EscMenuButtonDisabledBorderCrop", "EscMenuButtonDisabledBorder");
	    appendButtonBorderCrops("EscMenuButtonDisabledPushedBorderCrop", "EscMenuButtonDisabledPushedBorder");
	    appendButtonBorderCrops("EscMenuEditBoxBorderCrop", "EscMenuEditBoxBorder");
	    appendButtonBorderCrops("EscMenuSliderBorderCrop", "EscMenuSliderBorder");
	    appendButtonBorderCrops("EscMenuSliderDisabledBorderCrop", "EscMenuSliderDisabledBorder");
	    ok = packedEscMenuChrome && ok;
    packedCrops.insert(packedCrops.end(), escMenuChromeCrops.begin(), escMenuChromeCrops.end());
    if (packedEscMenuChrome) {
        const std::pair<const char*, const char*> escMenuChromeBrushes[] = {
            {"EscMenuBackgroundBrush", "EscMenuBackgroundCrop"},
            {"EscMenuBorderBrush", "EscMenuBorderCrop"},
            {"EscMenuButtonBackgroundBrush", "EscMenuButtonBackgroundCrop"},
            {"EscMenuButtonBorderBrush", "EscMenuButtonBorderCrop"},
            {"EscMenuButtonPushedBackgroundBrush", "EscMenuButtonPushedBackgroundCrop"},
            {"EscMenuButtonPushedBorderBrush", "EscMenuButtonPushedBorderCrop"},
            {"EscMenuButtonDisabledBackgroundBrush", "EscMenuButtonDisabledBackgroundCrop"},
            {"EscMenuButtonDisabledBorderBrush", "EscMenuButtonDisabledBorderCrop"},
            {"EscMenuButtonDisabledPushedBackgroundBrush", "EscMenuButtonDisabledPushedBackgroundCrop"},
            {"EscMenuButtonDisabledPushedBorderBrush", "EscMenuButtonDisabledPushedBorderCrop"},
            {"EscMenuButtonHighlightBrush", "EscMenuButtonHighlightCrop"},
            {"EscMenuSliderBackgroundBrush", "EscMenuSliderBackgroundCrop"},
            {"EscMenuSliderBorderBrush", "EscMenuSliderBorderCrop"},
            {"EscMenuSliderDisabledBorderBrush", "EscMenuSliderDisabledBorderCrop"},
            {"EscMenuSliderThumbButtonBrush", "EscMenuSliderThumbButtonCrop"},
            {"EscMenuSliderDisabledThumbButtonBrush", "EscMenuSliderDisabledThumbButtonCrop"},
            {"EscMenuCheckBoxBackgroundBrush", "EscMenuCheckBoxBackgroundCrop"},
            {"EscMenuDisabledCheckBoxBackgroundBrush", "EscMenuDisabledCheckBoxBackgroundCrop"},
            {"EscMenuCheckBoxPushedBackgroundBrush", "EscMenuCheckBoxPushedBackgroundCrop"},
            {"EscMenuCheckBoxCheckHighlightBrush", "EscMenuCheckBoxCheckHighlightCrop"},
            {"EscMenuDisabledCheckHighlightBrush", "EscMenuDisabledCheckHighlightCrop"},
            {"EscMenuRadioButtonBackgroundBrush", "EscMenuRadioButtonBackgroundCrop"},
            {"EscMenuRadioButtonDisabledBackgroundBrush", "EscMenuRadioButtonDisabledBackgroundCrop"},
            {"EscMenuRadioButtonPushedBackgroundBrush", "EscMenuRadioButtonPushedBackgroundCrop"},
            {"EscMenuRadioButtonSelectedHighlightBrush", "EscMenuRadioButtonSelectedHighlightCrop"},
            {"EscMenuDisabledRadioButtonSelectedHighlightBrush", "EscMenuDisabledRadioButtonSelectedHighlightCrop"},
            {"EscMenuEditBoxBackgroundBrush", "EscMenuEditBoxBackgroundCrop"},
            {"EscMenuEditBoxBorderBrush", "EscMenuEditBoxBorderCrop"},
            {"EscMenuPopupMenuMenuBackgroundBrush", "EscMenuPopupMenuMenuBackgroundCrop"},
            {"EscMenuPopupMenuArrowBrush", "EscMenuPopupMenuArrowCrop"},
        };
	        for (const auto& brush : escMenuChromeBrushes) {
	            const int tileViewport = std::string_view(brush.first) == "EscMenuBackgroundBrush"
	                ? FdfBackdropTileViewport(0.256f, 0.01f, 0.01f, 0.01f, 0.01f)
	                : 0;
	            brushes.push_back(BrushInfo{
                brush.first,
                std::string("{StaticResource ") + brush.second + "}",
                "EscMenuChrome atlas",
                "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png",
	                tileViewport
	            });
	        }
	        // EscMenu panel chrome is authored as square 64px slices. Keep the edge
	        // viewport square so horizontal and vertical borders use matching height.
	        const int escMenuCornerViewport = 64;
	        const BrushInfo escMenuBackdropBrushes[] = {
	            {"EscMenuBorderLeftBrush", "{StaticResource EscMenuBorderLeftCrop}", "EscMenuBorder 8-slice", "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", escMenuCornerViewport, 0},
	            {"EscMenuBorderRightBrush", "{StaticResource EscMenuBorderRightCrop}", "EscMenuBorder 8-slice", "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", escMenuCornerViewport, 0},
	            {"EscMenuBorderTopBrush", "{StaticResource EscMenuBorderTopTileCrop}", "EscMenuBorder 8-slice", "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", escMenuCornerViewport, 0},
	            {"EscMenuBorderBottomBrush", "{StaticResource EscMenuBorderBottomTileCrop}", "EscMenuBorder 8-slice", "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", escMenuCornerViewport, 0},
	            {"EscMenuBorderUpperLeftBrush", "{StaticResource EscMenuBorderUpperLeftCrop}", "EscMenuBorder 8-slice", "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", 0, 0},
	            {"EscMenuBorderUpperRightBrush", "{StaticResource EscMenuBorderUpperRightCrop}", "EscMenuBorder 8-slice", "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", 0, 0},
	            {"EscMenuBorderBottomLeftBrush", "{StaticResource EscMenuBorderBottomLeftCrop}", "EscMenuBorder 8-slice", "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", 0, 0},
	            {"EscMenuBorderBottomRightBrush", "{StaticResource EscMenuBorderBottomRightCrop}", "EscMenuBorder 8-slice", "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", 0, 0},
	        };
	        for (const BrushInfo& brush : escMenuBackdropBrushes) {
	            brushes.push_back(brush);
	        }
	        // FDF supplies crop coordinates; the Noesis 9-slice layout needs a thinner display
	        // viewport than the Java sprite edge draw, otherwise the button edge bands dominate.
	        const int escMenuButtonChromeViewport = 10;
	        const auto appendButtonBorderBrushes = [&](const char* prefix, int viewport) {
	            const std::string sourceNote = std::string(prefix) + " 8-slice";
	            brushes.push_back(BrushInfo{std::string(prefix) + "LeftBrush", std::string("{StaticResource ") + prefix + "LeftCrop}", sourceNote, "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", viewport, 0});
	            brushes.push_back(BrushInfo{std::string(prefix) + "RightBrush", std::string("{StaticResource ") + prefix + "RightCrop}", sourceNote, "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", viewport, 0});
	            brushes.push_back(BrushInfo{std::string(prefix) + "TopBrush", std::string("{StaticResource ") + prefix + "TopSourceCrop}", sourceNote, "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", viewport, 270});
	            brushes.push_back(BrushInfo{std::string(prefix) + "BottomBrush", std::string("{StaticResource ") + prefix + "BottomSourceCrop}", sourceNote, "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", viewport, 270});
	            brushes.push_back(BrushInfo{std::string(prefix) + "UpperLeftBrush", std::string("{StaticResource ") + prefix + "UpperLeftCrop}", sourceNote, "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", 0, 0});
	            brushes.push_back(BrushInfo{std::string(prefix) + "UpperRightBrush", std::string("{StaticResource ") + prefix + "UpperRightCrop}", sourceNote, "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", 0, 0});
	            brushes.push_back(BrushInfo{std::string(prefix) + "BottomLeftBrush", std::string("{StaticResource ") + prefix + "BottomLeftCrop}", sourceNote, "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", 0, 0});
	            brushes.push_back(BrushInfo{std::string(prefix) + "BottomRightBrush", std::string("{StaticResource ") + prefix + "BottomRightCrop}", sourceNote, "assets\\ui\\widgets\\escmenu\\orc\\EscMenuChrome.png", 0, 0});
	        };
	        appendButtonBorderBrushes("EscMenuButtonBorder", escMenuButtonChromeViewport);
	        appendButtonBorderBrushes("EscMenuButtonPushedBorder", escMenuButtonChromeViewport);
	        appendButtonBorderBrushes("EscMenuButtonDisabledBorder", escMenuButtonChromeViewport);
	        appendButtonBorderBrushes("EscMenuButtonDisabledPushedBorder", escMenuButtonChromeViewport);
	        appendButtonBorderBrushes("EscMenuEditBoxBorder", escMenuButtonChromeViewport);
	        appendButtonBorderBrushes("EscMenuSliderBorder", 7);
	        appendButtonBorderBrushes("EscMenuSliderDisabledBorder", 7);
	    }

    const std::vector<std::pair<std::string, std::string>> minimapButtonFields = {
        {"MiniMapSignalButtonEnabled", "MiniMapSignalButtonEnabledCrop"},
        {"MiniMapSignalButtonPushed", "MiniMapSignalButtonPushedCrop"},
        {"MiniMapSignalButtonDisabled", "MiniMapSignalButtonDisabledCrop"},
        {"MiniMapTerrainButtonActiveEnabled", "MiniMapTerrainButtonActiveEnabledCrop"},
        {"MiniMapTerrainButtonActivePushed", "MiniMapTerrainButtonActivePushedCrop"},
        {"MiniMapTerrainButtonActiveDisabled", "MiniMapTerrainButtonActiveDisabledCrop"},
        {"MiniMapTerrainButtonInactiveEnabled", "MiniMapTerrainButtonInactiveEnabledCrop"},
        {"MiniMapTerrainButtonInactivePushed", "MiniMapTerrainButtonInactivePushedCrop"},
        {"MiniMapTerrainButtonInactiveDisabled", "MiniMapTerrainButtonInactiveDisabledCrop"},
        {"MiniMapAllyButtonActiveEnabled", "MiniMapAllyButtonActiveEnabledCrop"},
        {"MiniMapAllyButtonActivePushed", "MiniMapAllyButtonActivePushedCrop"},
        {"MiniMapAllyButtonActiveDisabled", "MiniMapAllyButtonActiveDisabledCrop"},
        {"MiniMapAllyButtonInactiveEnabled", "MiniMapAllyButtonInactiveEnabledCrop"},
        {"MiniMapAllyButtonInactivePushed", "MiniMapAllyButtonInactivePushedCrop"},
        {"MiniMapAllyButtonInactiveDisabled", "MiniMapAllyButtonInactiveDisabledCrop"},
        {"MiniMapAllyButtonOffEnabled", "MiniMapAllyButtonOffEnabledCrop"},
        {"MiniMapAllyButtonOffPushed", "MiniMapAllyButtonOffPushedCrop"},
        {"MiniMapAllyButtonOffDisabled", "MiniMapAllyButtonOffDisabledCrop"},
        {"MiniMapCreepButtonActiveEnabled", "MiniMapCreepButtonActiveEnabledCrop"},
        {"MiniMapCreepButtonActivePushed", "MiniMapCreepButtonActivePushedCrop"},
        {"MiniMapCreepButtonActiveDisabled", "MiniMapCreepButtonActiveDisabledCrop"},
        {"MiniMapCreepButtonInactiveEnabled", "MiniMapCreepButtonInactiveEnabledCrop"},
        {"MiniMapCreepButtonInactivePushed", "MiniMapCreepButtonInactivePushedCrop"},
        {"MiniMapCreepButtonInactiveDisabled", "MiniMapCreepButtonInactiveDisabledCrop"},
        {"MiniMapFormationOnButtonEnabled", "MiniMapFormationOnButtonEnabledCrop"},
        {"MiniMapFormationOnButtonEnabledPushed", "MiniMapFormationOnButtonPushedCrop"},
        {"MiniMapFormationOnButtonDisabled", "MiniMapFormationOnButtonDisabledCrop"},
        {"MiniMapFormationOffButtonEnabled", "MiniMapFormationOffButtonEnabledCrop"},
        {"MiniMapFormationOffButtonEnabledPushed", "MiniMapFormationOffButtonPushedCrop"},
        {"MiniMapFormationOffButtonDisabled", "MiniMapFormationOffButtonDisabledCrop"},
    };
    ok = ExtractSkinFieldsToAtlas(mpq, minimapButtonFields, "Default",
                                  projectRoot / "assets" / "ui" / "widgets" / "console" / "human" / "MinimapButtons.png",
                                  "widgets/console/human/MinimapButtons.png", packedCrops) && ok;

    const std::vector<std::pair<std::string, std::string>> infoPanelIconFields = {
        {"InfoPanelIconHeroIconSTR", "HeroStrengthIconCrop"},
        {"InfoPanelIconHeroIconAGI", "HeroAgilityIconCrop"},
        {"InfoPanelIconHeroIconINT", "HeroIntelligenceIconCrop"},
        {"InfoPanelIconDamageUnknown", "InfoPanelIconDamageUnknownCrop"},
        {"InfoPanelIconDamageNormal", "InfoPanelIconDamageNormalCrop"},
        {"InfoPanelIconDamageMelee", "InfoPanelIconDamageMeleeCrop"},
        {"InfoPanelIconDamagePierce", "InfoPanelIconDamagePierceCrop"},
        {"InfoPanelIconDamageSiege", "InfoPanelIconDamageSiegeCrop"},
        {"InfoPanelIconDamageMagic", "InfoPanelIconDamageMagicCrop"},
        {"InfoPanelIconDamageChaos", "InfoPanelIconDamageChaosCrop"},
        {"InfoPanelIconDamageHero", "InfoPanelIconDamageHeroCrop"},
        {"InfoPanelIconDamageUnknownNeutral", "InfoPanelIconDamageUnknownNeutralCrop"},
        {"InfoPanelIconDamageNormalNeutral", "InfoPanelIconDamageNormalNeutralCrop"},
        {"InfoPanelIconDamageMeleeNeutral", "InfoPanelIconDamageMeleeNeutralCrop"},
        {"InfoPanelIconDamagePierceNeutral", "InfoPanelIconDamagePierceNeutralCrop"},
        {"InfoPanelIconDamageSiegeNeutral", "InfoPanelIconDamageSiegeNeutralCrop"},
        {"InfoPanelIconDamageMagicNeutral", "InfoPanelIconDamageMagicNeutralCrop"},
        {"InfoPanelIconDamageChaosNeutral", "InfoPanelIconDamageChaosNeutralCrop"},
        {"InfoPanelIconDamageHeroNeutral", "InfoPanelIconDamageHeroNeutralCrop"},
        {"InfoPanelIconArmorUnknown", "InfoPanelIconArmorUnknownCrop"},
        {"InfoPanelIconArmorSmall", "InfoPanelIconArmorSmallCrop"},
        {"InfoPanelIconArmorMedium", "InfoPanelIconArmorMediumCrop"},
        {"InfoPanelIconArmorLarge", "InfoPanelIconArmorLargeCrop"},
        {"InfoPanelIconArmorFort", "InfoPanelIconArmorFortCrop"},
        {"InfoPanelIconArmorNormal", "InfoPanelIconArmorNormalCrop"},
        {"InfoPanelIconArmorHero", "InfoPanelIconArmorHeroCrop"},
        {"InfoPanelIconArmorDivine", "InfoPanelIconArmorDivineCrop"},
        {"InfoPanelIconArmorNone", "InfoPanelIconArmorNoneCrop"},
        {"InfoPanelIconArmorUnknownNeutral", "InfoPanelIconArmorUnknownNeutralCrop"},
        {"InfoPanelIconArmorSmallNeutral", "InfoPanelIconArmorSmallNeutralCrop"},
        {"InfoPanelIconArmorMediumNeutral", "InfoPanelIconArmorMediumNeutralCrop"},
        {"InfoPanelIconArmorLargeNeutral", "InfoPanelIconArmorLargeNeutralCrop"},
        {"InfoPanelIconArmorFortNeutral", "InfoPanelIconArmorFortNeutralCrop"},
        {"InfoPanelIconArmorNormalNeutral", "InfoPanelIconArmorNormalNeutralCrop"},
        {"InfoPanelIconArmorHeroNeutral", "InfoPanelIconArmorHeroNeutralCrop"},
        {"InfoPanelIconArmorDivineNeutral", "InfoPanelIconArmorDivineNeutralCrop"},
        {"InfoPanelIconArmorNoneNeutral", "InfoPanelIconArmorNoneNeutralCrop"},
        {"InfoPanelIconGold", "InfoPanelIconGoldCrop"},
        {"InfoPanelIconFood", "InfoPanelIconFoodCrop"},
    };
    ok = ExtractSkinFieldsToAtlas(mpq, infoPanelIconFields, "Human",
                                  projectRoot / "assets" / "ui" / "widgets" / "console" / "human" / "InfoPanelIcons.png",
                                  "widgets/console/human/InfoPanelIcons.png", packedCrops) && ok;

    struct ExplicitBlpAsset {
        const char* sourcePath;
        const char* outputPath;
        const char* brushKey;
        const char* note;
    };

    const ExplicitBlpAsset explicitBlps[] = {
        {"UI\\Widgets\\Console\\Human\\CommandButton\\human-multipleselection-border.blp",
         "assets\\ui\\widgets\\console\\human\\MultiSelectionBorder.png",
         "MultiSelectionBorderBrush", "MultiSelectionBorder direct texture"},
        {"UI\\Feedback\\Autocast\\GlowStar.blp", "assets\\ui\\widgets\\feedback\\autocast\\GlowStar.png",
         "CommandButtonAutocastBrush", "CommandButtonAutocast overlay texture"},
        {"UI\\Feedback\\Cooldown\\cooldown.blp", "assets\\ui\\widgets\\feedback\\cooldown\\CommandButtonCooldown.png",
         "CommandButtonCooldownBrush", "CommandButtonCooldown texture from UI-Cooldown-Indicator model"},
        {"Textures\\Black32.blp", "assets\\ui\\common\\Black32.png",
         "Black32Brush", "Black32 shared UI backing texture"},
        {"ReplaceableTextures\\TeamColor\\TeamColor06.blp", "assets\\ui\\widgets\\feedback\\teamcolor\\TeamColor06.png",
         "TeamColor06Brush", "inventory placeholder team color texture"},
    };

    const ExplicitBlpAsset glueBlps[] = {
        {"UI\\Glues\\MainMenu\\WarCraftIIILogo_exp\\War3xLogo.blp", "assets\\ui\\glues\\mainmenu\\War3xLogo.png",
         "War3xLogoBrush", "Frozen Throne main menu logo texture"},
        {"UI\\Glues\\MainMenu\\MainMenu3d_exp\\SKY2.blp", "assets\\ui\\glues\\mainmenu\\Sky.png",
         "MainMenuSkyBrush", "Frozen Throne main menu sky texture"},
        {"UI\\Glues\\MainMenu\\MainMenu3d_exp\\Clouds8x8Fade.blp", "assets\\ui\\glues\\mainmenu\\CloudsFade.png",
         "MainMenuCloudsFadeBrush", "Frozen Throne main menu cloud texture"},
        {"UI\\Glues\\MainMenu\\MainMenu3d_exp\\Iceberg_New.blp", "assets\\ui\\glues\\mainmenu\\Iceberg.png",
         "MainMenuIcebergBrush", "Frozen Throne main menu iceberg texture"},
        {"UI\\Glues\\MainMenu\\MainMenu3d_exp\\Ice_Icedull.blp", "assets\\ui\\glues\\mainmenu\\IceDull.png",
         "MainMenuIceDullBrush", "Frozen Throne main menu ice texture"},
        {"UI\\Glues\\MainMenu\\MainMenu3d_exp\\Blue_Glow2.blp", "assets\\ui\\glues\\mainmenu\\BlueGlow.png",
         "MainMenuBlueGlowBrush", "Frozen Throne main menu glow texture"},
        {"UI\\Glues\\MainMenu\\MainMenu3d_exp\\FrostWyrm.blp", "assets\\ui\\glues\\mainmenu\\FrostWyrm.png",
         "MainMenuFrostWyrmBrush", "Frozen Throne MainMenu3D_exp model frost wyrm texture"},
        {"UI\\Glues\\MainMenu\\MainMenu3d_exp\\SnowyRocksdull2.blp", "assets\\ui\\glues\\mainmenu\\SnowyRocksDull2.png",
         "MainMenuSnowyRocksDull2Brush", "Frozen Throne MainMenu3D_exp model snowy rocks texture"},
        {"UI\\Glues\\MainMenu\\MainMenu3d_exp\\Ice_Natural01.blp", "assets\\ui\\glues\\mainmenu\\IceNatural01.png",
         "MainMenuIceNatural01Brush", "Frozen Throne MainMenu3D_exp model natural ice texture"},
        {"UI\\Glues\\MainMenu\\MainMenu3d_exp\\Ice_Ice.blp", "assets\\ui\\glues\\mainmenu\\IceIce.png",
         "MainMenuIceIceBrush", "Frozen Throne MainMenu3D_exp model ice texture"},
        {"UI\\Glues\\MainMenu\\MainMenu3d_exp\\Chains_silver.blp", "assets\\ui\\glues\\mainmenu\\ChainsSilver.png",
         "MainMenuChainsSilverBrush", "Frozen Throne MainMenu3D_exp model chain texture"},
        {"UI\\Glues\\MainMenu\\MainMenu3d_exp\\ocean_h.01.blp", "assets\\ui\\glues\\mainmenu\\OceanH01.png",
         "MainMenuOceanH01Brush", "Frozen Throne MainMenu3D_exp model ocean texture"},
        {"UI\\Glues\\MainMenu\\MainMenu3d_exp\\ocean_h.10.blp", "assets\\ui\\glues\\mainmenu\\OceanH10.png",
         "MainMenuOceanH10Brush", "Frozen Throne MainMenu3D_exp model ocean texture"},
        {"UI\\Glues\\MainMenu\\MainMenu3d_exp\\star32.blp", "assets\\ui\\glues\\mainmenu\\Star32.png",
         "MainMenuStar32Brush", "Frozen Throne MainMenu3D_exp model star texture"},
        {"UI\\Glues\\MainMenu\\MainMenu3d_exp\\snowflake.blp", "assets\\ui\\glues\\mainmenu\\Snowflake.png",
         "MainMenuSnowflakeBrush", "Frozen Throne MainMenu3D_exp model snowflake texture"},
        {"UI\\Glues\\MainMenu\\MainMenu3d_exp\\lensflare1Ax.blp", "assets\\ui\\glues\\mainmenu\\LensFlare1Ax.png",
         "MainMenuLensFlare1AxBrush", "Frozen Throne MainMenu3D_exp model lens flare texture"},
        {"UI\\Glues\\SinglePlayer\\NightElf_Exp\\SkyX.blp", "assets\\ui\\glues\\campaign\\NightElfSkyX.png",
         "CampaignNightElfSkyBrush", "Frozen Throne NightElfBackdrop_V1 sky texture"},
        {"UI\\Glues\\SinglePlayer\\NightElf_Exp\\WaterfallX.blp", "assets\\ui\\glues\\campaign\\NightElfWaterfallX.png",
         "CampaignNightElfWaterfallBrush", "Frozen Throne NightElfBackdrop_V1 waterfall texture"},
        {"UI\\Glues\\SinglePlayer\\Alliance_Exp\\outland_sky_menu.blp", "assets\\ui\\glues\\campaign\\HumanOutlandSky.png",
         "CampaignHumanSkyBrush", "Frozen Throne HumanBackdrop_V1 sky texture"},
        {"UI\\Glues\\SinglePlayer\\Alliance_Exp\\OutlandGround2B.blp", "assets\\ui\\glues\\campaign\\HumanOutlandGround.png",
         "CampaignHumanGroundBrush", "Frozen Throne HumanBackdrop_V1 ground texture"},
        {"UI\\Glues\\SinglePlayer\\Alliance_Exp\\BloodElf-old.blp", "assets\\ui\\glues\\campaign\\HumanBloodElf.png",
         "CampaignHumanBloodElfBrush", "Frozen Throne HumanBackdrop_V1 blood elf texture"},
        {"UI\\Glues\\SinglePlayer\\Undead3D_Exp\\skygrey32.blp", "assets\\ui\\glues\\campaign\\UndeadSkyGrey.png",
         "CampaignUndeadSkyBrush", "Frozen Throne UndeadBackdrop_V1 sky texture"},
        {"UI\\Glues\\SinglePlayer\\Undead3D_Exp\\icecrown_ground1.blp", "assets\\ui\\glues\\campaign\\UndeadIcecrownGround.png",
         "CampaignUndeadGroundBrush", "Frozen Throne UndeadBackdrop_V1 ground texture"},
        {"UI\\Glues\\SinglePlayer\\Undead3D_Exp\\ArthasCloakX.blp", "assets\\ui\\glues\\campaign\\UndeadArthasCloak.png",
         "CampaignUndeadArthasCloakBrush", "Frozen Throne UndeadBackdrop_V1 Arthas cloak texture"},
        {"UI\\Glues\\SinglePlayer\\Orc_Exp\\SKY1b.blp", "assets\\ui\\glues\\campaign\\OrcSky.png",
         "CampaignOrcSkyBrush", "Frozen Throne OrcBackdrop_V1 sky texture"},
        {"UI\\Glues\\SinglePlayer\\Orc_Exp\\orc_ground6.blp", "assets\\ui\\glues\\campaign\\OrcGround.png",
         "CampaignOrcGroundBrush", "Frozen Throne OrcBackdrop_V1 ground texture"},
        {"UI\\Glues\\SinglePlayer\\Orc_Exp\\Rexxar.blp", "assets\\ui\\glues\\campaign\\OrcRexxar.png",
         "CampaignOrcRexxarBrush", "Frozen Throne OrcBackdrop_V1 Rexxar texture"},
        {"UI\\Glues\\SpriteLayers\\Expansion\\GlueScreen-Panel-MetalBorder-Expansion.blp", "assets\\ui\\glues\\mainmenu\\PanelMetalBorderExpansion.png",
         "GluePanelMetalBorderExpansionBrush", "Frozen Throne glue panel expansion metal border"},
        {"UI\\Glues\\SpriteLayers\\GlueScreen-Panel-MetalBorder.blp", "assets\\ui\\glues\\mainmenu\\PanelMetalBorder.png",
         "GluePanelMetalBorderBrush", "Frozen Throne glue panel metal border"},
        {"UI\\Glues\\SpriteLayers\\Expansion\\GlueScreen-Panel-MetalPieces-Expansion.blp", "assets\\ui\\glues\\mainmenu\\PanelMetalPiecesExpansion.png",
         "GluePanelMetalPiecesExpansionBrush", "Frozen Throne glue panel expansion metal pieces"},
        {"UI\\Glues\\SpriteLayers\\GlueScreen-Panel-MetalPieces.blp", "assets\\ui\\glues\\mainmenu\\PanelMetalPieces.png",
         "GluePanelMetalPiecesBrush", "Frozen Throne glue panel metal pieces"},
        {"UI\\Glues\\SpriteLayers\\Expansion\\GlueScreen-Panel-ExpansionDetail.blp", "assets\\ui\\glues\\mainmenu\\PanelExpansionDetail.png",
         "GluePanelExpansionDetailBrush", "Frozen Throne glue panel expansion detail"},
        {"UI\\Glues\\SpriteLayers\\Expansion\\GlueScreen-Panel-GearRail.blp", "assets\\ui\\glues\\mainmenu\\PanelGearRail.png",
         "GluePanelGearRailBrush", "Frozen Throne glue panel gear rail"},
        {"UI\\Glues\\SpriteLayers\\Expansion\\GlueScreen-Panel-Rail.blp", "assets\\ui\\glues\\mainmenu\\PanelRail.png",
         "GluePanelRailBrush", "Frozen Throne glue panel rail"},
        {"UI\\Glues\\SpriteLayers\\Expansion\\GlueScreen-Panel-Rail00.blp", "assets\\ui\\glues\\mainmenu\\PanelRail00.png",
         "GluePanelRail00Brush", "Frozen Throne glue panel rail cap"},
        {"UI\\Glues\\BattleNet\\BattlenetChatGlue\\Expansion\\CenterPanel01-Expansion.blp", "assets\\ui\\glues\\centerpanel\\CenterPanel01Expansion.png",
         "GlueCenterPanel01ExpansionBrush", "Frozen Throne GlueSpriteLayerCenter expansion panel texture"},
        {"UI\\Glues\\BattleNet\\BattlenetChatGlue\\Expansion\\CenterPanel02-Expansion.blp", "assets\\ui\\glues\\centerpanel\\CenterPanel02Expansion.png",
         "GlueCenterPanel02ExpansionBrush", "Frozen Throne GlueSpriteLayerCenter expansion panel texture"},
        {"UI\\Glues\\BattleNet\\BattlenetChatGlue\\Expansion\\CenterPanel03-Expansion.blp", "assets\\ui\\glues\\centerpanel\\CenterPanel03Expansion.png",
         "GlueCenterPanel03ExpansionBrush", "Frozen Throne GlueSpriteLayerCenter expansion panel texture"},
        {"UI\\Glues\\BattleNet\\BattlenetChatGlue\\Expansion\\CenterPanel04-Expansion.blp", "assets\\ui\\glues\\centerpanel\\CenterPanel04Expansion.png",
         "GlueCenterPanel04ExpansionBrush", "Frozen Throne GlueSpriteLayerCenter expansion side texture"},
        {"Textures\\GlueScreen-Profile-EndCaps.blp", "assets\\ui\\glues\\spritelayers\\ProfileEndCaps.png",
         "GlueProfileEndCapsBrush", "Frozen Throne GlueSpriteLayerTopLeft expansion profile end caps texture"},
        {"Textures\\GlueScreen-Profile-Stretch.blp", "assets\\ui\\glues\\spritelayers\\ProfileStretch.png",
         "GlueProfileStretchBrush", "Frozen Throne GlueSpriteLayerTopLeft expansion profile stretch texture"},
        {"Textures\\GlueScreen-Profile-Stretch2.blp", "assets\\ui\\glues\\spritelayers\\ProfileStretch2.png",
         "GlueProfileStretch2Brush", "Frozen Throne GlueSpriteLayerTopLeft expansion profile lower stretch texture"},
        {"Textures\\GlueScreen-Panel-BNetChannel.blp", "assets\\ui\\glues\\spritelayers\\PanelBNetChannel.png",
         "GluePanelBNetChannelBrush", "Frozen Throne GlueSpriteLayerTopLeft/TopRight expansion channel texture"},
        {"ReplaceableTextures\\TeamColor\\TeamColor00.blp", "assets\\ui\\glues\\teamcolor\\TeamColor00.png",
         "GlueTeamColor00Brush", "Frozen Throne custom game slot color texture"},
        {"ReplaceableTextures\\TeamColor\\TeamColor01.blp", "assets\\ui\\glues\\teamcolor\\TeamColor01.png",
         "GlueTeamColor01Brush", "Frozen Throne custom game slot color texture"},
        {"ReplaceableTextures\\TeamColor\\TeamColor02.blp", "assets\\ui\\glues\\teamcolor\\TeamColor02.png",
         "GlueTeamColor02Brush", "Frozen Throne custom game slot color texture"},
        {"ReplaceableTextures\\TeamColor\\TeamColor03.blp", "assets\\ui\\glues\\teamcolor\\TeamColor03.png",
         "GlueTeamColor03Brush", "Frozen Throne custom game slot color texture"},
        {"ReplaceableTextures\\TeamColor\\TeamColor04.blp", "assets\\ui\\glues\\teamcolor\\TeamColor04.png",
         "GlueTeamColor04Brush", "Frozen Throne custom game slot color texture"},
        {"ReplaceableTextures\\TeamColor\\TeamColor05.blp", "assets\\ui\\glues\\teamcolor\\TeamColor05.png",
         "GlueTeamColor05Brush", "Frozen Throne custom game slot color texture"},
        {"ReplaceableTextures\\TeamColor\\TeamColor06.blp", "assets\\ui\\glues\\teamcolor\\TeamColor06.png",
         "GlueTeamColor06Brush", "Frozen Throne custom game slot color texture"},
        {"ReplaceableTextures\\TeamColor\\TeamColor07.blp", "assets\\ui\\glues\\teamcolor\\TeamColor07.png",
         "GlueTeamColor07Brush", "Frozen Throne custom game slot color texture"},
        {"ReplaceableTextures\\TeamColor\\TeamColor08.blp", "assets\\ui\\glues\\teamcolor\\TeamColor08.png",
         "GlueTeamColor08Brush", "Frozen Throne custom game slot color texture"},
        {"ReplaceableTextures\\TeamColor\\TeamColor09.blp", "assets\\ui\\glues\\teamcolor\\TeamColor09.png",
         "GlueTeamColor09Brush", "Frozen Throne custom game slot color texture"},
        {"ReplaceableTextures\\TeamColor\\TeamColor10.blp", "assets\\ui\\glues\\teamcolor\\TeamColor10.png",
         "GlueTeamColor10Brush", "Frozen Throne custom game slot color texture"},
        {"ReplaceableTextures\\TeamColor\\TeamColor11.blp", "assets\\ui\\glues\\teamcolor\\TeamColor11.png",
         "GlueTeamColor11Brush", "Frozen Throne custom game slot color texture"},
        {"UI\\Widgets\\Glues\\GlueScreen-Button1-Border.blp", "assets\\ui\\glues\\widgets\\Button1Border.png",
         "GlueButton1BorderBrush", "Frozen Throne glue menu button outer border"},
        {"UI\\Widgets\\Glues\\GlueScreen-Button1-BorderSingle.blp", "assets\\ui\\glues\\widgets\\Button1BorderSingle.png",
         "GlueButton1BorderSingleBrush", "Frozen Throne glue menu single button outer border"},
        {"UI\\Widgets\\Glues\\GlueScreen-Button1-SmallBorderSingle.blp", "assets\\ui\\glues\\widgets\\Button1SmallBorderSingle.png",
         "GlueButton1SmallBorderSingleBrush", "Frozen Throne glue menu small single button outer border"},
        {"UI\\Widgets\\Glues\\GlueScreen-Button1-LargeBorderSingle.blp", "assets\\ui\\glues\\widgets\\Button1LargeBorderSingle.png",
         "GlueButton1LargeBorderSingleBrush", "Frozen Throne glue menu large single button outer border"},
        {"UI\\Widgets\\Glues\\GlueScreen-Button1-BackdropBackground.blp", "assets\\ui\\glues\\widgets\\Button1BackdropBackground.png",
         "GlueButton1BackdropBackgroundBrush", "Frozen Throne glue menu button background"},
        {"UI\\Widgets\\Glues\\GlueScreen-Button1-BackdropBackground-Down.blp", "assets\\ui\\glues\\widgets\\Button1BackdropBackgroundDown.png",
         "GlueButton1BackdropBackgroundDownBrush", "Frozen Throne glue menu button pressed background"},
        {"UI\\Widgets\\Glues\\GlueScreen-Button1-BackdropBorder.blp", "assets\\ui\\glues\\widgets\\Button1BackdropBorder.png",
         "GlueButton1BackdropBorderBrush", "Frozen Throne glue menu button inner border"},
        {"UI\\Widgets\\Glues\\GlueScreen-Button1-BackdropBorder-Down.blp", "assets\\ui\\glues\\widgets\\Button1BackdropBorderDown.png",
         "GlueButton1BackdropBorderDownBrush", "Frozen Throne glue menu button pressed border"},
        {"UI\\Widgets\\Glues\\GlueScreen-Button1-BackdropBackground-Disabled.blp", "assets\\ui\\glues\\widgets\\Button1BackdropBackgroundDisabled.png",
         "GlueButton1BackdropBackgroundDisabledBrush", "Frozen Throne glue menu disabled button background"},
        {"UI\\Widgets\\Glues\\GlueScreen-Button1-BackdropBorder-Disabled.blp", "assets\\ui\\glues\\widgets\\Button1BackdropBorderDisabled.png",
         "GlueButton1BackdropBorderDisabledBrush", "Frozen Throne glue menu disabled button border"},
        {"UI\\Widgets\\Glues\\GlueScreen-Button-KeyboardHighlight.blp", "assets\\ui\\glues\\widgets\\ButtonKeyboardHighlight.png",
         "GlueButtonKeyboardHighlightBrush", "Frozen Throne glue menu keyboard/hover highlight"},
        {"UI\\Widgets\\Glues\\GlueScreen-Button1-KeyHighlight.blp", "assets\\ui\\glues\\widgets\\Button1KeyHighlight.png",
         "GlueButton1KeyHighlightBrush", "Frozen Throne glue menu button key highlight"},
    };

    for (const ExplicitBlpAsset& asset : explicitBlps) {
        const fs::path outputPath = projectRoot / asset.outputPath;
        const bool extracted = ExtractPathToPng(mpq, asset.sourcePath, outputPath);
        if (extracted) {
            std::string imageSource = asset.outputPath;
            const std::string prefix = "assets\\ui\\";
            if (imageSource.rfind(prefix, 0) == 0) {
                imageSource = imageSource.substr(prefix.size());
            }
            std::replace(imageSource.begin(), imageSource.end(), '\\', '/');
            brushes.push_back(BrushInfo{asset.brushKey, imageSource, asset.note, asset.sourcePath});
        }
        ok = extracted && ok;
    }
    ok = WriteUiResourceXaml(mpq, projectRoot / "assets" / "ui" / "War3UiResources.xaml", atlases, packedCrops, brushes) && ok;

    std::vector<BrushInfo> glueBrushes;
    for (const ExplicitBlpAsset& asset : glueBlps) {
        const fs::path outputPath = projectRoot / asset.outputPath;
        const bool extracted = ExtractPathToPng(mpq, asset.sourcePath, outputPath);
        if (extracted) {
            std::string imageSource = asset.outputPath;
            const std::string prefix = "assets\\ui\\";
            if (imageSource.rfind(prefix, 0) == 0) {
                imageSource = imageSource.substr(prefix.size());
            }
            std::replace(imageSource.begin(), imageSource.end(), '\\', '/');
            glueBrushes.push_back(BrushInfo{asset.brushKey, imageSource, asset.note, asset.sourcePath});
        }
        ok = extracted && ok;
    }

    const fs::path glueResourcePath = projectRoot / "assets" / "ui" / "GlueMenuResources.xaml";
    {
        std::ofstream out(glueResourcePath, std::ios::binary);
        if (!out) {
            std::cerr << "write failed: " << glueResourcePath.string() << std::endl;
            ok = false;
        } else {
            out << "<ResourceDictionary\n"
                << "  xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\"\n"
                << "  xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">\n\n";
            for (const BrushInfo& brush : glueBrushes) {
                out << "  <!-- war3skins " << brush.skinField << "=" << brush.sourcePath << " -->\n"
                    << "  <ImageBrush x:Key=\"" << brush.key << "\" ImageSource=\"" << brush.imageSource
                    << "\" Stretch=\"Fill\"/>\n";
            }
            out << "</ResourceDictionary>\n";
            std::cout << "generated: " << glueResourcePath.string() << std::endl;
        }
    }
    return ok ? 0 : 1;
}
