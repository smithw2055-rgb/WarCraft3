// Batch-24 tests: regeneration auras, item stat passives, and the
// ability-builder aliases that share an already implemented class.
#include <cmath>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ecs/abilities/ability_return_resources.h"
#include "ecs/abilities/auras.h"
#include "ecs/abilities/spells.h"
#include "ecs/abilities/spells12.h"
#include "ecs/abilities/spells24.h"
#include "ecs/abilities/spells3.h"
#include "ecs/abilities/spells4.h"
#include "ecs/abilities/spells6.h"
#include "ecs/abilities/spells7.h"
#include "ecs/ability_factory.h"
#include "ecs/csimulation.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeSpell24Type() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 0, War3ID(),
        std::vector<War3ID>{}, false, PathingGridMovementType::FOOT,
        0.0f, 32.0f, std::vector<std::string>{},
        std::unordered_set<CUnitClassification>{},
        std::vector<CUnitAttack>{atk},
        1, std::string(""), false, true, CDefenseType::NONE,
        0.0f, 1.0f, std::unordered_set<CTargetType>{},
        600.0f, 0.0f, std::vector<War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{},
        std::unordered_map<CUpgradeClass, War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{}, CUnitRace{"Human", War3ID("H")},
        100, 50, 1, 0, 30, 0, 0, 0,
        std::unordered_set<CBuildingPathingType>{},
        std::unordered_set<CBuildingPathingType>{},
        0.5f, 0.6f, std::vector<CUnitTypeRequirement>{},
        std::vector<std::vector<CUnitTypeRequirement>>{},
        1, false, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

CUnit* AdoptAt(CSimulation& sim, int handle, int player, float x, float y) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell24Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

} // namespace

bool TestRegenAuraReachesAllies() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* caster = AdoptAt(sim, 2401, 0, 0, 0);
    auto* ally = AdoptAt(sim, 2402, 0, 200, 0);
    auto* foe = AdoptAt(sim, 2403, 1, 200, 0);
    auto* far = AdoptAt(sim, 2404, 0, 2000, 0);
    // Adopted units join the live list on the next update.
    for (int i = 0; i < 2; i++) sim.Update();

    CAbilityRegenerationAura aura(24901);
    aura.OnTick(sim, caster);
    TEST_ASSERT(std::fabs(caster->GetBonusLifeRegen() - 1.0f) < 0.01f);
    TEST_ASSERT(std::fabs(ally->GetBonusLifeRegen() - 1.0f) < 0.01f);
    TEST_ASSERT(foe->GetBonusLifeRegen() == 0.0f);
    TEST_ASSERT(far->GetBonusLifeRegen() == 0.0f);

    CAbilityManaRegenerationAura mana(24902);
    mana.OnTick(sim, caster);
    TEST_ASSERT(std::fabs(caster->GetBonusManaRegen() - 1.0f) < 0.01f);
    TEST_ASSERT(foe->GetBonusManaRegen() == 0.0f);
    return true;
}

bool TestItemPassivesApplyAndRemove() {
    CSimulation sim;
    auto* hero = AdoptAt(sim, 2411, 0, 0, 0);
    const float speedBefore = hero->GetSpeed();
    const float aspdBefore = hero->GetAttackSpeedMultiplier();

    CAbilityItemMoveSpeed boots(24911);
    boots.OnAdd(sim, hero);
    TEST_ASSERT(hero->GetSpeed() == speedBefore + 50.0f);
    boots.OnRemove(sim, hero);
    TEST_ASSERT(hero->GetSpeed() == speedBefore);

    CAbilityItemAttackSpeed haste(24912);
    haste.OnAdd(sim, hero);
    TEST_ASSERT(std::fabs(hero->GetAttackSpeedMultiplier() - aspdBefore * 1.15f) < 0.001f);
    haste.OnRemove(sim, hero);
    TEST_ASSERT(std::fabs(hero->GetAttackSpeedMultiplier() - aspdBefore) < 0.001f);

    hero->SetMaxMana(200);
    CAbilityItemManaRegen sobi(24913);
    sobi.OnAdd(sim, hero);
    TEST_ASSERT(std::fabs(hero->GetBonusManaRegen() - 100.0f) < 0.01f);
    sobi.OnRemove(sim, hero);
    TEST_ASSERT(hero->GetBonusManaRegen() == 0.0f);

    CAbilityItemVampirism mask(24914);
    mask.OnAdd(sim, hero);
    TEST_ASSERT(hero->GetBuff(War3ID("BUav")) != nullptr);
    mask.OnRemove(sim, hero);
    TEST_ASSERT(hero->GetBuff(War3ID("BUav")) == nullptr);

    auto sight = AbilityFactory::CreateAbility(24915, War3ID("Agyv"));
    TEST_ASSERT(sight != nullptr);
    sight->OnAdd(sim, hero);
    TEST_ASSERT(hero->IsDetector());
    sight->OnRemove(sim, hero);
    TEST_ASSERT(!hero->IsDetector());
    return true;
}

bool TestSpells24FactoryCoverage() {
    auto regen = AbilityFactory::CreateAbility(24921, War3ID("Aoar"));
    TEST_ASSERT(dynamic_cast<CAbilityRegenerationAura*>(regen.get()) != nullptr);
    auto regenAlias = AbilityFactory::CreateAbility(24922, War3ID("Aabr"));
    TEST_ASSERT(dynamic_cast<CAbilityRegenerationAura*>(regenAlias.get()) != nullptr);
    TEST_ASSERT(regenAlias->GetAlias() == War3ID("Aabr"));
    auto mana = AbilityFactory::CreateAbility(24923, War3ID("Aarm"));
    TEST_ASSERT(dynamic_cast<CAbilityManaRegenerationAura*>(mana.get()) != nullptr);

    TEST_ASSERT(dynamic_cast<CAbilityItemAttackSpeed*>(
        AbilityFactory::CreateAbility(24924, War3ID("AIas")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemManaRegen*>(
        AbilityFactory::CreateAbility(24925, War3ID("AIrm")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemMoveSpeed*>(
        AbilityFactory::CreateAbility(24926, War3ID("AIms")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemVampirism*>(
        AbilityFactory::CreateAbility(24927, War3ID("AIva")).get()) != nullptr);

    TEST_ASSERT(dynamic_cast<CAbilityEnduranceAura*>(
        AbilityFactory::CreateAbility(24928, War3ID("Aasl")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityCommandAura*>(
        AbilityFactory::CreateAbility(24929, War3ID("Aakb")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityReincarnation*>(
        AbilityFactory::CreateAbility(24930, War3ID("ACrn")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityCriticalStrike*>(
        AbilityFactory::CreateAbility(24931, War3ID("ANdb")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityRejuvenation*>(
        AbilityFactory::CreateAbility(24932, War3ID("AIrg")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityRaiseDead*>(
        AbilityFactory::CreateAbility(24933, War3ID("AIrd")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityStormBolt*>(
        AbilityFactory::CreateAbility(24934, War3ID("ANfb")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityReturnResources*>(
        AbilityFactory::CreateAbility(24935, War3ID("Artn")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityTrueSight*>(
        AbilityFactory::CreateAbility(24936, War3ID("Adts")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityTrueSight*>(
        AbilityFactory::CreateAbility(24937, War3ID("Abdt")).get()) != nullptr);
    return true;
}

bool TestSpells24AliasOrders() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2421, 0, 0, 0);
    caster->SetMana(1000);

    auto firebolt = AbilityFactory::CreateAbility(24941, War3ID("ANfb"));
    TEST_ASSERT(firebolt != nullptr);
    TEST_ASSERT(firebolt->GetAlias() == War3ID("ANfb"));
    TEST_ASSERT(firebolt->CheckCanUse(sim, caster, 852231));
    TEST_ASSERT(firebolt->CheckCanUse(sim, caster, 852095));
    TEST_ASSERT(!firebolt->CheckCanUse(sim, caster, 852486));

    auto rejuv = AbilityFactory::CreateAbility(24942, War3ID("AIrg"));
    TEST_ASSERT(rejuv->CheckCanUse(sim, caster, 852609));
    TEST_ASSERT(rejuv->CheckCanUse(sim, caster, 852161));

    auto raised = AbilityFactory::CreateAbility(24943, War3ID("AIrd"));
    TEST_ASSERT(raised->CheckCanUse(sim, caster, 852624));
    TEST_ASSERT(raised->CheckCanUse(sim, caster, 852197));

    TEST_ASSERT(SpellBaseOrderId(War3ID("ANfb")) == 852231);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AIrg")) == 852609);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AIrd")) == 852624);
    TEST_ASSERT(OrderIdFromString("firebolt") == 852231);
    return true;
}
