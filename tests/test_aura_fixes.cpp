// Regression tests for warsmash_ref alignment fixes:
// - Vampiric/Unholy aura IDs distinct (AUau vs AUav)
// - Aura ARMOR/DAMAGE/LIFE_REGEN/MANA_REGEN/ATTACK_SPEED apply/remove
// - DivineShield stacking (remove one of two keeps invulnerable)
#include <cmath>
#include <memory>
#include <vector>

#include "ecs/abilities/ability_aura.h"
#include "game/war3/abilities/war3_spells.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) return false; } while (0)

namespace {

CUnitType* MakeAuraTestType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 20;
    atk.damageDice = 2;
    atk.damageSidesPerDie = 6; // avg dice = 7 -> base ~27
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        500, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 2, War3ID(),
        std::vector<War3ID>{}, false, PathingGridMovementType::FOOT,
        0.0f, 32.0f, std::vector<std::string>{},
        std::unordered_set<CUnitClassification>{},
        std::vector<CUnitAttack>{atk},
        1, std::string(""), false, true, CDefenseType::MEDIUM,
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

bool TestVampiricAndUnholyAuraIdsDiffer() {
    CAbilityVampiricAura vamp(1);
    CAbilityUnholyAura unholy(2);
    // Real object data: AUav = Vampiric Aura (buff BUav), AUau = Unholy Aura.
    TEST_ASSERT(vamp.GetCode() == War3ID("AUav"));
    TEST_ASSERT(unholy.GetCode() == War3ID("AUau"));
    TEST_ASSERT(!(vamp.GetCode() == unholy.GetCode()));
    return true;
}

bool TestAuraArmorDamageRegenAttackSpeedSymmetric() {
    CSimulation sim;
    CUnitType* type = MakeAuraTestType();
    CUnit source(101, type, 0, War3ID("hfoo"), 0, 0, 0);
    CUnit target(102, type, 0, War3ID("hfoo"), 100, 0, 0);
    sim.GetUnits().push_back(&source);
    sim.GetUnits().push_back(&target);

    CAbilityAura aura(200, War3ID("AHad"));
    aura.SetRadius(1000.0f);
    aura.SetEnemyOnly(true); // bypass alliance (no players in test sim)
    aura.SetAffectsAllies(false);
    aura.SetAffectsSelf(false);
    aura.AddBonus({CAuraEffectType::ARMOR, 2.0f, false});
    aura.AddBonus({CAuraEffectType::ATTACK_SPEED, 0.10f, true});
    aura.AddBonus({CAuraEffectType::LIFE_REGEN, 1.5f, false});
    aura.AddBonus({CAuraEffectType::MANA_REGEN, 0.75f, false});

    const int armorBefore = target.GetBonusArmor();
    const float atkBefore = target.GetAttackSpeedMultiplier();
    aura.OnTick(sim, &source);
    TEST_ASSERT(target.GetBonusArmor() == armorBefore + 2);
    TEST_NEAR(target.GetAttackSpeedMultiplier(), atkBefore * 1.10f, 1e-5f);
    TEST_NEAR(target.GetBonusLifeRegen(), 1.5f, 1e-5f);
    TEST_NEAR(target.GetBonusManaRegen(), 0.75f, 1e-5f);

    aura.OnRemove(sim, &source);
    TEST_ASSERT(target.GetBonusArmor() == armorBefore);
    TEST_NEAR(target.GetAttackSpeedMultiplier(), atkBefore, 1e-5f);
    TEST_NEAR(target.GetBonusLifeRegen(), 0.0f, 1e-5f);
    TEST_NEAR(target.GetBonusManaRegen(), 0.0f, 1e-5f);

    sim.GetUnits().clear();
    return true;
}

bool TestDivineShieldStackingKeepsInvulnerable() {
    CSimulation sim;
    CUnit u(103, MakeAuraTestType(), 0, War3ID("hfoo"), 0, 0, 0);
    TEST_ASSERT(!u.IsInvulnerable());
    u.AddBuff(std::make_unique<CBuffDivineShield>(10.0f), sim);
    TEST_ASSERT(u.IsInvulnerable());
    // Same-ID re-cast refreshes instead of stacking (CUnit::AddBuff), so the
    // unit must stay invulnerable and a single remove clears it.
    u.AddBuff(std::make_unique<CBuffDivineShield>(10.0f), sim);
    TEST_ASSERT(u.IsInvulnerable());
    TEST_ASSERT(u.HasBuff(War3ID("Bdsh")));
    u.RemoveBuff(War3ID("Bdsh"), sim);
    TEST_ASSERT(!u.IsInvulnerable());
    return true;
}

bool TestOverlappingAurasTakeStrongest() {
    CSimulation sim;
    CUnitType* type = MakeAuraTestType();
    CUnit source(111, type, 0, War3ID("hfoo"), 0, 0, 0);
    CUnit other(112, type, 0, War3ID("hfoo"), 50, 0, 0);
    CUnit target(113, type, 0, War3ID("hfoo"), 100, 0, 0);
    sim.GetUnits().push_back(&source);
    sim.GetUnits().push_back(&other);
    sim.GetUnits().push_back(&target);

    CAbilityAura weak(210, War3ID("AHad"));
    weak.SetRadius(1000.0f);
    weak.SetEnemyOnly(true);
    weak.SetAffectsAllies(false);
    weak.SetAffectsSelf(false);
    weak.AddBonus({CAuraEffectType::ARMOR, 2.0f, false});
    CAbilityAura strong(211, War3ID("AEmb"));
    strong.SetRadius(1000.0f);
    strong.SetEnemyOnly(true);
    strong.SetAffectsAllies(false);
    strong.SetAffectsSelf(false);
    strong.AddBonus({CAuraEffectType::ARMOR, 5.0f, false});

    const int armorBefore = target.GetBonusArmor();
    weak.OnTick(sim, &source);
    TEST_ASSERT(target.GetBonusArmor() == armorBefore + 2);
    TEST_ASSERT(target.HasBuff(War3ID("AHad")));
    // Second aura does not stack: strongest wins, buff visible too.
    strong.OnTick(sim, &other);
    TEST_ASSERT(target.GetBonusArmor() == armorBefore + 5);
    TEST_ASSERT(target.HasBuff(War3ID("AEmb")));
    // Stronger leaves: falls back to the weaker, then to nothing.
    strong.OnRemove(sim, &other);
    TEST_ASSERT(target.GetBonusArmor() == armorBefore + 2);
    weak.OnRemove(sim, &source);
    TEST_ASSERT(target.GetBonusArmor() == armorBefore);
    TEST_ASSERT(target.GetBuff(War3ID("AHad")) == nullptr);

    sim.GetUnits().clear();
    return true;
}

bool TestAuraDispelStripsBonus() {
    CSimulation sim;
    CUnitType* type = MakeAuraTestType();
    CUnit source(121, type, 0, War3ID("hfoo"), 0, 0, 0);
    CUnit target(122, type, 0, War3ID("hfoo"), 100, 0, 0);
    sim.GetUnits().push_back(&source);
    sim.GetUnits().push_back(&target);

    CAbilityAura aura(220, War3ID("AHad"));
    aura.SetRadius(1000.0f);
    aura.SetEnemyOnly(true);
    aura.SetAffectsAllies(false);
    aura.SetAffectsSelf(false);
    aura.AddBonus({CAuraEffectType::ARMOR, 2.0f, false});

    const int armorBefore = target.GetBonusArmor();
    aura.OnTick(sim, &source);
    TEST_ASSERT(target.GetBonusArmor() == armorBefore + 2);
    // Dispel strips the aura buff and its bonus follows.
    target.RemoveBuffsIf(sim, true, true, false, true);
    TEST_ASSERT(target.GetBuff(War3ID("AHad")) == nullptr);
    TEST_ASSERT(target.GetBonusArmor() == armorBefore);

    sim.GetUnits().clear();
    return true;
}

bool TestSameCodeAurasMergePerCaster() {
    CSimulation sim;
    CUnitType* type = MakeAuraTestType();
    CUnit source(131, type, 0, War3ID("hfoo"), 0, 0, 0);
    CUnit other(132, type, 0, War3ID("hfoo"), 50, 0, 0);
    CUnit target(133, type, 0, War3ID("hfoo"), 100, 0, 0);
    sim.GetUnits().push_back(&source);
    sim.GetUnits().push_back(&other);
    sim.GetUnits().push_back(&target);

    // Same aura code from two casters (two Devotion Auras): per-source
    // buffs, strongest wins, first leaver keeps the other's bonus.
    CAbilityAura weak(230, War3ID("AHad"));
    weak.SetRadius(1000.0f);
    weak.SetEnemyOnly(true);
    weak.SetAffectsAllies(false);
    weak.SetAffectsSelf(false);
    weak.AddBonus({CAuraEffectType::ARMOR, 2.0f, false});
    CAbilityAura strong(231, War3ID("AHad"));
    strong.SetRadius(1000.0f);
    strong.SetEnemyOnly(true);
    strong.SetAffectsAllies(false);
    strong.SetAffectsSelf(false);
    strong.AddBonus({CAuraEffectType::ARMOR, 5.0f, false});

    auto countBuffs = [&]() {
        int n = 0;
        for (const auto& b : target.GetBuffs()) {
            if (b && b->GetId() == War3ID("AHad")) n++;
        }
        return n;
    };

    const int armorBefore = target.GetBonusArmor();
    weak.OnTick(sim, &source);
    TEST_ASSERT(target.GetBonusArmor() == armorBefore + 2);
    TEST_ASSERT(countBuffs() == 1);
    strong.OnTick(sim, &other);
    TEST_ASSERT(countBuffs() == 2);
    TEST_ASSERT(target.GetBonusArmor() == armorBefore + 5);
    // Strong caster leaves range: falls back to the weak source, not zero.
    strong.OnRemove(sim, &other);
    TEST_ASSERT(countBuffs() == 1);
    TEST_ASSERT(target.GetBonusArmor() == armorBefore + 2);
    weak.OnRemove(sim, &source);
    TEST_ASSERT(countBuffs() == 0);
    TEST_ASSERT(target.GetBonusArmor() == armorBefore);

    sim.GetUnits().clear();
    return true;
}
