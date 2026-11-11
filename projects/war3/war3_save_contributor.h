#pragma once
// ============================================================================
// WarCraft III Project Plugin: War3SaveContributor
//
// Bridges `CSimulation` + `SimStateSnapshot` (`"W3SS"`, including pluggable
// `War3GameModule` state bytes) into the generic `"ESAV"` chunked
// `engine::world::DeterministicSaveArchive` container.
// ============================================================================
#include <cstdint>
#include <string>
#include <vector>

#include "ecs/csimulation.h"
#include "ecs/sim_snapshot.h"
#include "engine/world/simulation_extension.h"

namespace projects::war3 {

class War3SaveContributor final : public engine::world::ISaveChunkContributor {
public:
    explicit War3SaveContributor(CSimulation* boundSim = nullptr)
        : sim_(boundSim) {}

    void BindSimulation(CSimulation* sim) { sim_ = sim; }

    engine::TagId GetChunkTag() const override { return engine::TagId("W3SM"); }
    uint32_t GetChunkVersion() const override { return 1u; }

    void WriteChunk(engine::world::BinaryWriter& writer) const override {
        if (!sim_) {
            writer.WriteU8(0u);
            return;
        }
        writer.WriteU8(1u);
        const SimStateSnapshot snap = SimStateSnapshot::Capture(*sim_);
        const std::vector<uint8_t> simBytes = snap.SerializeBinary();
        writer.WriteU32(static_cast<uint32_t>(simBytes.size()));
        if (!simBytes.empty()) {
            writer.WriteBytes(simBytes.data(), simBytes.size());
        }
    }

    bool ReadChunk(engine::world::BinaryReader& reader, uint32_t version) override {
        if (version != 1u) return false;
        uint8_t hasSim = 0;
        if (!reader.ReadU8(hasSim)) return false;
        if (hasSim == 0u) return true;
        if (!sim_) return false;

        uint32_t simLen = 0;
        if (!reader.ReadU32(simLen)) return false;
        std::vector<uint8_t> simBytes(simLen);
        if (simLen > 0 && !reader.ReadBytes(simBytes.data(), simLen)) return false;

        SimStateSnapshot snap;
        if (!SimStateSnapshot::DeserializeBinary(simBytes.data(), simBytes.size(), snap)) {
            return false;
        }
        return snap.Restore(*sim_);
    }

private:
    CSimulation* sim_ = nullptr;
};

} // namespace projects::war3
