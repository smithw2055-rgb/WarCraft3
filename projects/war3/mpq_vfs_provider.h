#pragma once
// ============================================================================
// WarCraft III Project Plugin: MpqArchiveProvider (Adapts MpqManager to VFS)
// ============================================================================
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "engine/resource/vfs_archive.h"
#include "resource/mpq_storm.h"

namespace projects::war3 {

class MpqArchiveProvider final : public engine::vfs::IVfsArchiveProvider {
public:
    explicit MpqArchiveProvider(std::string archivePath)
        : archivePath_(std::move(archivePath)) {
        opened_ = mpq_.Open(archivePath_);
    }

    // Safe lifetime-tracked adapter over an external MpqManager (automatically
    // becomes inert if the external MpqManager is destroyed before unbinding).
    MpqArchiveProvider(const MpqManager* sharedMpq, std::string sourceLabel)
        : archivePath_(std::move(sourceLabel)),
          borrowedMpq_(sharedMpq),
          borrowedLifetime_(sharedMpq ? sharedMpq->GetLifetimeToken() : std::weak_ptr<const int>{}),
          opened_(sharedMpq != nullptr) {}

    bool IsOpen() const { return ActiveMpq() != nullptr; }

    const char* GetProviderType() const override { return "mpq"; }
    const std::string& GetSourcePath() const override { return archivePath_; }

    bool Contains(std::string_view virtualPath) const override {
        const MpqManager* active = ActiveMpq();
        return active && active->FileExists(std::string(virtualPath));
    }

    bool ReadAllBytes(std::string_view virtualPath, std::vector<uint8_t>& outBytes) const override {
        const MpqManager* active = ActiveMpq();
        return active && active->ReadFile(std::string(virtualPath), outBytes);
    }

    void EnumerateFiles(std::string_view globPattern, std::vector<std::string>& outFiles) const override {
        const MpqManager* active = ActiveMpq();
        if (!active) return;
        auto listed = active->ListAllFiles(std::string(globPattern));
        for (auto& item : listed) {
            outFiles.push_back(std::move(item));
        }
    }

    static void RegisterInVfs(engine::vfs::VfsMountRegistry& registry) {
        registry.RegisterProviderFactory("mpq", [](const std::string& path) -> std::shared_ptr<engine::vfs::IVfsArchiveProvider> {
            auto prov = std::make_shared<MpqArchiveProvider>(path);
            return prov->IsOpen() ? prov : nullptr;
        });
    }

private:
    const MpqManager* ActiveMpq() const {
        if (borrowedMpq_) {
            return borrowedLifetime_.expired() ? nullptr : borrowedMpq_;
        }
        return opened_ ? &mpq_ : nullptr;
    }

    std::string archivePath_;
    MpqManager mpq_;
    const MpqManager* borrowedMpq_ = nullptr;
    std::weak_ptr<const int> borrowedLifetime_;
    bool opened_ = false;
};

} // namespace projects::war3
