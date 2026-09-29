#pragma once
#include <vector>
#include "ecs/buff.h"
#include "ecs/abilities/ability_aura.h"

// Slow debuff - reduces movement speed and attack speed
class CBuffSlow : public CBuff {
public:
    CBuffSlow(float duration, float slowPercent, int casterHandle = -1);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float slowPercent_;
    int appliedReduction_ = 0;
};

// Poison debuff - periodic damage
class CBuffPoison : public CBuff {
public:
    CBuffPoison(float duration, float damagePerTick, int casterHandle = -1);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float damagePerTick_;
};

// Bloodlust buff - increases attack speed and movement speed
class CBuffBloodlust : public CBuff {
public:
    CBuffBloodlust(float duration, float speedBonus, float attackSpeedBonus);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float speedBonus_;
    float attackSpeedBonus_;
    int appliedSpeedBonus_ = 0;
};

// Thorns aura buff - reflects melee damage
class CBuffThorns : public CBuff {
public:
    CBuffThorns(float duration, float reflectPercent, War3ID id = War3ID("Bthr"));

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
    float GetReflectPercent() const { return reflectPercent_; }

private:
    float reflectPercent_;
};

// Vampiric aura buff - heals the attacker for a share of attack damage
// (ref CUnitDefaultLifestealListener; buff id BUav from object data).
class CBuffLifesteal : public CBuff {
public:
    CBuffLifesteal(float duration, float stealPercent);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
    float GetStealPercent() const { return stealPercent_; }

private:
    float stealPercent_;
};

// Divine Shield buff - invulnerable
class CBuffDivineShield : public CBuff {
public:
    CBuffDivineShield(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
};

// Ethereal buff (Banish): physical immunity + magic/spell bonus taken.
// Drives CUnit::IsEthereal, consumed by CCombatSystem (ref ethereal damage
// rules). Removal is stacking-aware like Divine Shield.
class CBuffEthereal : public CBuff {
public:
    explicit CBuffEthereal(float duration, War3ID id = War3ID("Bban"));

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
};

// Magic immunity buff (Anti-magic Shell): resists MAGIC and non-universal
// SPELLS. Drives CUnit::IsMagicImmune, consumed by CCombatSystem.
class CBuffMagicImmune : public CBuff {
public:
    explicit CBuffMagicImmune(float duration, War3ID id = War3ID("Bam2"));

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
};

// Stun debuff - cannot act
class CBuffStun : public CBuff {
public:
    CBuffStun(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
};

// Sleep debuff - like stun, but any damage wakes the unit (ref sleep).
class CBuffSleep : public CBuff {
public:
    CBuffSleep(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
};

// Silence debuff - cannot cast spells, but can move and attack (ref).
// Pure marker buff; casting is blocked in CBehaviorSpell.
class CBuffSilence : public CBuff {
public:
    CBuffSilence(float duration);

    CBuff* Clone() const override;
};

// Hex debuff - cannot attack or cast, but can still move (ref).
// Pure marker buff; attack/spell behaviors check IsHexed().
class CBuffHex : public CBuff {
public:
    CBuffHex(float duration);

    CBuff* Clone() const override;
};

// Rejuvenation buff - periodic heal
class CBuffRejuvenation : public CBuff {
public:
    CBuffRejuvenation(float duration, float healPerTick);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float healPerTick_;
};

// Timed life buff - marks a summoned unit and kills it on expiry.
// (ref CBuffTimedLife: summon spells attach this instead of a unit field;
// isTimedLifeBar() drives the blue dissipate bar in the renderer.)
class CBuffTimedLife : public CBuff {
public:
    CBuffTimedLife(War3ID alias, float duration);

    void OnRemove(CSimulation& game, CUnit* target) override;
    bool IsTimedLifeBar() const { return true; }
    CBuff* Clone() const override;
};

// Life drain channel - periodic damage to the bearer, healing the caster.
// (ref Life Drain: channelled, not the instant drain previously used.)
class CBuffLifeDrain : public CBuff {
public:
    CBuffLifeDrain(float duration, float drainPerTick, int casterHandle);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float drainPerTick_;
};

// Armor modifier - flat bonus armor while held (Frost Armor); negative
// values model armor-reducing curses (Faerie Fire).
class CBuffArmor : public CBuff {
public:
    CBuffArmor(War3ID id, float duration, int armorDelta, bool positive);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    int armorDelta_;
};

// Doom - periodic damage; if the bearer dies while doomed, a Doom Guard
// (nba2, ref UnitID1 of ANdo) rises for the caster's player.
class CBuffDoom : public CBuff {
public:
    CBuffDoom(float duration, float damagePerTick, int casterHandle);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    void OnDeath(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float damagePerTick_;
};

// Lightning Shield - periodic Spiral damage to the bearer's enemies.
// (ref Lightning Shield: the bearer is spared here; retail also zaps
// the shielded unit itself, a visual-side approximation.)
class CBuffLightningShield : public CBuff {
public:
    CBuffLightningShield(float duration, float damagePerTick, float radius);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float damagePerTick_;
    float radius_;
};

// Polymorph - behaves exactly like Hex (no attack/cast, can move); a
// subclass so IsHexed() (type-based) picks it up under its own id.
class CBuffPolymorph : public CBuffHex {
public:
    explicit CBuffPolymorph(float duration);

    CBuff* Clone() const override;
};

// Inner Fire - timed bonus damage and armor (ref Priest Inner Fire;
// Roar reuses this with zero armor).
class CBuffInnerFire : public CBuff {
public:
    CBuffInnerFire(float duration, int bonusDamage, int bonusArmor);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    int bonusDamage_;
    int bonusArmor_;
};

// Defend - marker buff; weapon-attack damage is halved in CUnit::Damage.
// (Timed approximation of the Footman/Knight defend toggle.)
class CBuffDefend : public CBuff {
public:
    explicit CBuffDefend(float duration);

    CBuff* Clone() const override;
};

// Clarity - timed bonus mana regen (ref Clarity Potion).
class CBuffClarity : public CBuff {
public:
    CBuffClarity(float duration, float regenBonus);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float regenBonus_;
};

// Healing Ward - periodic heal for the bearer's allies in radius.
// (Mirrors CBuffLightningShield with the opposite sign.)
class CBuffHealingWard : public CBuff {
public:
    CBuffHealingWard(float duration, float healPerTick, float radius);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float healPerTick_;
    float radius_;
};

// Ensnare - roots the bearer in place (movement behaviors honor
// IsRooted(); cf. Ancient root). Attacking still works.
class CBuffEnsnare : public CBuff {
public:
    explicit CBuffEnsnare(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
};

// Wind Walk - timed invisibility (visibility reads buff presence in
// CUnit::IsInvisibleTo); attacking breaks it with a backstab bonus.
class CBuffWindWalk : public CBuff {
public:
    CBuffWindWalk(float duration, float backstabDamage);

    float GetBackstabDamage() const { return backstabDamage_; }
    CBuff* Clone() const override;

private:
    float backstabDamage_;
};

// Berserk - faster attacks, weaker skin (cf. Defend's damage hook with
// the opposite sign).
class CBuffBerserk : public CBuff {
public:
    CBuffBerserk(float duration, float attackSpeedMul, float damageTakenMul);

    float GetDamageTakenMultiplier() const { return damageTakenMul_; }
    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float attackSpeedMul_;
    float damageTakenMul_;
};

// Burrow - underground: invisible and rooted (movement honors both).
// Burrowed units cannot be ordered to attack (ref, checked in
// CAbilityAttack::CheckCanTarget).
class CBuffBurrow : public CBuff {
public:
    explicit CBuffBurrow(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
};

// Alternate-form morph (Metamorphosis, Bear/Raven/Robo): swaps the
// bearer's type snapshot, restoring it on removal. Graceful without
// unit data (ref summon-without-data).
class CBuffTransform : public CBuff {
public:
    CBuffTransform(War3ID id, float duration, War3ID formId);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    War3ID formId_;
};

// Avatar - god-rage: bonus damage and life plus spell immunity (the
// immunity hook lives in CUnit::Damage).
class CBuffAvatar : public CBuff {
public:
    CBuffAvatar(float duration, int bonusDamage, int bonusLife);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    int bonusDamage_;
    int bonusLife_;
};

// Stone Form - living statue: invulnerable and rooted (cf. Burrow
// without the stealth).
class CBuffStoneForm : public CBuff {
public:
    explicit CBuffStoneForm(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
};

// Mana siphon - periodic mana drain, feeding the caster (cf. Life Drain
// with mana instead of life).
class CBuffManaSiphon : public CBuff {
public:
    CBuffManaSiphon(float duration, float manaPerTick, int casterHandle);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float manaPerTick_;
};

// Spawner (Pocket Factory) - periodically builds a unit for the
// bearer's player next to the bearer. A positive explode damage also
// arms each spawn with CBuffExplode (Clockwerk Goblins).
class CBuffSpawner : public CBuff {
public:
    CBuffSpawner(float duration, War3ID spawnType, float interval,
                 float explodeDamage = 0.0f, float explodeRadius = 0.0f);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    War3ID spawnType_;
    float explodeDamage_;
    float explodeRadius_;
};

// Split (Lava Spawn) - on death, breaks into smaller spawn (each
// carrying the next split down the chain).
class CBuffSplit : public CBuff {
public:
    CBuffSplit(War3ID splitType, int count, War3ID childType, int childCount);

    void OnDeath(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    War3ID splitType_;
    int count_;
    War3ID childType_;
    int childCount_;
};

// Explode (Clockwerk Goblin) - on death, burns nearby enemies.
class CBuffExplode : public CBuff {
public:
    CBuffExplode(float damage, float radius);

    void OnDeath(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float damage_;
    float radius_;
};

// Disease - periodic damage (cf. Poison with its own id).
class CBuffDisease : public CBuff {
public:
    CBuffDisease(War3ID id, float duration, float damagePerTick);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float damagePerTick_;
};

// Phoenix rebirth - on death, leaves an egg that hatches later.
class CBuffPhoenixRebirth : public CBuff {
public:
    CBuffPhoenixRebirth(float eggHatchDelay = 10.0f);

    void OnDeath(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float eggHatchDelay_;
};

// Hatch-after - on expiry of a living bearer, replaces it with the
// hatch type (Phoenix egg). A killed bearer hatches nothing.
class CBuffHatchAfter : public CBuff {
public:
    CBuffHatchAfter(float duration, War3ID hatchType, bool chainRebirth);

    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    War3ID hatchType_;
    bool chainRebirth_;
};

// Digest (Kodo Devour) - the swallowed victim takes damage over time
// inside the devourer (retail DataA 5 dps).
class CBuffDigest : public CBuff {
public:
    CBuffDigest(float duration, float damagePerTick);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float damagePerTick_;
};

// Howl of Terror - the bearer's attacks land weakened (retail DataA
// 0.3 read as a 30% reduction; the factor itself is explicit).
class CBuffHowl : public CBuff {
public:
    CBuffHowl(float duration, float damageFactor);

    float GetDamageFactor() const { return damageFactor_; }
    CBuff* Clone() const override;

private:
    float damageFactor_;
};

// Curse (Banshee, Acrs) - the bearer's attacks may miss (ref DataA).
class CBuffCurse : public CBuff {
public:
    CBuffCurse(float duration, float missChance);

    float GetMissChance() const { return missChance_; }
    CBuff* Clone() const override;

private:
    float missChance_;
};

// Drunken Haze - the bearer's attacks may miss outright (retail
// DataB 0.45 miss chance).
class CBuffHaze : public CBuff {
public:
    CBuffHaze(float duration, float missChance);

    float GetMissChance() const { return missChance_; }
    CBuff* Clone() const override;

private:
    float missChance_;
};

// Mana Shield - incoming damage drains mana instead of life while
// mana remains.
class CBuffManaShield : public CBuff {
public:
    explicit CBuffManaShield(float duration);

    CBuff* Clone() const override;
};

// Cyclone - like a stun, but the tossed unit cannot be harmed either.
class CBuffCyclone : public CBuffStun {
public:
    explicit CBuffCyclone(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
};

// Whirlwind (Bladestorm, AOww) - the bearer spins, striking nearby
// enemies every second. While spinning the bearer is magic immune and
// cannot attack.
class CBuffWhirlwind : public CBuff {
public:
    CBuffWhirlwind(float duration, float damagePerSecond, float radius);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float damagePerSecond_;
    float radius_;
};

// Immolation (Demon Hunter, AEim) - burning aura around the bearer that
// drains mana every second; guttering out when mana runs dry (ref
// CAbilityImmolation tick/deactivate).
class CBuffImmolation : public CBuff {
public:
    CBuffImmolation(float duration, float damagePerTick,
                    float manaDrainPerSecond, float radius);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float damagePerTick_;
    float manaDrainPerSecond_;
    float radius_;
};

// Phoenix Fire (Apxf) - the phoenix scorches nearby enemies every
// second (ref CAbilityTypeDefinitionPhoenixFire DataA initial damage,
// DataB damage per second, Area).
class CBuffPhoenixFire : public CBuff {
public:
    CBuffPhoenixFire(float duration, float damagePerSecond, float radius,
                     float initialDamage = 0.0f);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float damagePerSecond_;
    float radius_;
    float initialDamage_ = 0.0f;
};

// Spell Shield marker (retail Amulet of Spell Shield, buff BNss): pure
// marker buff consumed by CheckSpellEffectReaction. Duration is the
// shield window; blocking one spell removes it.
class CBuffSpellShield : public CBuff {
public:
    explicit CBuffSpellShield(float duration);

    CBuff* Clone() const override;
};

// Aura stat bonus (ref BuffAuraBase + non-stacking stat bonuses): one
// instance per aura ability per bearer, carrying all of that aura's
// bonuses. Each bonus registers in the unit's non-stacking table so
// overlapping auras take the strongest bonus per (stat, percent) slot
// instead of adding up. Removing the buff unregisters and recomputes.
// Effectively permanent (1e9s); the aura system removes it when the
// bearer leaves range.
class CBuffAuraStat : public CBuff {
public:
    struct Bonus {
        CAuraEffectType type = CAuraEffectType::MOVE_SPEED;
        float value = 0.0f;
        bool percent = false;
    };

    CBuffAuraStat(War3ID auraCode, std::vector<Bonus> bonuses,
                  int casterHandle = -1);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    std::vector<Bonus> bonuses_;
};

// Resistant Skin marker (Arsk). Negative buffs applied while this is
// present have their duration quartered in CUnit::AddBuff.
class CBuffResistant : public CBuff {
public:
    explicit CBuffResistant();
    CBuff* Clone() const override;
};

// Incoming attack-type multipliers and an additive outgoing attack
// multiplier (Runed Bracers AIsr, item reaction AIdd). 1 and 0 are no-ops.
class CBuffDamageMod : public CBuff {
public:
    CBuffDamageMod(War3ID id, float spellsTaken, float magicTaken,
                   float pierceTaken, float attackDealtAdd);

    float GetSpellsTaken() const { return spellsTaken_; }
    float GetMagicTaken() const { return magicTaken_; }
    float GetPierceTaken() const { return pierceTaken_; }
    float GetAttackDealtAdd() const { return attackDealtAdd_; }
    CBuff* Clone() const override;

private:
    float spellsTaken_;
    float magicTaken_;
    float pierceTaken_;
    float attackDealtAdd_;
};
