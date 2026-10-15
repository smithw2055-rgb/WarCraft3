#include "item_bonuses.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/abilities/ability_hero.h"

namespace {

CAbilityHero* FindHeroAbility(CUnit* unit) {
    if (!unit || !unit->IsHero()) return nullptr;
    CAbility* ability = unit->GetAbility(War3ID("AHer"));
    return dynamic_cast<CAbilityHero*>(ability);
}

void ApplyOne(CSimulation& game, CUnit* unit, const CItemBonus& bonus, int sign) {
    const int v = bonus.value * sign;
    switch (bonus.type) {
        case CItemBonusType::STRENGTH:
            if (auto* hero = FindHeroAbility(unit)) {
                hero->AddStrengthBonus(game, unit, v);
            } else {
                // Non-hero fallback: STR grants bonus life (25 HP per point)
                unit->AddBonusMaxLife((float)v * 25.0f);
            }
            break;
        case CItemBonusType::AGILITY:
            if (auto* hero = FindHeroAbility(unit)) {
                hero->AddAgilityBonus(game, unit, v);
            } else {
                // Non-hero fallback: AGI grants bonus armor at hero rate
                unit->AddBonusArmor(v >= 0 ? (v * 33 + 50) / 100 : -((-v * 33 + 50) / 100));
            }
            break;
        case CItemBonusType::INTELLIGENCE:
            if (auto* hero = FindHeroAbility(unit)) {
                hero->AddIntelligenceBonus(game, unit, v);
            } else {
                // Non-hero fallback: INT grants bonus mana (15 MP per point)
                unit->AddBonusMaxMana(v * 15);
            }
            break;
        case CItemBonusType::DAMAGE:
            unit->AddBonusDamage(v);
            break;
        case CItemBonusType::ARMOR:
            unit->AddBonusArmor(v);
            break;
        case CItemBonusType::ATTACK_SPEED:
            // value is a percentage: +15 means 15% faster attacks. Items
            // are the stacking speed class (ref attackSpeedBonus, plain
            // sum with everything); buffs/aura never touch this.
            // v already carries the apply/remove sign.
            unit->AddStackingAttackSpeed(v / 100.0f);
            break;
        case CItemBonusType::MAX_LIFE:
            unit->AddBonusMaxLife((float)v);
            break;
        case CItemBonusType::MAX_MANA:
            unit->AddBonusMaxMana(v);
            break;
        case CItemBonusType::MOVE_SPEED:
            unit->SetSpeed(unit->GetSpeed() + v);
            break;
        case CItemBonusType::LIFE_REGEN:
        case CItemBonusType::MANA_REGEN:
        case CItemBonusType::SIGHT_RADIUS:
            // No supporting unit fields yet; explicitly unhandled (was silent default).
            break;
        case CItemBonusType::NONE:
        default:
            break;
    }
}

} // namespace

void ApplyItemBonuses(CSimulation& game, CUnit* unit, const std::vector<CItemBonus>& bonuses) {
    if (!unit) return;
    for (const auto& bonus : bonuses) {
        ApplyOne(game, unit, bonus, +1);
    }
}

void RemoveItemBonuses(CSimulation& game, CUnit* unit, const std::vector<CItemBonus>& bonuses) {
    if (!unit) return;
    for (const auto& bonus : bonuses) {
        ApplyOne(game, unit, bonus, -1);
    }
}
