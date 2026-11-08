#include "spells25.h"
#include "data/abilitydata.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include <algorithm>

namespace {

CAbilityHero* FindHero(CUnit* unit) {
    if (!unit) return nullptr;
    return dynamic_cast<CAbilityHero*>(unit->GetAbility(War3ID("AHer")));
}

void KillUnit(CSimulation& game, CUnit* unit) {
    if (!unit || unit->IsDead()) return;
    unit->SetLife(game, 0);
}

void ApplyUnitStatSlotDelta(CUnit* unit, CAbilityItemStatBonus::StatSlot slot, int delta) {
    if (!unit || delta == 0) return;
    switch (slot) {
        case CAbilityItemStatBonus::StatSlot::DAMAGE:   unit->AddBonusDamage(delta); break;
        case CAbilityItemStatBonus::StatSlot::ARMOR:    unit->AddBonusArmor(delta); break;
        case CAbilityItemStatBonus::StatSlot::MAX_LIFE: unit->AddBonusMaxLife(static_cast<float>(delta)); break;
        case CAbilityItemStatBonus::StatSlot::MAX_MANA: unit->AddBonusMaxMana(delta); break;
        default: break;
    }
}

} // namespace

// ===== Curse =====
CAbilityCurse::CAbilityCurse(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Acrs"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
    SetManaCost(50);
}

void CAbilityCurse::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* unit = dynamic_cast<CUnit*>(target);
    if (!unit || unit->IsDead()) return;
    (void)caster;
    unit->AddBuff(std::make_unique<CBuffCurse>(duration_, missChance_), game);
}

void CAbilityCurse::ApplyEffectData(const CAbilityDefFull& def, int level) {
    missChance_ = def.DataFloatAt(level, 0, missChance_);
    duration_ = def.DurationAt(level);
}

// ===== Moon Well =====
CAbilityMoonWell::CAbilityMoonWell(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Ambt"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
}

void CAbilityMoonWell::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* unit = dynamic_cast<CUnit*>(target);
    if (!caster || !unit || unit->IsDead()) return;
    float mana = caster->GetMana();
    if (manaPerLife_ > 0.0f) {
        const float wanted = std::max(0.0f, unit->GetMaxLife() - unit->GetLife());
        if (wanted > 0.0f && mana > 0.0f) {
            const float gained = std::min(mana / manaPerLife_, wanted);
            unit->SetLife(game, unit->GetLife() + gained);
            mana -= gained * manaPerLife_;
        }
    }
    if (manaPerMana_ > 0.0f) {
        const float wanted = std::max(0.0f, (float)unit->GetMaxMana() - unit->GetMana());
        if (wanted > 0.0f && mana > 0.0f) {
            const float gained = std::min(mana / manaPerMana_, wanted);
            unit->SetMana(unit->GetMana() + gained);
            mana -= gained * manaPerMana_;
        }
    }
    caster->SetMana(mana);
}

void CAbilityMoonWell::ApplyEffectData(const CAbilityDefFull& def, int level) {
    manaPerMana_ = def.DataFloatAt(level, 0, manaPerMana_);
    manaPerLife_ = def.DataFloatAt(level, 1, manaPerLife_);
}

// ===== Eat Tree =====
CAbilityEatTree::CAbilityEatTree(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aeat"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(150.0f);
}

void CAbilityEatTree::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* tree = dynamic_cast<CDestructable*>(target);
    if (!caster || !tree || tree->IsDead()) return;
    tree->Damage(game, caster, false, false, CAttackType::NORMAL, CDamageType::NORMAL,
                 "", tree->GetLife() + 1.0f, 0);
    const float life = std::min(caster->GetMaxLife(), caster->GetLife() + hitPointsGained_);
    caster->SetLife(game, life);
}

void CAbilityEatTree::ApplyEffectData(const CAbilityDefFull& def, int level) {
    hitPointsGained_ = def.DataFloatAt(level, 2, hitPointsGained_);
}

// ===== Sacrifice =====
CAbilitySacrifice::CAbilitySacrifice(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Asac"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(100000.0f);
}

void CAbilitySacrifice::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* victim = dynamic_cast<CUnit*>(target);
    if (!victim) return;
    const float x = victim->GetX();
    const float y = victim->GetY();
    const int player = caster ? caster->GetPlayerIndex() : victim->GetPlayerIndex();
    KillUnit(game, victim);
    game.CreateUnit(War3ID("ushd"), player, x, y, 0);
}

CAbilityRequestSacrifice::CAbilityRequestSacrifice(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Alam"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(100000.0f);
}

void CAbilityRequestSacrifice::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)target;
    KillUnit(game, caster);
}

// ===== Hide =====
CAbilityHide::CAbilityHide(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Ahid"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityHide::DoEffect(CSimulation& game, CUnit* caster) {
    (void)game;
    if (caster) caster->SetInvisible(true);
}

// ===== Ethereal =====
CAbilityEthereal::CAbilityEthereal(int handleId)
    : CAbility(handleId, War3ID("Aeth"), CAbilityCategory::BUFF) {
    iconShowing_ = false;
    permanent_ = true;
}

void CAbilityEthereal::OnAdd(CSimulation& game, CUnit* unit) {
    if (!unit || applied_) return;
    savedCollision_ = unit->GetCollisionSize();
    unit->SetCollisionSize(0.0f);
    unit->AddBuff(std::make_unique<CBuffEthereal>(1.0e9f, War3ID("BEth")), game);
    applied_ = true;
}

void CAbilityEthereal::OnRemove(CSimulation& game, CUnit* unit) {
    if (!unit || !applied_) return;
    unit->SetCollisionSize(savedCollision_);
    unit->RemoveBuff(War3ID("BEth"), game);
    applied_ = false;
}

// ===== Chest of Gold =====
CAbilityChestOfGold::CAbilityChestOfGold(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIgo"), CAbilityCategory::ITEM) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityChestOfGold::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    if (CPlayer* player = game.GetPlayer(caster->GetPlayerIndex())) {
        player->AddGold(gold_);
    }
}

void CAbilityChestOfGold::ApplyEffectData(const CAbilityDefFull& def, int level) {
    gold_ = def.DataIntAt(level, 0, gold_);
}

// ===== Unified Item Stat Passives =====
CAbilityItemStatBonus::CAbilityItemStatBonus(int handleId, War3ID code, StatSlot slot, int defaultBonus)
    : CAbilitySpellBase(handleId, code, CAbilityCategory::ITEM)
    , slot_(slot)
    , bonus_(defaultBonus) {
    iconShowing_ = false;
    permanent_ = true;
    SetPassiveSpell(true);
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemStatBonus::OnAdd(CSimulation&, CUnit* unit) {
    if (!unit || slot_ == StatSlot::NONE) return;
    applied_ = bonus_;
    ApplyUnitStatSlotDelta(unit, slot_, applied_);
}

void CAbilityItemStatBonus::OnRemove(CSimulation&, CUnit* unit) {
    if (!unit || slot_ == StatSlot::NONE) return;
    ApplyUnitStatSlotDelta(unit, slot_, -applied_);
    applied_ = 0;
}

void CAbilityItemStatBonus::ApplyEffectData(const CAbilityDefFull& def, int level) {
    if (slot_ != StatSlot::NONE) {
        bonus_ = def.DataIntAt(level, 0, bonus_);
    }
}

CAbilityItemAttributeBonus::CAbilityItemAttributeBonus(int handleId)
    : CAbilityItemStatBonus(handleId, War3ID("AIab"), StatSlot::NONE, 0) {}

void CAbilityItemAttributeBonus::OnAdd(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    appliedAgi_ = agility_;
    appliedInt_ = intelligence_;
    appliedStr_ = strength_;
    if (CAbilityHero* hero = FindHero(unit)) {
        hero->AddAgilityBonus(game, unit, appliedAgi_);
        hero->AddIntelligenceBonus(game, unit, appliedInt_);
        hero->AddStrengthBonus(game, unit, appliedStr_);
    }
}

void CAbilityItemAttributeBonus::OnRemove(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    if (CAbilityHero* hero = FindHero(unit)) {
        hero->AddAgilityBonus(game, unit, -appliedAgi_);
        hero->AddIntelligenceBonus(game, unit, -appliedInt_);
        hero->AddStrengthBonus(game, unit, -appliedStr_);
    }
    appliedAgi_ = appliedInt_ = appliedStr_ = 0;
}

void CAbilityItemAttributeBonus::ApplyEffectData(const CAbilityDefFull& def, int level) {
    agility_ = def.DataIntAt(level, 0, agility_);
    intelligence_ = def.DataIntAt(level, 1, intelligence_);
    strength_ = def.DataIntAt(level, 2, strength_);
}
