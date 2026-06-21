#pragma once
#include <cstdint>
#include <vector>
#include <unordered_set>
#include "ecs/fwd.h"

// Simplified spatial partition - for now a simple grid-based approach
// In the full version this would use a quadtree
class CWorldCollision {
public:
    CWorldCollision();

    void AddUnit(CUnit* unit);
    void RemoveUnit(CUnit* unit);
    void UpdateUnit(CUnit* unit);

    void EnumUnitsInRect(float x, float y, float w, float h,
                         std::vector<CUnit*>& out) const;
    void EnumUnitsInRange(float cx, float cy, float radius,
                          std::vector<CUnit*>& out) const;

    bool CheckCollision(float x, float y, float radius,
                        CUnit* ignoreUnit = nullptr) const;

private:
    std::vector<CUnit*> allUnits_;
};
