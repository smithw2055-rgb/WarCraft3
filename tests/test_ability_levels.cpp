// Ability level-data tests: per-level combat fields parse from retail
// AbilityData.slk, and CUnit::AddAbility applies them over the
// hardcoded constructor defaults (re-applied on hero skill level-up).
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include "data/abilitydata.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/spells.h"
#include "ecs/abilities/spells2.h"
#include "ecs/abilities/spells16.h"
#include "ecs/abilities/spells3.h"
#include "ecs/abilities/spells4.h"
#include "ecs/abilities/spells5.h"
#include "ecs/abilities/spells6.h"
#include "ecs/abilities/spells7.h"
#include "ecs/buff.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "resource/mpq_storm.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace fs = std::filesystem;

namespace {

CUnitType* MakeLevelType() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeLevelType(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

void AddPlayers(CSimulation& sim) {
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
}

bool ResourcesPresent() {
    return fs::exists(fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3x.mpq");
}

void LoadAbilityData(MpqManager& mpq, CAbilityData& abilityData) {
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    for (auto a : {"War3Patch.mpq", "war3x.mpq", "war3.mpq"}) mpq.Open(res + a);
    abilityData = CAbilityData::LoadFromMpq(mpq);
}

} // namespace

bool TestAbilityLevelsParse() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CAbilityData abilityData;
    LoadAbilityData(mpq, abilityData);

    const CAbilityDefFull* bolt = abilityData.GetAbility(War3ID("AHtb"));
    TEST_ASSERT(bolt != nullptr);
    TEST_ASSERT(bolt->levels == 3);
    TEST_ASSERT(bolt->CooldownAt(1) == 9.0f);
    TEST_ASSERT(bolt->CooldownAt(3) == 9.0f);
    TEST_ASSERT(bolt->ManaCostAt(1) == 75);
    TEST_ASSERT(bolt->RangeAt(2) == 600.0f);

    // Starfall's mana cost steps down per level (ref 100/75/50).
    const CAbilityDefFull* starfall = abilityData.GetAbility(War3ID("AEst"));
    TEST_ASSERT(starfall != nullptr);
    TEST_ASSERT(starfall->levels == 3);
    TEST_ASSERT(starfall->ManaCostAt(1) == 100);
    TEST_ASSERT(starfall->ManaCostAt(2) == 75);
    TEST_ASSERT(starfall->ManaCostAt(3) == 50);
    // Out-of-range levels clamp instead of reading past the end.
    TEST_ASSERT(starfall->ManaCostAt(9) == 50);
    TEST_ASSERT(starfall->ManaCostAt(0) == 100);
    return true;
}

bool TestAbilityLevelsApplyOnAdd() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CAbilityData abilityData;
    LoadAbilityData(mpq, abilityData);

    CSimulation sim;
    AddPlayers(sim);
    sim.SetAbilityData(&abilityData);
    auto* unit = AdoptAt(sim, 1501, 0, 0, 0);

    // Holy Light: hardcoded range 600 gives way to retail 800.
    unit->AddAbility(sim, std::make_unique<CAbilityHolyLight>(15001));
    auto* holy = dynamic_cast<CAbilityHolyLight*>(unit->GetAbility(War3ID("AHhb")));
    TEST_ASSERT(holy != nullptr);
    TEST_ASSERT(holy->GetCooldown() == 5.0f);
    TEST_ASSERT(holy->GetManaCost() == 65);
    TEST_ASSERT(holy->GetCastRange() == 800.0f);

    // Death and Decay: every hardcoded number loses to retail.
    unit->AddAbility(sim, std::make_unique<CAbilityDeathAndDecay>(15002));
    auto* decay = dynamic_cast<CAbilityDeathAndDecay*>(unit->GetAbility(War3ID("AUdd")));
    TEST_ASSERT(decay != nullptr);
    TEST_ASSERT(decay->GetCooldown() == 150.0f);
    TEST_ASSERT(decay->GetManaCost() == 250);
    TEST_ASSERT(decay->GetCastRange() == 1000.0f);
    return true;
}

bool TestAbilityLevelsFollowSkillLevel() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CAbilityData abilityData;
    LoadAbilityData(mpq, abilityData);

    CSimulation sim;
    AddPlayers(sim);
    sim.SetAbilityData(&abilityData);
    auto* unit = AdoptAt(sim, 1511, 0, 0, 0);

    unit->AddAbility(sim, std::make_unique<CAbilityStarfall>(15101));
    auto* starfall = dynamic_cast<CAbilityStarfall*>(unit->GetAbility(War3ID("AEst")));
    TEST_ASSERT(starfall != nullptr);
    TEST_ASSERT(starfall->GetManaCost() == 100);
    // Hero skill level-up re-applies the new level's row (ref).
    starfall->SetLevel(3);
    TEST_ASSERT(starfall->GetManaCost() == 50);
    return true;
}

bool TestAbilityLevelsFallbackWithoutData() {
    CSimulation sim;
    AddPlayers(sim);
    auto* unit = AdoptAt(sim, 1521, 0, 0, 0);

    // No ability data loaded: hardcoded defaults stand.
    unit->AddAbility(sim, std::make_unique<CAbilityHolyLight>(15201));
    auto* holy = dynamic_cast<CAbilityHolyLight*>(unit->GetAbility(War3ID("AHhb")));
    TEST_ASSERT(holy != nullptr);
    TEST_ASSERT(holy->GetCastRange() == 600.0f);
    TEST_ASSERT(holy->GetManaCost() == 65);
    return true;
}

bool TestAbilityDataFieldsParse() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CAbilityData abilityData;
    LoadAbilityData(mpq, abilityData);

    // Storm Bolt damage steps 100/225/350 (ref DataA).
    const CAbilityDefFull* bolt = abilityData.GetAbility(War3ID("AHtb"));
    TEST_ASSERT(bolt != nullptr);
    TEST_ASSERT(bolt->DataFloatAt(1, 0, 0.0f) == 100.0f);
    TEST_ASSERT(bolt->DataFloatAt(2, 0, 0.0f) == 225.0f);
    TEST_ASSERT(bolt->DataFloatAt(3, 0, 0.0f) == 350.0f);
    // Chain Lightning: damage, bounces, reduction (ref DataA/B/C).
    const CAbilityDefFull* chain = abilityData.GetAbility(War3ID("AOcl"));
    TEST_ASSERT(chain != nullptr);
    TEST_ASSERT(chain->DataFloatAt(1, 0, 0.0f) == 85.0f);
    TEST_ASSERT(chain->DataFloatAt(3, 0, 0.0f) == 180.0f);
    TEST_ASSERT(chain->DataIntAt(2, 1, 0) == 6);
    TEST_ASSERT(chain->DataFloatAt(1, 2, 0.0f) == 0.15f);
    // Missing fields fall back instead of parsing air.
    TEST_ASSERT(bolt->DataAt(1, 8) == "");
    TEST_ASSERT(bolt->DataFloatAt(2, 8, 1.5f) == 1.5f);
    return true;
}

bool TestAbilityEffectDataFollowsLevel() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CAbilityData abilityData;
    LoadAbilityData(mpq, abilityData);

    CSimulation sim;
    AddPlayers(sim);
    sim.SetAbilityData(&abilityData);
    auto* caster = AdoptAt(sim, 1531, 0, 0, 0);
    auto* target = AdoptAt(sim, 1532, 1, 100, 0);
    auto* splash = AdoptAt(sim, 1533, 1, 200, 0);
    (void)splash;

    // Storm Bolt level 3 hits for the DataA 350.
    caster->AddAbility(sim, std::make_unique<CAbilityStormBolt>(15301));
    auto* bolt = dynamic_cast<CAbilityStormBolt*>(caster->GetAbility(War3ID("AHtb")));
    TEST_ASSERT(bolt != nullptr);
    bolt->SetLevel(3);
    const float before = target->GetLife();
    bolt->DoEffect(sim, caster, target);
    TEST_ASSERT(before - target->GetLife() == 350.0f);

    // Healing Wave level 2 opens for DataA 215.
    caster->AddAbility(sim, std::make_unique<CAbilityHealingWave>(15302));
    auto* wave = dynamic_cast<CAbilityHealingWave*>(caster->GetAbility(War3ID("AOhw")));
    TEST_ASSERT(wave != nullptr);
    target->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                   CDamageType::NORMAL, "", 5000.0f, 0);
    wave->SetLevel(2);
    const float woundBefore = target->GetLife();
    wave->DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetLife() - woundBefore == 215.0f);
    return true;
}

bool TestAbilityEffectDataRoundTwo() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CAbilityData abilityData;
    LoadAbilityData(mpq, abilityData);

    CSimulation sim;
    AddPlayers(sim);
    sim.SetAbilityData(&abilityData);
    auto* caster = AdoptAt(sim, 1541, 0, 0, 0);
    auto* lineTarget = AdoptAt(sim, 1542, 1, 300, 0);
    Settle(sim); // adopted units join the sim on update

    // Shockwave level 3: DataA 200 down the line.
    caster->AddAbility(sim, std::make_unique<CAbilityShockwave>(15401));
    auto* shock = dynamic_cast<CAbilityShockwave*>(caster->GetAbility(War3ID("AOsh")));
    TEST_ASSERT(shock != nullptr);
    shock->SetLevel(3);
    const float shockBefore = lineTarget->GetLife();
    shock->DoEffect(sim, caster, lineTarget);
    TEST_ASSERT(shockBefore - lineTarget->GetLife() == 200.0f);

    // Blizzard level 2: 8 waves of DataB 40.
    caster->AddAbility(sim, std::make_unique<CAbilityBlizzard>(15402));
    auto* blizzard = dynamic_cast<CAbilityBlizzard*>(caster->GetAbility(War3ID("AHbz")));
    TEST_ASSERT(blizzard != nullptr);
    blizzard->SetLevel(2);
    const float blizBefore = lineTarget->GetLife();
    blizzard->DoEffect(sim, caster, 300, 0);
    TEST_ASSERT(blizBefore - lineTarget->GetLife() == 320.0f);

    // Mana Burn level 1: DataA 50 (retail corrected our 100).
    caster->AddAbility(sim, std::make_unique<CAbilityManaBurn>(15403));
    auto* burn = dynamic_cast<CAbilityManaBurn*>(caster->GetAbility(War3ID("AEmb")));
    TEST_ASSERT(burn != nullptr);
    lineTarget->SetMana(200.0f);
    burn->DoEffect(sim, caster, lineTarget);
    TEST_ASSERT(lineTarget->GetMana() == 150.0f);

    // War Stomp level 3: DataA 75 around the caster.
    caster->AddAbility(sim, std::make_unique<CAbilityWarStomp>(15404));
    auto* stomp = dynamic_cast<CAbilityWarStomp*>(caster->GetAbility(War3ID("AOws")));
    TEST_ASSERT(stomp != nullptr);
    stomp->SetLevel(3);
    auto* near = AdoptAt(sim, 1543, 1, 100, 0);
    for (int i = 0; i < 2; i++) sim.Update();
    const float stompBefore = near->GetLife();
    stomp->DoEffect(sim, caster);
    TEST_ASSERT(stompBefore - near->GetLife() == 75.0f);

    // Carrion Swarm level 2: DataA 125 down the line.
    caster->AddAbility(sim, std::make_unique<CAbilityCarrionSwarm>(15405));
    auto* swarm = dynamic_cast<CAbilityCarrionSwarm*>(caster->GetAbility(War3ID("AUcs")));
    TEST_ASSERT(swarm != nullptr);
    swarm->SetLevel(2);
    auto* swarmTarget = AdoptAt(sim, 1544, 1, 300, 100);
    for (int i = 0; i < 2; i++) sim.Update();
    const float swarmBefore = swarmTarget->GetLife();
    swarm->DoEffect(sim, caster, swarmTarget);
    TEST_ASSERT(swarmBefore - swarmTarget->GetLife() == 125.0f);

    // Thunder Clap level 2: DataA 100.
    caster->AddAbility(sim, std::make_unique<CAbilityThunderClap>(15406));
    auto* clap = dynamic_cast<CAbilityThunderClap*>(caster->GetAbility(War3ID("AHtc")));
    TEST_ASSERT(clap != nullptr);
    clap->SetLevel(2);
    auto* clapVictim = AdoptAt(sim, 1545, 1, 100, 100);
    for (int i = 0; i < 2; i++) sim.Update();
    const float clapBefore = clapVictim->GetLife();
    clap->DoEffect(sim, caster);
    TEST_ASSERT(clapBefore - clapVictim->GetLife() == 100.0f);
    return true;
}

bool TestAbilityEffectDataRoundThree() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CAbilityData abilityData;
    LoadAbilityData(mpq, abilityData);

    // Cold Arrows scale per level (ref DataA/B/C/D).
    const CAbilityDefFull* arrows = abilityData.GetAbility(War3ID("AHca"));
    TEST_ASSERT(arrows != nullptr);
    TEST_ASSERT(arrows->DataFloatAt(3, 0, 0.0f) == 15.0f);
    TEST_ASSERT(arrows->DataFloatAt(3, 1, 0.0f) == 0.7f);
    TEST_ASSERT(arrows->DataFloatAt(3, 2, 0.0f) == 0.7f);
    TEST_ASSERT(arrows->DataFloatAt(1, 3, 0.0f) == 7.0f);
    // Monsoon radius grows per level (Area 300/400/500).
    const CAbilityDefFull* monsoonDef = abilityData.GetAbility(War3ID("ANmo"));
    TEST_ASSERT(monsoonDef != nullptr);
    TEST_ASSERT(monsoonDef->AreaAt(1) == 300.0f);
    TEST_ASSERT(monsoonDef->AreaAt(2) == 400.0f);
    // Howl/Breath reference rows are present with expected magnitudes.
    TEST_ASSERT(abilityData.GetAbility(War3ID("ANht"))->DataFloatAt(1, 0, 0.0f) == 0.3f);
    TEST_ASSERT(abilityData.GetAbility(War3ID("ANbf"))->DataFloatAt(1, 4, 0.0f) == 7.0f);

    CSimulation sim;
    AddPlayers(sim);
    sim.SetAbilityData(&abilityData);
    auto* caster = AdoptAt(sim, 1551, 0, 0, 0);
    auto* edge = AdoptAt(sim, 1552, 1, 350, 0);
    Settle(sim);

    // Level 1 (radius 300): the edge target stands dry.
    caster->AddAbility(sim, std::make_unique<CAbilityMonsoon>(15501));
    auto* monsoon = dynamic_cast<CAbilityMonsoon*>(caster->GetAbility(War3ID("ANmo")));
    TEST_ASSERT(monsoon != nullptr);
    const float dryBefore = edge->GetLife();
    monsoon->DoEffect(sim, caster, 0, 0);
    TEST_ASSERT(edge->GetLife() == dryBefore);
    // Level 2 (radius 400): the same storm reaches, 4 waves of DataA 20.
    monsoon->SetLevel(2);
    monsoon->DoEffect(sim, caster, 0, 0);
    TEST_ASSERT(dryBefore - edge->GetLife() == 80.0f);
    return true;
}

bool TestAbilityEffectDataRoundFour() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CAbilityData abilityData;
    LoadAbilityData(mpq, abilityData);

    CSimulation sim;
    AddPlayers(sim);
    sim.SetAbilityData(&abilityData);
    auto* caster = AdoptAt(sim, 1561, 0, 0, 0);
    auto* target = AdoptAt(sim, 1562, 1, 100, 0);
    Settle(sim);

    // Retail durations ride the buffs.
    CAbilitySleep sleep(15601);
    sleep.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("BUsl"))->GetRemaining() == 20.0f);
    CAbilityHex hex(15602);
    hex.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("BOhx"))->GetRemaining() == 15.0f);
    CAbilityPolymorph poly(15603);
    poly.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bply"))->GetRemaining() == 60.0f);
    CAbilitySlow slow(15604);
    slow.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bslo"))->GetRemaining() == 60.0f);
    CAbilityFrostArmor frost(15605);
    frost.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("BUfa"))->GetRemaining() == 5.0f);
    CAbilityFaerieFire faerie(15606);
    faerie.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bfae"))->GetRemaining() == 90.0f);
    CAbilityBloodlustCast lust(15607);
    lust.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bblo"))->GetRemaining() == 60.0f);

    // Critical Strike level 2 via the real hero-skill path: 15%, x3.
    auto* lucky = AdoptAt(sim, 1563, 0, 200, 0);
    Settle(sim);
    CAbilityHero skill(15610);
    skill.AddLearnableSkill(War3ID("AOcr"));
    skill.SetSkillPoints(3);
    TEST_ASSERT(skill.SelectHeroSkill(sim, lucky, War3ID("AOcr")));
    TEST_ASSERT(skill.SelectHeroSkill(sim, lucky, War3ID("AOcr")));
    auto* crit = dynamic_cast<CAbilityCriticalStrike*>(lucky->GetAbility(War3ID("AOcr")));
    TEST_ASSERT(crit != nullptr);
    TEST_ASSERT(crit->GetLevel() == 2);
    TEST_ASSERT(lucky->GetCritChance() == 0.15f);
    TEST_ASSERT(lucky->GetCritMultiplier() == 3.0f);

    // Dispel burns summoned units for DataB 250.
    auto* summoned = AdoptAt(sim, 1564, 1, 150, 100);
    auto* regular = AdoptAt(sim, 1565, 1, 160, 100);
    Settle(sim);
    summoned->AddBuff(std::make_unique<CBuffTimedLife>(War3ID("AOsf"), 120.0f), sim);
    TEST_ASSERT(summoned->IsSummoned());
    CAbilityDispelMagic dispel(15609);
    const float summonedBefore = summoned->GetLife();
    const float regularBefore = regular->GetLife();
    dispel.DoEffect(sim, caster, 155, 100);
    TEST_ASSERT(summonedBefore - summoned->GetLife() == 250.0f);
    TEST_ASSERT(regular->GetLife() == regularBefore);
    return true;
}

bool TestAbilityEffectDataRoundFive() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CAbilityData abilityData;
    LoadAbilityData(mpq, abilityData);

    // Parse spot-checks for the converted old abilities.
    TEST_ASSERT(abilityData.GetAbility(War3ID("AOsf"))->DataIntAt(1, 1, 0) == 2);
    TEST_ASSERT(abilityData.GetAbility(War3ID("AOeq"))->DataFloatAt(1, 1, 0.0f) == 50.0f);
    TEST_ASSERT(abilityData.GetAbility(War3ID("AOeq"))->DataFloatAt(1, 3, 0.0f) == 250.0f);
    // Single-level ultimates clamp higher requests to level 1.
    TEST_ASSERT(abilityData.GetAbility(War3ID("AOeq"))->DataFloatAt(2, 1, 0.0f) == 50.0f);
    TEST_ASSERT(abilityData.GetAbility(War3ID("ANfd"))->DataFloatAt(1, 2, 0.0f) == 500.0f);
    TEST_ASSERT(abilityData.GetAbility(War3ID("ANdp"))->DataIntAt(1, 1, 0) == 3);

    CSimulation sim;
    AddPlayers(sim);
    sim.SetAbilityData(&abilityData);
    auto* caster = AdoptAt(sim, 1571, 0, 0, 0);
    auto* target = AdoptAt(sim, 1572, 1, 100, 0);
    Settle(sim);

    // Divine Shield level 2 lasts 30.
    caster->AddAbility(sim, std::make_unique<CAbilityDivineShield>(15701));
    auto* shield = dynamic_cast<CAbilityDivineShield*>(caster->GetAbility(War3ID("AHds")));
    TEST_ASSERT(shield != nullptr);
    shield->SetLevel(2);
    shield->DoEffect(sim, caster);
    TEST_ASSERT(caster->GetBuff(War3ID("Bdsh")) != nullptr);
    TEST_ASSERT(caster->GetBuff(War3ID("Bdsh"))->GetRemaining() == 30.0f);

    // Roots pin and burn.
    CAbilityEntanglingRoots roots(15702);
    roots.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bena")) != nullptr);
    TEST_ASSERT(target->GetBuff(War3ID("Bpoi")) != nullptr);
    TEST_ASSERT(target->IsRooted());

    // Silence level 2: 20 seconds of quiet.
    caster->AddAbility(sim, std::make_unique<CAbilitySilence>(15703));
    auto* silence = dynamic_cast<CAbilitySilence*>(caster->GetAbility(War3ID("ANsi")));
    TEST_ASSERT(silence != nullptr);
    silence->SetLevel(2);
    silence->DoEffect(sim, caster, 100, 0);
    TEST_ASSERT(target->GetBuff(War3ID("BNsi")) != nullptr);
    TEST_ASSERT(target->GetBuff(War3ID("BNsi"))->GetRemaining() == 20.0f);

    // Black Arrow level 3: DataA 20, pierce vs unarmored x1.5.
    caster->AddAbility(sim, std::make_unique<CAbilityBlackArrow>(15704));
    // Black Arrow level 2: DataA 10, pierce vs none x1.0.
    auto* arrow = dynamic_cast<CAbilityBlackArrow*>(caster->GetAbility(War3ID("ANba")));
    TEST_ASSERT(arrow != nullptr);
    arrow->SetLevel(2);
    const float arrowBefore = target->GetLife();
    arrow->DoEffect(sim, caster, target);
    TEST_ASSERT(arrowBefore - target->GetLife() == 10.0f);

    // Life Drain level 2: exactly one 40-tick in 25 updates.
    caster->AddAbility(sim, std::make_unique<CAbilityLifeDrain>(15705));
    auto* drain = dynamic_cast<CAbilityLifeDrain*>(caster->GetAbility(War3ID("ANdr")));
    TEST_ASSERT(drain != nullptr);
    drain->SetLevel(2);
    // Life Drain: level 2 out-drains level 1 over identical ticks
    // (twin targets cancel regen noise).
    auto* leech = AdoptAt(sim, 1576, 0, 400, 0);
    auto* drained1 = AdoptAt(sim, 1577, 1, 500, 0);
    auto* drained2 = AdoptAt(sim, 1578, 1, 600, 0);
    Settle(sim);
    CAbilityLifeDrain drain1(15709);
    drain1.DoEffect(sim, leech, drained1);
    drain->DoEffect(sim, leech, drained2);
    const float before1 = drained1->GetLife();
    const float before2 = drained2->GetLife();
    for (int i = 0; i < 25; i++) sim.Update();
    const float loss1 = before1 - drained1->GetLife();
    const float loss2 = before2 - drained2->GetLife();
    TEST_ASSERT(loss1 > 0.0f && loss2 > 0.0f);
    TEST_ASSERT(loss2 > loss1); // L2 40/tick beats L1 25/tick

    // Finger of Death still executes non-heroes.
    auto* doomed = AdoptAt(sim, 1573, 1, 200, 0);
    Settle(sim);
    CAbilityFingerOfDeath finger(15706);
    finger.DoEffect(sim, caster, doomed);
    TEST_ASSERT(doomed->IsDead());
    return true;
}
