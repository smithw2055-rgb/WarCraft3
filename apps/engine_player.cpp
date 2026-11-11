// ============================================================================
// apps/engine_player.cpp — Zero-Coupling Generic Engine Host Executable
//
// Boots ANY project (`projects/war3/war3.project.json` OR
// `projects/minimal_rts/project.json`) strictly from its `ProjectManifest`
// and dynamically loaded shared plugin (`modulePlugin`), with ZERO compile-time
// or link-time dependencies on `projects/*` or `src/game/war3/*`!
// ============================================================================
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "engine/core/project_manifest.h"
#include "engine/render/rhi_backend.h"
#include "engine/render/standard_render_passes.h"
#include "engine/resource/vfs_archive.h"
#include "engine/ui/hud_view_model.h"
#include "engine/world/simulation_extension.h"

namespace {

using RegisterVfsFn = void (*)(engine::vfs::VfsMountRegistry*);
using CreateExtFn   = engine::world::ISimulationExtension* (*)();
using DestroyExtFn  = void (*)(engine::world::ISimulationExtension*);

struct LoadedExtensionPlugin {
    void* handle = nullptr;
    RegisterVfsFn registerVfs = nullptr;
    CreateExtFn createExt = nullptr;
    DestroyExtFn destroyExt = nullptr;

    ~LoadedExtensionPlugin() {
        Close();
    }

    void Close() {
        if (!handle) return;
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
        handle = nullptr;
        registerVfs = nullptr;
        createExt = nullptr;
        destroyExt = nullptr;
    }

    bool LoadByPluginName(const std::string& pluginBaseName, const std::filesystem::path& exeDir) {
        if (pluginBaseName.empty()) return false;

        std::vector<std::filesystem::path> candidates;
#if defined(_WIN32)
        const std::string fileName = pluginBaseName + ".dll";
#elif defined(__APPLE__)
        const std::string fileName = "lib" + pluginBaseName + ".dylib";
#else
        const std::string fileName = "lib" + pluginBaseName + ".so";
#endif
        candidates.push_back(exeDir / fileName);
        candidates.push_back(std::filesystem::path("build") / fileName);
        candidates.push_back(std::filesystem::path(".") / fileName);

        for (const auto& cand : candidates) {
            std::error_code ec;
            if (!std::filesystem::exists(cand, ec)) continue;
#if defined(_WIN32)
            HMODULE mod = LoadLibraryA(cand.string().c_str());
            if (!mod) continue;
            auto cFn = reinterpret_cast<CreateExtFn>(GetProcAddress(mod, "EngineCreateSimulationExtension"));
            auto dFn = reinterpret_cast<DestroyExtFn>(GetProcAddress(mod, "EngineDestroySimulationExtension"));
            auto vFn = reinterpret_cast<RegisterVfsFn>(GetProcAddress(mod, "EngineRegisterVfsProviders"));
            if (!cFn || !dFn) {
                FreeLibrary(mod);
                continue;
            }
            handle = mod;
#else
            void* mod = dlopen(cand.string().c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!mod) continue;
            auto cFn = reinterpret_cast<CreateExtFn>(dlsym(mod, "EngineCreateSimulationExtension"));
            auto dFn = reinterpret_cast<DestroyExtFn>(dlsym(mod, "EngineDestroySimulationExtension"));
            auto vFn = reinterpret_cast<RegisterVfsFn>(dlsym(mod, "EngineRegisterVfsProviders"));
            if (!cFn || !dFn) {
                dlclose(mod);
                continue;
            }
            handle = mod;
#endif
            createExt = cFn;
            destroyExt = dFn;
            registerVfs = vFn;
            return true;
        }
        return false;
    }
};

} // namespace

int main(int argc, char* argv[]) {
    std::string manifestPath = "projects/war3/war3.project.json";
    int ticksToRun = 60;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--project") == 0 && i + 1 < argc) {
            manifestPath = argv[++i];
        } else if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) {
            ticksToRun = std::max(1, std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::printf("Usage: %s [--project <manifest.json|project.godot>] [--ticks <N>]\n", argv[0]);
            return 0;
        }
    }

    engine::ProjectManifest manifest;
    std::string err;
    if (!engine::ProjectManifest::LoadFromFile(manifestPath, manifest, &err)) {
        std::fprintf(stderr, "engine_player: failed to load manifest '%s': %s\n",
                     manifestPath.c_str(), err.c_str());
        return 1;
    }

    const std::filesystem::path exeDir = std::filesystem::path(argv[0]).parent_path();
    LoadedExtensionPlugin plugin;
    if (!plugin.LoadByPluginName(manifest.modulePlugin, exeDir)) {
        std::fprintf(stderr, "engine_player: failed to load module plugin '%s' for project '%s'\n",
                     manifest.modulePlugin.c_str(), manifest.projectId.c_str());
        return 1;
    }

    engine::vfs::VfsMountRegistry vfs;
    if (plugin.registerVfs) {
        plugin.registerVfs(&vfs);
    }

    // Resolve base directory from CWD or ancestor of manifestPath so running from `build/` works seamlessly
    std::string baseDir = ".";
    if (!manifest.vfsMounts.empty()) {
        std::error_code ec;
        const std::string& sampleRel = manifest.vfsMounts.front().path;
        if (!std::filesystem::exists(std::filesystem::path(baseDir) / sampleRel, ec)) {
            std::filesystem::path probe = std::filesystem::absolute(manifestPath, ec).parent_path();
            for (int depth = 0; depth < 5 && !probe.empty(); ++depth) {
                if (std::filesystem::exists(probe / sampleRel, ec)) {
                    baseDir = probe.generic_string();
                    break;
                }
                const std::filesystem::path parent = probe.parent_path();
                if (parent == probe) break;
                probe = parent;
            }
        }
    }

    size_t mountedCount = 0;
    if (!vfs.MountFromManifest(manifest, baseDir, &mountedCount, &err)) {
        std::fprintf(stderr, "engine_player: VFS mount error: %s\n", err.c_str());
        return 1;
    }

    std::unique_ptr<engine::world::ISimulationExtension, DestroyExtFn> simExt(
        plugin.createExt(), plugin.destroyExt);
    if (!simExt) {
        std::fprintf(stderr, "engine_player: plugin '%s' returned null simulation extension\n",
                     manifest.modulePlugin.c_str());
        return 1;
    }

    // Resolve initial scene / prototype data via VFS or manifest mount mapping
    std::string configPayload;
    if (!manifest.defaultScene.empty()) {
        std::vector<uint8_t> sceneBytes;
        const bool isJsonData = manifest.defaultScene.size() >= 5 &&
            manifest.defaultScene.substr(manifest.defaultScene.size() - 5) == ".json";
        if (isJsonData) {
            if (!vfs.ReadFile(manifest.defaultScene, sceneBytes)) {
                const std::string normScene = engine::vfs::NormalizeVfsKey(manifest.defaultScene);
                for (const auto& mount : manifest.vfsMounts) {
                    const std::string normSrc = engine::vfs::NormalizeVfsKey(mount.path);
                    if (!normSrc.empty() &&
                        normScene.size() > normSrc.size() &&
                        normScene.compare(0, normSrc.size(), normSrc) == 0 &&
                        normScene[normSrc.size()] == '/') {
                        const std::string normMount = engine::vfs::NormalizeVfsKey(mount.mountPoint);
                        const std::string remapped = normMount.empty()
                            ? normScene.substr(normSrc.size() + 1)
                            : (normMount + "/" + normScene.substr(normSrc.size() + 1));
                        if (vfs.ReadFile(remapped, sceneBytes)) {
                            break;
                        }
                    }
                }
            }
            if (!sceneBytes.empty()) {
                configPayload.assign(reinterpret_cast<const char*>(sceneBytes.data()), sceneBytes.size());
            }
        } else {
            configPayload = manifest.defaultScene;
            std::error_code ec;
            if (!std::filesystem::exists(configPayload, ec) && baseDir != ".") {
                const std::filesystem::path candidate = std::filesystem::path(baseDir) / configPayload;
                if (std::filesystem::exists(candidate, ec)) {
                    configPayload = candidate.generic_string();
                }
            }
        }
    }

    if (!simExt->ConfigureFromProjectData(configPayload)) {
        std::fprintf(stderr, "engine_player: failed to configure extension '%s'\n",
                     simExt->GetExtensionId());
        return 1;
    }

    // Headless RHI + StandardRenderPassPipeline + MVVM HUD verification
    engine::render::NullRhiBackend nullRhi;
    engine::render::TerrainOverlayState overlay(64, 64);
    engine::render::RhiTextureDesc overlayTexDesc;
    overlayTexDesc.width = 64;
    overlayTexDesc.height = 64;
    overlayTexDesc.dynamicUpdate = true;
    const uint32_t overlayTex = nullRhi.CreateTexture2D(overlayTexDesc);
    auto renderPipeline = engine::render::StandardRenderPassPipeline::CreateStandardRtsPipeline();

    engine::ui::HudViewModel hudVm;
    engine::ui::HudViewBinder hudBinder;

    const float fixedDt = 1.0f / static_cast<float>(manifest.fixedTickHz);
    const int checkpointTick = ticksToRun / 2;
    std::vector<uint8_t> esavCheckpoint;

    engine::world::DeterministicSaveArchive saveArchive;
    saveArchive.RegisterContributor(simExt.get());

    for (int t = 0; t < ticksToRun; ++t) {
        if (t == checkpointTick) {
            esavCheckpoint = saveArchive.SaveAll(static_cast<uint64_t>(t));
        }
        simExt->OnTickPhase(engine::world::TickPhase::CommandIngest, fixedDt);
        simExt->OnTickPhase(engine::world::TickPhase::OrderBehavior, fixedDt);
        simExt->OnTickPhase(engine::world::TickPhase::MovementSteering, fixedDt);
        simExt->OnTickPhase(engine::world::TickPhase::CombatProjectiles, fixedDt);
        simExt->OnTickPhase(engine::world::TickPhase::PostTickCleanup, fixedDt);

        overlay.SetMaskCircle(16 + (t % 16), 16, 3, true);
        engine::render::StandardPassContext passCtx{
            &nullRhi, &overlay, overlayTex, manifest.windowWidth, manifest.windowHeight};
        renderPipeline.ExecuteFrame(passCtx);

        hudVm.SetPrimaryResource(500 + t);
        hudBinder.Dispatch(hudVm);
    }

    const uint64_t firstRunHash = simExt->ComputeDeterministicHash();

    // Verify ESAV midpoint restore -> replay produces bit-identical hash!
    if (!esavCheckpoint.empty()) {
        uint64_t restoredTick = 0;
        if (!saveArchive.LoadAll(esavCheckpoint, &restoredTick)) {
            std::fprintf(stderr, "engine_player: ESAV checkpoint restore failed!\n");
            return 1;
        }
        for (int t = checkpointTick; t < ticksToRun; ++t) {
            simExt->OnTickPhase(engine::world::TickPhase::CommandIngest, fixedDt);
            simExt->OnTickPhase(engine::world::TickPhase::OrderBehavior, fixedDt);
            simExt->OnTickPhase(engine::world::TickPhase::MovementSteering, fixedDt);
            simExt->OnTickPhase(engine::world::TickPhase::CombatProjectiles, fixedDt);
            simExt->OnTickPhase(engine::world::TickPhase::PostTickCleanup, fixedDt);
        }
        const uint64_t replayHash = simExt->ComputeDeterministicHash();
        if (replayHash != firstRunHash) {
            std::fprintf(stderr,
                         "engine_player: ESAV replay hash mismatch (0x%016llx != 0x%016llx)\n",
                         static_cast<unsigned long long>(replayHash),
                         static_cast<unsigned long long>(firstRunHash));
            return 1;
        }
    }

    simExt->OnShutdownWorld();
    nullRhi.DestroyTexture(overlayTex);

    std::printf(
        "EnginePlayer OK: project=%s extension=%s mounts=%zu ticks=%d stateHash=0x%016llx\n",
        manifest.projectId.c_str(),
        simExt->GetExtensionId(),
        mountedCount,
        ticksToRun,
        static_cast<unsigned long long>(firstRunHash));
    return 0;
}
