#include "ability_hero.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
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

        // Pick a random proper name
        auto& names = unitType->GetHeroProperNames();
        if (!names.empty()) {
            properName_ = names[std::rand() % names.size()];
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
    if (heroLevel_ >= 10) return; // max level

    // Apply player handicap
    auto* player = game.GetPlayer(unit->GetPlayerIndex());
    float handicap = player ? player->GetHandicap() : 1.0f;
    xp_ += (int)(amount * handicap);

    // Check for level ups
    while (heroLevel_ < 10) {
        int needed = GetXPForLevel(heroLevel_ + 1);
        if (xp_ < needed) break;
        heroLevel_++;
        skillPoints_++;
        CalculateDerivedStats(game, unit);

        std::cout << "Hero leveled up to " << heroLevel_ << std::endl;
    }
}

void CAbilityHero::SetHeroLevel(CSimulation& game, CUnit* unit, int level) {
    level = std::clamp(level, 1, 10);
    int totalXP = GetXPForLevel(level);
    if (totalXP > xp_) {
        AddXP(game, unit, totalXP - xp_);
    }
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
        // Level up existing ability (TODO: implement level system on abilities)
        skillPoints_--;
        std::cout << "Hero learned skill: " << skillCode.ToString() << std::endl;
    } else {
        // TODO: Create new ability instance from ability data and add to unit
        skillPoints_--;
        std::cout << "Hero learned new skill: " << skillCode.ToString() << std::endl;
    }
}

void CAbilityHero::CalculateDerivedStats(CSimulation& game, CUnit* unit) {
    (void)game;
    int str = strength_.GetCurrent(heroLevel_);
    int agi = agility_.GetCurrent(heroLevel_);
    int intel = intelligence_.GetCurrent(heroLevel_);

    // STR: +HP, +HP regen
    float strHPBonus = str * 25.0f; // standard WC3: 25 HP per STR
    unit->SetMaxLife((float)unit->GetUnitType()->GetMaxLife() + strHPBonus);

    // AGI: +armor, +attack speed
    float agiArmorBonus = agi * 0.33f; // standard WC3: ~0.33 armor per AGI
    (void)agiArmorBonus; // TODO: apply to defense

    // INT: +MP, +MP regen
    float intMPBonus = intel * 15.0f; // standard WC3: 15 MP per INT
    unit->SetMaxMana((int)((float)unit->GetUnitType()->GetManaMaximum() + intMPBonus));

    // Primary attribute damage bonus
    auto primary = unit->GetUnitType()->GetPrimaryAttribute();
    int primaryBase = 0;
    switch (primary) {
        case CPrimaryAttribute::STRENGTH: primaryBase = str; break;
        case CPrimaryAttribute::AGILITY: primaryBase = agi; break;
        case CPrimaryAttribute::INTELLIGENCE: primaryBase = intel; break;
    }
    (void)primaryBase; // TODO: apply as damage bonus
}

int CAbilityHero::GetXPForLevel(int level) const {
    // Standard WC3 XP table
    static const int xpTable[] = {
        0, 0, 200, 500, 900, 1400, 2000, 2700, 3500, 4400, 5400
    };
    if (level < 1) level = 1;
    if (level > 10) level = 10;
    return xpTable[level];
}
