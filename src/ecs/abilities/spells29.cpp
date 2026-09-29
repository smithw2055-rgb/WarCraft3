#include "spells29.h"
#include "ecs/abilities/ability_gold_mine.h"
#include "ecs/abilities/spells28.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"

namespace {

constexpr int kEntangleInstant = 852148;

void SetMineWorkDisabled(CUnit* unit, bool disabled) {
    if (!unit) return;
    const War3ID codes[] = {War3ID("Aegm"), War3ID("Aenc"), War3ID("Agl2")};
    for (War3ID code : codes) {
        if (CAbility* ability = unit->GetAbility(code)) {
            ability->SetDisabled(disabled, CAbilityDisableType::CONSTRUCTION);
        }
    }
}

} // namespace

CAbilityDrunkenBrawler::CAbilityDrunkenBrawler(int handleId)
    : CAbilityCriticalStrike(handleId) {
    SetAlias(War3ID("ANdb"));
}

void CAbilityDrunkenBrawler::OnAdd(CSimulation& game, CUnit* unit) {
    CAbilityCriticalStrike::OnAdd(game, unit);
    if (unit) unit->SetEvasionChance(dodge_);
}

void CAbilityDrunkenBrawler::OnRemove(CSimulation& game, CUnit* unit) {
    CAbilityCriticalStrike::OnRemove(game, unit);
    if (unit) unit->SetEvasionChance(0.0f);
}

CAbilityEntangleGoldMine::CAbilityEntangleGoldMine(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aent"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(200.0f);
    SetManaCost(0);
}

bool CAbilityEntangleGoldMine::CheckCanTarget(CSimulation& game, CUnit* unit, int orderId,
                                             CWidget* target) const {
    if (replacement_ && !replacement_->IsDead()) return false;
    if (!CAbilitySpellBase::CheckCanTarget(game, unit, orderId, target)) return false;
    auto* mine = dynamic_cast<CUnit*>(target);
    if (!mine) return false;
    return mine->GetAbility(War3ID("Agld")) != nullptr;
}

void CAbilityEntangleGoldMine::Release(CSimulation& game, bool killReplacement) {
    if (replacement_ && killReplacement && !replacement_->IsDead()) {
        replacement_->SetLife(game, 0);
    }
    if (original_) {
        original_->SetHidden(false);
        original_->SetPaused(false);
    }
    replacement_ = nullptr;
    original_ = nullptr;
    constructing_ = false;
}

CBehavior* CAbilityEntangleGoldMine::Begin(CSimulation& game, CUnit* caster, int orderId,
                                           CWidget* target) {
    instant_ = (orderId == kEntangleInstant);
    return CAbilitySpellBase::Begin(game, caster, orderId, target);
}

void CAbilityEntangleGoldMine::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* mineUnit = dynamic_cast<CUnit*>(target);
    if (!caster || !mineUnit || mineUnit->IsDead()) return;
    auto* gold = dynamic_cast<CAbilityGoldMine*>(mineUnit->GetAbility(War3ID("Agld")));
    if (!gold) return;
    if (replacement_ && !replacement_->IsDead()) return;

    CUnit* created = game.CreateUnit(resultId_, caster->GetPlayerIndex(),
                                     mineUnit->GetX(), mineUnit->GetY(), 0);
    if (!created) return;

    mineUnit->SetHidden(true);
    mineUnit->SetPaused(true);
    original_ = mineUnit;
    replacement_ = created;

    auto* overlay = dynamic_cast<CAbilityOverlayedGoldMine*>(created->GetAbility(War3ID("Agl2")));
    if (!overlay) {
        auto made = std::make_unique<CAbilityOverlayedGoldMine>(game.AllocateHandleId());
        overlay = made.get();
        created->AddAbility(game, std::move(made));
    }
    overlay->SetParentMine(mineUnit, gold);

    auto mine = std::make_unique<CAbilityEntangledMine>(game.AllocateHandleId());
    mine->SetParent(mineUnit);
    created->AddAbility(game, std::move(mine));
    if (!created->GetAbility(War3ID("Aenc"))) {
        created->AddAbility(game, std::make_unique<CAbilityCargoHold>(
            game.AllocateHandleId(), War3ID("Aenc"), false, 5));
    }

    if (!instant_) {
        created->SetConstructing(true);
        created->SetConstructionProgress(0);
        created->SetLife(game, created->GetMaxLife() * 0.1f);
        SetMineWorkDisabled(created, true);
        constructing_ = true;
    }
}

void CAbilityEntangleGoldMine::AdvanceConstruction(CSimulation& game) {
    if (!constructing_ || !replacement_ || replacement_->IsDead()) return;
    float buildTime = 30.0f;
    if (const CUnitType* type = replacement_->GetUnitType()) {
        if (type->GetBuildTime() > 0) buildTime = static_cast<float>(type->GetBuildTime());
    }
    float progress = replacement_->GetConstructionProgress() + game.GetSimStepTime();
    if (progress > buildTime) progress = buildTime;
    replacement_->SetConstructionProgress(progress);
    const float fraction = 0.1f + 0.9f * (progress / buildTime);
    replacement_->SetLife(game, replacement_->GetMaxLife() * fraction);
    if (progress >= buildTime) {
        replacement_->SetConstructing(false);
        replacement_->SetLife(game, replacement_->GetMaxLife());
        SetMineWorkDisabled(replacement_, false);
        constructing_ = false;
    }
}

void CAbilityEntangleGoldMine::OnTick(CSimulation& game, CUnit* unit) {
    (void)unit;
    if (replacement_ && replacement_->IsDead()) {
        Release(game, false);
        return;
    }
    AdvanceConstruction(game);
}

void CAbilityEntangleGoldMine::OnRemove(CSimulation& game, CUnit* unit) {
    (void)unit;
    Release(game, true);
}

CAbilityEntangledMine::CAbilityEntangledMine(int handleId)
    : CAbility(handleId, War3ID("Aegm"), CAbilityCategory::GOLD_MINE) {
    iconShowing_ = false;
    permanent_ = true;
}

void CAbilityEntangledMine::OnTick(CSimulation& game, CUnit* unit) {
    if (!unit || unit->IsDead() || !parent_) return;
    accum_ += game.GetSimStepTime();
    if (accum_ < interval_) return;
    accum_ -= interval_;

    if (auto* cargo = dynamic_cast<CAbilityCargoHold*>(unit->GetAbility(War3ID("Aenc")))) {
        const int cap = cargo->GetCapacity();
        if (cap <= 0) return;
        index_ = (index_ + 1) % cap;
        if (index_ >= (int)unit->GetCargoUnits().size()) return;
    }

    auto* gold = dynamic_cast<CAbilityGoldMine*>(parent_->GetAbility(War3ID("Agld")));
    if (!gold || gold->GetGold() <= 0) {
        unit->SetLife(game, 0);
        return;
    }
    const int gain = gold->GetGold() < goldPerInterval_ ? gold->GetGold() : goldPerInterval_;
    gold->SetGold(gold->GetGold() - gain);
    if (CPlayer* player = game.GetPlayer(unit->GetPlayerIndex())) {
        player->AddGold(gain);
    }
    if (gold->GetGold() <= 0) unit->SetLife(game, 0);
}

CAbilityChannelTest::CAbilityChannelTest(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANcl"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetManaCost(0);
}

void CAbilityChannelTest::DoEffect(CSimulation&, CUnit* caster) {
    (void)caster;
    channeling_ = true;
    remaining_ = artDuration_;
}

void CAbilityChannelTest::OnTick(CSimulation& game, CUnit* unit) {
    (void)unit;
    if (!channeling_) return;
    remaining_ -= game.GetSimStepTime();
    if (remaining_ <= 0.0f) {
        remaining_ = 0.0f;
        channeling_ = false;
    }
}
