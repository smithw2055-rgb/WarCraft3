#pragma once
// Minimal melee AI: keeps an economy running (workers harvest, trains
// replacements), expands supply/production, and sends attack waves.
// Race-agnostic: production roles are discovered from unit data (train
// lists, food output, harvest ability) instead of hardcoded rawcodes.
//
// Limitations (documented): no heroes/altar play, no research/upgrades,
// no static defense, undead/night-elf special harvest mechanics fall back
// to gold-mining when the standard HARVEST order is rejected.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "resource/slk.h"

class CSimulation;
class CUnit;
class CPlayer;
class CUnitType;
class COrder;

enum class AIDifficulty : int {
    EASY = 0,
    NORMAL = 1,
    INSANE = 2,
};

struct AIRacePlan {
    War3ID townHall;
    War3ID worker;
    War3ID farm;
    War3ID barracks;
    War3ID armyUnit;
    bool valid = false;
};

class MeleeAIController {
public:
    MeleeAIController(CSimulation* sim, int playerIndex);

    void SetDifficulty(int difficulty);
    int GetDifficulty() const { return (int)difficulty_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool IsEnabled() const { return enabled_; }
    // CommandAI hook: records the last command; command == 1 forces an
    // early attack wave with whatever army exists (approximation, the
    // numeric protocol is engine-internal).
    void Command(int command, int data);
    int GetLastCommand() const { return lastCommand_; }

    // Called every sim tick; thinking is throttled internally (~1/sec).
    void Update();

    // Introspection for tests/debugging.
    int GetThinkCount() const { return thinkCount_; }
    int GetWaveNumber() const { return waveNumber_; }
    // Difficulty pacing (retail: higher difficulties react faster and
    // attack more often; NORMAL values preserve existing behavior).
    int GetThinkIntervalTicks() const;
    int GetWaveCooldownTicks() const;
    bool HasPlan() const { return plan_.valid; }
    const AIRacePlan& GetPlan() const { return plan_; }

private:
    void Think();
    void DiscoverPlan();
    CUnit* FindTownHall() const;
    CUnit* FindIdleWorker() const;
    // Builder acquisition: idle worker first, else pull the nearest worker
    // off harvest (a builder is never yanked). Without this, a fully
    // employed workforce never builds anything (harvest loops never idle).
    CUnit* FindWorkerForBuild() const;
    CUnit* FindNearestGoldMine(float x, float y) const;
    class CDestructable* FindNearestTree(float x, float y) const;
    CUnit* FindIdleProduction(const War3ID& buildingType) const;
    int CountWorkers() const;
    int CountArmy() const;
    int CountBuildings(const War3ID& type) const;
    bool TryTrainWorkers();
    bool TryBuildSupply();
    bool TryBuildBarracks();
    bool TryTrainArmy();
    // Research one affordable upgrade per think on an idle production
    // building (ref gap: the AI never researched). Economy buffer scales
    // with difficulty so research never starves the army.
    bool TryResearchUpgrades();
    bool TryAttackWave(bool force);
    void AssignIdleWorkers();
    bool FindBuildSpot(float nearX, float nearY, const War3ID& buildingType,
                        float& outX, float& outY) const;
    bool FindAttackTarget(float& outX, float& outY) const;
    bool IsEnemyPlayer(int index) const;
    static bool IsWorkerType(const CUnitType* type);
    static bool IsCombatType(const CUnitType* type);
    // Route one AI decision through the sim lockstep log (replayable,
    // UI-visible, and identical on every peer since the AI is
    // deterministic). Falls back to a direct issue when sim_ is null.
    void IssueAiOrder(CUnit* unit, std::unique_ptr<COrder> order);

    CSimulation* sim_ = nullptr;
    int playerIndex_ = -1;
    AIDifficulty difficulty_ = AIDifficulty::NORMAL;
    bool enabled_ = true;
    int lastCommand_ = -1;
    int lastCommandData_ = 0;
    int tickCounter_ = 0;
    int thinkCount_ = 0;
    int waveNumber_ = 0;
    int lastWaveTick_ = -100000;
    AIRacePlan plan_;
};
