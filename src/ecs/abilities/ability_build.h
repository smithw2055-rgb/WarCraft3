#pragma once
#include "ecs/ability.h"
#include <vector>
#include <unordered_set>

// Build race type
enum class CBuildRace : uint8_t {
    HUMAN, ORC, UNDEAD, NIGHT_ELF, NAGA, NEUTRAL
};

// CAbilityBuild - build structure ability
// Supports all races with race-specific behavior
class CAbilityBuild : public CAbility {
public:
    CAbilityBuild(int handleId, War3ID code, CBuildRace race);

    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;

    // Order validation
    bool CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const override;
    bool CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const override;

    // Begin building
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) override;
    CBehavior* BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) override;

    // Build menu: which structures can this ability build?
    void AddBuildableStructure(War3ID unitTypeId) { buildableStructures_.insert(unitTypeId); }
    const std::unordered_set<War3ID>& GetBuildableStructures() const { return buildableStructures_; }

    CBuildRace GetRace() const { return race_; }

    // Get the base order ID for this race's build menu
    int GetBaseOrderId() const;

    // Static: validate build location
    static bool IsBuildLocationObstructed(CSimulation& game, float x, float y,
                                           int playerIndex, War3ID buildingTypeId);

    // Static: snap build position to grid
    static void RoundBuildPosition(float& x, float& y);

private:
    CBuildRace race_;
    std::unordered_set<War3ID> buildableStructures_;
};
