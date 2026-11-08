#pragma once
// ============================================================================
// WarCraft 3 Domain Spell, Aura, Passive & Race Economy Ability Catalog
// Phase 30 (Track B): Physically relocated out of generic `src/ecs/abilities/`
// into `src/game/war3/abilities/war3_spells.h`.
// ============================================================================
#include <memory>
#include <vector>
#include "ecs/abilities/ability_aura.h"
#include "ecs/abilities/ability_spell_base.h"

class CDestructable;
struct CAbilityDefFull;

// === auras.h ===
class CAbilityDevotionAura : public CAbilityAura { public: explicit CAbilityDevotionAura(int handleId); };
class CAbilityEnduranceAura : public CAbilityAura { public: explicit CAbilityEnduranceAura(int handleId); };
class CAbilityUnholyAura : public CAbilityAura { public: explicit CAbilityUnholyAura(int handleId); };
class CAbilityThornsAura : public CAbilityAura { public: explicit CAbilityThornsAura(int handleId); };
class CAbilityBrillianceAura : public CAbilityAura { public: explicit CAbilityBrillianceAura(int handleId); };
class CAbilityTrueshotAura : public CAbilityAura { public: explicit CAbilityTrueshotAura(int handleId); };
class CAbilityVampiricAura : public CAbilityAura { public: explicit CAbilityVampiricAura(int handleId); };
class CAbilityCommandAura : public CAbilityAura { public: explicit CAbilityCommandAura(int handleId); };
class CAbilityRegenerationAura : public CAbilityAura { public: explicit CAbilityRegenerationAura(int handleId); };
class CAbilityManaRegenerationAura : public CAbilityAura { public: explicit CAbilityManaRegenerationAura(int handleId); };
class CAbilityDiseaseCloud : public CAbilityAura { public: explicit CAbilityDiseaseCloud(int handleId); };

// === spells.h ===
class CAbilityBlizzard : public CAbilitySpellBase { public: CAbilityBlizzard(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: int waves_ = 6; float damagePerWave_ = 30.0f; float radius_ = 200.0f; };
class CAbilityHolyLight : public CAbilitySpellBase { public: CAbilityHolyLight(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float healAmount_ = 200.0f; };
class CAbilityDivineShield : public CAbilitySpellBase { public: CAbilityDivineShield(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 15.0f; };
class CAbilityThunderClap : public CAbilitySpellBase {
public:
    CAbilityThunderClap(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 60.0f;
    float radius_ = 250.0f;
    float slowPercent_ = 0.5f;
    float slowDuration_ = 5.0f;
};
class CAbilityStormBolt : public CAbilitySpellBase { public: CAbilityStormBolt(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float damage_ = 100.0f; float stunDuration_ = 2.0f; };
class CAbilityWarStomp : public CAbilitySpellBase { public: CAbilityWarStomp(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float damage_ = 25.0f; float radius_ = 250.0f; float stunDuration_ = 3.0f; };
class CAbilityDeathCoil : public CAbilitySpellBase { public: CAbilityDeathCoil(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float damage_ = 100.0f; float healAmount_ = 200.0f; };
class CAbilityFrostNova : public CAbilitySpellBase {
public:
    CAbilityFrostNova(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 100.0f;
    float aoeDamage_ = 50.0f;
    float radius_ = 200.0f;
    float slowPercent_ = 0.5f;
    float slowDuration_ = 4.0f;
};
class CAbilityManaBurn : public CAbilitySpellBase { public: CAbilityManaBurn(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: int manaBurned_ = 100; };
class CAbilityEntanglingRoots : public CAbilitySpellBase { public: CAbilityEntanglingRoots(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float damagePerSecond_ = 15.0f; float duration_ = 9.0f; };

// === spells2.h ===
class CAbilityHealingWave : public CAbilitySpellBase { public: CAbilityHealingWave(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float healAmount_ = 100.0f; int bounces_ = 3; float bounceRange_ = 500.0f; };
class CAbilityHex : public CAbilitySpellBase { public: CAbilityHex(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 15.0f; };
class CAbilityChainLightning : public CAbilitySpellBase {
public:
    CAbilityChainLightning(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 85.0f;
    int bounces_ = 4;
    float bounceRange_ = 500.0f;
    float damageReduction_ = 0.25f;
};
class CAbilityFeralSpirit : public CAbilitySpellBase { public: CAbilityFeralSpirit(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: War3ID wolfType_ = War3ID("nwlg"); int count_ = 2; float summonDuration_ = 60.0f; };
class CAbilityEarthquake : public CAbilitySpellBase { public: CAbilityEarthquake(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float damagePerSecond_ = 50.0f; float radius_ = 250.0f; float duration_ = 25.0f; };
class CAbilityCarrionSwarm : public CAbilitySpellBase { public: CAbilityCarrionSwarm(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float damage_ = 75.0f; float range_ = 700.0f; float width_ = 200.0f; };
class CAbilitySleep : public CAbilitySpellBase { public: CAbilitySleep(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 20.0f; };
class CAbilityInferno : public CAbilitySpellBase {
public:
    CAbilityInferno(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID infernalType_ = War3ID("ninf");
    float stunDuration_ = 2.0f;
    float stunRadius_ = 250.0f;
    float summonDuration_ = 180.0f;
};
class CAbilitySilence : public CAbilitySpellBase { public: CAbilitySilence(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 16.0f; float radius_ = 200.0f; };
class CAbilityBlackArrow : public CAbilitySpellBase { public: CAbilityBlackArrow(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float bonusDamage_ = 2.0f; };
class CAbilityLifeDrain : public CAbilitySpellBase { public: CAbilityLifeDrain(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float drainPerSecond_ = 25.0f; float duration_ = 8.0f; };
class CAbilityFingerOfDeath : public CAbilitySpellBase { public: CAbilityFingerOfDeath(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float damage_ = 50000.0f; float heroDamage_ = 500.0f; };
class CAbilityDarkPortal : public CAbilitySpellBase { public: CAbilityDarkPortal(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: War3ID portalUnitType_ = War3ID("nfel"); int summonCount_ = 3; float summonDuration_ = 120.0f; };
class CAbilityRainOfChaos : public CAbilitySpellBase { public: CAbilityRainOfChaos(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float damage_ = 100.0f; float radius_ = 900.0f; int waves_ = 5; };

// === spells3.h ===
class CAbilityMassTeleport : public CAbilitySpellBase { public: CAbilityMassTeleport(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float radius_ = 400.0f; float castingDelay_ = 0.0f; };
class CAbilitySummonWaterElemental : public CAbilitySpellBase { public: CAbilitySummonWaterElemental(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: War3ID elementalType_ = War3ID("hwat"); int count_ = 1; float summonDuration_ = 60.0f; };
class CAbilityResurrect : public CAbilitySpellBase { public: CAbilityResurrect(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; private: float radius_ = 400.0f; int maxTargets_ = 6; };
class CAbilityBlink : public CAbilitySpellBase { public: CAbilityBlink(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float maxRange_ = 1000.0f; };
class CAbilityMirrorImage : public CAbilitySpellBase { public: CAbilityMirrorImage(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: int count_ = 1; float duration_ = 60.0f; };
class CAbilityShockwave : public CAbilitySpellBase { public: CAbilityShockwave(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float damage_ = 100.0f; float range_ = 700.0f; float width_ = 200.0f; };
class CAbilityStarfall : public CAbilitySpellBase { public: CAbilityStarfall(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float damage_ = 50.0f; float radius_ = 400.0f; int waves_ = 8; };
class CAbilityTranquility : public CAbilitySpellBase { public: CAbilityTranquility(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float healPerTick_ = 20.0f; float radius_ = 900.0f; float duration_ = 10.0f; };
class CAbilityReincarnation : public CAbilitySpellBase { public: CAbilityReincarnation(int handleId, War3ID code = War3ID("AOre"), int manaCost = 150); void OnDeath(CSimulation& game, CUnit* unit) override; void OnTick(CSimulation& game, CUnit* unit) override; private: int manaCost_; float reincarnateCooldown_ = 240.0f; float reincarnateRemaining_ = 0.0f; };
class CAbilityCharm : public CAbilitySpellBase { public: CAbilityCharm(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: int maxLevel_ = 5; };

// === spells4.h ===
class CAbilityEvasion : public CAbilitySpellBase { public: CAbilityEvasion(int handleId); void OnAdd(CSimulation& game, CUnit* unit) override; void OnRemove(CSimulation& game, CUnit* unit) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float chance_ = 0.10f; };
class CAbilityCriticalStrike : public CAbilitySpellBase { public: CAbilityCriticalStrike(int handleId); void OnAdd(CSimulation& game, CUnit* unit) override; void OnRemove(CSimulation& game, CUnit* unit) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float chance_ = 0.15f; float multiplier_ = 2.0f; };
class CAbilityBash : public CAbilitySpellBase {
public:
    CAbilityBash(int handleId);
    float GetChance() const { return chance_; }
    float GetDuration() const { return duration_; }
    float GetProcChance() const override { return chance_; }
    float GetProcDuration() const override { return duration_; }
    void SetChance(float v) { chance_ = v; }
    void SetDuration(float v) { duration_ = v; }
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float chance_ = 0.20f;
    float duration_ = 2.0f;
};
class CAbilitySlow : public CAbilitySpellBase { public: CAbilitySlow(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 60.0f; float slowPercent_ = 0.6f; };
class CAbilityFrostArmor : public CAbilitySpellBase { public: CAbilityFrostArmor(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 5.0f; int armorBonus_ = 3; };
class CAbilityFaerieFire : public CAbilitySpellBase { public: CAbilityFaerieFire(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 90.0f; int armorPenalty_ = -4; };
class CAbilityDeathPact : public CAbilitySpellBase { public: CAbilityDeathPact(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; };
class CAbilityAnimateDead : public CAbilitySpellBase { public: CAbilityAnimateDead(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; private: float radius_ = 400.0f; float duration_ = 120.0f; };
class CAbilityDoom : public CAbilitySpellBase { public: CAbilityDoom(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 120.0f; float damagePerSecond_ = 40.0f; };
class CAbilityVolcano : public CAbilitySpellBase { public: CAbilityVolcano(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float damage_ = 100.0f; float radius_ = 500.0f; int waves_ = 6; };

// === spells5.h ===
class CAbilityFlameStrike : public CAbilitySpellBase { public: CAbilityFlameStrike(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; private: float damagePerWave_ = 80.0f; float radius_ = 200.0f; int waves_ = 3; };
class CAbilityDeathAndDecay : public CAbilitySpellBase { public: CAbilityDeathAndDecay(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float damageFraction_ = 0.04f; float radius_ = 300.0f; int waves_ = 4; };
class CAbilityImpale : public CAbilitySpellBase { public: CAbilityImpale(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float damage_ = 80.0f; float range_ = 700.0f; float width_ = 200.0f; float stunDuration_ = 2.0f; };
class CAbilityFanOfKnives : public CAbilitySpellBase { public: CAbilityFanOfKnives(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float damage_ = 100.0f; float radius_ = 300.0f; };
class CAbilityRainOfFire : public CAbilitySpellBase { public: CAbilityRainOfFire(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; private: float damagePerWave_ = 30.0f; float radius_ = 200.0f; int waves_ = 6; };
class CAbilitySpikedCarapace : public CAbilitySpellBase {
public:
    CAbilitySpikedCarapace(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    float GetReflectPercent() const override { return reflectPercent_; }
private:
    int armorBonus_ = 4;
    float reflectPercent_ = 0.25f;
};
class CAbilityDarkRitual : public CAbilitySpellBase { public: CAbilityDarkRitual(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; };
class CAbilityShadowStrike : public CAbilitySpellBase { public: CAbilityShadowStrike(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float damage_ = 50.0f; float slowPercent_ = 0.5f; float slowDuration_ = 4.0f; float poisonPerTick_ = 10.0f; float poisonDuration_ = 10.0f; };

// === spells6.h ===
class CAbilityHealingSpray : public CAbilitySpellBase { public: CAbilityHealingSpray(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; private: float healAmount_ = 100.0f; float radius_ = 200.0f; };
class CAbilityAcidBomb : public CAbilitySpellBase { public: CAbilityAcidBomb(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float duration_ = 15.0f; int armorPenalty_ = -4; float poisonPerTick_ = 8.0f; };
class CAbilityChemicalRage : public CAbilitySpellBase { public: CAbilityChemicalRage(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 20.0f; float speedBonus_ = 0.3f; float attackSpeedBonus_ = 0.4f; };
class CAbilityTransmute : public CAbilitySpellBase { public: CAbilityTransmute(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: int goldYield_ = 200; };
class CAbilityBloodlustCast : public CAbilitySpellBase { public: CAbilityBloodlustCast(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 60.0f; float speedBonus_ = 0.4f; float attackSpeedBonus_ = 0.25f; };
class CAbilityPurge : public CAbilitySpellBase { public: CAbilityPurge(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float slowPercent_ = 0.5f; float slowDuration_ = 4.0f; float summonDamage_ = 200.0f; };
class CAbilityLightningShield : public CAbilitySpellBase { public: CAbilityLightningShield(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float duration_ = 20.0f; float damagePerTick_ = 20.0f; float radius_ = 200.0f; };
class CAbilityRaiseDead : public CAbilitySpellBase { public: CAbilityRaiseDead(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: War3ID skeletonType_ = War3ID("uske"); float radius_ = 500.0f; int maxTargets_ = 2; };

// === spells7.h ===
class CAbilitySerpentWard : public CAbilitySpellBase { public: CAbilitySerpentWard(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; private: War3ID wardType_ = War3ID("osp1"); };
class CAbilityPolymorph : public CAbilitySpellBase { public: CAbilityPolymorph(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 60.0f; };
class CAbilityInnerFire : public CAbilitySpellBase { public: CAbilityInnerFire(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float duration_ = 30.0f; int bonusDamage_ = 5; int bonusArmor_ = 3; };
class CAbilityHeal : public CAbilitySpellBase { public: CAbilityHeal(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float healAmount_ = 200.0f; };
class CAbilityDispelMagic : public CAbilitySpellBase { public: CAbilityDispelMagic(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float radius_ = 250.0f; float summonDamage_ = 250.0f; };
class CAbilityRejuvenation : public CAbilitySpellBase { public: CAbilityRejuvenation(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float duration_ = 12.0f; float healPerTick_ = 10.0f; };
class CAbilityRoar : public CAbilitySpellBase { public: CAbilityRoar(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 20.0f; int bonusDamage_ = 5; float radius_ = 50.0f; };
class CAbilityDefend : public CAbilitySpellBase { public: CAbilityDefend(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 30.0f; };

// === spells8.h ===
class CAbilityItemHealGroup : public CAbilitySpellBase { public: CAbilityItemHealGroup(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; private: float healAmount_ = 150.0f; float radius_ = 300.0f; };
class CAbilityItemTownPortal : public CAbilitySpellBase { public: CAbilityItemTownPortal(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; private: float radius_ = 400.0f; };
class CAbilityItemInvulnerable : public CAbilitySpellBase { public: CAbilityItemInvulnerable(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 5.0f; };
class CAbilityItemHealSelf : public CAbilitySpellBase { public: CAbilityItemHealSelf(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float healAmount_ = 400.0f; };
class CAbilityItemManaSelf : public CAbilitySpellBase { public: CAbilityItemManaSelf(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float manaAmount_ = 200.0f; };
class CAbilityItemClarity : public CAbilitySpellBase { public: CAbilityItemClarity(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 30.0f; float regenBonus_ = 1.0f; };
class CAbilityItemManaSteal : public CAbilitySpellBase { public: CAbilityItemManaSteal(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float stealAmount_ = 100.0f; };
class CAbilityItemProtection : public CAbilitySpellBase { public: CAbilityItemProtection(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 30.0f; int armorBonus_ = 3; };

// === spells9.h ===
class CAbilityOrbOfFrost : public CAbilitySpellBase {
public:
    CAbilityOrbOfFrost(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    float GetSlowPercent() const { return slowPercent_; }
    float GetDuration() const { return duration_; }
private:
    float slowPercent_ = 0.3f;
    float duration_ = 3.0f;
};
class CAbilityOrbOfCorruption : public CAbilitySpellBase {
public:
    CAbilityOrbOfCorruption(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    float GetDuration() const { return duration_; }
    int GetArmorPenalty() const { return armorPenalty_; }
private:
    float duration_ = 5.0f;
    int armorPenalty_ = -3;
};
class CAbilityOrbOfFire : public CAbilitySpellBase {
public:
    CAbilityOrbOfFire(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    float GetBonusDamage() const { return bonusDamage_; }
private:
    float bonusDamage_ = 15.0f;
};
class CAbilityIllusion : public CAbilitySpellBase { public: CAbilityIllusion(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float duration_ = 60.0f; };
class CAbilityLightningShieldWand : public CAbilitySpellBase { public: CAbilityLightningShieldWand(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float duration_ = 20.0f; float damagePerTick_ = 20.0f; float radius_ = 200.0f; };
class CAbilityBeastScroll : public CAbilitySpellBase { public: CAbilityBeastScroll(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 30.0f; int bonusDamage_ = 4; float radius_ = 300.0f; };
class CAbilityHealingWard : public CAbilitySpellBase { public: CAbilityHealingWard(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; private: War3ID wardType_ = War3ID("ohwd"); float healPerTick_ = 10.0f; float auraDuration_ = 120.0f; float radius_ = 200.0f; };

// === spells10.h ===
class CAbilityCleavingAttack : public CAbilitySpellBase {
public:
    CAbilityCleavingAttack(int handleId);
    float GetSplashFraction() const override { return splashFraction_; }
    float GetSplashRadius() const override { return splashRadius_; }
private:
    float splashFraction_ = 0.5f;
    float splashRadius_ = 200.0f;
};
class CAbilitySearingArrows : public CAbilitySpellBase {
public:
    CAbilitySearingArrows(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    float GetBonusDamage() const { return bonusDamage_; }
private:
    float bonusDamage_ = 20.0f;
};
class CAbilityEnsnare : public CAbilitySpellBase { public: CAbilityEnsnare(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float duration_ = 6.0f; };
class CAbilityAncestralSpirit : public CAbilitySpellBase { public: CAbilityAncestralSpirit(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; };
class CAbilityHealingWardUnit : public CAbilitySpellBase { public: CAbilityHealingWardUnit(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; private: War3ID wardType_ = War3ID("ohwd"); float healPerTick_ = 10.0f; float auraDuration_ = 120.0f; float radius_ = 200.0f; };
class CAbilityUnstableConcoction : public CAbilitySpellBase { public: CAbilityUnstableConcoction(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float damage_ = 150.0f; float selfDamage_ = 100.0f; float radius_ = 250.0f; };
class CAbilityAbolishMagic : public CAbilitySpellBase { public: CAbilityAbolishMagic(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; private: float radius_ = 200.0f; };
class CAbilityClusterRockets : public CAbilitySpellBase { public: CAbilityClusterRockets(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; private: float damage_ = 75.0f; float radius_ = 150.0f; };

// === spells11.h ===
class CAbilityWindWalk : public CAbilitySpellBase { public: CAbilityWindWalk(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 30.0f; float backstabDamage_ = 40.0f; };
class CAbilityInvisibility : public CAbilitySpellBase { public: CAbilityInvisibility(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float duration_ = 20.0f; };
class CAbilityPermanentInvisibility : public CAbilitySpellBase { public: CAbilityPermanentInvisibility(int handleId); void OnAdd(CSimulation& game, CUnit* unit) override; void OnRemove(CSimulation& game, CUnit* unit) override; };
class CAbilityShadowmeld : public CAbilitySpellBase { public: CAbilityShadowmeld(int handleId); void OnAdd(CSimulation& game, CUnit* unit) override; void OnRemove(CSimulation& game, CUnit* unit) override; void OnTick(CSimulation& game, CUnit* unit) override; private: float lastX_ = 0.0f; float lastY_ = 0.0f; bool tracking_ = false; };
class CAbilityDustOfAppearance : public CAbilitySpellBase { public: CAbilityDustOfAppearance(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; private: float duration_ = 12.0f; float radius_ = 300.0f; };
class CAbilityBerserk : public CAbilitySpellBase { public: CAbilityBerserk(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 20.0f; float attackSpeedMul_ = 1.4f; float damageTakenMul_ = 1.25f; };
class CAbilitySummonBear : public CAbilitySpellBase { public: CAbilitySummonBear(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: War3ID bearType_ = War3ID("ngz1"); };
class CAbilityBattleRoar : public CAbilitySpellBase { public: CAbilityBattleRoar(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 30.0f; int bonusDamage_ = 6; float radius_ = 650.0f; };

// === spells12.h ===
class CAbilityTrueSight : public CAbilitySpellBase { public: CAbilityTrueSight(int handleId); void OnAdd(CSimulation& game, CUnit* unit) override; void OnRemove(CSimulation& game, CUnit* unit) override; };
class CAbilitySentryWard : public CAbilitySpellBase { public: CAbilitySentryWard(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; private: War3ID wardType_ = War3ID("oeye"); };
class CAbilityFlare : public CAbilitySpellBase { public: CAbilityFlare(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; private: float duration_ = 15.0f; float radius_ = 400.0f; };
class CAbilityBurrow : public CAbilitySpellBase { public: CAbilityBurrow(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 120.0f; };
class CAbilityCannibalize : public CAbilitySpellBase { public: CAbilityCannibalize(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float healAmount_ = 200.0f; };
class CAbilityCarrionBeetles : public CAbilitySpellBase { public: CAbilityCarrionBeetles(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: War3ID beetleType_ = War3ID("ucry"); int count_ = 2; };
class CAbilityWeb : public CAbilitySpellBase { public: CAbilityWeb(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float duration_ = 6.0f; };

// === spells13.h ===
class CAbilityMetamorphosis : public CAbilitySpellBase { public: CAbilityMetamorphosis(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 60.0f; };
class CAbilityBearForm : public CAbilitySpellBase { public: CAbilityBearForm(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 60.0f; };
class CAbilityRavenForm : public CAbilitySpellBase { public: CAbilityRavenForm(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 60.0f; };
class CAbilityRoboGoblin : public CAbilitySpellBase { public: CAbilityRoboGoblin(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 60.0f; };
class CAbilityStoneForm : public CAbilitySpellBase { public: CAbilityStoneForm(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 30.0f; };
class CAbilityAvatar : public CAbilitySpellBase { public: CAbilityAvatar(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 60.0f; int bonusDamage_ = 20; int bonusLife_ = 500; };
class CAbilitySiphonMana : public CAbilitySpellBase { public: CAbilitySiphonMana(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float duration_ = 6.0f; float manaPerTick_ = 15.0f; };
class CAbilityBigBadVoodoo : public CAbilitySpellBase { public: CAbilityBigBadVoodoo(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 30.0f; float radius_ = 800.0f; };

// === spells14.h ===
class CAbilityPocketFactory : public CAbilitySpellBase {
public:
    CAbilityPocketFactory(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID factoryType_ = War3ID("nfac");
    War3ID goblinType_ = War3ID("ncgb");
    float factoryLife_ = 40.0f;
    float spawnInterval_ = 12.0f;
    float explodeDamage_ = 50.0f;
    float explodeRadius_ = 150.0f;
};
class CAbilityLavaSpawn : public CAbilitySpellBase {
public:
    CAbilityLavaSpawn(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID spawnType_ = War3ID("nlv1");
    War3ID splitType_ = War3ID("nlv2");
    int splitCount_ = 2;
    War3ID childType_ = War3ID("nlv3");
    int childCount_ = 2;
    float summonDuration_ = 70.0f;
};
class CAbilitySoulBurn : public CAbilitySpellBase { public: CAbilitySoulBurn(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float damage_ = 100.0f; float burnPerTick_ = 10.0f; float burnDuration_ = 8.0f; };
class CAbilityQuilbeast : public CAbilitySpellBase { public: CAbilityQuilbeast(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: War3ID beastType_ = War3ID("nqb1"); float summonDuration_ = 70.0f; };
class CAbilityFrostBreath : public CAbilitySpellBase { public: CAbilityFrostBreath(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float damage_ = 275.0f; float radius_ = 100.0f; float slowPercent_ = 0.5f; float slowDuration_ = 4.0f; };
class CAbilityPulverize : public CAbilitySpellBase {
public:
    CAbilityPulverize(int handleId);
    float GetChance() const { return chance_; }
    float GetRadius() const { return radius_; }
    float GetProcChance() const override { return chance_; }
    float GetSplashRadius() const override { return radius_; }
    void SetChance(float v) { chance_ = v; }
private:
    float chance_ = 0.25f;
    float radius_ = 200.0f;
};
class CAbilityForkedLightning : public CAbilitySpellBase {
public:
    CAbilityForkedLightning(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 85.0f;
    int bounces_ = 3;
    float bounceRange_ = 500.0f;
    float damageReduction_ = 0.25f;
};

// === spells15.h ===
class CAbilityPhoenix : public CAbilitySpellBase {
public:
    CAbilityPhoenix(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
    float GetHatchDelay() const { return hatchDelay_; }
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID phoenixType_ = War3ID("hphx");
    float hatchDelay_ = 10.0f;
};

// === spells16.h ===
class CAbilityDevour : public CAbilitySpellBase { public: CAbilityDevour(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float digestPerTick_ = 5.0f; float digestDuration_ = 3600.0f; };
class CAbilityDevourMagic : public CAbilitySpellBase { public: CAbilityDevourMagic(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float manaPerBuff_ = 50.0f; };
class CAbilityColdArrows : public CAbilitySpellBase {
public:
    CAbilityColdArrows(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    float GetSlowPercent() const { return slowPercent_; }
    float GetSlowDuration() const { return slowDuration_; }
    float GetPoisonPerTick() const { return poisonPerTick_; }
    float GetPoisonDuration() const { return poisonDuration_; }
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float slowPercent_ = 0.3f;
    float slowDuration_ = 3.0f;
    float poisonPerTick_ = 5.0f;
    float poisonDuration_ = 6.0f;
};
class CAbilityMonsoon : public CAbilitySpellBase { public: CAbilityMonsoon(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float damagePerWave_ = 50.0f; float radius_ = 300.0f; int waves_ = 4; };
class CAbilityAbsorbMana : public CAbilitySpellBase { public: CAbilityAbsorbMana(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float manaGain_ = 100.0f; };
class CAbilityUnholyFrenzy : public CAbilitySpellBase { public: CAbilityUnholyFrenzy(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float duration_ = 30.0f; float attackSpeedMul_ = 1.5f; };

// === spells17.h ===
class CAbilityHowlOfTerror : public CAbilitySpellBase { public: CAbilityHowlOfTerror(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 15.0f; float radius_ = 500.0f; float damageFactor_ = 0.7f; };
class CAbilityBreathOfFire : public CAbilitySpellBase {
public:
    CAbilityBreathOfFire(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 65.0f;
    float radius_ = 125.0f;
    float burnPerTick_ = 7.0f;
    float burnDuration_ = 5.0f;
};
class CAbilityDrunkenHaze : public CAbilitySpellBase {
public:
    CAbilityDrunkenHaze(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float duration_ = 12.0f;
    float radius_ = 200.0f;
    float missChance_ = 0.45f;
    float slowPercent_ = 0.15f;
};
class CAbilityManaShield : public CAbilitySpellBase { public: CAbilityManaShield(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; private: float duration_ = 60.0f; };
class CAbilityLocustSwarm : public CAbilitySpellBase { public: CAbilityLocustSwarm(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float damagePerWave_ = 20.0f; float radius_ = 300.0f; int waves_ = 6; };
class CAbilityCyclone : public CAbilitySpellBase { public: CAbilityCyclone(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 20.0f; };
class CAbilityVengeance : public CAbilitySpellBase { public: CAbilityVengeance(int handleId); void OnDeath(CSimulation& game, CUnit* unit) override; private: War3ID spiritType_ = War3ID("even"); };

// === spells18.h ===
class CAbilityWhirlwind : public CAbilitySpellBase { public: CAbilityWhirlwind(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float damagePerSecond_ = 110.0f; float radius_ = 250.0f; float duration_ = 6.0f; };
class CAbilityImmolation : public CAbilitySpellBase {
public:
    CAbilityImmolation(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damagePerTick_ = 10.0f;
    float manaDrain_ = 7.0f;
    float radius_ = 250.0f;
    float duration_ = 60.0f;
};
class CAbilityForceOfNature : public CAbilitySpellBase { public: CAbilityForceOfNature(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: int count_ = 2; War3ID treantType_ = War3ID("etrp"); float summonDuration_ = 120.0f; };
class CAbilityOwlScout : public CAbilitySpellBase { public: CAbilityOwlScout(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: War3ID owlType_ = War3ID("nef1"); float summonDuration_ = 90.0f; };
class CAbilitySummonHawk : public CAbilitySpellBase { public: CAbilitySummonHawk(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: War3ID hawkType_ = War3ID("nehw"); float summonDuration_ = 70.0f; };
class CAbilityFactory : public CAbilitySpellBase {
public:
    CAbilityFactory(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID factoryType_ = War3ID("nfac");
    War3ID goblinType_ = War3ID("ncgb");
    float factoryLife_ = 60.0f;
    float spawnInterval_ = 8.0f;
    float explodeDamage_ = 75.0f;
    float explodeRadius_ = 200.0f;
};
class CAbilityKaboom : public CAbilitySpellBase {
public:
    CAbilityKaboom(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
    void OnDeath(CSimulation& game, CUnit* unit) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    void Explode(CSimulation& game, CUnit* caster);
    bool armed_ = false;
    float fullRadius_ = 150.0f;
    float fullDamage_ = 300.0f;
    float partialRadius_ = 250.0f;
    float partialDamage_ = 200.0f;
    float buildingFactor_ = 1.5f;
    bool explodesOnDeath_ = true;
};
class CAbilityPhoenixFire : public CAbilitySpellBase {
public:
    CAbilityPhoenixFire(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float initialDamage_ = 10.0f;
    float damagePerSecond_ = 10.0f;
    float radius_ = 250.0f;
};

// === spells19.h ===
class CAbilityBanish : public CAbilitySpellBase { public: CAbilityBanish(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 12.0f; };
class CAbilityAntiMagicShell : public CAbilitySpellBase { public: CAbilityAntiMagicShell(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 60.0f; };

// === spells20.h ===
class CAbilityPossession : public CAbilitySpellBase { public: CAbilityPossession(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: int maxLevel_ = 99; };
class CAbilityStampede : public CAbilitySpellBase {
public:
    CAbilityStampede(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 120.0f;
    float range_ = 800.0f;
    float width_ = 200.0f;
    float slowDuration_ = 2.0f;
};

// === spells21.h ===
class CAbilityStasisTrap : public CAbilitySpellBase { public: CAbilityStasisTrap(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float radius_ = 200.0f; float stunDuration_ = 8.0f; float lifetime_ = 120.0f; };
class CAbilityTornado : public CAbilitySpellBase {
public:
    CAbilityTornado(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    int count_ = 3;
    float radius_ = 150.0f;
    float damage_ = 30.0f;
    float duration_ = 20.0f;
    float speed_ = 150.0f;
};

// === spells22.h ===
class CAbility;
std::unique_ptr<CAbility> CreateSpells22Alias(int handleId, War3ID code);

// === spells23.h ===
class CAbilityChainDispel : public CAbilitySpellBase { public: CAbilityChainDispel(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: int bounces_ = 4; float range_ = 500.0f; };
class CAbilityTaunt : public CAbilitySpellBase { public: CAbilityTaunt(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float radius_ = 500.0f; };

// === spells24.h ===
class CAbilitySelfStatPassive : public CAbility { public: CAbilitySelfStatPassive(int handleId, War3ID code, CAuraEffectType type, float value, bool percent); void OnAdd(CSimulation& game, CUnit* unit) override; void OnRemove(CSimulation& game, CUnit* unit) override; private: CAuraEffectType type_; float value_; bool percent_; };
class CAbilityItemAttackSpeed : public CAbilitySelfStatPassive { public: explicit CAbilityItemAttackSpeed(int handleId); };
class CAbilityItemManaRegen : public CAbilitySelfStatPassive { public: explicit CAbilityItemManaRegen(int handleId); };
class CAbilityItemMoveSpeed : public CAbilitySelfStatPassive { public: explicit CAbilityItemMoveSpeed(int handleId); };
class CAbilityItemVampirism : public CAbility { public: explicit CAbilityItemVampirism(int handleId); void OnAdd(CSimulation& game, CUnit* unit) override; void OnRemove(CSimulation& game, CUnit* unit) override; };

// === spells25.h ===
class CAbilityCurse : public CAbilitySpellBase { public: CAbilityCurse(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float duration_ = 15.0f; float missChance_ = 0.33f; };
class CAbilityMoonWell : public CAbilitySpellBase { public: CAbilityMoonWell(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float manaPerMana_ = 1.0f; float manaPerLife_ = 4.0f; };
class CAbilityEatTree : public CAbilitySpellBase { public: CAbilityEatTree(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float hitPointsGained_ = 100.0f; };
class CAbilitySacrifice : public CAbilitySpellBase { public: CAbilitySacrifice(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; };
class CAbilityRequestSacrifice : public CAbilitySpellBase { public: CAbilityRequestSacrifice(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; };
class CAbilityHide : public CAbilitySpellBase { public: CAbilityHide(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; };
class CAbilityEthereal : public CAbility { public: explicit CAbilityEthereal(int handleId); void OnAdd(CSimulation& game, CUnit* unit) override; void OnRemove(CSimulation& game, CUnit* unit) override; private: float savedCollision_ = 0.0f; bool applied_ = false; };
class CAbilityChestOfGold : public CAbilitySpellBase { public: CAbilityChestOfGold(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: int gold_ = 250; };
class CAbilityItemStatBonus : public CAbilitySpellBase { public: enum class StatSlot : uint8_t { NONE = 0, DAMAGE, ARMOR, MAX_LIFE, MAX_MANA };
    CAbilityItemStatBonus(int handleId, War3ID code, StatSlot slot = StatSlot::NONE, int defaultBonus = 0);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
    StatSlot slot_ = StatSlot::NONE;
    int bonus_ = 0;
    int applied_ = 0;
};
class CAbilityItemDamageBonus : public CAbilityItemStatBonus {
public:
    explicit CAbilityItemDamageBonus(int handleId)
        : CAbilityItemStatBonus(handleId, War3ID("AIat"), StatSlot::DAMAGE, 3) {}
};
class CAbilityItemArmorBonus : public CAbilityItemStatBonus {
public:
    explicit CAbilityItemArmorBonus(int handleId)
        : CAbilityItemStatBonus(handleId, War3ID("AIde"), StatSlot::ARMOR, 2) {}
};
class CAbilityItemLifeBonus : public CAbilityItemStatBonus {
public:
    explicit CAbilityItemLifeBonus(int handleId)
        : CAbilityItemStatBonus(handleId, War3ID("AIml"), StatSlot::MAX_LIFE, 150) {}
};
class CAbilityItemManaPoolBonus : public CAbilityItemStatBonus {
public:
    explicit CAbilityItemManaPoolBonus(int handleId)
        : CAbilityItemStatBonus(handleId, War3ID("AImm"), StatSlot::MAX_MANA, 100) {}
};
class CAbilityItemAttributeBonus : public CAbilityItemStatBonus {
public:
    explicit CAbilityItemAttributeBonus(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    int agility_ = 1;
    int intelligence_ = 1;
    int strength_ = 1;
    int appliedAgi_ = 0;
    int appliedInt_ = 0;
    int appliedStr_ = 0;
};

// === spells26.h ===
class CAbilitySpellShieldPassive : public CAbility {
public:
    explicit CAbilitySpellShieldPassive(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    void OnTick(CSimulation& game, CUnit* unit) override;
    float GetCooldown() const { return cooldown_; }
private:
    void Arm(CSimulation& game, CUnit* unit);
    float cooldown_ = 40.0f;
    float remaining_ = 0.0f;
    bool armed_ = false;
};
class CAbilitySpellShieldArea : public CAbilitySpellBase { public: CAbilitySpellShieldArea(int handleId); void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float radius_ = 300.0f; float duration_ = 15.0f; };
class CAbilityItemPermanentStats : public CAbilitySpellBase { public: CAbilityItemPermanentStats(int handleId, War3ID code, int agility, int intelligence, int strength); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: int agility_; int intelligence_; int strength_; };
class CAbilityItemPermanentLife : public CAbilitySpellBase { public: explicit CAbilityItemPermanentLife(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: int life_ = 50; };
class CAbilityItemExperience : public CAbilitySpellBase { public: explicit CAbilityItemExperience(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: int xp_ = 100; };
class CAbilityItemLevelGain : public CAbilitySpellBase { public: explicit CAbilityItemLevelGain(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: int levels_ = 1; };
class CAbilityItemFigurine : public CAbilitySpellBase { public: explicit CAbilityItemFigurine(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: War3ID summonId_ = War3ID("nwwf"); int count_ = 1; float duration_ = 60.0f; };
class CAbilityPermanentInvulnerable : public CAbility { public: explicit CAbilityPermanentInvulnerable(int handleId); void OnAdd(CSimulation& game, CUnit* unit) override; void OnRemove(CSimulation& game, CUnit* unit) override; };
class CAbilityChangeTimeOfDay : public CAbilitySpellBase { public: explicit CAbilityChangeTimeOfDay(int handleId); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: int hour_ = 0; int minute_ = 0; };
class CAbilityToggleForm : public CAbilitySpellBase { public: CAbilityToggleForm(int handleId, War3ID code); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: War3ID formId_; };
class CAbilityChaos : public CAbilitySpellBase { public: explicit CAbilityChaos(int handleId); void OnAdd(CSimulation& game, CUnit* unit) override; void OnRemove(CSimulation& game, CUnit* unit) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: War3ID formId_; };

// === spells27.h ===
enum class CPassiveState {
    Ethereal,
    MagicImmune,
    Resistant,
    Ghost
};
class CAbilityStatePassive : public CAbility { public: CAbilityStatePassive(int handleId, War3ID code, CPassiveState state); void OnAdd(CSimulation& game, CUnit* unit) override; void OnRemove(CSimulation& game, CUnit* unit) override; private: CPassiveState state_; float savedCollision_ = 0.0f; bool applied_ = false; };
class CAbilityItemSpellResist : public CAbilitySpellBase {
public:
    explicit CAbilityItemSpellResist(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float attackBonus_ = 0.0f;
    float spellReduction_ = 0.33f;
    int appliedAttack_ = 0;
};
class CAbilityItemMixedDefense : public CAbilitySpellBase {
public:
    explicit CAbilityItemMixedDefense(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float pierceTaken_ = 0.80f;
    float dealtAdd_ = 0.20f;
    float spellTaken_ = 0.80f;
};
class CAbilityMagicDefense : public CAbilitySpellBase {
public:
    explicit CAbilityMagicDefense(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    void TurnOff(CSimulation& game, CUnit* unit);
    float speedPenalty_ = 0.50f;
    float appliedFactor_ = 1.0f;
    bool active_ = false;
};
class CAbilityMagicLeash : public CAbilitySpellBase { public: explicit CAbilityMagicLeash(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: float damagePerSecond_ = 10.0f; float duration_ = 10.0f; };
class CAbilityReplenish : public CAbilitySpellBase { public: CAbilityReplenish(int handleId, War3ID code, bool restoreLife); void DoEffect(CSimulation& game, CUnit* caster) override; protected: void ApplyEffectData(const CAbilityDefFull& def, int level) override; private: bool restoreLife_; float amount_ = 10.0f; float radius_ = 400.0f; };
class CAbilitySphere : public CAbility { public: explicit CAbilitySphere(int handleId); };

// === spells28.h ===
enum class CPairMode {
    MilitiaCall,
    Militia,
    Mount,
    LoadPartner,
    Couple
};
class CAbilityPairing : public CAbilitySpellBase {
public:
    CAbilityPairing(int handleId, War3ID code, CPairMode mode);
    void DoEffect(CSimulation& game, CUnit* caster) override;
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
    bool Convert(CSimulation& game, CUnit* self) override;
    bool IsConverted() const { return converted_; }
    void SetConverted(bool v) override { converted_ = v; }
    void SetMilitiaDuration(float seconds) { militiaDuration_ = seconds; }
    float GetMilitiaDuration() const { return militiaDuration_; }
    CPairMode GetMode() const { return mode_; }
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    CUnit* FindPartner(CSimulation& game, CUnit* self) const;
    void MorphIfKnown(CSimulation& game, CUnit* unit, War3ID formId);
    CPairMode mode_;
    War3ID partnerCode_;
    War3ID resultId_;
    float radius_ = 512.0f;
    bool converted_ = false;
    float militiaDuration_ = 0.0f;
};
class CAbilityBlight : public CAbility {
public:
    explicit CAbilityBlight(int handleId);
    void OnTick(CSimulation& game, CUnit* unit) override;
    float GetCurrentRadius() const { return current_; }
private:
    float area_ = 768.0f;
    float expansion_ = 64.0f;
    float interval_ = 1.0f;
    float current_ = 0.0f;
    float accum_ = 0.0f;
    bool creates_ = true;
};
class CAbilityBlightedGoldMine : public CAbility {
public:
    explicit CAbilityBlightedGoldMine(int handleId);
    void OnTick(CSimulation& game, CUnit* unit) override;
    bool IsResourceMine() const override { return true; }
    bool CanAddMiner() const override { return miners_ < maxMiners_; }
    bool AddMiner() override;
    void RemoveMiner() override;
    int GetGold() const override { return gold_; }
    void SetGold(int v) override { gold_ = v; }
    int GetMinerCount() const { return miners_; }
    bool IsActivelyWorking() const override { return miners_ > 0 && gold_ > 0; }
private:
    int gold_ = 12500;
    int goldPerInterval_ = 10;
    float interval_ = 1.0f;
    int maxMiners_ = 5;
    int miners_ = 0;
    float accum_ = 0.0f;
};
class CAbilityAcolyteHarvest : public CAbilitySpellBase {
public:
    explicit CAbilityAcolyteHarvest(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
    void OnDeath(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    bool IsBound() const { return mineUnitHandle_ >= 0; }
    int GetBoundMineHandle() const { return mineUnitHandle_; }
private:
    void Unbind(CSimulation& game);
    int mineUnitHandle_ = -1;
};
class CAbilityWispHarvest : public CAbilitySpellBase {
public:
    explicit CAbilityWispHarvest(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
    void OnTick(CSimulation& game, CUnit* unit) override;
    int GetBoundTreeHandle() const { return treeHandle_; }
private:
    int treeHandle_ = -1;
    int lumberPerInterval_ = 5;
    float damagePerInterval_ = 5.0f;
    float interval_ = 1.0f;
    float accum_ = 0.0f;
};
class CAbilityHarvestLumber : public CAbilitySpellBase { public: explicit CAbilityHarvestLumber(int handleId); void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override; private: float damageToTree_ = 10.0f; int capacity_ = 10; };
class CAbilityCargoHold : public CAbility {
public:
    CAbilityCargoHold(int handleId, War3ID code, bool burrow, int capacity = 4);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    bool TryLoad(CUnit* holder, CUnit* passenger) override;
    void UnloadAll(CUnit* holder) override;
    int GetCapacity() const { return capacity_; }
    void SetCapacity(int v) { if (v > 0) capacity_ = v; }
    int GetCargoCapacity() const override { return capacity_; }
    void SetCargoCapacity(int v) override { SetCapacity(v); }
    bool IsBurrow() const { return burrow_; }
private:
    bool burrow_;
    int capacity_ = 4;
    int speedSteps_ = 0;
};
class CAbilityDropInstant : public CAbilitySpellBase { public: CAbilityDropInstant(int handleId, War3ID code); void DoEffect(CSimulation& game, CUnit* caster) override; };
class CAbilityWayGate : public CAbility {
public:
    explicit CAbilityWayGate(int handleId);
    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    bool CheckBeforeQueue(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;
    void SetDestination(CUnit* gate, float x, float y);
    bool TryTeleport(CUnit* gate, CUnit* target) const;
    bool IsEnabled() const { return enabled_; }
    float GetWidth() const { return width_; }
    float GetHeight() const { return height_; }
private:
    bool Inside(const CUnit* gate, const CUnit* target) const;
    float width_ = 256.0f;
    float height_ = 256.0f;
    float destX_ = 0.0f;
    float destY_ = 0.0f;
    bool enabled_ = false;
};
class CAbilityShopPurchase : public CAbility { public: explicit CAbilityShopPurchase(int handleId); };
class CAbilityNeutralBuilding : public CAbility {
public:
    static constexpr int kPlayerSlots = 12;
    CAbilityNeutralBuilding(int handleId, War3ID code, bool onlyAllies);
    void OnTick(CSimulation& game, CUnit* unit) override;
    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;
    CUnit* GetActiveUnit() const;
    CUnit* GetSelectedUnit(int playerIndex) const;
    bool OnlyAllies() const { return onlyAllies_; }
private:
    bool CanSelect(const CSimulation& game, const CUnit* shop, const CUnit* other) const;
    void Rescan(CSimulation& game, CUnit* shop);
    CUnit* shopper_[kPlayerSlots] = {};
    bool HireFromTavern(CSimulation& game, CUnit* tavern, CUnit* hero);
    bool onlyAllies_;
    float radius_ = 400.0f;
    float interval_ = 3.0f;
    float accum_ = 0.0f;
    bool primed_ = false;
    CUnit* selected_[kPlayerSlots] = {};
};

// === spells29.h ===
class CAbilityDrunkenBrawler : public CAbilitySpellBase {
public:
    explicit CAbilityDrunkenBrawler(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float critChance_ = 0.15f;
    float critMultiplier_ = 2.0f;
    float dodge_ = 0.10f;
};
class CAbilityEntangleGoldMine : public CAbilitySpellBase {
public:
    explicit CAbilityEntangleGoldMine(int handleId);
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
    void OnTick(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    CUnit* GetReplacement() const { return replacement_; }
    CUnit* GetOriginal() const { return original_; }
    bool IsConstructing() const { return constructing_; }
private:
    void Release(CSimulation& game, bool killReplacement);
    void AdvanceConstruction(CSimulation& game);
    War3ID resultId_ = War3ID("egol");
    CUnit* original_ = nullptr;
    CUnit* replacement_ = nullptr;
    bool instant_ = true;
    bool constructing_ = false;
};
class CAbilityEntangledMine : public CAbility {
public:
    explicit CAbilityEntangledMine(int handleId);
    void OnTick(CSimulation& game, CUnit* unit) override;
    void SetParent(CUnit* parent) { parent_ = parent; }
    CUnit* GetParent() const { return parent_; }
    int GetGoldPerInterval() const { return goldPerInterval_; }
private:
    CUnit* parent_ = nullptr;
    int goldPerInterval_ = 10;
    float interval_ = 1.0f;
    float accum_ = 0.0f;
    int index_ = 0;
};
class CAbilityChannelTest : public CAbilitySpellBase {
public:
    explicit CAbilityChannelTest(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
    void OnTick(CSimulation& game, CUnit* unit) override;
    bool IsChanneling() const { return channeling_; }
    float GetRemaining() const { return remaining_; }
private:
    float artDuration_ = 2.0f;
    float remaining_ = 0.0f;
    bool channeling_ = false;
};
