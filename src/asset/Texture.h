#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include <cstdint>
#include <vector>
#include <string>

class Texture {
public:
    Texture();
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    bool LoadFromFile(const std::string& path);
    bool LoadFromMemory(const uint8_t* data, size_t size);
    bool Create(int width, int height, const uint8_t* pixels);
    bool CreateFromImage(int width, int height, int channels, const uint8_t* pixels);

    void Bind(int unit = 0) const;
    static void Unbind(int unit = 0);

    int Width() const { return width_; }
    int Height() const { return height_; }
    bool IsValid() const { return sg_query_image_state(image_) == SG_RESOURCESTATE_VALID; }

    sg_image GetImage() const { return image_; }
    sg_sampler GetSampler() const { return sampler_; }

private:
    void Release();

    sg_image image_;
    sg_sampler sampler_;
    int width_ = 0;
    int height_ = 0;
};
