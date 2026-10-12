#include "ability_hero.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/ability_factory.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>

CAbilityHero::CAbilityHero(int handleId)
    : CAbility(handleId, War3ID("AHer"), CAbilityCategory::HERO) {
    permanent_ = true; // hero ability survives unit type changes
    iconShowing_ = false;
}

void CAbilityHero::OnAdd(CSimulation& game, CUnit* unit) {
    // Initialize stats from unit type
    auto* unitType = unit->GetUnitType();
    if (unitType) {
        strength_.base = unitType->GetStartingStrength();
        strength_.perLevelFactor = unitType->GetStrengthPerLevel();
        agility_.base = unitType->GetStartingAgility();
        agility_.perLevelFactor = unitType->GetAgilityPerLevel();
        intelligence_.base = unitType->GetStartingIntelligence();
        intelligence_.perLevelFactor = unitType->GetIntelligencePerLevel();

        // Pick a proper name via the synced sim RNG (std::rand is banned).
        auto& names = unitType->GetHeroProperNames();
        if (!names.empty()) {
            properName_ = names[(size_t)(game.SimRand01() * (float)names.size()) % names.size()];
        }
    }

    heroLevel_ = 1;
    xp_ = 0;
    skillPoints_ = 1;

    CalculateDerivedStats(game, unit);
}

void CAbilityHero::OnTick(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
    // Hero passive effects could go here (e.g., aura updates)
}

bool CAbilityHero::CheckBeforeQueueNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    // Check if this is a learn-skill order
    // Learn-skill orders encode the ability rawcode in the order ID
    War3ID skillCode(static_cast<uint32_t>(orderId));

    if (skillsAvailable_.count(skillCode) > 0) {
        LearnSkill(game, caster, skillCode);
        return false; // consume the order, don't queue it
    }
    return true; // allow queueing
}

void CAbilityHero::AddXP(CSimulation& game, CUnit* unit, int amount) {
    const int maxLevel = game.GetGameplayConstants().GetMaxHeroLevel();
    if (heroLevel_ >= maxLevel) return; // max level
    if (game.IsHeroXPSuspended()) return;

    // Apply player XP handicap (ref CAbilityHero.addXp uses getHandicapXP)
    auto* player = game.GetPlayer(unit->GetPlayerIndex());
    float handicap = player ? player->GetHandicapXP() : 1.0f;
    xp_ += (int)(amount * handicap);

    // Check for level ups
    while (heroLevel_ < maxLevel) {
        int needed = game.GetGameplayConstants().GetXPForLevel(heroLevel_ + 1);
        if (xp_ < needed) break;
        heroLevel_++;
        skillPoints_++;
        CalculateDerivedStats(game, unit);

        std::cout << "Hero leveled up to " << heroLevel_ << std::endl;
    }
}

void CAbilityHero::SetHeroLevel(CSimulation& game, CUnit* unit, int level) {
    const int maxLevel = game.GetGameplayConstants().GetMaxHeroLevel();
    level = std::clamp(level, 1, maxLevel);
    int totalXP = game.GetGameplayConstants().GetXPForLevel(level);
    if (totalXP > xp_) {
        AddXP(game, unit, totalXP - xp_);
    }
}

bool CAbilityHero::StripLevels(CSimulation& game, CUnit* unit, int levels) {
    if (levels <= 0 || heroLevel_ <= 1) return false;
    heroLevel_ = std::max(1, heroLevel_ - levels);
    xp_ = game.GetGameplayConstants().GetXPForLevel(heroLevel_);
    CalculateDerivedStats(game, unit);
    return true;
}

void CAbilityHero::SetXP(CSimulation& game, CUnit* unit, int value) {
    // Ref semantics (CAbilityHero.setXp): only increases XP/level, never
    // decreases. Applies handicapXP rounding like the reference.
    auto* player = game.GetPlayer(unit->GetPlayerIndex());
    float handicap = player ? player->GetHandicapXP() : 1.0f;
    int newXp = (int)((float)value * handicap + 0.5f);
    if (newXp > xp_) {
        AddXP(game, unit, newXp - xp_);
    }
}

bool CAbilityHero::SelectHeroSkill(CSimulation& game, CUnit* unit, War3ID skillCode) {
    if (skillPoints_ <= 0) return false;
    if (skillsAvailable_.count(skillCode) == 0) {
        // Like ref selectHeroSkill: unknown skills still consume the order
        // only when learnable; report failure otherwise.
        return false;
    }
    const int pointsBefore = skillPoints_;
    LearnSkill(game, unit, skillCode);
    // LearnSkill keeps the point for unknown codes; report success only
    // when a point was actually spent (ref selectHeroSkill).
    return skillPoints_ < pointsBefore;
}

void CAbilityHero::LearnSkill(CSimulation& game, CUnit* unit, War3ID skillCode) {
    if (skillPoints_ <= 0) return;

    // Check if unit already has this ability
    auto& abilities = unit->GetAbilities();
    CAbility* existingAbility = nullptr;
    for (auto* ab : abilities) {
        if (ab && (ab->GetCode() == skillCode || ab->GetAlias() == skillCode)) {
            existingAbility = ab;
            break;
        }
    }

    if (existingAbility) {
        // Level up the existing ability (ref hero skill levels 1-3).
        existingAbility->SetLevel(existingAbility->GetLevel() + 1);
        existingAbility->OnLevelChanged(game, unit);
        skillPoints_--;
        std::cout << "Hero leveled skill: " << skillCode.ToString()
                  << " to " << existingAbility->GetLevel() << std::endl;
        if (const auto& cb = game.GetHeroSkillCallback()) {
            cb(unit->GetHandleId(), skillCode.value, existingAbility->GetLevel());
        }
    } else if (auto created = AbilityFactory::CreateAbility(game.AllocateHandleId(), skillCode)) {
        created->SetLevel(1);
        unit->AddAbility(game, std::move(created));
        skillPoints_--;
        std::cout << "Hero learned new skill: " << skillCode.ToString() << std::endl;
        if (const auto& cb = game.GetHeroSkillCallback()) {
            cb(unit->GetHandleId(), skillCode.value, 1);
        }
    } else {
        // Unknown skill code (e.g. custom ability with no C++ class yet):
        // keep the point, like ref selectHeroSkill reporting failure.
        std::cout << "Hero cannot learn unknown skill: " << skillCode.ToString() << std::endl;
    }
}

void CAbilityHero::CalculateDerivedStats(CSimulation& game, CUnit* unit) {
    if (!unit || !unit->GetUnitType()) return;
    const auto& gc = game.GetGameplayConstants();
    int str = strength_.GetCurrent(heroLevel_);
    int agi = agility_.GetCurrent(heroLevel_);
    int intel = intelligence_.GetCurrent(heroLevel_);

    // STR: +HP, +HP regen. Stored as the hero layer so item/upgrade
    // max-life bonuses survive level-ups and stat changes.
    float strHPBonus = str * gc.strHitPointBonus;
    unit->SetHeroBonusLife(strHPBonus);

    // AGI: +armor, +attack speed
    int agiArmorBonus = (int)(agi * gc.agiDefenseBonus);
    unit->SetDefense(unit->GetUnitType()->GetDefense() + agiArmorBonus);

    // INT: +MP, +MP regen (hero layer, see above)
    float intMPBonus = intel * gc.intManaBonus;
    unit->SetHeroBonusMana((int)intMPBonus);

    // Primary attribute damage bonus
    auto primary = unit->GetUnitType()->GetPrimaryAttribute();
    int primaryBase = 0;
    switch (primary) {
        case CPrimaryAttribute::STRENGTH: primaryBase = str; break;
        case CPrimaryAttribute::AGILITY: primaryBase = agi; break;
        case CPrimaryAttribute::INTELLIGENCE: primaryBase = intel; break;
    }
    unit->SetHeroBonusDamage(primaryBase);
}

void CAbilityHero::AddStrengthBonus(CSimulation& game, CUnit* unit, int v) {
    strength_.bonus += v;
    CalculateDerivedStats(game, unit);
}

void CAbilityHero::AddAgilityBonus(CSimulation& game, CUnit* unit, int v) {
    agility_.bonus += v;
    CalculateDerivedStats(game, unit);
}

void CAbilityHero::AddIntelligenceBonus(CSimulation& game, CUnit* unit, int v) {
    intelligence_.bonus += v;
    CalculateDerivedStats(game, unit);
}

int CAbilityHero::GetXPForLevel(int level) const {
    // Data-driven table lives on CGameplayConstants, but hero abilities
    // predate the sim in some construction paths (no game handle). Fall
    // back to TFT retail when the table is empty.
    // Note: static fallback only; live path queries gameplay constants
    // via the sim. This const method has no game handle, so callers pass
    // thresholds in. Kept for StripLevels/SetHeroLevel compat.
    static const int xpTable[] = {
        0, 0, 200, 500, 900, 1400, 2000, 2700, 3500, 4400, 5400
    };
    if (level < 1) level = 1;
    if (level > 10) level = 10;
    return xpTable[level];
}
