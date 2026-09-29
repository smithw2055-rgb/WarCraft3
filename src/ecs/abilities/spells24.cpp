#include "spells24.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"

CAbilitySelfStatPassive::CAbilitySelfStatPassive(int handleId, War3ID code,
                                                 CAuraEffectType type, float value,
                                                 bool percent)
    : CAbility(handleId, code, CAbilityCategory::ITEM)
    , type_(type), value_(value), percent_(percent) {
    iconShowing_ = false;
    permanent_ = true;
}

void CAbilitySelfStatPassive::OnAdd(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    std::vector<CBuffAuraStat::Bonus> bonuses{{type_, value_, percent_}};
    unit->AddBuff(std::make_unique<CBuffAuraStat>(GetCode(), std::move(bonuses)), game);
}

void CAbilitySelfStatPassive::OnRemove(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    unit->RemoveBuff(GetCode(), game);
}

CAbilityItemAttackSpeed::CAbilityItemAttackSpeed(int handleId)
    : CAbilitySelfStatPassive(handleId, War3ID("AIas"),
                              CAuraEffectType::ATTACK_SPEED, 0.15f, true) {}

CAbilityItemManaRegen::CAbilityItemManaRegen(int handleId)
    : CAbilitySelfStatPassive(handleId, War3ID("AIrm"),
                              CAuraEffectType::MANA_REGEN, 0.50f, true) {}

CAbilityItemMoveSpeed::CAbilityItemMoveSpeed(int handleId)
    : CAbilitySelfStatPassive(handleId, War3ID("AIms"),
                              CAuraEffectType::MOVE_SPEED, 50.0f, false) {}

CAbilityItemVampirism::CAbilityItemVampirism(int handleId)
    : CAbility(handleId, War3ID("AIva"), CAbilityCategory::ITEM) {
    iconShowing_ = false;
    permanent_ = true;
}

void CAbilityItemVampirism::OnAdd(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    unit->AddBuff(std::make_unique<CBuffLifesteal>(1.0e9f, 0.15f), game);
}

void CAbilityItemVampirism::OnRemove(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    unit->RemoveBuff(War3ID("BUav"), game);
}
