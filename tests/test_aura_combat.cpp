// Thorns/Vampiric aura buff wiring tests (ref lifesteal/thorns listeners).
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "game/war3/abilities/war3_spells.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) { std::cerr << "NEAR @" << __LINE__ << ": " << (a) << " vs " << (b) << "\n"; return false; } } while (0)

namespace {

CUnitType* MakeLeechTestType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        500, 1.0f, 0.5f, CRegenType::ALWAYS,
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

} // namespace

bool TestThornsAuraGrantsReflectBuff() {
    CSimulation sim;
    CUnitType* type = MakeLeechTestType();
    CUnit source(4001, type, 0, War3ID("hfoo"), 0, 0, 0);
    CUnit target(4002, type, 0, War3ID("hfoo"), 100, 0, 0);
    CUnit attacker(4003, type, 1, War3ID("hfoo"), 200, 0, 0);
    sim.GetUnits().push_back(&source);
    sim.GetUnits().push_back(&target);
    sim.GetUnits().push_back(&attacker);

    CAbilityThornsAura aura(500);
    aura.SetRadius(1000.0f);
    aura.SetEnemyOnly(true); // test sim has no players; affect everyone in range
    aura.SetAffectsAllies(false);
    aura.SetAffectsSelf(false);
    aura.OnTick(sim, &source);
    TEST_ASSERT(target.HasBuff(War3ID("BEah")));

    // Melee attack on the thorny target reflects 10% (level-1 DataA1).
    const float atkBefore = attacker.GetLife();
    target.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    const float reflected = atkBefore - attacker.GetLife();
    TEST_ASSERT(reflected > 0.0f);

    // Ranged attacks are not reflected (ref listener is melee-only).
    CUnit archer(4004, type, 1, War3ID("hfoo"), 200, 0, 0);
    const float archerBefore = archer.GetLife();
    target.Damage(sim, &archer, true, true, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(archer.GetLife(), archerBefore, 0.001f);

    // Leaving the aura removes the buff.
    aura.OnRemove(sim, &source);
    TEST_ASSERT(!target.HasBuff(War3ID("BEah")));
    sim.GetUnits().clear();
    return true;
}

bool TestVampiricAuraHealsAttacker() {
    CSimulation sim;
    CUnitType* type = MakeLeechTestType();
    CUnit attacker(4101, type, 0, War3ID("hfoo"), 0, 0, 0);
    CUnit victim(4102, type, 1, War3ID("hfoo"), 100, 0, 0);

    attacker.AddBuff(std::make_unique<CBuffLifesteal>(10.0f, 0.15f), sim);
    attacker.SetLife(sim, 100.0f); // wound the attacker
    victim.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    // 15% of ~100 dealt, clamped to max 500.
    TEST_ASSERT(attacker.GetLife() > 100.0f);
    TEST_ASSERT(attacker.GetLife() <= attacker.GetMaxLife());
    return true;
}

bool TestVampiricAuraViaAuraAbility() {
    CSimulation sim;
    CUnitType* type = MakeLeechTestType();
    CUnit source(4201, type, 0, War3ID("hfoo"), 0, 0, 0);
    CUnit ally(4202, type, 0, War3ID("hfoo"), 100, 0, 0);
    sim.GetUnits().push_back(&source);
    sim.GetUnits().push_back(&ally);

    CAbilityVampiricAura aura(501);
    aura.SetRadius(1000.0f);
    aura.SetEnemyOnly(true);
    aura.SetAffectsAllies(false);
    aura.SetAffectsSelf(false);
    aura.OnTick(sim, &source);
    TEST_ASSERT(ally.HasBuff(War3ID("BUav")));
    aura.OnRemove(sim, &source);
    TEST_ASSERT(!ally.HasBuff(War3ID("BUav")));
    sim.GetUnits().clear();
    return true;
}

namespace {

CUnitType* MakeArmoredType() {
    // Same shape as MakeLeechTestType but with 10 armor, so base (100)
    // and post-armor actual (62.5) differ and pin the thorns basis.
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        500, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 10, War3ID(),
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

void AddTwoPlayers(CSimulation& sim) {
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
}

} // namespace

bool TestThornsScalesOnIncomingBase() {
    // Ref thorns listener scales the raw incoming base (pre-armor), not
    // the sustained actual: 10% of 100 base into 10 armor (0.375
    // reduction, 62.5 sustained) still reflects 10.
    CSimulation sim;
    CUnitType* type = MakeLeechTestType();
    CUnitType* armored = MakeArmoredType();
    CUnit attacker(4301, type, 0, War3ID("hfoo"), 0, 0, 0);
    CUnit victim(4302, armored, 1, War3ID("hfoo"), 100, 0, 0);
    victim.AddBuff(std::make_unique<CBuffThorns>(10.0f, 0.10f, War3ID("Bthr")), sim);
    const float atkBefore = attacker.GetLife();
    victim.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    // Sustained is only 62.5, but the base-100 reflection holds.
    TEST_NEAR(atkBefore - attacker.GetLife(), 10.0f, 0.05f);
    return true;
}

bool TestThornsRequiresNormalType() {
    // Magic-typed melee hits do not trigger thorns (ref damageType gate).
    CSimulation sim;
    CUnitType* type = MakeLeechTestType();
    CUnit attacker(4311, type, 0, War3ID("hfoo"), 0, 0, 0);
    CUnit victim(4312, type, 1, War3ID("hfoo"), 100, 0, 0);
    victim.AddBuff(std::make_unique<CBuffThorns>(10.0f, 0.10f, War3ID("Bthr")), sim);
    const float atkBefore = attacker.GetLife();
    victim.Damage(sim, &attacker, true, false, CAttackType::MAGIC, CDamageType::MAGIC, "", 100.0f, 0);
    TEST_NEAR(attacker.GetLife(), atkBefore, 0.001f);
    return true;
}

bool TestThornsBlockedByAlliance() {
    // Allies never eat thorns (ref ENEMY_TARGET gate).
    CSimulation sim;
    AddTwoPlayers(sim);
    sim.GetPlayer(1)->SetAlliance(0, CAllianceType::ALLIED, true);
    CUnitType* type = MakeLeechTestType();
    CUnit attacker(4321, type, 0, War3ID("hfoo"), 0, 0, 0);
    CUnit victim(4322, type, 1, War3ID("hfoo"), 100, 0, 0);
    victim.AddBuff(std::make_unique<CBuffThorns>(10.0f, 0.10f, War3ID("Bthr")), sim);
    const float atkBefore = attacker.GetLife();
    victim.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(attacker.GetLife(), atkBefore, 0.001f);
    return true;
}

bool TestThornsResistsGate() {
    // Magic-immune attackers shrug thorns only while ResistsThorns is set.
    // Both blocks run with generic ultimate-immunity off (retail
    // MiscGame.txt sets it 0); otherwise the DEFENSIVE-typed reflection
    // fizzles in the generic magic-immune gate before ResistsThorns is
    // even consulted.
    CUnitType* type = MakeLeechTestType();
    {
        CSimulation sim;
        CGameplayConstants gc;
        gc.magicImmuneResistsUltimates = false;
        sim.SetGameplayConstants(gc);
        CUnit attacker(4331, type, 0, War3ID("hfoo"), 0, 0, 0);
        CUnit victim(4332, type, 1, War3ID("hfoo"), 100, 0, 0);
        attacker.SetMagicImmune(true);
        victim.AddBuff(std::make_unique<CBuffThorns>(10.0f, 0.10f, War3ID("Bthr")), sim);
        const float atkBefore = attacker.GetLife();
        victim.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
        TEST_NEAR(atkBefore - attacker.GetLife(), 10.0f, 0.05f); // gate off: reflects
    }
    {
        CSimulation sim;
        CGameplayConstants gc;
        gc.magicImmuneResistsUltimates = false;
        gc.magicImmuneResistsThorns = true;
        sim.SetGameplayConstants(gc);
        CUnit attacker(4333, type, 0, War3ID("hfoo"), 0, 0, 0);
        CUnit victim(4334, type, 1, War3ID("hfoo"), 100, 0, 0);
        attacker.SetMagicImmune(true);
        victim.AddBuff(std::make_unique<CBuffThorns>(10.0f, 0.10f, War3ID("Bthr")), sim);
        const float atkBefore = attacker.GetLife();
        victim.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
        TEST_NEAR(attacker.GetLife(), atkBefore, 0.001f);
    }
    return true;
}

bool TestThornsStrongestWins() {
    // Several thorns buffs (item + aura ids) reflect the strongest share,
    // not the first found.
    CSimulation sim;
    CUnitType* type = MakeLeechTestType();
    CUnit attacker(4341, type, 0, War3ID("hfoo"), 0, 0, 0);
    CUnit victim(4342, type, 1, War3ID("hfoo"), 100, 0, 0);
    victim.AddBuff(std::make_unique<CBuffThorns>(10.0f, 0.10f, War3ID("Bthr")), sim);
    victim.AddBuff(std::make_unique<CBuffThorns>(10.0f, 0.30f, War3ID("BEah")), sim);
    const float atkBefore = attacker.GetLife();
    victim.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(atkBefore - attacker.GetLife(), 30.0f, 0.05f);
    return true;
}

bool TestLeechBlockedByPassiveAlliance() {
    // No leech across a PASSIVE alliance (ref lifesteal listener).
    CSimulation sim;
    AddTwoPlayers(sim);
    sim.GetPlayer(0)->SetAlliance(1, CAllianceType::PASSIVE, true);
    CUnitType* type = MakeLeechTestType();
    CUnit attacker(4351, type, 0, War3ID("hfoo"), 0, 0, 0);
    CUnit victim(4352, type, 1, War3ID("hfoo"), 100, 0, 0);
    attacker.AddBuff(std::make_unique<CBuffLifesteal>(10.0f, 0.15f), sim);
    attacker.SetLife(sim, 100.0f);
    victim.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(attacker.GetLife(), 100.0f, 0.001f);
    return true;
}

bool TestLeechResistsGate() {
    // Magic-immune victims grant no leech while ResistsLeech is set.
    CUnitType* type = MakeLeechTestType();
    {
        CSimulation sim;
        CUnit attacker(4361, type, 0, War3ID("hfoo"), 0, 0, 0);
        CUnit victim(4362, type, 1, War3ID("hfoo"), 100, 0, 0);
        victim.SetMagicImmune(true);
        attacker.AddBuff(std::make_unique<CBuffLifesteal>(10.0f, 0.15f), sim);
        attacker.SetLife(sim, 100.0f);
        victim.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
        TEST_ASSERT(attacker.GetLife() > 100.0f); // default: leeches
    }
    {
        CSimulation sim;
        CGameplayConstants gc;
        gc.magicImmuneResistsLeech = true;
        sim.SetGameplayConstants(gc);
        CUnit attacker(4363, type, 0, War3ID("hfoo"), 0, 0, 0);
        CUnit victim(4364, type, 1, War3ID("hfoo"), 100, 0, 0);
        victim.SetMagicImmune(true);
        attacker.AddBuff(std::make_unique<CBuffLifesteal>(10.0f, 0.15f), sim);
        attacker.SetLife(sim, 100.0f);
        victim.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
        TEST_NEAR(attacker.GetLife(), 100.0f, 0.001f);
    }
    return true;
}
