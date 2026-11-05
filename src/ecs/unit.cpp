#include "ecs/pathing/pathfinding.h"
#include "csimulation.h"
#include "ecs/buffs.h"
#include "ecs/abilities/ability_spell_base.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/ability_transport.h"
#include "ecs/abilities/spells28.h"
#include "data/abilitydata.h"
#include "data/unitdata.h"
#include "data/itemdata.h"
#include "ecs/abilities/spells4.h"
#include "ecs/abilities/spells5.h"
#include "ecs/abilities/spells9.h"
#include "ecs/abilities/spells10.h"
#include "ecs/abilities/spells14.h"
#include "ecs/abilities/spells16.h"
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

static const char* AbilityCategoryName(CAbilityCategory category) {
    switch (category) {
        case CAbilityCategory::CORE: return "CORE";
        case CAbilityCategory::ATTACK: return "ATTACK";
        case CAbilityCategory::MOVEMENT: return "MOVEMENT";
        case CAbilityCategory::BUFF: return "BUFF";
        case CAbilityCategory::ITEM: return "ITEM";
        case CAbilityCategory::HERO: return "HERO";
        case CAbilityCategory::BUILD: return "BUILD";
        case CAbilityCategory::INVENTORY: return "INVENTORY";
        case CAbilityCategory::HARVEST: return "HARVEST";
        case CAbilityCategory::CARGO_HOLD: return "CARGO_HOLD";
        case CAbilityCategory::UPGRADE: return "UPGRADE";
        case CAbilityCategory::QUEUE: return "QUEUE";
        case CAbilityCategory::RALLY: return "RALLY";
        case CAbilityCategory::NEUTRAL_BUILDING: return "NEUTRAL_BUILDING";
        case CAbilityCategory::GOLD_MINE: return "GOLD_MINE";
        case CAbilityCategory::SELL_ITEMS: return "SELL_ITEMS";
        case CAbilityCategory::ROOT: return "ROOT";
        case CAbilityCategory::MOON_WELL: return "MOON_WELL";
        case CAbilityCategory::BLIGHT: return "BLIGHT";
        case CAbilityCategory::SPELL: return "SPELL";
        default: return "UNKNOWN";
    }
}

static const char* BehaviorCategoryName(CBehaviorCategory category) {
    switch (category) {
        case CBehaviorCategory::NONE: return "NONE";
        case CBehaviorCategory::STOP: return "STOP";
        case CBehaviorCategory::MOVE: return "MOVE";
        case CBehaviorCategory::ATTACK: return "ATTACK";
        case CBehaviorCategory::SPELL: return "SPELL";
        case CBehaviorCategory::ATTACK_MOVE: return "ATTACK_MOVE";
        case CBehaviorCategory::PATROL: return "PATROL";
        case CBehaviorCategory::FOLLOW: return "FOLLOW";
        case CBehaviorCategory::HOLD_POSITION: return "HOLD_POSITION";
        case CBehaviorCategory::BUILD: return "BUILD";
        case CBehaviorCategory::HARVEST: return "HARVEST";
        case CBehaviorCategory::RETURN_RESOURCES: return "RETURN_RESOURCES";
        case CBehaviorCategory::BOARD_TRANSPORT: return "BOARD_TRANSPORT";
        case CBehaviorCategory::STUN: return "STUN";
        case CBehaviorCategory::INVENTORY: return "INVENTORY";
        case CBehaviorCategory::ROOT: return "ROOT";
        default: return "UNKNOWN";
    }
}

}

CUnit::CUnit(int handleId, CUnitType* unitType, int playerIndex,
             War3ID typeId, float x, float y, float facing)
    : CWidget(handleId, x, y, (float)unitType->GetMaxLife())
    , unitType_(unitType), typeId_(typeId), playerIndex_(playerIndex)
    , facing_(facing)
    , speed_(unitType->GetSpeed())
    , baseSpeed_(unitType->GetSpeed())
    , baseMaxLife_(unitType->GetMaxLife())
    , baseMaxMana_(unitType->GetManaMaximum())
    , maxMana_(unitType->GetManaMaximum())
    , currentDefense_(unitType->GetDefense())
    , turnRate_(unitType->GetTurnRate())
    , movementType_(unitType->GetMovementType())
    , collisionSize_(unitType->GetCollisionSize())
    , isBuilding_(unitType->IsBuilding())
    , isHero_(unitType->IsHero())
{
    maxLife_ = (float)unitType->GetMaxLife();
    mana_ = (float)unitType->GetManaInitial();
    flyHeight_ = unitType->GetDefaultFlyingHeight();
    foodUsed_ = unitType->GetFoodUsed();
    foodMade_ = unitType->GetFoodMade();
}

float CUnit::GetSpeed() const {
    if (baseSpeed_ <= 0) return 0.0f;
    float base = (float)baseSpeed_ + (float)bonusSpeed_;
    float percent = auraMoveSpeedPercent_;
    for (const auto& [id, mod] : speedModifiers_) {
        percent += mod;
    }
    float finalSpeed = base * (1.0f + percent);
    return std::clamp(finalSpeed, 150.0f, 522.0f);
}

void CUnit::SetSpeed(int s) {
    baseSpeed_ = s;
    speed_ = s;
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
    pathingStampId_ = pf->GetPathingMap()->StampFootprint(ux, uy, 270, img, false);
}

void CUnit::KillPathingInstance(CSimulation& game) {
    if (pathingStampId_ < 0) return;
    if (auto* pf = game.GetPathfinding()) {
        if (pf->HasPathingMap()) pf->GetPathingMap()->UnstampFootprint(pathingStampId_);
    }
    pathingStampId_ = -1;
}

void CUnit::DropDeathItems(CSimulation& game) {
    if (deathDrops_.empty()) return;
    for (const auto& drop : deathDrops_) {
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
    const int workerHandle = workerHandleId_;
    const bool inside = workerInside_;
    workerHandleId_ = -1;
    workerInside_ = false;
    // Undead/human builders stay outside (inside=false); nothing to pop.
    if (workerHandle < 0 || !inside) return;
    CUnit* worker = game.GetUnit(workerHandle);
    if (!worker || worker->IsDead()) return;
    worker->SetInvulnerable(false);
    worker->SetHidden(false);
    worker->SetPaused(false);
    // Step out beside the ruin (ref nudgeAround -> setPointAndCheckUnstuck).
    worker->SetPositionAndCheckUnstuck(GetX(), GetY(), game);
    if (constructionConsumesWorker_) {
        // Night-elf wisps were zeroed at build start (see
        // CBehaviorBuild::CreateBuilding); give their food back (ref
        // popoutWorker restores worker type foodUsed to its player).
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
    constructionConsumesWorker_ = false;
}

void CUnit::RefreshMaxLifeTotal() {
    maxLife_ = (float)baseMaxLife_ + heroBonusLife_ + bonusMaxLife_;
    if (maxLife_ < 1.0f) maxLife_ = 1.0f;
    if (life_ > maxLife_) life_ = maxLife_;
}

void CUnit::RefreshMaxManaTotal() {
    maxMana_ = baseMaxMana_ + heroBonusMana_ + bonusMaxMana_;
    if (maxMana_ < 0) maxMana_ = 0;
    if (mana_ > (float)maxMana_) mana_ = (float)maxMana_;
}

void CUnit::MorphTo(CUnitType* form, War3ID formId) {
    if (!form) return;
    if (!morphed_) {
        morphed_ = true;
        originalType_ = unitType_;
        originalTypeId_ = typeId_;
        savedSpeed_ = baseSpeed_;
        savedBaseMaxLife_ = baseMaxLife_;
        savedBaseMaxMana_ = baseMaxMana_;
        savedDefense_ = currentDefense_;
        savedTurnRate_ = turnRate_;
        savedMovementType_ = movementType_;
        savedCollisionSize_ = collisionSize_;
        savedBuilding_ = isBuilding_;
        savedFlyHeight_ = GetFlyHeight();
    }
    const float oldMaxLife = maxLife_;
    const float oldMaxMana = (float)maxMana_;
    const float lifeRatio = (oldMaxLife > 0.0f) ? (life_ / oldMaxLife) : 1.0f;
    const float manaRatio = (oldMaxMana > 0.0f) ? (mana_ / oldMaxMana) : 1.0f;

    // Attacks read unitType_ live, so the form's arsenal (and model,
    // render-side) applies immediately. Hero/ability state is untouched.
    unitType_ = form;
    typeId_ = formId;
    baseSpeed_ = form->GetSpeed();
    speed_ = baseSpeed_;
    baseMaxLife_ = form->GetMaxLife();
    baseMaxMana_ = form->GetManaMaximum();
    currentDefense_ = form->GetDefense();
    turnRate_ = form->GetTurnRate();
    movementType_ = form->GetMovementType();
    collisionSize_ = form->GetCollisionSize();
    isBuilding_ = form->IsBuilding();
    SetFlyHeight(form->GetDefaultFlyingHeight());
    RefreshMaxLifeTotal();
    RefreshMaxManaTotal();
    life_ = std::clamp(lifeRatio * maxLife_, 1.0f, maxLife_);
    mana_ = std::clamp(manaRatio * (float)maxMana_, 0.0f, (float)maxMana_);
}

void CUnit::Unmorph() {
    if (!morphed_) return;
    morphed_ = false;
    const float oldMaxLife = maxLife_;
    const float oldMaxMana = (float)maxMana_;
    const float lifeRatio = (oldMaxLife > 0.0f) ? (life_ / oldMaxLife) : 1.0f;
    const float manaRatio = (oldMaxMana > 0.0f) ? (mana_ / oldMaxMana) : 1.0f;

    unitType_ = originalType_;
    typeId_ = originalTypeId_;
    baseSpeed_ = savedSpeed_;
    speed_ = baseSpeed_;
    baseMaxLife_ = savedBaseMaxLife_;
    baseMaxMana_ = savedBaseMaxMana_;
    currentDefense_ = savedDefense_;
    turnRate_ = savedTurnRate_;
    movementType_ = savedMovementType_;
    collisionSize_ = savedCollisionSize_;
    isBuilding_ = savedBuilding_;
    SetFlyHeight(savedFlyHeight_);
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
    AttackModifierSlot& slot = attackModifiers_[attackIndex];
    slot.installer = installer;
    slot.effect = std::move(effect);
}

void CUnit::ClearAttackModifier(int attackIndex, War3ID installer) {
    auto it = attackModifiers_.find(attackIndex);
    if (it == attackModifiers_.end()) return;
    if (!(it->second.installer == installer)) return;
    attackModifiers_.erase(it);
}

const CAttackModifierEffect* CUnit::GetAttackModifier(int attackIndex) const {
    auto it = attackModifiers_.find(attackIndex);
    if (it == attackModifiers_.end() || !it->second.effect) return nullptr;
    return it->second.effect.get();
}

CAttackModifierEffect* CUnit::GetAttackModifier(int attackIndex) {
    auto it = attackModifiers_.find(attackIndex);
    if (it == attackModifiers_.end() || !it->second.effect) return nullptr;
    return it->second.effect.get();
}

void CUnit::FireAttackModifiers(CSimulation& game, CUnit* victim, float damageDealt) {
    if (!victim || victim->IsDead()) return;
    CAttackModifierEffect* effect = GetAttackModifier(0);
    if (!effect) return;
    if (!effect->ChargeResources(this)) return;
    effect->OnAttackLanded(game, this, victim, damageDealt);
}

void CUnit::AddBonusMaxLife(float v) {
    bonusMaxLife_ += v;
    RefreshMaxLifeTotal();
}

void CUnit::AddBonusMaxMana(int v) {
    bonusMaxMana_ += v;
    RefreshMaxManaTotal();
}

void CUnit::SetHeroBonusLife(float v) {
    heroBonusLife_ = v;
    RefreshMaxLifeTotal();
}

void CUnit::SetHeroBonusMana(int v) {
    heroBonusMana_ = v;
    RefreshMaxManaTotal();
}

void CUnit::SetPositionAndCheckUnstuck(float x, float y, CSimulation& simulation) {    CPathfindingProcessor* path = simulation.GetPathfinding();
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

    // Check requirements
    if (!ability->IsRequirementsMet(game, this)) {
        ability->SetDisabled(true, CAbilityDisableType::REQUIREMENTS);
    }

    // Check player-level disable
    auto* player = game.GetPlayer(playerIndex_);
    if (player && player->IsAbilityDisabled(ability->GetAlias())) {
        ability->SetDisabled(true, CAbilityDisableType::PLAYER);
    }

    CAbility* rawPtr = ability.get();
    ownedAbilities_.push_back(std::move(ability));
    abilities_.push_back(rawPtr);

    // Retail per-level numbers override the hardcoded constructor
    // defaults when ability data is loaded (ref AbilityData.slk).
    if (auto* spell = dynamic_cast<CAbilitySpellBase*>(rawPtr)) {
        if (CAbilityData* data = game.GetAbilityData()) {
            if (const CAbilityDefFull* def = data->GetAbility(spell->GetAlias())) {
                spell->ApplyData(*def);
            }
        }
    }
    // Cargo capacity from data (ref CAbilityTypeDefinitionCargoHold:
    // DATA_A = capacity). Load range mirrors the ability Range.
    if (CAbilityData* data = game.GetAbilityData()) {
        if (const CAbilityDefFull* def = data->GetAbility(rawPtr->GetAlias())) {
            if (auto* cargo = dynamic_cast<CAbilityCargoHold*>(rawPtr)) {
                const int cap = def->DataIntAt(1, 0, cargo->GetCapacity());
                cargo->SetCapacity(cap);
            }
            if (auto* load = dynamic_cast<CAbilityLoad*>(rawPtr)) {
                const float range = def->RangeAt(1);
                if (range > 0) load->SetLoadRange(range);
                // Mirror the holder's CargoHold capacity so Load gating
                // matches the data-driven hold (default 8 covers ships
                // when data is absent).
                if (auto* hold = GetAbility(War3ID("Acar"))) {
                    if (auto* h = dynamic_cast<CAbilityCargoHold*>(hold)) {
                        load->SetCargoCapacity(h->GetCapacity());
                    }
                }
            }
        }
    }

    // Call OnAdd if not disabled
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

    // Resistant Skin (Arsk): negative effects wear off in a quarter of
    // the usual time. The marker itself is positive and is not shortened.
    if (buff->IsNegative() && HasBuff(War3ID("Brsk"))) {
        buff->ScaleDuration(0.25f);
    }

    // Check if buff already exists (by ID)
    for (auto& existing : buffs_) {
        if (existing && existing->GetId() == buff->GetId()) {
            // Aura buffs merge per caster (ref per-source aura buffs):
            // a different caster's twin is skipped so the scan can still
            // find (and refresh) our own; with no twin found the new
            // source coexists and strongest-wins applies in the table.
            if (existing->MergeByCaster() && buff->MergeByCaster() &&
                existing->GetCasterHandleId() != buff->GetCasterHandleId()) {
                continue;
            }
            // REPLACE, stack, or refresh
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

    // New buff
    buff->OnApply(game, this);
    buffs_.push_back(std::move(buff));
}

void CUnit::RemoveBuff(War3ID buffId, CSimulation& game) {
    for (auto it = buffs_.begin(); it != buffs_.end(); ++it) {
        if (it->get() && it->get()->GetId() == buffId) {
            it->get()->OnRemove(game, this);
            buffs_.erase(it);
            return;
        }
    }
}

void CUnit::RemoveBuffByIdAndCaster(War3ID buffId, int caster, CSimulation& game) {
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
    return GetBuff(buffId) != nullptr;
}

bool CUnit::CheckSpellEffectReaction(CSimulation& game, CUnit* caster) {
    if (!caster || caster == this) return true;
    // Allied casters bypass the shield; missing players count as hostile.
    CPlayer* self = game.GetPlayer(GetPlayerIndex());
    CPlayer* foe = game.GetPlayer(caster->GetPlayerIndex());
    if (self && foe && self->HasAlliance(*foe, CAllianceType::PASSIVE)) {
        return true;
    }
    if (!HasBuff(War3ID("BNss"))) return true;
    RemoveBuff(War3ID("BNss"), game);
    return false;
}

bool CUnit::IsHexed() const {
    for (const auto& buff : buffs_) {
        if (buff && dynamic_cast<const CBuffHex*>(buff.get())) return true;
    }
    return false;
}

void CUnit::SetSharesVisionWith(int player, bool share) {
    if (player < 0) return;
    if (share) sharedVision_.insert(player);
    else sharedVision_.erase(player);
}

bool CUnit::IsStealthed() const {
    if (invisible_) return true;
    for (const auto& buff : buffs_) {
        if (buff && dynamic_cast<const CBuffWindWalk*>(buff.get())) return true;
    }
    return false;
}

bool CUnit::IsInvisibleTo(const CUnit* viewer) const {
    if (!viewer || viewer->GetPlayerIndex() == playerIndex_) return false;
    // Faerie Fire reveals invisible units (ref reveal-invisible half).
    if (GetBuff(War3ID("Bfae"))) return false;
    bool hidden = invisible_;
    if (!hidden) {
        for (const auto& buff : buffs_) {
            if (buff && dynamic_cast<const CBuffWindWalk*>(buff.get())) {
                hidden = true;
                break;
            }
        }
    }
    if (!hidden) return false;
    // Detectors (shades, sentry wards) pierce stealth within their
    // sight range (ref true sight).
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
    auto key = std::make_pair((int)type, percent);
    auto& sources = auraStats_[key];
    for (auto& s : sources) {
        if (s.buffId == buffId && s.caster == caster) {
            s.value = value;
            RecomputeAuraStat(type, percent);
            return;
        }
    }
    sources.push_back({buffId, caster, value});
    RecomputeAuraStat(type, percent);
}

void CUnit::RemoveAuraStatBonus(CAuraEffectType type, bool percent, War3ID buffId, int caster) {
    auto key = std::make_pair((int)type, percent);
    auto it = auraStats_.find(key);
    if (it == auraStats_.end()) return;
    auto& sources = it->second;
    sources.erase(std::remove_if(sources.begin(), sources.end(),
                                 [&](const AuraStatSource& s) {
                                     return s.buffId == buffId && s.caster == caster;
                                 }),
                  sources.end());
    if (sources.empty()) {
        auraStats_.erase(it);
    }
    RecomputeAuraStat(type, percent);
}

void CUnit::RecomputeAllAuraStats() {
    // Snapshot keys first: RecomputeAuraStat mutates auraApplied_ in place.
    std::vector<std::pair<int, bool>> keys;
    keys.reserve(auraStats_.size());
    for (const auto& [key, sources] : auraStats_) {
        (void)sources;
        keys.push_back(key);
    }
    for (const auto& [type, percent] : keys) {
        RecomputeAuraStat(static_cast<CAuraEffectType>(type), percent);
    }
}

void CUnit::SetNonStackingDamageBonus(War3ID key, int flatValue) {
    if (flatValue == 0) {
        nsDamageFlat_.erase(key);
        return;
    }
    nsDamageFlat_[key] = flatValue;
}

void CUnit::SetNonStackingDamagePctBonus(War3ID key, int flatValue) {
    if (flatValue == 0) {
        nsDamagePct_.erase(key);
        return;
    }
    nsDamagePct_[key] = flatValue;
}

void CUnit::ClearNonStackingDamageBonus(War3ID key) {
    nsDamageFlat_.erase(key);
    nsDamagePct_.erase(key);
}

int CUnit::GetNonStackingDamageTotal() const {
    int total = 0;
    for (const auto& [key, value] : nsDamageFlat_) total += value;
    for (const auto& [key, value] : nsDamagePct_) total += value;
    return total;
}

void CUnit::SetNonStackingAttackSpeed(War3ID key, float bonus) {
    if (bonus == 0.0f) {
        nsAtkSpeed_.erase(key);
        return;
    }
    nsAtkSpeed_[key] = bonus;
}

void CUnit::ClearNonStackingAttackSpeed(War3ID key) {
    nsAtkSpeed_.erase(key);
}

float CUnit::GetNonStackingAttackSpeedTotal() const {
    float total = 0.0f;
    for (const auto& [key, value] : nsAtkSpeed_) total += value;
    return total;
}

float CUnit::GetAttackSpeedMultiplier() const {
    // Retail attack math (ref CUnitAttack.computeDerivedFields): all
    // stacking and non-stacking speed bonuses add, then clamp to
    // [-0.9, +4.0] so the multiplier lands in [0.1, 5.0]. The legacy
    // multiplicative chain (Burrow cargo steps) still rides on top.
    const float bonus = attackSpeedStackBonus_ + GetNonStackingAttackSpeedTotal();
    const float clamped = std::max(-0.9f, std::min(4.0f, bonus));
    return (1.0f + clamped) * attackSpeedMul_;
}

void CUnit::AddEvasionSource(War3ID id, float chance) {
    for (auto& src : evasionSources_) {
        if (src.id == id) {
            src.chance = chance;
            return;
        }
    }
    evasionSources_.push_back({id, chance});
}

void CUnit::RemoveEvasionSource(War3ID id) {
    evasionSources_.erase(
        std::remove_if(evasionSources_.begin(), evasionSources_.end(),
                       [&](const EvasionSource& src) { return src.id == id; }),
        evasionSources_.end());
}

float CUnit::GetEvasionChance() const {
    float survive = 1.0f;
    for (const auto& src : evasionSources_) {
        survive *= (1.0f - src.chance);
    }
    return 1.0f - survive;
}

void CUnit::SetEvasionChance(float v) {
    evasionSources_.clear();
    if (v != 0.0f) evasionSources_.push_back({War3ID(0u), v});
}

void CUnit::AddCritSource(War3ID id, float chance, float multiplier) {
    for (auto& src : critSources_) {
        if (src.id == id) {
            src.chance = chance;
            src.multiplier = multiplier;
            return;
        }
    }
    critSources_.push_back({id, chance, multiplier});
}

void CUnit::RemoveCritSource(War3ID id) {
    critSources_.erase(
        std::remove_if(critSources_.begin(), critSources_.end(),
                       [&](const CritSource& src) { return src.id == id; }),
        critSources_.end());
}

float CUnit::GetCritChance() const {
    float best = 0.0f;
    for (const auto& src : critSources_) {
        if (src.chance > best) best = src.chance;
    }
    return best;
}

float CUnit::GetCritMultiplier() const {
    float bestChance = 0.0f;
    float multiplier = 1.0f;
    for (const auto& src : critSources_) {
        if (src.chance > bestChance) {
            bestChance = src.chance;
            multiplier = src.multiplier;
        }
    }
    return multiplier;
}

void CUnit::SetCriticalStrike(float chance, float multiplier) {
    critSources_.clear();
    if (chance != 0.0f || multiplier != 1.0f) critSources_.push_back({War3ID(0u), chance, multiplier});
}

void CUnit::RecomputeAuraStat(CAuraEffectType type, bool percent) {    auto key = std::make_pair((int)type, percent);
    float best = 0.0f;
    bool have = false;
    auto sit = auraStats_.find(key);
    if (sit != auraStats_.end()) {
        for (const auto& s : sit->second) {
            if (!have || s.value > best) {
                best = s.value;
                have = true;
            }
        }
    }
    float applied = 0.0f;
    auto ait = auraApplied_.find(key);
    if (ait != auraApplied_.end()) applied = ait->second;

    // Undo the previous effective contribution, then apply the new
    // strongest one. Math mirrors the legacy direct-mutation aura path
    // (including its truncation behavior) so single-aura results are
    // bit-identical.
    switch (type) {
        case CAuraEffectType::ARMOR: {
            const int oldFlat = (int)std::lround(applied);
            const int newFlat = have ? (int)std::lround(best) : 0;
            if (oldFlat != 0) AddBonusArmor(-oldFlat);
            if (newFlat != 0) AddBonusArmor(newFlat);
            if (newFlat != 0) auraApplied_[key] = (float)newFlat;
            else auraApplied_.erase(key);
            break;
        }
        case CAuraEffectType::MOVE_SPEED: {
            if (percent) {
                const float newFactor = have ? best : 0.0f;
                auraMoveSpeedPercent_ = newFactor;
                if (have) {
                    auraApplied_[key] = best;
                } else {
                    auraApplied_.erase(key);
                }
            } else {
                const int oldFlat = (int)applied;
                const int newFlat = have ? (int)best : 0;
                if (oldFlat != 0) bonusSpeed_ -= oldFlat;
                if (newFlat != 0) bonusSpeed_ += newFlat;
                if (newFlat != 0) auraApplied_[key] = (float)newFlat;
                else auraApplied_.erase(key);
            }
            break;
        }
        case CAuraEffectType::ATTACK_SPEED: {
            // Per-buff best into the shared non-stacking speed table (ref
            // ATKSPD: max within a key, sum across keys). Stale aura-owned
            // keys are cleared so a lapsed aura stops contributing, while
            // timed-buff entries sharing the maps are left alone.
            std::unordered_map<War3ID, float> bestById;
            if (sit != auraStats_.end()) {
                for (const auto& s : sit->second) {
                    auto it = bestById.find(s.buffId);
                    if (it == bestById.end() || s.value > it->second) {
                        bestById[s.buffId] = s.value;
                    }
                }
            }
            for (auto it = auraSpeedKeys_.begin(); it != auraSpeedKeys_.end();) {
                if (bestById.find(*it) == bestById.end()) {
                    ClearNonStackingAttackSpeed(*it);
                    it = auraSpeedKeys_.erase(it);
                } else {
                    ++it;
                }
            }
            for (const auto& [id, value] : bestById) {
                SetNonStackingAttackSpeed(id, value);
                auraSpeedKeys_.insert(id);
            }
            break;
        }
        case CAuraEffectType::DAMAGE: {
            // Same shape as ATTACK_SPEED, split across the flat/pct damage
            // maps (ref MELEEATK vs MELEEATKPCT both applying). The pct slot
            // converts against the primary-attack base exactly like the old
            // direct-mutation path, so single-aura results are bit-identical.
            float base = 0.0f;
            if (percent) {
                if (auto* unitType = GetUnitType()) {
                    const auto& attacks = unitType->GetAttacks();
                    if (!attacks.empty()) {
                        base = (float)attacks[0].damageBase;
                        if (attacks[0].damageDice > 0 && attacks[0].damageSidesPerDie > 0) {
                            base += (float)attacks[0].damageDice *
                                    (float)(attacks[0].damageSidesPerDie + 1) * 0.5f;
                        }
                    }
                }
            }
            std::unordered_map<War3ID, float> bestById;
            if (sit != auraStats_.end()) {
                for (const auto& s : sit->second) {
                    auto it = bestById.find(s.buffId);
                    if (it == bestById.end() || s.value > it->second) {
                        bestById[s.buffId] = s.value;
                    }
                }
            }
            auto& owned = percent ? auraDamagePctKeys_ : auraDamageFlatKeys_;
            for (auto it = owned.begin(); it != owned.end();) {
                if (bestById.find(*it) == bestById.end()) {
                    // Map-specific erase (Clear would also drop the sibling
                    // map's entry for an aura carrying both flat and pct).
                    if (percent) SetNonStackingDamagePctBonus(*it, 0);
                    else SetNonStackingDamageBonus(*it, 0);
                    it = owned.erase(it);
                } else {
                    ++it;
                }
            }
            for (const auto& [id, value] : bestById) {
                const int flat = percent ? (int)std::lround(base * value)
                                         : (int)std::lround(value);
                if (percent) SetNonStackingDamagePctBonus(id, flat);
                else SetNonStackingDamageBonus(id, flat);
                owned.insert(id);
            }
            break;
        }
        case CAuraEffectType::LIFE_REGEN: {
            if (applied != 0.0f) AddBonusLifeRegen(-applied);
            if (have && best != 0.0f) {
                AddBonusLifeRegen(best);
                auraApplied_[key] = best;
            } else {
                auraApplied_.erase(key);
            }
            break;
        }
        case CAuraEffectType::MANA_REGEN: {
            const float oldFlat = applied;
            float newFlat = 0.0f;
            if (have && best != 0.0f) {
                newFlat = percent ? best * static_cast<float>(GetMaxMana()) : best;
            }
            if (oldFlat != 0.0f) AddBonusManaRegen(-oldFlat);
            if (newFlat != 0.0f) {
                AddBonusManaRegen(newFlat);
                auraApplied_[key] = newFlat;
            } else {
                auraApplied_.erase(key);
            }
            break;
        }
        default:
            break;
    }
}

void CUnit::UpdateBuffs(CSimulation& game) {
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
    if (index >= 0 && index < (int)attackRangeBonus_.size()) {
        base += attackRangeBonus_[(size_t)index];
    }
    return base;
}

void CUnit::AddAttackRangeBonus(int index, float v) {
    if (index < 0) return;
    if (index >= (int)attackRangeBonus_.size()) {
        attackRangeBonus_.resize((size_t)index + 1, 0.0f);
    }
    attackRangeBonus_[(size_t)index] += v;
}

void CUnit::AddAttackRangeBonusAll(float v) {
    size_t count = 1;
    if (unitType_) {
        count = std::max<size_t>(1, unitType_->GetAttacks().size());
    } else if (!attackRangeBonus_.empty()) {
        count = attackRangeBonus_.size();
    }
    if (attackRangeBonus_.size() < count) {
        attackRangeBonus_.resize(count, 0.0f);
    }
    for (auto& b : attackRangeBonus_) b += v;
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
    for (auto* loaded : cargoUnits_) {
        if (loaded == passenger) return true; // already aboard
    }
    passenger->loadedAboard_ = true;
    passenger->SetHidden(true);
    cargoUnits_.push_back(passenger);
    if (unitLoadedHook_) {
        unitLoadedHook_(passenger->GetHandleId());
    }
    return true;
}

void CUnit::UnloadUnitAt(CUnit* passenger, float x, float y) {
    if (!passenger) return;
    auto it = std::find(cargoUnits_.begin(), cargoUnits_.end(), passenger);
    if (it == cargoUnits_.end()) return;
    cargoUnits_.erase(it);
    passenger->loadedAboard_ = false;
    passenger->SetHidden(false);
    passenger->SetPosition(x, y);
}

bool CUnit::CanBeTargetedBy(CSimulation& simulation, CUnit* source,
                            const std::vector<CTargetType>& targetsAllowed) const {
    if (loadedAboard_) return false; // hidden cargo can't be targeted
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
    corpse_ = false;
    boneCorpse_ = false;
    deathTurnTick_ = 0;
    hidden_ = false;
    SetPosition(x, y);
    RefreshMaxLifeTotal();
    RefreshMaxManaTotal();
    SetLife(game, GetMaxLife());
    // Buildings never revive via heroes, but a restored structure needs
    // its footprint back (ref pathingInstance lifecycle).
    if (IsBuilding() && pathingStampId_ < 0) RegeneratePathingInstance(game);
    // Mana intentionally preserved (reincarnation spends it pre-revive).
}

void CUnit::ApplyRegen(CSimulation& simulation) {
    if (IsDead() || IsBuilding()) return;

    float dt = simulation.GetSimStepTime();
    bool isNight = simulation.IsNighttime();
    CRegenType regenType = unitType_->GetLifeRegenType();

    // HP regen
    bool canRegen = (regenType == CRegenType::ALWAYS) ||
                    (regenType == CRegenType::ONLY_AT_NIGHT && isNight);
    if (canRegen && life_ < maxLife_) {
        life_ += (unitType_->GetLifeRegen() + bonusLifeRegen_) * dt;
        if (life_ > maxLife_) life_ = maxLife_;
    }

    // Mana regen
    if (mana_ < (float)maxMana_) {
        mana_ += (unitType_->GetManaRegen() + bonusManaRegen_) * dt;
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
        if (deathTurnTick_ == 0) {
            deathTurnTick_ = currentTick;
            corpse_ = false;
            boneCorpse_ = false;
            // Retail death clears the building footprint (ref killPathingInstance).
            KillPathingInstance(simulation);
            // The fallen drop their orders with their behavior (ref kill:
            // beginBehavior(null) + orderQueue.clear()).
            if (currentBehavior_) {
                currentBehavior_->End(simulation);
                currentBehavior_ = nullptr;
                ownedCurrentBehavior_.reset();
            }
            while (!orderQueue_.empty()) orderQueue_.pop();
            // Hidden construction workers emerge from the ruin (ref
            // popoutWorker; no-op without a worker link).
            PopoutWorker(simulation);
            // Ref death path: the fallen stop satisfying techtree
            // requirements (heroes keep theirs while awaiting revive).
            if (!IsHero()) {
                if (CPlayer* player = simulation.GetPlayer(GetPlayerIndex())) {
                    player->RemoveTechUnlocked(simulation, GetTypeId());
                }
            }
            // Death reveal (ref CUnitDeathVisionFogModifier for owner +
            // killer): a short timed VISIBLE circle on the fog grid.
            {
                const auto& gc = simulation.GetGameplayConstants();
                if (gc.dyingRevealRadius > 0.0f && gc.fogFlashTime > 0.0f) {
                    if (CFogSystem* fog = simulation.GetFogSystem()) {
                        const CFogArea area =
                            CFogArea::Circle(GetX(), GetY(), gc.dyingRevealRadius);
                        fog->AddTimedModifier(GetPlayerIndex(), area,
                                              CFogState::VISIBLE, gc.fogFlashTime);
                        if (deathKillerPlayer_ >= 0 &&
                            deathKillerPlayer_ != GetPlayerIndex()) {
                            fog->AddTimedModifier(deathKillerPlayer_, area,
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
            // Food refund at death, not at removal (ref kill zeroes both:
            // player.setUnitFoodMade/Used(this, 0)).
            if (CPlayer* player = simulation.GetPlayer(GetPlayerIndex())) {
                if (foodMade_ != 0) {
                    if (!IsConstructing()) {
                        player->SetFoodCap(std::max(0, player->GetFoodCap() - foodMade_));
                    }
                    foodMade_ = 0;
                }
                const int used = GetFoodUsed();
                if (used != 0) {
                    player->SetFoodUsed(std::max(0, player->GetFoodUsed() - used));
                    foodUsed_ = 0;
                }
            }
            // Death drops (units.doo dropSets + w3i droppedItemTable): won
            // items materialize at the corpse on the death tick.
            DropDeathItems(simulation);
            // Cargo goes down with the transport (WC3 rule).
            auto passengers = std::move(cargoUnits_);
            cargoUnits_.clear();
            const float dropX = GetX();
            const float dropY = GetY();
            for (auto* passenger : passengers) {
                if (passenger) {
                    passenger->loadedAboard_ = false;
                    passenger->SetHidden(false);
                    passenger->SetPosition(dropX, dropY);
                    if (!passenger->IsDead()) {
                        passenger->Damage(simulation, nullptr, false, false,
                                          CAttackType::CHAOS, CDamageType::NORMAL,
                                          "", 1e9f, 0);
                    }
                }
            }
            // Notify abilities of death
            for (auto* ab : abilities_) {
                if (ab) ab->OnDeath(simulation, this);
            }
            // Notify buffs of death (ref ability death listeners; e.g.
            // Doom raises its guard when the bearer dies, not on expiry).
            for (const auto& buff : buffs_) {
                if (buff) buff->OnDeath(simulation, this);
            }
            return false;
        }

        if (decaySuspended_) return false;

        if (!corpse_) {
            const int deathTicks = std::max(1, (int)std::ceil(unitType_->GetDeathTime() / simulation.GetSimStepTime()));
            if ((currentTick - deathTurnTick_) > deathTicks) {
                corpse_ = true;
                if (!unitType_->CanRaise()) {
                    boneCorpse_ = true;
                }
                if (!IsHero() && !unitType_->CanDecay()) {
                    return true;
                }
                deathTurnTick_ = currentTick;
                // Decay begins (retail EVENT_UNIT_DECAY); one-shot by
                // construction (corpse_ flips, this branch never reruns).
                if (auto& decayCb = simulation.GetUnitDecayCallback()) {
                    decayCb(handleId_);
                }
            }
            return false;
        }

        if (!boneCorpse_) {
            const int decayTicks = std::max(1, (int)std::ceil(
                simulation.GetGameplayConstants().decayTime / simulation.GetSimStepTime()));
            if ((currentTick - deathTurnTick_) > decayTicks) {
                boneCorpse_ = true;
                deathTurnTick_ = currentTick;
            }
            return false;
        }

        const int endingDecayTicks = std::max(1, (int)std::ceil(
            GetEndingDecayTime(simulation) / simulation.GetSimStepTime()));
        if ((currentTick - deathTurnTick_) > endingDecayTicks) {
            if (IsHero()) {
                hidden_ = true;
                return false;
            }
            return true;
        }
        return false;
    }

    if (paused_) return false;

    // Apply health/mana regen
    ApplyRegen(simulation);

    // ESC-cancel interrupts an unfinished construction immediately (queued
    // orders otherwise wait for the active behavior to finish; ref
    // cancel-build refunds, removes the foundation, fires the event).
    if (!orderQueue_.empty() && currentBehavior_) {
        const int frontId =
            orderQueue_.front() ? orderQueue_.front()->GetOrderId() : -1;
        if (frontId == OrderIds::CANCEL || frontId == OrderIds::CANCEL_BUILD) {
            if (auto* build = dynamic_cast<CBehaviorBuild*>(currentBehavior_)) {
                if (build->IsActivelyBuilding() || build->IsMovingToSite()) {
                    // NOTE: no order callback here; it fired when the CANCEL
                    // order was issued (AdoptUnit hook), and the queue has a
                    // single push site (IssueOrder).
                    orderQueue_.pop();
                    build->CancelConstruction(simulation);
                }
            }
        }
    }

    // Process behavior. New orders preempt an interruptable behavior
    // immediately (ref order()); while stunned they collapse to the
    // latest single order and run when the stun expires.
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
                // Try to get next behavior from order queue
                PollNextOrderBehavior(simulation);
            }
        }
    } else if (!orderQueue_.empty()) {
        PollNextOrderBehavior(simulation);
    } else {
        PerformDefaultBehavior(simulation);
    }

    // Cargo rides along with the transport.
    for (auto* passenger : cargoUnits_) {
        if (passenger) passenger->SetPosition(x_, y_);
    }

    // Tick all abilities
    for (auto* ab : abilities_) {
        if (ab && !ab->IsDisabled()) {
            ab->OnTick(simulation, this);
        }
    }

    // Update buffs (tick, expire, remove)
    UpdateBuffs(simulation);

    return false;
}

void CUnit::PerformDefaultBehavior(CSimulation& simulation) {
    // Idle autocast first (ref autoAcquireAutocastTargets): wounded-ally
    // heals and nearest-enemy debuffs fire while standing idle.
    if (TryAutocast(simulation)) return;
    // Use default behavior (usually stop/idle)
    if (defaultBehavior_) {
        ownedCurrentBehavior_.reset();
        currentBehavior_ = defaultBehavior_.get();
        currentBehavior_->SetUnit(this);
        currentBehavior_->Begin(simulation);
    }
}

// Autocast target acquisition (ref CUnit.autoAcquireAutocastTargets +
// AutocastTargetFinderEnum). Only idle units with an enabled autocast
// ability, mana, and cooldown ready participate.
bool CUnit::TryAutocast(CSimulation& simulation) {
    if (IsDead() || IsBuilding() || IsPaused()) return false;
    if (!orderQueue_.empty() || currentBehavior_) return false;
    if (IsSilenced() || IsHexed() || IsAsleep()) return false;

    CPlayer* self = simulation.GetPlayer(GetPlayerIndex());
    const War3ID defaultAuto =
        unitType_ ? unitType_->GetDefaultAutocastAbility() : War3ID(0u);

    for (auto* ab : abilities_) {
        auto* spell = dynamic_cast<CAbilitySpellBase*>(ab);
        if (!spell || spell->IsDisabled() || spell->IsOnCooldown()) continue;
        CAutocastKind kind = GetAutocastKind(spell->GetAlias());
        if (kind == CAutocastKind::NONE) kind = GetAutocastKind(spell->GetCode());
        if (kind == CAutocastKind::NONE) continue;
        if (!spell->IsAutocastActive() && defaultAuto != spell->GetAlias() &&
            defaultAuto != spell->GetCode()) continue;
        if ((int)GetMana() < spell->GetManaCost()) continue;
        const int orderId = spell->GetBaseOrderId();
        if (orderId <= 0) continue;
        if (!spell->CheckCanUse(simulation, this, orderId)) continue;

        const float range = spell->GetCastRange() + 400.0f;
        const float rangeSq = range * range;
        CUnit* best = nullptr;
        if (kind == CAutocastKind::LOWEST_HP_ALLY) {
            float bestFrac = 1.0f;
            for (CUnit* other : simulation.GetUnits()) {
                if (!other || other->IsDead() || other->IsHidden() ||
                    other->IsLoadedAboard() || other->IsBuilding()) continue;
                CPlayer* foe = simulation.GetPlayer(other->GetPlayerIndex());
                const bool ally = (other == this) ||
                    (self && foe && self->HasAlliance(*foe, CAllianceType::PASSIVE));
                if (!ally) continue;
                if (other->GetLife() >= other->GetMaxLife()) continue;
                const float dx = other->GetX() - GetX();
                const float dy = other->GetY() - GetY();
                if (dx * dx + dy * dy > rangeSq) continue;
                if (!spell->CheckCanTarget(simulation, this, orderId, other)) continue;
                const float frac = other->GetLife() / other->GetMaxLife();
                if (!best || frac < bestFrac) {
                    best = other;
                    bestFrac = frac;
                }
            }
        } else {
            float bestDist = rangeSq;
            for (CUnit* other : simulation.GetUnits()) {
                if (!other || other->IsDead() || other->IsHidden() ||
                    other->IsLoadedAboard() || other == this) continue;
                CPlayer* foe = simulation.GetPlayer(other->GetPlayerIndex());
                if (self && foe && self->HasAlliance(*foe, CAllianceType::PASSIVE)) continue;
                const float dx = other->GetX() - GetX();
                const float dy = other->GetY() - GetY();
                const float d2 = dx * dx + dy * dy;
                if (d2 > rangeSq || (best && d2 >= bestDist)) continue;
                if (!spell->CheckCanTarget(simulation, this, orderId, other)) continue;
                best = other;
                bestDist = d2;
            }
        }
        if (best) {
            IssueOrder(std::make_unique<COrderTargetWidget>(orderId, best));
            PollNextOrderBehavior(simulation);
            return currentBehavior_ != nullptr;
        }
    }
    return false;
}

float CUnit::Damage(CSimulation& simulation, CUnit* source,
                    bool isAttack, bool isRanged,
                    CAttackType attackType, CDamageType damageType,
                    const std::string& weaponSoundType,
                    float damage, float bonusDamage) {
    // Incoming base for damage-taken listeners (ref thorns scales the raw
    // base damage): snapshotted before mana shield / weaken mods below
    // mutate the `damage` parameter.
    const float incomingBaseDamage = damage;
    const float incomingBaseBonus = bonusDamage;
    // Drunken Haze (Brewmaster): the swing may miss outright and land
    // nothing at all (ref miss chance). Checked before everything,
    // including invulnerability and callbacks.
    if (isAttack && source && source != this) {
        if (auto* haze = dynamic_cast<CBuffHaze*>(source->GetBuff(War3ID("BNdh")))) {
            if (simulation.SimRand01() < haze->GetMissChance()) return 0;
        }
        if (auto* curse = dynamic_cast<CBuffCurse*>(source->GetBuff(War3ID("Bcrs")))) {
            if (simulation.SimRand01() < curse->GetMissChance()) return 0;
        }
    }
    // Uphill accuracy (ref CUnitDefaultAccuracyCheckListener): firing up
    // a cliff layer misses with ChanceToMiss. Same early gate as the
    // buff miss rolls above (no RNG consumed on flat ground).
    if (simulation.CheckHighlandMiss(source, this, isAttack)) {
        simulation.FireCombatText(source ? source->GetHandleId() : -1, handleId_,
                                  CSimulation::CombatTextKind::MISS, 0.0f);
        return 0;
    }

    // Retail invulnerability suppresses only the taken-mods, the life
    // change and the kill (ref CUnit.damage guard); taken-listeners, the
    // damage event and return-fire below still run. CalculateDamage rolls
    // evasion/crit regardless and reports 0 dealt for invulnerable targets.
    const bool invulnerableTarget = invulnerable_;

    // Mana Shield (Sea Witch): incoming damage drains mana first.
    // A taken-mod: suppressed while invulnerable (no mana should burn).
    if (!invulnerableTarget && damage > 0 && GetMana() > 0 && GetBuff(War3ID("BNms"))) {
        float absorbed = damage;
        if (absorbed > GetMana()) absorbed = GetMana();
        SetMana(GetMana() - absorbed);
        damage -= absorbed;
        if (damage <= 0) return 0;
    }

    // Howl of Terror (Pit Lord): the attacker's blows land weakened.
    if (isAttack && source && source != this) {
        if (auto* howl = dynamic_cast<CBuffHowl*>(source->GetBuff(War3ID("BNht")))) {
            damage *= howl->GetDamageFactor();
        }
        // Item reaction AIdd: addDamageDealtMultiplier on the attacker's
        // pre-damage listener, before armor.
        float dealt = 1.0f;
        for (const auto& buff : source->GetBuffs()) {
            if (auto* mod = dynamic_cast<CBuffDamageMod*>(buff.get())) {
                dealt += mod->GetAttackDealtAdd();
            }
        }
        if (dealt < 0.0f) dealt = 0.0f;
        damage *= dealt;
    }

    // Avatar (Mountain King): spell immunity while the god-rage holds.
    // A taken-mod gate, so it only short-circuits live targets; an
    // invulnerable target falls through to the listener path below.
    if (!invulnerableTarget && attackType == CAttackType::SPELLS && GetBuff(War3ID("BAvt"))) return 0;

    // Single shared formula (ref CUnit.damage): attack-type table always
    // applies; armor value only matters for Normal damage type.
    DamageResult result = simulation.GetCombatSystem()->CalculateDamage(
        source, this, isAttack, isRanged, attackType, damageType,
        damage, bonusDamage, 0, 0);
    float actualDamage = result.damageDealt;
    // Floating combat text (ref spawnTextTag): evasion shows "miss",
    // crits show the final number. Fired here for the direct-damage path
    // (ApplyDamage covers the splash/JASS path).
    if (result.isEvaded) {
        simulation.FireCombatText(source ? source->GetHandleId() : -1, handleId_,
                                  CSimulation::CombatTextKind::MISS, 0.0f);
    } else if (result.isCritical) {
        simulation.FireCombatText(source ? source->GetHandleId() : -1, handleId_,
                                  CSimulation::CombatTextKind::CRIT, actualDamage);
    }

    // Defend (Footman/Knight): timed-buff stand-in for the defend toggle;
    // halves weapon-attack damage before it lands (ref Defend reduction).
    // Berserk runs the same gate with the opposite sign (ref Beserk).
    if (isAttack && actualDamage > 0) {
        for (const auto& buff : buffs_) {
            if (dynamic_cast<CBuffDefend*>(buff.get())) {
                actualDamage *= 0.5f;
                break;
            }
        }
        for (const auto& buff : buffs_) {
            if (auto* berserk = dynamic_cast<CBuffBerserk*>(buff.get())) {
                actualDamage *= berserk->GetDamageTakenMultiplier();
                break;
            }
        }
    }

    // Runed Bracers / item reaction: per-attack-type damage taken.
    if (actualDamage > 0) {
        for (const auto& buff : buffs_) {
            auto* mod = dynamic_cast<CBuffDamageMod*>(buff.get());
            if (!mod) continue;
            float mul = 1.0f;
            if (attackType == CAttackType::SPELLS) mul = mod->GetSpellsTaken();
            else if (attackType == CAttackType::MAGIC) mul = mod->GetMagicTaken();
            else if (attackType == CAttackType::PIERCE) mul = mod->GetPierceTaken();
            actualDamage *= mul;
        }
    }

    float oldLife = life_;
    if (!invulnerableTarget) {
        life_ -= actualDamage;
        if (life_ < 0) life_ = 0;
        // Retail heals on negative damage, capped at max life (unless the
        // unit was already above max, ref CUnit.damage).
        if (actualDamage < 0 && oldLife <= GetMaxLife() && life_ > GetMaxLife()) {
            life_ = GetMaxLife();
        }
    }

    // Sleep breaks on damage (ref sleep listener runs on every damage call,
    // even vs invulnerable targets). Missed/zero swings stay asleep.
    if ((invulnerableTarget || actualDamage != 0) && IsAsleep()) {
        RemoveBuff(War3ID("BUsl"), simulation);
    }

    // Thorns aura: reflect a share of melee NORMAL-damage attacks back to
    // the attacker (ref CUnitDefaultThornsListener: NORMAL type, melee,
    // victim targetable-as-enemy, ResistsThorns gate). Scales the incoming
    // BASE (pre-armor, pre-crit), not the sustained actual; the strongest
    // thorns buff wins when several apply, and Spiked Carapace reflects on
    // top as its own passive. Reflected hits are sourceless non-attacks of
    // DEFENSIVE type, so they never re-reflect (no ping-pong) and never
    // re-proc attacker listeners.
    // Any CBuffThorns counts, whatever its id (Bthr item buff, BEah aura).
    // Runs even vs invulnerable targets (ref damageTakenListeners outside
    // the invulnerability guard), but never on an evaded swing (retail
    // zeroes the base first) and never on zero/negative actuals beyond the
    // reflected>0 guard below.
    if (isAttack && !isRanged && source && source != this &&
        damageType == CDamageType::NORMAL &&
        (invulnerableTarget ? !result.isEvaded : actualDamage != 0)) {
        // Enemy gate (ref ENEMY_TARGET check): no reflection for allies.
        // Mapless tests (no players) stay permissive.
        bool enemy = true;
        if (CPlayer* self = simulation.GetPlayer(GetPlayerIndex())) {
            if (CPlayer* foe = simulation.GetPlayer(source->GetPlayerIndex())) {
                enemy = !self->HasAlliance(*foe, CAllianceType::ALLIED);
            }
        }
        const bool resisted = simulation.GetGameplayConstants().magicImmuneResistsThorns &&
                              source->IsMagicImmune();
        if (enemy && !resisted) {
            float bestPercent = 0.0f;
            for (const auto& buff : buffs_) {
                if (auto* thorns = dynamic_cast<CBuffThorns*>(buff.get())) {
                    bestPercent = std::max(bestPercent, thorns->GetReflectPercent());
                }
            }
            if (bestPercent > 0.0f) {
                const float reflected = (incomingBaseDamage + incomingBaseBonus) * bestPercent;
                if (reflected > 0) {
                    source->Damage(simulation, nullptr, false, false,
                                   CAttackType::SPELLS, CDamageType::DEFENSIVE,
                                   "", reflected, 0);
                }
            }
            // Spiked Carapace (Crypt Lord passive): same melee-only reflect
            // terms as thorns; the armor half rides the unit bonus field.
            if (auto* carapace = dynamic_cast<CAbilitySpikedCarapace*>(GetAbility(War3ID("AUts")))) {
                const float reflected = (incomingBaseDamage + incomingBaseBonus) *
                                        carapace->GetReflectPercent();
                if (reflected > 0) {
                    source->Damage(simulation, nullptr, false, false,
                                   CAttackType::SPELLS, CDamageType::DEFENSIVE,
                                   "", reflected, 0);
                }
            }
        }
    }

    // Vampiric aura: heal the attacker for a share of attack damage DEALT
    // (post-mitigation actual, ref lifesteal listener). No leech from
    // buildings or mechanical units, none across a PASSIVE alliance, none
    // from magic-immune victims while ResistsLeech is set. Strongest leech
    // wins when several apply.
    if (isAttack && source && source != this && actualDamage > 0 && !IsBuilding()) {
        bool mechanical = false;
        if (unitType_) {
            const auto& classes = unitType_->GetClassifications();
            mechanical = classes.find(CUnitClassification::MECHANICAL) != classes.end();
        }
        // PASSIVE-alliance gate (ref): mapless tests stay permissive.
        bool passiveAlly = false;
        if (CPlayer* atk = simulation.GetPlayer(source->GetPlayerIndex())) {
            if (CPlayer* vic = simulation.GetPlayer(GetPlayerIndex())) {
                passiveAlly = atk->HasAlliance(*vic, CAllianceType::PASSIVE);
            }
        }
        const bool resisted = simulation.GetGameplayConstants().magicImmuneResistsLeech &&
                              IsMagicImmune();
        if (!mechanical && !passiveAlly && !resisted) {
            float bestPercent = 0.0f;
            for (const auto& buff : source->GetBuffs()) {
                if (auto* leech = dynamic_cast<CBuffLifesteal*>(buff.get())) {
                    bestPercent = std::max(bestPercent, leech->GetStealPercent());
                }
            }
            if (bestPercent > 0.0f) {
                float healed = source->GetLife() + actualDamage * bestPercent;
                if (healed > source->GetMaxLife()) healed = source->GetMaxLife();
                source->SetLife(simulation, healed);
            }
        }
    }

    // Bash (Mountain King passive): chance to stun on attack landed.
    // Lives here next to the thorns/lifesteal listeners (ref attack
    // listeners); deterministic roll via the sim RNG.
    if (isAttack && source && source != this && actualDamage > 0 && !IsDead()) {
        if (auto* bash = dynamic_cast<CAbilityBash*>(source->GetAbility(War3ID("AHbh")))) {
            if (simulation.SimRand01() < bash->GetChance()) {
                AddBuff(std::make_unique<CBuffStun>(bash->GetDuration()), simulation);
                simulation.FireCombatText(source->GetHandleId(), handleId_,
                                          CSimulation::CombatTextKind::BASH, 0.0f);
            }
        }
    }

    // Attack modifier slot (ref attackReplacement): orb/arrow procs are
    // data-driven effects installed by their abilities, not hardcoded
    // branches. Exactly one family fires per attack (installing a second
    // modifier replaces the first); procs deal non-attack damage so they
    // never re-proc.
    if (isAttack && source && source != this && actualDamage > 0 && !IsDead()) {
        source->FireAttackModifiers(simulation, this, actualDamage);
    }
    // Cleaving Attack (Pit Lord): share of the landed damage splashes
    // to the victim's neighbors (ref cleave). Non-attack damage so the
    // splash itself never re-procs.
    if (isAttack && source && source != this && actualDamage > 0 && !IsDead()) {
        if (auto* cleave = dynamic_cast<CAbilityCleavingAttack*>(source->GetAbility(War3ID("ANca")))) {
            const float splash = actualDamage * cleave->GetSplashFraction();
            const float radius = cleave->GetSplashRadius();
            if (splash > 0) {
                CPlayer* srcPlayer = simulation.GetPlayer(source->GetPlayerIndex());
                for (auto* unit : simulation.GetUnits()) {
                    if (!unit || unit->IsDead() || unit == this || unit == source) continue;
                    CPlayer* tgtPlayer = simulation.GetPlayer(unit->GetPlayerIndex());
                    const bool isAlly = (unit->GetPlayerIndex() == source->GetPlayerIndex()) ||
                        (srcPlayer && tgtPlayer && (srcPlayer->HasAlliance(*tgtPlayer, CAllianceType::ALLIED) ||
                                                    srcPlayer->HasAlliance(*tgtPlayer, CAllianceType::PASSIVE)));
                    if (isAlly) continue;
                    const float dx = unit->GetX() - GetX();
                    const float dy = unit->GetY() - GetY();
                    if (dx * dx + dy * dy <= radius * radius) {
                        unit->Damage(simulation, source, false, false,
                                     CAttackType::SPELLS, CDamageType::NORMAL,
                                     "", splash, 0);
                    }
                }
            }
        }
    }
    // Wind Walk break (ref): landing an attack reveals the attacker;
    // the strike carries a backstab bonus while the victim stands.
    if (isAttack && source && source != this && actualDamage > 0) {
        if (auto* walk = dynamic_cast<CBuffWindWalk*>(source->GetBuff(War3ID("BOwk")))) {
            const float backstab = walk->GetBackstabDamage();
            source->RemoveBuff(War3ID("BOwk"), simulation);
            if (backstab > 0 && !IsDead()) {
                Damage(simulation, source, false, false,
                       CAttackType::SPELLS, CDamageType::NORMAL,
                       "", backstab, 0);
            }
        }
    }
    // Pulverize (Mountain Giant): chance for the landed damage to
    // slam the victim's neighbors too (ref). Non-attack, no re-proc.
    if (isAttack && source && source != this && actualDamage > 0 && !IsDead()) {
        if (auto* pulv = dynamic_cast<CAbilityPulverize*>(source->GetAbility(War3ID("Awar")))) {
            if (simulation.SimRand01() < pulv->GetChance()) {
                const float radius = pulv->GetRadius();
                CPlayer* srcPlayer = simulation.GetPlayer(source->GetPlayerIndex());
                for (auto* unit : simulation.GetUnits()) {
                    if (!unit || unit->IsDead() || unit == this || unit == source) continue;
                    CPlayer* tgtPlayer = simulation.GetPlayer(unit->GetPlayerIndex());
                    const bool isAlly = (unit->GetPlayerIndex() == source->GetPlayerIndex()) ||
                        (srcPlayer && tgtPlayer && (srcPlayer->HasAlliance(*tgtPlayer, CAllianceType::ALLIED) ||
                                                    srcPlayer->HasAlliance(*tgtPlayer, CAllianceType::PASSIVE)));
                    if (isAlly) continue;
                    const float dx = unit->GetX() - GetX();
                    const float dy = unit->GetY() - GetY();
                    if (dx * dx + dy * dy <= radius * radius) {
                        unit->Damage(simulation, source, false, false,
                                     CAttackType::SPELLS, CDamageType::NORMAL,
                                     "", actualDamage, 0);
                    }
                }
            }
        }
    }

    auto& dmgCb = simulation.GetUnitDamageCallback();
    if (dmgCb) {
        int srcHandle = source ? source->GetHandleId() : -1;
        dmgCb(handleId_, srcHandle, actualDamage);
    }

    (void)weaponSoundType;

    if (oldLife > 0 && life_ <= 0) {
        // Killer side-channel for the death tick (ref kill(source): killer
        // gets its own death-vision reveal next to the owner's).
        deathKillerPlayer_ = source ? source->GetPlayerIndex() : -1;
        AwardBountyAndXP(simulation, source);
        auto& deathCb = simulation.GetUnitDeathCallback();
        int srcHandle = source ? source->GetHandleId() : -1;
        if (deathCb) deathCb(handleId_, srcHandle, playerIndex_);
    } else if (life_ > 0) {
        MaybeReturnFire(simulation, source);
    }

    return oldLife - life_;
}

// Kill rewards (ref CUnit.killUnit bounty/XP block): creep bounties pay
// the killer's player, and nearby allied heroes split the kill's XP.
void CUnit::AwardBountyAndXP(CSimulation& simulation, CUnit* source) {
    if (!source || !unitType_) return;
    CPlayer* victimOwner = simulation.GetPlayer(GetPlayerIndex());
    CPlayer* sourcePlayer = simulation.GetPlayer(source->GetPlayerIndex());
    if (!sourcePlayer) return;
    if (victimOwner && sourcePlayer->HasAlliance(*victimOwner, CAllianceType::PASSIVE)) return;

    // Gold/lumber bounty dice (ref base + dice d sides, seeded sim RNG).
    if (victimOwner && victimOwner->GetGivesBounty()) {
        int gold = unitType_->GetGoldBountyAwardedBase();
        const int goldDice = unitType_->GetGoldBountyAwardedDice();
        const int goldSides = unitType_->GetGoldBountyAwardedSides();
        for (int i = 0; i < goldDice; ++i) {
            gold += goldSides <= 0 ? 0 : (int)(simulation.SimRand01() * goldSides) + 1;
        }
        if (gold > 0) sourcePlayer->AddGold(gold);
        int lumber = unitType_->GetLumberBountyAwardedBase();
        const int lumberDice = unitType_->GetLumberBountyAwardedDice();
        const int lumberSides = unitType_->GetLumberBountyAwardedSides();
        for (int i = 0; i < lumberDice; ++i) {
            lumber += lumberSides <= 0 ? 0 : (int)(simulation.SimRand01() * lumberSides) + 1;
        }
        if (lumber > 0) sourcePlayer->AddLumber(lumber);
    }

    // Hero XP split (ref grant tables + heroExpRange + global fallback).
    const auto& gc = simulation.GetGameplayConstants();
    if (!gc.buildingKillsGiveExp && source->IsBuilding()) return;
    const int availableXp = IsHero()
        ? gc.GetGrantHeroXP(GetHeroLevel() > 0 ? GetHeroLevel() : 1)
        : gc.GetGrantNormalXP(unitType_->GetLevel());
    if (availableXp <= 0) return;

    std::vector<CUnit*> receivers;
    const float range = (float)gc.heroExpRange;
    const float rangeSq = range * range;
    for (CUnit* other : simulation.GetUnits()) {
        if (!other || other->IsDead() || !other->IsHero()) continue;
        CPlayer* ally = simulation.GetPlayer(other->GetPlayerIndex());
        if (!ally || !sourcePlayer->HasAlliance(*ally, CAllianceType::SHARED_XP)) continue;
        const float dx = other->GetX() - GetX();
        const float dy = other->GetY() - GetY();
        if (dx * dx + dy * dy > rangeSq) continue;
        receivers.push_back(other);
    }
    if (receivers.empty() && gc.globalExperience) {
        for (int pi = 0; pi < simulation.GetPlayerCount(); ++pi) {
            CPlayer* ally = simulation.GetPlayer(pi);
            if (!ally || !sourcePlayer->HasAlliance(*ally, CAllianceType::SHARED_XP)) continue;
            for (CUnit* hero : ally->GetHeroes()) {
                if (hero && !hero->IsDead()) receivers.push_back(hero);
            }
        }
    }
    if (receivers.empty()) return;
    for (CUnit* hero : receivers) {
        auto* heroAbility = dynamic_cast<CAbilityHero*>(hero->GetAbility(War3ID("AHer")));
        if (!heroAbility) continue;
        const int share = (int)(availableXp * (1.0f / (float)receivers.size()) *
                                gc.GetHeroFactorXp(hero->GetHeroLevel()));
        if (share > 0) heroAbility->AddXP(simulation, hero, share);
    }
}

void CUnit::MaybeReturnFire(CSimulation& simulation, CUnit* source) {
    if (!source || source == this || !unitType_) return;
    // Idle or interruptable-default units only (ref damage() else branch:
    // null, or default behavior that is itself interruptable).
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

    // Return fire: first attack that can target the attacker.
    if (hostile && !isPeon && !unitType_->GetAttacks().empty()) {
        if (source->CanBeTargetedBy(simulation, this, {}) && !source->IsDead()) {
            if (CBehaviorAttack* attack = GetAttackBehavior()) {
                attack->SetTarget(source);
                adoptBehavior(attack);
                return;
            }
        }
    }

    // Flee: workers run one speed-step away from the attacker.
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

    // If no ability handled a SMART order targeting a widget, fall back to moving to that widget's position
    if (orderId == OrderIds::SMART) {
        if (auto* widgetOrder = dynamic_cast<COrderTargetWidget*>(order.get())) {
            auto* tgt = widgetOrder->GetTarget();
            if (tgt) {
                ConvertOrderToBehavior(simulation, std::make_unique<COrderTargetPoint>(OrderIds::SMART, tgt->GetX(), tgt->GetY()));
                return;
            }
        }
    }

    // If explicit stop / hold position or preemptive targeted/smart order was unhandled:
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
