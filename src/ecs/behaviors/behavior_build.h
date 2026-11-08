#pragma once
#include "ecs/behaviors/behavior.h"
#include "resource/slk.h"
#include <deque>

#include "ecs/ability.h"

// Build construction model
enum class CBuildModel : uint8_t {
    HUMAN,      // Worker walks to site, creates structure, then repairs it
    ORC,        // Worker walks to site, creates structure, hides inside
    UNDEAD,     // Worker walks to site, creates structure, stays outside working
    NIGHT_ELF   // Worker walks to site, creates structure, hides inside (ancient)
};

// CBehaviorBuild - worker moves to build site and constructs a building
class CBehaviorBuild : public CBehavior {
public:
    CBehaviorBuild();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::BUILD; }

    void SetBuildTarget(float x, float y, War3ID buildingTypeId, CBuildRace race);
    bool IsMovingToSite() const { return movingToSite_; }
    bool IsMovingToWorkSite() const override { return movingToSite_; }
    bool IsActivelyBuilding() const { return building_ && !completed_; }
    bool IsActivelyWorking() const override { return IsActivelyBuilding(); }
    bool TracksPropellingState() const override { return true; }
    bool IsPropelling() const override { return propelling_; }
    // ESC-cancel (ref construction cancel): full refund, foundation removed,
    // worker released; no-op once completed.
    void CancelConstruction(CSimulation& simulation);
    bool TryCancelConstruction(CSimulation& simulation) override {
        if (IsActivelyBuilding() || IsMovingToSite()) {
            CancelConstruction(simulation);
            return true;
        }
        return false;
    }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    void CreateBuilding(CSimulation& simulation);
    void CompleteBuilding(CSimulation& simulation);

    float buildX_ = 0, buildY_ = 0;
    War3ID buildingTypeId_;
    CBuildRace race_;
    CBuildModel buildModel_;

    // Build state
    bool movingToSite_ = true;
    bool building_ = false;
    bool completed_ = false;
    float constructionProgress_ = 0;
    float buildTime_ = 0;
    int createdUnitHandle_ = -1;

    // Pathfinding
    std::deque<std::pair<float,float>> waypoints_;
    int pathRecalcCooldown_ = 0;
    bool propelling_ = false;
};
