#pragma once
// ============================================================================
// Phase 13, 14, 15 & 16 Engine Foundation: engine::ISimulationModule & NullSimulationModule
//
// Decouples the deterministic ECS world kernel (CSimulation), generic entity
// class (CUnit), generic player state container (CPlayer), combat engine
// (CCombatSystem), tactical AI host, item bonus pipeline, and campaign hero
// carry-over serializer from game-specific WarCraft 3 rulesets (such as
// War3GameModule, a custom sci-fi RTS module, or a headless pure-ECS harness).
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
class CAbility;
struct CarriedHero;

namespace engine {

struct SimulationModuleStats {
    uint64_t unitsAssembled = 0;
    uint64_t abilitiesAttached = 0;
    uint64_t customSpellsRegistered = 0;
    uint64_t customAutocastRegistered = 0;
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
    uint64_t preMitigationEvaluations = 0;
    uint64_t postMitigationEvaluations = 0;
    uint64_t postDamageReactions = 0;
    uint64_t bountiesAwarded = 0;
    uint64_t heroXpDistributions = 0;
    uint64_t autocastEvaluations = 0;
    uint64_t autocastOrdersIssued = 0;
    uint64_t itemBonusesApplied = 0;
    uint64_t heroesCarriedOver = 0;
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

    virtual void OnUnitAbilityAdded(CSimulation& /*sim*/,
                                    CUnit& /*unit*/,
                                    CAbility& /*ability*/) {}

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

    // ========================================================================
    // Phase 16: Unit Combat Reactions, Passive Procs, Kill Bounty & Hero XP,
    //           Idle Spell Autocast, Item Bonuses & Campaign Hero Carry-Over
    // ========================================================================

    // Evaluates pre-mitigation miss rolls (Drunken Haze, Curse, Highland cliff miss),
    // Mana Shield absorption, attacker damage-dealt modifiers (Howl of Terror, AIdd),
    // and Avatar spell immunity before CCombatSystem::CalculateDamage.
    // Returns false if the attack is completely missed/negated; otherwise updates inOutDamage.
    virtual bool OnEvaluatePreMitigationDamage(CSimulation& /*sim*/,
                                               CUnit* /*attacker*/,
                                               CUnit& /*target*/,
                                               bool /*isAttack*/,
                                               bool /*isRanged*/,
                                               int /*attackTypeOrd*/,
                                               int /*damageTypeOrd*/,
                                               bool /*invulnerableTarget*/,
                                               float& /*inOutDamage*/) {
        ++stats_.preMitigationEvaluations;
        return true;
    }

    // Modifies post-mitigation damage (Defend 0.5x, Berserk taken multiplier, Runed Bracers).
    virtual void OnModifyPostMitigationDamage(CSimulation& /*sim*/,
                                              CUnit* /*attacker*/,
                                              CUnit& /*target*/,
                                              bool /*isAttack*/,
                                              bool /*isRanged*/,
                                              int /*attackTypeOrd*/,
                                              int /*damageTypeOrd*/,
                                              float& /*inOutActualDamage*/) {
        ++stats_.postMitigationEvaluations;
    }

    // Executes post-hit combat reactions & passive procs (Sleep wake-up, Thorns/Spiked Carapace
    // melee reflection, Vampiric Aura lifesteal, Bash stun proc, Orb/Arrow attack modifiers,
    // Cleaving Attack splash, Wind Walk backstab break, and Pulverize AoE slam).
    virtual void OnPostDamageLanded(CSimulation& /*sim*/,
                                    CUnit* /*attacker*/,
                                    CUnit& /*target*/,
                                    bool /*isAttack*/,
                                    bool /*isRanged*/,
                                    int /*attackTypeOrd*/,
                                    int /*damageTypeOrd*/,
                                    float /*incomingBaseDamage*/,
                                    float /*incomingBaseBonus*/,
                                    float /*actualDamage*/,
                                    bool /*wasEvaded*/,
                                    bool /*invulnerableTarget*/) {
        ++stats_.postDamageReactions;
    }

    // Awards gold/lumber kill bounty dice to the killer's owner and splits Hero XP
    // among nearby allied heroes (or globally when globalExperience is active).
    virtual void OnAwardKillBountyAndXP(CSimulation& /*sim*/,
                                        CUnit* /*killer*/,
                                        CUnit& /*victim*/) {}

    // Evaluates idle spell autocast target acquisition (heals on lowest-HP ally, debuffs on nearest enemy).
    virtual bool OnTryUnitIdleAutocast(CSimulation& /*sim*/, CUnit& /*unit*/) {
        ++stats_.autocastEvaluations;
        return false;
    }

    // Phase 18: Resolves the autocast acquisition kind (0 = NONE, 1 = LOWEST_HP_ALLY, 2 = NEAREST_ENEMY)
    // for a given ability rawcode.
    virtual int GetAbilityAutocastKind(uint32_t /*abilityRawcode*/) const {
        return 0;
    }

    // Negative buff duration multiplier (e.g., 0.25x when Resistant Skin 'Brsk' is active).
    virtual float GetNegativeBuffDurationMultiplier(const CUnit& /*target*/) const {
        return 1.0f;
    }

    // Spell Shield reaction check (returns false when a spell shield 'BNss' blocks the spell).
    virtual bool OnCheckSpellEffectReaction(CSimulation& /*sim*/,
                                            CUnit& /*target*/,
                                            CUnit* /*caster*/) {
        return true;
    }

    // Applies or removes an item stat bonus on a unit (routes STR/AGI/INT through Hero ability in War3 mode).
    virtual void OnApplyUnitItemBonus(CSimulation& /*sim*/,
                                      CUnit& /*unit*/,
                                      int /*bonusTypeOrd*/,
                                      int /*signedDelta*/) {
        ++stats_.itemBonusesApplied;
    }

    // Campaign Hero Carry-Over hooks (SnapshotHero / RespawnCarriedHero)
    virtual bool IsCarriableHero(const CSimulation& /*sim*/, CUnit& /*unit*/) const {
        return false;
    }
    virtual bool OnSnapshotCarriedHero(const CSimulation& /*sim*/,
                                       CUnit& /*unit*/,
                                       CarriedHero& /*outCarried*/) const {
        return false;
    }
    virtual CUnit* OnRespawnCarriedHero(CSimulation& /*sim*/,
                                        const CarriedHero& /*carried*/,
                                        float /*x*/,
                                        float /*y*/) {
        return nullptr;
    }

    const SimulationModuleStats& GetModuleStats() const { return stats_; }
    void ResetModuleStats() { stats_ = {}; }

protected:
    mutable SimulationModuleStats stats_{};
};

// Pure Engine Mode module: creates bare ECS entities without any WarCraft 3
// race/worker/building abilities, food accounting, techtree equivalences,
// upgrade mutations, upkeep taxes, melee victory/defeat rules, War3
// armor/attack counter multipliers, or War3 passive/buff combat procs.
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
