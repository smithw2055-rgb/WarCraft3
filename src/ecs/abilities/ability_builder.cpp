#include "ecs/abilities/ability_builder.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/buffs.h"
#include <cmath>

ABActionDamageArea::ABActionDamageArea(float radius, float amount,
                                       CAttackType attackType, CDamageType damageType)
    : radius_(radius), amount_(amount), attackType_(attackType), damageType_(damageType) {}

void ABActionDamageArea::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                             bool hasPoint, float x, float y) {
    float cx = x, cy = y;
    if (unitTarget) {
        cx = unitTarget->GetX();
        cy = unitTarget->GetY();
    } else if (!hasPoint && caster) {
        cx = caster->GetX();
        cy = caster->GetY();
    }
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        float dx = unit->GetX() - cx;
        float dy = unit->GetY() - cy;
        if (dx * dx + dy * dy <= radius_ * radius_) {
            unit->Damage(game, caster, false, false, attackType_, damageType_, "", amount_, 0);
        }
    }
}

ABActionHealArea::ABActionHealArea(float radius, float amount)
    : radius_(radius), amount_(amount) {}

void ABActionHealArea::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                           bool hasPoint, float x, float y) {
    float cx = x, cy = y;
    if (unitTarget) {
        cx = unitTarget->GetX();
        cy = unitTarget->GetY();
    } else if (!hasPoint && caster) {
        cx = caster->GetX();
        cy = caster->GetY();
    }
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        float dx = unit->GetX() - cx;
        float dy = unit->GetY() - cy;
        if (dx * dx + dy * dy <= radius_ * radius_) {
            float newLife = unit->GetLife() + amount_;
            if (newLife > unit->GetMaxLife()) newLife = unit->GetMaxLife();
            unit->SetLife(game, newLife);
        }
    }
}

ABActionApplyBuffArea::ABActionApplyBuffArea(float radius, BuffFactory factory)
    : radius_(radius), factory_(std::move(factory)) {}

void ABActionApplyBuffArea::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                bool hasPoint, float x, float y) {
    if (!factory_) return;
    float cx = x, cy = y;
    if (unitTarget) {
        cx = unitTarget->GetX();
        cy = unitTarget->GetY();
    } else if (!hasPoint && caster) {
        cx = caster->GetX();
        cy = caster->GetY();
    }
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        float dx = unit->GetX() - cx;
        float dy = unit->GetY() - cy;
        if (dx * dx + dy * dy <= radius_ * radius_) {
            if (auto buff = factory_) {
                unit->AddBuff(buff(), game);
            }
        }
    }
}

ABActionSummon::ABActionSummon(War3ID typeId, War3ID buffAlias, int count,
                               float duration, float spread)
    : typeId_(typeId), buffAlias_(buffAlias), count_(count),
      duration_(duration), spread_(spread) {}

void ABActionSummon::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                         bool hasPoint, float x, float y) {
    float cx = x, cy = y;
    if (unitTarget) {
        cx = unitTarget->GetX();
        cy = unitTarget->GetY();
    } else if (!hasPoint && caster) {
        cx = caster->GetX();
        cy = caster->GetY();
    }
    for (int i = 0; i < count_; i++) {
        float ox = (i - count_ / 2) * spread_;
        SummonTimedUnit(game, caster, typeId_, buffAlias_,
                        cx + ox, cy + spread_ * 0.5f,
                        caster ? caster->GetFacing() : 0.0f, duration_);
    }
}

void ABEffectChain::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                        bool hasPoint, float x, float y) const {
    for (const auto& action : actions_) {
        if (action) action->Run(game, caster, unitTarget, hasPoint, x, y);
    }
}

AbilityBuilderRegistry& AbilityBuilderRegistry::Instance() {
    static AbilityBuilderRegistry instance;
    return instance;
}

void AbilityBuilderRegistry::Register(War3ID alias, ABEffectChain chain) {
    chains_[alias.value] = std::move(chain);
}

const ABEffectChain* AbilityBuilderRegistry::Find(War3ID alias) const {
    auto it = chains_.find(alias.value);
    return it != chains_.end() ? &it->second : nullptr;
}

CUnit* SummonTimedUnit(CSimulation& game, CUnit* caster, War3ID typeId,
                       War3ID buffAlias, float x, float y, float facing,
                       float duration) {
    if (!caster) return nullptr;
    CUnit* summoned = game.CreateUnit(typeId, caster->GetPlayerIndex(), x, y, facing);
    if (!summoned) return nullptr;
    summoned->AddBuff(std::make_unique<CBuffTimedLife>(buffAlias, duration), game);
    if (const auto& cb = game.GetUnitSummonedCallback()) {
        cb(caster->GetHandleId(), summoned->GetHandleId());
    }
    return summoned;
}
