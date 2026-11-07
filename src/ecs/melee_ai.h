#pragma once
// Minimal melee AI: keeps an economy running (workers harvest, trains
// replacements), expands supply/production, trains and revives heroes,
// and sends attack waves.
// Race-agnostic: production roles are discovered from unit data (train
// lists, food output, harvest ability, hero flag) instead of hardcoded
// rawcodes.
//
// Limitations (documented): no expansion bases, no static defense,
// undead/night-elf special harvest mechanics fall back to gold-mining
// when the standard HARVEST order is rejected.

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
    War3ID altar;
    War3ID hero;
    War3ID tower;
    bool valid = false;
};

// ============================================================
// Stage Z2: Spatial Threat & Utility Grid (Godot LimboAI / RTS
// Influence & Threat Map architecture).
// ============================================================
struct TacticalThreatCell {
    float enemyThreat = 0.0f;      // Aggregated combat danger of enemy units in cell
    float friendlyPower = 0.0f;    // Aggregated combat strength of friendly units in cell
    float highValueScore = 0.0f;   // Priority score for wounded/high-value targets
    int enemyCount = 0;
    int friendlyCount = 0;
};

class TacticalThreatMap {
public:
    static constexpr int kDefaultGridSize = 32;

    TacticalThreatMap(int gridW = kDefaultGridSize, int gridH = kDefaultGridSize,
                      float minX = -8192.0f, float minY = -8192.0f,
                      float maxX = 8192.0f, float maxY = 8192.0f);

    void Rebuild(const CSimulation& sim, int aiPlayerIndex);
    const TacticalThreatCell& GetCellAtWorld(float x, float y) const;
    float SampleThreatAt(float x, float y) const;
    float SampleFriendlyPowerAt(float x, float y) const;

    // Utility AI target scoring: evaluates focus-fire priority of candidateTarget
    // relative to attacker (higher score = higher priority: low HP ratio, hero/combat threat, proximity).
    float ScoreTargetUtility(const CUnit* attacker, const CUnit* candidateTarget) const;

    // Computes a normalized safe retreat/kiting direction vector (outDirX, outDirY)
    // away from local enemy threat gradient toward safer / allied cells.
    bool ComputeRetreatVector(float unitX, float unitY, float& outDirX, float& outDirY) const;

    int GetGridWidth() const { return gridW_; }
    int GetGridHeight() const { return gridH_; }
    int GetActiveThreatCellCount() const { return activeThreatCells_; }

private:
    int WorldToCol(float x) const;
    int WorldToRow(float y) const;

    int gridW_ = kDefaultGridSize;
    int gridH_ = kDefaultGridSize;
    float minX_ = -8192.0f, minY_ = -8192.0f;
    float maxX_ = 8192.0f, maxY_ = 8192.0f;
    const CSimulation* boundSim_ = nullptr;
    std::vector<TacticalThreatCell> cells_;
    TacticalThreatCell emptyCell_{};
    int activeThreatCells_ = 0;
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

    // Stage Z2: Tactical Threat Map & Utility AI Micro APIs
    void RebuildThreatMap();
    const TacticalThreatMap& GetThreatMap() const { return threatMap_; }
    CUnit* EvaluateBestFocusTarget(const CUnit* attacker, float searchRadius = 900.0f) const;
    int ExecuteTacticalMicro(float retreatHpRatioThreshold = 0.30f, float retreatStepDist = 280.0f);
    int GetKitingRetreatCount() const { return kitingRetreatCount_; }
    int GetFocusFireOrderCount() const { return focusFireOrderCount_; }

    static bool IsWorkerType(const CUnitType* type);
    static bool IsCombatType(const CUnitType* type);
    static bool IsTowerType(const CUnitType* type);

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
    // Owned town halls of the plan type (main first, then expos).
    std::vector<CUnit*> FindOwnedHalls() const;
    // Live owned heroes (any hero type, not just the plan pick).
    int CountHeroes() const;
    // Hero cap by difficulty (EASY 1 / NORMAL 2 / INSANE 3).
    int MaxHeroes() const;
    // Dead owned hero awaiting revive, if any.
    CUnit* FindDeadHero() const;
    CUnit* FindAltar() const;
    bool TryTrainWorkers();
    bool TryBuildSupply();
    bool TryBuildBarracks();
    bool TryBuildAltar();
    bool TryTrainArmy();
    bool TryTrainHero();
    bool TryReviveHeroes();
    // Static defense: cheapest same-race armed building, ringed around the
    // town hall up to the difficulty cap. Ranged towers outrange melee
    // harass; melee-only battlements still body-block.
    bool TryBuildTowers();
    // Expansion: when the main mine runs dry, plant the town hall at the
    // nearest rich unowned mine and walk harvesters over. One expo at a
    // time; new halls join worker training automatically (FindOwnedHalls).
    bool TryExpand();
    // Nearest mine with gold that is not the excluded one (null to compare
    // all), skipping hidden/haunted backing mines like FindNearestGoldMine.
    CUnit* FindRichMine(float x, float y, const CUnit* exclude) const;
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

    // Stage Z2 state
    TacticalThreatMap threatMap_;
    int kitingRetreatCount_ = 0;
    int focusFireOrderCount_ = 0;
};
