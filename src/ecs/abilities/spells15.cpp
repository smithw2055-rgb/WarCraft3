#include "spells15.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"

// ===== Phoenix =====
CAbilityPhoenix::CAbilityPhoenix(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHpx"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(180.0f);
    SetManaCost(175);
}

void CAbilityPhoenix::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    // Graceful without unit data (ref summon-without-data).
    CUnit* bird = game.CreateUnit(phoenixType_, caster->GetPlayerIndex(),
                                  caster->GetX() + 64.0f, caster->GetY(), 0);
    if (!bird) return;
    bird->AddBuff(std::make_unique<CBuffPhoenixRebirth>(), game);
    if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
        summonedCb(caster->GetHandleId(), bird->GetHandleId());
    }
}
