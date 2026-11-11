#pragma once
// ============================================================================
// Engine Render: IRhiBackend & NullRhiBackend
//
// Hardware-agnostic Render Hardware Interface (RHI) contract decoupling
// Engine/RenderServer from Sokol/OpenGL/Vulkan/D3D11/Metal headers.
// ============================================================================
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace engine::render {

enum class RhiBufferUsage : uint8_t {
    Vertex = 0,
    Index = 1,
    InstanceStream = 2,
    Uniform = 3
};

enum class RhiPixelFormat : uint8_t {
    RGBA8 = 0,
    RGBA32F = 1,
    R8 = 2,
    R32F = 3,
    Depth24Stencil8 = 4
};

struct RhiBufferDesc {
    RhiBufferUsage usage = RhiBufferUsage::Vertex;
    size_t sizeBytes = 0;
    bool dynamicUpdate = false;
    const void* initialData = nullptr;
};

struct RhiTextureDesc {
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t layers = 1;
    uint32_t mipLevels = 1;
    RhiPixelFormat format = RhiPixelFormat::RGBA8;
    bool dynamicUpdate = false;
    const void* initialData = nullptr;
    size_t initialDataSize = 0;
};

struct RhiPipelineDesc {
    uint32_t shaderId = 0;
    bool depthWrite = true;
    bool depthTest = true;
    bool alphaBlend = false;
    bool additiveBlend = false;
    bool cullBack = true;
};

struct RhiBackendStats {
    uint64_t buffersCreated = 0;
    uint64_t buffersDestroyed = 0;
    uint64_t bufferUpdates = 0;
    uint64_t texturesCreated = 0;
    uint64_t texturesDestroyed = 0;
    uint64_t textureUpdates = 0;
    uint64_t pipelinesCreated = 0;
    uint64_t passesBegun = 0;
    uint64_t drawCalls = 0;
    uint64_t instancesDrawn = 0;
};

class IRhiBackend {
public:
    virtual ~IRhiBackend() = default;

    virtual const char* GetBackendName() const = 0;
    virtual bool IsHeadless() const = 0;

    virtual uint32_t CreateBuffer(const RhiBufferDesc& desc) = 0;
    virtual void UpdateBuffer(uint32_t handle, const void* data, size_t sizeBytes) = 0;
    virtual void DestroyBuffer(uint32_t handle) = 0;
    virtual bool IsBufferValid(uint32_t handle) const = 0;

    virtual uint32_t CreateTexture2D(const RhiTextureDesc& desc) = 0;
    virtual void UpdateTexture2D(uint32_t handle,
                                 uint32_t x,
                                 uint32_t y,
                                 uint32_t w,
                                 uint32_t h,
                                 const void* pixels,
                                 size_t byteSize) = 0;
    virtual void DestroyTexture(uint32_t handle) = 0;
    virtual bool IsTextureValid(uint32_t handle) const = 0;

    virtual uint32_t CreatePipeline(const RhiPipelineDesc& desc) = 0;
    virtual void DestroyPipeline(uint32_t handle) = 0;

    virtual void BeginPass(float clearR, float clearG, float clearB, float clearA) = 0;
    virtual void SetViewport(int x, int y, int width, int height) = 0;
    virtual void SetScissor(int x, int y, int width, int height) = 0;
    virtual void BindPipeline(uint32_t pipelineHandle) = 0;
    virtual void DrawIndexed(uint32_t baseElement, uint32_t elementCount, uint32_t instanceCount) = 0;
    virtual void EndPass() = 0;

    virtual const RhiBackendStats& GetStats() const = 0;
    virtual void ResetStats() = 0;
};

class NullRhiBackend final : public IRhiBackend {
public:
    const char* GetBackendName() const override { return "NullRhiBackend"; }
    bool IsHeadless() const override { return true; }

    uint32_t CreateBuffer(const RhiBufferDesc& desc) override {
        if (desc.sizeBytes == 0) return 0u;
        const uint32_t h = nextHandle_++;
        liveBuffers_.insert(h);
        ++stats_.buffersCreated;
        return h;
    }

    void UpdateBuffer(uint32_t handle, const void* data, size_t sizeBytes) override {
        if (data && sizeBytes > 0 && liveBuffers_.count(handle) > 0) {
            ++stats_.bufferUpdates;
        }
    }

    void DestroyBuffer(uint32_t handle) override {
        if (liveBuffers_.erase(handle) > 0) {
            ++stats_.buffersDestroyed;
        }
    }

    bool IsBufferValid(uint32_t handle) const override {
        return liveBuffers_.count(handle) > 0;
    }

    uint32_t CreateTexture2D(const RhiTextureDesc& desc) override {
        if (desc.width == 0 || desc.height == 0) return 0u;
        const uint32_t h = nextHandle_++;
        NullTexSlot slot;
        slot.width = desc.width;
        slot.height = desc.height;
        slot.bpp = BytesPerPixelForFormat(desc.format);
        const size_t totalBytes =
            static_cast<size_t>(slot.width) * static_cast<size_t>(slot.height) * slot.bpp;
        slot.pixels.assign(totalBytes, 0u);
        if (desc.initialData && desc.initialDataSize > 0) {
            std::memcpy(slot.pixels.data(),
                        desc.initialData,
                        std::min(totalBytes, desc.initialDataSize));
        }
        liveTextures_.emplace(h, std::move(slot));
        ++stats_.texturesCreated;
        return h;
    }

    void UpdateTexture2D(uint32_t handle,
                         uint32_t x,
                         uint32_t y,
                         uint32_t w,
                         uint32_t h,
                         const void* pixels,
                         size_t byteSize) override {
        if (!pixels || w == 0 || h == 0 || byteSize == 0) return;
        auto it = liveTextures_.find(handle);
        if (it == liveTextures_.end()) return;
        NullTexSlot& slot = it->second;
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
            uint8_t* dstRow = slot.pixels.data() +
                              static_cast<size_t>(y + row) * dstPitch +
                              static_cast<size_t>(x) * slot.bpp;
            std::memcpy(dstRow, srcBytes + static_cast<size_t>(row) * rowBytes, rowBytes);
        }
        ++stats_.textureUpdates;
    }

    void DestroyTexture(uint32_t handle) override {
        if (liveTextures_.erase(handle) > 0) {
            ++stats_.texturesDestroyed;
        }
    }

    bool IsTextureValid(uint32_t handle) const override {
        return liveTextures_.find(handle) != liveTextures_.end();
    }

    bool ReadTextureTexelRgba8(uint32_t handle,
                               uint32_t x,
                               uint32_t y,
                               uint8_t outRgba[4]) const {
        if (!outRgba) return false;
        auto it = liveTextures_.find(handle);
        if (it == liveTextures_.end()) return false;
        const NullTexSlot& slot = it->second;
        if (slot.bpp < 4 || x >= slot.width || y >= slot.height) return false;
        const size_t off =
            (static_cast<size_t>(y) * static_cast<size_t>(slot.width) + static_cast<size_t>(x)) *
            slot.bpp;
        if (off + 4 > slot.pixels.size()) return false;
        std::memcpy(outRgba, slot.pixels.data() + off, 4u);
        return true;
    }

    uint32_t CreatePipeline(const RhiPipelineDesc& /*desc*/) override {
        const uint32_t h = nextHandle_++;
        livePipelines_.insert(h);
        ++stats_.pipelinesCreated;
        return h;
    }

    void DestroyPipeline(uint32_t handle) override {
        livePipelines_.erase(handle);
    }

    void BeginPass(float /*clearR*/, float /*clearG*/, float /*clearB*/, float /*clearA*/) override {
        ++stats_.passesBegun;
    }

    void SetViewport(int x, int y, int width, int height) override {
        lastViewport_[0] = x;
        lastViewport_[1] = y;
        lastViewport_[2] = width;
        lastViewport_[3] = height;
    }

    void SetScissor(int x, int y, int width, int height) override {
        lastScissor_[0] = x;
        lastScissor_[1] = y;
        lastScissor_[2] = width;
        lastScissor_[3] = height;
    }

    void BindPipeline(uint32_t pipelineHandle) override {
        activePipeline_ = pipelineHandle;
    }

    void DrawIndexed(uint32_t /*baseElement*/, uint32_t elementCount, uint32_t instanceCount) override {
        if (elementCount == 0 || instanceCount == 0) return;
        ++stats_.drawCalls;
        stats_.instancesDrawn += instanceCount;
    }

    void EndPass() override {}

    const RhiBackendStats& GetStats() const override { return stats_; }
    void ResetStats() override { stats_ = RhiBackendStats{}; }

    size_t LiveBufferCount() const { return liveBuffers_.size(); }
    size_t LiveTextureCount() const { return liveTextures_.size(); }

private:
    struct NullTexSlot {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t bpp = 4;
        std::vector<uint8_t> pixels;
    };

    static constexpr uint32_t BytesPerPixelForFormat(RhiPixelFormat fmt) noexcept {
        switch (fmt) {
            case RhiPixelFormat::RGBA32F: return 16u;
            case RhiPixelFormat::R8:      return 1u;
            case RhiPixelFormat::R32F:    return 4u;
            default:                      return 4u;
        }
    }

    uint32_t nextHandle_ = 0x10000u;
    uint32_t activePipeline_ = 0u;
    int lastViewport_[4] = {0, 0, 0, 0};
    int lastScissor_[4] = {0, 0, 0, 0};
    std::unordered_set<uint32_t> liveBuffers_;
    std::unordered_map<uint32_t, NullTexSlot> liveTextures_;
    std::unordered_set<uint32_t> livePipelines_;
    RhiBackendStats stats_{};
};

} // namespace engine::render
