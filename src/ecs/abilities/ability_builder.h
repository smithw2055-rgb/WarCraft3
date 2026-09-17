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
