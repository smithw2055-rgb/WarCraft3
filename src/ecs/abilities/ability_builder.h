#pragma once
// Data-driven spell authoring (C++ port of warsmash_ref abilitybuilder).
//
// Reference model: `resources/Scripts/Abilities/**/*.j` structs compose
// small actions (damage area, summon, buff, heal) guarded by conditions.
// Instead of porting all ~400 Java files 1:1, this header provides the
// composer primitives: ABAction list + ABEffectChain + a War3ID-keyed
// registry. Hardcoded CAbilitySpellBase subclasses (spells*.cpp) are being
// migrated onto chains one spell at a time (Dark Portal first).
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include "data/combat_types.h"
#include "resource/slk.h"

class CSimulation;
class CUnit;
class CWidget;
class CBuff;
class CAbility;

enum class ABTargetKind : uint8_t {
    NO_TARGET,
    UNIT,
    POINT,
};

// One composable spell step. Runs against an explicit target triple so the
// same chain works for no-target / unit / point spell overloads.
class ABAction {
public:
    virtual ~ABAction() = default;
    virtual void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                     bool hasPoint, float x, float y) = 0;
};

// Deal flat damage to every living unit (except the caster) in radius.
class ABActionDamageArea : public ABAction {
public:
    ABActionDamageArea(float radius, float amount,
                       CAttackType attackType = CAttackType::SPELLS,
                       CDamageType damageType = CDamageType::NORMAL);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float radius_;
    float amount_;
    CAttackType attackType_;
    CDamageType damageType_;
};

// Heal living units in radius (caster faction only when allies known,
// otherwise every living unit but the caster is skipped).
class ABActionHealArea : public ABAction {
public:
    ABActionHealArea(float radius, float amount);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float radius_;
    float amount_;
};

// Apply a buff (built by factory) to living units in radius.
class ABActionApplyBuffArea : public ABAction {
public:
    using BuffFactory = std::function<std::unique_ptr<CBuff>()>;
    ABActionApplyBuffArea(float radius, BuffFactory factory);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float radius_;
    BuffFactory factory_;
};

// Summon `count` units of typeId around the target point with timed life.
// (ref AbilitySummonWaterElemental.doEffect + CBuffTimedLife.)
class ABActionSummon : public ABAction {
public:
    ABActionSummon(War3ID typeId, War3ID buffAlias, int count, float duration,
                   float spread = 64.0f);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    War3ID typeId_;
    War3ID buffAlias_;
    int count_;
    float duration_;
    float spread_;
};

// Ordered list of actions executed for one spell effect.
class ABEffectChain {
public:
    void Add(std::unique_ptr<ABAction> action) { actions_.push_back(std::move(action)); }
    bool Empty() const { return actions_.empty(); }
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) const;

private:
    std::vector<std::unique_ptr<ABAction>> actions_;
};

// War3ID-keyed registry of effect chains (populated at startup / by tests).
class AbilityBuilderRegistry {
public:
    static AbilityBuilderRegistry& Instance();
    void Register(War3ID alias, ABEffectChain chain);
    const ABEffectChain* Find(War3ID alias) const;

private:
    std::unordered_map<uint32_t, ABEffectChain> chains_; // keyed by War3ID::value
};

// Shared summon helper: create + timed-life buff + summoned callback.
// Returns the summoned unit or nullptr.
CUnit* SummonTimedUnit(CSimulation& game, CUnit* caster, War3ID typeId,
                       War3ID buffAlias, float x, float y, float facing,
                       float duration);

// ---------------------------------------------------------------------------
// AbilityBuilder deep-fill (round 2): conditions + single-target / utility
// actions mirroring warsmash_ref abilitybuilder/behavior/action/*.
//
// Reference model: each .j script (resources/Scripts/Abilities/**/*.j)
// composes small ABAction/ABCondition steps. The C++ port does not replicate
// all ~200 Java classes 1:1; it provides the composer primitives that cover
// the patterns actually used by the scripts:
//   StormBolt.j      -> missile + single-target damage + stun
//   Blizzard.j       -> iterate units in range + damage per wave
//   MassTeleport.j   -> channel + teleport group to point
//   SummonWaterEle.j -> create unit + timed life (already had ABActionSummon)
//   Brilliance/Devotion.j -> aura stat buffs (covered by BuffFactory actions)
// ---------------------------------------------------------------------------

// Condition evaluated against the same target triple as ABAction::Run.
class ABCondition {
public:
    virtual ~ABCondition() = default;
    virtual bool Test(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                      bool hasPoint, float x, float y) const = 0;
};

// Always-true / always-false constants (ref ABCondition truisms).
class ABCondTrue : public ABCondition {
public:
    bool Test(CSimulation&, CUnit*, CWidget*, bool, float, float) const override {
        return true;
    }
};

// Target must be a living unit (ref target-alive checks in .j filters).
class ABCondTargetAlive : public ABCondition {
public:
    bool Test(CSimulation&, CUnit*, CWidget* unitTarget,
              bool, float, float) const override;
};

// Target must be an undead unit (ref IsUnitType(target, UNIT_TYPE_UNDEAD);
// HolyLight.j branches damage vs heal on this).
class ABCondTargetUndead : public ABCondition {
public:
    bool Test(CSimulation&, CUnit*, CWidget* unitTarget,
              bool, float, float) const override;
};

// Target must be allied (passive) with the caster (ref Death Coil's
// heal-vs-harm branch; players missing on either side reads as hostile).
class ABCondTargetAllyOfCaster : public ABCondition {
public:
    bool Test(CSimulation& game, CUnit* caster, CWidget* unitTarget,
              bool, float, float) const override;
};

// Conjunction of conditions (ref composed .j filter expressions).
class ABCondAll : public ABCondition {
public:
    explicit ABCondAll(std::vector<std::shared_ptr<ABCondition>> conds);
    bool Test(CSimulation& game, CUnit* caster, CWidget* unitTarget,
              bool hasPoint, float x, float y) const override;

private:
    std::vector<std::shared_ptr<ABCondition>> conds_;
};

// Lambda-backed condition for tests / script glue.
class ABCondFunc : public ABCondition {
public:
    using Fn = std::function<bool(CSimulation&, CUnit*, CWidget*, bool, float, float)>;
    explicit ABCondFunc(Fn fn) : fn_(std::move(fn)) {}
    bool Test(CSimulation& game, CUnit* caster, CWidget* unitTarget,
              bool hasPoint, float x, float y) const override {
        return fn_ ? fn_(game, caster, unitTarget, hasPoint, x, y) : false;
    }

private:
    Fn fn_;
};

// Single-target damage (ref ABActionDamageTarget; cf. StormBolt onHit).
// `ranged` mirrors the isRanged flag of UnitDamageTarget (HolyLight.j
// passes true for its divine bolt); `isAttack` marks weapon attacks
// (ref Black Arrow's piercing shot, which can miss/evade/crit).
class ABActionDamageTarget : public ABAction {
public:
    ABActionDamageTarget(float amount,
                         CAttackType attackType = CAttackType::SPELLS,
                         CDamageType damageType = CDamageType::NORMAL,
                         bool ranged = false, bool isAttack = false);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float amount_;
    CAttackType attackType_;
    CDamageType damageType_;
    bool ranged_;
    bool isAttack_;
};

// Faction filter for area iteration (ref .j targetsAllowed checks).
// Legacy parity note: several hardcoded spells filter by raw player index
// rather than alliance; OTHER_PLAYER_THAN_CASTER reproduces exactly that.
enum class ABFactionFilter : uint8_t {
    ANY,
    SAME_PLAYER_AS_CASTER,
    OTHER_PLAYER_THAN_CASTER,
};

// Life filter for area iteration (default LIVING preserves every existing
// caller; DEAD serves Resurrect-style corpse iteration).
enum class ABLifeFilter : uint8_t {
    LIVING,
    DEAD,
    ANY,
};

// Run a sub-chain once per living unit inside a corridor starting at the
// caster toward the unit target (or effect point): projection within
// [0, range] and perpendicular distance within width/2
// (ref Shockwave / Impale line geometry). Degenerate (< 1 unit) direction
// is a no-op, matching the hand-rolled loops.
class ABActionForEachInLine : public ABAction {
public:
    ABActionForEachInLine(float range, float width, ABEffectChain chain,
                          bool includeCaster = false, int maxCount = -1);
    void SetExclude(const CWidget* exclude) { exclude_ = exclude; }
    void SetFactionFilter(ABFactionFilter filter) { factionFilter_ = filter; }
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float range_;
    float width_;
    ABEffectChain chain_;
    bool includeCaster_;
    int maxCount_;
    const CWidget* exclude_ = nullptr;
    ABFactionFilter factionFilter_ = ABFactionFilter::ANY;
};

// Bounce a per-hit chain across nearest neighbors (ref Chain Lightning /
// Healing Wave): starting at the unit target, run up to `bounces` hits,
// each time hopping to the nearest living unit within `range` of the
// current one. Each unit is struck at most once per cast (retail chains
// never revisit; legacy ping-pong was a bug the exact-damage tests pin).
// The chain is rebuilt per bounce by `factory`, which receives the 0-based
// bounce index -- callers bake falloff (damage/heal decay) into the
// amounts they close over.
class ABActionBounce : public ABAction {
public:
    using ChainFactory = std::function<ABEffectChain(int bounceIndex)>;
    ABActionBounce(int bounces, float range, ChainFactory factory,
                   bool includeCaster = true);
    void SetFactionFilter(ABFactionFilter filter) { factionFilter_ = filter; }
    void SetLifeFilter(ABLifeFilter filter) { lifeFilter_ = filter; }
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    int bounces_;
    float range_;
    ChainFactory factory_;
    bool includeCaster_;
    ABFactionFilter factionFilter_ = ABFactionFilter::ANY;
    ABLifeFilter lifeFilter_ = ABLifeFilter::LIVING;
};

// Create one unit of `typeId` at the unit target's position for the
// caster's player (ref Raise Dead: skeletons rise where corpses lie).
// Graceful without unit data (null CreateUnit is a no-op); fires the
// summoned callback on success like SummonTimedUnit. `count`/`stepX`
// lay multiple units along +X (ref Carrion Beetles i*64 spacing).
class ABActionCreateUnitAtTarget : public ABAction {
public:
    explicit ABActionCreateUnitAtTarget(War3ID typeId, int count = 1,
                                        float stepX = 0.0f);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    War3ID typeId_;
    int count_;
    float stepX_;
};

// Create one unit of `typeId` at an explicit point for the caster's
// player (ref Serpent Ward / Spirit Bear: exact placement, facing 0,
// no timed life). Graceful without unit data; fires the summoned
// callback on success.
class ABActionCreateUnitAtPoint : public ABAction {
public:
    ABActionCreateUnitAtPoint(War3ID typeId, float x, float y, float facing = 0.0f);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    War3ID typeId_;
    float x_;
    float y_;
    float facing_;
};

// Ring of timed summons around a point (ref Force of Nature: treants on
// a 64-radius ring, each facing outward). One SummonTimedUnit per slot,
// sharing type/buff/duration.
class ABActionSummonRing : public ABAction {
public:
    ABActionSummonRing(War3ID typeId, War3ID buffAlias, int count, float duration,
                       float radius = 64.0f);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    War3ID typeId_;
    War3ID buffAlias_;
    int count_;
    float duration_;
    float radius_;
};

// Steal up to `maxAmount` mana from the unit target and give it to the
// caster (clamped to the caster's max; ref Scroll of Mana Stealing, no
// damage unlike Mana Burn).
class ABActionManaSteal : public ABAction {
public:
    explicit ABActionManaSteal(float maxAmount);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float maxAmount_;
};

// Heal the caster by a flat amount, clamped to max life
// (ref Death Pact: sacrifice proceeds feed the caster).
class ABActionHealCaster : public ABAction {
public:
    explicit ABActionHealCaster(float amount);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float amount_;
};

// Grant gold to the caster's player (ref Transmute gold yield).
class ABActionAddGoldToCaster : public ABAction {
public:
    explicit ABActionAddGoldToCaster(int gold);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    int gold_;
};

// Remove the unit target from the simulation (ref Cannibalize consuming
// the corpse; the eating/healing is a separate chained action).
class ABActionRemoveTarget : public ABAction {
public:
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;
};

// Create a unit and immediately buff it, then fire the summoned callback
// (ref Healing Ward: the ward is born already radiating; ref factories:
// TimedLife plus Spawner/Split/Rebirth ride the same birth).
class ABActionCreateUnitWithBuff : public ABAction {
public:
    using BuffFactory = ABActionApplyBuffArea::BuffFactory;
    ABActionCreateUnitWithBuff(War3ID typeId, float x, float y,
                               std::vector<BuffFactory> factories);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    War3ID typeId_;
    float x_;
    float y_;
    std::vector<BuffFactory> factories_;
};

// Transfer the unit target's ownership to the caster's player
// (ref Charm; cf. Ancestral Spirit's inline transfer).
class ABActionTransferToCaster : public ABAction {
public:
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;
};

// Create a unit and grant it a constructed ability, then fire the
// summoned callback (ref Sentry Ward: born with True Sight). The factory
// receives the sim and the newborn to build any ability object.
class ABActionCreateUnitWithAbility : public ABAction {
public:
    using AbilityFactory =
        std::function<std::unique_ptr<CAbility>(CSimulation&, CUnit*)>;
    ABActionCreateUnitWithAbility(War3ID typeId, float x, float y,
                                  AbilityFactory factory);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    War3ID typeId_;
    float x_;
    float y_;
    AbilityFactory factory_;
};

// Summon a timed copy of the unit target's type beside it, facing the
// same way (ref Illusion: +64/+0 single copy; ref Mirror Image: callers
// fan several copies with per-action offsets).
class ABActionSummonCopyOfTarget : public ABAction {
public:
    ABActionSummonCopyOfTarget(War3ID buffAlias, float duration,
                               float offsetX = 64.0f, float offsetY = 0.0f);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    War3ID buffAlias_;
    float duration_;
    float offsetX_;
    float offsetY_;
};

// Full Animate Dead ritual on a dead unit target: revive in place,
// invulnerable, transfer to the caster's player, timed life, summoned
// callback (ref AUan; radius iteration stays in the caller).
class ABActionAnimateDeadCorpse : public ABAction {
public:
    ABActionAnimateDeadCorpse(War3ID buffAlias, float duration);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    War3ID buffAlias_;
    float duration_;
};

// Revive a Tauren corpse for the caster (ref Ancestral Spirit: only
// otau/Otch answer, and the risen changes ownership).
class ABActionAncestralRevive : public ABAction {
public:
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;
};

// Devour all dispellable buffs on the target and refund mana per buff
// eaten (ref Devour Magic).
class ABActionDevourMagic : public ABAction {
public:
    explicit ABActionDevourMagic(float manaPerBuff);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float manaPerBuff_;
};

// Burn up to `maxBurn` mana from the unit target and deal the burned
// amount as damage (ref Mana Burn: min(DataA, current mana)).
class ABActionManaBurn : public ABAction {
public:
    ABActionManaBurn(int maxBurn,
                     CAttackType attackType = CAttackType::SPELLS,
                     CDamageType damageType = CDamageType::MIND);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    int maxBurn_;
    CAttackType attackType_;
    CDamageType damageType_;
};

// Single-target damage as a fraction of the target's max life
// (ref Death and Decay DataA: corruption scales with victim bulk).
class ABActionDamageTargetMaxLifePct : public ABAction {
public:
    ABActionDamageTargetMaxLifePct(float fraction,
                                   CAttackType attackType = CAttackType::SPELLS,
                                   CDamageType damageType = CDamageType::NORMAL);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float fraction_;
    CAttackType attackType_;
    CDamageType damageType_;
};

// Single-target heal, clamped to max life (ref ABActionHeal).
class ABActionHealTarget : public ABAction {
public:
    explicit ABActionHealTarget(float amount);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float amount_;
};

// Single-target buff attach (ref ABActionAddBuff).
class ABActionAddBuffTarget : public ABAction {
public:
    using BuffFactory = ABActionApplyBuffArea::BuffFactory;
    explicit ABActionAddBuffTarget(BuffFactory factory);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    BuffFactory factory_;
};

// Single-target stun (ref ABActionAddStunBuff; StormBolt/WarStomp onHit).
class ABActionStunTarget : public ABAction {
public:
    explicit ABActionStunTarget(float duration);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float duration_;
};

// Kill the unit target (ref ABActionKillUnit).
class ABActionKillTarget : public ABAction {
public:
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;
};

// Resurrect a dead unit target at full life (ref ABActionResurrect;
// Paladin Resurrect.j pattern, simplified: no corpse handling).
class ABActionResurrectTarget : public ABAction {
public:
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;
};

// Revive a dead unit target in place via CUnit::Revive (ref Resurrect
// spell; full corpse-flag reset, unlike ResurrectTarget's life refill).
// Living targets are skipped.
class ABActionReviveTarget : public ABAction {
public:
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;
};

// Set life / mana on the unit target (ref ABActionSetHp / ABActionSetMp).
class ABActionSetLifeTarget : public ABAction {
public:
    explicit ABActionSetLifeTarget(float life);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float life_;
};

class ABActionSetManaTarget : public ABAction {
public:
    explicit ABActionSetManaTarget(float mana);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float mana_;
};

// Create `count` units for the caster's player around the target point
// without timed life (ref ABActionCreateUnit; MassTeleport staging,
// Charm/Transmute-style spawns). Positions fan out along the caster facing.
class ABActionCreateUnit : public ABAction {
public:
    ABActionCreateUnit(War3ID typeId, int count, float spread = 64.0f);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    War3ID typeId_;
    int count_;
    float spread_;
};

// Teleport the unit target to the effect point (ref MassTeleport
// doChannelTick relocation, single-target primitive). The fixed-destination
// overload carries MassTeleport's spell destination through area iteration,
// where the effect point would otherwise be the iterated unit's position.
class ABActionTeleportTarget : public ABAction {
public:
    ABActionTeleportTarget();
    ABActionTeleportTarget(float x, float y);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    bool hasDest_ = false;
    float destX_ = 0.0f;
    float destY_ = 0.0f;
};

// Run a sub-chain once per living unit in radius of the effect center
// (ref ABActionIterateUnitsInRangeOfLocation; Blizzard wave pattern).
// The sub-chain receives each iterated unit as its unitTarget.
// SetExclude skips one widget (e.g. Frost Nova's primary target, which
// takes its own direct hit instead of the splash).
class ABActionForEachInRange : public ABAction {
public:
    ABActionForEachInRange(float radius, ABEffectChain chain,
                           bool includeCaster = false, int maxCount = -1);
    void SetExclude(const CWidget* exclude) { exclude_ = exclude; }
    void SetFactionFilter(ABFactionFilter filter) { factionFilter_ = filter; }
    void SetLifeFilter(ABLifeFilter filter) { lifeFilter_ = filter; }
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float radius_;
    ABEffectChain chain_;
    bool includeCaster_;
    int maxCount_;
    const CWidget* exclude_ = nullptr;
    ABFactionFilter factionFilter_ = ABFactionFilter::ANY;
    ABLifeFilter lifeFilter_ = ABLifeFilter::LIVING;
};

// Conditional branch (ref ABActionIf): runs then-chain or else-chain
// based on a condition.
class ABActionIf : public ABAction {
public:
    ABActionIf(std::shared_ptr<ABCondition> cond, ABEffectChain thenChain,
               ABEffectChain elseChain = ABEffectChain{});
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    std::shared_ptr<ABCondition> cond_;
    ABEffectChain thenChain_;
    ABEffectChain elseChain_;
};

// Begin a channeled cast on the caster (ref ABActionBeginChanneling):
// installs a CBehaviorChannel holding for `durationTicks` (per-tick
// `tick` chain each tick, `finish` chain once at the end). Single-shot:
// the chains move into the behavior on Run, later Runs are no-ops.
class ABActionBeginChannel : public ABAction {
public:
    ABActionBeginChannel(int durationTicks, ABEffectChain tick,
                         ABEffectChain finish);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    int durationTicks_;
    ABEffectChain tick_;
    ABEffectChain finish_;
    bool spent_ = false;
};

// Reveal a circular area for the caster's player (ref vision modifier
// actions, e.g. Far Sight): creates and starts a fog modifier at the
// effect center. A positive duration auto-removes it afterwards.
class ABActionCreateVision : public ABAction {
public:
    ABActionCreateVision(float radius, float duration = 0.0f);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float radius_;
    float duration_;
};

// Homing damage missile at the unit target (ref StormBoltMissile +
// ABActionCreateUnitTargetedProjectile): the missile carries the damage
// and applies the stun in its on-hit hook, so both land on impact and a
// mid-flight death fizzles the whole effect.
class ABActionMissileStun : public ABAction {
public:
    ABActionMissileStun(float damage, float stunDuration, float speed = 800.0f,
                        CAttackType attackType = CAttackType::SPELLS,
                        CDamageType damageType = CDamageType::LIGHTNING);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float damage_;
    float stunDuration_;
    float speed_;
    CAttackType attackType_;
    CDamageType damageType_;
};

// Dispel an area: strip non-timed-life buffs around the effect center
// (ref dispel patterns; Abolish Magic spares timed life).
class ABActionDispelArea : public ABAction {
public:
    ABActionDispelArea(float radius, bool removePositive, bool removeNegative);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    float radius_;
    bool removePositive_;
    bool removeNegative_;
};

// Dispel a single unit target (ref Purge / Dispel Magic per-unit step;
// keepTimedLife=true so summons survive the strip itself -- summon burn
// is a separate chained action).
class ABActionDispelTarget : public ABAction {
public:
    ABActionDispelTarget(bool removePositive, bool removeNegative);
    void Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
             bool hasPoint, float x, float y) override;

private:
    bool removePositive_;
    bool removeNegative_;
};
