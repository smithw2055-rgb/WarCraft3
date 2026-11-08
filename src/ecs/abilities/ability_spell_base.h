#pragma once
#include <functional>
#include "ecs/ability.h"
#include "ecs/unit_attack.h"
#include "ecs/abilities/ability_builder.h"

struct CAbilityDefFull;

// ============================================================
// CAbilitySpellBase - base class for all spell abilities
//
// Phase 17–19 Modernization:
// - Implements CAbility virtual casting/cooldown/target-type contract so
//   CBehaviorSpell, MainUI, and JASS natives never depend on CAbilitySpellBase.
// - Eliminates duplicate `level_` field shadowing `CAbility::level_` and packs
//   `maxLevel_` (`int16_t`) beside `targetType_`, reducing `sizeof(CAbilitySpellBase)`
//   from 72 bytes down to 56 bytes (22.2% reduction).
// ============================================================
class CAbilitySpellBase : public CAbility {
public:
    CAbilitySpellBase(int handleId, War3ID code, CAbilityCategory category);

    // Configuration (implements CAbility virtual spell capability contract)
    bool IsSpellAbility() const override { return true; }
    CSpellTargetType GetTargetType() const override { return targetType_; }
    void SetTargetType(CSpellTargetType v) override { targetType_ = v; }
    float GetCastRange() const override { return castRange_; }
    void SetCastRange(float v) override { castRange_ = v; }
    float GetCastPoint() const override { return castPoint_; }
    void SetCastPoint(float v) override { castPoint_ = v; }
    float GetCooldown() const override { return cooldown_; }
    void SetCooldown(float v) override { cooldown_ = v; }
    int GetManaCost() const override { return manaCost_; }
    void SetManaCost(int v) override { manaCost_ = v; }
    int GetLevel() const override { return level_; }
    void SetLevel(int v) override;
    int GetMaxLevel() const { return maxLevel_; }
    void SetMaxLevel(int v) { maxLevel_ = static_cast<int16_t>(v); }
    void ApplyData(const CAbilityDefFull& def) override;

    // Order validation
    int GetBaseOrderId() const override;
    bool CheckCanUse(CSimulation& game, CUnit* unit, int orderId) const override;
    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    bool CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const override;
    bool CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const override;

    // Begin casting
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) override;
    CBehavior* BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) override;

    // The actual spell effect - defaults to dispatching any registered
    // ABEffectChain in AbilityBuilderRegistry::Instance(), or overridden in subclasses.
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
    void DoEffect(CSimulation& game, CUnit* caster) override;

    // Cooldown state
    bool IsOnCooldown() const override { return cooldownRemaining_ > 0; }
    void StartCooldown() override { cooldownRemaining_ = cooldown_; }
    void ResetCooldown() override { cooldownRemaining_ = 0.0f; }
    void UpdateCooldown(float dt) override { if (cooldownRemaining_ > 0) cooldownRemaining_ -= dt; }
    float GetCooldownRemaining() const override { return cooldownRemaining_; }

    // ===== Phase 17–18 Declarative Spell Effect Primitives =====
    static void CastBuffOnTarget(CSimulation& game, CUnit* caster, CWidget* target,
                                 ABActionAddBuffTarget::BuffFactory factory);
    static void CastBuffOnSelf(CSimulation& game, CUnit* caster,
                               ABActionAddBuffTarget::BuffFactory factory);
    static void CastPointDamageWaves(CSimulation& game, CUnit* caster, float x, float y,
                                     int waves, float radius, float dmgPerWave,
                                     CAttackType atkType, CDamageType dmgType,
                                     ABFactionFilter filter = ABFactionFilter::ANY);
    static void CastCasterAreaDamageWaves(CSimulation& game, CUnit* caster,
                                          int waves, float radius, float dmgPerWave,
                                          CAttackType atkType, CDamageType dmgType,
                                          ABFactionFilter filter = ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    static void CastPointAreaBuff(CSimulation& game, CUnit* caster, float x, float y,
                                  float radius, ABActionAddBuffTarget::BuffFactory factory,
                                  ABFactionFilter filter = ABFactionFilter::ANY,
                                  bool includeCaster = false);
    static void CastCasterAreaBuff(CSimulation& game, CUnit* caster,
                                   float radius, ABActionAddBuffTarget::BuffFactory factory,
                                   ABFactionFilter filter = ABFactionFilter::SAME_PLAYER_AS_CASTER,
                                   bool includeCaster = false);
    static void CastLineDamageAndStun(CSimulation& game, CUnit* caster,
                                      CWidget* unitTarget, bool hasPoint, float x, float y,
                                      float range, float width, float damage, float stunDuration,
                                      CAttackType atkType = CAttackType::SPELLS,
                                      CDamageType dmgType = CDamageType::NORMAL,
                                      ABFactionFilter filter = ABFactionFilter::ANY);
    static void CastTimedSpawnerUnit(CSimulation& game, CUnit* caster, float x, float y,
                                     War3ID unitType, War3ID buffCode, float lifeDuration,
                                     War3ID spawnType, float spawnInterval,
                                     float boomDamage, float boomRadius);
    static void CastPointAreaHeal(CSimulation& game, CUnit* caster, float x, float y,
                                  float radius, float amount,
                                  ABFactionFilter filter = ABFactionFilter::SAME_PLAYER_AS_CASTER,
                                  bool includeCaster = true);
    static void ForEachPassiveAllyInRange(CSimulation& game, CUnit* caster,
                                          float cx, float cy, float radius,
                                          bool includeCaster,
                                          const std::function<void(CUnit*)>& fn);
    static bool IsFriendlyToPlayer(CSimulation& game, int playerIndex, const CUnit* other);

protected:
    virtual void ApplyEffectData(const CAbilityDefFull& def, int level);

    CSpellTargetType targetType_ = CSpellTargetType::NO_TARGET;
    int16_t maxLevel_ = 3;
    float castRange_ = 600.0f;
    float castPoint_ = 0.3f;
    float cooldown_ = 0;
    int manaCost_ = 0;
    float cooldownRemaining_ = 0;
    const CAbilityDefFull* dataDef_ = nullptr;
};

// Shared attack modifier proc for flat bonus elemental spell damage on weapon hit
// (unifies Orb of Fire `AIfb` and Searing Arrows `AHfa`).
class CAttackBonusDamageEffect : public CAttackModifierEffect {
public:
    explicit CAttackBonusDamageEffect(float bonusDamage,
                                      CDamageType damageType = CDamageType::FIRE)
        : bonusDamage_(bonusDamage), damageType_(damageType) {}
    void OnAttackLanded(CSimulation& game, CUnit* attacker, CUnit* victim,
                        float damageDealt) override;
private:
    float bonusDamage_;
    CDamageType damageType_;
};

// ============================================================
// CAbilityDataDrivenSpell - Unity ScriptableObject / Godot Resource
// style data-driven spell implementation.
// ============================================================
class CAbilityDataDrivenSpell : public CAbilitySpellBase {
public:
    CAbilityDataDrivenSpell(int handleId, War3ID code, const ABSpellDescriptor& desc);

    const ABSpellDescriptor& GetDescriptor() const { return desc_; }
    const ABSpellEffectContext& GetEffectContext() const { return ctx_; }
    int GetBaseOrderId() const override;
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
    void DoEffect(CSimulation& game, CUnit* caster) override;

protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;

private:
    ABSpellDescriptor desc_;
    ABSpellEffectContext ctx_;
};

// ============================================================
// Phase 30 (Track B): Compact Table-Driven Spell Archetype Rule
// Eliminates repetitive constructor/ApplyEffectData/DoEffect boilerplate
// across 150+ Warcraft 3 active/item spell subclasses.
// ============================================================
enum class SpellArchetypeEffectKind : uint8_t {
    CUSTOM_CALLBACK = 0,
    POINT_WAVE_DAMAGE,          // Blizzard, FlameStrike, RainOfFire, Monsoon
    CASTER_WAVE_DAMAGE,         // FanOfKnives, LocustSwarm
    LINE_DAMAGE_STUN,           // CarrionSwarm, Shockwave, Impale, Stampede
    POINT_AREA_HEAL,            // HealingSpray, ItemHealGroup
    SELF_HEAL,                  // ItemHealSelf
    SELF_MANA,                  // ItemManaSelf
    TARGET_HEAL,                // Heal
    TARGET_MANA_BURN,           // ManaBurn
    TARGET_MANA_STEAL,          // ItemManaSteal
    SELF_DIVINE_SHIELD,         // DivineShield, ItemInvulnerable
    SELF_STONE_FORM,            // StoneForm
    SELF_BURROW,                // Burrow
    SELF_DEFEND,                // Defend
    SELF_MANA_SHIELD,           // ManaShield
    SELF_IMMOLATION,            // Immolation
    SELF_WHIRLWIND,             // Whirlwind
    SELF_WIND_WALK,             // WindWalk
    SELF_BERSERK,               // Berserk
    SELF_AVATAR,                // Avatar
    SELF_CHEMICAL_RAGE,         // ChemicalRage
    SELF_CLARITY,               // ItemClarity
    SELF_ARMOR_BUFF,            // ItemProtection
    SELF_TRANSFORM,             // Metamorphosis, BearForm, RavenForm, RoboGoblin
    TARGET_SLOW_BUFF,           // Slow
    TARGET_HEX_BUFF,            // Hex
    TARGET_POLYMORPH_BUFF,      // Polymorph
    TARGET_CYCLONE_BUFF,        // Cyclone
    TARGET_ENSNARE_BUFF,        // Ensnare
    TARGET_CURSE_BUFF,          // Curse
    TARGET_INVISIBILITY_BUFF,   // Invisibility
    TARGET_BANISH_BUFF,         // Banish
    TARGET_ANTIMAGIC_BUFF,      // AntiMagicShell
    TARGET_REJUVENATION_BUFF,   // Rejuvenation
    TARGET_INNER_FIRE_BUFF,     // InnerFire
    TARGET_LIGHTNING_SHIELD,    // LightningShield, LightningShieldWand
    TARGET_FROST_ARMOR,         // FrostArmor
    TARGET_FAERIE_FIRE,         // FaerieFire
    TARGET_LIFE_DRAIN,          // LifeDrain
    TARGET_SIPHON_MANA,         // SiphonMana
    TARGET_DOOM,                // Doom
    TARGET_UNHOLY_FRENZY,       // UnholyFrenzy
    POINT_SILENCE_AREA,         // Silence
    POINT_REVEAL_DUST_FLARE,    // DustOfAppearance, Flare
    CASTER_AREA_ROAR,           // Roar, BattleRoar, BeastScroll
    CASTER_AREA_HOWL,           // HowlOfTerror
    CASTER_AREA_VOODOO,         // BigBadVoodoo
    SUMMON_TIMED_UNIT,          // FeralSpirit, SummonWaterElemental, Quilbeast, OwlScout, SummonHawk
    SUMMON_TIMED_RING,          // ForceOfNature
    TIMED_SPAWNER_FACTORY,      // PocketFactory, Factory
    GRANT_CASTER_GOLD           // ChestOfGold
};

struct SpellArchetypeRule {
    uint32_t rawcode = 0;
    CSpellTargetType targetType = CSpellTargetType::NO_TARGET;
    CAbilityCategory category = CAbilityCategory::SPELL;
    SpellArchetypeEffectKind effectKind = SpellArchetypeEffectKind::CUSTOM_CALLBACK;
    CAttackType attackType = CAttackType::SPELLS;
    CDamageType damageType = CDamageType::NORMAL;
    ABFactionFilter factionFilter = ABFactionFilter::ANY;
    float castRange = 600.0f;
    float castPoint = 0.3f;
    float cooldown = 0.0f;
    int manaCost = 0;
    float param0 = 0.0f;   // e.g. damage / heal / bonus / waves
    float param1 = 0.0f;   // e.g. secondary damage / slowPct / bounces / count
    float param2 = 0.0f;   // e.g. tertiary value / width / interval
    float radius = 0.0f;   // e.g. AoE radius / range
    float duration = 0.0f; // e.g. buff/summon duration
    uint32_t auxId0 = 0;   // e.g. summon unit War3ID or buff War3ID
    uint32_t auxId1 = 0;   // e.g. secondary unit/buff War3ID
    int8_t slotParam0 = -1; // CAbilityDefFull Data slot index for param0 (-1 = none, -2 = Int)
    int8_t slotParam1 = -1; // CAbilityDefFull Data slot index for param1
    int8_t slotParam2 = -1; // CAbilityDefFull Data slot index for param2
    bool scaleRadiusFromArea = false;
    bool scaleDurationFromDur = false;
    bool scaleAuxId0FromUnitId = false;

    void Execute(CSimulation& game, CUnit* caster, CWidget* target,
                 bool hasPoint, float x, float y) const;
    void ApplyDef(const CAbilityDefFull& def, int level);
};

// ============================================================
// CAbilityTableRuleSpell - base class for table-rule-backed spells
// Reduces each table-driven spell subclass to a 1-line constructor!
// ============================================================
class CAbilityTableRuleSpell : public CAbilitySpellBase {
public:
    CAbilityTableRuleSpell(int handleId, const SpellArchetypeRule& initialRule);

    const SpellArchetypeRule& GetRule() const { return rule_; }
    SpellArchetypeRule& MutableRule() { return rule_; }

    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
    void DoEffect(CSimulation& game, CUnit* caster) override;

protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;

    SpellArchetypeRule rule_;
};
