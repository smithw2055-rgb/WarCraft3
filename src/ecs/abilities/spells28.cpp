#include "spells28.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_harvest.h"
#include "ecs/buff.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include <algorithm>
#include <cmath>

namespace {

void MorphIfKnownImpl(CSimulation& game, CUnit* unit, War3ID formId) {
    if (!unit) return;
    CUnitData* data = game.GetUnitData();
    CUnitType* form = data ? data->GetUnitType(formId) : nullptr;
    if (form) unit->MorphTo(form, formId);
}

CAbilityCargoHold* FindCargo(CUnit* unit) {
    if (!unit) return nullptr;
    if (auto* burrow = dynamic_cast<CAbilityCargoHold*>(unit->GetAbility(War3ID("Abun")))) {
        return burrow;
    }
    return dynamic_cast<CAbilityCargoHold*>(unit->GetAbility(War3ID("Acar")));
}

void SetHarvestDisabled(CUnit* unit, bool disabled) {
    if (!unit) return;
    if (auto* harvest = unit->GetAbility(War3ID("Ahar"))) {
        harvest->SetDisabled(disabled, CAbilityDisableType::TRANSFORMATION);
    }
}

void SendBackToWork(CSimulation& game, CUnit* unit) {
    auto* harvest = dynamic_cast<CAbilityHarvest*>(unit ? unit->GetAbility(War3ID("Ahar")) : nullptr);
    if (!harvest || harvest->GetLastTargetHandle() < 0) return;
    CWidget* target = game.GetWidget(harvest->GetLastTargetHandle());
    if (!target || target->IsDead()) return;
    unit->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::HARVEST, target));
}

void ReturnCarriedResources(CSimulation& game, CUnit* unit) {
    auto* harvest = dynamic_cast<CAbilityHarvest*>(unit ? unit->GetAbility(War3ID("Ahar")) : nullptr);
    if (!harvest || harvest->GetCarriedAmount() <= 0) return;
    if (CPlayer* player = game.GetPlayer(unit->GetPlayerIndex())) {
        if (harvest->GetCarriedResourceType() == CResourceType::GOLD) {
            player->AddGold(harvest->GetCarriedAmount());
        } else if (harvest->GetCarriedResourceType() == CResourceType::LUMBER) {
            player->AddLumber(harvest->GetCarriedAmount());
        }
    }
    harvest->ClearCarried();
}

class CBuffMilitia : public CBuff {
public:
    CBuffMilitia(float duration, War3ID formId)
        : CBuff(War3ID("Bmil"), duration, true), formId_(formId) {}

    void OnApply(CSimulation& game, CUnit* target) override {
        if (!target) return;
        SetHarvestDisabled(target, true);
        CUnitData* data = game.GetUnitData();
        if (!data) return;
        if (CUnitType* form = data->GetUnitType(formId_)) {
            target->MorphTo(form, formId_);
        }
    }

    void OnRemove(CSimulation& game, CUnit* target) override {
        if (!target) return;
        target->Unmorph();
        SetHarvestDisabled(target, false);
        if (auto* militia = dynamic_cast<CAbilityPairing*>(target->GetAbility(War3ID("Amil")))) {
            militia->SetConverted(false);
        }
        SendBackToWork(game, target);
    }

    CBuff* Clone() const override {
        auto* copy = new CBuffMilitia(GetDuration(), formId_);
        copy->SetRemaining(GetRemaining());
        return copy;
    }

private:
    War3ID formId_;
};

} // namespace

CAbilityPairing::CAbilityPairing(int handleId, War3ID code, CPairMode mode)
    : CAbilitySpellBase(handleId, code, CAbilityCategory::SPELL), mode_(mode) {
    SetManaCost(0);
    SetCooldown(0.0f);
    partnerCode_ = code;
    resultId_ = code;
    switch (mode) {
    case CPairMode::MilitiaCall:
        partnerCode_ = War3ID("Amil");
        radius_ = 512.0f;
        SetTargetType(CSpellTargetType::NO_TARGET);
        break;
    case CPairMode::Militia:
        partnerCode_ = War3ID("Amic");
        resultId_ = War3ID("hmil");
        radius_ = 512.0f;
        SetTargetType(CSpellTargetType::NO_TARGET);
        break;
    case CPairMode::Mount:
        partnerCode_ = War3ID("Acoh");
        resultId_ = War3ID("ehpr");
        radius_ = 200.0f;
        SetTargetType(CSpellTargetType::NO_TARGET);
        break;
    case CPairMode::LoadPartner:
        partnerCode_ = War3ID("Acoa");
        resultId_ = War3ID("ehpr");
        radius_ = 200.0f;
        SetTargetType(CSpellTargetType::TARGET_UNIT);
        SetCastRange(200.0f);
        break;
    case CPairMode::Couple:
        partnerCode_ = War3ID("Acoi");
        resultId_ = War3ID("ecop");
        radius_ = 200.0f;
        SetTargetType(CSpellTargetType::NO_TARGET);
        break;
    }
}

void CAbilityPairing::MorphIfKnown(CSimulation& game, CUnit* unit, War3ID formId) {
    MorphIfKnownImpl(game, unit, formId);
}

CUnit* CAbilityPairing::FindPartner(CSimulation& game, CUnit* self) const {
    if (!self) return nullptr;
    CUnit* best = nullptr;
    float bestDist = radius_ * radius_;
    for (CUnit* other : game.GetUnits()) {
        if (!other || other == self || other->IsDead() || other->IsLoadedAboard()) continue;
        if (other->GetPlayerIndex() != self->GetPlayerIndex()) continue;
        if (!other->GetAbility(partnerCode_)) continue;
        const float dx = other->GetX() - self->GetX();
        const float dy = other->GetY() - self->GetY();
        const float dist = dx * dx + dy * dy;
        if (dist > bestDist) continue;
        bestDist = dist;
        best = other;
    }
    return best;
}

bool CAbilityPairing::Convert(CSimulation& game, CUnit* self) {
    if (!self || converted_) return converted_;
    if (mode_ == CPairMode::Militia) {
        if (!FindPartner(game, self)) return false;
        ReturnCarriedResources(game, self);
        SetHarvestDisabled(self, true);
        if (militiaDuration_ > 0.0f) {
            self->AddBuff(std::make_unique<CBuffMilitia>(militiaDuration_, resultId_), game);
        } else {
            MorphIfKnown(game, self, resultId_);
        }
        converted_ = true;
        return true;
    }
    return false;
}

void CAbilityPairing::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    if (mode_ == CPairMode::Militia && converted_) {
        if (caster->GetBuff(War3ID("Bmil"))) {
            caster->RemoveBuff(War3ID("Bmil"), game);
        } else {
            caster->Unmorph();
            SetHarvestDisabled(caster, false);
            converted_ = false;
            SendBackToWork(game, caster);
        }
        return;
    }
    if (mode_ == CPairMode::MilitiaCall) {
        for (CUnit* other : game.GetUnits()) {
            if (!other || other->IsDead()) continue;
            if (other->GetPlayerIndex() != caster->GetPlayerIndex()) continue;
            const float dx = other->GetX() - caster->GetX();
            const float dy = other->GetY() - caster->GetY();
            if (dx * dx + dy * dy > radius_ * radius_) continue;
            if (auto* militia = dynamic_cast<CAbilityPairing*>(other->GetAbility(War3ID("Amil")))) {
                militia->Convert(game, other);
            }
        }
        return;
    }
    if (mode_ == CPairMode::Militia) {
        Convert(game, caster);
        return;
    }
    CUnit* partner = FindPartner(game, caster);
    if (!partner) return;
    if (mode_ == CPairMode::Mount || mode_ == CPairMode::LoadPartner) {
        CUnit* mount = (mode_ == CPairMode::Mount) ? partner : caster;
        CUnit* rider = (mode_ == CPairMode::Mount) ? caster : partner;
        if (mount->LoadUnit(rider)) {
            MorphIfKnown(game, mount, resultId_);
            converted_ = true;
        }
        return;
    }
    if (mode_ == CPairMode::Couple) {
        MorphIfKnown(game, caster, resultId_);
        partner->SetLife(game, 0);
        converted_ = true;
    }
}

void CAbilityPairing::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* unit = dynamic_cast<CUnit*>(target);
    if (mode_ == CPairMode::LoadPartner && caster && unit && unit->GetAbility(partnerCode_)) {
        if (caster->LoadUnit(unit)) {
            MorphIfKnown(game, caster, resultId_);
            converted_ = true;
        }
        return;
    }
    DoEffect(game, caster);
}

void CAbilityPairing::ApplyEffectData(const CAbilityDefFull& def, int level) {
    (void)def;
    (void)level;
}

CAbilityBlight::CAbilityBlight(int handleId)
    : CAbility(handleId, War3ID("Abli"), CAbilityCategory::BLIGHT) {
    iconShowing_ = false;
    permanent_ = true;
}

void CAbilityBlight::OnTick(CSimulation& game, CUnit* unit) {
    if (!unit || !creates_) return;
    accum_ += game.GetSimStepTime();
    if (accum_ < interval_) return;
    accum_ = 0.0f;
    if (current_ >= area_) return;
    current_ = std::min(area_, current_ + expansion_);
    game.SetBlightCircle(unit->GetX(), unit->GetY(), current_, true);
}

CAbilityBlightedGoldMine::CAbilityBlightedGoldMine(int handleId)
    : CAbility(handleId, War3ID("Abgm"), CAbilityCategory::GOLD_MINE) {
    iconShowing_ = false;
    permanent_ = true;
}

bool CAbilityBlightedGoldMine::AddMiner() {
    if (miners_ >= maxMiners_) return false;
    miners_++;
    return true;
}

void CAbilityBlightedGoldMine::RemoveMiner() {
    if (miners_ > 0) miners_--;
}

void CAbilityBlightedGoldMine::OnTick(CSimulation& game, CUnit* unit) {
    if (!unit || miners_ <= 0 || gold_ <= 0) return;
    accum_ += game.GetSimStepTime();
    const float interval = interval_ * ((float)maxMiners_ / (float)miners_);
    if (accum_ < interval) return;
    accum_ -= interval;
    const int gain = std::min(gold_, goldPerInterval_);
    gold_ -= gain;
    if (CPlayer* player = game.GetPlayer(unit->GetPlayerIndex())) {
        player->AddGold(gain);
    }
}

CAbilityAcolyteHarvest::CAbilityAcolyteHarvest(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aaha"), CAbilityCategory::HARVEST) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(100.0f);
    SetManaCost(0);
}

bool CAbilityAcolyteHarvest::CheckCanTarget(CSimulation& game, CUnit* unit, int orderId,
                                           CWidget* target) const {
    if (!CAbilitySpellBase::CheckCanTarget(game, unit, orderId, target)) return false;
    auto* mineUnit = dynamic_cast<CUnit*>(target);
    if (!mineUnit || !unit) return false;
    if (mineUnit->GetPlayerIndex() != unit->GetPlayerIndex()) return false;
    return mineUnit->GetAbility(War3ID("Abgm")) != nullptr;
}

void CAbilityAcolyteHarvest::Unbind(CSimulation& game) {
    if (mineUnitHandle_ < 0) return;
    if (CUnit* mineUnit = game.GetUnit(mineUnitHandle_)) {
        if (auto* mine = dynamic_cast<CAbilityBlightedGoldMine*>(
                mineUnit->GetAbility(War3ID("Abgm")))) {
            mine->RemoveMiner();
        }
    }
    mineUnitHandle_ = -1;
}

void CAbilityAcolyteHarvest::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* mineUnit = dynamic_cast<CUnit*>(target);
    if (!caster || !mineUnit) return;
    auto* mine = dynamic_cast<CAbilityBlightedGoldMine*>(mineUnit->GetAbility(War3ID("Abgm")));
    if (!mine) return;
    if (mineUnitHandle_ == mineUnit->GetHandleId()) return; // already here
    Unbind(game);
    if (!mine->AddMiner()) return; // full: DENIED, stays unbound (ref error text)
    mineUnitHandle_ = mineUnit->GetHandleId();
}

void CAbilityAcolyteHarvest::OnDeath(CSimulation& game, CUnit* unit) {
    (void)unit;
    Unbind(game);
}

void CAbilityAcolyteHarvest::OnRemove(CSimulation& game, CUnit* unit) {
    (void)unit;
    Unbind(game);
}

CAbilityWispHarvest::CAbilityWispHarvest(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Awha"), CAbilityCategory::HARVEST) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(100.0f);
    SetManaCost(0);
}

void CAbilityWispHarvest::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)game;
    auto* tree = dynamic_cast<CDestructable*>(target);
    if (!caster || !tree || tree->IsDead()) return;
    treeHandle_ = tree->GetHandleId();
    accum_ = interval_;
    // ref CBehaviorWispHarvest: instant jump onto the tree (no walkaround,
    // no carry/return trips); lumber then trickles in OnTick.
    caster->SetPosition(tree->GetX(), tree->GetY());
}

void CAbilityWispHarvest::OnTick(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    CDestructable* tree =
        treeHandle_ >= 0 ? game.GetDestructable(treeHandle_) : nullptr;
    if (!tree || tree->IsDead()) {
        treeHandle_ = -1;
        return;
    }
    accum_ += game.GetSimStepTime();
    if (accum_ < interval_) return;
    accum_ -= interval_;
    tree->Damage(game, unit, false, false, CAttackType::NORMAL, CDamageType::NORMAL,
                  "", damagePerInterval_, 0);
    if (CPlayer* player = game.GetPlayer(unit->GetPlayerIndex())) {
        player->AddLumber(lumberPerInterval_);
    }
    if (tree->IsDead()) treeHandle_ = -1;
}

CAbilityHarvestLumber::CAbilityHarvestLumber(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Ahrl"), CAbilityCategory::HARVEST) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(128.0f);
    SetManaCost(0);
}

void CAbilityHarvestLumber::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* tree = dynamic_cast<CDestructable*>(target);
    if (!caster || !tree || tree->IsDead()) return;
    tree->Damage(game, caster, false, false, CAttackType::NORMAL, CDamageType::NORMAL,
                 "", damageToTree_, 0);
    if (auto* harvest = dynamic_cast<CAbilityHarvest*>(caster->GetAbility(War3ID("Ahar")))) {
        harvest->SetCarried(CResourceType::LUMBER, capacity_);
        return;
    }
    if (CPlayer* player = game.GetPlayer(caster->GetPlayerIndex())) {
        player->AddLumber(capacity_);
    }
}

CAbilityCargoHold::CAbilityCargoHold(int handleId, War3ID code, bool burrow, int capacity)
    : CAbility(handleId, code, CAbilityCategory::CARGO_HOLD), burrow_(burrow), capacity_(capacity) {
    iconShowing_ = false;
    permanent_ = true;
}

void CAbilityCargoHold::OnAdd(CSimulation&, CUnit* unit) {
    if (burrow_ && unit) unit->SetAttacksDisabled(true);
}

void CAbilityCargoHold::OnRemove(CSimulation&, CUnit* unit) {
    UnloadAll(unit);
    if (burrow_ && unit) unit->SetAttacksDisabled(false);
}

bool CAbilityCargoHold::TryLoad(CUnit* holder, CUnit* passenger) {
    if (!holder || !passenger) return false;
    if ((int)holder->GetCargoUnits().size() >= capacity_) return false;
    if (!holder->LoadUnit(passenger)) return false;
    if (burrow_) {
        holder->SetAttacksDisabled(false);
        holder->MultiplyAttackSpeed(2.0f);
        speedSteps_++;
    }
    return true;
}

void CAbilityCargoHold::UnloadAll(CUnit* holder) {
    if (!holder) return;
    std::vector<CUnit*> cargo = holder->GetCargoUnits();
    for (CUnit* passenger : cargo) {
        holder->UnloadUnitAt(passenger, holder->GetX(), holder->GetY());
        if (burrow_ && speedSteps_ > 0) {
            holder->DivideAttackSpeed(2.0f);
            speedSteps_--;
        }
    }
    if (burrow_ && holder->GetCargoUnits().empty()) {
        holder->SetAttacksDisabled(true);
    }
}

CAbilityDropInstant::CAbilityDropInstant(int handleId, War3ID code)
    : CAbilitySpellBase(handleId, code, CAbilityCategory::CARGO_HOLD) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetManaCost(0);
}

void CAbilityDropInstant::DoEffect(CSimulation&, CUnit* caster) {
    if (!caster) return;
    if (CAbilityCargoHold* cargo = FindCargo(caster)) {
        cargo->UnloadAll(caster);
        return;
    }
    std::vector<CUnit*> aboard = caster->GetCargoUnits();
    for (CUnit* passenger : aboard) {
        caster->UnloadUnitAt(passenger, caster->GetX(), caster->GetY());
    }
}

CAbilityWayGate::CAbilityWayGate(int handleId)
    : CAbility(handleId, War3ID("Awrp"), CAbilityCategory::MOVEMENT) {
    iconShowing_ = false;
    permanent_ = true;
}

void CAbilityWayGate::SetDestination(CUnit* gate, float x, float y) {
    destX_ = x;
    destY_ = y;
    enabled_ = true;
    if (!gate) return;
    gate->SetWaygateDestination(x, y);
    gate->SetWaygateActive(true);
}

bool CAbilityWayGate::Inside(const CUnit* gate, const CUnit* target) const {
    if (!gate || !target) return false;
    return std::fabs(target->GetX() - gate->GetX()) <= width_ * 0.5f
        && std::fabs(target->GetY() - gate->GetY()) <= height_ * 0.5f;
}

bool CAbilityWayGate::TryTeleport(CUnit* gate, CUnit* target) const {
    if (!enabled_ || !gate || !target || target == gate) return false;
    if (target->IsDead() || target->IsBuilding() || target->IsPaused()) return false;
    if (target->IsLoadedAboard()) return false;
    if (!Inside(gate, target)) return false;
    target->SetPosition(destX_, destY_);
    return true;
}

bool CAbilityWayGate::CheckCanTarget(CSimulation&, CUnit* unit, int orderId, CWidget* target) const {
    if (!enabled_ || orderId != OrderIds::SMART) return false;
    auto* unitTarget = dynamic_cast<CUnit*>(target);
    if (!unitTarget || unitTarget->IsDead() || unitTarget->IsBuilding()) return false;
    return Inside(unit, unitTarget);
}

bool CAbilityWayGate::CheckBeforeQueue(CSimulation&, CUnit* caster, int orderId, CWidget* target) {
    if (orderId != OrderIds::SMART) return true;
    TryTeleport(caster, dynamic_cast<CUnit*>(target));
    return false;
}

CBehavior* CAbilityWayGate::Begin(CSimulation&, CUnit* caster, int, CWidget* target) {
    TryTeleport(caster, dynamic_cast<CUnit*>(target));
    return nullptr;
}

CAbilityShopPurchase::CAbilityShopPurchase(int handleId)
    : CAbility(handleId, War3ID("Apit"), CAbilityCategory::SELL_ITEMS) {
    iconShowing_ = false;
    permanent_ = true;
}

CAbilityNeutralBuilding::CAbilityNeutralBuilding(int handleId, War3ID code, bool onlyAllies)
    : CAbility(handleId, code, CAbilityCategory::NEUTRAL_BUILDING), onlyAllies_(onlyAllies) {
    iconShowing_ = false;
    permanent_ = true;
}

void CAbilityNeutralBuilding::OnTick(CSimulation& game, CUnit* unit) {
    accum_ += game.GetSimStepTime();
    if (primed_ && accum_ < interval_) return;
    primed_ = true;
    accum_ = 0.0f;
    Rescan(game, unit);
}

bool CAbilityNeutralBuilding::CanSelect(CUnit* shop, CUnit* other) const {
    if (!shop || !other || other == shop || other->IsDead() || other->IsBuilding()) return false;
    if (other->IsLoadedAboard()) return false;
    if (onlyAllies_ && other->GetPlayerIndex() != shop->GetPlayerIndex()) return false;
    const float dx = other->GetX() - shop->GetX();
    const float dy = other->GetY() - shop->GetY();
    return dx * dx + dy * dy <= radius_ * radius_;
}

void CAbilityNeutralBuilding::Rescan(CSimulation& game, CUnit* shop) {
    if (!shop) return;
    bool search = false;
    for (int i = 0; i < kPlayerSlots; i++) {
        if (!CanSelect(shop, selected_[i])) {
            selected_[i] = nullptr;
            search = true;
        }
    }
    if (!search) return;
    for (CUnit* other : game.GetUnits()) {
        if (!other) continue;
        const int player = other->GetPlayerIndex();
        if (player < 0 || player >= kPlayerSlots) continue;
        if (selected_[player] != nullptr) continue;
        if (CanSelect(shop, other)) selected_[player] = other;
    }
}

CUnit* CAbilityNeutralBuilding::GetActiveUnit() const {
    for (CUnit* unit : selected_) {
        if (unit) return unit;
    }
    return nullptr;
}

CUnit* CAbilityNeutralBuilding::GetSelectedUnit(int playerIndex) const {
    if (playerIndex < 0 || playerIndex >= kPlayerSlots) return nullptr;
    return selected_[playerIndex];
}

bool CAbilityNeutralBuilding::CheckCanTarget(CSimulation&, CUnit* unit, int orderId,
                                             CWidget* target) const {
    if (orderId != OrderIds::NEUTRALINTERACT) return false;
    return CanSelect(unit, dynamic_cast<CUnit*>(target));
}

CBehavior* CAbilityNeutralBuilding::Begin(CSimulation&, CUnit* caster, int,
                                          CWidget* target) {
    auto* unitTarget = dynamic_cast<CUnit*>(target);
    if (unitTarget && CanSelect(caster, unitTarget)) {
        const int player = unitTarget->GetPlayerIndex();
        if (player >= 0 && player < kPlayerSlots) selected_[player] = unitTarget;
    }
    return nullptr;
}
