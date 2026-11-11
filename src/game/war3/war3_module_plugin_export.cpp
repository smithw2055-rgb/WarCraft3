// ============================================================================
// Phase 13 & Stage 6 Dynamic Plugin Entry Points for War3GameModule
//
// Exports both:
// 1. Legacy Warsmash C-ABI hooks (`WarsmashCreateSimulationModule`,
//    `WarsmashCreateGameModule`, etc.) for `engine::DynamicModuleLoader`.
// 2. Generic Engine Host C-ABI hooks (`EngineRegisterVfsProviders`,
//    `EngineCreateSimulationExtension`, `EngineDestroySimulationExtension`) so
//    the zero-coupling `engine_player` executable drives `war3.project.json`
//    through the exact same plugin ABI as `minimal_rts`!
// ============================================================================
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

#include "core/unit_presentation_system.h"
#include "core/world_scene_session.h"
#include "ecs/csimulation.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "engine/dynamic_module_loader.h"
#include "engine/world/simulation_extension.h"
#include "game/war3/war3_game_module.h"
#include "projects/war3/mpq_vfs_provider.h"
#include "projects/war3/war3_save_contributor.h"
#include "render/scene.h"
#include "resource/mpq_storm.h"

namespace {

class War3SimulationExtension final : public engine::world::ISimulationExtension {
public:
    War3SimulationExtension()
        : war3Module_(std::make_shared<war3::War3GameModule>()),
          saveContributor_(&sim_) {
        sim_.SetSimulationModule(war3Module_);
        war3Module_->BindSimulation(&sim_);
    }

    ~War3SimulationExtension() override {
        OnShutdownWorld();
    }

    const char* GetExtensionId() const override {
        return war3::War3GameModule::kModuleId;
    }

    bool ConfigureFromProjectData(std::string_view sceneOrDataPath) override {
        scenePath_ = std::string(sceneOrDataPath);
        OnInitializeWorld();
        return true;
    }

    void OnInitializeWorld() override {
        if (initialized_) {
            session_.TeardownCurrentSession(mpq_, sim_, scene_, presentation_);
            initialized_ = false;
        }

        // If defaultScene points to an existing .w3x/.w3m map on disk, load it directly
        std::error_code ec;
        if (!scenePath_.empty() && std::filesystem::is_regular_file(scenePath_, ec)) {
            if (session_.LoadMap(mpq_, sim_, scene_, presentation_, scenePath_)) {
                initialized_ = true;
                return;
            }
        }

        // Fallback when retail maps are not installed: boot a deterministic 2-player
        // WarCraft III melee skirmish (Human Footman vs Orc Grunt) on a 64x64 grid.
        sim_.AddPlayer(std::make_unique<CPlayer>(0, "Player 1 (Human)", CMapControl::USER));
        sim_.AddPlayer(std::make_unique<CPlayer>(1, "Player 2 (Orc)", CMapControl::COMPUTER));
        if (auto* p0 = sim_.GetPlayer(0)) {
            p0->SetGold(500);
            p0->SetLumber(150);
            p0->SetFoodCap(12);
        }
        if (auto* p1 = sim_.GetPlayer(1)) {
            p1->SetGold(500);
            p1->SetLumber(150);
            p1->SetFoodCap(12);
        }

        sim_.SeedRng(2026);
        CUnit* u0 = sim_.CreateUnit(War3ID("hfoo"), 0, -256.0f, 0.0f, 0.0f);
        CUnit* u1 = sim_.CreateUnit(War3ID("ogru"), 1, 256.0f, 0.0f, 180.0f);
        if (u0) {
            u0->IssueOrder(std::make_unique<COrder>(
                OrderIds::MOVE, COrder::TargetKind::POINT, 128.0f, 0.0f), false);
        }
        if (u1) {
            u1->IssueOrder(std::make_unique<COrder>(
                OrderIds::MOVE, COrder::TargetKind::POINT, -128.0f, 0.0f), false);
        }
        sim_.Update();
        initialized_ = true;
    }

    void OnTickPhase(engine::world::TickPhase phase, float /*fixedDt*/) override {
        if (!initialized_) {
            OnInitializeWorld();
        }
        if (phase == engine::world::TickPhase::PostTickCleanup) {
            sim_.Update();
        }
    }

    uint64_t ComputeDeterministicHash() const override {
        return sim_.HashState();
    }

    void OnShutdownWorld() override {
        if (initialized_) {
            session_.TeardownCurrentSession(mpq_, sim_, scene_, presentation_);
            initialized_ = false;
        }
    }

    engine::TagId GetChunkTag() const override {
        return saveContributor_.GetChunkTag();
    }

    uint32_t GetChunkVersion() const override {
        return saveContributor_.GetChunkVersion();
    }

    void WriteChunk(engine::world::BinaryWriter& writer) const override {
        saveContributor_.WriteChunk(writer);
    }

    bool ReadChunk(engine::world::BinaryReader& reader, uint32_t version) override {
        return saveContributor_.ReadChunk(reader, version);
    }

private:
    MpqManager mpq_;
    CSimulation sim_;
    SceneManager scene_;
    UnitPresentationSystem presentation_;
    core::WorldSceneSession session_;
    std::shared_ptr<war3::War3GameModule> war3Module_;
    projects::war3::War3SaveContributor saveContributor_;
    std::string scenePath_;
    bool initialized_ = false;
};

} // namespace

WARSMASH_PLUGIN_EXPORT const char* WarsmashGetModulePluginId() {
    return war3::War3GameModule::kModuleId;
}

WARSMASH_PLUGIN_EXPORT engine::ISimulationModule* WarsmashCreateSimulationModule() {
    return new war3::War3GameModule();
}

WARSMASH_PLUGIN_EXPORT void WarsmashDestroySimulationModule(engine::ISimulationModule* instance) {
    delete instance;
}

WARSMASH_PLUGIN_EXPORT engine::IGameModule* WarsmashCreateGameModule() {
    return new war3::War3GameModule();
}

WARSMASH_PLUGIN_EXPORT void WarsmashDestroyGameModule(engine::IGameModule* instance) {
    delete instance;
}

WARSMASH_PLUGIN_EXPORT const char* EngineGetExtensionPluginId() {
    return war3::War3GameModule::kModuleId;
}

WARSMASH_PLUGIN_EXPORT void EngineRegisterVfsProviders(engine::vfs::VfsMountRegistry* vfs) {
    if (vfs) {
        projects::war3::MpqArchiveProvider::RegisterInVfs(*vfs);
    }
}

WARSMASH_PLUGIN_EXPORT engine::world::ISimulationExtension* EngineCreateSimulationExtension() {
    return new War3SimulationExtension();
}

WARSMASH_PLUGIN_EXPORT void EngineDestroySimulationExtension(engine::world::ISimulationExtension* ext) {
    delete ext;
}
