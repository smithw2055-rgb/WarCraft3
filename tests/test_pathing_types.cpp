// P1-14 movement-type pathing: FLY/FLOAT/AMPHIBIOUS read their own WPM
// bits (ref PathingGrid.MovementType); boundary ring fails everything.
#include <vector>

#include "ecs/pathing/pathfinding.h"
#include "ecs/unit_type.h"
#include "map/wpm.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)

namespace {

WpmData MakeTypeGrid() {
    WpmData wpm;
    wpm.width = 5;
    wpm.height = 1;
    wpm.pathing = {
        (uint8_t)WpmData::UNSWIMABLE, // land (walkable, ships cannot sail)
        (uint8_t)WpmData::UNWALKABLE, // deep water (swimmable, foot cannot walk)
        (uint8_t)(WpmData::UNWALKABLE | WpmData::UNSWIMABLE), // wall
        (uint8_t)WpmData::UNFLYABLE, // canopy
        (uint8_t)WpmData::BOUNDARY, // map edge
    };
    return wpm;
}

float CellCenterX(int cell) { return (float)cell * 32.0f + 16.0f; }

bool PathableAt(CPathfindingProcessor& pf, int cell, PathingGridMovementType type) {
    return pf.IsWorldPointPathableForUnit(CellCenterX(cell), 16.0f, 0.0f, type, nullptr);
}

} // namespace

bool TestMovementTypePathingBits() {
    WpmData wpm = MakeTypeGrid();
    CPathfindingProcessor pf;
    pf.SetPathingMap(&wpm, 0.0f, 0.0f);
    using M = PathingGridMovementType;
    // FOOT: land/canopy only.
    TEST_ASSERT(PathableAt(pf, 0, M::FOOT));
    TEST_ASSERT(!PathableAt(pf, 1, M::FOOT));
    TEST_ASSERT(!PathableAt(pf, 2, M::FOOT));
    TEST_ASSERT(PathableAt(pf, 3, M::FOOT));
    TEST_ASSERT(!PathableAt(pf, 4, M::FOOT));
    // FLOAT: swimmable only (UNSWIMABLE clear); wall blocks ships too.
    TEST_ASSERT(!PathableAt(pf, 0, M::FLOAT));
    TEST_ASSERT(PathableAt(pf, 1, M::FLOAT));
    TEST_ASSERT(!PathableAt(pf, 2, M::FLOAT));
    TEST_ASSERT(PathableAt(pf, 3, M::FLOAT));
    TEST_ASSERT(!PathableAt(pf, 4, M::FLOAT));
    // AMPHIBIOUS: either ground or water; the wall fails both.
    TEST_ASSERT(PathableAt(pf, 0, M::AMPHIBIOUS));
    TEST_ASSERT(PathableAt(pf, 1, M::AMPHIBIOUS));
    TEST_ASSERT(!PathableAt(pf, 2, M::AMPHIBIOUS));
    TEST_ASSERT(PathableAt(pf, 3, M::AMPHIBIOUS));
    TEST_ASSERT(!PathableAt(pf, 4, M::AMPHIBIOUS));
    // FLY: everything but canopy and edge.
    TEST_ASSERT(PathableAt(pf, 0, M::FLY));
    TEST_ASSERT(PathableAt(pf, 1, M::FLY));
    TEST_ASSERT(PathableAt(pf, 2, M::FLY));
    TEST_ASSERT(!PathableAt(pf, 3, M::FLY));
    TEST_ASSERT(!PathableAt(pf, 4, M::FLY));
    return true;
}

bool TestBoundaryFlagQueries() {
    WpmData wpm = MakeTypeGrid();
    TEST_ASSERT(!wpm.IsBoundary(0, 0));
    TEST_ASSERT(wpm.IsBoundary(4, 0));
    TEST_ASSERT(wpm.IsBoundary(-1, 0)); // off-grid counts as boundary
    TEST_ASSERT(wpm.IsBoundary(0, 5));
    // Off-map movement fails for every type (was already false via OOB).
    CPathfindingProcessor pf;
    pf.SetPathingMap(&wpm, 0.0f, 0.0f);
    using M = PathingGridMovementType;
    TEST_ASSERT(!pf.IsWorldPointPathableForUnit(-100.0f, -100.0f, 0.0f, M::FOOT, nullptr));
    TEST_ASSERT(!pf.IsWorldPointPathableForUnit(-100.0f, -100.0f, 0.0f, M::FLY, nullptr));
    return true;
}

bool TestMaplessPathingPasses() {
    CPathfindingProcessor pf; // no map: everything passes, nothing crashes
    using M = PathingGridMovementType;
    TEST_ASSERT(pf.IsWorldPointPathableForUnit(0.0f, 0.0f, 0.0f, M::FLOAT, nullptr));
    TEST_ASSERT(pf.IsCellPathableFor(3, 3, M::AMPHIBIOUS));
    return true;
}
