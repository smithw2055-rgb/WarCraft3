#pragma once
#include "ecs/widget.h"
#include "ecs/unit_type.h"
#include "ecs/player.h"
#include "ecs/abilities.h"
#include "ecs/buff.h"
#include "ecs/behaviors/behavior.h"
#include "ecs/behaviors/behavior_stop.h"
#include "ecs/behaviors/behavior_move.h"
#include "ecs/behaviors/behavior_follow.h"
#include "ecs/behaviors/behavior_patrol.h"
#include "ecs/behaviors/behavior_hold_position.h"
#include "ecs/behaviors/behavior_attack.h"
#include "ecs/behaviors/behavior_attack_move.h"
#include "ecs/orders/order.h"
#include "data/combat_types.h"
#include <vector>
#include <queue>
#include <memory>
#include <unordered_set>

class CAbilityAttack;
class CAbilityMove;
class CAbilityHero;
class CAbilityInventory;

class CUnit : public CWidget {
public:
    CUnit(int handleId, CUnitType* unitType, int playerIndex,
          War3ID typeId, float x, float y, float facing);

    // Type info
    CUnitType* GetUnitType() const { return unitType_; }
    War3ID GetTypeId() const { return typeId_; }
    int GetPlayerIndex() const { return playerIndex_; }

    // Movement
    float GetFacing() const { return facing_; }
    void SetFacing(float f) { facing_ = f; }
    float GetTurnRate() const { return turnRate_; }
    PathingGridMovementType GetMovementType() const { return movementType_; }
    float GetSpeed() const { return speed_; }
    void SetSpeed(int s) { speed_ = s; }

    // Position (override from CWidget)
    void SetPosition(float x, float y);
    void SetPositionAndCheckUnstuck(float x, float y, CSimulation& simulation);

    // Life / mana
    float GetMana() const { return mana_; }
    void SetMana(float v) { mana_ = v; }
    int GetMaxMana() const { return maxMana_; }
    void SetMaxMana(int v) { maxMana_ = v; }
    int GetBaseMaxLife() const { return baseMaxLife_; }
    int GetDefense() const { return currentDefense_; }

    // Regen
    void ApplyRegen(CSimulation& simulation);

    // ===== Ability System =====
    // Add/remove abilities
    void AddAbility(CSimulation& game, std::unique_ptr<CAbility> ability);
    void RemoveAbility(CSimulation& game, int abilityHandleId);
    CAbility* GetAbility(War3ID code) const;
    const std::vector<CAbility*>& GetAbilities() const { return abilities_; }

    // Legacy ability defs (deprecated, kept for backward compat)
    const std::vector<CAbilityDef>& GetAbilityDefs() const { return abilityDefs_; }
    void AddAbilityDef(const CAbilityDef& def) { abilityDefs_.push_back(def); }

    // ===== Buff System =====
    void AddBuff(std::unique_ptr<CBuff> buff, CSimulation& game);
    void RemoveBuff(War3ID buffId, CSimulation& game);
    CBuff* GetBuff(War3ID buffId) const;
    bool HasBuff(War3ID buffId) const;
    const std::vector<std::unique_ptr<CBuff>>& GetBuffs() const { return buffs_; }
    void UpdateBuffs(CSimulation& game);

    // ===== Behavior System =====
    // Current behavior
    CBehavior* GetCurrentBehavior() const { return currentBehavior_.get(); }
    void SetCurrentBehavior(std::unique_ptr<CBehavior> behavior);
    void IssueOrder(std::unique_ptr<COrder> order);
    void ClearOrders();
    void Stop(CSimulation& simulation);

    // Behavior storage (set by abilities on Add)
    CBehaviorMove* GetMoveBehavior() const { return moveBehavior_.get(); }
    CBehaviorFollow* GetFollowBehavior() const { return followBehavior_.get(); }
    CBehaviorPatrol* GetPatrolBehavior() const { return patrolBehavior_.get(); }
    CBehaviorHoldPosition* GetHoldPositionBehavior() const { return holdPositionBehavior_.get(); }
    CBehaviorAttack* GetAttackBehavior() const { return attackBehavior_.get(); }
    CBehaviorAttackMove* GetAttackMoveBehavior() const { return attackMoveBehavior_.get(); }
    CBehavior* GetDefaultBehavior() const { return defaultBehavior_.get(); }

    void SetMoveBehavior(std::unique_ptr<CBehaviorMove> b);
    void SetFollowBehavior(std::unique_ptr<CBehaviorFollow> b);
    void SetPatrolBehavior(std::unique_ptr<CBehaviorPatrol> b);
    void SetHoldPositionBehavior(std::unique_ptr<CBehaviorHoldPosition> b);
    void SetAttackBehavior(std::unique_ptr<CBehaviorAttack> b);
    void SetAttackMoveBehavior(std::unique_ptr<CBehaviorAttackMove> b);
    void SetDefaultBehavior(std::unique_ptr<CBehavior> b);

    // Poll next order from queue, returns behavior or nullptr
    CBehavior* PollNextOrderBehavior(CSimulation& simulation);

    // Update - called each simulation tick
    bool Update(CSimulation& simulation);

    // Damage
    float Damage(CSimulation& simulation, CUnit* source,
                 bool isAttack, bool isRanged,
                 CAttackType attackType, CDamageType damageType,
                 const std::string& weaponSoundType,
                 float damage, float bonusDamage = 0) override;

    // Properties
    bool IsBuilding() const { return isBuilding_; }
    bool IsHero() const { return isHero_; }
    bool IsInvulnerable() const override { return invulnerable_; }
    void SetInvulnerable(bool v) { invulnerable_ = v; }

    // State
    void SetHidden(bool v) { hidden_ = v; }
    bool IsHidden() const { return hidden_; }

    // Construction state
    bool IsConstructing() const { return constructing_; }
    void SetConstructing(bool v) { constructing_ = v; }
    bool IsConstructionPaused() const { return constructionPaused_; }
    void SetConstructionPaused(bool v) { constructionPaused_ = v; }
    float GetConstructionProgress() const { return constructionProgress_; }
    void SetConstructionProgress(float v) { constructionProgress_ = v; }

    // Hero level (per-unit instance, not from unit type)
    int GetHeroLevel() const { return heroLevel_; }
    void SetHeroLevel(int lvl) { heroLevel_ = lvl; }

    // Death
    int GetDeathTurnTick() const { return deathTurnTick_; }
    void SetDeathTurnTick(int t) { deathTurnTick_ = t; }

    // Collision
    float GetCollisionSize() const { return collisionSize_; }

    // Resource / food
    int GetFoodUsed() const { return foodUsed_; }
    int GetFoodMade() const { return foodMade_; }
    void SetFoodUsed(int v) { foodUsed_ = v; }
    void SetFoodMade(int v) { foodMade_ = v; }

    // Fly height
    float GetDefaultFlyHeight() const { return flyHeight_; }
    void SetFlyingHeight(float h) { SetFlyHeight(h); }

private:
    void PerformDefaultBehavior(CSimulation& simulation);
    void ConvertOrderToBehavior(CSimulation& simulation, std::unique_ptr<COrder> order);

    CUnitType* unitType_;
    War3ID typeId_;
    int playerIndex_;
    float facing_;

    // Stats
    int speed_;
    int baseMaxLife_;
    float mana_;
    int maxMana_;
    int currentDefense_;
    float turnRate_;
    PathingGridMovementType movementType_;
    float collisionSize_;

    // State
    bool isBuilding_;
    bool isHero_;
    bool hidden_ = false;
    bool invulnerable_ = false;
    bool constructing_ = false;
    bool constructionPaused_ = false;
    float constructionProgress_ = 0;
    int deathTurnTick_ = 0;
    int foodUsed_ = 0;
    int foodMade_ = 0;

    // Hero
    int heroLevel_ = 1;

    // Abilities
    std::vector<CAbility*> abilities_; // raw pointers to owned abilities
    std::vector<std::unique_ptr<CAbility>> ownedAbilities_;
    std::vector<CAbilityDef> abilityDefs_; // legacy

    // Buffs
    std::vector<std::unique_ptr<CBuff>> buffs_;

    // Behavior storage (owned by unit, created by abilities)
    std::unique_ptr<CBehaviorMove> moveBehavior_;
    std::unique_ptr<CBehaviorFollow> followBehavior_;
    std::unique_ptr<CBehaviorPatrol> patrolBehavior_;
    std::unique_ptr<CBehaviorHoldPosition> holdPositionBehavior_;
    std::unique_ptr<CBehaviorAttack> attackBehavior_;
    std::unique_ptr<CBehaviorAttackMove> attackMoveBehavior_;
    std::unique_ptr<CBehavior> defaultBehavior_;

    // Current behavior and order queue
    std::unique_ptr<CBehavior> currentBehavior_;
    std::queue<std::unique_ptr<COrder>> orderQueue_;
};
