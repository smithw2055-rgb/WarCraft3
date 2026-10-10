#pragma once
// Campaign hero carry-over (retail ChangeLevel): plain-data snapshot of a
// hero plus sim-only restore. Split out of GameController so the transform
// is unit-testable without a renderer; GameController only handles map
// positions and the collect/consume lifecycle.
#include <string>
#include <utility>
#include <vector>

#include "resource/slk.h"

class CUnit;
class CSimulation;

// Heroes of human players travel to the next map with level, XP, unspent
// skill points, learned skills (code + level) and inventory (type +
// charges, in slot order). Plain data so it survives the sim teardown
// between maps.
struct CarriedHero {
    War3ID typeId;
    int ownerPlayer = -1;
    int level = 1;
    int xp = 0;
    int skillPoints = 1;
    std::string properName;
    std::vector<std::pair<War3ID, int>> learnedSkills;
    struct CarriedItem {
        War3ID typeId;
        int charges = 0;
    };
    std::vector<CarriedItem> items;
};

// True when the unit carries over: live, hero-typed, human-owned, with a
// hero ability attached. Dead heroes stay behind (documented retail
// approximation: retail revives them on arrival; ours drops them).
bool IsCarriableHero(CUnit* unit, const CSimulation& sim);

// Empty typeId (War3ID(0)) when not carriable; otherwise the full snapshot.
CarriedHero SnapshotHero(CUnit* unit, const CSimulation& sim);

// Recreate a snapshot at (x, y): hero with level/XP/points/name, learned
// skills granted at recorded levels, inventory refilled in slot order
// (unplaceable items drop at the hero's feet). Null when the type or the
// owner is absent from this sim (e.g. custom next map without the hero).
CUnit* RespawnCarriedHero(CSimulation& sim, const CarriedHero& carried,
                          float x, float y);
