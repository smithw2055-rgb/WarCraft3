#include "item_bonuses.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"

namespace {

void ApplyGenericFallbackBonus(CUnit& unit, CItemBonusType type, int v) {
    switch (type) {
        case CItemBonusType::STRENGTH:
            unit.AddBonusMaxLife(static_cast<float>(v) * 25.0f);
            break;
        case CItemBonusType::AGILITY:
            unit.AddBonusArmor(v >= 0 ? (v * 33 + 50) / 100 : -((-v * 33 + 50) / 100));
            break;
        case CItemBonusType::INTELLIGENCE:
            unit.AddBonusMaxMana(v * 15);
            break;
        case CItemBonusType::DAMAGE:
            unit.AddBonusDamage(v);
            break;
        case CItemBonusType::ARMOR:
            unit.AddBonusArmor(v);
            break;
        case CItemBonusType::ATTACK_SPEED:
            unit.AddStackingAttackSpeed(static_cast<float>(v) / 100.0f);
            break;
        case CItemBonusType::MAX_LIFE:
            unit.AddBonusMaxLife(static_cast<float>(v));
            break;
        case CItemBonusType::MAX_MANA:
            unit.AddBonusMaxMana(v);
            break;
        case CItemBonusType::MOVE_SPEED:
            unit.AddBonusSpeed(v);
            break;
        case CItemBonusType::LIFE_REGEN:
            unit.AddBonusLifeRegen(static_cast<float>(v));
            break;
        case CItemBonusType::MANA_REGEN:
            unit.AddBonusManaRegen(static_cast<float>(v));
            break;
        case CItemBonusType::SIGHT_RADIUS:
        case CItemBonusType::NONE:
        default:
            break;
    }
}

void ApplyOne(CSimulation& game, CUnit* unit, const CItemBonus& bonus, int sign) {
    if (!unit) return;
    const int signedDelta = bonus.value * sign;
    if (auto* mod = game.GetSimulationModule()) {
        if (!game.IsPureEngineMode()) {
            mod->OnApplyUnitItemBonus(game, *unit, static_cast<int>(bonus.type), signedDelta);
            return;
        }
    }
    ApplyGenericFallbackBonus(*unit, bonus.type, signedDelta);
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
