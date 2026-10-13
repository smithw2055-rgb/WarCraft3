#pragma once
#include <cstdint>
#include "ecs/fwd.h"

enum class CBehaviorCategory : uint8_t {
    NONE, MOVE, ATTACK, STOP, HOLD_POSITION, FOLLOW, PATROL, ATTACK_MOVE,
    BOARD_TRANSPORT, STUN, SPELL, BUILD, HARVEST, RETURN_RESOURCES,
    INVENTORY, ROOT
};

class CBehavior {
public:
    virtual ~CBehavior() = default;
    virtual CBehaviorCategory GetCategory() const = 0;
    virtual bool Update(CSimulation& simulation) = 0;
    virtual void Begin(CSimulation& simulation) { (void)simulation; }
    virtual void End(CSimulation& simulation) { (void)simulation; }
    // New orders preempt the current behavior when true (ref
    // CBehavior.interruptable; only stun returns false). Deferred orders
    // wait in the queue instead.
    virtual bool IsInterruptable() const { return true; }
    CUnit* GetUnit() const { return unit_; }
    void SetUnit(CUnit* u) { unit_ = u; }
protected:
    CUnit* unit_ = nullptr;
};
