#include "unit.h"
#include "csimulation.h"
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
}

void CUnit::SetPosition(float x, float y) {
    x_ = x;
    y_ = y;
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
        propulsionWindow = std::max(1.0f, unitType_->GetPropWindow());
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

void CUnit::SetCurrentBehavior(std::unique_ptr<CBehavior> behavior) {
    currentBehavior_ = nullptr;
    ownedCurrentBehavior_ = std::move(behavior);
    currentBehavior_ = ownedCurrentBehavior_.get();
    if (currentBehavior_) {
        currentBehavior_->SetUnit(this);
    }
}

void CUnit::IssueOrder(std::unique_ptr<COrder> order) {
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

void CUnit::ApplyRegen(CSimulation& simulation) {
    if (IsDead() || IsBuilding()) return;

    float dt = simulation.GetSimStepTime();
    bool isNight = simulation.IsNighttime();
    CRegenType regenType = unitType_->GetLifeRegenType();

    // HP regen
    bool canRegen = (regenType == CRegenType::ALWAYS) ||
                    (regenType == CRegenType::ONLY_AT_NIGHT && isNight);
    if (canRegen && life_ < maxLife_) {
        life_ += unitType_->GetLifeRegen() * dt;
        if (life_ > maxLife_) life_ = maxLife_;
    }

    // Mana regen
    if (mana_ < (float)maxMana_) {
        mana_ += unitType_->GetManaRegen() * dt;
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
            // Notify abilities of death
            for (auto* ab : abilities_) {
                if (ab) ab->OnDeath(simulation, this);
            }
            return false;
        }

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
    if (invulnerable_) return 0;

    float damageReduction = CDefenseTypeInfo::CalcDamageReduction(currentDefense_, 0);
    float actualDamage = damage * (1.0f - damageReduction) + bonusDamage;
    if (actualDamage < 0) actualDamage = 0;

    float oldLife = life_;
    life_ -= actualDamage;
    if (life_ < 0) life_ = 0;

    auto& dmgCb = simulation.GetUnitDamageCallback();
    if (dmgCb) {
        int srcHandle = source ? source->GetHandleId() : -1;
        dmgCb(handleId_, srcHandle, actualDamage);
    }

    (void)isAttack;
    (void)isRanged;
    (void)attackType;
    (void)damageType;
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
