#pragma once
// ============================================================================
// Minimal RTS Verification Project: MinimalRtsModule
//
// Proves the engine foundation (`TagId`, `StaticTypeId`, `ProjectManifest`,
// `ISimulationExtension`, `DeterministicSaveArchive`, `NullRhiBackend`,
// `HudViewModel`) hosts a completely non-Warcraft RTS game with zero Warcraft
// headers, zero RTTI, strict team bounds checking, and structured JSON loading.
// ============================================================================
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "engine/core/tag_id.h"
#include "engine/world/simulation_extension.h"

namespace projects::minimal_rts {

enum class UnitRole : uint8_t {
    Dropoff = 0,
    ResourceNode = 1,
    Harvester = 2,
    Combat = 3
};

struct UnitPrototype {
    engine::TagId id;
    std::string name;
    UnitRole role = UnitRole::Combat;
    int32_t maxHp = 100;
    float moveSpeed = 180.0f;
    int32_t attackDamage = 10;
    float attackRange = 128.0f;
    int32_t carryCapacity = 0;
};

using EntityId = uint32_t;
inline constexpr EntityId kNullEntity = 0u;

struct EntityRecord {
    EntityId id = kNullEntity;
    engine::TagId protoId;
    uint16_t teamId = 0;
    UnitRole role = UnitRole::Combat;
    float x = 0.0f;
    float y = 0.0f;
    int32_t hp = 100;
    int32_t maxHp = 100;
    int32_t carriedCrystals = 0;
    int32_t remainingYield = 0;
    bool alive = true;

    // Order state
    enum class OrderKind : uint8_t {
        Idle = 0,
        Move = 1,
        Harvest = 2,
        ReturnCargo = 3,
        AttackMove = 4
    };
    OrderKind order = OrderKind::Idle;
    float targetX = 0.0f;
    float targetY = 0.0f;
    EntityId targetEntity = kNullEntity;
    float cooldownSec = 0.0f;
};

struct MinimalRtsCommand {
    uint64_t executeTick = 0;
    uint16_t teamId = 0;
    EntityId entityId = kNullEntity;
    EntityRecord::OrderKind order = EntityRecord::OrderKind::Idle;
    float targetX = 0.0f;
    float targetY = 0.0f;
    EntityId targetEntity = kNullEntity;
};

class MinimalRtsModule final : public engine::world::ISimulationExtension {
public:
    static constexpr const char* kModuleId = "MinimalRtsModule";
    static constexpr uint16_t kMaxTeams = 4u;

    MinimalRtsModule();
    ~MinimalRtsModule() override = default;

    const char* GetExtensionId() const override { return kModuleId; }

    // Structured JSON prototype loader via engine::detail::MiniJsonParser
    bool LoadPrototypesFromJson(std::string_view jsonText);
    bool ConfigureFromProjectData(std::string_view sceneOrDataJson) override;

    size_t PrototypeCount() const { return prototypes_.size(); }
    const UnitPrototype* FindPrototype(engine::TagId id) const;

    // Bounds-checked entity spawning (`teamId >= kMaxTeams` returns `kNullEntity`)
    EntityId SpawnEntity(engine::TagId protoId, uint16_t teamId, float x, float y);
    const EntityRecord* GetEntity(EntityId id) const;
    size_t AliveEntityCount() const;

    void QueueCommand(const MinimalRtsCommand& cmd);
    void StepSingleTick(float fixedDt = 0.05f);

    int32_t GetTeamCrystals(uint16_t teamId) const {
        return (teamId < kMaxTeams) ? teamCrystals_[teamId] : 0;
    }
    void SetTeamCrystals(uint16_t teamId, int32_t amount) {
        if (teamId < kMaxTeams) {
            teamCrystals_[teamId] = amount;
        }
    }

    uint64_t CurrentTick() const { return tickCount_; }
    int WinningTeam() const { return winningTeam_; }

    // ISimulationExtension lifecycle
    void OnInitializeWorld() override;
    void OnTickPhase(engine::world::TickPhase phase, float fixedDt) override;
    uint64_t ComputeDeterministicHash() const override;

    // ISaveChunkContributor ("MRTS")
    engine::TagId GetChunkTag() const override { return engine::TagId("MRTS"); }
    uint32_t GetChunkVersion() const override { return 1u; }
    void WriteChunk(engine::world::BinaryWriter& writer) const override;
    bool ReadChunk(engine::world::BinaryReader& reader, uint32_t version) override;

    static void EnsureRegistered();

private:
    EntityRecord* FindMutableEntity(EntityId id);
    EntityId FindNearestByRole(uint16_t teamFilter, bool matchTeam, UnitRole role, float x, float y) const;

    std::unordered_map<engine::TagId, UnitPrototype, engine::TagIdHasher> prototypes_;
    std::vector<EntityRecord> entities_;
    std::vector<MinimalRtsCommand> commandQueue_;
    std::array<int32_t, kMaxTeams> teamCrystals_{};
    EntityId nextEntityId_ = 1u;
    uint64_t tickCount_ = 0u;
    int winningTeam_ = -1;
};

} // namespace projects::minimal_rts
