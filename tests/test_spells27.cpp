// Batch-27 tests: state passives, spell resist, magic defense, leash, replenish.
#include <cmath>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/spells27.h"
#include "ecs/ability_factory.h"
#include "ecs/buffs.h"
#include "ecs/combat/projectile.h"
#include "ecs/csimulation.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeSpell27Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell27Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim) {
    for (int i = 0; i < 2; i++) sim.Update();
}

float LifeLoss(CUnit* unit, CSimulation& sim, CUnit* source, bool isAttack,
               CAttackType attack) {
    const float before = unit->GetLife();
    unit->Damage(sim, source, isAttack, false, attack, CDamageType::MAGIC, "", 100.0f, 0);
    return before - unit->GetLife();
}

} // namespace

bool TestStatePassivesApply() {
    CSimulation sim;
    auto* eth = AdoptAt(sim, 2701, 0, 0, 0);
    auto* immune = AdoptAt(sim, 2702, 0, 10, 0);
    auto* resist = AdoptAt(sim, 2703, 0, 20, 0);
    auto* ghost = AdoptAt(sim, 2704, 0, 30, 0);
    const float collision = ghost->GetCollisionSize();

    CAbilityStatePassive ethereal(27901, War3ID("Aetl"), CPassiveState::Ethereal);
    ethereal.OnAdd(sim, eth);
    TEST_ASSERT(eth->IsEthereal());
    const float before = eth->GetLife();
    eth->Damage(sim, nullptr, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 80.0f, 0);
    TEST_ASSERT(std::fabs(eth->GetLife() - before) < 0.01f);
    ethereal.OnRemove(sim, eth);
    TEST_ASSERT(!eth->IsEthereal());

    CAbilityStatePassive magic(27902, War3ID("Amim"), CPassiveState::MagicImmune);
    magic.OnAdd(sim, immune);
    TEST_ASSERT(immune->IsMagicImmune());
    const float immuneBefore = immune->GetLife();
    immune->Damage(sim, nullptr, false, false, CAttackType::MAGIC, CDamageType::MAGIC, "", 80.0f, 0);
    TEST_ASSERT(std::fabs(immune->GetLife() - immuneBefore) < 0.01f);
    magic.OnRemove(sim, immune);
    TEST_ASSERT(!immune->IsMagicImmune());

    CAbilityStatePassive skin(27903, War3ID("Arsk"), CPassiveState::Resistant);
    skin.OnAdd(sim, resist);
    resist->AddBuff(std::make_unique<CBuffSlow>(20.0f, 0.5f), sim);
    CBuff* slow = resist->GetBuff(War3ID("Bslo"));
    TEST_ASSERT(slow != nullptr);
    TEST_ASSERT(std::fabs(slow->GetRemaining() - 5.0f) < 0.01f);
    skin.OnRemove(sim, resist);
    auto* plain = AdoptAt(sim, 2705, 0, 40, 0);
    plain->AddBuff(std::make_unique<CBuffSlow>(20.0f, 0.5f), sim);
    TEST_ASSERT(std::fabs(plain->GetBuff(War3ID("Bslo"))->GetRemaining() - 20.0f) < 0.01f);

    CAbilityStatePassive shade(27904, War3ID("Agho"), CPassiveState::Ghost);
    shade.OnAdd(sim, ghost);
    TEST_ASSERT(ghost->IsInvisible());
    TEST_ASSERT(ghost->GetCollisionSize() == 0.0f);
    shade.OnRemove(sim, ghost);
    TEST_ASSERT(!ghost->IsInvisible());
    TEST_ASSERT(std::fabs(ghost->GetCollisionSize() - collision) < 0.01f);
    return true;
}

bool TestSpellResistAndMixedDefense() {
    CSimulation sim;
    auto* plain = AdoptAt(sim, 2711, 0, 0, 0);
    auto* bracers = AdoptAt(sim, 2712, 0, 10, 0);
    auto* mixed = AdoptAt(sim, 2713, 0, 20, 0);
    auto* attacker = AdoptAt(sim, 2714, 0, 30, 0);

    CAbilityItemSpellResist resist(27911);
    resist.OnAdd(sim, bracers);
    const float plainSpell = LifeLoss(plain, sim, nullptr, false, CAttackType::SPELLS);
    const float reduced = LifeLoss(bracers, sim, nullptr, false, CAttackType::SPELLS);
    TEST_ASSERT(plainSpell > 1.0f);
    TEST_ASSERT(std::fabs(reduced - plainSpell * 0.67f) < 0.5f);
    resist.OnRemove(sim, bracers);
    const float restored = LifeLoss(bracers, sim, nullptr, false, CAttackType::SPELLS);
    TEST_ASSERT(std::fabs(restored - plainSpell) < 0.5f);

    CAbilityItemMixedDefense defense(27912);
    defense.OnAdd(sim, mixed);
    auto* piercePlain = AdoptAt(sim, 2715, 0, 40, 0);
    const float plainPierce = LifeLoss(piercePlain, sim, attacker, false, CAttackType::PIERCE);
    const float mixedPierce = LifeLoss(mixed, sim, attacker, false, CAttackType::PIERCE);
    TEST_ASSERT(plainPierce > 1.0f);
    TEST_ASSERT(std::fabs(mixedPierce - plainPierce * 0.80f) < 0.5f);

    auto* dealtPlain = AdoptAt(sim, 2716, 0, 50, 0);
    auto* dealtTarget = AdoptAt(sim, 2717, 0, 60, 0);
    defense.OnAdd(sim, attacker);
    const float without = LifeLoss(dealtPlain, sim, piercePlain, true, CAttackType::NORMAL);
    const float with = LifeLoss(dealtTarget, sim, attacker, true, CAttackType::NORMAL);
    TEST_ASSERT(without > 1.0f);
    TEST_ASSERT(std::fabs(with - without * 1.20f) < 0.5f);

    const float mixedBefore = mixed->GetLife();
    const float attackerBefore = attacker->GetLife();
    CProjectile shot;
    shot.attackerHandleId = attacker->GetHandleId();
    shot.targetHandleId = mixed->GetHandleId();
    shot.x = mixed->GetX();
    shot.y = mixed->GetY();
    shot.speed = 2000.0f;
    shot.damage = 30.0f;
    shot.attackType = CAttackType::PIERCE;
    shot.maxTravel = 4000.0f;
    sim.SpawnProjectile(std::move(shot));
    sim.Update();
    TEST_ASSERT(mixed->GetLife() == mixedBefore);
    sim.Update();
    TEST_ASSERT(attacker->GetLife() < attackerBefore);
    return true;
}

bool TestMagicDefenseToggles() {
    CSimulation sim;
    auto* unit = AdoptAt(sim, 2721, 0, 0, 0);
    CAbilityMagicDefense defense(27921);
    TEST_ASSERT(std::fabs(unit->GetAttackSpeedMultiplier() - 1.0f) < 0.01f);
    defense.DoEffect(sim, unit);
    TEST_ASSERT(unit->IsMagicImmune());
    TEST_ASSERT(std::fabs(unit->GetAttackSpeedMultiplier() - 0.5f) < 0.01f);
    defense.DoEffect(sim, unit);
    TEST_ASSERT(!unit->IsMagicImmune());
    TEST_ASSERT(std::fabs(unit->GetAttackSpeedMultiplier() - 1.0f) < 0.01f);
    defense.DoEffect(sim, unit);
    defense.OnRemove(sim, unit);
    TEST_ASSERT(!unit->IsMagicImmune());
    TEST_ASSERT(std::fabs(unit->GetAttackSpeedMultiplier() - 1.0f) < 0.01f);
    return true;
}

bool TestMagicLeashStuns() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* caster = AdoptAt(sim, 2731, 0, 0, 0);
    auto* foe = AdoptAt(sim, 2732, 1, 40, 0);
    const float before = foe->GetLife();
    CAbilityMagicLeash leash(27931);
    leash.DoEffect(sim, caster, foe);
    TEST_ASSERT(foe->HasBuff(War3ID("Bstn")));
    TEST_ASSERT(foe->HasBuff(War3ID("Bpoi")));
    TEST_ASSERT(before - foe->GetLife() > 1.0f);
    return true;
}

bool TestReplenishRestoresAllies() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* statue = AdoptAt(sim, 2741, 0, 0, 0);
    auto* ally = AdoptAt(sim, 2742, 0, 100, 0);
    auto* foe = AdoptAt(sim, 2743, 1, 100, 0);
    auto* far = AdoptAt(sim, 2744, 0, 2000, 0);
    Settle(sim);
    ally->SetLife(sim, ally->GetMaxLife() - 40.0f);
    foe->SetLife(sim, foe->GetMaxLife() - 40.0f);
    far->SetLife(sim, far->GetMaxLife() - 40.0f);
    const float allyBefore = ally->GetLife();
    const float foeBefore = foe->GetLife();
    const float farBefore = far->GetLife();
    CAbilityReplenish life(27941, War3ID("Arpl"), true);
    life.DoEffect(sim, statue);
    TEST_ASSERT(std::fabs(ally->GetLife() - (allyBefore + 10.0f)) < 0.01f);
    TEST_ASSERT(std::fabs(foe->GetLife() - foeBefore) < 0.01f);
    TEST_ASSERT(std::fabs(far->GetLife() - farBefore) < 0.01f);

    ally->AddBonusMaxMana(100);
    ally->SetMana(0.0f);
    foe->AddBonusMaxMana(100);
    foe->SetMana(0.0f);
    CAbilityReplenish mana(27942, War3ID("Arpm"), false);
    mana.DoEffect(sim, statue);
    TEST_ASSERT(std::fabs(ally->GetMana() - 10.0f) < 0.01f);
    TEST_ASSERT(std::fabs(foe->GetMana()) < 0.01f);
    return true;
}

bool TestSpells27FactoryCoverage() {
    const char* codes[] = {
        "Aetl", "Amim", "Arsk", "Agho", "AIsr", "AIdd", "Amdf",
        "Amls", "Arpl", "Arpm", "Asph", "AIco"
    };
    for (const char* code : codes) {
        auto ability = AbilityFactory::CreateAbility(27950, War3ID(code));
        if (!ability) {
            std::cerr << "missing factory ability " << code << "\n";
            return false;
        }
    }
    auto charm = AbilityFactory::CreateAbility(27951, War3ID("AIco"));
    TEST_ASSERT(charm->GetCode() == War3ID("ANch"));
    TEST_ASSERT(charm->GetAlias() == War3ID("AIco"));
    return true;
}

bool TestSpells27OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("Amdf")) == 852478);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Amdf"), 852479));
    TEST_ASSERT(SpellBaseOrderId(War3ID("Amls")) == 852480);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Arpl")) == 852545);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Arpm")) == 852548);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AIco")) == 852267);
    return true;
}
