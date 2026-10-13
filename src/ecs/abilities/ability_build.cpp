#include "ability_build.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/behaviors/behavior_build.h"
#include "ecs/player.h"
#include "data/unitdata.h"
#include "map/wpm.h"
#include <cmath>
#include <iostream>

CAbilityBuild::CAbilityBuild(int handleId, War3ID code, CBuildRace race)
    : CAbility(handleId, code, CAbilityCategory::BUILD), race_(race) {
    iconShowing_ = true;
}

int CAbilityBuild::GetBaseOrderId() const {
    switch (race_) {
        case CBuildRace::HUMAN:     return OrderIds::HUMAN_BUILD;
        case CBuildRace::ORC:       return OrderIds::ORC_BUILD;
        case CBuildRace::UNDEAD:    return OrderIds::UNDEAD_BUILD;
        case CBuildRace::NIGHT_ELF: return OrderIds::NIGHT_ELF_BUILD;
        // Ref CAbilityNagaBuild builds orc-style; neutral has no dedicated
        // order set (pollNextOrderBehavior), human default is closest.
        case CBuildRace::NAGA:      return OrderIds::ORC_BUILD;
        default: return OrderIds::HUMAN_BUILD;
    }
}

void CAbilityBuild::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
    // Build ability doesn't create persistent behaviors on add
    // Behaviors are created per-order in Begin()
}

void CAbilityBuild::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
}

bool CAbilityBuild::CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const {
    (void)game; (void)unit; (void)x; (void)y;
    if (IsDisabled()) return false;

    War3ID buildingTypeId(static_cast<uint32_t>(orderId));
    return buildableStructures_.count(buildingTypeId) > 0;
}

bool CAbilityBuild::CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const {
    (void)game; (void)unit;
    if (IsDisabled()) return false;
    // Cancel build
    return orderId == OrderIds::CANCEL;
}

CBehavior* CAbilityBuild::Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) {
    (void)orderId;
    // The orderId encodes which building to build
    // For now, we use the orderId directly as the building type
    // In a full implementation, the UI would pass the building type
    War3ID buildingTypeId(static_cast<uint32_t>(orderId));

    // Snap to grid (footprint-aware when the type has a pathTex)
    if (auto* unitData = game.GetUnitData()) {
        RoundBuildPositionForType(unitData->GetUnitType(buildingTypeId), x, y);
    } else {
        RoundBuildPosition(x, y);
    }

    // Validate placement
    if (IsBuildLocationObstructed(game, x, y, caster->GetPlayerIndex(), buildingTypeId)) {
        std::cout << "Build location obstructed!" << std::endl;
        return nullptr;
    }

    // Check if player can afford it
    auto* player = game.GetPlayer(caster->GetPlayerIndex());
    auto* unitData = game.GetUnitData();
    if (player && unitData) {
        auto* buildType = unitData->GetUnitType(buildingTypeId);
        if (buildType) {
            if (player->GetGold() < buildType->GetGoldCost()) {
                std::cout << "Not enough gold!" << std::endl;
                return nullptr;
            }
            if (player->GetLumber() < buildType->GetLumberCost()) {
                std::cout << "Not enough lumber!" << std::endl;
                return nullptr;
            }
        }
    }

    // Create build behavior
    auto* behavior = new CBehaviorBuild();
    behavior->SetUnit(caster);
    behavior->SetBuildTarget(x, y, buildingTypeId, race_);
    return behavior;
}

CBehavior* CAbilityBuild::BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    (void)game; (void)caster; (void)orderId;
    return nullptr;
}

void CAbilityBuild::RoundBuildPosition(float& x, float& y) {
    // Snap to 64-unit grid (standard WC3 building grid)
    x = std::round(x / 64.0f) * 64.0f;
    y = std::round(y / 64.0f) * 64.0f;
}

void CAbilityBuild::RoundBuildPositionForType(const CUnitType* type, float& x, float& y) {
    RoundBuildPosition(x, y);
    auto img = type ? type->GetPathTexImage() : nullptr;
    if (img && img->valid) {
        if (((img->w / 2) % 2) == 1) x += 32.0f;
        if (((img->h / 2) % 2) == 1) y += 32.0f;
    }
}

bool CAbilityBuild::IsBuildLocationObstructed(CSimulation& game, float x, float y,
                                               int playerIndex, War3ID buildingTypeId) {
    (void)playerIndex;
    auto* unitData = game.GetUnitData();
    if (!unitData) return true;

    auto* buildType = unitData->GetUnitType(buildingTypeId);
    if (!buildType) return true;

    // Check collision size of the building
    float collisionSize = buildType->GetCollisionSize();
    if (collisionSize <= 0) collisionSize = 64.0f;

    // Footprint vs the pathing grid. An empty prevented set still blocks
    // UNBUILDABLE cells (retail ground buildings). Required types must
    // cover every cell of the footprint (blight for haunted ground). The
    // rect comes from the type's pathTex footprint when decoded (ref
    // checkPathingTexture: the texture only shapes the rect; the map's
    // flags are tested), else the legacy collision-size square.
    auto* pathfinding = game.GetPathfinding();
    if (pathfinding && pathfinding->HasPathingMap()) {
        const WpmData* wpm = pathfinding->GetPathingMap();
        auto prevented = buildType->GetPreventedPathingTypes();
        if (prevented.empty()) prevented.insert(CBuildingPathingType::UNBUILDABLE);
        const auto& required = buildType->GetRequiredPathingTypes();
        int x0, x1, y0, y1;
        auto pathTex = buildType->GetPathTexImage();
        if (pathTex && pathTex->valid) {
            const int rot = (int)game.GetGameplayConstants().buildingAngle;
            int rx, ry, rw, rh;
            PathTexRotate(0, 0, pathTex->w, pathTex->h, rot, rx, ry, rw, rh);
            const int baseCx = pathfinding->WorldToCellX(x);
            const int baseCy = pathfinding->WorldToCellY(y);
            x0 = baseCx - rw / 2;
            x1 = x0 + rw - 1;
            y0 = baseCy - rh / 2;
            y1 = y0 + rh - 1;
        } else {
            x0 = pathfinding->WorldToCellX(x - collisionSize);
            x1 = pathfinding->WorldToCellX(x + collisionSize);
            y0 = pathfinding->WorldToCellY(y - collisionSize);
            y1 = pathfinding->WorldToCellY(y + collisionSize);
        }
        auto flagOf = [](CBuildingPathingType type) -> uint8_t {
            switch (type) {
                case CBuildingPathingType::UNWALKABLE: return WpmData::UNWALKABLE;
                case CBuildingPathingType::UNFLYABLE: return WpmData::UNFLYABLE;
                case CBuildingPathingType::UNBUILDABLE:
                case CBuildingPathingType::UNFIXABLE: return WpmData::UNBUILDABLE;
                case CBuildingPathingType::BLIGHTED: return WpmData::BLIGHTED;
                default: return 0;
            }
        };
        auto cellFlag = [&](int cx, int cy, uint8_t flag) {
            if (cx < 0 || cy < 0 || cx >= wpm->width || cy >= wpm->height) return false;
            return (wpm->pathing[(size_t)cy * (size_t)wpm->width + (size_t)cx] & flag) != 0;
        };
        for (int cy = y0; cy <= y1; ++cy) {
            for (int cx = x0; cx <= x1; ++cx) {
                if (cx < 0 || cy < 0 || cx >= wpm->width || cy >= wpm->height) return true;
                for (CBuildingPathingType type : prevented) {
                    const uint8_t flag = flagOf(type);
                    if (flag && cellFlag(cx, cy, flag)) return true;
                }
                for (CBuildingPathingType type : required) {
                    const uint8_t flag = flagOf(type);
                    if (!flag) continue;
                    if (type == CBuildingPathingType::BLIGHTED) {
                        const float wx = pathfinding->CellToWorldX(cx);
                        const float wy = pathfinding->CellToWorldY(cy);
                        if (!cellFlag(cx, cy, flag) && !game.IsPointBlighted(wx, wy)) return true;
                        continue;
                    }
                    if (!cellFlag(cx, cy, flag)) return true;
                }
            }
        }
    }

    // Check for overlapping units
    for (auto* unit : game.GetUnits()) {
        if (unit->IsDead()) continue;
        float dx = unit->GetX() - x;
        float dy = unit->GetY() - y;
        float dist = std::sqrt(dx * dx + dy * dy);
        float minDist = collisionSize + unit->GetCollisionSize();
        if (dist < minDist) {
            return true; // obstructed
        }
    }

    return false;
}
