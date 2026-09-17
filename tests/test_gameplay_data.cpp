// GameplayConstants data-loading tests: Units\MiscGame.txt DamageBonus
// rows + Units\MiscData.txt DefenseArmor drive combat, with TFT defaults
// when fields are absent.
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "data/combat_types.h"
#include "data/gameplay_constants.h"
#include "ecs/csimulation.h"
#include "ecs/combat/combat_system.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "resource/slk.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) return false; } while (0)

namespace {

TxtTable MakeMisc(const std::string& body) {
    TxtTable t;
    std::string text = "[Misc]\n" + body;
    t.Load(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    return t;
}

CUnitType* MakeDataType() {
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

} // namespace

bool TestMiscDataTableAndArmorParsed() {
    // Distinctive values per column (Small..None order in data) so the test
    // fails if parsing is skipped and defaults are used instead.
    TxtTable misc = MakeMisc(
        "DefenseArmor=0.12\n"
        "DamageBonusNormal=9.00,8.00,7.00,6.00,5.00,4.00,3.00,2.00\n");
    CGameplayConstants gc(misc);
    TEST_NEAR(gc.GetDefenseArmor(), 0.12f, 1e-6f);
    TEST_NEAR(gc.GetDamageRatioAgainst(CAttackType::NORMAL, CDefenseType::NONE), 5.0f, 1e-6f);
    TEST_NEAR(gc.GetDamageRatioAgainst(CAttackType::NORMAL, CDefenseType::LIGHT), 9.0f, 1e-6f);
    TEST_NEAR(gc.GetDamageRatioAgainst(CAttackType::NORMAL, CDefenseType::MEDIUM), 8.0f, 1e-6f);
    TEST_NEAR(gc.GetDamageRatioAgainst(CAttackType::NORMAL, CDefenseType::HEAVY), 7.0f, 1e-6f);
    TEST_NEAR(gc.GetDamageRatioAgainst(CAttackType::NORMAL, CDefenseType::FORTIFIED), 6.0f, 1e-6f);
    TEST_NEAR(gc.GetDamageRatioAgainst(CAttackType::NORMAL, CDefenseType::HERO), 4.0f, 1e-6f);
    TEST_NEAR(gc.GetDamageRatioAgainst(CAttackType::NORMAL, CDefenseType::DIVINE), 3.0f, 1e-6f);
    TEST_NEAR(gc.GetDamageRatioAgainst(CAttackType::NORMAL, CDefenseType::UNARMORED), 2.0f, 1e-6f);
    // Untouched rows keep TFT defaults.
    TEST_NEAR(gc.GetDamageRatioAgainst(CAttackType::PIERCE, CDefenseType::LIGHT), 2.0f, 1e-6f);
    return true;
}

bool TestSpellsRowFallsBackToMagic() {
    TxtTable misc = MakeMisc("DamageBonusMagic=1.25,0.75,2.00,0.35,1.00,0.50,0.05,1.00\n");
    CGameplayConstants gc(misc);
    // No DamageBonusSpells field: spells row mirrors the magic row.
    TEST_NEAR(gc.GetDamageRatioAgainst(CAttackType::SPELLS, CDefenseType::HEAVY),
              gc.GetDamageRatioAgainst(CAttackType::MAGIC, CDefenseType::HEAVY), 1e-6f);
    TEST_NEAR(gc.GetDamageRatioAgainst(CAttackType::SPELLS, CDefenseType::HEAVY), 2.0f, 1e-6f);
    return true;
}

bool TestMalformedRowKeepsDefaults() {
    TxtTable misc = MakeMisc("DamageBonusNormal=1.00,1.50,1.00\n");
    CGameplayConstants gc(misc);
    TEST_NEAR(gc.GetDamageRatioAgainst(CAttackType::NORMAL, CDefenseType::MEDIUM), 1.5f, 1e-6f);
    TEST_NEAR(gc.GetDefenseArmor(), 0.06f, 1e-6f);
    return true;
}

bool TestConfiguredTableDrivesDamage() {
    CSimulation sim;
    // Double all Normal damage via data, then verify combat follows.
    TxtTable misc = MakeMisc(
        "DamageBonusNormal=2.00,2.00,2.00,2.00,2.00,2.00,2.00,2.00\n"
        "DamageBonusPierce=2.00,0.75,1.00,0.35,1.00,0.50,0.05,1.50\n"
        "DamageBonusSiege=1.00,0.50,1.00,1.50,1.00,0.50,0.05,1.50\n"
        "DamageBonusMagic=1.25,0.75,2.00,0.35,1.00,0.50,0.05,1.00\n"
        "DamageBonusChaos=1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00\n"
        "DamageBonusHero=1.00,1.00,1.00,0.50,1.00,1.00,0.05,1.00\n"
        "DamageBonusSpells=1.00,1.00,1.00,1.00,1.00,0.70,0.05,1.00\n");
    sim.SetGameplayConstants(CGameplayConstants(misc));
    CUnit target(1, MakeDataType(), 0, War3ID("hfoo"), 0, 0, 0);
    const float lifeBefore = target.GetLife();
    target.Damage(sim, nullptr, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(lifeBefore - target.GetLife(), 200.0f, 0.01f);
    return true;
}

bool TestConfiguredDefenseArmorDrivesReduction() {
    CSimulation sim;
    TxtTable misc = MakeMisc("DefenseArmor=0.12\n");
    sim.SetGameplayConstants(CGameplayConstants(misc));
    // 10 armor with k=0.12: reduction = 1.2/2.2.
    CUnitType* type = MakeDataType();
    CUnit target(2, type, 0, War3ID("hfoo"), 0, 0, 0);
    target.AddBonusArmor(10);
    const float lifeBefore = target.GetLife();
    target.Damage(sim, nullptr, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 220.0f, 0);
    TEST_NEAR(lifeBefore - target.GetLife(), 220.0f * (1.0f - 1.2f / 2.2f), 0.05f);
    return true;
}
