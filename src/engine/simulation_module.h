#pragma once
// ============================================================================
// Phase 13, 14 & 15 Engine Foundation: engine::ISimulationModule & NullSimulationModule
//
// Decouples the deterministic ECS world kernel (CSimulation), generic player
// state container (CPlayer), combat engine (CCombatSystem), and tactical AI
// host from game-specific unit/ability/economy/techtree/upgrade/victory-defeat/
// armor-counter/AI rulesets (such as War3GameModule, a custom sci-fi RTS
// module, or a headless pure-ECS benchmark harness).
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class CSimulation;
class CPlayer;
class CUnit;
class CUnitType;
class CItem;
class CDestructable;
class CCombatSystem;

namespace engine {

struct SimulationModuleStats {
    uint64_t unitsAssembled = 0;
    uint64_t abilitiesAttached = 0;
    uint64_t itemsConfigured = 0;
    uint64_t destructablesConfigured = 0;
    uint64_t unitsCommitted = 0;
    uint64_t unitsRemoved = 0;
    uint64_t unitsDied = 0;
    uint64_t unitsMorphed = 0;
    uint64_t techEventsProcessed = 0;
    uint64_t unitUpgradesApplied = 0;
    uint64_t victoryEvaluations = 0;
    uint64_t combatDamageEvaluations = 0;
    uint64_t aiControllersCreated = 0;
    uint64_t aiControllerTicks = 0;
    uint64_t tacticalTargetEvaluations = 0;
    uint64_t attachCount = 0;
    uint64_t detachCount = 0;
};

class ISimulationModule {
public:
    virtual ~ISimulationModule() = default;

    virtual const char* GetModuleId() const = 0;
    virtual uint32_t GetModuleVersion() const { return 1; }

    // Lifecycle hooks when attached to or detached from a CSimulation world
    virtual void OnAttachToSimulation(CSimulation& /*sim*/) {
        ++stats_.attachCount;
    }
    virtual void OnDetachFromSimulation(CSimulation& /*sim*/) {
        ++stats_.detachCount;
    }

    // Dynamic ability catalog lifecycle
    virtual void OnRegisterAbilities() {}
    virtual void OnUnregisterAbilities() {}

    // Entity archetype assembly & configuration hooks invoked by CSimulation
    virtual void OnAssembleUnitAbilities(CSimulation& sim,
                                         CUnit& unit,
                                         const CUnitType& unitType) = 0;

    virtual void OnConfigureCreatedItem(CSimulation& /*sim*/, CItem& /*item*/) {}

    virtual void OnConfigureCreatedDestructable(CSimulation& /*sim*/,
                                                CDestructable& /*destructable*/,
                                                float /*initialLife*/,
                                                int /*rotationDeg*/) {}

    // World commit, death, morph & removal hooks (player possession, food cap, techtree unlock)
    virtual void OnUnitCommittedToWorld(CSimulation& /*sim*/, CUnit& /*unit*/) {}

    virtual void OnUnitDied(CSimulation& /*sim*/, CUnit& /*unit*/) {
        ++stats_.unitsDied;
    }

    virtual void OnUnitMorphed(CSimulation& /*sim*/,
                               CUnit& /*unit*/,
                               const CUnitType* /*oldType*/,
                               uint32_t /*oldTypeRawcode*/,
                               const CUnitType* /*newType*/,
                               uint32_t /*newTypeRawcode*/) {
        ++stats_.unitsMorphed;
    }

    virtual void OnUnitRemovedFromWorld(CSimulation& /*sim*/,
                                        CUnit& /*unit*/,
                                        bool /*wasActiveUnit*/) {}

    // ========================================================================
    // Phase 14: Player Techtree, Upgrade System & Upkeep Tax Rules
    // ========================================================================
    virtual int GetEffectiveTechLevel(const CSimulation& /*sim*/,
                                      int /*playerIndex*/,
                                      uint32_t /*techRawcode*/) const {
        return 0;
    }

    virtual bool MeetsTechRequirements(const CSimulation& /*sim*/,
                                       int /*playerIndex*/,
                                       uint32_t /*targetRawcode*/) const {
        return true;
    }

    virtual std::vector<uint32_t> GetMissingTechRequirements(const CSimulation& /*sim*/,
                                                             int /*playerIndex*/,
                                                             uint32_t /*targetRawcode*/) const {
        return {};
    }

    virtual void OnPlayerTechLevelChanged(CSimulation& /*sim*/,
                                          CPlayer& /*player*/,
                                          uint32_t /*techRawcode*/,
                                          int /*oldLevel*/,
                                          int /*newLevel*/) {
        ++stats_.techEventsProcessed;
    }

    virtual void OnApplyPlayerUpgradesToUnit(CSimulation& /*sim*/,
                                             CPlayer& /*player*/,
                                             CUnit& /*unit*/) {}

    virtual int ComputeUpkeepLevel(const CPlayer& /*player*/) const { return 0; }
    virtual int ApplyUpkeepGoldTax(const CPlayer& /*player*/, int amount) const { return amount; }

    // ========================================================================
    // Phase 14: Victory / Defeat & Crippled Evaluation Rules
    // ========================================================================
    virtual void EvaluateVictoryDefeat(CSimulation& /*sim*/) {
        ++stats_.victoryEvaluations;
    }

    virtual bool IsPlayerCrippled(const CSimulation& /*sim*/, int /*playerIndex*/) const {
        return false;
    }

    virtual void OnPlayerRemoved(CSimulation& /*sim*/, int /*playerIndex*/, int /*gameResultCode*/) {}

    // ========================================================================
    // Phase 15: Combat Armor/Attack Type Multiplier Matrix & Damage Rules
    // ========================================================================
    virtual void OnConfigureCombatSystem(CSimulation& /*sim*/, CCombatSystem& /*combat*/) {}

    // Returns the damage multiplier for (attackTypeOrd, defenseTypeOrd).
    // In Pure Engine Mode (NullSimulationModule), returns 1.0f for all combinations.
    virtual float GetAttackDefenseMultiplier(int /*attackTypeOrd*/, int /*defenseTypeOrd*/) const {
        return 1.0f;
    }

    // Computes fractional armor damage reduction in [-inf, 0.95f] for normal physical damage.
    // In Pure Engine Mode (NullSimulationModule), returns 0.0f.
    virtual float ComputeArmorDamageReduction(int /*defense*/,
                                              int /*attackerLevel*/,
                                              float /*defenseArmorFactor*/) const {
        ++stats_.combatDamageEvaluations;
        return 0.0f;
    }

    // Evaluates whether a magic-immune target completely resists an incoming attack/spell.
    virtual bool ShouldResistMagicDamage(int /*attackTypeOrd*/,
                                         int /*damageTypeOrd*/,
                                         bool /*targetIsMagicImmune*/,
                                         bool /*magicImmuneResistsDamage*/,
                                         bool /*magicImmuneResistsUltimates*/) const {
        return false;
    }

    // Evaluates Ethereal damage amplification / physical immunity.
    // Returns false if the attack is completely negated by Ethereal state; otherwise sets outMultiplier.
    virtual bool EvaluateEtherealDamageFactor(int /*attackTypeOrd*/,
                                              int /*damageTypeOrd*/,
                                              bool /*targetIsEthereal*/,
                                              bool /*allied*/,
                                              bool /*etherealBonusAlly*/,
                                              float /*etherealBonusMagic*/,
                                              float /*etherealBonusSpells*/,
                                              float& outMultiplier) const {
        outMultiplier = 1.0f;
        return true;
    }

    // ========================================================================
    // Phase 15: Tactical AI Controller Lifecycle & Counter-Scoring Rules
    // ========================================================================
    virtual bool CreatePlayerAI(CSimulation& /*sim*/, int /*playerIndex*/, int /*difficulty*/) {
        return false;
    }
    virtual bool RemovePlayerAI(int /*playerIndex*/) { return false; }
    virtual void SetPlayerAIEnabled(int /*playerIndex*/, bool /*enabled*/) {}
    virtual void CommandPlayerAI(int /*playerIndex*/, int /*command*/, int /*data*/) {}
    virtual void TickPlayerAIs(CSimulation& /*sim*/, float /*dt*/) {}

    // Evaluates tactical target priority score (adds Attack-vs-Defense counter bonus in War3GameModule).
    virtual float EvaluateTacticalTargetScore(const CSimulation& /*sim*/,
                                              const CUnit& /*attacker*/,
                                              const CUnit& /*candidateTarget*/,
                                              float baseScore) const {
        ++stats_.tacticalTargetEvaluations;
        return baseScore;
    }

    const SimulationModuleStats& GetModuleStats() const { return stats_; }
    void ResetModuleStats() { stats_ = {}; }

protected:
    mutable SimulationModuleStats stats_{};
};

// Pure Engine Mode module: creates bare ECS entities without any WarCraft 3
// race/worker/building abilities, food accounting, techtree equivalences,
// upgrade mutations, upkeep taxes, melee victory/defeat rules, or War3
// armor/attack counter multipliers.
class NullSimulationModule final : public ISimulationModule {
public:
    const char* GetModuleId() const override { return "NullPureEngineModule"; }

    void OnAssembleUnitAbilities(CSimulation& /*sim*/,
                                 CUnit& /*unit*/,
                                 const CUnitType& /*unitType*/) override {
        ++stats_.unitsAssembled;
    }
};

} // namespace engine
