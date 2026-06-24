#include "world_collision.h"
#include "ecs/unit.h"
#include <cmath>

CWorldCollision::CWorldCollision() {}

void CWorldCollision::AddUnit(CUnit* unit) {
    allUnits_.push_back(unit);
}

void CWorldCollision::RemoveUnit(CUnit* unit) {
    for (size_t i = 0; i < allUnits_.size(); i++) {
        if (allUnits_[i] == unit) {
            allUnits_.erase(allUnits_.begin() + i);
            return;
        }
    }
}

void CWorldCollision::UpdateUnit(CUnit* unit) {
    (void)unit;
}

void CWorldCollision::EnumUnitsInRect(float x, float y, float w, float h,
                                       std::vector<CUnit*>& out) const {
    for (auto* u : allUnits_) {
        if (u->IsHidden() || u->IsDead()) continue;
        float ux = u->GetX(), uy = u->GetY();
        if (ux >= x && ux <= x + w && uy >= y && uy <= y + h) {
            out.push_back(u);
        }
    }
}

void CWorldCollision::EnumUnitsInRange(float cx, float cy, float radius,
                                        std::vector<CUnit*>& out) const {
    float r2 = radius * radius;
    for (auto* u : allUnits_) {
        if (u->IsHidden() || u->IsDead()) continue;
        float dx = u->GetX() - cx;
        float dy = u->GetY() - cy;
        if (dx * dx + dy * dy <= r2) {
            out.push_back(u);
        }
    }
}

bool CWorldCollision::CheckCollision(float x, float y, float radius,
                                      CUnit* ignoreUnit) const {
    const float queryRadius = std::max(radius, 8.0f);
    for (auto* u : allUnits_) {
        if (u == ignoreUnit || u->IsHidden() || u->IsDead()) continue;
        const float otherRadius = std::max(u->GetCollisionSize(), 8.0f);
        const float combinedRadius = queryRadius + otherRadius;
        const float r2 = combinedRadius * combinedRadius;
        float dx = u->GetX() - x;
        float dy = u->GetY() - y;
        if (dx * dx + dy * dy < r2) return true;
    }
    return false;
}
