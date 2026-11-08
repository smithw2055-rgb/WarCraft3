#include "spells21.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "data/abilitydata.h"
#include <cmath>

// ===== Stasis Trap =====
CAbilityStasisTrap::CAbilityStasisTrap(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Asta"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(700.0f);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityStasisTrap::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    const int casterPlayer = caster->GetPlayerIndex();
    CSimulation::SimHazard trap;
    trap.x = x;
    trap.y = y;
    trap.radius = radius_;
    trap.ticksLeft = (int)(lifetime_ / CSimulation::SIM_STEP_TIME);
    trap.tickPeriod = 5; // 4 Hz proximity poll
    trap.tick = [casterPlayer, stun = stunDuration_, radius = radius_](
                    CSimulation& sim, CSimulation::SimHazard& h) {
        bool tripped = false;
        for (CUnit* unit : sim.GetUnits()) {
            if (!unit || unit->IsDead() || IsFriendlyToPlayer(sim, casterPlayer, unit)) continue;
            const float dx = unit->GetX() - h.x;
            const float dy = unit->GetY() - h.y;
            if (dx * dx + dy * dy > radius * radius) continue;
            unit->AddBuff(std::make_unique<CBuffStun>(stun), sim);
            tripped = true;
        }
        return tripped; // consumed on the first trip (retail single-shot)
    };
    game.AddHazard(std::move(trap));
}

void CAbilityStasisTrap::ApplyEffectData(const CAbilityDefFull& def, int level) {
    stunDuration_ = def.DurationAt(level);
    radius_ = def.AreaAt(level);
}

// ===== Tornado =====
CAbilityTornado::CAbilityTornado(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANto"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(800.0f);
    SetCooldown(20.0f);
    SetManaCost(150);
}

void CAbilityTornado::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    const int casterHandle = caster->GetHandleId();
    const int casterPlayer = caster->GetPlayerIndex();
    for (int i = 0; i < count_; i++) {
        CSimulation::SimHazard twister;
        twister.x = x + game.SimRandRange(-64.0f, 64.0f);
        twister.y = y + game.SimRandRange(-64.0f, 64.0f);
        twister.radius = radius_;
        twister.ticksLeft = (int)(duration_ / CSimulation::SIM_STEP_TIME);
        twister.tickPeriod = 20; // toss once per second
        twister.speed = speed_;
        twister.tx = x + game.SimRandRange(-400.0f, 400.0f);
        twister.ty = y + game.SimRandRange(-400.0f, 400.0f);
        twister.tick = [casterHandle, casterPlayer, radius = radius_, damage = damage_](
                           CSimulation& sim, CSimulation::SimHazard& h) {
            const float dx = h.tx - h.x;
            const float dy = h.ty - h.y;
            if (dx * dx + dy * dy < 32.0f * 32.0f) {
                h.tx = h.x + sim.SimRandRange(-400.0f, 400.0f);
                h.ty = h.y + sim.SimRandRange(-400.0f, 400.0f);
            }
            CUnit* caster = sim.GetUnit(casterHandle);
            for (CUnit* unit : sim.GetUnits()) {
                if (!unit || unit->IsDead() || IsFriendlyToPlayer(sim, casterPlayer, unit)) continue;
                const float ux = unit->GetX() - h.x;
                const float uy = unit->GetY() - h.y;
                if (ux * ux + uy * uy > radius * radius) continue;
                sim.GetCombatSystem()->ApplyDamage(sim, caster, unit, false, false,
                                                  CAttackType::SPELLS, CDamageType::MAGIC,
                                                  "", damage, 0.0f, 0, 0);
                unit->AddBuff(std::make_unique<CBuffCyclone>(2.0f), sim);
            }
            return false;
        };
        game.AddHazard(std::move(twister));
    }
}

void CAbilityTornado::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 0, damage_);
    radius_ = def.AreaAt(level);
    duration_ = def.DurationAt(level);
}
