#pragma once
// ============================================================================
// Phase 13, 14, 15 & 16 Modernization: war3::War3GameModule (src/game/war3/war3_game_module.h)
//
// Extracts WarCraft III's:
//   1. Unit/building/worker/hero ability assembly & ~190 spell/ability catalog
//   2. Item & destructable data binding
//   3. CPlayer techtree tier equivalences (htow<-hkee<-hcas, ogre<-ostr<-ofor,
//      unpl<-unp1<-unp2, etol<-etoa<-etoe), building morph tech transfer,
//      and live Upgrade Effect Engine (rarm, ratx/ratd/ratc, rhpx/rhpo,
//      rmnx/rmno, rmvx, rats, rlev)
//   4. Economy upkeep tax thresholds (0-50 none, 51-80 30%, 81+ 60%)
//   5. Melee & Allied Victory/Defeat & Crippled player evaluation rules
//   6. CCombatSystem 7x8 Attack-vs-Defense multiplier matrix, 0.06 armor
//      reduction curve, and Ethereal/Magic-Immunity rules
//   7. MeleeAIController tactical AI lifecycle, race build-order discovery,
//      and armor-counter tactical target scoring
//   8. [Phase 16] Unit combat reactions & passive procs (Drunken Haze/Curse/
//      Highland miss, Mana Shield, Howl of Terror, Avatar, Defend, Berserk,
//      Runed Bracers, Sleep wake, Thorns/Spiked Carapace reflection, Vampiric
//      lifesteal, Bash stun, Orb/Arrow attack modifiers, Cleaving Attack,
//      Wind Walk backstab, Pulverize AoE slam), Kill Bounty Dice & Hero XP
//      distribution, Idle Spell Autocast target acquisition, Item Attribute
//      Bonus routing, and Campaign Hero Carry-Over serialization
// out of CSimulation, CUnit, CPlayer, CCombatSystem, MeleeAIController,
// item_bonuses, and hero_carry into an independent, dynamically loadable
// plugin module implementing both:
//   - engine::ISimulationModule (plugs into CSimulation)
//   - engine::IGameModule       (plugs into engine::EngineKernel)
// ============================================================================
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "data/combat_types.h"
#include "ecs/buff.h"
#include "ecs/melee_ai.h"
#include "engine/dynamic_module_loader.h"
#include "engine/engine_kernel.h"
#include "engine/game_module.h"
#include "engine/simulation_module.h"
#include "resource/slk.h"

struct CUpgradeType;
struct ABSpellDescriptor;

namespace war3 {

class War3GameModule final : public engine::IGameModule, public engine::ISimulationModule {
public:
    static constexpr const char* kModuleId = "War3GameModule";

    War3GameModule();
    ~War3GameModule() override = default;

    // Shared default instance used when CSimulation boots in standard WarCraft 3 mode
    static std::shared_ptr<War3GameModule> GetSharedDefaultInstance();

    // Register / unregister "War3GameModule" in engine::DynamicModuleLoader
    static void EnsureRegisteredInDynamicLoader();

    // Register / unregister the ~190 WarCraft 3 abilities & canonical spell descriptors
    static void RegisterWar3AbilityCatalog();
    static size_t UnregisterWar3AbilityCatalog();

    // Phase 17: Register a custom data-driven ABSpellDescriptor scoped to War3GameModule
    void RegisterCustomSpellDescriptor(const ::ABSpellDescriptor& desc);
    size_t GetRegisteredAbilityCount() const;

    // --- engine::IGameModule interface ---
    const char* GetModuleName() const override { return kModuleId; }
    bool OnInit(engine::EngineKernel& kernel) override;
    void OnFixedTick(engine::EngineKernel& kernel, float fixedDt) override;
    void OnFrameUpdate(engine::EngineKernel& kernel, float dt, float interpolationAlpha) override;
    void OnShutdown(engine::EngineKernel& kernel) override;

    // --- engine::ISimulationModule interface ---
    const char* GetModuleId() const override { return kModuleId; }
    uint32_t GetModuleVersion() const override { return 28u; }

    void OnAttachToSimulation(CSimulation& sim) override;
    void OnDetachFromSimulation(CSimulation& sim) override;

    void OnRegisterAbilities() override;
    void OnUnregisterAbilities() override;

    void OnAssembleUnitAbilities(CSimulation& sim,
                                 CUnit& unit,
                                 const CUnitType& unitType) override;

    void OnUnitAbilityAdded(CSimulation& sim,
                            CUnit& unit,
                            CAbility& ability) override;

    void OnConfigureCreatedItem(CSimulation& sim, CItem& item) override;

    void OnConfigureCreatedDestructable(CSimulation& sim,
                                        CDestructable& destructable,
                                        float initialLife,
                                        int rotationDeg) override;

    void OnUnitCommittedToWorld(CSimulation& sim, CUnit& unit) override;

    void OnUnitDied(CSimulation& sim, CUnit& unit) override;

    void OnUnitMorphed(CSimulation& sim,
                       CUnit& unit,
                       const CUnitType* oldType,
                       uint32_t oldTypeRawcode,
                       const CUnitType* newType,
                       uint32_t newTypeRawcode) override;

    void OnUnitRemovedFromWorld(CSimulation& sim,
                                CUnit& unit,
                                bool wasActiveUnit) override;

    // --- Phase 14: Techtree Equivalences, Upgrade Engine & Upkeep Rules ---
    int GetEffectiveTechLevel(const CSimulation& sim,
                              int playerIndex,
                              uint32_t techRawcode) const override;

    bool MeetsTechRequirements(const CSimulation& sim,
                               int playerIndex,
                               uint32_t targetRawcode) const override;

    std::vector<uint32_t> GetMissingTechRequirements(const CSimulation& sim,
                                                     int playerIndex,
                                                     uint32_t targetRawcode) const override;

    void OnPlayerTechLevelChanged(CSimulation& sim,
                                  CPlayer& player,
                                  uint32_t techRawcode,
                                  int oldLevel,
                                  int newLevel) override;

    void OnApplyPlayerUpgradesToUnit(CSimulation& sim,
                                     CPlayer& player,
                                     CUnit& unit) override;

    int ComputeUpkeepLevel(const CPlayer& player) const override;
    int ApplyUpkeepGoldTax(const CPlayer& player, int amount) const override;

    void RegisterTechEquivalence(War3ID baseTechId, War3ID higherEquivalentId);
    void ClearCustomTechEquivalences() { customTechEquivalents_.clear(); }
    std::vector<War3ID> GetTechEquivalents(War3ID baseTechId) const;

    // --- Phase 14: Melee & Allied Victory/Defeat & Crippled Evaluation ---
    void EvaluateVictoryDefeat(CSimulation& sim) override;
    bool IsPlayerCrippled(const CSimulation& sim, int playerIndex) const override;
    void OnPlayerRemoved(CSimulation& sim, int playerIndex, int gameResultCode) override;

    void SetMeleeVictoryRulesEnabled(bool enabled) { meleeVictoryRulesEnabled_ = enabled; }
    bool IsMeleeVictoryRulesEnabled() const { return meleeVictoryRulesEnabled_; }

    void SetRequireZeroUnitsForDefeat(bool requireZeroUnits) { requireZeroUnitsForDefeat_ = requireZeroUnits; }
    bool GetRequireZeroUnitsForDefeat() const { return requireZeroUnitsForDefeat_; }

    bool IsMatchEnded() const { return matchEnded_; }
    int GetWinningPlayerIndex() const { return winningPlayerIndex_; }
    void ResetMatchState() {
        matchEnded_ = false;
        winningPlayerIndex_ = -1;
    }

    static bool ArePlayersAlliedForVictory(const CPlayer& a, const CPlayer& b);

    // --- Phase 15: Combat Armor/Attack Type Multiplier Matrix & Damage Rules ---
    static const AttackDefenseTable& GetCanonicalWar3AttackDefenseTable();

    void OnConfigureCombatSystem(CSimulation& sim, CCombatSystem& combat) override;

    float GetAttackDefenseMultiplier(int attackTypeOrd, int defenseTypeOrd) const override;
    float GetAttackDefenseMultiplier(CAttackType atk, CDefenseType def) const;
    void SetAttackDefenseMultiplier(CAttackType atk, CDefenseType def, float multiplier);

    const AttackDefenseTable& GetCombatTable() const { return combatTable_; }
    void SetCombatTable(const AttackDefenseTable& table);
    void ResetCombatRulesToCanonicalDefaults();

    float GetDefenseArmorFactor() const { return defenseArmorFactor_; }
    void SetDefenseArmorFactor(float k) { defenseArmorFactor_ = k; }

    float ComputeArmorDamageReduction(int defense,
                                      int attackerLevel,
                                      float defenseArmorFactor) const override;

    bool ShouldResistMagicDamage(int attackTypeOrd,
                                 int damageTypeOrd,
                                 bool targetIsMagicImmune,
                                 bool magicImmuneResistsDamage,
                                 bool magicImmuneResistsUltimates) const override;

    bool EvaluateEtherealDamageFactor(int attackTypeOrd,
                                      int damageTypeOrd,
                                      bool targetIsEthereal,
                                      bool allied,
                                      bool etherealBonusAlly,
                                      float etherealBonusMagic,
                                      float etherealBonusSpells,
                                      float& outMultiplier) const override;

    // --- Phase 15: Tactical AI Controller Lifecycle & Counter-Scoring Engine ---
    bool CreatePlayerAI(CSimulation& sim, int playerIndex, int difficulty) override;
    void AttachPlayerAI(int playerIndex, std::shared_ptr<MeleeAIController> controller);
    MeleeAIController* GetPlayerAI(int playerIndex) const;
    bool RemovePlayerAI(int playerIndex) override;
    void SetPlayerAIEnabled(int playerIndex, bool enabled) override;
    void CommandPlayerAI(int playerIndex, int command, int data) override;
    void TickPlayerAIs(CSimulation& sim, float dt) override;
    size_t GetActiveAICount() const { return aiControllers_.size(); }

    float EvaluateTacticalTargetScore(const CSimulation& sim,
                                      const CUnit& attacker,
                                      const CUnit& candidateTarget,
                                      float baseScore) const override;

    bool OnDiscoverAIRacePlan(const CSimulation& sim,
                              int playerIndex,
                              AIRacePlan& outPlan) const override;
    static bool DiscoverRaceBuildPlan(const CSimulation& sim,
                                      int playerIndex,
                                      AIRacePlan& outPlan);

    // --- Phase 16: Unit Combat Reactions, Passive Procs, Kill Bounty & Hero XP,
    //               Idle Spell Autocast, Item Bonuses & Campaign Hero Carry-Over ---
    bool OnEvaluatePreMitigationDamage(CSimulation& sim,
                                       CUnit* attacker,
                                       CUnit& target,
                                       bool isAttack,
                                       bool isRanged,
                                       int attackTypeOrd,
                                       int damageTypeOrd,
                                       bool invulnerableTarget,
                                       float& inOutDamage) override;

    void OnModifyPostMitigationDamage(CSimulation& sim,
                                      CUnit* attacker,
                                      CUnit& target,
                                      bool isAttack,
                                      bool isRanged,
                                      int attackTypeOrd,
                                      int damageTypeOrd,
                                      float& inOutActualDamage) override;

    void OnPostDamageLanded(CSimulation& sim,
                            CUnit* attacker,
                            CUnit& target,
                            bool isAttack,
                            bool isRanged,
                            int attackTypeOrd,
                            int damageTypeOrd,
                            float incomingBaseDamage,
                            float incomingBaseBonus,
                            float actualDamage,
                            bool wasEvaded,
                            bool invulnerableTarget) override;

    void OnAwardKillBountyAndXP(CSimulation& sim,
                                CUnit* killer,
                                CUnit& victim) override;

    bool OnTryUnitIdleAutocast(CSimulation& sim, CUnit& unit) override;

    // Phase 18: Module-scoped autocast acquisition registry & query
    int GetAbilityAutocastKind(uint32_t abilityRawcode) const override;
    void RegisterAutocastAbility(War3ID abilityCode, int autocastKindOrd);
    void ClearCustomAutocastAbilities() { customAutocastKinds_.clear(); }

    // Phase 19: Building placement grid snap & obstruction validation
    void OnSnapBuildPosition(const CUnitType* buildType,
                             float& inOutX,
                             float& inOutY) const override;
    bool IsBuildLocationObstructed(CSimulation& sim,
                                   float x,
                                   float y,
                                   int playerIndex,
                                   uint32_t buildingTypeRawcode) const override;

    // Phase 20: Dynamic order & declarative aura registration
    void RegisterCustomOrder(const std::string& orderName,
                             int orderId,
                             uint32_t abilityRawcode = 0,
                             int secondaryOrderId = 0) override;
    void RegisterCustomAura(War3ID code,
                            float radius,
                            bool affectsAllies,
                            bool affectsSelf,
                            bool enemyOnly,
                            const std::vector<CAuraBonus>& bonuses);
    bool UnregisterCustomAura(War3ID code);

    // Phase 21: In-memory custom data-table archetype registration (Items, Upgrades, Abilities)
    bool RegisterCustomItemArchetype(CSimulation& sim,
                                     War3ID itemId,
                                     const std::string& name,
                                     int goldCost,
                                     int lumberCost,
                                     int charges,
                                     const std::vector<War3ID>& abilityList);
    bool RegisterCustomUpgradeArchetype(CSimulation& sim,
                                        War3ID upgradeId,
                                        const std::string& name,
                                        int maxLevel,
                                        int goldBase,
                                        int goldMod,
                                        const std::string& effectCode,
                                        float effectBase,
                                        float effectMod);
    bool RegisterCustomAbilityDef(CSimulation& sim,
                                  War3ID abilityId,
                                  const std::string& name,
                                  int levels,
                                  float cooldown,
                                  int manaCost,
                                  float range,
                                  float area,
                                  float duration);

    // Phase 22: Dynamic Buff Factory & Declarative DataDrivenBuffSpec Registry
    using BuffCreatorFn = std::function<std::unique_ptr<CBuff>(float duration, int casterHandleId)>;
    void RegisterCustomBuffCreator(War3ID buffCode, BuffCreatorFn creator);
    void RegisterCustomBuffSpec(const DataDrivenBuffSpec& spec);
    bool UnregisterCustomBuff(War3ID buffCode);
    bool HasCustomBuff(War3ID buffCode) const;

    CBuff* CreateTimedLifeBuff(uint32_t buffRawcode,
                               float duration) const override;
    CBuff* CreateBuffByCode(uint32_t buffRawcode,
                            float duration = 0.0f,
                            int casterHandleId = -1) const override;
    bool TryApplyCustomBuffByCode(CSimulation& sim,
                                  CUnit& target,
                                  uint32_t buffRawcode,
                                  float durationOverride = 0.0f,
                                  int casterHandleId = -1) override;

    // Phase 23: Engine-Level Pluggable Projectile & Unit Behavior State Machine
    struct CustomProjectileSpec {
        War3ID code;
        float speed = 900.0f;
        float damage = 0.0f;
        CAttackType attackType = CAttackType::SPELLS;
        CDamageType damageType = CDamageType::MAGIC;
        bool isAttack = false;
        bool isRanged = true;
        bool groundTarget = false;
        bool checkEffectReaction = false;
        bool customImpactOnly = false;
        float splashFull = 0.0f;
        float splashHalf = 0.0f;
        float splashQuarter = 0.0f;
        float splashFactorHalf = 0.5f;
        float splashFactorQuarter = 0.25f;
        int bounces = 0;
        float bounceRange = 500.0f;
        float bounceLossFactor = 0.8f;
        float spillDistance = 0.0f;
        float spillRadius = 0.0f;
        War3ID impactBuffCode;
        float impactBuffDuration = 0.0f;
        std::function<void(CSimulation& sim, CUnit* source, CWidget* target, float x, float y)> onImpact;
    };

    using ProjectileImpactFn = std::function<void(CSimulation& sim, CProjectile& proj, CUnit* source, CWidget* target, float x, float y)>;

    void RegisterCustomProjectileSpec(const CustomProjectileSpec& spec);
    void RegisterCustomProjectileImpact(War3ID projCode, ProjectileImpactFn fn);
    bool UnregisterCustomProjectile(War3ID projCode);
    bool HasCustomProjectile(War3ID projCode) const;

    bool OnProjectilePreImpact(CSimulation& sim,
                               CProjectile& proj,
                               CUnit* source,
                               CWidget* target,
                               bool& outTerminate) override;

    bool OnProjectileCustomImpact(CSimulation& sim,
                                  CProjectile& proj,
                                  CUnit* source,
                                  CWidget* target,
                                  float impactX,
                                  float impactY) override;

    CUnit* OnFindProjectileBounceTarget(CSimulation& sim,
                                        const CProjectile& proj,
                                        CUnit* source,
                                        CWidget* currentTarget) override;

    bool SpawnCustomProjectile(CSimulation& sim,
                               uint32_t projRawcode,
                               CUnit* source,
                               CWidget* target,
                               float targetX = 0.0f,
                               float targetY = 0.0f) override;

    struct CustomBehaviorSpec {
        War3ID code;
        int orderId = 0;
        CBehaviorCategory category = CBehaviorCategory::CUSTOM;
        float range = 128.0f;
        float duration = 0.0f;
        float tickInterval = 0.5f;
        bool interruptable = true;
        std::function<void(CSimulation& sim, CUnit* unit, CWidget* target, float tx, float ty)> onBegin;
        std::function<bool(CSimulation& sim, CUnit* unit, CWidget* target, float tx, float ty, float elapsed)> onTick;
        std::function<void(CSimulation& sim, CUnit* unit)> onEnd;
    };

    using BehaviorCreatorFn = std::function<CBehavior*(CSimulation& sim, CUnit* unit, const COrder& order)>;

    void RegisterCustomBehaviorCreator(int orderId, BehaviorCreatorFn fn);
    void RegisterCustomBehaviorSpec(const CustomBehaviorSpec& spec);
    bool UnregisterCustomBehavior(int orderId);
    bool HasCustomBehavior(int orderId) const;

    CBehavior* CreateUnitBehavior(CBehaviorCategory category) const override;
    CBehavior* CreateStunBehavior(float duration) const override;
    CBehavior* TryCreateCustomOrderBehavior(CSimulation& sim,
                                            CUnit* unit,
                                            const COrder& order) override;

    // Phase 24: Category Behavior Overrides & Pluggable Resource Dropoff / Harvest Rules
    using CategoryBehaviorFactoryFn = std::function<CBehavior*()>;
    void RegisterCategoryBehaviorOverride(CBehaviorCategory category, CategoryBehaviorFactoryFn fn);
    bool UnregisterCategoryBehaviorOverride(CBehaviorCategory category);
    bool HasCategoryBehaviorOverride(CBehaviorCategory category) const;

    struct CustomDropoffRule {
        bool acceptsGold = true;
        bool acceptsLumber = true;
    };
    void RegisterCustomDropoffUnitType(War3ID unitTypeId, bool acceptsGold = true, bool acceptsLumber = true);
    bool UnregisterCustomDropoffUnitType(War3ID unitTypeId);
    bool HasCustomDropoffUnitType(War3ID unitTypeId) const;

    bool CanUnitAcceptResourceDropoff(const CUnit& dropoffUnit,
                                      int resourceTypeOrd) const override;
    CUnit* OnFindNearestResourceDropoff(CSimulation& sim,
                                        const CUnit& worker,
                                        int resourceTypeOrd) override;
    CWidget* OnFindNearestHarvestTarget(CSimulation& sim,
                                        const CUnit& worker,
                                        int resourceTypeOrd,
                                        float refX,
                                        float refY) override;

    // Phase 25: Vision/Fog Multipliers & Extra Sight Sources, Custom Cell Passability, & Module Snapshot
    void SetPlayerSightMultiplier(int playerIndex, float multiplier);
    float GetPlayerSightMultiplier(int playerIndex) const;

    void RegisterCustomSightSource(int sourceId, int playerIndex, float x, float y, float radius, bool isFlying = true);
    bool UnregisterCustomSightSource(int sourceId);
    bool HasCustomSightSource(int sourceId) const;

    float OnComputeUnitSightRadius(const CSimulation& sim,
                                   const CUnit& unit,
                                   float baseSightRadius,
                                   bool isNight) const override;

    void OnCollectExtraSightSources(const CSimulation& sim,
                                    std::vector<CFogSightSource>& outSources) const override;

    struct CustomCellPassabilityRule {
        bool walkable = true;
        bool flyable = true;
    };
    void SetCustomCellPassabilityRule(int cellX, int cellY, bool walkable, bool flyable = true);
    bool RemoveCustomCellPassabilityRule(int cellX, int cellY);
    void ClearCustomCellPassabilityRules() { customCellPassabilityRules_.clear(); }

    int OnCheckCustomCellPassability(int cellX,
                                     int cellY,
                                     int movementTypeOrd) const override;

    void OnCaptureModuleSnapshot(const CSimulation& sim,
                                 std::vector<uint8_t>& outBytes) const override;

    bool OnRestoreModuleSnapshot(CSimulation& sim,
                                 const uint8_t* data,
                                 size_t size) override;

    // Phase 26: Lockstep Command Validation, Global Allied Tribute & Custom SimCommands,
    //           and Deterministic Module State Hashing
    static constexpr int32_t kOrderTributeGold   = 0x54524744; // "TRGD"
    static constexpr int32_t kOrderTributeLumber = 0x54524C4D; // "TRLM"

    using CustomGlobalCommandFn = std::function<bool(CSimulation& sim, const SimCommand& cmd)>;

    void SetPlayerCommandMuted(int playerIndex, bool muted);
    bool IsPlayerCommandMuted(int playerIndex) const;

    void RegisterCustomGlobalCommand(int32_t orderId, CustomGlobalCommandFn fn);
    bool UnregisterCustomGlobalCommand(int32_t orderId);
    bool HasCustomGlobalCommand(int32_t orderId) const;
    uint32_t GetTributeCommandsExecuted() const { return tributeCommandsExecuted_; }

    bool OnValidateLockstepCommand(const CSimulation& sim,
                                   const SimCommand& cmd) const override;

    bool OnExecuteCustomSimCommand(CSimulation& sim,
                                   const SimCommand& cmd) override;

    uint64_t OnComputeModuleStateHash(const CSimulation& sim) const override;

    // Phase 27: Pluggable Cell Traversal Cost Multiplier & Squad Formation Rank Priority
    void SetCustomCellCostMultiplier(int cellX, int cellY, float costMultiplier);
    float GetCustomCellCostMultiplier(int cellX, int cellY) const;
    bool RemoveCustomCellCostMultiplier(int cellX, int cellY);
    void ClearCustomCellCostMultipliers() { customCellCostMultipliers_.clear(); }

    float OnGetCellTraversalCostMultiplier(int cellX,
                                           int cellY,
                                           int movementTypeOrd) const override;

    void SetCustomUnitFormationRank(War3ID unitTypeId, int rankPriority);
    int GetCustomUnitFormationRank(War3ID unitTypeId) const;
    bool RemoveCustomUnitFormationRank(War3ID unitTypeId);

    int OnGetUnitFormationRankPriority(const CUnit& unit) const override;

    // Phase 28: Pluggable Upkeep Display Label & Unit Combat/Death Sound Routing
    void SetCustomUpkeepLabel(int upkeepLevel, const std::string& label);
    std::string GetUpkeepDisplayLabel(int upkeepLevel) const override;

    struct CustomUnitSoundEntry {
        std::string hitClip;
        std::string deathClip;
    };
    void RegisterCustomUnitSoundOverride(War3ID unitTypeId,
                                         const std::string& hitClip,
                                         const std::string& deathClip);
    bool UnregisterCustomUnitSoundOverride(War3ID unitTypeId);
    bool HasCustomUnitSoundOverride(War3ID unitTypeId) const;

    bool OnResolveUnitCombatSound(const CSimulation& sim,
                                  int unitHandle,
                                  int sourceHandle,
                                  bool isDeath,
                                  std::string& outClipKey) const override;

    float GetNegativeBuffDurationMultiplier(const CUnit& target) const override;

    bool OnCheckSpellEffectReaction(CSimulation& sim,
                                    CUnit& target,
                                    CUnit* caster) override;

    void OnApplyUnitItemBonus(CSimulation& sim,
                              CUnit& unit,
                              int bonusTypeOrd,
                              int signedDelta) override;

    bool IsCarriableHero(const CSimulation& sim, CUnit& unit) const override;

    bool OnSnapshotCarriedHero(const CSimulation& sim,
                               CUnit& unit,
                               CarriedHero& outCarried) const override;

    CUnit* OnRespawnCarriedHero(CSimulation& sim,
                                const CarriedHero& carried,
                                float x,
                                float y) override;

    // Configurable runtime multipliers for custom map modes (e.g., 2.0x turbo XP/bounty)
    void SetBountyMultiplier(float mult) { bountyMultiplier_ = std::max(0.0f, mult); }
    float GetBountyMultiplier() const { return bountyMultiplier_; }

    void SetExperienceMultiplier(float mult) { experienceMultiplier_ = std::max(0.0f, mult); }
    float GetExperienceMultiplier() const { return experienceMultiplier_; }

    // Optional bound CSimulation when hosted inside EngineKernel
    void BindSimulation(CSimulation* sim) { boundSim_ = sim; }
    CSimulation* GetBoundSimulation() const { return boundSim_; }

    // WarCraft 3 domain classification helpers
    static bool IsRepairAbilityAlias(War3ID abilityCode);
    static bool IsTownHallReturnResourceType(War3ID typeId);
    static bool IsStandardRaceWorkerType(War3ID typeId);
    static bool DoesUpgradeApplyToUnit(const CUnit& unit,
                                       const CUnitType& unitType,
                                       const CUpgradeType& upgrade);

private:
    void ApplySingleUpgradeToUnit(CSimulation& sim,
                                  CPlayer& player,
                                  CUnit& unit,
                                  const CUpgradeType& upgrade,
                                  int targetLevel);
    void RefreshUnitAbilityRequirements(CSimulation& sim, CUnit& unit);

    CSimulation* boundSim_ = nullptr;
    std::unordered_map<uint32_t, std::vector<War3ID>> customTechEquivalents_;
    std::unordered_map<uint32_t, int> customAutocastKinds_;
    std::unordered_map<uint32_t, BuffCreatorFn> customBuffCreators_;
    std::unordered_map<uint32_t, DataDrivenBuffSpec> customBuffSpecs_;
    std::unordered_map<uint32_t, CustomProjectileSpec> customProjectileSpecs_;
    std::unordered_map<uint32_t, ProjectileImpactFn> customProjectileImpacts_;
    std::unordered_map<int, BehaviorCreatorFn> customBehaviorCreators_;
    std::unordered_map<int, CustomBehaviorSpec> customBehaviorSpecs_;
    std::unordered_map<uint8_t, CategoryBehaviorFactoryFn> customCategoryBehaviorOverrides_;
    std::unordered_map<uint32_t, CustomDropoffRule> customDropoffTypes_;
    std::unordered_map<int, float> playerSightMultipliers_;
    std::unordered_map<int, CFogSightSource> customSightSources_;
    std::unordered_map<int64_t, CustomCellPassabilityRule> customCellPassabilityRules_;
    std::unordered_map<int64_t, float> customCellCostMultipliers_;
    std::unordered_map<uint32_t, int> customUnitFormationRanks_;
    std::unordered_map<int, std::string> customUpkeepLabels_;
    std::unordered_map<uint32_t, CustomUnitSoundEntry> customUnitSoundOverrides_;
    std::unordered_map<int32_t, CustomGlobalCommandFn> customGlobalCommands_;
    uint32_t mutedPlayerMask_ = 0;
    uint32_t tributeCommandsExecuted_ = 0;
    AttackDefenseTable combatTable_;
    float defenseArmorFactor_ = 0.06f;
    float bountyMultiplier_ = 1.0f;
    float experienceMultiplier_ = 1.0f;
    std::unordered_map<int, std::shared_ptr<MeleeAIController>> aiControllers_;
    bool meleeVictoryRulesEnabled_ = false;
    bool requireZeroUnitsForDefeat_ = false;
    bool matchEnded_ = false;
    int winningPlayerIndex_ = -1;
};

} // namespace war3
