// ============================================================================
// apps/engine_asset_cli.cpp — Unified VFS & Archive Inspection Tool
//
// Consolidates legacy single-purpose MPQ/MDX/Cliff inspection utilities into a
// single multi-subcommand CLI:
//   engine_asset_cli manifest <project.json>
//   engine_asset_cli list <archive_or_dir> [glob_pattern]
//   engine_asset_cli probe <archive_or_dir> <virtual_path>
// ============================================================================
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "engine/core/project_manifest.h"
#include "engine/resource/format_loader.h"
#include "engine/resource/vfs_archive.h"
#include "projects/war3/mpq_vfs_provider.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::fprintf(
            stderr,
            "Usage:\n"
            "  %s manifest <project.json|project.godot>\n"
            "  %s list <archive_or_folder> [glob]\n"
            "  %s probe <archive_or_folder> <virtual_path>\n",
            argv[0], argv[0], argv[0]);
        return 1;
    }

    const std::string_view subcmd(argv[1]);
    engine::vfs::VfsMountRegistry vfs;
    projects::war3::MpqArchiveProvider::RegisterInVfs(vfs);

    if (subcmd == "manifest") {
        engine::ProjectManifest manifest;
        std::string err;
        if (!engine::ProjectManifest::LoadFromFile(argv[2], manifest, &err)) {
            std::fprintf(stderr, "Manifest error: %s\n", err.c_str());
            return 1;
        }
        size_t mountedCount = 0;
        if (!vfs.MountFromManifest(manifest, ".", &mountedCount, &err)) {
            std::fprintf(stderr, "Mount error: %s\n", err.c_str());
            return 1;
        }
        std::printf("Manifest OK: project=%s declaredMounts=%zu activeMounts=%zu\n",
                    manifest.projectId.c_str(),
                    manifest.vfsMounts.size(),
                    mountedCount);
        return 0;
    }

    const std::filesystem::path targetPath(argv[2]);
    std::error_code ec;
    if (std::filesystem::is_directory(targetPath, ec)) {
        vfs.Mount(std::make_shared<engine::vfs::DirectoryArchiveProvider>(targetPath.string()), 100, "/");
    } else {
        auto mpqProv = std::make_shared<projects::war3::MpqArchiveProvider>(targetPath.string());
        if (!mpqProv->IsOpen()) {
            std::fprintf(stderr, "Failed to mount archive: %s\n", argv[2]);
            return 1;
        }
        vfs.Mount(mpqProv, 100, "/");
    }

    if (subcmd == "list") {
        const std::string_view pattern = (argc >= 4) ? std::string_view(argv[3]) : "*";
        const auto files = vfs.ListFiles(pattern);
        for (const auto& f : files) {
            std::printf("%s\n", f.c_str());
        }
        std::printf("Total matched files: %zu\n", files.size());
        return 0;
    }

    if (subcmd == "probe" && argc >= 4) {
        std::vector<uint8_t> bytes;
        if (!vfs.ReadFile(argv[3], bytes)) {
            std::fprintf(stderr, "Not found: %s\n", argv[3]);
            return 1;
        }
        std::printf("Found '%s' (%zu bytes)\n", argv[3], bytes.size());
        return 0;
    }

    std::fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    return 1;
}
