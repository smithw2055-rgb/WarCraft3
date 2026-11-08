#include "spells26.h"
#include "data/abilitydata.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include <algorithm>
#include <cmath>

namespace {

CAbilityHero* FindHero(CUnit* unit) {
    if (!unit) return nullptr;
    return dynamic_cast<CAbilityHero*>(unit->GetAbility(War3ID("AHer")));
}

} // namespace

CAbilitySpellShieldPassive::CAbilitySpellShieldPassive(int handleId)
    : CAbility(handleId, War3ID("ANss"), CAbilityCategory::SPELL) {
    iconShowing_ = false;
    permanent_ = true;
}

void CAbilitySpellShieldPassive::Arm(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    if (!unit->HasBuff(War3ID("BNss"))) {
        unit->AddBuff(std::make_unique<CBuffSpellShield>(1.0e9f), game);
    }
    armed_ = true;
    remaining_ = 0.0f;
}

void CAbilitySpellShieldPassive::OnAdd(CSimulation& game, CUnit* unit) {
    Arm(game, unit);
}

void CAbilitySpellShieldPassive::OnRemove(CSimulation& game, CUnit* unit) {
    if (unit) unit->RemoveBuff(War3ID("BNss"), game);
    armed_ = false;
    remaining_ = 0.0f;
}

void CAbilitySpellShieldPassive::OnTick(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    if (remaining_ > 0.0f) {
        remaining_ -= game.GetSimStepTime();
        if (remaining_ <= 0.0f) Arm(game, unit);
        return;
    }
    if (armed_ && !unit->HasBuff(War3ID("BNss"))) {
        armed_ = false;
        remaining_ = cooldown_;
    }
}

CAbilitySpellShieldArea::CAbilitySpellShieldArea(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANse"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(600.0f);
    SetManaCost(50);
}

void CAbilitySpellShieldArea::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    ForEachPassiveAllyInRange(game, caster, x, y, radius_, true,
                              [&game, duration = duration_](CUnit* other) {
                                  other->AddBuff(std::make_unique<CBuffSpellShield>(duration), game);
                              });
}

void CAbilitySpellShieldArea::ApplyEffectData(const CAbilityDefFull& def, int level) {
    radius_ = def.AreaAt(level);
    duration_ = def.DurationAt(level);
}

CAbilityItemPermanentStats::CAbilityItemPermanentStats(int handleId, War3ID code,
                                                       int agility, int intelligence, int strength)
    : CAbilitySpellBase(handleId, code, CAbilityCategory::ITEM)
    , agility_(agility), intelligence_(intelligence), strength_(strength) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemPermanentStats::DoEffect(CSimulation& game, CUnit* caster) {
    CAbilityHero* hero = FindHero(caster);
    if (!hero) return;
    hero->SetAgilityBase(hero->GetAgility().base + agility_);
    hero->SetIntelligenceBase(hero->GetIntelligence().base + intelligence_);
    hero->SetStrengthBase(hero->GetStrength().base + strength_);
    hero->AddStrengthBonus(game, caster, 0);
}

void CAbilityItemPermanentStats::ApplyEffectData(const CAbilityDefFull& def, int level) {
    agility_ = def.DataIntAt(level, 0, agility_);
    intelligence_ = def.DataIntAt(level, 1, intelligence_);
    strength_ = def.DataIntAt(level, 2, strength_);
}

CAbilityItemPermanentLife::CAbilityItemPermanentLife(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AImi"), CAbilityCategory::ITEM) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemPermanentLife::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster || life_ == 0) return;
    const float before = caster->GetMaxLife();
    const float ratio = before > 0.0f ? caster->GetLife() / before : 1.0f;
    caster->AddBonusMaxLife((float)life_);
    caster->SetLife(game, ratio * caster->GetMaxLife());
}

void CAbilityItemPermanentLife::ApplyEffectData(const CAbilityDefFull& def, int level) {
    life_ = def.DataIntAt(level, 0, life_);
}

CAbilityItemExperience::CAbilityItemExperience(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIem"), CAbilityCategory::ITEM) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemExperience::DoEffect(CSimulation& game, CUnit* caster) {
    if (CAbilityHero* hero = FindHero(caster)) {
        hero->AddXP(game, caster, xp_);
    }
}

void CAbilityItemExperience::ApplyEffectData(const CAbilityDefFull& def, int level) {
    xp_ = def.DataIntAt(level, 0, xp_);
}

CAbilityItemLevelGain::CAbilityItemLevelGain(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIlm"), CAbilityCategory::ITEM) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemLevelGain::DoEffect(CSimulation& game, CUnit* caster) {
    if (CAbilityHero* hero = FindHero(caster)) {
        hero->SetHeroLevel(game, caster, hero->GetHeroLevel() + levels_);
    }
}

void CAbilityItemLevelGain::ApplyEffectData(const CAbilityDefFull& def, int level) {
    levels_ = def.DataIntAt(level, 0, levels_);
}

CAbilityItemFigurine::CAbilityItemFigurine(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIfs"), CAbilityCategory::ITEM) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemFigurine::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster || count_ <= 0) return;
    for (int i = 0; i < count_; i++) {
        CUnit* summoned = game.CreateUnit(summonId_, caster->GetPlayerIndex(),
                                          caster->GetX() + 64.0f, caster->GetY(),
                                          caster->GetFacing());
        if (!summoned) continue;
        summoned->AddBuff(std::make_unique<CBuffTimedLife>(War3ID("BFig"), duration_), game);
    }
}

void CAbilityItemFigurine::ApplyEffectData(const CAbilityDefFull& def, int level) {
    count_ = def.DataIntAt(level, 0, count_);
    const std::string id = def.DataAt(level, 2);
    if (id.size() == 4) summonId_ = War3ID(id.c_str());
    duration_ = def.DurationAt(level);
}

CAbilityPermanentInvulnerable::CAbilityPermanentInvulnerable(int handleId)
    : CAbility(handleId, War3ID("Avul"), CAbilityCategory::BUFF) {
    iconShowing_ = false;
    permanent_ = true;
}

void CAbilityPermanentInvulnerable::OnAdd(CSimulation&, CUnit* unit) {
    if (unit) unit->SetInvulnerable(true);
}

void CAbilityPermanentInvulnerable::OnRemove(CSimulation&, CUnit* unit) {
    if (unit) unit->SetInvulnerable(false);
}

CAbilityChangeTimeOfDay::CAbilityChangeTimeOfDay(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIct"), CAbilityCategory::ITEM) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityChangeTimeOfDay::DoEffect(CSimulation& game, CUnit* caster) {
    (void)caster;
    const float clockHours = (float)hour_ + (float)minute_ / 60.0f;
    game.SetGameTimeOfDay(clockHours);
}

void CAbilityChangeTimeOfDay::ApplyEffectData(const CAbilityDefFull& def, int level) {
    hour_ = def.DataIntAt(level, 0, hour_);
    minute_ = def.DataIntAt(level, 1, minute_);
}

CAbilityToggleForm::CAbilityToggleForm(int handleId, War3ID code)
    : CAbilitySpellBase(handleId, code, CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    formId_ = code;
}

void CAbilityToggleForm::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    if (caster->GetBuff(GetCode())) {
        caster->RemoveBuff(GetCode(), game);
        return;
    }
    caster->AddBuff(std::make_unique<CBuffTransform>(GetCode(), 1.0e9f, formId_), game);
}

void CAbilityToggleForm::ApplyEffectData(const CAbilityDefFull& def, int level) {
    formId_ = def.UnitIdAt(level, formId_);
}

CAbilityChaos::CAbilityChaos(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Acha"), CAbilityCategory::SPELL) {
    iconShowing_ = false;
    permanent_ = true;
    SetPassiveSpell(true);
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityChaos::OnAdd(CSimulation& game, CUnit* unit) {
    if (!unit || unit->GetBuff(GetCode())) return;
    unit->AddBuff(std::make_unique<CBuffTransform>(GetCode(), 1.0e9f, formId_), game);
}

void CAbilityChaos::OnRemove(CSimulation& game, CUnit* unit) {
    if (unit) unit->RemoveBuff(GetCode(), game);
}

void CAbilityChaos::ApplyEffectData(const CAbilityDefFull& def, int level) {
    formId_ = def.UnitIdAt(level, formId_);
}
