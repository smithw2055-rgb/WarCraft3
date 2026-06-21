#include "shadow_manager.h"
#include "renderer.h"
#include <cstring>
#include <iostream>

DynamicShadowManager::DynamicShadowManager() = default;

DynamicShadowManager::~DynamicShadowManager() {
    delete[] shadowData_;
    delete[] staticShadowData_;
    if (!Renderer::IsActive()) return;
    if (shadowMap_.id && sg_query_image_state(shadowMap_) == SG_RESOURCESTATE_VALID)
        sg_destroy_image(shadowMap_);
    if (shadowSampler_.id && sg_query_sampler_state(shadowSampler_) == SG_RESOURCESTATE_VALID)
        sg_destroy_sampler(shadowSampler_);
}

void DynamicShadowManager::Init(int columns, int rows) {
    columns_ = columns;
    rows_ = rows;
    shadowWidth_ = (columns - 1) * 4;
    shadowHeight_ = (rows - 1) * 4;

    shadowData_ = new unsigned char[shadowWidth_ * shadowHeight_]();
    staticShadowData_ = new unsigned char[shadowWidth_ * shadowHeight_]();

    sg_image_desc imgDesc = {};
    imgDesc.type = SG_IMAGETYPE_2D;
    imgDesc.usage.immutable = false;
    imgDesc.usage.dynamic_update = true;
    imgDesc.width = shadowWidth_;
    imgDesc.height = shadowHeight_;
    imgDesc.pixel_format = SG_PIXELFORMAT_R8;
    imgDesc.num_mipmaps = 1;
    imgDesc.data.mip_levels[0].size = (size_t)(shadowWidth_ * shadowHeight_);
    imgDesc.data.mip_levels[0].ptr = nullptr;
    shadowMap_ = sg_make_image(&imgDesc);

    sg_sampler_desc smpDesc = {};
    smpDesc.min_filter = SG_FILTER_LINEAR;
    smpDesc.mag_filter = SG_FILTER_LINEAR;
    smpDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smpDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    shadowSampler_ = sg_make_sampler(&smpDesc);
}

void DynamicShadowManager::SetupFrame() {
    sg_image_data data = {};
    data.mip_levels[0].ptr = staticShadowData_;
    data.mip_levels[0].size = (size_t)(shadowWidth_ * shadowHeight_);
    sg_update_image(shadowMap_, &data);
}

void DynamicShadowManager::LoadStaticShadows(const unsigned char* data, int size) {
    int copySize = size < shadowWidth_ * shadowHeight_ ? size : shadowWidth_ * shadowHeight_;
    for (int i = 0; i < copySize; i++)
        staticShadowData_[i] = (data[i] & 0xFF) / 2;
}

void DynamicShadowManager::AddShadow(float worldX, float worldY, int width, int height,
                                      const unsigned char* alphaData) {
    (void)worldX; (void)worldY; (void)width; (void)height; (void)alphaData;
}

void DynamicShadowManager::RemoveShadow(int index) { (void)index; }

void DynamicShadowManager::ReloadToGPU() {
    sg_image_data data = {};
    data.mip_levels[0].ptr = shadowData_;
    data.mip_levels[0].size = (size_t)(shadowWidth_ * shadowHeight_);
    sg_update_image(shadowMap_, &data);
}

void DynamicShadowManager::BlitShadowData(float worldX, float worldY, int texWidth, int texHeight,
                                           const unsigned char* rgbaData, const float* centerOffset) {
    (void)worldX; (void)worldY; (void)texWidth; (void)texHeight;
    (void)rgbaData; (void)centerOffset;
}
