#pragma once
// ============================================================================
// Stage L: Data-Oriented ECS Hot-Pool Synchronization & Deterministic
//          Full-State Snapshot Serializer (Unity DOTS / Rollback Netcode Style)
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ecs/csimulation.h"
#include "ecs/unit_components.h"

struct SimHotFleetMetrics {
    size_t activeUnitCount = 0;
    float centroidX = 0.0f;
    float centroidY = 0.0f;
    float totalAliveLife = 0.0f;
    float totalMana = 0.0f;
};

class SimHotComponentSystem {
public:
    // Synchronize all live CUnits in CSimulation into contiguous SparseComponentPools
    // (`UnitHotTransformComponent` & `UnitHotVitalsComponent`) in SimEcsRegistry.
    static size_t SyncUnitsToHotPools(CSimulation& sim);

    // Write back mutated hot components (`UnitHotTransformComponent` & `UnitHotVitalsComponent`)
    // from contiguous SimEcsRegistry pools to live CUnits and rebuild SpatialHashGrid.
    static size_t ApplyHotPoolsToUnits(CSimulation& sim);

    // High-throughput cache-friendly linear scan over contiguous hot component arrays
    // without chasing CUnit* heap pointers (playerFilter < 0 queries all players).
    static SimHotFleetMetrics ComputeFleetMetricsFromHotPools(const SimEcsRegistry& reg,
                                                              int playerFilter = -1);
};

struct SimPlayerSnapshot {
    int playerId = 0;
    int gold = 0;
    int lumber = 0;
    int foodUsed = 0;
    int foodCap = 0;
    int heroTokens = 0;
    int goldUpkeepRate = 0;
    int lumberUpkeepRate = 0;
};

struct SimUnitSnapshot {
    int handleId = 0;
    uint32_t typeId = 0;
    int playerIndex = 0;
    float x = 0.0f;
    float y = 0.0f;
    float facingDeg = 0.0f;
    float flyHeight = 0.0f;
    float life = 0.0f;
    int baseMaxLife = 0;
    float bonusMaxLife = 0.0f;
    float mana = 0.0f;
    int maxMana = 0;
    int defense = 0;
    int baseSpeed = 270;
    bool isDead = false;
    bool isHidden = false;
    bool isInvulnerable = false;
    bool isPaused = false;
    bool pathingEnabled = true;
    uint16_t stateFlagsBitmask = kDefaultUnitStateFlags;

    bool hasVisualComp = false;
    UnitVisualComponent visualComp{};

    bool hasConstructionComp = false;
    UnitConstructionComponent constructionComp{};

    bool hasStructureNavComp = false;
    UnitStructureNavComponent structureNavComp{};
};

struct SimItemSnapshot {
    int handleId = 0;
    uint32_t typeId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float life = 100.0f;
    float maxLife = 100.0f;
    int charges = 0;
    bool hidden = false;
    bool invulnerable = false;
};

struct SimDestructableSnapshot {
    int handleId = 0;
    uint32_t typeId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float life = 100.0f;
    float maxLife = 100.0f;
    float occluderHeight = 0.0f;
    bool dead = false;
    bool invulnerable = false;
};

class SimStateSnapshot {
public:
    static constexpr uint32_t kMagic = 0x53533357u; // "W3SS"
    static constexpr uint32_t kVersion = 1u;

    int32_t gameTurnTick = 0;
    uint32_t simRandState = 0;
    uint32_t combatRandState = 0;
    float gameTimeOfDay = 12.0f;
    bool daytime = true;
    bool timeOfDaySuspended = false;
    float timeOfDayScale = 1.0f;
    bool heroXPSuspended = false;
    bool paused = false;
    uint64_t stateHash = 0;

    std::vector<SimPlayerSnapshot> players;
    std::vector<SimUnitSnapshot> units;
    std::vector<SimItemSnapshot> items;
    std::vector<SimDestructableSnapshot> destructables;
    std::vector<CProjectile> projectiles;

    // Capture full deterministic state from a live CSimulation
    static SimStateSnapshot Capture(const CSimulation& sim);

    // Restore a CSimulation to this exact snapshot state (recreating removed units
    // and pruning units spawned after the snapshot) so sim.HashState() matches 100%.
    bool Restore(CSimulation& sim) const;

    // Binary wire/disk serialization for SaveGame/LoadGame & Lockstep Resync
    std::vector<uint8_t> SerializeBinary() const;
    static bool DeserializeBinary(const uint8_t* data, size_t size, SimStateSnapshot& out);
};
