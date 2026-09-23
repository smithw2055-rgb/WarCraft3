#include "ecs/abilities/ability_builder.h"
#include "ecs/ability.h"
#include "ecs/combat/projectile.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "ecs/buffs.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

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

namespace {

// Resolve the effect center the same way the area actions do: unit target
// wins, then the explicit point, then the caster position.
void ResolveCenter(CUnit* caster, CWidget* unitTarget, bool hasPoint,
                   float x, float y, float& outX, float& outY) {
    if (unitTarget) {
        outX = unitTarget->GetX();
        outY = unitTarget->GetY();
    } else if (hasPoint || !caster) {
        outX = x;
        outY = y;
    } else {
        outX = caster->GetX();
        outY = caster->GetY();
    }
}

// Shared iteration filter: life, caster/exclude/faction rules.
bool ShouldVisitUnit(CUnit* caster, CUnit* unit, bool includeCaster,
                     const CWidget* exclude, ABFactionFilter factionFilter,
                     ABLifeFilter lifeFilter = ABLifeFilter::LIVING) {
    if (!unit) return false;
    if (lifeFilter == ABLifeFilter::LIVING && unit->IsDead()) return false;
    if (lifeFilter == ABLifeFilter::DEAD && !unit->IsDead()) return false;
    if (unit == caster && !includeCaster) return false;
    if (unit == exclude) return false;
    if (factionFilter != ABFactionFilter::ANY) {
        if (!caster) return false;
        const bool samePlayer = unit->GetPlayerIndex() == caster->GetPlayerIndex();
        if (factionFilter == ABFactionFilter::SAME_PLAYER_AS_CASTER && !samePlayer) {
            return false;
        }
        if (factionFilter == ABFactionFilter::OTHER_PLAYER_THAN_CASTER && samePlayer) {
            return false;
        }
    }
    return true;
}

} // namespace

bool ABCondTargetAlive::Test(CSimulation&, CUnit*, CWidget* unitTarget,
                             bool, float, float) const {
    auto* unit = dynamic_cast<CUnit*>(unitTarget);
    return unit != nullptr && !unit->IsDead();
}

bool ABCondTargetUndead::Test(CSimulation&, CUnit*, CWidget* unitTarget,
                              bool, float, float) const {
    auto* unit = dynamic_cast<CUnit*>(unitTarget);
    if (!unit || unit->IsDead()) return false;
    auto* type = unit->GetUnitType();
    return type != nullptr &&
           type->GetClassifications().count(CUnitClassification::UNDEAD) > 0;
}

bool ABCondTargetAllyOfCaster::Test(CSimulation& game, CUnit* caster,
                                    CWidget* unitTarget,
                                    bool, float, float) const {
    auto* unit = dynamic_cast<CUnit*>(unitTarget);
    if (!caster || !unit) return false;
    auto* player = game.GetPlayer(caster->GetPlayerIndex());
    auto* targetPlayer = game.GetPlayer(unit->GetPlayerIndex());
    if (!player || !targetPlayer) return false;
    return player->HasAlliance(*targetPlayer, CAllianceType::PASSIVE);
}

ABCondAll::ABCondAll(std::vector<std::shared_ptr<ABCondition>> conds)
    : conds_(std::move(conds)) {}

bool ABCondAll::Test(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                     bool hasPoint, float x, float y) const {
    for (const auto& cond : conds_) {
        if (!cond || !cond->Test(game, caster, unitTarget, hasPoint, x, y)) {
            return false;
        }
    }
    return true;
}

ABActionDamageTarget::ABActionDamageTarget(float amount, CAttackType attackType,
                                           CDamageType damageType, bool ranged,
                                           bool isAttack)
    : amount_(amount), attackType_(attackType), damageType_(damageType),
      ranged_(ranged), isAttack_(isAttack) {}

void ABActionDamageTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                               bool, float, float) {
    if (!unitTarget || unitTarget->IsDead()) return;
    unitTarget->Damage(game, caster, isAttack_, ranged_, attackType_, damageType_, "",
                       amount_, 0);
}

ABActionDamageTargetMaxLifePct::ABActionDamageTargetMaxLifePct(
    float fraction, CAttackType attackType, CDamageType damageType)
    : fraction_(fraction), attackType_(attackType), damageType_(damageType) {}

void ABActionDamageTargetMaxLifePct::Run(CSimulation& game, CUnit* caster,
                                         CWidget* unitTarget,
                                         bool, float, float) {
    if (!unitTarget || unitTarget->IsDead()) return;
    unitTarget->Damage(game, caster, false, false, attackType_, damageType_, "",
                       unitTarget->GetMaxLife() * fraction_, 0);
}

ABActionHealTarget::ABActionHealTarget(float amount) : amount_(amount) {}

void ABActionHealTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                             bool, float, float) {
    (void)caster;
    auto* unit = dynamic_cast<CUnit*>(unitTarget);
    if (!unit || unit->IsDead()) return;
    float healed = unit->GetLife() + amount_;
    if (healed > unit->GetMaxLife()) healed = unit->GetMaxLife();
    unit->SetLife(game, healed);
}

ABActionAddBuffTarget::ABActionAddBuffTarget(BuffFactory factory)
    : factory_(std::move(factory)) {}

void ABActionAddBuffTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                bool, float, float) {
    (void)caster;
    auto* unit = dynamic_cast<CUnit*>(unitTarget);
    if (!unit || unit->IsDead() || !factory_) return;
    unit->AddBuff(factory_(), game);
}

ABActionStunTarget::ABActionStunTarget(float duration) : duration_(duration) {}

void ABActionStunTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                             bool, float, float) {
    (void)caster;
    auto* unit = dynamic_cast<CUnit*>(unitTarget);
    if (!unit || unit->IsDead()) return;
    unit->AddBuff(std::make_unique<CBuffStun>(duration_), game);
}

void ABActionKillTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                             bool, float, float) {
    (void)caster;
    auto* unit = dynamic_cast<CUnit*>(unitTarget);
    if (!unit || unit->IsDead()) return;
    unit->SetLife(game, 0.0f);
}

void ABActionResurrectTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                  bool, float, float) {
    (void)caster;
    auto* unit = dynamic_cast<CUnit*>(unitTarget);
    if (!unit || !unit->IsDead()) return;
    unit->SetLife(game, unit->GetMaxLife());
}

void ABActionReviveTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                               bool, float, float) {
    (void)caster;
    auto* unit = dynamic_cast<CUnit*>(unitTarget);
    if (!unit || !unit->IsDead()) return;
    unit->Revive(game, unit->GetX(), unit->GetY());
}

ABActionSetLifeTarget::ABActionSetLifeTarget(float life) : life_(life) {}

void ABActionSetLifeTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                bool, float, float) {
    (void)caster;
    auto* unit = dynamic_cast<CUnit*>(unitTarget);
    if (!unit) return;
    unit->SetLife(game, life_);
}

ABActionSetManaTarget::ABActionSetManaTarget(float mana) : mana_(mana) {}

void ABActionSetManaTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                bool, float, float) {
    (void)caster;
    auto* unit = dynamic_cast<CUnit*>(unitTarget);
    if (!unit) return;
    unit->SetMana(mana_);
}

ABActionCreateUnit::ABActionCreateUnit(War3ID typeId, int count, float spread)
    : typeId_(typeId), count_(count), spread_(spread) {}

void ABActionCreateUnit::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                             bool hasPoint, float x, float y) {
    if (!caster) return;
    float cx, cy;
    ResolveCenter(caster, unitTarget, hasPoint, x, y, cx, cy);
    const float facing = caster->GetFacing();
    const float rad = facing * 3.14159265f / 180.0f;
    for (int i = 0; i < count_; i++) {
        const float off = (static_cast<float>(i) - static_cast<float>(count_ - 1) * 0.5f) * spread_;
        game.CreateUnit(typeId_, caster->GetPlayerIndex(),
                        cx + std::cos(rad) * off, cy + std::sin(rad) * off, facing);
    }
}

ABActionTeleportTarget::ABActionTeleportTarget() = default;

ABActionTeleportTarget::ABActionTeleportTarget(float x, float y)
    : hasDest_(true), destX_(x), destY_(y) {}

void ABActionTeleportTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                 bool hasPoint, float x, float y) {
    (void)game;
    (void)caster;
    auto* unit = dynamic_cast<CUnit*>(unitTarget);
    // MassTeleport relocates onto the clicked point; without a point (and
    // no fixed destination) there is nothing to do (matches the .j guard
    // on target location).
    if (!unit || unit->IsDead()) return;
    if (hasDest_) {
        unit->SetPositionAndCheckUnstuck(destX_, destY_, game);
    } else if (hasPoint) {
        unit->SetPositionAndCheckUnstuck(x, y, game);
    }
}

ABActionForEachInRange::ABActionForEachInRange(float radius, ABEffectChain chain,
                                               bool includeCaster, int maxCount)
    : radius_(radius), chain_(std::move(chain)),
      includeCaster_(includeCaster), maxCount_(maxCount) {}

void ABActionForEachInRange::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                 bool hasPoint, float x, float y) {
    if (chain_.Empty()) return;
    float cx, cy;
    ResolveCenter(caster, unitTarget, hasPoint, x, y, cx, cy);
    int visited = 0;
    for (auto* unit : game.GetUnits()) {
        if (!ShouldVisitUnit(caster, unit, includeCaster_, exclude_, factionFilter_,
                             lifeFilter_)) {
            continue;
        }
        const float dx = unit->GetX() - cx;
        const float dy = unit->GetY() - cy;
        if (dx * dx + dy * dy > radius_ * radius_) continue;
        chain_.Run(game, caster, unit, true, unit->GetX(), unit->GetY());
        if (maxCount_ >= 0 && ++visited >= maxCount_) break;
    }
}

ABActionForEachInLine::ABActionForEachInLine(float range, float width,
                                             ABEffectChain chain,
                                             bool includeCaster, int maxCount)
    : range_(range), width_(width), chain_(std::move(chain)),
      includeCaster_(includeCaster), maxCount_(maxCount) {}

void ABActionForEachInLine::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                bool hasPoint, float x, float y) {
    if (chain_.Empty() || !caster) return;
    float tx, ty;
    if (unitTarget) {
        tx = unitTarget->GetX();
        ty = unitTarget->GetY();
    } else if (hasPoint) {
        tx = x;
        ty = y;
    } else {
        return;
    }
    const float dx = tx - caster->GetX();
    const float dy = ty - caster->GetY();
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1.0f) return;
    const float nx = dx / dist, ny = dy / dist;
    int visited = 0;
    for (auto* unit : game.GetUnits()) {
        if (!ShouldVisitUnit(caster, unit, includeCaster_, exclude_, factionFilter_)) {
            continue;
        }
        const float ux = unit->GetX() - caster->GetX();
        const float uy = unit->GetY() - caster->GetY();
        const float proj = ux * nx + uy * ny;
        if (proj < 0 || proj > range_) continue;
        const float perp = std::fabs(ux * ny - uy * nx);
        if (perp > width_ / 2) continue;
        chain_.Run(game, caster, unit, true, unit->GetX(), unit->GetY());
        if (maxCount_ >= 0 && ++visited >= maxCount_) break;
    }
}

ABActionBounce::ABActionBounce(int bounces, float range, ChainFactory factory,
                               bool includeCaster)
    : bounces_(bounces), range_(range), factory_(std::move(factory)),
      includeCaster_(includeCaster) {}

void ABActionBounce::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                         bool hasPoint, float x, float y) {
    if (!factory_ || !unitTarget || unitTarget->IsDead()) return;
    (void)hasPoint;
    (void)x;
    (void)y;
    CWidget* current = unitTarget;
    std::unordered_set<CWidget*> struck;
    for (int i = 0; i < bounces_ && current; i++) {
        ABEffectChain chain = factory_(i);
        chain.Run(game, caster, current, true, current->GetX(), current->GetY());
        struck.insert(current);
        CUnit* next = nullptr;
        float bestDist = range_;
        for (auto* other : game.GetUnits()) {
            if (other == current || struck.count(other) > 0) continue;
            if (!ShouldVisitUnit(caster, other, includeCaster_, nullptr, factionFilter_,
                                 lifeFilter_)) {
                continue;
            }
            const float dx = other->GetX() - current->GetX();
            const float dy = other->GetY() - current->GetY();
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < bestDist) {
                bestDist = dist;
                next = other;
            }
        }
        current = next;
    }
}

ABActionIf::ABActionIf(std::shared_ptr<ABCondition> cond, ABEffectChain thenChain,
                       ABEffectChain elseChain)
    : cond_(std::move(cond)), thenChain_(std::move(thenChain)),
      elseChain_(std::move(elseChain)) {}

ABActionCreateUnitAtTarget::ABActionCreateUnitAtTarget(War3ID typeId, int count,
                                                           float stepX)
    : typeId_(typeId), count_(count), stepX_(stepX) {}

void ABActionCreateUnitAtTarget::Run(CSimulation& game, CUnit* caster,
                                     CWidget* unitTarget,
                                     bool, float, float) {
    if (!caster || !unitTarget) return;
    for (int i = 0; i < count_; i++) {
        CUnit* created = game.CreateUnit(typeId_, caster->GetPlayerIndex(),
                                         unitTarget->GetX() + i * stepX_,
                                         unitTarget->GetY(), 0);
        if (created) {
            if (const auto& cb = game.GetUnitSummonedCallback()) {
                cb(caster->GetHandleId(), created->GetHandleId());
            }
        }
    }
}

ABActionCreateUnitAtPoint::ABActionCreateUnitAtPoint(War3ID typeId, float x, float y,
                                                     float facing)
    : typeId_(typeId), x_(x), y_(y), facing_(facing) {}

void ABActionCreateUnitAtPoint::Run(CSimulation& game, CUnit* caster,
                                    CWidget*, bool, float, float) {
    if (!caster) return;
    CUnit* created =
        game.CreateUnit(typeId_, caster->GetPlayerIndex(), x_, y_, facing_);
    if (created) {
        if (const auto& cb = game.GetUnitSummonedCallback()) {
            cb(caster->GetHandleId(), created->GetHandleId());
        }
    }
}

ABActionSummonRing::ABActionSummonRing(War3ID typeId, War3ID buffAlias, int count,
                                       float duration, float radius)
    : typeId_(typeId), buffAlias_(buffAlias), count_(count),
      duration_(duration), radius_(radius) {}

void ABActionSummonRing::Run(CSimulation& game, CUnit* caster, CWidget*,
                             bool hasPoint, float x, float y) {
    if (!caster || !hasPoint) return;
    for (int i = 0; i < count_; i++) {
        const float angle =
            (count_ > 1) ? (2.0f * 3.14159265f * (float)i / (float)count_) : 0.0f;
        SummonTimedUnit(game, caster, typeId_, buffAlias_,
                        x + radius_ * std::cos(angle), y + radius_ * std::sin(angle),
                        angle, duration_);
    }
}

ABActionManaSteal::ABActionManaSteal(float maxAmount) : maxAmount_(maxAmount) {}

void ABActionManaSteal::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                            bool, float, float) {
    (void)game;
    if (!unitTarget || !caster) return;
    auto* targetUnit = dynamic_cast<CUnit*>(unitTarget);
    if (!targetUnit || targetUnit->IsDead()) return;
    float stolen = maxAmount_;
    if (stolen > targetUnit->GetMana()) stolen = targetUnit->GetMana();
    targetUnit->SetMana(targetUnit->GetMana() - stolen);
    float mana = caster->GetMana() + stolen;
    if (mana > static_cast<float>(caster->GetMaxMana())) {
        mana = static_cast<float>(caster->GetMaxMana());
    }
    caster->SetMana(mana);
}

ABActionHealCaster::ABActionHealCaster(float amount) : amount_(amount) {}

void ABActionHealCaster::Run(CSimulation& game, CUnit* caster, CWidget*,
                             bool, float, float) {
    if (!caster || caster->IsDead()) return;
    float healed = caster->GetLife() + amount_;
    if (healed > caster->GetMaxLife()) healed = caster->GetMaxLife();
    caster->SetLife(game, healed);
}

ABActionAddGoldToCaster::ABActionAddGoldToCaster(int gold) : gold_(gold) {}

void ABActionAddGoldToCaster::Run(CSimulation& game, CUnit* caster, CWidget*,
                                  bool, float, float) {
    if (!caster) return;
    if (CPlayer* owner = game.GetPlayer(caster->GetPlayerIndex())) {
        owner->AddGold(gold_);
    }
}

void ABActionRemoveTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                               bool, float, float) {
    (void)caster;
    auto* unit = dynamic_cast<CUnit*>(unitTarget);
    if (!unit) return;
    game.RemoveUnit(unit);
}

ABActionCreateUnitWithBuff::ABActionCreateUnitWithBuff(War3ID typeId, float x, float y,
                                                       std::vector<BuffFactory> factories)
    : typeId_(typeId), x_(x), y_(y), factories_(std::move(factories)) {}

void ABActionCreateUnitWithBuff::Run(CSimulation& game, CUnit* caster, CWidget*,
                                     bool, float, float) {
    if (!caster) return;
    CUnit* created =
        game.CreateUnit(typeId_, caster->GetPlayerIndex(), x_, y_, 0);
    if (!created) return;
    for (auto& factory : factories_) {
        if (factory) created->AddBuff(factory(), game);
    }
    if (const auto& cb = game.GetUnitSummonedCallback()) {
        cb(caster->GetHandleId(), created->GetHandleId());
    }
}

void ABActionTransferToCaster::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                   bool, float, float) {
    auto* target = dynamic_cast<CUnit*>(unitTarget);
    if (!caster || !target) return;
    if (target->GetPlayerIndex() == caster->GetPlayerIndex()) return;
    if (CPlayer* oldOwner = game.GetPlayer(target->GetPlayerIndex())) {
        oldOwner->RemoveUnit(target);
    }
    target->SetPlayerIndex(caster->GetPlayerIndex());
    if (CPlayer* newOwner = game.GetPlayer(caster->GetPlayerIndex())) {
        newOwner->AddUnit(target);
    }
}

ABActionCreateUnitWithAbility::ABActionCreateUnitWithAbility(
    War3ID typeId, float x, float y, AbilityFactory factory)
    : typeId_(typeId), x_(x), y_(y), factory_(std::move(factory)) {}

void ABActionCreateUnitWithAbility::Run(CSimulation& game, CUnit* caster, CWidget*,
                                        bool, float, float) {
    if (!caster || !factory_) return;
    CUnit* created =
        game.CreateUnit(typeId_, caster->GetPlayerIndex(), x_, y_, 0);
    if (!created) return;
    if (auto ability = factory_(game, created)) {
        created->AddAbility(game, std::move(ability));
    }
    if (const auto& cb = game.GetUnitSummonedCallback()) {
        cb(caster->GetHandleId(), created->GetHandleId());
    }
}

ABActionSummonCopyOfTarget::ABActionSummonCopyOfTarget(War3ID buffAlias, float duration,
                                                       float offsetX, float offsetY)
    : buffAlias_(buffAlias), duration_(duration), offsetX_(offsetX), offsetY_(offsetY) {}

void ABActionSummonCopyOfTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                     bool, float, float) {
    auto* target = dynamic_cast<CUnit*>(unitTarget);
    if (!caster || !target || target->IsDead()) return;
    SummonTimedUnit(game, caster, target->GetTypeId(), buffAlias_,
                    target->GetX() + offsetX_, target->GetY() + offsetY_,
                    target->GetFacing(), duration_);
}

ABActionAnimateDeadCorpse::ABActionAnimateDeadCorpse(War3ID buffAlias, float duration)
    : buffAlias_(buffAlias), duration_(duration) {}

void ABActionAnimateDeadCorpse::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                    bool, float, float) {
    auto* target = dynamic_cast<CUnit*>(unitTarget);
    if (!caster || !target || !target->IsDead()) return;
    target->Revive(game, target->GetX(), target->GetY());
    target->SetInvulnerable(true);
    if (target->GetPlayerIndex() != caster->GetPlayerIndex()) {
        if (CPlayer* oldOwner = game.GetPlayer(target->GetPlayerIndex())) {
            oldOwner->RemoveUnit(target);
        }
        target->SetPlayerIndex(caster->GetPlayerIndex());
        if (CPlayer* newOwner = game.GetPlayer(caster->GetPlayerIndex())) {
            newOwner->AddUnit(target);
        }
    }
    target->AddBuff(std::make_unique<CBuffTimedLife>(buffAlias_, duration_), game);
    if (const auto& cb = game.GetUnitSummonedCallback()) {
        cb(caster->GetHandleId(), target->GetHandleId());
    }
}

void ABActionAncestralRevive::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                                  bool, float, float) {
    auto* target = dynamic_cast<CUnit*>(unitTarget);
    if (!caster || !target || !target->IsDead()) return;
    const War3ID typeId = target->GetTypeId();
    if (typeId != War3ID("otau") && typeId != War3ID("Otch")) return;
    target->Revive(game, target->GetX(), target->GetY());
    if (target->GetPlayerIndex() != caster->GetPlayerIndex()) {
        if (CPlayer* oldOwner = game.GetPlayer(target->GetPlayerIndex())) {
            oldOwner->RemoveUnit(target);
        }
        target->SetPlayerIndex(caster->GetPlayerIndex());
        if (CPlayer* newOwner = game.GetPlayer(caster->GetPlayerIndex())) {
            newOwner->AddUnit(target);
        }
    }
}

ABActionDevourMagic::ABActionDevourMagic(float manaPerBuff) : manaPerBuff_(manaPerBuff) {}

void ABActionDevourMagic::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                              bool, float, float) {
    auto* target = dynamic_cast<CUnit*>(unitTarget);
    if (!target || target->IsDead()) return;
    const size_t before = target->GetBuffs().size();
    target->RemoveBuffsIf(game, true, true, false, true);
    const size_t eaten = before - target->GetBuffs().size();
    if (eaten > 0 && caster && !caster->IsDead()) {
        float mana = caster->GetMana() + manaPerBuff_ * (float)eaten;
        if (mana > (float)caster->GetMaxMana()) mana = (float)caster->GetMaxMana();
        caster->SetMana(mana);
    }
}

ABActionManaBurn::ABActionManaBurn(int maxBurn, CAttackType attackType,
                                   CDamageType damageType)
    : maxBurn_(maxBurn), attackType_(attackType), damageType_(damageType) {}

void ABActionManaBurn::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                           bool, float, float) {
    auto* unit = dynamic_cast<CUnit*>(unitTarget);
    if (!unit || unit->IsDead()) return;
    const int burned = std::min(maxBurn_, static_cast<int>(unit->GetMana()));
    unit->SetMana(unit->GetMana() - burned);
    unit->Damage(game, caster, false, false, attackType_, damageType_, "",
                 static_cast<float>(burned), 0);
}

void ABActionIf::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                     bool hasPoint, float x, float y) {
    const bool take = cond_ && cond_->Test(game, caster, unitTarget, hasPoint, x, y);
    (take ? thenChain_ : elseChain_).Run(game, caster, unitTarget, hasPoint, x, y);
}

ABActionMissileStun::ABActionMissileStun(float damage, float stunDuration, float speed,
                                         CAttackType attackType, CDamageType damageType)
    : damage_(damage), stunDuration_(stunDuration), speed_(speed),
      attackType_(attackType), damageType_(damageType) {}

void ABActionMissileStun::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                              bool, float, float) {
    auto* target = dynamic_cast<CUnit*>(unitTarget);
    if (!caster || !target || target->IsDead()) return;
    // Stun lands with the cast (deterministic); the visible missile carries
    // the damage so travel time still applies to the life total.
    target->AddBuff(std::make_unique<CBuffStun>(stunDuration_), game);
    CProjectile missile;
    missile.attackerHandleId = caster->GetHandleId();
    missile.targetHandleId = target->GetHandleId();
    missile.x = caster->GetX();
    missile.y = caster->GetY();
    missile.speed = speed_;
    missile.damage = damage_;
    missile.attackType = attackType_;
    missile.maxTravel = 4096.0f;
    game.SpawnProjectile(std::move(missile));
}

ABActionDispelArea::ABActionDispelArea(float radius, bool removePositive,
                                       bool removeNegative)
    : radius_(radius), removePositive_(removePositive),
      removeNegative_(removeNegative) {}

void ABActionDispelArea::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                             bool hasPoint, float x, float y) {
    float cx, cy;
    ResolveCenter(caster, unitTarget, hasPoint, x, y, cx, cy);
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        const float dx = unit->GetX() - cx;
        const float dy = unit->GetY() - cy;
        if (dx * dx + dy * dy > radius_ * radius_) continue;
        // keepTimedLife=true: summons survive dispels (Abolish Magic rule).
        unit->RemoveBuffsIf(game, removePositive_, removeNegative_, false, true);
    }
}

ABActionDispelTarget::ABActionDispelTarget(bool removePositive, bool removeNegative)
    : removePositive_(removePositive), removeNegative_(removeNegative) {}

void ABActionDispelTarget::Run(CSimulation& game, CUnit* caster, CWidget* unitTarget,
                               bool, float, float) {
    (void)caster;
    auto* unit = dynamic_cast<CUnit*>(unitTarget);
    if (!unit || unit->IsDead()) return;
    unit->RemoveBuffsIf(game, removePositive_, removeNegative_, false, true);
}
