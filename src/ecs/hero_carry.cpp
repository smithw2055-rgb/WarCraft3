#include "ecs/hero_carry.h"

#include <iostream>

#include "ecs/ability_factory.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/ability_inventory.h"
#include "ecs/csimulation.h"
#include "ecs/item.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "data/unitdata.h"

namespace {

CAbilityHero* FindHeroAbility(CUnit* unit) {
    if (!unit) return nullptr;
    for (CAbility* ab : unit->GetAbilities()) {
        if (auto* hero = dynamic_cast<CAbilityHero*>(ab)) return hero;
    }
    return nullptr;
}

} // namespace

bool IsCarriableHero(CUnit* unit, const CSimulation& sim) {
    if (!unit || unit->IsHidden()) return false;
    const CUnitType* type = unit->GetUnitType();
    if (!type || !type->IsHero()) return false;
    CPlayer* owner = sim.GetPlayer(unit->GetPlayerIndex());
    if (!owner || owner->GetController() != CMapControl::USER) return false;
    return FindHeroAbility(unit) != nullptr;
}

CarriedHero SnapshotHero(CUnit* unit, const CSimulation& sim) {
    CarriedHero carried;
    if (!IsCarriableHero(unit, sim)) return carried;
    CAbilityHero* hero = FindHeroAbility(unit);
    carried.typeId = unit->GetTypeId();
    carried.ownerPlayer = unit->GetPlayerIndex();
    carried.level = hero->GetHeroLevel();
    carried.xp = hero->GetXP();
    carried.skillPoints = hero->GetSkillPoints();
    carried.properName = hero->GetProperName();
    const auto& learnable = hero->GetLearnableSkills();
    for (CAbility* ab : unit->GetAbilities()) {
        if (!ab || learnable.count(ab->GetCode()) == 0) continue;
        carried.learnedSkills.emplace_back(ab->GetCode(), ab->GetLevel());
    }
    if (auto* inv = dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv")))) {
        for (int slot = 0; slot < inv->GetItemCapacity(); ++slot) {
            CItem* item = inv->GetItem(slot);
            if (!item) continue;
            CarriedHero::CarriedItem ci;
            ci.typeId = item->GetTypeId();
            ci.charges = item->GetCharges();
            carried.items.push_back(ci);
        }
    }
    return carried;
}

CUnit* RespawnCarriedHero(CSimulation& sim, const CarriedHero& carried,
                          float x, float y) {
    if (carried.typeId.value == 0) return nullptr;
    CPlayer* owner = sim.GetPlayer(carried.ownerPlayer);
    if (!owner) return nullptr;
    if (!sim.GetUnitData() ||
        !sim.GetUnitData()->GetUnitType(carried.typeId)) {
        return nullptr; // hero type absent from the next map's data
    }
    CUnit* hero = sim.CreateUnit(carried.typeId, carried.ownerPlayer, x, y, 0.0f);
    if (!hero) return nullptr;
    if (CAbilityHero* heroAb = FindHeroAbility(hero)) {
        heroAb->SetHeroLevel(sim, hero, carried.level);
        heroAb->SetXP(sim, hero, carried.xp);
        heroAb->SetSkillPoints(carried.skillPoints);
        if (!carried.properName.empty()) heroAb->SetProperName(carried.properName);
    }
    // Learned skills: direct grant at the recorded level (mirrors LearnSkill
    // without spending points, which were restored above).
    for (const auto& [code, level] : carried.learnedSkills) {
        if (level <= 0) continue;
        if (CAbility* existing = hero->GetAbility(code)) {
            existing->SetLevel(level);
            continue;
        }
        auto created = AbilityFactory::CreateAbility(sim.AllocateHandleId(), code);
        if (!created) continue;
        created->SetLevel(level);
        hero->AddAbility(sim, std::move(created));
    }
    // Inventory in slot order; unplaceable items drop at the hero's feet.
    if (!carried.items.empty()) {
        if (auto* inv = dynamic_cast<CAbilityInventory*>(hero->GetAbility(War3ID("AInv")))) {
            for (const auto& ci : carried.items) {
                CItem* item = sim.CreateItem(ci.typeId, x, y);
                if (!item) continue;
                item->SetCharges(ci.charges);
                if (!inv->PickUpItem(sim, hero, item)) {
                    break; // full or powerup-consumed: rest stay on ground
                }
            }
        } else {
            for (const auto& ci : carried.items) {
                sim.CreateItem(ci.typeId, x, y);
            }
        }
    }
    return hero;
}
