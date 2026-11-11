#pragma once
// ============================================================================
// Render Hardware Interface Device (`render::RhiDevice` & `SokolRhiBackend`)
//
// Bridges `src/render/` and `RenderServer` directly onto `engine::render::IRhiBackend`
// (`SokolRhiBackend` when GPU swapchain is active, `NullRhiBackend` in headless
// simulation/CI mode).
// ============================================================================
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "engine/render/rhi_backend.h"
#include "third_party/sokol/sokol_gfx.h"

template <typename T>
inline sg_range sg_rng(T& data) {
    sg_range r = {&data, sizeof(data)};
    return r;
}

class Texture;

namespace Renderer {
    bool Initialize();
    void Shutdown();
    bool IsActive();
    void BeginFrame();
    void EndPass();
    void CommitFrame();
    void EndFrame();
    void SetWireframe(bool enabled);
    void SetClearColor(const float color[4]);
    sg_pass_action GetDefaultPassAction();
} // namespace Renderer

namespace render {

// Concrete Sokol implementation of `engine::render::IRhiBackend`
class SokolRhiBackend final : public engine::render::IRhiBackend {
public:
    const char* GetBackendName() const override { return "SokolRhiBackend"; }
    bool IsHeadless() const override { return !Renderer::IsActive(); }

    uint32_t CreateBuffer(const engine::render::RhiBufferDesc& desc) override {
        if (desc.sizeBytes == 0 || !Renderer::IsActive()) return 0u;
        sg_buffer_desc sd = {};
        if (desc.usage == engine::render::RhiBufferUsage::Index) {
            sd.usage.index_buffer = true;
        } else {
            sd.usage.vertex_buffer = true;
        }
        if (desc.dynamicUpdate) {
            sd.size = desc.sizeBytes;
            sd.usage.stream_update = true;
        } else {
            sd.usage.immutable = true;
            sd.data.ptr = desc.initialData;
            sd.data.size = desc.sizeBytes;
        }
        const sg_buffer buf = sg_make_buffer(&sd);
        if (buf.id != 0) {
            ++stats_.buffersCreated;
        }
        return buf.id;
    }

    void UpdateBuffer(uint32_t handle, const void* data, size_t sizeBytes) override {
        if (!handle || !data || sizeBytes == 0 || !Renderer::IsActive()) return;
        sg_range rng = {data, sizeBytes};
        sg_update_buffer(sg_buffer{handle}, &rng);
        ++stats_.bufferUpdates;
    }

    void DestroyBuffer(uint32_t handle) override {
        if (!handle || !Renderer::IsActive()) return;
        sg_destroy_buffer(sg_buffer{handle});
        ++stats_.buffersDestroyed;
    }

    bool IsBufferValid(uint32_t handle) const override {
        if (!handle || !Renderer::IsActive()) return false;
        return sg_query_buffer_state(sg_buffer{handle}) == SG_RESOURCESTATE_VALID;
    }

    uint32_t CreateTexture2D(const engine::render::RhiTextureDesc& desc) override {
        if (desc.width == 0 || desc.height == 0 || !Renderer::IsActive()) return 0u;
        sg_image_desc id = {};
        id.type = SG_IMAGETYPE_2D;
        id.width = static_cast<int>(desc.width);
        id.height = static_cast<int>(desc.height);
        id.num_mipmaps = static_cast<int>(std::max(1u, desc.mipLevels));
        uint32_t bpp = 4u;
        switch (desc.format) {
            case engine::render::RhiPixelFormat::RGBA32F: id.pixel_format = SG_PIXELFORMAT_RGBA32F; bpp = 16u; break;
            case engine::render::RhiPixelFormat::R8:      id.pixel_format = SG_PIXELFORMAT_R8;      bpp = 1u;  break;
            case engine::render::RhiPixelFormat::R32F:    id.pixel_format = SG_PIXELFORMAT_R32F;    bpp = 4u;  break;
            default:                                      id.pixel_format = SG_PIXELFORMAT_RGBA8;   bpp = 4u;  break;
        }
        if (desc.dynamicUpdate) {
            id.usage.stream_update = true;
        } else if (desc.initialData && desc.initialDataSize > 0) {
            id.data.mip_levels[0].ptr = desc.initialData;
            id.data.mip_levels[0].size = desc.initialDataSize;
        }
        const sg_image img = sg_make_image(&id);
        if (img.id != 0) {
            ++stats_.texturesCreated;
            if (desc.dynamicUpdate) {
                RegisterExternalDynamicTexture(
                    img.id, desc.width, desc.height, bpp, desc.initialData, desc.initialDataSize);
            }
        }
        return img.id;
    }

    void RegisterExternalDynamicTexture(uint32_t handle,
                                        uint32_t width,
                                        uint32_t height,
                                        uint32_t bpp,
                                        const void* initialData = nullptr,
                                        size_t initialBytes = 0) {
        if (!handle || width == 0 || height == 0 || bpp == 0) return;
        DynamicTexSlot slot;
        slot.width = width;
        slot.height = height;
        slot.bpp = bpp;
        const size_t totalBytes =
            static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(bpp);
        slot.cpuShadow.assign(totalBytes, 0u);
        if (initialData && initialBytes > 0) {
            std::memcpy(slot.cpuShadow.data(), initialData, std::min(totalBytes, initialBytes));
            slot.dirtyThisFrame = true;
        }
        dynamicTextures_[handle] = std::move(slot);
    }

    void UpdateTexture2D(uint32_t handle,
                         uint32_t x,
                         uint32_t y,
                         uint32_t w,
                         uint32_t h,
                         const void* pixels,
                         size_t byteSize) override {
        if (!handle || !pixels || w == 0 || h == 0 || byteSize == 0 || !Renderer::IsActive()) return;
        auto it = dynamicTextures_.find(handle);
        if (it == dynamicTextures_.end()) return;
        DynamicTexSlot& slot = it->second;
        if (x >= slot.width || y >= slot.height || w > slot.width - x || h > slot.height - y) {
            return;
        }
        const size_t rowBytes = static_cast<size_t>(w) * slot.bpp;
        if (byteSize < rowBytes * static_cast<size_t>(h)) {
            return;
        }
        const size_t dstPitch = static_cast<size_t>(slot.width) * slot.bpp;
        const auto* srcBytes = static_cast<const uint8_t*>(pixels);
        for (uint32_t row = 0; row < h; ++row) {
            uint8_t* dstRow = slot.cpuShadow.data() +
                              static_cast<size_t>(y + row) * dstPitch +
                              static_cast<size_t>(x) * slot.bpp;
            std::memcpy(dstRow, srcBytes + static_cast<size_t>(row) * rowBytes, rowBytes);
        }
        slot.dirtyThisFrame = true;
        ++stats_.textureUpdates;
    }

    void FlushPendingUploads() {
        if (!Renderer::IsActive()) return;
        for (auto& kv : dynamicTextures_) {
            DynamicTexSlot& slot = kv.second;
            if (!slot.dirtyThisFrame || slot.cpuShadow.empty()) continue;
            slot.dirtyThisFrame = false;
            sg_image_data imgData = {};
            imgData.mip_levels[0].ptr = slot.cpuShadow.data();
            imgData.mip_levels[0].size = slot.cpuShadow.size();
            sg_update_image(sg_image{kv.first}, &imgData);
        }
    }

    void ClearDynamicTextures() {
        dynamicTextures_.clear();
    }

    void DestroyTexture(uint32_t handle) override {
        dynamicTextures_.erase(handle);
        if (!handle || !Renderer::IsActive()) return;
        sg_destroy_image(sg_image{handle});
        ++stats_.texturesDestroyed;
    }

    bool IsTextureValid(uint32_t handle) const override {
        if (!handle || !Renderer::IsActive()) return false;
        return sg_query_image_state(sg_image{handle}) == SG_RESOURCESTATE_VALID;
    }

    uint32_t CreatePipeline(const engine::render::RhiPipelineDesc& desc) override {
        if (!Renderer::IsActive() || desc.shaderId == 0) return 0u;
        sg_pipeline_desc pd = {};
        pd.shader = sg_shader{desc.shaderId};
        pd.depth.write_enabled = desc.depthWrite;
        pd.depth.compare = desc.depthTest ? SG_COMPAREFUNC_LESS_EQUAL : SG_COMPAREFUNC_ALWAYS;
        pd.cull_mode = desc.cullBack ? SG_CULLMODE_BACK : SG_CULLMODE_NONE;
        const sg_pipeline pip = sg_make_pipeline(&pd);
        if (pip.id != 0) {
            ++stats_.pipelinesCreated;
        }
        return pip.id;
    }

    void DestroyPipeline(uint32_t handle) override {
        if (!handle || !Renderer::IsActive()) return;
        sg_destroy_pipeline(sg_pipeline{handle});
    }

    void BeginPass(float clearR, float clearG, float clearB, float clearA) override {
        const float color[4] = {clearR, clearG, clearB, clearA};
        Renderer::SetClearColor(color);
        if (Renderer::IsActive()) {
            Renderer::BeginFrame();
        }
        ++stats_.passesBegun;
    }

    void SetViewport(int x, int y, int width, int height) override {
        if (Renderer::IsActive()) {
            sg_apply_viewport(x, y, width, height, true);
        }
    }

    void SetScissor(int x, int y, int width, int height) override {
        if (Renderer::IsActive()) {
            sg_apply_scissor_rect(x, y, width, height, true);
        }
    }

    void BindPipeline(uint32_t pipelineHandle) override {
        if (pipelineHandle && Renderer::IsActive()) {
            sg_apply_pipeline(sg_pipeline{pipelineHandle});
        }
    }

    void DrawIndexed(uint32_t baseElement, uint32_t elementCount, uint32_t instanceCount) override {
        if (elementCount == 0 || instanceCount == 0) return;
        if (Renderer::IsActive()) {
            FlushPendingUploads();
            sg_draw(static_cast<int>(baseElement),
                    static_cast<int>(elementCount),
                    static_cast<int>(instanceCount));
        }
        ++stats_.drawCalls;
        stats_.instancesDrawn += instanceCount;
    }

    void EndPass() override {
        if (Renderer::IsActive()) {
            FlushPendingUploads();
            Renderer::EndFrame();
        }
    }

    const engine::render::RhiBackendStats& GetStats() const override { return stats_; }
    void ResetStats() override { stats_ = engine::render::RhiBackendStats{}; }

private:
    struct DynamicTexSlot {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t bpp = 4;
        std::vector<uint8_t> cpuShadow;
        bool dirtyThisFrame = false;
    };

    engine::render::RhiBackendStats stats_{};
    std::unordered_map<uint32_t, DynamicTexSlot> dynamicTextures_;
};

struct RhiDeviceStats {
    int buffersCreated = 0;
    int buffersDestroyed = 0;
    int liveBuffers = 0;
    int texturesCreated = 0;
    int texturesDestroyed = 0;
    int liveTextures = 0;
    int viewsCreated = 0;
    int viewsDestroyed = 0;
    int liveViews = 0;
    int samplersCreated = 0;
    int samplersDestroyed = 0;
    int shadersCreated = 0;
    int pipelinesCreated = 0;
    int pipelineBinds = 0;
    int drawCallsSubmitted = 0;
    int instancesDrawn = 0;
};

class RhiDevice {
public:
    static RhiDevice& Instance() {
        static RhiDevice instance;
        return instance;
    }

    RhiDevice()
        : nullBackend_(std::make_shared<engine::render::NullRhiBackend>()),
          sokolBackend_(std::make_shared<SokolRhiBackend>()),
          activeBackend_(nullBackend_) {}

    void ActivateSokolBackend() {
        activeBackend_ = sokolBackend_;
    }

    void DeactivateSokolBackend() {
        if (sokolBackend_) {
            sokolBackend_->ClearDynamicTextures();
        }
        activeBackend_ = nullBackend_;
    }

    void SetBackend(std::shared_ptr<engine::render::IRhiBackend> customBackend) {
        activeBackend_ = customBackend ? std::move(customBackend) : nullBackend_;
    }

    std::shared_ptr<engine::render::IRhiBackend> GetBackend() const {
        return activeBackend_;
    }

    void Reset() {
        stats_ = RhiDeviceStats{};
        if (nullBackend_) nullBackend_->ResetStats();
        if (sokolBackend_) sokolBackend_->ResetStats();
        if (activeBackend_) activeBackend_->ResetStats();
        mockViews_.clear();
        mockSamplers_.clear();
        mockPipelines_.clear();
        nextAuxHandle_ = 0x40000u;
    }

    void SetHeadlessMockEnabled(bool enabled) {
        headlessMockEnabled_ = enabled;
        if (enabled && !IsGpuActive()) {
            activeBackend_ = nullBackend_;
        }
    }

    bool IsHeadlessMockEnabled() const { return headlessMockEnabled_; }
    bool IsGpuActive() const { return Renderer::IsActive(); }

    sg_buffer CreateVertexBuffer(const void* data, size_t sizeBytes, bool streamUpdate = false) {
        if (!IsGpuActive() && !headlessMockEnabled_) {
            return {0};
        }
        engine::render::RhiBufferDesc desc;
        desc.usage = streamUpdate
            ? engine::render::RhiBufferUsage::InstanceStream
            : engine::render::RhiBufferUsage::Vertex;
        desc.sizeBytes = sizeBytes;
        desc.dynamicUpdate = streamUpdate;
        desc.initialData = data;
        const uint32_t id = activeBackend_->CreateBuffer(desc);
        if (id != 0) {
            stats_.buffersCreated++;
            stats_.liveBuffers++;
        }
        return sg_buffer{id};
    }

    sg_buffer CreateIndexBuffer(const void* data, size_t sizeBytes) {
        if (!IsGpuActive() && !headlessMockEnabled_) {
            return {0};
        }
        engine::render::RhiBufferDesc desc;
        desc.usage = engine::render::RhiBufferUsage::Index;
        desc.sizeBytes = sizeBytes;
        desc.dynamicUpdate = false;
        desc.initialData = data;
        const uint32_t id = activeBackend_->CreateBuffer(desc);
        if (id != 0) {
            stats_.buffersCreated++;
            stats_.liveBuffers++;
        }
        return sg_buffer{id};
    }

    void UpdateBuffer(sg_buffer& buf, const void* data, size_t sizeBytes) {
        if (!buf.id || !data || sizeBytes == 0) return;
        activeBackend_->UpdateBuffer(buf.id, data, sizeBytes);
    }

    void DestroyBuffer(sg_buffer& buf) {
        if (!buf.id) return;
        if (activeBackend_->IsBufferValid(buf.id) || IsGpuActive()) {
            activeBackend_->DestroyBuffer(buf.id);
            stats_.buffersDestroyed++;
            stats_.liveBuffers = std::max(0, stats_.liveBuffers - 1);
        }
        buf = {0};
    }

    bool IsBufferValid(sg_buffer buf) const {
        return buf.id != 0 && activeBackend_->IsBufferValid(buf.id);
    }

    sg_image CreateTexture2D(int width,
                             int height,
                             sg_pixel_format fmt,
                             const void* pixels = nullptr,
                             size_t byteSize = 0,
                             bool streamUpdate = false) {
        if (!IsGpuActive() && !headlessMockEnabled_) {
            return {0};
        }
        if (IsGpuActive()) {
            sg_image_desc desc = {};
            desc.type = SG_IMAGETYPE_2D;
            desc.width = width;
            desc.height = height;
            desc.pixel_format = fmt;
            desc.num_mipmaps = 1;
            if (streamUpdate) {
                desc.usage.stream_update = true;
            } else if (pixels && byteSize > 0) {
                desc.data.mip_levels[0].ptr = pixels;
                desc.data.mip_levels[0].size = byteSize;
            }
            sg_image img = sg_make_image(&desc);
            if (img.id) {
                stats_.texturesCreated++;
                stats_.liveTextures++;
                if (streamUpdate && sokolBackend_ && width > 0 && height > 0) {
                    uint32_t bpp = 4u;
                    if (fmt == SG_PIXELFORMAT_RGBA32F) bpp = 16u;
                    else if (fmt == SG_PIXELFORMAT_R8) bpp = 1u;
                    else if (fmt == SG_PIXELFORMAT_R32F) bpp = 4u;
                    sokolBackend_->RegisterExternalDynamicTexture(
                        img.id,
                        static_cast<uint32_t>(width),
                        static_cast<uint32_t>(height),
                        bpp,
                        pixels,
                        byteSize);
                }
            }
            return img;
        }
        engine::render::RhiTextureDesc rhiDesc;
        rhiDesc.width = static_cast<uint32_t>(std::max(0, width));
        rhiDesc.height = static_cast<uint32_t>(std::max(0, height));
        rhiDesc.dynamicUpdate = streamUpdate;
        rhiDesc.initialData = pixels;
        rhiDesc.initialDataSize = byteSize;
        const uint32_t id = activeBackend_->CreateTexture2D(rhiDesc);
        if (id != 0) {
            stats_.texturesCreated++;
            stats_.liveTextures++;
        }
        return sg_image{id};
    }

    void DestroyTexture(sg_image& img) {
        if (!img.id) return;
        if (IsGpuActive() || activeBackend_->IsTextureValid(img.id)) {
            activeBackend_->DestroyTexture(img.id);
            stats_.texturesDestroyed++;
            stats_.liveTextures = std::max(0, stats_.liveTextures - 1);
        }
        img = {0};
    }

    sg_view CreateTextureView(sg_image img) {
        if (!img.id) return {0};
        if (IsGpuActive()) {
            if (sg_query_image_state(img) != SG_RESOURCESTATE_VALID) return {0};
            sg_view_desc vd = {};
            vd.texture.image = img;
            sg_view v = sg_make_view(&vd);
            if (v.id) {
                stats_.viewsCreated++;
                stats_.liveViews++;
            }
            return v;
        }
        if (headlessMockEnabled_ && activeBackend_->IsTextureValid(img.id)) {
            sg_view v = {nextAuxHandle_++};
            mockViews_.insert(v.id);
            stats_.viewsCreated++;
            stats_.liveViews++;
            return v;
        }
        return {0};
    }

    void DestroyView(sg_view& view) {
        if (!view.id) return;
        if (IsGpuActive()) {
            if (sg_query_view_state(view) == SG_RESOURCESTATE_VALID) {
                sg_destroy_view(view);
            }
            stats_.viewsDestroyed++;
            stats_.liveViews = std::max(0, stats_.liveViews - 1);
        } else if (mockViews_.erase(view.id) > 0) {
            stats_.viewsDestroyed++;
            stats_.liveViews = std::max(0, stats_.liveViews - 1);
        }
        view = {0};
    }

    sg_sampler CreateSampler(sg_filter minMagFilter, sg_wrap wrapU, sg_wrap wrapV, bool mipmaps = true) {
        if (IsGpuActive()) {
            sg_sampler_desc sd = {};
            sd.min_filter = minMagFilter;
            sd.mag_filter = minMagFilter;
            sd.mipmap_filter = mipmaps ? minMagFilter : static_cast<sg_filter>(0);
            sd.wrap_u = wrapU;
            sd.wrap_v = wrapV;
            sg_sampler smp = sg_make_sampler(&sd);
            if (smp.id) stats_.samplersCreated++;
            return smp;
        }
        if (headlessMockEnabled_) {
            sg_sampler smp = {nextAuxHandle_++};
            mockSamplers_.insert(smp.id);
            stats_.samplersCreated++;
            return smp;
        }
        return {0};
    }

    void DestroySampler(sg_sampler& smp) {
        if (!smp.id) return;
        if (IsGpuActive()) {
            sg_destroy_sampler(smp);
            stats_.samplersDestroyed++;
        } else if (mockSamplers_.erase(smp.id) > 0) {
            stats_.samplersDestroyed++;
        }
        smp = {0};
    }

    sg_pipeline CreatePipeline(const sg_pipeline_desc& desc) {
        if (IsGpuActive()) {
            sg_pipeline pip = sg_make_pipeline(&desc);
            if (pip.id) stats_.pipelinesCreated++;
            return pip;
        }
        if (headlessMockEnabled_) {
            engine::render::RhiPipelineDesc rhiDesc;
            rhiDesc.shaderId = desc.shader.id;
            const uint32_t id = activeBackend_->CreatePipeline(rhiDesc);
            if (id != 0) {
                mockPipelines_.insert(id);
                stats_.pipelinesCreated++;
                return sg_pipeline{id};
            }
        }
        return {0};
    }

    void ApplyPipeline(sg_pipeline pip) {
        if (!pip.id) return;
        stats_.pipelineBinds++;
        activeBackend_->BindPipeline(pip.id);
    }

    void Draw(int baseElement, int numElements, int numInstances = 1) {
        if (numElements <= 0 || numInstances <= 0) return;
        stats_.drawCallsSubmitted++;
        stats_.instancesDrawn += numInstances;
        activeBackend_->DrawIndexed(
            static_cast<uint32_t>(std::max(0, baseElement)),
            static_cast<uint32_t>(numElements),
            static_cast<uint32_t>(numInstances));
    }

    void ResetStateCache() {
        if (IsGpuActive()) {
            sg_reset_state_cache();
        }
    }

    void ApplyViewport(int x, int y, int width, int height, bool originTopLeft = true) {
        if (IsGpuActive()) {
            sg_apply_viewport(x, y, width, height, originTopLeft);
        } else {
            activeBackend_->SetViewport(x, y, width, height);
        }
    }

    void ApplyScissorRect(int x, int y, int width, int height, bool originTopLeft = true) {
        if (IsGpuActive()) {
            sg_apply_scissor_rect(x, y, width, height, originTopLeft);
        } else {
            activeBackend_->SetScissor(x, y, width, height);
        }
    }

    void ResetFullViewport(int width, int height) {
        ResetStateCache();
        ApplyViewport(0, 0, width, height, true);
        ApplyScissorRect(0, 0, width, height, true);
    }

    void FlushPendingUploads() {
        if (IsGpuActive() && sokolBackend_) {
            sokolBackend_->FlushPendingUploads();
        }
    }

    const RhiDeviceStats& GetStats() const { return stats_; }

private:
    bool headlessMockEnabled_ = false;
    uint32_t nextAuxHandle_ = 0x40000u;
    RhiDeviceStats stats_{};
    std::shared_ptr<engine::render::NullRhiBackend> nullBackend_;
    std::shared_ptr<SokolRhiBackend> sokolBackend_;
    std::shared_ptr<engine::render::IRhiBackend> activeBackend_;
    std::unordered_set<uint32_t> mockViews_;
    std::unordered_set<uint32_t> mockSamplers_;
    std::unordered_set<uint32_t> mockPipelines_;
};

} // namespace render
