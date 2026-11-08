#pragma once
#include <vector>
#include "ecs/buff.h"
#include "ecs/ability.h"

// Slow debuff - reduces movement speed and attack speed
class CBuffSlow : public CBuffCloneable<CBuffSlow> {
public:
    CBuffSlow(float duration, float slowPercent, int casterHandle = -1);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;

private:
    float slowPercent_;
    int appliedReduction_ = 0;
};

// Poison debuff - periodic damage
class CBuffPoison : public CBuffCloneable<CBuffPoison> {
public:
    CBuffPoison(float duration, float damagePerTick, int casterHandle = -1);

    void OnPeriodic(CSimulation& game, CUnit* target) override;

private:
    float damagePerTick_;
};

// Bloodlust buff - increases attack speed and movement speed
class CBuffBloodlust : public CBuffCloneable<CBuffBloodlust> {
public:
    CBuffBloodlust(float duration, float speedBonus, float attackSpeedBonus);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;

private:
    float speedBonus_;
    float attackSpeedBonus_;
    int appliedSpeedBonus_ = 0;
};

// Thorns aura buff - reflects melee damage
class CBuffThorns : public CBuffCloneable<CBuffThorns> {
public:
    CBuffThorns(float duration, float reflectPercent, War3ID id = War3ID("Bthr"))
        : CBuffCloneable(id, duration, true), reflectPercent_(reflectPercent) {}

    float GetReflectPercent() const { return reflectPercent_; }

private:
    float reflectPercent_;
};

// Vampiric aura buff - heals the attacker for a share of attack damage
class CBuffLifesteal : public CBuffCloneable<CBuffLifesteal> {
public:
    CBuffLifesteal(float duration, float stealPercent)
        : CBuffCloneable(War3ID("BUav"), duration, true), stealPercent_(stealPercent) {}

    float GetStealPercent() const { return stealPercent_; }

private:
    float stealPercent_;
};

// Divine Shield buff - invulnerable
class CBuffDivineShield : public CBuffCloneable<CBuffDivineShield> {
public:
    explicit CBuffDivineShield(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
};

// Ethereal buff (Banish): physical immunity + magic/spell bonus taken.
class CBuffEthereal : public CBuffCloneable<CBuffEthereal> {
public:
    explicit CBuffEthereal(float duration, War3ID id = War3ID("Bban"));

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
};

// Magic immunity buff (Anti-magic Shell): resists MAGIC and non-universal SPELLS.
class CBuffMagicImmune : public CBuffCloneable<CBuffMagicImmune> {
public:
    explicit CBuffMagicImmune(float duration, War3ID id = War3ID("Bam2"));

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
};

// Stun debuff - cannot act
class CBuffStun : public CBuffCloneable<CBuffStun> {
public:
    explicit CBuffStun(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    void OnRefresh(CSimulation& game, CUnit* target, const CBuff& newBuff) override;
};

// Sleep debuff - like stun, but any damage wakes the unit (ref sleep).
class CBuffSleep : public CBuffCloneable<CBuffSleep> {
public:
    explicit CBuffSleep(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
};

// Silence debuff - cannot cast spells, but can move and attack.
class CBuffSilence : public CBuffCloneable<CBuffSilence> {
public:
    explicit CBuffSilence(float duration)
        : CBuffCloneable(War3ID("BNsi"), duration, false) {}
};

// Hex debuff - cannot attack or cast, but can still move.
class CBuffHex : public CBuffCloneable<CBuffHex> {
public:
    explicit CBuffHex(float duration)
        : CBuffCloneable(War3ID("BOhx"), duration, false) {}
};

// Rejuvenation buff - periodic heal
class CBuffRejuvenation : public CBuffCloneable<CBuffRejuvenation> {
public:
    CBuffRejuvenation(float duration, float healPerTick);

    void OnPeriodic(CSimulation& game, CUnit* target) override;

private:
    float healPerTick_;
};

// Timed life buff - marks a summoned unit and kills it on expiry.
class CBuffTimedLife : public CBuffCloneable<CBuffTimedLife> {
public:
    CBuffTimedLife(War3ID alias, float duration);

    void OnRemove(CSimulation& game, CUnit* target) override;
    bool IsTimedLifeBar() const { return true; }
};

// Life drain channel - periodic damage to the bearer, healing the caster.
class CBuffLifeDrain : public CBuffCloneable<CBuffLifeDrain> {
public:
    CBuffLifeDrain(float duration, float drainPerTick, int casterHandle);

    void OnPeriodic(CSimulation& game, CUnit* target) override;

private:
    float drainPerTick_;
};

// Armor modifier - flat bonus armor while held (Frost Armor / Faerie Fire).
class CBuffArmor : public CBuffCloneable<CBuffArmor> {
public:
    CBuffArmor(War3ID id, float duration, int armorDelta, bool positive);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;

private:
    int armorDelta_;
};

// Doom - periodic damage; if the bearer dies while doomed, a Doom Guard rises.
class CBuffDoom : public CBuffCloneable<CBuffDoom> {
public:
    CBuffDoom(float duration, float damagePerTick, int casterHandle);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    void OnDeath(CSimulation& game, CUnit* target) override;

private:
    float damagePerTick_;
};

// Lightning Shield - periodic Spiral damage to nearby units.
class CBuffLightningShield : public CBuffCloneable<CBuffLightningShield> {
public:
    CBuffLightningShield(float duration, float damagePerTick, float radius);

    void OnPeriodic(CSimulation& game, CUnit* target) override;

private:
    float damagePerTick_;
    float radius_;
};

// Polymorph - behaves like Hex under its own buff id (`Bply`).
class CBuffPolymorph : public CBuffCloneable<CBuffPolymorph, CBuffHex> {
public:
    explicit CBuffPolymorph(float duration)
        : CBuffCloneable(duration) { id_ = War3ID("Bply"); }
};

// Inner Fire - timed bonus damage and armor (Roar reuses this with `Broa`).
class CBuffInnerFire : public CBuffCloneable<CBuffInnerFire> {
public:
    CBuffInnerFire(float duration, int bonusDamage, int bonusArmor,
                   War3ID buffId = War3ID("Binf"));

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;

private:
    int bonusDamage_;
    int bonusArmor_;
};

// Defend - marker buff; weapon-attack damage is halved in War3GameModule.
class CBuffDefend : public CBuffCloneable<CBuffDefend> {
public:
    explicit CBuffDefend(float duration)
        : CBuffCloneable(War3ID("Bdef"), duration, true) {}
};

// Clarity - timed bonus mana regen (Clarity Potion).
class CBuffClarity : public CBuffCloneable<CBuffClarity> {
public:
    CBuffClarity(float duration, float regenBonus);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;

private:
    float regenBonus_;
};

// Healing Ward - periodic heal for the bearer's allies in radius.
class CBuffHealingWard : public CBuffCloneable<CBuffHealingWard> {
public:
    CBuffHealingWard(float duration, float healPerTick, float radius);

    void OnPeriodic(CSimulation& game, CUnit* target) override;

private:
    float healPerTick_;
    float radius_;
};

// Ensnare - roots the bearer in place.
class CBuffEnsnare : public CBuffCloneable<CBuffEnsnare> {
public:
    explicit CBuffEnsnare(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
};

// Wind Walk - timed invisibility; attacking breaks it with backstab bonus.
class CBuffWindWalk : public CBuffCloneable<CBuffWindWalk> {
public:
    CBuffWindWalk(float duration, float backstabDamage)
        : CBuffCloneable(War3ID("BOwk"), duration, true), backstabDamage_(backstabDamage) {}

    float GetBackstabDamage() const { return backstabDamage_; }

private:
    float backstabDamage_;
};

// Berserk - faster attacks, increased damage taken.
class CBuffBerserk : public CBuffCloneable<CBuffBerserk> {
public:
    CBuffBerserk(float duration, float attackSpeedMul, float damageTakenMul);

    float GetDamageTakenMultiplier() const { return damageTakenMul_; }
    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;

private:
    float attackSpeedMul_;
    float damageTakenMul_;
};

// Burrow - underground: invisible and rooted.
class CBuffBurrow : public CBuffCloneable<CBuffBurrow> {
public:
    explicit CBuffBurrow(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
};

// Alternate-form morph (Metamorphosis, Bear/Raven/Robo).
class CBuffTransform : public CBuffCloneable<CBuffTransform> {
public:
    CBuffTransform(War3ID id, float duration, War3ID formId);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;

private:
    War3ID formId_;
};

// Avatar - bonus damage and life plus spell immunity.
class CBuffAvatar : public CBuffCloneable<CBuffAvatar> {
public:
    CBuffAvatar(float duration, int bonusDamage, int bonusLife);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;

private:
    int bonusDamage_;
    int bonusLife_;
};

// Stone Form - living statue: invulnerable and rooted.
class CBuffStoneForm : public CBuffCloneable<CBuffStoneForm> {
public:
    explicit CBuffStoneForm(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
};

// Mana siphon - periodic mana drain, feeding the caster.
class CBuffManaSiphon : public CBuffCloneable<CBuffManaSiphon> {
public:
    CBuffManaSiphon(float duration, float manaPerTick, int casterHandle);

    void OnPeriodic(CSimulation& game, CUnit* target) override;

private:
    float manaPerTick_;
};

// Spawner (Pocket Factory) - periodically builds a unit next to the bearer.
class CBuffSpawner : public CBuffCloneable<CBuffSpawner> {
public:
    CBuffSpawner(float duration, War3ID spawnType, float interval,
                 float explodeDamage = 0.0f, float explodeRadius = 0.0f);

    void OnPeriodic(CSimulation& game, CUnit* target) override;

private:
    War3ID spawnType_;
    float explodeDamage_;
    float explodeRadius_;
};

// Split (Lava Spawn) - on death, breaks into smaller spawn.
class CBuffSplit : public CBuffCloneable<CBuffSplit> {
public:
    CBuffSplit(War3ID splitType, int count, War3ID childType, int childCount);

    void OnDeath(CSimulation& game, CUnit* target) override;

private:
    War3ID splitType_;
    int count_;
    War3ID childType_;
    int childCount_;
};

// Explode (Clockwerk Goblin) - on death, burns nearby enemies.
class CBuffExplode : public CBuffCloneable<CBuffExplode> {
public:
    CBuffExplode(float damage, float radius);

    void OnDeath(CSimulation& game, CUnit* target) override;

private:
    float damage_;
    float radius_;
};

// Disease - periodic damage (cf. Poison with its own id).
class CBuffDisease : public CBuffCloneable<CBuffDisease> {
public:
    CBuffDisease(War3ID id, float duration, float damagePerTick);

    void OnPeriodic(CSimulation& game, CUnit* target) override;

private:
    float damagePerTick_;
};

// Phoenix rebirth - on death, leaves an egg that hatches later.
class CBuffPhoenixRebirth : public CBuffCloneable<CBuffPhoenixRebirth> {
public:
    explicit CBuffPhoenixRebirth(float eggHatchDelay = 10.0f);

    void OnDeath(CSimulation& game, CUnit* target) override;

private:
    float eggHatchDelay_;
};

// Hatch-after - on expiry of a living bearer, replaces it with the hatch type.
class CBuffHatchAfter : public CBuffCloneable<CBuffHatchAfter> {
public:
    CBuffHatchAfter(float duration, War3ID hatchType, bool chainRebirth);

    void OnRemove(CSimulation& game, CUnit* target) override;

private:
    War3ID hatchType_;
    bool chainRebirth_;
};

// Digest (Kodo Devour) - swallowed victim takes damage over time.
class CBuffDigest : public CBuffCloneable<CBuffDigest> {
public:
    CBuffDigest(float duration, float damagePerTick);

    void OnPeriodic(CSimulation& game, CUnit* target) override;

private:
    float damagePerTick_;
};

// Howl of Terror - bearer's attacks land weakened.
class CBuffHowl : public CBuffCloneable<CBuffHowl> {
public:
    CBuffHowl(float duration, float damageFactor)
        : CBuffCloneable(War3ID("BNht"), duration, false), damageFactor_(damageFactor) {}

    float GetDamageFactor() const { return damageFactor_; }

private:
    float damageFactor_;
};

// Curse (Banshee, Acrs) - bearer's attacks may miss.
class CBuffCurse : public CBuffCloneable<CBuffCurse> {
public:
    CBuffCurse(float duration, float missChance)
        : CBuffCloneable(War3ID("Bcrs"), duration, false), missChance_(missChance) {}

    float GetMissChance() const { return missChance_; }

private:
    float missChance_;
};

// Drunken Haze - bearer's attacks may miss outright.
class CBuffHaze : public CBuffCloneable<CBuffHaze> {
public:
    CBuffHaze(float duration, float missChance)
        : CBuffCloneable(War3ID("BNdh"), duration, false), missChance_(missChance) {}

    float GetMissChance() const { return missChance_; }

private:
    float missChance_;
};

// Mana Shield - incoming damage drains mana instead of life.
class CBuffManaShield : public CBuffCloneable<CBuffManaShield> {
public:
    explicit CBuffManaShield(float duration)
        : CBuffCloneable(War3ID("BNms"), duration, true) {}
};

// Cyclone - like a stun, but the tossed unit is invulnerable while airborne.
class CBuffCyclone : public CBuffCloneable<CBuffCyclone, CBuffStun> {
public:
    explicit CBuffCyclone(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
};

// Whirlwind (Bladestorm, AOww) - spins, striking nearby enemies every second.
class CBuffWhirlwind : public CBuffCloneable<CBuffWhirlwind> {
public:
    CBuffWhirlwind(float duration, float damagePerSecond, float radius);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    void OnPeriodic(CSimulation& game, CUnit* target) override;

private:
    float damagePerSecond_;
    float radius_;
};

// Immolation (Demon Hunter, AEim) - burning aura draining mana each second.
class CBuffImmolation : public CBuffCloneable<CBuffImmolation> {
public:
    CBuffImmolation(float duration, float damagePerTick,
                    float manaDrainPerSecond, float radius);

    void OnPeriodic(CSimulation& game, CUnit* target) override;

private:
    float damagePerTick_;
    float manaDrainPerSecond_;
    float radius_;
};

// Phoenix Fire (Apxf) - scorches nearby enemies every second.
class CBuffPhoenixFire : public CBuffCloneable<CBuffPhoenixFire> {
public:
    CBuffPhoenixFire(float duration, float damagePerSecond, float radius,
                     float initialDamage = 0.0f);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnPeriodic(CSimulation& game, CUnit* target) override;

private:
    float damagePerSecond_;
    float radius_;
    float initialDamage_ = 0.0f;
};

// Spell Shield marker (BNss): consumed by CheckSpellEffectReaction.
class CBuffSpellShield : public CBuffCloneable<CBuffSpellShield> {
public:
    explicit CBuffSpellShield(float duration)
        : CBuffCloneable(War3ID("BNss"), duration, true) {}
};

// Aura stat bonus (non-stacking stat bonuses per aura code & caster).
class CBuffAuraStat : public CBuffCloneable<CBuffAuraStat> {
public:
    struct Bonus {
        CAuraEffectType type = CAuraEffectType::MOVE_SPEED;
        float value = 0.0f;
        bool percent = false;
    };

    CBuffAuraStat(War3ID auraCode, std::vector<Bonus> bonuses,
                  int casterHandle = -1);

    bool MergeByCaster() const override { return true; }

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;

private:
    std::vector<Bonus> bonuses_;
};

// Resistant Skin marker (Arsk).
class CBuffResistant : public CBuffCloneable<CBuffResistant> {
public:
    CBuffResistant() : CBuffCloneable(War3ID("Brsk"), 1.0e9f, true) {}
};

// Incoming attack-type multipliers and additive outgoing attack multiplier (AIsr, AIdd).
class CBuffDamageMod : public CBuffCloneable<CBuffDamageMod> {
public:
    CBuffDamageMod(War3ID id, float spellsTaken, float magicTaken,
                   float pierceTaken, float attackDealtAdd)
        : CBuffCloneable(id, 1.0e9f, true)
        , spellsTaken_(spellsTaken)
        , magicTaken_(magicTaken)
        , pierceTaken_(pierceTaken)
        , attackDealtAdd_(attackDealtAdd) {}

    float GetSpellsTaken() const { return spellsTaken_; }
    float GetMagicTaken() const { return magicTaken_; }
    float GetPierceTaken() const { return pierceTaken_; }
    float GetAttackDealtAdd() const { return attackDealtAdd_; }

private:
    float spellsTaken_;
    float magicTaken_;
    float pierceTaken_;
    float attackDealtAdd_;
};
