// AbilityBuilder tests: timed-life summons, hex/sleep/silence semantics,
// life-drain channel, effect chains, deterministic sim RNG.
// (C++ port of warsmash_ref abilitybuilder behavior/ + CBuffTimedLife.)
#include <cmath>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/abilities/spells2.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) return false; } while (0)

namespace {

CUnitType* MakeBuilderType() {
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

CUnit* AdoptAt(CSimulation& sim, int handle, float x, float y) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeBuilderType(), 0,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

} // namespace

bool TestTimedLifeKillsBearerOnExpiry() {
    CSimulation sim;
    auto* u = AdoptAt(sim, 101, 0, 0);
    Settle(sim);
    u->AddBuff(std::make_unique<CBuffTimedLife>(War3ID("AOsf"), 0.2f), sim);
    TEST_ASSERT(u->IsSummoned());
    // 0.2s at 20 ticks/s = 4 ticks; run well past expiry.
    for (int i = 0; i < 30; i++) sim.Update();
    TEST_ASSERT(u->IsDead());
    return true;
}

bool TestHexUsesMarkerNotStun() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 111, 0, 0);
    auto* target = AdoptAt(sim, 112, 300, 0);
    Settle(sim);
    CAbilityHex hex(1001);
    hex.DoEffect(sim, caster, target);
    TEST_ASSERT(target->IsHexed());
    TEST_ASSERT(!target->IsSilenced());
    TEST_ASSERT(!target->IsAsleep());
    TEST_ASSERT(target->GetBuff(War3ID("BOhx")) != nullptr);
    // Hexed casters cannot cast.
    TEST_ASSERT(!hex.CheckCanUse(sim, target, 0));
    return true;
}

bool TestSleepWakesOnDamage() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 121, 0, 0);
    auto* target = AdoptAt(sim, 122, 300, 0);
    Settle(sim);
    CAbilitySleep sleep(1002);
    sleep.DoEffect(sim, caster, target);
    TEST_ASSERT(target->IsAsleep());
    TEST_ASSERT(target->GetBuff(War3ID("BUsl")) != nullptr);
    const float lifeBefore = target->GetLife();
    target->Damage(sim, caster, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 5.0f, 0);
    TEST_ASSERT(target->GetLife() < lifeBefore);
    TEST_ASSERT(!target->IsAsleep());
    return true;
}

bool TestSilenceBlocksCastOnly() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 131, 0, 0);
    auto* victim = AdoptAt(sim, 132, 100, 0);
    Settle(sim);
    CAbilitySilence silence(1003);
    silence.DoEffect(sim, caster, 100, 0);
    TEST_ASSERT(victim->IsSilenced());
    TEST_ASSERT(!victim->IsHexed());
    TEST_ASSERT(!victim->IsAsleep());
    TEST_ASSERT(victim->GetBuff(War3ID("BNsi")) != nullptr);
    // Caster itself is excluded from its own silence.
    TEST_ASSERT(!caster->IsSilenced());
    return true;
}

bool TestLifeDrainTicksHealCaster() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 141, 0, 0);
    auto* target = AdoptAt(sim, 142, 300, 0);
    Settle(sim);
    // Wound the caster so the leech has somewhere to go.
    caster->Damage(sim, nullptr, false, false, CAttackType::CHAOS,
                   CDamageType::NORMAL, "", 500.0f, 0);
    const float casterBefore = caster->GetLife();
    const float targetBefore = target->GetLife();
    CAbilityLifeDrain drain(1004);
    drain.DoEffect(sim, caster, target);
    // 8s channel, 1s ticks: 3s of updates = 3 drain ticks.
    for (int i = 0; i < 60; i++) sim.Update();
    TEST_ASSERT(target->GetLife() < targetBefore);
    TEST_ASSERT(caster->GetLife() > casterBefore);
    return true;
}

bool TestEffectChainDamageAndHeal() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 151, 0, 0);
    auto* foe = AdoptAt(sim, 152, 100, 0);
    auto* friend_ = AdoptAt(sim, 153, -100, 0);
    Settle(sim);
    friend_->Damage(sim, nullptr, false, false, CAttackType::CHAOS,
                    CDamageType::NORMAL, "", 300.0f, 0);
    const float foeBefore = foe->GetLife();
    const float friendBefore = friend_->GetLife();
    ABEffectChain damage;
    damage.Add(std::make_unique<ABActionDamageArea>(500.0f, 50.0f));
    damage.Run(sim, caster, nullptr, false, 0, 0);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    const float friendAfterBlast = friend_->GetLife();
    ABEffectChain heal;
    heal.Add(std::make_unique<ABActionHealArea>(500.0f, 25.0f));
    heal.Run(sim, caster, nullptr, false, 0, 0);
    TEST_ASSERT(friend_->GetLife() > friendAfterBlast);
    (void)friendBefore;
    return true;
}

bool TestBuilderRegistryRoundTrip() {
    ABEffectChain chain;
    chain.Add(std::make_unique<ABActionDamageArea>(10.0f, 1.0f));
    AbilityBuilderRegistry::Instance().Register(War3ID("ZZZ9"), std::move(chain));
    const ABEffectChain* found = AbilityBuilderRegistry::Instance().Find(War3ID("ZZZ9"));
    TEST_ASSERT(found != nullptr && !found->Empty());
    TEST_ASSERT(AbilityBuilderRegistry::Instance().Find(War3ID("ZZZ8")) == nullptr);
    return true;
}

bool TestSimRandIsDeterministic() {
    CSimulation a, b;
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT(a.SimRand01() == b.SimRand01());
    }
    CSimulation c;
    for (int i = 0; i < 16; i++) {
        float v = c.SimRand01();
        TEST_ASSERT(v >= 0.0f && v < 1.0f);
    }
    return true;
}

bool TestSummonWithoutUnitDataIsGraceful() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 161, 0, 0);
    Settle(sim);
    // No unit data registered: helper must return nullptr, not crash, and
    // no summoned callback may fire.
    bool fired = false;
    sim.SetUnitSummonedCallback([&](int, int) { fired = true; });
    CUnit* out = SummonTimedUnit(sim, caster, War3ID("nwlg"), War3ID("AOsf"),
                                 64, 64, 0, 60.0f);
    TEST_ASSERT(out == nullptr);
    TEST_ASSERT(!fired);
    return true;
}
