#include "ecs/unit_text_utils.h"
#include "ecs/pathing/pathfinding.h"
#include "csimulation.h"
#include "ecs/buffs.h"
#include "data/unitdata.h"
#include "data/itemdata.h"
#include "behaviors/behavior_move.h"
#include "behaviors/behavior_attack.h"
#include "behaviors/behavior_build.h"
#include "behaviors/behavior_stop.h"
#include "behaviors/behavior_hold_position.h"
#include "behaviors/behavior_follow.h"
#include "behaviors/behavior_patrol.h"
#include "behaviors/behavior_attack_move.h"
#include "ecs/orders/order.h"
#include "debug/move_debug.h"
#include "pathing/world_collision.h"
#include <cmath>
#include <algorithm>
#include <sstream>

namespace {

constexpr float kPi = 3.14159265358979323846f;

static float NormalizeFacingDegrees(float facing) {
    while (facing < 0.0f) {
        facing += 360.0f;
    }
    while (facing >= 360.0f) {
        facing -= 360.0f;
    }
    return facing;
}

static float NormalizeFacingDelta(float delta) {
    while (delta < -180.0f) {
        delta += 360.0f;
    }
    while (delta > 180.0f) {
        delta -= 360.0f;
    }
    return delta;
}

using unit_text_utils::AbilityCategoryName;
using unit_text_utils::BehaviorCategoryName;

}

CUnit::CUnit(int handleId, CUnitType* unitType, int playerIndex,
             War3ID typeId, float x, float y, float facing)
    : CWidget(handleId, x, y, (float)unitType->GetMaxLife())
    , unitType_(unitType), typeId_(typeId), playerIndex_(playerIndex)
    , facing_(facing)
    , maxMana_(unitType->GetManaMaximum())
    , turnRate_(unitType->GetTurnRate())
    , movementType_(unitType->GetMovementType())
    , collisionSize_(unitType->GetCollisionSize())
    , stateFlags_(kDefaultUnitStateFlags)
{
    SetStateFlag(UnitStateFlag::Building, unitType->IsBuilding());
    SetStateFlag(UnitStateFlag::Hero, unitType->IsHero());
    attrs_.InitFromUnitType(unitType);
    maxLife_ = (float)unitType->GetMaxLife();
    mana_ = (float)unitType->GetManaInitial();
    flyHeight_ = unitType->GetDefaultFlyingHeight();
    foodUsed_ = unitType->GetFoodUsed();
    foodMade_ = unitType->GetFoodMade();
    dataDrivenBuffs_.reserve(16);
}

CUnit::~CUnit() {
    if (!registry_) {
        DetachedRegistry().DestroyEntity(handleId_);
    }
}

SimEcsRegistry& CUnit::DetachedRegistry() {
    static SimEcsRegistry s_detached;
    return s_detached;
}

void CUnit::BindRegistry(SimEcsRegistry* registry) {
    if (registry_ == registry) return;
    SimEcsRegistry& oldReg = ActiveRegistry();
    SimEcsRegistry& newReg = registry ? *registry : DetachedRegistry();
    if (&oldReg != &newReg) {
        auto migrate = [&](auto tag) {
            using CompT = decltype(tag);
            if (auto* existing = oldReg.TryGet<CompT>(handleId_)) {
                newReg.Emplace<CompT>(handleId_, std::move(*existing));
                oldReg.Remove<CompT>(handleId_);
            }
        };
        migrate(UnitConstructionComponent{});
        migrate(UnitCargoComponent{});
        migrate(UnitMorphComponent{});
        migrate(UnitStructureNavComponent{});
        migrate(UnitVisualComponent{});
        migrate(UnitCorpseDropComponent{});
        migrate(UnitScriptOverridesComponent{});
    }
    registry_ = registry;
}

float CUnit::GetSpeed() const {
    return attrs_.ComputeEffectiveSpeed();
}

void CUnit::SetSpeed(int s) {
    attrs_.SetBaseSpeed(s);
}

void CUnit::SetPosition(float x, float y) {
    x_ = x;
    y_ = y;
}

void CUnit::RegeneratePathingInstance(CSimulation& game) {
    KillPathingInstance(game);
    if (!unitType_) return;
    auto img = unitType_->GetPathTexImage();
    if (!img || !img->valid) return;
    // Retail snap (ref CUnit.regeneratePathingInstance): floor to the
    // 64-unit build grid, then nudge odd half-footprints by one cell so
    // the blit rect stays centered on the building.
    float ux = std::floor(GetX() / 64.0f) * 64.0f;
    float uy = std::floor(GetY() / 64.0f) * 64.0f;
    if (((img->w / 2) % 2) == 1) ux += 32.0f;
    if (((img->h / 2) % 2) == 1) uy += 32.0f;
    SetPosition(ux, uy);
    auto* pf = game.GetPathfinding();
    if (!pf || !pf->HasPathingMap()) return;
    StructureNavComp().pathingStampId = pf->GetPathingMap()->StampFootprint(ux, uy, 270, img, false);
}

void CUnit::KillPathingInstance(CSimulation& game) {
    if (StructureNavComp().pathingStampId < 0) return;
    if (auto* pf = game.GetPathfinding()) {
        if (pf->HasPathingMap()) pf->GetPathingMap()->UnstampFootprint(StructureNavComp().pathingStampId);
    }
    StructureNavComp().pathingStampId = -1;
}

void CUnit::DropDeathItems(CSimulation& game) {
    if (CorpseDropComp().deathDrops.empty()) return;
    for (const auto& drop : CorpseDropComp().deathDrops) {
        if (drop.itemId.value == 0 || drop.chancePercent <= 0) continue;
        if (drop.chancePercent < 100 &&
            game.SimRand01() * 100.0f >= (float)drop.chancePercent) {
            continue;
        }
        auto* itemData = game.GetItemData();
        if (!itemData || !itemData->HasItem(drop.itemId)) continue;
        game.CreateItem(drop.itemId, GetX(), GetY());
    }
}

void CUnit::PopoutWorker(CSimulation& game) {
    const int workerHandle = ConstructionComp().workerHandleId;
    const bool inside = ConstructionComp().workerInside;
    ConstructionComp().workerHandleId = -1;
    ConstructionComp().workerInside = false;
    // Undead/human builders stay outside (inside=false); nothing to pop.
    if (workerHandle < 0 || !inside) return;
    CUnit* worker = game.GetUnit(workerHandle);
    if (!worker || worker->IsDead()) return;
    worker->SetInvulnerable(false);
    worker->SetHidden(false);
    worker->SetPaused(false);
    worker->SetPositionAndCheckUnstuck(GetX(), GetY(), game);
    if (ConstructionComp().constructionConsumesWorker) {
        int restore = 0;
        if (auto* data = game.GetUnitData()) {
            if (auto* type = data->GetUnitType(worker->GetTypeId())) {
                restore = type->GetFoodUsed();
            }
        }
        if (restore > 0) {
            worker->SetFoodUsed(restore);
            if (CPlayer* player = game.GetPlayer(worker->GetPlayerIndex())) {
                player->SetFoodUsed(player->GetFoodUsed() + restore);
            }
        }
    }
    ConstructionComp().constructionConsumesWorker = false;
}

void CUnit::RefreshMaxLifeTotal() {
    maxLife_ = attrs_.ComputeMaxLife();
    if (life_ > maxLife_) life_ = maxLife_;
}

void CUnit::RefreshMaxManaTotal() {
    maxMana_ = attrs_.ComputeMaxMana();
    if (mana_ > (float)maxMana_) mana_ = (float)maxMana_;
}

void CUnit::MorphTo(CUnitType* form, War3ID formId) {
    if (!form) return;
    if (!MorphComp().morphed) {
        MorphComp().morphed = true;
        MorphComp().originalType = unitType_;
        MorphComp().originalTypeId = typeId_;
        MorphComp().savedSpeed = attrs_.GetBaseSpeed();
        MorphComp().savedBaseMaxLife = attrs_.GetBaseMaxLife();
        MorphComp().savedBaseMaxMana = attrs_.GetBaseMaxMana();
        MorphComp().savedDefense = attrs_.GetCurrentDefense();
        MorphComp().savedTurnRate = turnRate_;
        MorphComp().savedMovementType = movementType_;
        MorphComp().savedCollisionSize = collisionSize_;
        MorphComp().savedBuilding = IsBuilding();
        MorphComp().savedFlyHeight = GetFlyHeight();
    }
    const float oldMaxLife = maxLife_;
    const float oldMaxMana = (float)maxMana_;
    const float lifeRatio = (oldMaxLife > 0.0f) ? (life_ / oldMaxLife) : 1.0f;
    const float manaRatio = (oldMaxMana > 0.0f) ? (mana_ / oldMaxMana) : 1.0f;

    unitType_ = form;
    typeId_ = formId;
    attrs_.SetBaseSpeed(form->GetSpeed());
    attrs_.SetBaseMaxLife(form->GetMaxLife());
    attrs_.SetBaseMaxMana(form->GetManaMaximum());
    attrs_.SetCurrentDefense(form->GetDefense());
    turnRate_ = form->GetTurnRate();
    movementType_ = form->GetMovementType();
    collisionSize_ = form->GetCollisionSize();
    SetStateFlag(UnitStateFlag::Building, form->IsBuilding());
    SetFlyHeight(form->GetDefaultFlyingHeight());
    RefreshMaxLifeTotal();
    RefreshMaxManaTotal();
    life_ = std::clamp(lifeRatio * maxLife_, 1.0f, maxLife_);
    mana_ = std::clamp(manaRatio * (float)maxMana_, 0.0f, (float)maxMana_);
}

void CUnit::Unmorph() {
    if (!MorphComp().morphed) return;
    MorphComp().morphed = false;
    const float oldMaxLife = maxLife_;
    const float oldMaxMana = (float)maxMana_;
    const float lifeRatio = (oldMaxLife > 0.0f) ? (life_ / oldMaxLife) : 1.0f;
    const float manaRatio = (oldMaxMana > 0.0f) ? (mana_ / oldMaxMana) : 1.0f;

    unitType_ = MorphComp().originalType;
    typeId_ = MorphComp().originalTypeId;
    attrs_.SetBaseSpeed(MorphComp().savedSpeed);
    attrs_.SetBaseMaxLife(MorphComp().savedBaseMaxLife);
    attrs_.SetBaseMaxMana(MorphComp().savedBaseMaxMana);
    attrs_.SetCurrentDefense(MorphComp().savedDefense);
    turnRate_ = MorphComp().savedTurnRate;
    movementType_ = MorphComp().savedMovementType;
    collisionSize_ = MorphComp().savedCollisionSize;
    SetStateFlag(UnitStateFlag::Building, MorphComp().savedBuilding);
    SetFlyHeight(MorphComp().savedFlyHeight);
    RefreshMaxLifeTotal();
    RefreshMaxManaTotal();
    life_ = std::clamp(lifeRatio * maxLife_, 1.0f, maxLife_);
    mana_ = std::clamp(manaRatio * (float)maxMana_, 0.0f, (float)maxMana_);
}

void CUnit::SetAttackModifier(int attackIndex, War3ID installer,
                              std::unique_ptr<CAttackModifierEffect> effect) {
    if (!effect) {
        ClearAttackModifier(attackIndex, installer);
        return;
    }
    for (auto& slot : attackModifiers_) {
        if (slot.attackIndex == attackIndex) {
            slot.installer = installer;
            slot.effect = std::move(effect);
            return;
        }
    }
    attackModifiers_.push_back(AttackModifierSlot{attackIndex, installer, std::move(effect)});
}

void CUnit::ClearAttackModifier(int attackIndex, War3ID installer) {
    for (auto it = attackModifiers_.begin(); it != attackModifiers_.end(); ++it) {
        if (it->attackIndex == attackIndex) {
            if (it->installer == installer) {
                attackModifiers_.erase(it);
            }
            return;
        }
    }
}

const CAttackModifierEffect* CUnit::GetAttackModifier(int attackIndex) const {
    for (const auto& slot : attackModifiers_) {
        if (slot.attackIndex == attackIndex) {
            return slot.effect.get();
        }
    }
    return nullptr;
}

CAttackModifierEffect* CUnit::GetAttackModifier(int attackIndex) {
    for (auto& slot : attackModifiers_) {
        if (slot.attackIndex == attackIndex) {
            return slot.effect.get();
        }
    }
    return nullptr;
}

void CUnit::FireAttackModifiers(CSimulation& game, CUnit* victim, float damageDealt) {
    if (!victim || victim->IsDead()) return;
    CAttackModifierEffect* effect = GetAttackModifier(0);
    if (!effect) return;
    if (!effect->ChargeResources(this)) return;
    effect->OnAttackLanded(game, this, victim, damageDealt);
}

void CUnit::AddBonusMaxLife(float v) {
    attrs_.AddBonusMaxLife(v);
    RefreshMaxLifeTotal();
}

void CUnit::AddBonusMaxMana(int v) {
    attrs_.AddBonusMaxMana(v);
    RefreshMaxManaTotal();
}

void CUnit::SetHeroBonusLife(float v) {
    attrs_.SetHeroBonusLife(v);
    RefreshMaxLifeTotal();
}

void CUnit::SetHeroBonusMana(int v) {
    attrs_.SetHeroBonusMana(v);
    RefreshMaxManaTotal();
}

void CUnit::SetPositionAndCheckUnstuck(float x, float y, CSimulation& simulation) {
    CPathfindingProcessor* path = simulation.GetPathfinding();
    if (path && path->HasPathingMap()) {
        const float radius = std::max(0.0f, collisionSize_);
        auto fits = [&](float px, float py) {
            return path->IsWorldPointPathableForUnit(px, py, radius, movementType_, this);
        };
        if (!fits(x, y)) {
            constexpr float kStep = 32.0f;
            bool found = false;
            for (int ring = 1; ring <= 8 && !found; ++ring) {
                for (int i = -ring; i <= ring && !found; ++i) {
                    const float samples[4][2] = {
                        {x + i * kStep, y + ring * kStep},
                        {x + i * kStep, y - ring * kStep},
                        {x + ring * kStep, y + i * kStep},
                        {x - ring * kStep, y + i * kStep},
                    };
                    for (const auto& sample : samples) {
                        if (!fits(sample[0], sample[1])) continue;
                        x = sample[0];
                        y = sample[1];
                        found = true;
                        break;
                    }
                }
            }
        }
    }
    x_ = x;
    y_ = y;
}

bool CUnit::TurnTowardPoint(float targetX, float targetY) {
    const float dx = targetX - x_;
    const float dy = targetY - y_;
    if ((dx * dx + dy * dy) <= 0.01f) {
        return true;
    }

    float goalFacing = std::atan2(dy, dx) * 180.0f / kPi;
    goalFacing = NormalizeFacingDegrees(goalFacing);
    float currentFacing = NormalizeFacingDegrees(facing_);
    float delta = NormalizeFacingDelta(goalFacing - currentFacing);
    float absDelta = std::fabs(delta);

    float turnStepDegrees = std::max(0.1f, turnRate_ * 180.0f / kPi);

    if (absDelta <= 1.0f && absDelta != 0.0f) {
        facing_ = goalFacing;
        delta = 0.0f;
        absDelta = 0.0f;
    } else if (absDelta > 0.0f) {
        float angleToAdd = (delta < 0.0f ? -1.0f : 1.0f) * turnStepDegrees;
        if (absDelta < std::fabs(angleToAdd)) {
            angleToAdd = delta;
        }
        facing_ = NormalizeFacingDegrees(currentFacing + angleToAdd);
        delta = NormalizeFacingDelta(goalFacing - facing_);
        absDelta = std::fabs(delta);
    }

    float propulsionWindow = 60.0f;
    if (unitType_) {
        float eff = GetEffectivePropWindow();
        if (eff > 0.0f) {
            propulsionWindow = (eff <= 3.15f) ? (eff * 180.0f / kPi) : eff;
        }
    } else if (propWindow_ > 0.0f) {
        propulsionWindow = (propWindow_ <= 3.15f) ? (propWindow_ * 180.0f / kPi) : propWindow_;
    }
    propulsionWindow = std::max(propulsionWindow, 60.0f);

    return absDelta <= propulsionWindow;
}

// ===== Ability System =====

void CUnit::AddAbility(CSimulation& game, std::unique_ptr<CAbility> ability) {
    if (!ability) return;

    if (!ability->IsRequirementsMet(game, this)) {
        ability->SetDisabled(true, CAbilityDisableType::REQUIREMENTS);
    }

    auto* player = game.GetPlayer(playerIndex_);
    if (player && player->IsAbilityDisabled(ability->GetAlias())) {
        ability->SetDisabled(true, CAbilityDisableType::PLAYER);
    }

    CAbility* rawPtr = ability.get();
    ownedAbilities_.push_back(std::move(ability));
    abilities_.push_back(rawPtr);

    // Phase 16: Delegate game-specific ability data binding (AbilityData.slk
    // spell scaling, CargoHold capacity, and Load range) to ISimulationModule.
    if (auto* mod = game.GetSimulationModule()) {
        mod->OnUnitAbilityAdded(game, *this, *rawPtr);
    }

    if (!rawPtr->IsDisabled()) {
        rawPtr->OnAdd(game, this);
    }
}

void CUnit::RemoveAbility(CSimulation& game, int abilityHandleId) {
    auto it = std::find_if(ownedAbilities_.begin(), ownedAbilities_.end(),
        [abilityHandleId](const std::unique_ptr<CAbility>& a) {
            return a && a->GetHandleId() == abilityHandleId;
        });

    if (it != ownedAbilities_.end()) {
        CAbility* rawPtr = it->get();
        if (!rawPtr->IsDisabled()) {
            rawPtr->OnRemove(game, this);
        }
        abilities_.erase(
            std::remove(abilities_.begin(), abilities_.end(), rawPtr),
            abilities_.end());
        ownedAbilities_.erase(it);
    }
}

bool CUnit::RemoveAbility(CSimulation& game, War3ID abilityCode) {
    CAbility* ability = GetAbility(abilityCode);
    if (!ability) {
        return false;
    }
    const int handleId = ability->GetHandleId();
    RemoveAbility(game, handleId);
    return GetAbility(abilityCode) == nullptr;
}

CAbility* CUnit::GetAbility(War3ID code) const {
    for (auto* ab : abilities_) {
        if (ab && (ab->GetCode() == code || ab->GetAlias() == code)) {
            return ab;
        }
    }
    return nullptr;
}

// ===== Buff System =====

void CUnit::AddBuff(std::unique_ptr<CBuff> buff, CSimulation& game) {
    if (!buff) return;

    // Phase 16: Delegate negative buff duration scaling (e.g., Resistant Skin 0.25x)
    // to the active ISimulationModule.
    if (buff->IsNegative()) {
        if (const auto* mod = game.GetSimulationModule()) {
            const float durMult = mod->GetNegativeBuffDurationMultiplier(*this);
            if (durMult != 1.0f) {
                buff->ScaleDuration(durMult);
            }
        }
    }

    for (auto& existing : buffs_) {
        if (existing && existing->GetId() == buff->GetId()) {
            if (existing->MergeByCaster() && buff->MergeByCaster() &&
                existing->GetCasterHandleId() != buff->GetCasterHandleId()) {
                continue;
            }
            if (existing->GetStackType() == CBuffStackType::REPLACE ||
                buff->GetStackType() == CBuffStackType::REPLACE) {
                existing->OnRemove(game, this);
                buff->OnApply(game, this);
                existing = std::move(buff);
                return;
            }
            if (existing->GetStackType() == CBuffStackType::STACK) {
                existing->AddStack();
            } else {
                existing->OnRefresh(game, this, *buff);
            }
            return;
        }
    }

    buff->OnApply(game, this);
    buffs_.push_back(std::move(buff));
}

void CUnit::SyncDataDrivenBuffModifiers(DataDrivenBuffRecord& rec, bool apply) {
    const float stacks = static_cast<float>(std::max(1, rec.stackCount));
    if (apply) {
        if (rec.spec.moveSpeedPercent != 0.0f) {
            attrs_.AddSpeedModifier(rec.spec.id, rec.spec.moveSpeedPercent * stacks);
        }
        if (rec.spec.attackSpeedBonus != 0.0f) {
            attrs_.SetNonStackingAttackSpeed(rec.spec.id, rec.spec.attackSpeedBonus * stacks);
        }
        if (rec.spec.armorDelta != 0.0f) {
            const float newArmor = rec.spec.armorDelta * stacks;
            const float diff = newArmor - rec.appliedArmorTotal;
            attrs_.AddTempDefenseBonus(diff);
            rec.appliedArmorTotal = newArmor;
        }
        if (rec.spec.flatDamageBonus != 0) {
            attrs_.SetNonStackingDamageBonus(rec.spec.id, rec.spec.flatDamageBonus * rec.stackCount);
        }
    } else {
        if (rec.spec.moveSpeedPercent != 0.0f) {
            attrs_.RemoveSpeedModifier(rec.spec.id);
        }
        if (rec.spec.attackSpeedBonus != 0.0f) {
            attrs_.ClearNonStackingAttackSpeed(rec.spec.id);
        }
        if (rec.appliedArmorTotal != 0.0f) {
            attrs_.AddTempDefenseBonus(-rec.appliedArmorTotal);
            rec.appliedArmorTotal = 0.0f;
        }
        if (rec.spec.flatDamageBonus != 0) {
            attrs_.ClearNonStackingDamageBonus(rec.spec.id);
        }
    }
}

bool CUnit::ApplyDataDrivenBuff(const DataDrivenBuffSpec& spec, CSimulation& game) {
    if (spec.id.value == 0 || spec.duration <= 0.0f) return false;

    float effectiveDuration = spec.duration;
    if (!spec.isPositive) {
        if (const auto* mod = game.GetSimulationModule()) {
            effectiveDuration *= mod->GetNegativeBuffDurationMultiplier(*this);
        }
    }

    for (auto& existing : dataDrivenBuffs_) {
        if (existing.active && existing.spec.id == spec.id) {
            ++zeroAllocBuffAppliedCount_;
            if (existing.spec.stackType == CBuffStackType::REPLACE ||
                spec.stackType == CBuffStackType::REPLACE) {
                SyncDataDrivenBuffModifiers(existing, false);
                existing.spec = spec;
                existing.spec.duration = effectiveDuration;
                existing.remaining = effectiveDuration;
                existing.periodicTimer = 0.0f;
                existing.stackCount = 1;
                SyncDataDrivenBuffModifiers(existing, true);
                return true;
            }
            if (existing.spec.stackType == CBuffStackType::STACK) {
                const int maxSt = std::max(1, spec.maxStacks);
                if (existing.stackCount < maxSt) {
                    existing.stackCount++;
                }
                existing.remaining = effectiveDuration;
                SyncDataDrivenBuffModifiers(existing, true);
                return true;
            }
            existing.remaining = effectiveDuration;
            return true;
        }
    }

    const size_t prevCap = dataDrivenBuffs_.capacity();
    DataDrivenBuffRecord rec;
    rec.spec = spec;
    rec.spec.duration = effectiveDuration;
    rec.remaining = effectiveDuration;
    rec.periodicTimer = 0.0f;
    rec.stackCount = 1;
    rec.appliedArmorTotal = 0.0f;
    rec.suspended = false;
    rec.active = true;
    SyncDataDrivenBuffModifiers(rec, true);
    dataDrivenBuffs_.push_back(rec);
    if (dataDrivenBuffs_.capacity() == prevCap) {
        ++zeroAllocBuffAppliedCount_;
    }
    return true;
}

bool CUnit::RemoveDataDrivenBuff(War3ID buffId, CSimulation& game) {
    (void)game;
    for (size_t i = 0; i < dataDrivenBuffs_.size(); ++i) {
        if (dataDrivenBuffs_[i].active && dataDrivenBuffs_[i].spec.id == buffId) {
            SyncDataDrivenBuffModifiers(dataDrivenBuffs_[i], false);
            dataDrivenBuffs_[i] = dataDrivenBuffs_.back();
            dataDrivenBuffs_.pop_back();
            return true;
        }
    }
    return false;
}

const DataDrivenBuffRecord* CUnit::GetDataDrivenBuff(War3ID buffId) const {
    for (const auto& rec : dataDrivenBuffs_) {
        if (rec.active && rec.spec.id == buffId) {
            return &rec;
        }
    }
    return nullptr;
}

void CUnit::RemoveBuff(War3ID buffId, CSimulation& game) {
    if (RemoveDataDrivenBuff(buffId, game)) {
        return;
    }
    for (auto it = buffs_.begin(); it != buffs_.end(); ++it) {
        if (it->get() && it->get()->GetId() == buffId) {
            it->get()->OnRemove(game, this);
            buffs_.erase(it);
            return;
        }
    }
}

void CUnit::RemoveBuffByIdAndCaster(War3ID buffId, int caster, CSimulation& game) {
    for (size_t i = 0; i < dataDrivenBuffs_.size(); ++i) {
        if (dataDrivenBuffs_[i].active && dataDrivenBuffs_[i].spec.id == buffId &&
            dataDrivenBuffs_[i].spec.casterHandleId == caster) {
            SyncDataDrivenBuffModifiers(dataDrivenBuffs_[i], false);
            dataDrivenBuffs_[i] = dataDrivenBuffs_.back();
            dataDrivenBuffs_.pop_back();
            return;
        }
    }
    for (auto it = buffs_.begin(); it != buffs_.end(); ++it) {
        if (it->get() && it->get()->GetId() == buffId &&
            it->get()->GetCasterHandleId() == caster) {
            it->get()->OnRemove(game, this);
            buffs_.erase(it);
            return;
        }
    }
}

void CUnit::RemoveBuffsIf(CSimulation& game, bool removePositive, bool removeNegative,
                          bool timedLifeOnly, bool keepTimedLife) {
    if (!timedLifeOnly) {
        for (size_t i = 0; i < dataDrivenBuffs_.size();) {
            auto& rec = dataDrivenBuffs_[i];
            bool match = rec.active;
            if (match && rec.spec.isPositive && !removePositive) match = false;
            if (match && !rec.spec.isPositive && !removeNegative) match = false;
            if (match) {
                SyncDataDrivenBuffModifiers(rec, false);
                dataDrivenBuffs_[i] = dataDrivenBuffs_.back();
                dataDrivenBuffs_.pop_back();
            } else {
                ++i;
            }
        }
    }
    for (auto it = buffs_.begin(); it != buffs_.end();) {
        CBuff* buff = it->get();
        bool match = buff != nullptr;
        const bool isTimedLife = dynamic_cast<CBuffTimedLife*>(buff) != nullptr;
        if (match && timedLifeOnly) {
            match = isTimedLife;
        }
        if (match && keepTimedLife && isTimedLife) match = false;
        if (match && buff->IsPositive() && !removePositive) match = false;
        if (match && buff->IsNegative() && !removeNegative) match = false;
        if (match) {
            buff->OnRemove(game, this);
            it = buffs_.erase(it);
        } else {
            ++it;
        }
    }
}

CBuff* CUnit::GetBuff(War3ID buffId) const {
    for (auto& buff : buffs_) {
        if (buff && buff->GetId() == buffId) {
            return buff.get();
        }
    }
    return nullptr;
}

bool CUnit::HasBuff(War3ID buffId) const {
    return GetBuff(buffId) != nullptr || GetDataDrivenBuff(buffId) != nullptr;
}

bool CUnit::CheckSpellEffectReaction(CSimulation& game, CUnit* caster) {
    if (auto* mod = game.GetSimulationModule()) {
        return mod->OnCheckSpellEffectReaction(game, *this, caster);
    }
    return true;
}

bool CUnit::IsHexed() const {
    for (const auto& buff : buffs_) {
        if (buff && dynamic_cast<const CBuffHex*>(buff.get())) return true;
    }
    return false;
}

void CUnit::SetSharesVisionWith(int player, bool share) {
    if (player < 0) return;
    if (share) {
        ScriptOverridesComp().sharedVisionPlayers.insert(player);
    } else if (auto* sc = ActiveRegistry().TryGet<UnitScriptOverridesComponent>(handleId_)) {
        sc->sharedVisionPlayers.erase(player);
    }
}

bool CUnit::IsStealthed() const {
    if (IsInvisible()) return true;
    for (const auto& buff : buffs_) {
        if (buff && dynamic_cast<const CBuffWindWalk*>(buff.get())) return true;
    }
    return false;
}

bool CUnit::IsInvisibleTo(const CUnit* viewer) const {
    if (!viewer || viewer->GetPlayerIndex() == playerIndex_) return false;
    if (GetBuff(War3ID("Bfae"))) return false;
    bool hidden = IsInvisible();
    if (!hidden) {
        for (const auto& buff : buffs_) {
            if (buff && dynamic_cast<const CBuffWindWalk*>(buff.get())) {
                hidden = true;
                break;
            }
        }
    }
    if (!hidden) return false;
    if (viewer->IsDetector() && viewer->GetUnitType()) {
        const float sight = static_cast<float>(viewer->GetUnitType()->GetSightRadiusDay());
        const float dx = GetX() - viewer->GetX();
        const float dy = GetY() - viewer->GetY();
        if (dx * dx + dy * dy <= sight * sight) return false;
    }
    return true;
}

bool CUnit::IsSilenced() const {
    for (const auto& buff : buffs_) {
        if (buff && dynamic_cast<const CBuffSilence*>(buff.get())) return true;
    }
    return false;
}

bool CUnit::IsAsleep() const {
    for (const auto& buff : buffs_) {
        if (buff && dynamic_cast<const CBuffSleep*>(buff.get())) return true;
    }
    return false;
}

bool CUnit::IsSummoned() const {
    for (const auto& buff : buffs_) {
        if (buff && dynamic_cast<const CBuffTimedLife*>(buff.get())) return true;
    }
    return false;
}

void CUnit::AddAuraStatBonus(CAuraEffectType type, bool percent, War3ID buffId,
                             float value, int caster) {
    attrs_.AddAuraStatBonus(type, percent, buffId, value, caster, unitType_, GetMaxMana());
}

void CUnit::RemoveAuraStatBonus(CAuraEffectType type, bool percent, War3ID buffId, int caster) {
    attrs_.RemoveAuraStatBonus(type, percent, buffId, caster, unitType_, GetMaxMana());
}

void CUnit::RecomputeAllAuraStats() {
    attrs_.RecomputeAllAuraStats(unitType_, GetMaxMana());
}

void CUnit::RecomputeAuraStat(CAuraEffectType type, bool percent) {
    attrs_.RecomputeAuraStat(type, percent, unitType_, GetMaxMana());
}

void CUnit::SetNonStackingDamageBonus(War3ID key, int flatValue) {
    attrs_.SetNonStackingDamageBonus(key, flatValue);
}

void CUnit::SetNonStackingDamagePctBonus(War3ID key, int flatValue) {
    attrs_.SetNonStackingDamagePctBonus(key, flatValue);
}

void CUnit::ClearNonStackingDamageBonus(War3ID key) {
    attrs_.ClearNonStackingDamageBonus(key);
}

int CUnit::GetNonStackingDamageTotal() const {
    return attrs_.GetNonStackingDamageTotal();
}

void CUnit::SetNonStackingAttackSpeed(War3ID key, float bonus) {
    attrs_.SetNonStackingAttackSpeed(key, bonus);
}

void CUnit::ClearNonStackingAttackSpeed(War3ID key) {
    attrs_.ClearNonStackingAttackSpeed(key);
}

float CUnit::GetNonStackingAttackSpeedTotal() const {
    return attrs_.GetNonStackingAttackSpeedTotal();
}

void CUnit::UpdateBuffs(CSimulation& game) {
    const float dt = game.GetSimStepTime();
    for (size_t i = 0; i < dataDrivenBuffs_.size();) {
        DataDrivenBuffRecord& rec = dataDrivenBuffs_[i];
        if (!rec.active) {
            dataDrivenBuffs_[i] = dataDrivenBuffs_.back();
            dataDrivenBuffs_.pop_back();
            continue;
        }
        const War3ID buffId = rec.spec.id;
        bool removedDuringCallback = false;
        if (!rec.suspended) {
            rec.remaining -= dt;
            if (rec.spec.periodic && rec.spec.periodicInterval > 0.0f) {
                rec.periodicTimer += dt;
                while (dataDrivenBuffs_[i].periodicTimer >= dataDrivenBuffs_[i].spec.periodicInterval) {
                    dataDrivenBuffs_[i].periodicTimer -= dataDrivenBuffs_[i].spec.periodicInterval;
                    const float totalDmg = dataDrivenBuffs_[i].spec.periodicDamage *
                                           static_cast<float>(std::max(1, dataDrivenBuffs_[i].stackCount));
                    const int casterId = dataDrivenBuffs_[i].spec.casterHandleId;
                    if (totalDmg > 0.0f) {
                        CUnit* caster = (casterId >= 0) ? game.GetUnit(casterId) : nullptr;
                        Damage(game, caster, false, false, CAttackType::SPELLS, CDamageType::MAGIC, "", totalDmg, 0.0f);
                        if (IsDead()) return;
                        if (i >= dataDrivenBuffs_.size() || !dataDrivenBuffs_[i].active ||
                            dataDrivenBuffs_[i].spec.id != buffId) {
                            removedDuringCallback = true;
                            break;
                        }
                    } else if (totalDmg < 0.0f) {
                        SetLife(game, std::min(GetMaxLife(), GetLife() - totalDmg));
                    }
                }
            }
        }
        if (removedDuringCallback) {
            continue;
        }
        if (dataDrivenBuffs_[i].remaining <= 0.0f) {
            SyncDataDrivenBuffModifiers(dataDrivenBuffs_[i], false);
            dataDrivenBuffs_[i] = dataDrivenBuffs_.back();
            dataDrivenBuffs_.pop_back();
        } else {
            ++i;
        }
    }

    for (auto it = buffs_.begin(); it != buffs_.end();) {
        auto& buff = *it;
        if (!buff) {
            it = buffs_.erase(it);
            continue;
        }

        buff->OnTick(game, this);

        if (buff->IsExpired()) {
            buff->OnRemove(game, this);
            it = buffs_.erase(it);
        } else {
            ++it;
        }
    }
}

// ===== Behavior System =====

CDefenseType CUnit::GetDefenseType() const {
    if (unitType_) return unitType_->GetDefenseType();
    return CDefenseType::NONE;
}

float CUnit::GetEffectivePropWindow() const {
    if (propWindow_ >= 0.0f) return propWindow_;
    if (unitType_) return unitType_->GetPropWindow();
    return 1.0f;
}

float CUnit::GetAttackRange(int index) const {
    float base = 100.0f;
    if (unitType_ && index >= 0 && index < (int)unitType_->GetAttacks().size()) {
        base = unitType_->GetAttacks()[(size_t)index].range;
    }
    return base + attrs_.GetAttackRangeBonus(index);
}

void CUnit::AddAttackRangeBonus(int index, float v) {
    attrs_.AddAttackRangeBonus(index, v);
}

void CUnit::AddAttackRangeBonusAll(float v) {
    const size_t attackCount = unitType_ ? unitType_->GetAttacks().size() : 1;
    attrs_.AddAttackRangeBonusAll(v, attackCount);
}

void CUnit::SetCurrentBehavior(std::unique_ptr<CBehavior> behavior) {
    currentBehavior_ = nullptr;
    ownedCurrentBehavior_ = std::move(behavior);
    currentBehavior_ = ownedCurrentBehavior_.get();
    if (currentBehavior_) {
        currentBehavior_->SetUnit(this);
    }
}

// ===== Cargo / transport =====

bool CUnit::LoadUnit(CUnit* passenger) {
    if (!passenger || passenger == this || passenger->IsDead()) return false;
    for (auto* loaded : CargoComp().cargoUnits) {
        if (loaded == passenger) return true;
    }
    passenger->CargoComp().loadedAboard = true;
    passenger->SetHidden(true);
    CargoComp().cargoUnits.push_back(passenger);
    if (unitLoadedHook_) {
        unitLoadedHook_(passenger->GetHandleId());
    }
    return true;
}

void CUnit::UnloadUnitAt(CUnit* passenger, float x, float y) {
    if (!passenger) return;
    auto it = std::find(CargoComp().cargoUnits.begin(), CargoComp().cargoUnits.end(), passenger);
    if (it == CargoComp().cargoUnits.end()) return;
    CargoComp().cargoUnits.erase(it);
    passenger->CargoComp().loadedAboard = false;
    passenger->SetHidden(false);
    passenger->SetPosition(x, y);
}

bool CUnit::CanBeTargetedBy(CSimulation& simulation, CUnit* source,
                            const std::vector<CTargetType>& targetsAllowed) const {
    if (CargoComp().loadedAboard) return false;
    return CWidget::CanBeTargetedBy(simulation, source, targetsAllowed);
}

void CUnit::IssueOrder(std::unique_ptr<COrder> order, bool queue) {
    if (!order) return;
    if (!queue) {
        while (!orderQueue_.empty()) orderQueue_.pop();
    }
    lastOrderId_ = order->GetOrderId();
    const int targetKind = (int)order->GetTargetKind();
    float x = 0.0f, y = 0.0f;
    int targetHandle = -1;
    if (auto* pt = dynamic_cast<COrderTargetPoint*>(order.get())) {
        x = pt->GetX();
        y = pt->GetY();
    } else if (auto* wg = dynamic_cast<COrderTargetWidget*>(order.get())) {
        if (wg->GetTarget()) targetHandle = wg->GetTarget()->GetHandleId();
    }
    orderQueue_.push(std::move(order));
    if (orderIssuedHook_) {
        orderIssuedHook_(lastOrderId_, targetKind, x, y, targetHandle);
    }
}

void CUnit::ClearOrders() {
    while (!orderQueue_.empty()) orderQueue_.pop();
    currentBehavior_ = nullptr;
    ownedCurrentBehavior_.reset();
}

void CUnit::Stop(CSimulation& simulation) {
    ClearOrders();
    if (defaultBehavior_) {
        defaultBehavior_->SetUnit(this);
        defaultBehavior_->Begin(simulation);
        currentBehavior_ = defaultBehavior_.get();
    }
}

// Behavior storage setters
void CUnit::SetMoveBehavior(std::unique_ptr<CBehaviorMove> b) {
    if (currentBehavior_ == moveBehavior_.get()) currentBehavior_ = nullptr;
    moveBehavior_ = std::move(b);
}
void CUnit::SetFollowBehavior(std::unique_ptr<CBehaviorFollow> b) {
    if (currentBehavior_ == followBehavior_.get()) currentBehavior_ = nullptr;
    followBehavior_ = std::move(b);
}
void CUnit::SetPatrolBehavior(std::unique_ptr<CBehaviorPatrol> b) {
    if (currentBehavior_ == patrolBehavior_.get()) currentBehavior_ = nullptr;
    patrolBehavior_ = std::move(b);
}
void CUnit::SetHoldPositionBehavior(std::unique_ptr<CBehaviorHoldPosition> b) {
    if (currentBehavior_ == holdPositionBehavior_.get()) currentBehavior_ = nullptr;
    holdPositionBehavior_ = std::move(b);
}
void CUnit::SetAttackBehavior(std::unique_ptr<CBehaviorAttack> b) {
    if (currentBehavior_ == attackBehavior_.get()) currentBehavior_ = nullptr;
    attackBehavior_ = std::move(b);
}
void CUnit::SetAttackMoveBehavior(std::unique_ptr<CBehaviorAttackMove> b) {
    if (currentBehavior_ == attackMoveBehavior_.get()) currentBehavior_ = nullptr;
    attackMoveBehavior_ = std::move(b);
}
void CUnit::SetDefaultBehavior(std::unique_ptr<CBehavior> b) {
    if (currentBehavior_ == defaultBehavior_.get()) currentBehavior_ = nullptr;
    defaultBehavior_ = std::move(b);
}

CBehavior* CUnit::PollNextOrderBehavior(CSimulation& simulation) {
    while (!orderQueue_.empty()) {
        auto order = std::move(orderQueue_.front());
        orderQueue_.pop();
        if (order) {
            ConvertOrderToBehavior(simulation, std::move(order));
            if (currentBehavior_) {
                return currentBehavior_;
            }
        }
    }
    return nullptr;
}

bool CUnit::HasQueuedOrders() const {
    return !orderQueue_.empty();
}

void CUnit::Revive(CSimulation& game, float x, float y) {
    CorpseDropComp().corpse = false;
    CorpseDropComp().boneCorpse = false;
    CorpseDropComp().deathTurnTick = 0;
    SetHidden(false);
    SetPosition(x, y);
    RefreshMaxLifeTotal();
    RefreshMaxManaTotal();
    SetLife(game, GetMaxLife());
    // Buildings never revive via heroes, but a restored structure needs
    // its footprint back (ref pathingInstance lifecycle).
    if (IsBuilding() && StructureNavComp().pathingStampId < 0) RegeneratePathingInstance(game);
    // Mana intentionally preserved (reincarnation spends it pre-revive).
}

void CUnit::ApplyRegen(CSimulation& simulation) {
    if (IsDead() || IsBuilding()) return;

    float dt = simulation.GetSimStepTime();
    bool isNight = simulation.IsNighttime();
    CRegenType regenType = unitType_->GetLifeRegenType();

    bool canRegen = (regenType == CRegenType::ALWAYS) ||
                    (regenType == CRegenType::ONLY_AT_NIGHT && isNight);
    if (canRegen && life_ < maxLife_) {
        life_ += (unitType_->GetLifeRegen() + attrs_.GetBonusLifeRegen()) * dt;
        if (life_ > maxLife_) life_ = maxLife_;
    }

    if (mana_ < (float)maxMana_) {
        mana_ += (unitType_->GetManaRegen() + attrs_.GetBonusManaRegen()) * dt;
        if (mana_ > (float)maxMana_) mana_ = (float)maxMana_;
    }
}

float CUnit::GetEndingDecayTime(const CSimulation& simulation) const {
    const auto& gameplayConstants = simulation.GetGameplayConstants();
    if (IsBuilding()) {
        return gameplayConstants.structureDecayTime;
    }
    if (IsHero()) {
        return gameplayConstants.dissipateTime;
    }
    return gameplayConstants.boneDecayTime;
}

bool CUnit::Update(CSimulation& simulation) {
    if (IsDead()) {
        const int currentTick = simulation.GetGameTurnTick();
        if (CorpseDropComp().deathTurnTick == 0) {
            CorpseDropComp().deathTurnTick = currentTick;
            CorpseDropComp().corpse = false;
            CorpseDropComp().boneCorpse = false;
            // Retail death clears the building footprint (ref killPathingInstance).
            KillPathingInstance(simulation);
            if (currentBehavior_) {
                currentBehavior_->End(simulation);
                currentBehavior_ = nullptr;
                ownedCurrentBehavior_.reset();
            }
            while (!orderQueue_.empty()) orderQueue_.pop();
            PopoutWorker(simulation);
            {
                const auto& gc = simulation.GetGameplayConstants();
                if (gc.dyingRevealRadius > 0.0f && gc.fogFlashTime > 0.0f) {
                    if (CFogSystem* fog = simulation.GetFogSystem()) {
                        const CFogArea area =
                            CFogArea::Circle(GetX(), GetY(), gc.dyingRevealRadius);
                        fog->AddTimedModifier(GetPlayerIndex(), area,
                                              CFogState::VISIBLE, gc.fogFlashTime);
                        if (CorpseDropComp().deathKillerPlayer >= 0 &&
                            CorpseDropComp().deathKillerPlayer != GetPlayerIndex()) {
                            fog->AddTimedModifier(CorpseDropComp().deathKillerPlayer, area,
                                                  CFogState::VISIBLE, gc.fogFlashTime);
                        }
                    }
                }
            }
            // Foundations killed mid-build explode instead of leaving a
            // corpse animation (ref kill constructing branch ->
            // createDeathExplodeEffect; presentation-only, sim keeps the
            // normal decay bookkeeping below).
            if (IsConstructing()) {
                simulation.FireDeathExplode(handleId_, explodeBuffId_);
            }
            if (auto* mod = simulation.GetSimulationModule()) {
                mod->OnUnitDied(simulation, *this);
            }
            DropDeathItems(simulation);
            auto passengers = std::move(CargoComp().cargoUnits);
            CargoComp().cargoUnits.clear();
            const float dropX = GetX();
            const float dropY = GetY();
            for (auto* passenger : passengers) {
                if (passenger) {
                    passenger->CargoComp().loadedAboard = false;
                    passenger->SetHidden(false);
                    passenger->SetPosition(dropX, dropY);
                    if (!passenger->IsDead()) {
                        passenger->Damage(simulation, nullptr, false, false,
                                          CAttackType::CHAOS, CDamageType::NORMAL,
                                          "", 1e9f, 0);
                    }
                }
            }
            for (auto* ab : abilities_) {
                if (ab) ab->OnDeath(simulation, this);
            }
            for (const auto& buff : buffs_) {
                if (buff) buff->OnDeath(simulation, this);
            }
            return false;
        }

        if (CorpseDropComp().decaySuspended) return false;

        if (!CorpseDropComp().corpse) {
            const int deathTicks = std::max(1, (int)std::ceil(unitType_->GetDeathTime() / simulation.GetSimStepTime()));
            if ((currentTick - CorpseDropComp().deathTurnTick) > deathTicks) {
                CorpseDropComp().corpse = true;
                if (!unitType_->CanRaise()) {
                    CorpseDropComp().boneCorpse = true;
                }
                if (!IsHero() && !unitType_->CanDecay()) {
                    return true;
                }
                CorpseDropComp().deathTurnTick = currentTick;
                if (auto& decayCb = simulation.GetUnitDecayCallback()) {
                    decayCb(handleId_);
                }
            }
            return false;
        }

        if (!CorpseDropComp().boneCorpse) {
            const int decayTicks = std::max(1, (int)std::ceil(
                simulation.GetGameplayConstants().decayTime / simulation.GetSimStepTime()));
            if ((currentTick - CorpseDropComp().deathTurnTick) > decayTicks) {
                CorpseDropComp().boneCorpse = true;
                CorpseDropComp().deathTurnTick = currentTick;
            }
            return false;
        }

        const int endingDecayTicks = std::max(1, (int)std::ceil(
            GetEndingDecayTime(simulation) / simulation.GetSimStepTime()));
        if ((currentTick - CorpseDropComp().deathTurnTick) > endingDecayTicks) {
            if (IsHero()) {
                SetHidden(true);
                return false;
            }
            return true;
        }
        return false;
    }

    if (IsPaused()) return false;

    ApplyRegen(simulation);

    if (!orderQueue_.empty() && currentBehavior_) {
        const int frontId =
            orderQueue_.front() ? orderQueue_.front()->GetOrderId() : -1;
        if (frontId == OrderIds::CANCEL || frontId == OrderIds::CANCEL_BUILD) {
            if (auto* build = dynamic_cast<CBehaviorBuild*>(currentBehavior_)) {
                if (build->IsActivelyBuilding() || build->IsMovingToSite()) {
                    orderQueue_.pop();
                    build->CancelConstruction(simulation);
                }
            }
        }
    }

    if (currentBehavior_ && !orderQueue_.empty() && currentBehavior_->IsInterruptable()) {
        currentBehavior_->End(simulation);
        if (currentBehavior_ == ownedCurrentBehavior_.get()) {
            ownedCurrentBehavior_.reset();
        }
        currentBehavior_ = nullptr;
    }
    if (currentBehavior_ && !currentBehavior_->IsInterruptable()) {
        while (orderQueue_.size() > 1) orderQueue_.pop();
    }
    if (currentBehavior_) {
        CBehavior* active = currentBehavior_;
        bool done = active->Update(simulation);
        if (done && currentBehavior_ == active) {
            active->End(simulation);
            if (currentBehavior_ == active) {
                if (currentBehavior_ == ownedCurrentBehavior_.get()) {
                    ownedCurrentBehavior_.reset();
                }
                currentBehavior_ = nullptr;
                PollNextOrderBehavior(simulation);
            }
        }
    } else if (!orderQueue_.empty()) {
        PollNextOrderBehavior(simulation);
    } else {
        PerformDefaultBehavior(simulation);
    }

    for (auto* passenger : CargoComp().cargoUnits) {
        if (passenger) passenger->SetPosition(x_, y_);
    }

    for (auto* ab : abilities_) {
        if (ab && !ab->IsDisabled()) {
            ab->OnTick(simulation, this);
        }
    }

    UpdateBuffs(simulation);

    return false;
}

void CUnit::PerformDefaultBehavior(CSimulation& simulation) {
    if (TryAutocast(simulation)) return;
    if (defaultBehavior_) {
        ownedCurrentBehavior_.reset();
        currentBehavior_ = defaultBehavior_.get();
        currentBehavior_->SetUnit(this);
        currentBehavior_->Begin(simulation);
    }
}

bool CUnit::TryAutocast(CSimulation& simulation) {
    if (auto* mod = simulation.GetSimulationModule()) {
        return mod->OnTryUnitIdleAutocast(simulation, *this);
    }
    return false;
}

float CUnit::Damage(CSimulation& simulation, CUnit* source,
                    bool isAttack, bool isRanged,
                    CAttackType attackType, CDamageType damageType,
                    const std::string& weaponSoundType,
                    float damage, float bonusDamage) {
    const float incomingBaseDamage = damage;
    const float incomingBaseBonus = bonusDamage;
    const bool invulnerableTarget = IsInvulnerable();

    auto* simMod = simulation.GetSimulationModule();

    // Phase 16: Delegate pre-mitigation miss rolls (Drunken Haze, Curse, Highland cliff miss),
    // Mana Shield absorption, attacker damage-dealt modifiers (Howl of Terror, AIdd), and
    // Avatar spell immunity to the active ISimulationModule.
    if (simMod) {
        if (!simMod->OnEvaluatePreMitigationDamage(
                simulation, source, *this, isAttack, isRanged,
                static_cast<int>(attackType), static_cast<int>(damageType),
                invulnerableTarget, damage)) {
            return 0.0f;
        }
    }

    // Core armor/attack-type damage calculation (delegated via CCombatSystem -> ISimulationModule)
    DamageResult result = simulation.GetCombatSystem()->CalculateDamage(
        source, this, isAttack, isRanged, attackType, damageType,
        damage, bonusDamage, 0, 0);
    float actualDamage = result.damageDealt;

    if (result.isEvaded) {
        simulation.FireCombatText(source ? source->GetHandleId() : -1, handleId_,
                                  CSimulation::CombatTextKind::MISS, 0.0f);
    } else if (result.isCritical) {
        simulation.FireCombatText(source ? source->GetHandleId() : -1, handleId_,
                                  CSimulation::CombatTextKind::CRIT, actualDamage);
    }

    // Phase 16: Delegate post-mitigation damage-taken modifiers (Defend 0.5x,
    // Berserk taken multiplier, Runed Bracers per-attack-type reduction) to ISimulationModule.
    if (simMod && actualDamage > 0.0f) {
        simMod->OnModifyPostMitigationDamage(
            simulation, source, *this, isAttack, isRanged,
            static_cast<int>(attackType), static_cast<int>(damageType),
            actualDamage);
    }

    float oldLife = life_;
    if (!invulnerableTarget) {
        life_ -= actualDamage;
        if (life_ < 0.0f) life_ = 0.0f;
        if (actualDamage < 0.0f && oldLife <= GetMaxLife() && life_ > GetMaxLife()) {
            life_ = GetMaxLife();
        }
    }

    // Phase 16: Delegate post-hit combat reactions & passive procs (Sleep wake-up,
    // Thorns/Spiked Carapace melee reflection, Vampiric Aura lifesteal, Bash stun proc,
    // Orb/Arrow attack modifiers, Cleaving Attack splash, Wind Walk backstab, Pulverize slam)
    // to ISimulationModule.
    if (simMod) {
        simMod->OnPostDamageLanded(
            simulation, source, *this, isAttack, isRanged,
            static_cast<int>(attackType), static_cast<int>(damageType),
            incomingBaseDamage, incomingBaseBonus, actualDamage,
            result.isEvaded, invulnerableTarget);
    }

    auto& dmgCb = simulation.GetUnitDamageCallback();
    if (dmgCb) {
        int srcHandle = source ? source->GetHandleId() : -1;
        dmgCb(handleId_, srcHandle, actualDamage);
    }

    (void)weaponSoundType;

    if (oldLife > 0.0f && life_ <= 0.0f) {
        CorpseDropComp().deathKillerPlayer = source ? source->GetPlayerIndex() : -1;
        if (simMod) {
            simMod->OnAwardKillBountyAndXP(simulation, source, *this);
        }
        auto& deathCb = simulation.GetUnitDeathCallback();
        int srcHandle = source ? source->GetHandleId() : -1;
        if (deathCb) deathCb(handleId_, srcHandle, playerIndex_);
    } else if (life_ > 0.0f) {
        MaybeReturnFire(simulation, source);
    }

    return oldLife - life_;
}

void CUnit::MaybeReturnFire(CSimulation& simulation, CUnit* source) {
    if (!source || source == this || !unitType_) return;
    if (currentBehavior_ &&
        (currentBehavior_ != defaultBehavior_.get() || !currentBehavior_->IsInterruptable())) {
        return;
    }
    if (IsDead() || IsPaused()) return;

    auto adoptBehavior = [&](CBehavior* behavior) {
        if (!behavior || currentBehavior_ == behavior) return;
        if (currentBehavior_) {
            currentBehavior_->End(simulation);
            if (currentBehavior_ == ownedCurrentBehavior_.get()) {
                ownedCurrentBehavior_.reset();
            }
            currentBehavior_ = nullptr;
        }
        behavior->SetUnit(this);
        behavior->Begin(simulation);
        currentBehavior_ = behavior;
    };

    CPlayer* self = simulation.GetPlayer(GetPlayerIndex());
    CPlayer* foe = simulation.GetPlayer(source->GetPlayerIndex());
    const bool hostile = self && foe && !self->HasAlliance(*foe, CAllianceType::PASSIVE);

    bool isPeon = false;
    if (unitType_) {
        const auto& classes = unitType_->GetClassifications();
        isPeon = classes.find(CUnitClassification::PEON) != classes.end();
    }

    if (hostile && !isPeon && !unitType_->GetAttacks().empty()) {
        if (source->CanBeTargetedBy(simulation, this, {}) && !source->IsDead()) {
            if (CBehaviorAttack* attack = GetAttackBehavior()) {
                attack->SetTarget(source);
                adoptBehavior(attack);
                return;
            }
        }
    }

    if (unitType_->CanFlee() && !IsRooted() && GetPlayerIndex() != source->GetPlayerIndex()) {
        if (CBehaviorMove* move = GetMoveBehavior()) {
            const float dx = GetX() - source->GetX();
            const float dy = GetY() - source->GetY();
            if (dx * dx + dy * dy > 0.01f) {
                const float ang = std::atan2(dy, dx);
                const float step = (float)GetSpeed();
                move->SetTarget(GetX() + step * std::cos(ang),
                                GetY() + step * std::sin(ang));
                adoptBehavior(move);
            }
        }
    }
}

void CUnit::ConvertOrderToBehavior(CSimulation& simulation, std::unique_ptr<COrder> order) {
    if (!order) return;
    int orderId = order->GetOrderId();

    auto tryAbilityRange = [&](bool movementPass) -> bool {
        for (auto* ab : abilities_) {
            if (!ab || ab->IsDisabled()) continue;
            if ((ab->GetCategory() == CAbilityCategory::MOVEMENT) != movementPass) continue;

            CBehavior* newBehavior = nullptr;

            if (auto* widgetOrder = dynamic_cast<COrderTargetWidget*>(order.get())) {
                if (ab->CheckCanTarget(simulation, this, orderId, widgetOrder->GetTarget())) {
                    if (!ab->CheckBeforeQueue(simulation, this, orderId, widgetOrder->GetTarget())) {
                        return true;
                    }
                    newBehavior = ab->Begin(simulation, this, orderId, widgetOrder->GetTarget());
                    if (!newBehavior) {
                        return true;
                    }
                }
            } else if (auto* pointOrder = dynamic_cast<COrderTargetPoint*>(order.get())) {
                if (ab->CheckCanTargetPoint(simulation, this, orderId, pointOrder->GetX(), pointOrder->GetY())) {
                    if (!ab->CheckBeforeQueue(simulation, this, orderId, pointOrder->GetX(), pointOrder->GetY())) {
                        return true;
                    }
                    newBehavior = ab->Begin(simulation, this, orderId, pointOrder->GetX(), pointOrder->GetY());
                    if (!newBehavior) {
                        return true;
                    }
                }
            } else if (dynamic_cast<COrderNoTarget*>(order.get())) {
                if (ab->CheckCanTargetNoTarget(simulation, this, orderId)) {
                    if (!ab->CheckBeforeQueueNoTarget(simulation, this, orderId)) {
                        return true;
                    }
                    newBehavior = ab->BeginNoTarget(simulation, this, orderId);
                    if (!newBehavior) {
                        return true;
                    }
                }
            }

            if (newBehavior) {
                std::ostringstream oss;
                oss << "[MoveDebug] dispatch"
                    << " unit=" << handleId_
                    << " type=" << typeId_.ToString()
                    << " order=" << orderId
                    << " ability=" << ab->GetAlias().ToString()
                    << " abilityCat=" << AbilityCategoryName(ab->GetCategory())
                    << " behavior=" << BehaviorCategoryName(newBehavior->GetCategory());
                if (auto* widgetOrder = dynamic_cast<COrderTargetWidget*>(order.get())) {
                    auto* target = widgetOrder->GetTarget();
                    if (target) {
                        oss << " targetWidget=" << target->GetHandleId()
                            << " targetPos=(" << target->GetX() << ", " << target->GetY() << ")";
                    }
                } else if (auto* pointOrder = dynamic_cast<COrderTargetPoint*>(order.get())) {
                    oss << " targetPoint=(" << pointOrder->GetX() << ", " << pointOrder->GetY() << ")";
                }
                MoveDebugLogLine(oss.str());
                if (currentBehavior_) {
                    currentBehavior_->End(simulation);
                    if (currentBehavior_ == ownedCurrentBehavior_.get()) {
                        ownedCurrentBehavior_.reset();
                    }
                    currentBehavior_ = nullptr;
                }
                newBehavior->SetUnit(this);
                newBehavior->Begin(simulation);
                currentBehavior_ = newBehavior;
                return true;
            }
        }
        return false;
    };

    const bool movementLast = orderId == OrderIds::SMART;
    if (movementLast) {
        if (tryAbilityRange(false) || tryAbilityRange(true)) {
            return;
        }
    } else if (tryAbilityRange(false) || tryAbilityRange(true)) {
        return;
    }

    if (orderId == OrderIds::SMART) {
        if (auto* widgetOrder = dynamic_cast<COrderTargetWidget*>(order.get())) {
            auto* tgt = widgetOrder->GetTarget();
            if (tgt) {
                ConvertOrderToBehavior(simulation, std::make_unique<COrderTargetPoint>(OrderIds::SMART, tgt->GetX(), tgt->GetY()));
                return;
            }
        }
    }

    if (orderId == OrderIds::STOP || orderId == OrderIds::HOLD_POSITION ||
        dynamic_cast<COrderTargetPoint*>(order.get()) ||
        dynamic_cast<COrderTargetWidget*>(order.get())) {
        auto behavior = std::make_unique<CBehaviorStop>();
        behavior->SetUnit(this);
        behavior->Begin(simulation);
        ownedCurrentBehavior_ = std::move(behavior);
        currentBehavior_ = ownedCurrentBehavior_.get();
        std::ostringstream oss;
        oss << "[MoveDebug] dispatch_stop"
            << " unit=" << handleId_
            << " type=" << typeId_.ToString()
            << " order=" << orderId
            << " behavior=STOP";
        MoveDebugLogLine(oss.str());
    }
}
