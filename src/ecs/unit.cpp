#include "unit.h"
#include "csimulation.h"
#include "ecs/buffs.h"
#include "ecs/abilities/ability_spell_base.h"
#include "data/abilitydata.h"
#include "ecs/abilities/spells4.h"
#include "ecs/abilities/spells5.h"
#include "ecs/abilities/spells9.h"
#include "ecs/abilities/spells10.h"
#include "ecs/abilities/spells14.h"
#include "ecs/abilities/spells16.h"
#include "behaviors/behavior_move.h"
#include "behaviors/behavior_attack.h"
#include "behaviors/behavior_stop.h"
#include "behaviors/behavior_hold_position.h"
#include "behaviors/behavior_follow.h"
#include "behaviors/behavior_patrol.h"
#include "behaviors/behavior_attack_move.h"
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

void CUnit::SetPosition(float x, float y) {
    x_ = x;
    y_ = y;
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
        savedSpeed_ = speed_;
        savedBaseMaxLife_ = baseMaxLife_;
        savedBaseMaxMana_ = baseMaxMana_;
        savedDefense_ = currentDefense_;
        savedTurnRate_ = turnRate_;
        savedMovementType_ = movementType_;
        savedCollisionSize_ = collisionSize_;
        savedBuilding_ = isBuilding_;
        savedFlyHeight_ = GetFlyHeight();
    }
    // Attacks read unitType_ live, so the form's arsenal (and model,
    // render-side) applies immediately. Hero/ability state is untouched.
    unitType_ = form;
    typeId_ = formId;
    speed_ = form->GetSpeed();
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
}

void CUnit::Unmorph() {
    if (!morphed_) return;
    morphed_ = false;
    unitType_ = originalType_;
    typeId_ = originalTypeId_;
    speed_ = savedSpeed_;
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

void CUnit::SetPositionAndCheckUnstuck(float x, float y, CSimulation& simulation) {
    x_ = x;
    y_ = y;
    (void)simulation;
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

    float propulsionWindow = 1.0f;
    float turnStepDegrees = 180.0f;
    if (unitType_) {
        propulsionWindow = std::max(1.0f, GetEffectivePropWindow());
    } else if (propWindow_ >= 0.0f) {
        propulsionWindow = std::max(1.0f, propWindow_);
    }
    turnStepDegrees = std::max(0.1f, turnRate_ * 180.0f / kPi);

    if (std::fabs(delta) <= propulsionWindow) {
        facing_ = goalFacing;
        return true;
    }

    const float appliedStep = std::min(std::fabs(delta), turnStepDegrees) * (delta < 0.0f ? -1.0f : 1.0f);
    facing_ = NormalizeFacingDegrees(currentFacing + appliedStep);
    return false;
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

    // Check if buff already exists (by ID)
    for (auto& existing : buffs_) {
        if (existing && existing->GetId() == buff->GetId()) {
            // Stack or refresh
            if (existing->GetStackType() == CBuffStackType::STACK) {
                existing->AddStack();
            } else {
                existing->RefreshDuration();
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

void CUnit::RemoveBuffsIf(CSimulation& game, bool removePositive, bool removeNegative, bool timedLifeOnly) {
    for (auto it = buffs_.begin(); it != buffs_.end();) {
        CBuff* buff = it->get();
        bool match = buff != nullptr;
        if (match && timedLifeOnly) {
            match = dynamic_cast<CBuffTimedLife*>(buff) != nullptr;
        }
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

bool CUnit::IsHexed() const {
    for (const auto& buff : buffs_) {
        if (buff && dynamic_cast<const CBuffHex*>(buff.get())) return true;
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

void CUnit::IssueOrder(std::unique_ptr<COrder> order) {
    if (order) lastOrderId_ = order->GetOrderId();
    orderQueue_.push(std::move(order));
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
            // Cargo goes down with the transport (WC3 rule).
            for (auto* passenger : cargoUnits_) {
                if (passenger && !passenger->IsDead()) {
                    passenger->Damage(simulation, nullptr, false, false,
                                      CAttackType::CHAOS, CDamageType::NORMAL,
                                      "", 1e9f, 0);
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

    // Apply health/mana regen
    ApplyRegen(simulation);

    // Process behavior
    if (currentBehavior_) {
        bool done = currentBehavior_->Update(simulation);
        if (done) {
            currentBehavior_->End(simulation);
            if (currentBehavior_ == ownedCurrentBehavior_.get()) {
                ownedCurrentBehavior_.reset();
            }
            currentBehavior_ = nullptr;
            // Try to get next behavior from order queue
            PollNextOrderBehavior(simulation);
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
    // Use default behavior (usually stop/idle)
    if (defaultBehavior_) {
        ownedCurrentBehavior_.reset();
        currentBehavior_ = defaultBehavior_.get();
        currentBehavior_->SetUnit(this);
        currentBehavior_->Begin(simulation);
    }
}

float CUnit::Damage(CSimulation& simulation, CUnit* source,
                    bool isAttack, bool isRanged,
                    CAttackType attackType, CDamageType damageType,
                    const std::string& weaponSoundType,
                    float damage, float bonusDamage) {
    // Drunken Haze (Brewmaster): the swing may miss outright and land
    // nothing at all (ref miss chance). Checked before everything,
    // including invulnerability and callbacks.
    if (isAttack && source && source != this) {
        if (auto* haze = dynamic_cast<CBuffHaze*>(source->GetBuff(War3ID("BNdh")))) {
            if (simulation.SimRand01() < haze->GetMissChance()) return 0;
        }
    }

    if (invulnerable_) return 0;

    // Mana Shield (Sea Witch): incoming damage drains mana first.
    if (damage > 0 && GetMana() > 0 && GetBuff(War3ID("BNms"))) {
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
    }

    // Avatar (Mountain King): spell immunity while the god-rage holds.
    if (attackType == CAttackType::SPELLS && GetBuff(War3ID("BAvt"))) return 0;

    // Single shared formula (ref CUnit.damage): attack-type table always
    // applies; armor value only matters for Normal damage type.
    DamageResult result = simulation.GetCombatSystem()->CalculateDamage(
        source, this, isAttack, isRanged, attackType, damageType,
        damage, bonusDamage, 0, 0);
    float actualDamage = result.damageDealt;

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

    float oldLife = life_;
    life_ -= actualDamage;
    if (life_ < 0) life_ = 0;

    // Sleep breaks on any damage (ref sleep). Removal runs OnRemove; the
    // buff has no behavior of its own so no extra cleanup is needed.
    if (actualDamage > 0 && IsAsleep()) {
        RemoveBuff(War3ID("BUsl"), simulation);
    }

    // Thorns aura: reflect a share of melee attack damage back to the attacker.
    // Guarded by isAttack+!isRanged so reflected hits never re-reflect
    // (no ping-pong) and ranged attacks are unaffected (ref listener:
    // SPELLS attack + DEFENSIVE damage, non-attack).
    // Any CBuffThorns counts, whatever its id (Bthr item buff, BEah aura).
    if (isAttack && !isRanged && source && source != this && actualDamage > 0) {
        for (const auto& buff : buffs_) {
            auto* thorns = dynamic_cast<CBuffThorns*>(buff.get());
            if (!thorns) continue;
            float reflected = actualDamage * thorns->GetReflectPercent();
            if (reflected > 0) {
                source->Damage(simulation, nullptr, false, false,
                               CAttackType::SPELLS, CDamageType::DEFENSIVE,
                               "", reflected, 0);
            }
            break;
        }
        // Spiked Carapace (Crypt Lord passive): same melee-only reflect
        // terms as thorns; the armor half rides the unit bonus field.
        if (auto* carapace = dynamic_cast<CAbilitySpikedCarapace*>(GetAbility(War3ID("AUts")))) {
            float reflected = actualDamage * carapace->GetReflectPercent();
            if (reflected > 0) {
                source->Damage(simulation, nullptr, false, false,
                               CAttackType::SPELLS, CDamageType::DEFENSIVE,
                               "", reflected, 0);
            }
        }
    }

    // Vampiric aura: heal the attacker for a share of attack damage dealt.
    // No leech from buildings or mechanical units (ref lifesteal listener).
    if (isAttack && source && source != this && actualDamage > 0 && !IsBuilding()) {
        bool mechanical = false;
        if (unitType_) {
            const auto& classes = unitType_->GetClassifications();
            mechanical = classes.find(CUnitClassification::MECHANICAL) != classes.end();
        }
        if (!mechanical) {
            if (auto* leech = dynamic_cast<CBuffLifesteal*>(source->GetBuff(War3ID("BUav")))) {
                float healed = source->GetLife() + actualDamage * leech->GetStealPercent();
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
            }
        }
    }

    // Orb attack modifiers (ref orb listeners): frost chills, corruption
    // strips armor, fire adds a spell-damage proc. Same isAttack guard as
    // Bash; procs deal non-attack damage so they never re-proc.
    if (isAttack && source && source != this && actualDamage > 0 && !IsDead()) {
        if (auto* frost = dynamic_cast<CAbilityOrbOfFrost*>(source->GetAbility(War3ID("AIob")))) {
            AddBuff(std::make_unique<CBuffSlow>(
                frost->GetDuration(), frost->GetSlowPercent(), source->GetHandleId()), simulation);
        }
        if (auto* corr = dynamic_cast<CAbilityOrbOfCorruption*>(source->GetAbility(War3ID("AIcb")))) {
            AddBuff(std::make_unique<CBuffArmor>(
                War3ID("AIcb"), corr->GetDuration(), corr->GetArmorPenalty(), false), simulation);
        }
        if (auto* fire = dynamic_cast<CAbilityOrbOfFire*>(source->GetAbility(War3ID("AIfb")))) {
            Damage(simulation, source, false, false,
                   CAttackType::SPELLS, CDamageType::FIRE,
                   "", fire->GetBonusDamage(), 0);
        }
        // Cold/Frost Arrows (AHca/ANfa): chilling poison proc on attack.
        CAbilityColdArrows* arrows = dynamic_cast<CAbilityColdArrows*>(
            source->GetAbility(War3ID("AHca")));
        if (!arrows) arrows = dynamic_cast<CAbilityColdArrows*>(
            source->GetAbility(War3ID("ANfa")));
        if (arrows) {
            AddBuff(std::make_unique<CBuffSlow>(
                arrows->GetSlowDuration(), arrows->GetSlowPercent(), source->GetHandleId()), simulation);
            AddBuff(std::make_unique<CBuffPoison>(
                arrows->GetPoisonDuration(), arrows->GetPoisonPerTick(), source->GetHandleId()), simulation);
        }
        // Searing Arrows (Priestess toggle): same fire-proc shape as the
        // orb; per-shot mana cost is behavior-side.
        if (auto* searing = dynamic_cast<CAbilitySearingArrows*>(source->GetAbility(War3ID("AHfa")))) {
            Damage(simulation, source, false, false,
                   CAttackType::SPELLS, CDamageType::FIRE,
                   "", searing->GetBonusDamage(), 0);
        }
        // Cleaving Attack (Pit Lord): share of the landed damage splashes
        // to the victim's neighbors (ref cleave). Non-attack damage so the
        // splash itself never re-procs.
        if (auto* cleave = dynamic_cast<CAbilityCleavingAttack*>(source->GetAbility(War3ID("ANca")))) {
            const float splash = actualDamage * cleave->GetSplashFraction();
            const float radius = cleave->GetSplashRadius();
            if (splash > 0) {
                for (auto* unit : simulation.GetUnits()) {
                    if (!unit || unit->IsDead() || unit == this || unit == source) continue;
                    if (unit->GetPlayerIndex() == source->GetPlayerIndex()) continue;
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
                    for (auto* unit : simulation.GetUnits()) {
                        if (!unit || unit->IsDead() || unit == this || unit == source) continue;
                        if (unit->GetPlayerIndex() == source->GetPlayerIndex()) continue;
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
    }

    auto& dmgCb = simulation.GetUnitDamageCallback();
    if (dmgCb) {
        int srcHandle = source ? source->GetHandleId() : -1;
        dmgCb(handleId_, srcHandle, actualDamage);
    }

    (void)weaponSoundType;

    if (oldLife > 0 && life_ <= 0) {
        auto& deathCb = simulation.GetUnitDeathCallback();
        int srcHandle = source ? source->GetHandleId() : -1;
        if (deathCb) deathCb(handleId_, srcHandle, playerIndex_);
    }

    return oldLife - life_;
}

void CUnit::ConvertOrderToBehavior(CSimulation& simulation, std::unique_ptr<COrder> order) {
    if (!order) return;
    int orderId = order->GetOrderId();

    // Fire order event callback
    auto& orderCb = simulation.GetUnitOrderCallback();
    if (orderCb) {
        int targetHandle = -1;
        float ox = 0, oy = 0;
        if (auto* widgetOrder = dynamic_cast<COrderTargetWidget*>(order.get())) {
            auto* tgt = widgetOrder->GetTarget();
            if (tgt) targetHandle = tgt->GetHandleId();
        } else if (auto* pointOrder = dynamic_cast<COrderTargetPoint*>(order.get())) {
            ox = pointOrder->GetX();
            oy = pointOrder->GetY();
        }
        orderCb(handleId_, orderId, targetHandle, ox, oy);
    }

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
                }
            } else if (auto* pointOrder = dynamic_cast<COrderTargetPoint*>(order.get())) {
                if (ab->CheckCanTargetPoint(simulation, this, orderId, pointOrder->GetX(), pointOrder->GetY())) {
                    if (!ab->CheckBeforeQueue(simulation, this, orderId, pointOrder->GetX(), pointOrder->GetY())) {
                        return true;
                    }
                    newBehavior = ab->Begin(simulation, this, orderId, pointOrder->GetX(), pointOrder->GetY());
                }
            } else if (dynamic_cast<COrderNoTarget*>(order.get())) {
                if (ab->CheckCanTargetNoTarget(simulation, this, orderId)) {
                    if (!ab->CheckBeforeQueueNoTarget(simulation, this, orderId)) {
                        return true;
                    }
                    newBehavior = ab->BeginNoTarget(simulation, this, orderId);
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

    // No ability handled this order - fallback to stop
    auto behavior = std::make_unique<CBehaviorStop>();
    behavior->SetUnit(this);
    behavior->Begin(simulation);
    ownedCurrentBehavior_ = std::move(behavior);
    currentBehavior_ = ownedCurrentBehavior_.get();
    std::ostringstream oss;
    oss << "[MoveDebug] dispatch_fallback"
        << " unit=" << handleId_
        << " type=" << typeId_.ToString()
        << " order=" << orderId
        << " behavior=STOP";
    MoveDebugLogLine(oss.str());
}
