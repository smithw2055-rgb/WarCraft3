#include "unit.h"
#include "csimulation.h"
#include "behaviors/behavior_move.h"
#include "behaviors/behavior_attack.h"
#include "behaviors/behavior_stop.h"
#include "behaviors/behavior_hold_position.h"
#include "behaviors/behavior_follow.h"
#include "behaviors/behavior_patrol.h"
#include "behaviors/behavior_attack_move.h"
#include "pathing/world_collision.h"
#include <cmath>
#include <iostream>
#include <algorithm>

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
    if (currentBehavior_) {
        currentBehavior_.reset();
    }
    currentBehavior_ = std::move(behavior);
    if (currentBehavior_) {
        currentBehavior_->SetUnit(this);
    }
}

void CUnit::IssueOrder(std::unique_ptr<COrder> order) {
    orderQueue_.push(std::move(order));
}

void CUnit::ClearOrders() {
    while (!orderQueue_.empty()) orderQueue_.pop();
    currentBehavior_.reset();
}

void CUnit::Stop(CSimulation& simulation) {
    ClearOrders();
    if (defaultBehavior_) {
        auto behavior = std::make_unique<CBehaviorStop>();
        behavior->SetUnit(this);
        behavior->Begin(simulation);
        currentBehavior_ = std::move(behavior);
    }
}

// Behavior storage setters
void CUnit::SetMoveBehavior(std::unique_ptr<CBehaviorMove> b) {
    moveBehavior_ = std::move(b);
}
void CUnit::SetFollowBehavior(std::unique_ptr<CBehaviorFollow> b) {
    followBehavior_ = std::move(b);
}
void CUnit::SetPatrolBehavior(std::unique_ptr<CBehaviorPatrol> b) {
    patrolBehavior_ = std::move(b);
}
void CUnit::SetHoldPositionBehavior(std::unique_ptr<CBehaviorHoldPosition> b) {
    holdPositionBehavior_ = std::move(b);
}
void CUnit::SetAttackBehavior(std::unique_ptr<CBehaviorAttack> b) {
    attackBehavior_ = std::move(b);
}
void CUnit::SetAttackMoveBehavior(std::unique_ptr<CBehaviorAttackMove> b) {
    attackMoveBehavior_ = std::move(b);
}
void CUnit::SetDefaultBehavior(std::unique_ptr<CBehavior> b) {
    defaultBehavior_ = std::move(b);
}

CBehavior* CUnit::PollNextOrderBehavior(CSimulation& simulation) {
    while (!orderQueue_.empty()) {
        auto order = std::move(orderQueue_.front());
        orderQueue_.pop();
        if (order) {
            ConvertOrderToBehavior(simulation, std::move(order));
            if (currentBehavior_) {
                return currentBehavior_.get();
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

bool CUnit::Update(CSimulation& simulation) {
    if (IsDead()) {
        int currentTick = simulation.GetGameTurnTick();
        if (deathTurnTick_ == 0) {
            deathTurnTick_ = currentTick;
            // Notify abilities of death
            for (auto* ab : abilities_) {
                if (ab) ab->OnDeath(simulation, this);
            }
            return false;
        }
        float deathTime = unitType_->GetDeathTime();
        int deathTicks = (int)(deathTime / 0.1f);
        if (currentTick - deathTurnTick_ > deathTicks) {
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
            currentBehavior_.reset();
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
        // Reset and start default behavior
        currentBehavior_.reset();
        auto* defBehavior = defaultBehavior_.get();
        defBehavior->SetUnit(this);
        defBehavior->Begin(simulation);
        // We don't move ownership - default behavior is a template
        // The actual behavior is a stop behavior
        auto stopBehavior = std::make_unique<CBehaviorStop>();
        stopBehavior->SetUnit(this);
        stopBehavior->Begin(simulation);
        currentBehavior_ = std::move(stopBehavior);
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

    // Try each ability to handle this order
    for (auto* ab : abilities_) {
        if (!ab || ab->IsDisabled()) continue;

        // Check if ability can handle this order
        CBehavior* newBehavior = nullptr;

        if (auto* widgetOrder = dynamic_cast<COrderTargetWidget*>(order.get())) {
            if (ab->CheckCanTarget(simulation, this, orderId, widgetOrder->GetTarget())) {
                // Let ability intercept or queue
                if (!ab->CheckBeforeQueue(simulation, this, orderId, widgetOrder->GetTarget())) {
                    return; // order consumed by ability
                }
                newBehavior = ab->Begin(simulation, this, orderId, widgetOrder->GetTarget());
            }
        } else if (auto* pointOrder = dynamic_cast<COrderTargetPoint*>(order.get())) {
            if (ab->CheckCanTargetPoint(simulation, this, orderId, pointOrder->GetX(), pointOrder->GetY())) {
                if (!ab->CheckBeforeQueue(simulation, this, orderId, pointOrder->GetX(), pointOrder->GetY())) {
                    return;
                }
                newBehavior = ab->Begin(simulation, this, orderId, pointOrder->GetX(), pointOrder->GetY());
            }
        } else if (auto* noTargetOrder = dynamic_cast<COrderNoTarget*>(order.get())) {
            if (ab->CheckCanTargetNoTarget(simulation, this, orderId)) {
                if (!ab->CheckBeforeQueueNoTarget(simulation, this, orderId)) {
                    return;
                }
                newBehavior = ab->BeginNoTarget(simulation, this, orderId);
            }
        }

        if (newBehavior) {
            // End current behavior
            if (currentBehavior_) {
                currentBehavior_->End(simulation);
                currentBehavior_.reset();
            }
            newBehavior->SetUnit(this);
            newBehavior->Begin(simulation);
            currentBehavior_.reset(newBehavior);
            return;
        }
    }

    // No ability handled this order - fallback to stop
    auto behavior = std::make_unique<CBehaviorStop>();
    behavior->SetUnit(this);
    behavior->Begin(simulation);
    currentBehavior_ = std::move(behavior);
}
