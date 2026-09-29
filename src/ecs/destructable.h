#pragma once
#include "ecs/widget.h"
#include "resource/slk.h"

class CDestructable : public CWidget {
public:
    CDestructable(int handleId, War3ID typeId, float x, float y, float life);

    War3ID GetTypeId() const { return typeId_; }
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
    // Occluder height for the renderer (ref Get/SetDestructableOccluderHeight).
    float GetOccluderHeight() const { return occluderHeight_; }
    void SetOccluderHeight(float v) { occluderHeight_ = v; }

private:
    War3ID typeId_;
    bool invulnerable_ = false;
    bool hidden_ = false;
    float animationSpeed_ = 1.0f;
    float occluderHeight_ = 0.0f;
    std::string animation_;
    std::string queuedAnimation_;
    int animGeneration_ = 0;
};
