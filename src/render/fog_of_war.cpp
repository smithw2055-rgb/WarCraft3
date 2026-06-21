#include "fog_of_war.h"
#include "renderer.h"
#include <cstring>

FogOfWar::FogOfWar() = default;

FogOfWar::~FogOfWar() {
    delete[] data_;
    if (!Renderer::IsActive()) return;
    if (texture_.id && sg_query_image_state(texture_) == SG_RESOURCESTATE_VALID)
        sg_destroy_image(texture_);
    if (sampler_.id && sg_query_sampler_state(sampler_) == SG_RESOURCESTATE_VALID)
        sg_destroy_sampler(sampler_);
}

void FogOfWar::Init(int width, int height) {
    width_ = width;
    height_ = height;
    data_ = new unsigned char[width * height]();

    sg_image_desc imgDesc = {};
    imgDesc.type = SG_IMAGETYPE_2D;
    imgDesc.usage.immutable = false;
    imgDesc.usage.dynamic_update = true;
    imgDesc.width = width;
    imgDesc.height = height;
    imgDesc.pixel_format = SG_PIXELFORMAT_R8;
    imgDesc.num_mipmaps = 1;
    imgDesc.data.mip_levels[0].size = 0;
    imgDesc.data.mip_levels[0].ptr = nullptr;
    texture_ = sg_make_image(&imgDesc);

    sg_sampler_desc smpDesc = {};
    smpDesc.min_filter = SG_FILTER_LINEAR;
    smpDesc.mag_filter = SG_FILTER_LINEAR;
    smpDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smpDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    sampler_ = sg_make_sampler(&smpDesc);
}

void FogOfWar::SetData(const unsigned char* data) {
    std::memcpy(data_, data, width_ * height_);
}

void FogOfWar::ReloadToGPU() {
    sg_image_data imgData = {};
    imgData.mip_levels[0].ptr = data_;
    imgData.mip_levels[0].size = (size_t)(width_ * height_);
    sg_update_image(texture_, &imgData);
}
