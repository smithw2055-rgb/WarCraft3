#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 25: remaining ref gaps that sit on existing unit/combat hooks.
// Curse, moon well, eat tree, sacrifice, ambush, ethereal collision,
// chest of gold, and the item stat passives from CAbilityData.

struct CAbilityDefFull;

// ===== Curse (Banshee, Acrs) - attacks may miss =====
class CAbilityCurse : public CAbilitySpellBase {
public:
    CAbilityCurse(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float duration_ = 15.0f;
    float missChance_ = 0.33f;
};

// ===== Moon Well (Ambt) - spend mana to restore life, then mana =====
class CAbilityMoonWell : public CAbilitySpellBase {
public:
    CAbilityMoonWell(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float manaPerMana_ = 1.0f;
    float manaPerLife_ = 4.0f;
};

// ===== Eat Tree (Aeat) - destroy a tree and heal =====
class CAbilityEatTree : public CAbilitySpellBase {
public:
    CAbilityEatTree(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float hitPointsGained_ = 100.0f;
};

// ===== Sacrifice (Asac) - the pit consumes the target =====
class CAbilitySacrifice : public CAbilitySpellBase {
public:
    CAbilitySacrifice(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
};

// ===== Request Sacrifice (Alam) - the acolyte offers itself =====
class CAbilityRequestSacrifice : public CAbilitySpellBase {
public:
    CAbilityRequestSacrifice(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
};

// ===== Hide / Ambush (Ahid) - instant invisibility =====
class CAbilityHide : public CAbilitySpellBase {
public:
    CAbilityHide(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
};

// ===== Ethereal (Aeth) - no unit collision while the ability is on =====
class CAbilityEthereal : public CAbility {
public:
    explicit CAbilityEthereal(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
private:
    float savedCollision_ = 0.0f;
    bool applied_ = false;
};

// ===== Chest of Gold (AIgo) - grant gold on use =====
class CAbilityChestOfGold : public CAbilitySpellBase {
public:
    CAbilityChestOfGold(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    int gold_ = 250;
};

// ============================================================================
// Phase 17 Unified Passive Item Stat Bonus Base (`CAbilityItemStatBonus`)
// Consolidates duplicated OnAdd/OnRemove/ApplyEffectData logic across
// CAbilityItemDamageBonus, CAbilityItemArmorBonus, CAbilityItemLifeBonus,
// and CAbilityItemManaPoolBonus into a single parameterized base.
// ============================================================================
class CAbilityItemStatBonus : public CAbilitySpellBase {
public:
    enum class StatSlot : uint8_t {
        NONE = 0,
        DAMAGE,
        ARMOR,
        MAX_LIFE,
        MAX_MANA
    };

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

// AIab: DataA agility, DataB intelligence, DataC strength (ref definition).
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
