#include "behavior_item.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/item.h"
#include "ecs/abilities/ability_inventory.h"
#include "ecs/pathing/world_collision.h"
#include <cmath>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

bool StepToward(CSimulation& simulation, CUnit* unit, float tx, float ty, float stopRange) {
    const float dx = tx - unit->GetX();
    const float dy = ty - unit->GetY();
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= stopRange) return true; // in range
    if (!unit->TurnTowardPoint(tx, ty)) return false;
    const float speed = unit->GetSpeed() * simulation.GetSimStepTime();
    if (speed <= 0.0f || dist <= 0.0f) return false;
    const float step = std::min(speed, dist - stopRange + 8.0f);
    if (step <= 0.0f) return true;
    const float nx = unit->GetX() + (dx / dist) * step;
    const float ny = unit->GetY() + (dy / dist) * step;
    if (auto* collision = simulation.GetWorldCollision()) {
        const float radius = std::max(unit->GetCollisionSize(), 16.0f);
        if (collision->CheckCollision(nx, ny, radius, unit)) {
            return false; // blocked; retry next tick
        }
    }
    unit->SetPosition(nx, ny);
    return false;
}

CAbilityInventory* GetInventory(CUnit* unit) {
    if (!unit) return nullptr;
    return dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv")));
}

} // namespace

// ---------------------------------------------------------------------------
// CBehaviorGetItem
// ---------------------------------------------------------------------------
CBehaviorGetItem::CBehaviorGetItem(float pickupRange) : pickupRange_(pickupRange) {}

void CBehaviorGetItem::Begin(CSimulation& simulation) { (void)simulation; }
void CBehaviorGetItem::End(CSimulation& simulation) { (void)simulation; }

bool CBehaviorGetItem::TryPickupNow(CSimulation& simulation) {
    if (!unit_ || !item_) return true;
    if (item_->IsDead() || item_->IsInInventory()) return true;
    auto* inv = GetInventory(unit_);
    if (!inv) return true; // no inventory: order fizzles (ref no-op)
    inv->PickUpItem(simulation, unit_, item_);
    return true;
}

bool CBehaviorGetItem::Update(CSimulation& simulation) {
    if (!unit_ || unit_->IsDead()) return true;
    if (!item_ || item_->IsDead() || item_->IsInInventory()) return true;
    if (StepToward(simulation, unit_, item_->GetX(), item_->GetY(), pickupRange_)) {
        return TryPickupNow(simulation);
    }
    return false;
}

// ---------------------------------------------------------------------------
// CBehaviorDropItem
// ---------------------------------------------------------------------------
CBehaviorDropItem::CBehaviorDropItem(int slot, bool hasPoint, float x, float y)
    : slot_(slot), hasPoint_(hasPoint), x_(x), y_(y) {}

void CBehaviorDropItem::Begin(CSimulation& simulation) { (void)simulation; }

bool CBehaviorDropItem::Update(CSimulation& simulation) {
    if (!unit_ || unit_->IsDead()) return true;
    auto* inv = GetInventory(unit_);
    if (!inv) return true;
    if (hasPoint_) {
        inv->DropItemAt(simulation, unit_, slot_, x_, y_);
    } else {
        inv->DropItem(simulation, unit_, slot_);
    }
    return true; // instant
}

// ---------------------------------------------------------------------------
// CBehaviorGiveItem
// ---------------------------------------------------------------------------
CBehaviorGiveItem::CBehaviorGiveItem(int slot, float giveRange)
    : slot_(slot), giveRange_(giveRange) {}

void CBehaviorGiveItem::Begin(CSimulation& simulation) { (void)simulation; }
void CBehaviorGiveItem::End(CSimulation& simulation) { (void)simulation; }

bool CBehaviorGiveItem::TryGiveNow(CSimulation& simulation) {
    if (!unit_ || !target_ || target_->IsDead()) return true;
    auto* giver = GetInventory(unit_);
    auto* receiver = GetInventory(target_);
    if (!giver || !receiver) return true;
    CItem* item = giver->GetItem(slot_);
    if (!item) return true;
    // Detach from giver without scattering to the ground: drop to feet
    // first is wrong (fires drop events); instead remove from the slot
    // via DropItemAt to a scratch position is avoided -- do a direct
    // transfer: clear giver slot, then receiver pickup.
    const int giverSlot = giver->FindItemSlot(item);
    if (giverSlot < 0) return true;
    // Use the public drop-then-pickup path so ability grant/revoke and
    // item events stay consistent (ref GiveItemToHero drops + picks up).
    const float gx = unit_->GetX();
    const float gy = unit_->GetY();
    if (!giver->DropItemAt(simulation, unit_, giverSlot, gx, gy)) return true;
    if (!receiver->PickUpItem(simulation, target_, item)) {
        // Receiver full: leave it on the ground where the giver stood.
        return true;
    }
    return true;
}

bool CBehaviorGiveItem::Update(CSimulation& simulation) {
    if (!unit_ || unit_->IsDead()) return true;
    if (!target_ || target_->IsDead()) return true;
    if (StepToward(simulation, unit_, target_->GetX(), target_->GetY(), giveRange_)) {
        return TryGiveNow(simulation);
    }
    return false;
}
