#include "ecs/hero_carry.h"

#include "ecs/csimulation.h"
#include "ecs/unit.h"

bool IsCarriableHero(CUnit* unit, const CSimulation& sim) {
    if (!unit) return false;
    if (const auto* mod = sim.GetSimulationModule()) {
        return mod->IsCarriableHero(sim, *unit);
    }
    return false;
}

CarriedHero SnapshotHero(CUnit* unit, const CSimulation& sim) {
    CarriedHero carried;
    if (!unit) return carried;
    if (const auto* mod = sim.GetSimulationModule()) {
        mod->OnSnapshotCarriedHero(sim, *unit, carried);
    }
    return carried;
}

CUnit* RespawnCarriedHero(CSimulation& sim, const CarriedHero& carried,
                          float x, float y) {
    if (carried.typeId.value == 0) return nullptr;
    if (auto* mod = sim.GetSimulationModule()) {
        return mod->OnRespawnCarriedHero(sim, carried, x, y);
    }
    return nullptr;
}
