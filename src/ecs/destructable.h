#pragma once
#include <vector>
#include "ecs/widget.h"
#include "resource/slk.h"

class CDestructable : public CWidget {
public:
    CDestructable(int handleId, War3ID typeId, float x, float y, float life);

    War3ID GetTypeId() const { return typeId_; }
    void SetPosition(float x, float y) { SetX(x); SetY(y); }
    float GetImpactZ() const override { return 0; }
    bool IsInvulnerable() const override { return invulnerable_; }

    float Damage(CSimulation& simulation, CUnit* source,
                 bool isAttack, bool isRanged,
                 CAttackType attackType, CDamageType damageType,
                 const std::string& weaponSoundType,
                 float damage, float bonusDamage = 0) override;

    bool CanBeTargetedBy(CSimulation& simulation, CUnit* source,
                          const std::vector<CTargetType>& targetsAllowed) const override;

    // Animation tag for the renderer (Set/QueueDestructableAnimation).
    const std::string& GetAnimation() const { return animation_; }
    void SetAnimation(const std::string& anim) {
        animation_ = anim;
        queuedAnimation_.clear();
        ++animGeneration_;
    }
    void QueueAnimation(const std::string& anim) { queuedAnimation_ = anim; }
    const std::string& GetQueuedAnimation() const { return queuedAnimation_; }
    int GetAnimGeneration() const { return animGeneration_; }
    // Current clip finished: the queued name becomes the playing name.
    void PromoteQueuedAnimation() {
        if (queuedAnimation_.empty()) return;
        animation_ = std::move(queuedAnimation_);
        queuedAnimation_.clear();
    }
    // Animation playback speed multiplier (ref SetDestructableAnimationSpeed).
    float GetAnimationSpeed() const { return animationSpeed_; }
    void SetAnimationSpeed(float v) { animationSpeed_ = v; }
    void SetInvulnerable(bool v) { invulnerable_ = v; }
    // Visibility (ref ShowDestructable; renderer hides flagged instances).
    bool IsHidden() const { return hidden_; }
    void SetHidden(bool v) { hidden_ = v; }
    bool IsBlighted() const { return blighted_; }
    void SetBlighted(bool v) { blighted_ = v; }
    // Occluder height for the renderer (ref Get/SetDestructableOccluderHeight).
    float GetOccluderHeight() const { return occluderHeight_; }
    void SetOccluderHeight(float v) { occluderHeight_ = v; }
    // Pathing footprints (ref RemovablePathingMapInstance pair): live
    // pathTex while alive, pathTexDeath once dead. Bind on creation,
    // swap on life transitions, unbind on removal.
    void BindPathing(CSimulation& sim, int rotationDeg);
    void RefreshPathingAfterLifeChange(CSimulation& sim);
    void UnbindPathing(CSimulation& sim);
    int GetPathingStampId() const { return pathingStampId_; }
    int GetDeathStampId() const { return deathStampId_; }
    void SetLife(CSimulation& simulation, float life) override;
    // Death drops (ref war3map.doo itemSets + w3i itemTable): items created
    // at the stump on the killing blow, each rolling its own percent chance
    // independently (same rule as CUnit::DropDeathItems).
    struct DeathDropItem {
        War3ID itemId;
        int chancePercent = 0;
    };
    void SetDeathDrops(std::vector<DeathDropItem> drops) {
        deathDrops_ = std::move(drops);
    }
    const std::vector<DeathDropItem>& GetDeathDrops() const { return deathDrops_; }
    void DropDeathItems(CSimulation& simulation);

private:
    War3ID typeId_;
    uint8_t invulnerable_ : 1;
    uint8_t hidden_ : 1;
    uint8_t blighted_ : 1;
    uint8_t reservedFlags_ : 5;
    float animationSpeed_ = 1.0f;
    float occluderHeight_ = 0.0f;
    std::string animation_;
    std::string queuedAnimation_;
    int animGeneration_ = 0;
    int pathingStampId_ = -1;
    int deathStampId_ = -1;
    int pathingRotationDeg_ = 0;
    std::vector<DeathDropItem> deathDrops_;
};
