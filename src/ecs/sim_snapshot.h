#pragma once
// ============================================================================
// Stage L & Phase 25: Data-Oriented ECS Hot-Pool Synchronization & Deterministic
//                     Full-State Snapshot Serializer (Unity DOTS / Rollback Style)
//
// Phase 25 Improvements:
// 1. Trivially-copyable `SimProjectileSnapshot` wire struct eliminates undefined
//    behavior from raw `memcpy` on `CProjectile` (`std::string` + `std::function`).
// 2. Compact bitfields on `SimUnitSnapshot`, `SimItemSnapshot`, and
//    `SimDestructableSnapshot` reduce per-entity wire footprint.
// 3. Pluggable `moduleStateBytes` (`ISimulationModule::OnCaptureModuleSnapshot` /
//    `OnRestoreModuleSnapshot`) serializes module-level runtime state across resyncs.
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
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
    static size_t SyncUnitsToHotPools(CSimulation& sim);
    static size_t ApplyHotPoolsToUnits(CSimulation& sim);
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
    uint16_t stateFlagsBitmask = kDefaultUnitStateFlags;

    // Phase 25: 8 boolean snapshot flags packed into 1 byte (saves 8 bytes per unit snapshot)
    uint8_t isDead : 1;
    uint8_t isHidden : 1;
    uint8_t isInvulnerable : 1;
    uint8_t isPaused : 1;
    uint8_t pathingEnabled : 1;
    uint8_t hasVisualComp : 1;
    uint8_t hasConstructionComp : 1;
    uint8_t hasStructureNavComp : 1;
    uint8_t reservedPad_ = 0;

    UnitVisualComponent visualComp{};
    UnitConstructionComponent constructionComp{};
    UnitStructureNavComponent structureNavComp{};

    SimUnitSnapshot()
        : isDead(0)
        , isHidden(0)
        , isInvulnerable(0)
        , isPaused(0)
        , pathingEnabled(1)
        , hasVisualComp(0)
        , hasConstructionComp(0)
        , hasStructureNavComp(0) {}
};

struct SimItemSnapshot {
    int handleId = 0;
    uint32_t typeId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float life = 100.0f;
    float maxLife = 100.0f;
    int charges = 0;
    uint8_t hidden : 1;
    uint8_t invulnerable : 1;
    uint8_t reservedFlags : 6;

    SimItemSnapshot() : hidden(0), invulnerable(0), reservedFlags(0) {}
};

struct SimDestructableSnapshot {
    int handleId = 0;
    uint32_t typeId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float life = 100.0f;
    float maxLife = 100.0f;
    float occluderHeight = 0.0f;
    uint8_t dead : 1;
    uint8_t invulnerable : 1;
    uint8_t hidden : 1;
    uint8_t blighted : 1;
    uint8_t reservedFlags : 4;

    SimDestructableSnapshot() : dead(0), invulnerable(0), hidden(0), blighted(0), reservedFlags(0) {}
};

// Trivially-copyable POD wire representation of CProjectile for binary snapshots
// (safe to memcpy over network / disk without std::string or std::function corruption).
struct SimProjectileSnapshot {
    int attackerHandleId = -1;
    int targetHandleId = -1;
    float x = 0.0f;
    float y = 0.0f;
    float speed = 0.0f;
    float damage = 0.0f;
    uint32_t projectileTypeRawcode = 0;
    uint8_t attackTypeOrd = 0;
    uint8_t damageTypeOrd = 0;
    uint8_t flagsBitmask = 0;
    uint8_t reservedPad = 0;
    float maxTravel = 0.0f;
    float traveled = 0.0f;
    float splashFull = 0.0f;
    float splashHalf = 0.0f;
    float splashQuarter = 0.0f;
    float splashFactorHalf = 0.5f;
    float splashFactorQuarter = 0.25f;
    float splashBonus = 0.0f;
    int bouncesLeft = 0;
    float bounceRange = 0.0f;
    float bounceLossFactor = 1.0f;
    float tx = 0.0f;
    float ty = 0.0f;
    float fx = 0.0f;
    float fy = 0.0f;
    float spillDistance = 0.0f;
    float spillRadius = 0.0f;
    char weaponSound[32]{};

    static SimProjectileSnapshot FromProjectile(const CProjectile& p) {
        SimProjectileSnapshot s;
        s.attackerHandleId = p.attackerHandleId;
        s.targetHandleId = p.targetHandleId;
        s.x = p.x;
        s.y = p.y;
        s.speed = p.speed;
        s.damage = p.damage;
        s.projectileTypeRawcode = p.projectileTypeId.value;
        s.attackTypeOrd = static_cast<uint8_t>(p.attackType);
        s.damageTypeOrd = static_cast<uint8_t>(p.damageType);
        s.flagsBitmask = (p.isAttack ? 1u : 0u) |
                         (p.isRanged ? 2u : 0u) |
                         (p.groundTarget ? 4u : 0u) |
                         (p.checkEffectReaction ? 8u : 0u) |
                         (p.pierceReflected ? 16u : 0u) |
                         (p.customImpactOnly ? 32u : 0u);
        s.maxTravel = p.maxTravel;
        s.traveled = p.traveled;
        s.splashFull = p.splashFull;
        s.splashHalf = p.splashHalf;
        s.splashQuarter = p.splashQuarter;
        s.splashFactorHalf = p.splashFactorHalf;
        s.splashFactorQuarter = p.splashFactorQuarter;
        s.splashBonus = p.splashBonus;
        s.bouncesLeft = p.bouncesLeft;
        s.bounceRange = p.bounceRange;
        s.bounceLossFactor = p.bounceLossFactor;
        s.tx = p.tx;
        s.ty = p.ty;
        s.fx = p.fx;
        s.fy = p.fy;
        s.spillDistance = p.spillDistance;
        s.spillRadius = p.spillRadius;
        if (!p.weaponSound.empty()) {
            std::strncpy(s.weaponSound, p.weaponSound.c_str(), sizeof(s.weaponSound) - 1);
            s.weaponSound[sizeof(s.weaponSound) - 1] = '\0';
        }
        return s;
    }

    CProjectile ToProjectile() const {
        CProjectile p;
        p.attackerHandleId = attackerHandleId;
        p.targetHandleId = targetHandleId;
        p.x = x;
        p.y = y;
        p.speed = speed;
        p.damage = damage;
        p.projectileTypeId = War3ID(projectileTypeRawcode);
        p.attackType = static_cast<CAttackType>(attackTypeOrd);
        p.damageType = static_cast<CDamageType>(damageTypeOrd);
        p.isAttack = (flagsBitmask & 1u) != 0;
        p.isRanged = (flagsBitmask & 2u) != 0;
        p.groundTarget = (flagsBitmask & 4u) != 0;
        p.checkEffectReaction = (flagsBitmask & 8u) != 0;
        p.pierceReflected = (flagsBitmask & 16u) != 0;
        p.customImpactOnly = (flagsBitmask & 32u) != 0;
        p.maxTravel = maxTravel;
        p.traveled = traveled;
        p.splashFull = splashFull;
        p.splashHalf = splashHalf;
        p.splashQuarter = splashQuarter;
        p.splashFactorHalf = splashFactorHalf;
        p.splashFactorQuarter = splashFactorQuarter;
        p.splashBonus = splashBonus;
        p.bouncesLeft = bouncesLeft;
        p.bounceRange = bounceRange;
        p.bounceLossFactor = bounceLossFactor;
        p.tx = tx;
        p.ty = ty;
        p.fx = fx;
        p.fy = fy;
        p.spillDistance = spillDistance;
        p.spillRadius = spillRadius;
        if (weaponSound[0] != '\0') {
            p.weaponSound = weaponSound;
        }
        return p;
    }
};
static_assert(std::is_trivially_copyable_v<SimProjectileSnapshot>,
              "SimProjectileSnapshot must be trivially copyable for binary wire serialization");

class SimStateSnapshot {
public:
    static constexpr uint32_t kMagic = 0x53533357u; // "W3SS"
    static constexpr uint32_t kVersion = 2u;

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
    std::vector<uint8_t> moduleStateBytes;

    // Capture full deterministic state from a live CSimulation
    static SimStateSnapshot Capture(const CSimulation& sim);

    // Restore a CSimulation to this exact snapshot state (recreating removed units
    // and pruning units spawned after the snapshot) so sim.HashState() matches 100%.
    bool Restore(CSimulation& sim) const;

    // Binary wire/disk serialization for SaveGame/LoadGame & Lockstep Resync
    std::vector<uint8_t> SerializeBinary() const;
    static bool DeserializeBinary(const uint8_t* data, size_t size, SimStateSnapshot& out);
};
