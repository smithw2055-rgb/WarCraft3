#include "world_collision.h"
#include "ecs/unit.h"
#include "ecs/item.h"
#include "ecs/spatial_grid.h"
#include <algorithm>
#include <cmath>
#include <limits>

CWorldCollision::CWorldCollision() = default;

void CWorldCollision::AddUnit(CUnit* unit) {
    if (!unit) return;
    allUnits_.push_back(unit);
}

void CWorldCollision::RemoveUnit(CUnit* unit) {
    auto it = std::find(allUnits_.begin(), allUnits_.end(), unit);
    if (it != allUnits_.end()) {
        allUnits_.erase(it);
    }
}

void CWorldCollision::UpdateUnit(CUnit* unit) {
    (void)unit;
}

void CWorldCollision::AddItem(CItem* item) {
    if (!item) return;
    allItems_.push_back(item);
}

void CWorldCollision::RemoveItem(CItem* item) {
    auto it = std::find(allItems_.begin(), allItems_.end(), item);
    if (it != allItems_.end()) {
        allItems_.erase(it);
    }
}

void CWorldCollision::Rebuild(const std::vector<CUnit*>& units, const std::vector<CItem*>& items) {
    allUnits_ = units;
    allItems_ = items;
}

void CWorldCollision::EnumUnitsInRect(float x, float y, float w, float h,
                                      std::vector<CUnit*>& out) const {
    ++rangeQueriesPerformed_;
    const float maxX = x + w;
    const float maxY = y + h;
    for (auto* u : allUnits_) {
        if (!u || u->IsHidden() || u->IsDead()) continue;
        const float ux = u->GetX();
        const float uy = u->GetY();
        if (ux >= x && ux <= maxX && uy >= y && uy <= maxY) {
            out.push_back(u);
        }
    }
}

void CWorldCollision::EnumUnitsInRange(float cx, float cy, float radius,
                                       std::vector<CUnit*>& out) const {
    ++rangeQueriesPerformed_;
    if (radius < 0.0f) return;
    const float r2 = radius * radius;
    for (auto* u : allUnits_) {
        if (!u || u->IsHidden() || u->IsDead()) continue;
        const float dx = u->GetX() - cx;
        const float dy = u->GetY() - cy;
        if (dx * dx + dy * dy <= r2) {
            out.push_back(u);
        }
    }
}

CUnit* CWorldCollision::FindNearestUnit(float cx, float cy, float maxRadius,
                                        const std::function<bool(const CUnit*)>& predicate) const {
    ++rangeQueriesPerformed_;
    if (maxRadius < 0.0f) return nullptr;
    float bestDistSq = maxRadius * maxRadius;
    CUnit* best = nullptr;
    for (auto* u : allUnits_) {
        if (!u || u->IsHidden() || u->IsDead()) continue;
        if (predicate && !predicate(u)) continue;
        const float dx = u->GetX() - cx;
        const float dy = u->GetY() - cy;
        const float dSq = dx * dx + dy * dy;
        if (dSq <= bestDistSq) {
            bestDistSq = dSq;
            best = u;
        }
    }
    return best;
}

size_t CWorldCollision::CountUnitsInRange(float cx, float cy, float radius) const {
    ++rangeQueriesPerformed_;
    if (radius < 0.0f) return 0;
    const float r2 = radius * radius;
    size_t count = 0;
    for (auto* u : allUnits_) {
        if (!u || u->IsHidden() || u->IsDead()) continue;
        const float dx = u->GetX() - cx;
        const float dy = u->GetY() - cy;
        if (dx * dx + dy * dy <= r2) {
            ++count;
        }
    }
    return count;
}

void CWorldCollision::EnumItemsInRect(float x, float y, float w, float h,
                                      std::vector<CItem*>& out) const {
    const float maxX = x + w;
    const float maxY = y + h;
    for (auto* item : allItems_) {
        if (!item || item->IsHidden() || item->IsDead()) continue;
        const float ix = item->GetX();
        const float iy = item->GetY();
        if (ix >= x && ix <= maxX && iy >= y && iy <= maxY) {
            out.push_back(item);
        }
    }
}

bool CWorldCollision::CheckCollision(float x, float y, float radius,
                                     CUnit* ignoreUnit) const {
    ++collisionChecksPerformed_;
    const float queryRadius = std::max(radius, 8.0f);
    for (auto* u : allUnits_) {
        if (!u || u == ignoreUnit || u->IsHidden() || u->IsDead()) continue;
        const float otherRadius = std::max(u->GetCollisionSize(), 8.0f);
        const float combinedRadius = queryRadius + otherRadius;
        const float r2 = combinedRadius * combinedRadius;
        const float dx = u->GetX() - x;
        const float dy = u->GetY() - y;
        const float d2 = dx * dx + dy * dy;
        if (d2 < r2) {
            if (ignoreUnit) {
                const float prevDx = u->GetX() - ignoreUnit->GetX();
                const float prevDy = u->GetY() - ignoreUnit->GetY();
                const float prevD2 = prevDx * prevDx + prevDy * prevDy;
                // If already penetrating/overlapping and this step increases or maintains distance, allow separating movement!
                if (prevD2 < r2 && d2 >= prevD2) {
                    continue;
                }
            }
            return true;
        }
    }
    return false;
}
