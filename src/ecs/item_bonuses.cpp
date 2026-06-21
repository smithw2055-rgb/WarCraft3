#include "item_bonuses.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"

void ApplyItemBonuses(CSimulation& game, CUnit* unit, const std::vector<CItemBonus>& bonuses) {
    (void)game;
    for (const auto& bonus : bonuses) {
        switch (bonus.type) {
            case CItemBonusType::STRENGTH:
                // TODO: modify hero STR bonus
                break;
            case CItemBonusType::AGILITY:
                // TODO: modify hero AGI bonus
                break;
            case CItemBonusType::INTELLIGENCE:
                // TODO: modify hero INT bonus
                break;
            case CItemBonusType::DAMAGE:
                // TODO: add damage bonus
                break;
            case CItemBonusType::ARMOR:
                // TODO: add armor bonus
                break;
            case CItemBonusType::MAX_LIFE:
                unit->SetMaxLife(unit->GetMaxLife() + bonus.value);
                break;
            case CItemBonusType::MAX_MANA:
                unit->SetMaxMana(unit->GetMaxMana() + bonus.value);
                break;
            case CItemBonusType::MOVE_SPEED:
                unit->SetSpeed(unit->GetSpeed() + bonus.value);
                break;
            default:
                break;
        }
    }
}

void RemoveItemBonuses(CSimulation& game, CUnit* unit, const std::vector<CItemBonus>& bonuses) {
    (void)game;
    for (const auto& bonus : bonuses) {
        switch (bonus.type) {
            case CItemBonusType::STRENGTH:
                // TODO: remove hero STR bonus
                break;
            case CItemBonusType::AGILITY:
                // TODO: remove hero AGI bonus
                break;
            case CItemBonusType::INTELLIGENCE:
                // TODO: remove hero INT bonus
                break;
            case CItemBonusType::DAMAGE:
                // TODO: remove damage bonus
                break;
            case CItemBonusType::ARMOR:
                // TODO: remove armor bonus
                break;
            case CItemBonusType::MAX_LIFE:
                unit->SetMaxLife(unit->GetMaxLife() - bonus.value);
                break;
            case CItemBonusType::MAX_MANA:
                unit->SetMaxMana(unit->GetMaxMana() - bonus.value);
                break;
            case CItemBonusType::MOVE_SPEED:
                unit->SetSpeed(unit->GetSpeed() - bonus.value);
                break;
            default:
                break;
        }
    }
}
