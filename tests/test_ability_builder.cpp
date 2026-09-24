// AbilityBuilder tests: timed-life summons, hex/sleep/silence semantics,
// life-drain channel, effect chains, deterministic sim RNG.
// (C++ port of warsmash_ref abilitybuilder behavior/ + CBuffTimedLife.)
#include <cmath>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "data/gameplay_constants.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/spells.h"
#include "ecs/abilities/spells2.h"
#include "ecs/abilities/spells3.h"
#include "ecs/abilities/spells5.h"
#include "ecs/behaviors/behavior_channel.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "resource/mpq_storm.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) return false; } while (0)

namespace {

CUnitType* MakeBuilderTypeWith(std::unordered_set<CUnitClassification> cls) {
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
        std::move(cls),
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

CUnitType* MakeBuilderType();

CUnit* AdoptAt(CSimulation& sim, int handle, float x, float y) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeBuilderType(), 0,
                                                 War3ID("hfoo"), x, y, 0));
}

CUnitType* MakeBuilderType() { return MakeBuilderTypeWith({}); }

CUnit* AdoptUndeadAt(CSimulation& sim, int handle, float x, float y) {
    return sim.AdoptUnit(std::make_unique<CUnit>(
        handle, MakeBuilderTypeWith({CUnitClassification::UNDEAD}), 0,
        War3ID("hfoo"), x, y, 0));
}

CUnit* AdoptAtFor(CSimulation& sim, int handle, int player, float x, float y) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeBuilderType(), player,
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

bool TestSummonWithoutUnitDataIsGraceful() {    CSimulation sim;
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

bool TestHeroSkillLevelsAndGrants() {
    CSimulation sim;
    auto* hero = AdoptAt(sim, 171, 0, 0);
    Settle(sim);
    CAbilityHero skill(2001);
    skill.AddLearnableSkill(War3ID("AHtb")); // Storm Bolt exists in factory
    skill.AddLearnableSkill(War3ID("ZZZ7")); // unknown: point must be kept
    skill.SetSkillPoints(3);
    TEST_ASSERT(skill.SelectHeroSkill(sim, hero, War3ID("AHtb")));
    CAbility* granted = hero->GetAbility(War3ID("AHtb"));
    TEST_ASSERT(granted != nullptr);
    TEST_ASSERT(granted->GetLevel() == 1);
    TEST_ASSERT(skill.GetSkillPoints() == 2);
    // Second point levels the existing ability instead of duplicating it.
    TEST_ASSERT(skill.SelectHeroSkill(sim, hero, War3ID("AHtb")));
    TEST_ASSERT(granted->GetLevel() == 2);
    TEST_ASSERT(skill.GetSkillPoints() == 1);
    // Unknown skill keeps the point.
    TEST_ASSERT(!skill.SelectHeroSkill(sim, hero, War3ID("ZZZ7")));
    TEST_ASSERT(skill.GetSkillPoints() == 1);
    return true;
}

// StormBolt.j pattern: single-target damage + stun composed as a chain.
bool TestABSingleTargetNukeAndStun() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 181, 0, 0);
    auto* target = AdoptAt(sim, 182, 300, 0);
    Settle(sim);
    const float before = target->GetLife();
    ABEffectChain chain;
    chain.Add(std::make_unique<ABActionDamageTarget>(100.0f));
    chain.Add(std::make_unique<ABActionStunTarget>(2.0f));
    chain.Run(sim, caster, target, false, 0, 0);
    TEST_ASSERT(target->GetLife() < before);
    TEST_ASSERT(target->GetBuff(War3ID("Bstn")) != nullptr);
    // Dead targets are skipped.
    target->SetLife(sim, 0.0f);
    const float deadLife = target->GetLife();
    chain.Run(sim, caster, target, false, 0, 0);
    TEST_ASSERT(target->GetLife() == deadLife);
    return true;
}

// Blizzard.j pattern: iterate units in range, damage each (one wave).
bool TestABForEachInRangeBlizzardWave() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 191, 0, 0);
    auto* inRange = AdoptAt(sim, 192, 100, 0);
    auto* outRange = AdoptAt(sim, 193, 900, 0);
    Settle(sim);
    const float inBefore = inRange->GetLife();
    const float outBefore = outRange->GetLife();
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDamageTarget>(40.0f));
    ABEffectChain wave;
    wave.Add(std::make_unique<ABActionForEachInRange>(300.0f, std::move(perUnit)));
    wave.Run(sim, caster, nullptr, true, 0, 0);
    TEST_ASSERT(inRange->GetLife() < inBefore);
    TEST_ASSERT(outRange->GetLife() == outBefore);
    // maxCount caps the iteration.
    auto* second = AdoptAt(sim, 194, 120, 0);
    Settle(sim);
    const float secondBefore = second->GetLife();
    ABEffectChain one;
    one.Add(std::make_unique<ABActionDamageTarget>(40.0f));
    ABEffectChain capped;
    capped.Add(std::make_unique<ABActionForEachInRange>(300.0f, std::move(one),
                                                        false, 1));
    capped.Run(sim, caster, nullptr, true, 0, 0);
    TEST_ASSERT((inRange->GetLife() < inBefore - 40.0f + 1.0f) !=
                (second->GetLife() < secondBefore));
    return true;
}

// MassTeleport primitive: relocate the unit target onto the point.
bool TestABTeleportTarget() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 201, 0, 0);
    auto* target = AdoptAt(sim, 202, 100, 0);
    Settle(sim);
    ABActionTeleportTarget tp;
    tp.Run(sim, caster, target, true, 1500, 1500);
    TEST_NEAR(target->GetX(), 1500.0f, 65.0f);
    TEST_NEAR(target->GetY(), 1500.0f, 65.0f);
    // No point: no-op. Dead: no-op.
    tp.Run(sim, caster, target, false, 0, 0);
    TEST_NEAR(target->GetX(), 1500.0f, 65.0f);
    target->SetLife(sim, 0.0f);
    tp.Run(sim, caster, target, true, 0, 0);
    TEST_ASSERT(target->IsDead());
    return true;
}

bool TestABIfBranch() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 211, 0, 0);
    auto* living = AdoptAt(sim, 212, 100, 0);
    auto* dead = AdoptAt(sim, 213, 120, 0);
    Settle(sim);
    dead->SetLife(sim, 0.0f);
    ABEffectChain heal;
    heal.Add(std::make_unique<ABActionHealTarget>(50.0f));
    ABEffectChain harm;
    harm.Add(std::make_unique<ABActionDamageTarget>(10.0f));
    // Wound the living unit first so the heal is observable.
    living->Damage(sim, nullptr, false, false, CAttackType::CHAOS,
                   CDamageType::NORMAL, "", 200.0f, 0);
    const float livingBefore = living->GetLife();
    ABActionIf branch(std::make_shared<ABCondTargetAlive>(), std::move(heal),
                      std::move(harm));
    branch.Run(sim, caster, living, false, 0, 0);
    TEST_ASSERT(living->GetLife() > livingBefore);
    const float deadLife = dead->GetLife();
    branch.Run(sim, caster, dead, false, 0, 0);
    TEST_ASSERT(dead->GetLife() == deadLife); // harm skips dead targets
    return true;
}

// StormBolt missile: nothing lands at cast; damage + stun arrive together
// on impact; a mid-flight death fizzles everything.
bool TestABMissileStun() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 221, 0, 0);
    auto* target = AdoptAt(sim, 222, 300, 0);
    auto* doomed = AdoptAt(sim, 223, 300, 0);
    Settle(sim);
    const float before = target->GetLife();
    ABActionMissileStun bolt(100.0f, 2.0f, 800.0f);
    bolt.Run(sim, caster, target, false, 0, 0);
    TEST_ASSERT(target->GetBuff(War3ID("Bstn")) == nullptr);
    TEST_ASSERT(target->GetLife() == before);
    for (int i = 0; i < 30; i++) sim.Update();
    TEST_ASSERT(target->GetLife() < before);
    TEST_ASSERT(target->GetBuff(War3ID("Bstn")) != nullptr);
    // A target that dies mid-flight takes neither damage nor stun, even
    // after the missile would have arrived (300 units at 50/s = 6s).
    ABActionMissileStun bolt2(100.0f, 2.0f, 50.0f);
    bolt2.Run(sim, caster, doomed, false, 0, 0);
    doomed->SetLife(sim, 0.0f);
    for (int i = 0; i < 150; i++) sim.Update();
    TEST_ASSERT(doomed->GetBuff(War3ID("Bstn")) == nullptr);
    return true;
}

// Abolish Magic rule: dispel strips debuffs but spares timed life.
bool TestABDispelArea() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 231, 0, 0);
    auto* victim = AdoptAt(sim, 232, 100, 0);
    Settle(sim);
    victim->AddBuff(std::make_unique<CBuffSlow>(30.0f, 0.5f), sim);
    victim->AddBuff(std::make_unique<CBuffTimedLife>(War3ID("AOsf"), 60.0f), sim);
    TEST_ASSERT(victim->GetBuff(War3ID("Bslo")) != nullptr);
    ABActionDispelArea dispel(500.0f, false, true);
    dispel.Run(sim, caster, nullptr, false, 0, 0);
    TEST_ASSERT(victim->GetBuff(War3ID("Bslo")) == nullptr);
    TEST_ASSERT(victim->GetBuff(War3ID("AOsf")) != nullptr);
    return true;
}

bool TestABKillAndResurrect() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 241, 0, 0);
    auto* target = AdoptAt(sim, 242, 100, 0);
    Settle(sim);
    ABActionKillTarget kill;
    kill.Run(sim, caster, target, false, 0, 0);
    TEST_ASSERT(target->IsDead());
    ABActionResurrectTarget rez;
    rez.Run(sim, caster, target, false, 0, 0);
    TEST_ASSERT(!target->IsDead());
    TEST_ASSERT(target->GetLife() == target->GetMaxLife());
    // Resurrecting the living is a no-op.
    target->Damage(sim, nullptr, false, false, CAttackType::CHAOS,
                   CDamageType::NORMAL, "", 50.0f, 0);
    const float wounded = target->GetLife();
    rez.Run(sim, caster, target, false, 0, 0);
    TEST_ASSERT(target->GetLife() == wounded);
    return true;
}

bool TestABCreateUnit() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 251, 0, 0);
    Settle(sim);
    // Without unit data: graceful no-op (ref summon-without-data rule).
    {
        const size_t before = sim.GetUnits().size();
        ABActionCreateUnit spawn(War3ID("hfoo"), 2);
        spawn.Run(sim, caster, nullptr, true, 400, 400);
        TEST_ASSERT(sim.GetUnits().size() == before);
    }
    // With real game data: creates `count` units for the caster's player.
    MpqManager mpq;
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    bool opened = mpq.Open(res + "War3x.mpq") || mpq.Open(res + "war3x.mpq");
    if (!opened) return true;
    CGameplayConstants gc;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gc);
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.SetUnitData(&unitData);
    const size_t before = sim.GetUnits().size();
    ABActionCreateUnit spawn(War3ID("hfoo"), 2);
    spawn.Run(sim, caster, nullptr, true, 400, 400);
    for (int i = 0; i < 3; i++) sim.Update(); // join new units
    TEST_ASSERT(sim.GetUnits().size() == before + 2);
    TEST_ASSERT(sim.GetUnits().back()->GetTypeId() == War3ID("hfoo"));
    return true;
}

bool TestStormBoltChainParity() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 261, 0, 0);
    auto* target = AdoptAt(sim, 262, 300, 0);
    Settle(sim);
    CAbilityStormBolt bolt(2601);
    const float before = target->GetLife();
    bolt.DoEffect(sim, caster, target);
    // Homing missile: nothing at cast time, damage + stun on impact.
    TEST_ASSERT(target->GetLife() == before);
    TEST_ASSERT(target->GetBuff(War3ID("Bstn")) == nullptr);
    for (int i = 0; i < 30; i++) sim.Update();
    TEST_ASSERT(target->GetLife() < before);
    TEST_ASSERT(target->GetBuff(War3ID("Bstn")) != nullptr);
    return true;
}

bool TestHolyLightUndeadBranch() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 271, 0, 0);
    auto* ally = AdoptAt(sim, 272, 100, 0);
    auto* undead = AdoptUndeadAt(sim, 273, 200, 0);
    Settle(sim);
    CAbilityHolyLight holy(2701);
    // Wounded living ally is healed (default 200).
    ally->Damage(sim, nullptr, false, false, CAttackType::CHAOS,
                 CDamageType::NORMAL, "", 1000.0f, 0);
    const float allyBefore = ally->GetLife();
    holy.DoEffect(sim, caster, ally);
    TEST_ASSERT(ally->GetLife() > allyBefore);
    // Undead takes half (100) as a DIVINE bolt instead of being healed.
    const float undeadBefore = undead->GetLife();
    holy.DoEffect(sim, caster, undead);
    TEST_ASSERT(undead->GetLife() < undeadBefore);
    TEST_ASSERT(undead->GetBuff(War3ID("Bstn")) == nullptr);
    // Dead targets are skipped on both branches.
    undead->SetLife(sim, 0.0f);
    holy.DoEffect(sim, caster, undead);
    TEST_ASSERT(undead->IsDead());
    return true;
}

bool TestWarStompChainParity() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 281, 0, 0);
    auto* foe = AdoptAt(sim, 282, 100, 0);
    auto* far = AdoptAt(sim, 283, 900, 0);
    Settle(sim);
    const float foeBefore = foe->GetLife();
    const float farBefore = far->GetLife();
    const float casterBefore = caster->GetLife();
    CAbilityWarStomp stomp(2801);
    stomp.DoEffect(sim, caster);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(foe->GetBuff(War3ID("Bstn")) != nullptr);
    TEST_ASSERT(far->GetLife() == farBefore);
    TEST_ASSERT(caster->GetLife() == casterBefore);
    return true;
}

bool TestDeathCoilAllyUndeadHealed() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    sim.GetPlayer(0)->SetAlliance(1, CAllianceType::PASSIVE, true);
    sim.GetPlayer(1)->SetAlliance(0, CAllianceType::PASSIVE, true);
    auto* caster = AdoptAtFor(sim, 291, 0, 0, 0);
    auto* allyUndead = sim.AdoptUnit(std::make_unique<CUnit>(
        292, MakeBuilderTypeWith({CUnitClassification::UNDEAD}), 1,
        War3ID("hfoo"), 100, 0, 0));
    auto* allyLiving = AdoptAtFor(sim, 293, 1, 200, 0);
    Settle(sim);
    CAbilityDeathCoil coil(2901);
    // Allied undead is healed (default 200).
    allyUndead->Damage(sim, nullptr, false, false, CAttackType::CHAOS,
                       CDamageType::NORMAL, "", 1000.0f, 0);
    const float undeadBefore = allyUndead->GetLife();
    coil.DoEffect(sim, caster, allyUndead);
    TEST_ASSERT(allyUndead->GetLife() > undeadBefore);
    // Allied living is harmed instead (default 100).
    const float livingBefore = allyLiving->GetLife();
    coil.DoEffect(sim, caster, allyLiving);
    TEST_ASSERT(allyLiving->GetLife() < livingBefore);
    // Alliance broken: undead is harmed too.
    sim.GetPlayer(0)->SetAlliance(1, CAllianceType::PASSIVE, false);
    sim.GetPlayer(1)->SetAlliance(0, CAllianceType::PASSIVE, false);
    const float foeBefore = allyUndead->GetLife();
    coil.DoEffect(sim, caster, allyUndead);
    TEST_ASSERT(allyUndead->GetLife() < foeBefore);
    return true;
}

bool TestBlizzardWavesTotal() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 301, -500, 0);
    auto* target = AdoptAt(sim, 302, 100, 0);
    auto* far = AdoptAt(sim, 303, 900, 0);
    Settle(sim);
    CAbilityBlizzard blizzard(3001);
    const float before = target->GetLife();
    const float farBefore = far->GetLife();
    blizzard.DoEffect(sim, caster, 0, 0);
    // Default 6 waves of 30: area target hurt, outsider untouched.
    TEST_ASSERT(target->GetLife() < before);
    TEST_ASSERT(before - target->GetLife() == 180.0f);
    TEST_ASSERT(far->GetLife() == farBefore);
    return true;
}

bool TestFrostNovaChainParity() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 311, 0, 0);
    auto* primary = AdoptAt(sim, 312, 100, 0);
    auto* nearby = AdoptAt(sim, 313, 150, 0);
    auto* far = AdoptAt(sim, 314, 900, 0);
    Settle(sim);
    CAbilityFrostNova nova(3101);
    const float primaryBefore = primary->GetLife();
    const float nearbyBefore = nearby->GetLife();
    const float farBefore = far->GetLife();
    const float casterBefore = caster->GetLife();
    nova.DoEffect(sim, caster, primary);
    // Primary: full 100, no slow (excluded from its own splash).
    TEST_ASSERT(primaryBefore - primary->GetLife() == 100.0f);
    TEST_ASSERT(primary->GetBuff(War3ID("Bslo")) == nullptr);
    // Nearby: splash 50 + slow.
    TEST_ASSERT(nearbyBefore - nearby->GetLife() == 50.0f);
    TEST_ASSERT(nearby->GetBuff(War3ID("Bslo")) != nullptr);
    // Outsider untouched; caster in radius takes the legacy splash.
    TEST_ASSERT(far->GetLife() == farBefore);
    TEST_ASSERT(caster->GetLife() < casterBefore);
    return true;
}

bool TestFanOfKnivesChainParity() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* caster = AdoptAtFor(sim, 321, 0, 0, 0);
    auto* foe = AdoptAtFor(sim, 322, 1, 100, 0);
    auto* ally = AdoptAtFor(sim, 323, 0, -100, 0);
    auto* far = AdoptAtFor(sim, 324, 1, 5000, 5000);
    Settle(sim);
    CAbilityFanOfKnives spell(3201);
    const float foeBefore = foe->GetLife();
    spell.DoEffect(sim, caster);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(ally->GetLife() == ally->GetMaxLife());
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife());
    return true;
}

bool TestRainOfFireChainParity() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 331, -500, 0);
    auto* foe = AdoptAt(sim, 332, 100, 100);
    auto* far = AdoptAt(sim, 333, 5000, 5000);
    Settle(sim);
    CAbilityRainOfFire spell(3301);
    const float foeBefore = foe->GetLife();
    const float farBefore = far->GetLife();
    spell.DoEffect(sim, caster, 100, 100);
    // Default 6 waves of 30.
    TEST_ASSERT(foeBefore - foe->GetLife() == 180.0f);
    TEST_ASSERT(far->GetLife() == farBefore);
    return true;
}

bool TestFlameStrikeChainParity() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 341, -500, 0);
    auto* foe = AdoptAt(sim, 342, 100, 100);
    auto* far = AdoptAt(sim, 343, 5000, 5000);
    Settle(sim);
    CAbilityFlameStrike strike(3401);
    const float foeBefore = foe->GetLife();
    const float farBefore = far->GetLife();
    strike.DoEffect(sim, caster, 100, 100);
    // Default 3 waves of 80.
    TEST_ASSERT(foeBefore - foe->GetLife() == 240.0f);
    TEST_ASSERT(far->GetLife() == farBefore);
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife());
    return true;
}

bool TestDeathAndDecayChainParity() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* caster = AdoptAtFor(sim, 351, 0, 0, 0);
    auto* foe = AdoptAtFor(sim, 352, 1, 100, 100);
    auto* ally = AdoptAtFor(sim, 353, 0, 150, 100);
    Settle(sim);
    CAbilityDeathAndDecay spell(3501);
    const float foeBefore = foe->GetLife();
    const float allyBefore = ally->GetLife();
    spell.DoEffect(sim, caster, 125, 100);
    // Corrupts friend and foe alike; caster exempt.
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(ally->GetLife() < allyBefore);
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife());
    return true;
}

bool TestShockwaveChainParity() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 361, 0, 0);
    auto* ahead = AdoptAt(sim, 362, 400, 0);
    auto* aside = AdoptAt(sim, 363, 400, 500);
    Settle(sim);
    CAbilityShockwave spell(3601);
    const float aheadBefore = ahead->GetLife();
    const float asideBefore = aside->GetLife();
    const float casterBefore = caster->GetLife();
    spell.DoEffect(sim, caster, ahead);
    TEST_ASSERT(ahead->GetLife() < aheadBefore);
    TEST_ASSERT(aside->GetLife() == asideBefore);
    TEST_ASSERT(caster->GetLife() == casterBefore);
    return true;
}

bool TestImpaleChainParity() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 371, 0, 0);
    auto* target = AdoptAt(sim, 372, 300, 0);
    auto* behind = AdoptAt(sim, 373, 500, 0);
    auto* offline = AdoptAt(sim, 374, 300, 300);
    Settle(sim);
    CAbilityImpale spell(3701);
    const float targetBefore = target->GetLife();
    const float behindBefore = behind->GetLife();
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetLife() < targetBefore);
    TEST_ASSERT(target->GetBuff(War3ID("Bstn")) != nullptr);
    TEST_ASSERT(behind->GetLife() < behindBefore);
    TEST_ASSERT(behind->GetBuff(War3ID("Bstn")) != nullptr);
    TEST_ASSERT(offline->GetLife() == offline->GetMaxLife());
    TEST_ASSERT(offline->GetBuff(War3ID("Bstn")) == nullptr);
    // Dead aim target aborts the whole line (legacy early-out).
    target->SetLife(sim, 0.0f);
    behind->SetLife(sim, behind->GetMaxLife());
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(behind->GetLife() == behind->GetMaxLife());
    return true;
}

bool TestChainLightningBounceParity() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 381, 0, 0);
    auto* t0 = AdoptAt(sim, 382, 2000, 0);
    auto* t1 = AdoptAt(sim, 383, 2200, 0);
    auto* t2 = AdoptAt(sim, 384, 2300, 0);
    auto* far = AdoptAt(sim, 385, 9000, 0);
    Settle(sim);
    CAbilityChainLightning spell(3801);
    const float b0 = t0->GetLife();
    const float b1 = t1->GetLife();
    const float b2 = t2->GetLife();
    spell.DoEffect(sim, caster, t0);
    // Default 85 with 0.25 falloff: first hit full, later hops decayed;
    // no unit is struck twice in one cast.
    TEST_ASSERT(b0 - t0->GetLife() == 85.0f);
    TEST_ASSERT(t1->GetLife() < b1);
    TEST_ASSERT(t2->GetLife() < b2);
    TEST_ASSERT((b2 - t2->GetLife()) < (b0 - t0->GetLife()));
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    return true;
}

bool TestResurrectChainParity() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* caster = AdoptAtFor(sim, 401, 0, 0, 0);
    auto* fallen = AdoptAtFor(sim, 402, 0, 100, 0);
    auto* living = AdoptAtFor(sim, 403, 0, 150, 0);
    auto* farFallen = AdoptAtFor(sim, 404, 0, 5000, 0);
    Settle(sim);
    fallen->SetLife(sim, 0.0f);
    farFallen->SetLife(sim, 0.0f);
    TEST_ASSERT(fallen->IsDead() && farFallen->IsDead());
    CAbilityResurrect spell(4001);
    spell.DoEffect(sim, caster, 100, 0);
    TEST_ASSERT(!fallen->IsDead());
    TEST_ASSERT(fallen->GetLife() == fallen->GetMaxLife());
    TEST_ASSERT(farFallen->IsDead());
    TEST_ASSERT(!living->IsDead());
    return true;
}

bool TestThunderClapChainParity() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 411, 0, 0);
    auto* foe = AdoptAt(sim, 412, 100, 0);
    auto* far = AdoptAt(sim, 413, 900, 0);
    Settle(sim);
    CAbilityThunderClap clap(4101);
    const float foeBefore = foe->GetLife();
    clap.DoEffect(sim, caster);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(foe->GetBuff(War3ID("Bslo")) != nullptr);
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife());
    return true;
}

bool TestManaBurnChainParity() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 421, 0, 0);
    auto* target = AdoptAt(sim, 422, 100, 0);
    auto* poor = AdoptAt(sim, 423, 200, 0);
    Settle(sim);
    CAbilityManaBurn burn(4201);
    target->SetMana(200.0f);
    poor->SetMana(30.0f);
    // Read back: synthetic types may clamp mana below the request.
    const float targetMana = target->GetMana();
    const float poorMana = poor->GetMana();
    const float targetLife = target->GetLife();
    const float poorLife = poor->GetLife();
    burn.DoEffect(sim, caster, target);
    burn.DoEffect(sim, caster, poor);
    const float expectTarget = std::min(100.0f, targetMana);
    const float expectPoor = std::min(100.0f, poorMana);
    TEST_ASSERT(target->GetMana() == targetMana - expectTarget);
    TEST_ASSERT(targetLife - target->GetLife() == expectTarget);
    TEST_ASSERT(poor->GetMana() == poorMana - expectPoor);
    TEST_ASSERT(poorLife - poor->GetLife() == expectPoor);
    return true;
}

bool TestEntanglingRootsChainParity() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 431, 0, 0);
    auto* target = AdoptAt(sim, 432, 100, 0);
    Settle(sim);
    CAbilityEntanglingRoots roots(4301);
    roots.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bena")) != nullptr);
    TEST_ASSERT(target->GetBuff(War3ID("Bpoi")) != nullptr);
    return true;
}

bool TestHealingWaveBounceParity() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 391, 0, 0);
    auto* t0 = AdoptAt(sim, 392, 2000, 0);
    auto* t1 = AdoptAt(sim, 393, 2200, 0);
    auto* t2 = AdoptAt(sim, 394, 2300, 0);
    Settle(sim);
    for (auto* u : {t0, t1, t2}) {
        u->Damage(sim, nullptr, false, false, CAttackType::CHAOS,
                  CDamageType::NORMAL, "", 900.0f, 0);
    }
    CAbilityHealingWave spell(3901);
    const float b0 = t0->GetLife();
    const float b1 = t1->GetLife();
    const float b2 = t2->GetLife();
    spell.DoEffect(sim, caster, t0);
    // Default 100 with 0.75x falloff: strictly decreasing gains.
    const float g0 = t0->GetLife() - b0;
    const float g1 = t1->GetLife() - b1;
    const float g2 = t2->GetLife() - b2;
    TEST_ASSERT(g0 == 100.0f);
    TEST_ASSERT(g0 > g1 && g1 > g2 && g2 > 0.0f);
    return true;
}

bool TestChannelFinishRunsAtEnd() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 501, 0, 0);
    auto* target = AdoptAt(sim, 502, 300, 0);
    Settle(sim);
    const float before = target->GetLife();
    ABEffectChain finish;
    finish.Add(std::make_unique<ABActionDamageTarget>(100.0f));
    ABActionBeginChannel channel(3, ABEffectChain{}, std::move(finish));
    channel.Run(sim, caster, target, false, 0, 0);
    TEST_ASSERT(dynamic_cast<CBehaviorChannel*>(caster->GetCurrentBehavior()) != nullptr);
    sim.Update();
    sim.Update();
    TEST_ASSERT(target->GetLife() == before);
    sim.Update();
    TEST_ASSERT(target->GetLife() < before);
    return true;
}

bool TestChannelTickRunsEachTick() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 511, 0, 0);
    auto* target = AdoptAt(sim, 512, 300, 0);
    Settle(sim);
    const float before = target->GetLife();
    ABEffectChain tick;
    tick.Add(std::make_unique<ABActionDamageTarget>(10.0f));
    ABEffectChain finish;
    finish.Add(std::make_unique<ABActionDamageTarget>(100.0f));
    ABActionBeginChannel channel(3, std::move(tick), std::move(finish));
    channel.Run(sim, caster, target, false, 0, 0);
    sim.Update();
    const float afterOne = target->GetLife();
    TEST_ASSERT(afterOne < before);
    sim.Update();
    sim.Update();
    // Two ticks (10 each) plus the 100 finish, modulo armor rounding.
    TEST_ASSERT(target->GetLife() < afterOne);
    TEST_ASSERT(before - target->GetLife() >= 100.0f);
    return true;
}

bool TestChannelInterruptCancels() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 521, 0, 0);
    auto* target = AdoptAt(sim, 522, 300, 0);
    Settle(sim);
    const float before = target->GetLife();
    ABEffectChain finish;
    finish.Add(std::make_unique<ABActionDamageTarget>(100.0f));
    ABActionBeginChannel channel(5, ABEffectChain{}, std::move(finish));
    channel.Run(sim, caster, target, false, 0, 0);
    sim.Update();
    // New orders replace the behavior: the finish never fires.
    std::unique_ptr<CBehavior> empty;
    caster->SetCurrentBehavior(std::move(empty));
    for (int i = 0; i < 10; i++) sim.Update();
    TEST_ASSERT(target->GetLife() == before);
    return true;
}

bool TestChannelFizzleOnDeath() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 531, 0, 0);
    auto* target = AdoptAt(sim, 532, 300, 0);
    Settle(sim);
    const float before = target->GetLife();
    ABEffectChain finish;
    finish.Add(std::make_unique<ABActionDamageTarget>(100.0f));
    ABActionBeginChannel channel(2, ABEffectChain{}, std::move(finish));
    channel.Run(sim, caster, target, false, 0, 0);
    caster->SetLife(sim, 0.0f);
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(target->GetLife() == before);
    return true;
}

bool TestSpellShieldBlocksDamage() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 541, 0, 0);
    auto* target = AdoptAt(sim, 542, 300, 0);
    Settle(sim);
    target->AddBuff(std::make_unique<CBuffSpellShield>(60.0f), sim);
    const float before = target->GetLife();
    ABEffectChain nuke;
    nuke.Add(std::make_unique<ABActionDamageTarget>(100.0f));
    nuke.Run(sim, caster, target, false, 0, 0);
    // Blocked and consumed; the follow-up lands.
    TEST_ASSERT(target->GetLife() == before);
    TEST_ASSERT(target->GetBuff(War3ID("BNss")) == nullptr);
    nuke.Run(sim, caster, target, false, 0, 0);
    TEST_ASSERT(target->GetLife() < before);
    return true;
}

bool TestSpellShieldAllyBypass() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    sim.GetPlayer(0)->SetAlliance(1, CAllianceType::PASSIVE, true);
    sim.GetPlayer(1)->SetAlliance(0, CAllianceType::PASSIVE, true);
    auto* caster = AdoptAtFor(sim, 551, 0, 0, 0);
    auto* ally = AdoptAtFor(sim, 552, 1, 300, 0);
    Settle(sim);
    ally->AddBuff(std::make_unique<CBuffSpellShield>(60.0f), sim);
    const float before = ally->GetLife();
    ABEffectChain nuke;
    nuke.Add(std::make_unique<ABActionDamageTarget>(100.0f));
    nuke.Run(sim, caster, ally, false, 0, 0);
    TEST_ASSERT(ally->GetLife() < before);
    TEST_ASSERT(ally->GetBuff(War3ID("BNss")) != nullptr);
    return true;
}

bool TestMissileDeflected() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 561, 0, 0);
    auto* target = AdoptAt(sim, 562, 300, 0);
    Settle(sim);
    target->AddBuff(std::make_unique<CBuffSpellShield>(60.0f), sim);
    const float before = target->GetLife();
    CAbilityStormBolt bolt(5601);
    bolt.DoEffect(sim, caster, target);
    for (int i = 0; i < 30; i++) sim.Update();
    TEST_ASSERT(target->GetLife() == before);
    TEST_ASSERT(target->GetBuff(War3ID("Bstn")) == nullptr);
    TEST_ASSERT(target->GetBuff(War3ID("BNss")) == nullptr);
    return true;
}

bool TestHealIgnoresShield() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 571, 0, 0);
    auto* ally = AdoptAt(sim, 572, 300, 0);
    Settle(sim);
    ally->AddBuff(std::make_unique<CBuffSpellShield>(60.0f), sim);
    ally->Damage(sim, nullptr, false, false, CAttackType::CHAOS,
                 CDamageType::NORMAL, "", 200.0f, 0);
    const float wounded = ally->GetLife();
    ABEffectChain mend;
    mend.Add(std::make_unique<ABActionHealTarget>(100.0f));
    mend.Run(sim, caster, ally, false, 0, 0);
    TEST_ASSERT(ally->GetLife() > wounded);
    TEST_ASSERT(ally->GetBuff(War3ID("BNss")) != nullptr);
    return true;
}
