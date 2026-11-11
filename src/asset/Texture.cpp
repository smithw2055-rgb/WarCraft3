#include "Texture.h"
#include "BLP.h"
#include "MipChain.h"
#include "render/rhi_device.h"
#include "stb/stb_image.h"
#include <cstring>
#include <iostream>

Texture::Texture() : image_({0}), sampler_({0}) {}

Texture::~Texture() { Release(); }

Texture::Texture(Texture&& other) noexcept
    : image_(other.image_), sampler_(other.sampler_),
      width_(other.width_), height_(other.height_) {
    other.image_ = {0};
    other.sampler_ = {0};
    other.width_ = 0;
    other.height_ = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        Release();
        image_ = other.image_;
        sampler_ = other.sampler_;
        width_ = other.width_;
        height_ = other.height_;
        other.image_ = {0};
        other.sampler_ = {0};
        other.width_ = 0;
        other.height_ = 0;
    }
    return *this;
}

bool Texture::LoadFromFile(const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        std::cerr << "Texture: cannot open " << path << std::endl;
        return false;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) { fclose(fp); return false; }

    std::vector<uint8_t> buf(static_cast<size_t>(size));
    if (fread(buf.data(), 1, size_t(size), fp) != size_t(size)) {
        fclose(fp);
        return false;
    }
    fclose(fp);

    return LoadFromMemory(buf.data(), buf.size());
}

bool Texture::LoadFromMemory(const uint8_t* data, size_t size) {
    if (size < 4) return false;

    if (std::memcmp(data, "BLP1", 4) == 0 || std::memcmp(data, "BLP2", 4) == 0) {
        // Embedded mips first (retail quality); generated tail fills gaps.
        std::vector<BLPImage> fileLevels;
        if (!DecodeBLPLevels(data, size, fileLevels) || fileLevels.empty()) {
            std::cerr << "Texture: BLP decode failed" << std::endl;
            return false;
        }
        std::vector<MipLevel> embedded;
        embedded.reserve(fileLevels.size() > 0 ? fileLevels.size() - 1 : 0);
        for (size_t i = 1; i < fileLevels.size(); ++i) {
            MipLevel level;
            level.width = fileLevels[i].width;
            level.height = fileLevels[i].height;
            level.pixels = std::move(fileLevels[i].pixels);
            embedded.push_back(std::move(level));
        }
        MipChain chain = BuildMipChain(fileLevels[0].width, fileLevels[0].height,
                                       fileLevels[0].pixels.data(), std::move(embedded));
        sg_image chained = MakeChainedImage(chain);
        if (sg_query_image_state(chained) != SG_RESOURCESTATE_VALID) {
            std::cerr << "Texture: chained BLP upload failed ("
                      << chain.levels.size() << " levels)" << std::endl;
            return false;
        }
        Release();
        image_ = chained;
        width_ = chain.levels[0].width;
        height_ = chain.levels[0].height;

        sg_sampler_desc smp_desc = {};
        smp_desc.min_filter = SG_FILTER_LINEAR;
        smp_desc.mag_filter = SG_FILTER_LINEAR;
        smp_desc.mipmap_filter = SG_FILTER_LINEAR;
        smp_desc.wrap_u = SG_WRAP_REPEAT;
        smp_desc.wrap_v = SG_WRAP_REPEAT;
        sampler_ = sg_make_sampler(&smp_desc);
        return true;
    }

    int w, h, channels;
    stbi_uc* pixels = stbi_load_from_memory(data, int(size), &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) {
        std::cerr << "Texture: stb_image failed: " << stbi_failure_reason() << std::endl;
        return false;
    }
    bool ok = CreateFromImage(w, h, 4, pixels);
    stbi_image_free(pixels);
    return ok;
}

bool Texture::CreateFromImage(int width, int height, int channels, const uint8_t* pixels) {
    if (!pixels || width <= 0 || height <= 0) return false;
    return Create(width, height, pixels);
}

bool Texture::Create(int width, int height, const uint8_t* pixels) {
    Release();

    // Full generated chain: the sampler is mipmapped, so a bare level 0
    // would leave the texture incomplete (black on strict drivers).
    MipChain chain = BuildMipChain(width, height, pixels);
    image_ = MakeChainedImage(chain);

    if (sg_query_image_state(image_) != SG_RESOURCESTATE_VALID) {
        std::cerr << "Texture: sg_make_image failed (" << width << "x" << height << ")" << std::endl;
        return false;
    }

    sg_sampler_desc smp_desc = {};
    smp_desc.min_filter = SG_FILTER_LINEAR;
    smp_desc.mag_filter = SG_FILTER_LINEAR;
    smp_desc.mipmap_filter = SG_FILTER_LINEAR;
    smp_desc.wrap_u = SG_WRAP_REPEAT;
    smp_desc.wrap_v = SG_WRAP_REPEAT;
    sampler_ = sg_make_sampler(&smp_desc);

    width_ = width;
    height_ = height;
    return true;
}

void Texture::Bind(int /*unit*/) const {}
void Texture::Unbind(int /*unit*/) {}

void Texture::Release() {
    bool active = Renderer::IsActive();
    if (active && sg_query_image_state(image_) == SG_RESOURCESTATE_VALID)
        sg_destroy_image(image_);
    if (active && sg_query_sampler_state(sampler_) == SG_RESOURCESTATE_VALID)
        sg_destroy_sampler(sampler_);
    image_ = {0};
    sampler_ = {0};
    width_ = 0;
    height_ = 0;
}
