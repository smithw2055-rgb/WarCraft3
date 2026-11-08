#pragma once
#include <cstdint>
#include <memory>
#include "ecs/fwd.h"
#include "resource/slk.h"

class CAbility;

enum class CBehaviorCategory : uint8_t {
    NONE, MOVE, ATTACK, STOP, HOLD_POSITION, FOLLOW, PATROL, ATTACK_MOVE,
    BOARD_TRANSPORT, STUN, SPELL, BUILD, HARVEST, RETURN_RESOURCES,
    INVENTORY, ROOT, CUSTOM
};

// ============================================================================
// CBehavior (CUnitBehavior) - Engine-Level Pluggable Unit State Machine (Phase 23–24)
//
// Provides:
// 1. A unified polymorphic contract for movement, combat, harvesting,
//    construction, spell-casting, crowd-control, and dynamically registered custom
//    plugin behaviors (`War3GameModule` v23–v24).
// 2. Engine-level static factory constructors (`CBehavior::Create*`) so that
//    `CUnit`, `CBuff`, `CAbility*`, `animation_state`, and `animation_utils`
//    have ZERO `#include "ecs/behaviors/behavior_*.h"` couplings and ZERO
//    `dynamic_cast<CBehavior*>` calls across the entire `src/` tree.
// ============================================================================
class CBehavior {
public:
    virtual ~CBehavior() = default;
    virtual CBehaviorCategory GetCategory() const = 0;
    virtual War3ID GetCustomBehaviorCode() const { return War3ID(); }
    virtual bool Update(CSimulation& simulation) = 0;
    virtual void Begin(CSimulation& simulation) { (void)simulation; }
    virtual void End(CSimulation& simulation) { (void)simulation; }

    // New orders preempt the current behavior when true (ref
    // CBehavior.interruptable; only stun returns false). Deferred orders
    // wait in the queue instead.
    virtual bool IsInterruptable() const { return true; }

    // ===== Unified Target & Navigation Polymorphic Contract =====
    virtual void SetTarget(CWidget* target) { (void)target; }
    virtual void SetTarget(CWidget* target, float range) { SetTarget(target); (void)range; }
    virtual CWidget* GetTarget() const { return nullptr; }
    virtual void SetTarget(float x, float y) { (void)x; (void)y; }
    virtual void SetTargetPoint(float x, float y) { SetTarget(x, y); }
    virtual void SetDestination(float x, float y) { SetTarget(x, y); }
    virtual void SetPoints(float x1, float y1, float x2, float y2) { (void)x1; (void)y1; (void)x2; (void)y2; }
    virtual void SetTargetWidget(CWidget* target) { SetTarget(target); }

    // FlowField + RVO Crowd Steering telemetry & configuration
    virtual void SetUseFlowFieldSteering(bool enabled) { (void)enabled; }
    virtual bool IsUsingFlowFieldSteering() const { return false; }
    virtual uint64_t GetFlowFieldSteeringSteps() const { return 0; }
    virtual uint64_t GetRvoSidestepAdjustments() const { return 0; }

    // ===== Visual & Animation State Polymorphic Query Contract =====
    virtual bool TracksPropellingState() const { return false; }
    virtual bool IsPropelling() const { return false; }
    virtual bool IsMovingToWorkSite() const { return false; }
    virtual bool IsActivelyWorking() const { return false; }
    virtual bool IsMovingToDropoff() const { return false; }
    virtual bool IsDepositing() const { return false; }
    virtual bool IsMovingToRange() const { return false; }
    virtual bool IsFacingTarget() const { return false; }
    virtual bool IsCasting() const { return false; }
    virtual bool IsBackswing() const { return false; }
    virtual float GetSpellAnimationDuration() const { return 0.0f; }
    virtual bool IsAttackAnimationActive(int currentTick) const { (void)currentTick; return false; }
    virtual int GetAttackAnimationStartTick() const { return -1; }
    virtual float GetAttackAnimationDuration() const { return 0.0f; }

    // ===== Gameplay Action & Resource Hook Contract =====
    virtual bool TryCancelConstruction(CSimulation& simulation) { (void)simulation; return false; }
    virtual void SetDuration(float duration) { (void)duration; }
    virtual float GetRemaining() const { return 0.0f; }
    virtual void SetRemaining(float remaining) { (void)remaining; }
    virtual int GetGoldCapacity() const { return 0; }
    virtual int GetPopoutFromMineTurnTick() const { return 0; }
    virtual void PopoutFromMine(int goldMined) { (void)goldMined; }

    // ===== Phase 24 Engine-Level Static Behavior Factories =====
    static std::unique_ptr<CBehavior> CreateMove();
    static std::unique_ptr<CBehavior> CreateFollow();
    static std::unique_ptr<CBehavior> CreatePatrol();
    static std::unique_ptr<CBehavior> CreateHoldPosition();
    static std::unique_ptr<CBehavior> CreateStop();
    static std::unique_ptr<CBehavior> CreateAttack();
    static std::unique_ptr<CBehavior> CreateAttackMove();
    static std::unique_ptr<CBehavior> CreateStun(float duration = 0.0f);

    static CBehavior* CreateLoadPassenger(float loadRange, CWidget* transport);
    static CBehavior* CreateBoardTransport(float loadRange, int cargoCapacity, CWidget* target);
    static CBehavior* CreateDropCargo(float x, float y, float durationSec);
    static CBehavior* CreateRootMorph(float durationSec, bool uproot = false);
    static CBehavior* CreateSpellCast(CUnit* caster, CAbility* spellAbility,
                                      CWidget* target, bool hasPoint, float x, float y,
                                      float castRange, float castPoint, float backswingDuration);
    static CBehavior* CreateBuild(CUnit* caster, float x, float y,
                                  War3ID buildingTypeId, int buildRaceOrd);
    static CBehavior* CreateHarvest(CUnit* caster, CWidget* target,
                                    int resourceTypeOrd, int capacity);
    static CBehavior* CreateReturnResources(CSimulation& sim, CUnit* caster,
                                            const CAbility* harvestAbility,
                                            CWidget* explicitTarget = nullptr);
    static CBehavior* CreateRepair(CUnit* caster, const CAbility* repairAbility,
                                   CWidget* target);

    CUnit* GetUnit() const { return unit_; }
    void SetUnit(CUnit* u) { unit_ = u; }

protected:
    CUnit* unit_ = nullptr;
};

using CUnitBehavior = CBehavior;
