// Ethereal / magic-immune damage rules + unit response sound sets.
// Ref: CUnitDefaultEtherealDamageModListener,
// CUnitDefaultMagicImmuneDamageModListener, CGameplayConstants ethereal
// parsing, UnitSound / UnitSoundset (UnitAckSounds.slk).
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "audio/UnitSoundset.h"
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

CUnitType* MakeMagicType() {
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

SlkTable MakeAckSlk() {
    // Minimal UnitAckSounds.slk: header row + one FootmanWhat row.
    const std::string text =
        "ID;PWXL;N;E\n"
        "C;X1;Y1;K\"Name\"\n"
        "C;X2;Y1;K\"FileNames\"\n"
        "C;X3;Y1;K\"DirectoryBase\"\n"
        "C;X4;Y1;K\"Volume\"\n"
        "C;X5;Y1;K\"Flags\"\n"
        "C;X1;Y2;K\"FootmanWhat\"\n"
        "C;X2;Y2;K\"FootmanWhat1.wav,FootmanWhat2.wav\"\n"
        "C;X3;Y2;K\"Units\\Human\\Footman\\\"\n"
        "C;X4;Y2;K\"100\"\n"
        "C;X5;Y2;K\"\"\n"
        "E\n";
    SlkTable slk;
    slk.Load(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    return slk;
}

} // namespace

bool TestEtherealNegatesPhysical() {
    CSimulation sim;
    auto* attacker = sim.AdoptUnit(std::make_unique<CUnit>(601, MakeMagicType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* target = sim.AdoptUnit(std::make_unique<CUnit>(602, MakeMagicType(), 1, War3ID("hfoo"), 100, 0, 0));
    sim.Update();
    target->SetEthereal(true);
    // NORMAL damage from a NORMAL attack is physical: negated.
    DamageResult r = sim.GetCombatSystem()->CalculateDamage(
        attacker, target, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 100, 0, 0, 0);
    TEST_ASSERT(r.damageDealt == 0.0f);
    return true;
}

bool TestEtherealBonusesMagic() {
    CSimulation sim;
    auto* attacker = sim.AdoptUnit(std::make_unique<CUnit>(611, MakeMagicType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* target = sim.AdoptUnit(std::make_unique<CUnit>(612, MakeMagicType(), 1, War3ID("hfoo"), 100, 0, 0));
    sim.Update();
    DamageResult plain = sim.GetCombatSystem()->CalculateDamage(
        attacker, target, false, false, CAttackType::MAGIC, CDamageType::MAGIC, 100, 0, 0, 0);
    DamageResult plainS = sim.GetCombatSystem()->CalculateDamage(
        attacker, target, false, false, CAttackType::SPELLS, CDamageType::FIRE, 100, 0, 0, 0);
    target->SetEthereal(true);
    DamageResult eth = sim.GetCombatSystem()->CalculateDamage(
        attacker, target, false, false, CAttackType::MAGIC, CDamageType::MAGIC, 100, 0, 0, 0);
    TEST_ASSERT(plain.damageDealt > 0.0f);
    TEST_NEAR(eth.damageDealt, plain.damageDealt * 1.6667f, 0.5f);
    // SPELLS bonus path too.
    DamageResult ethS = sim.GetCombatSystem()->CalculateDamage(
        attacker, target, false, false, CAttackType::SPELLS, CDamageType::FIRE, 100, 0, 0, 0);
    TEST_ASSERT(ethS.damageDealt > plainS.damageDealt);
    return true;
}

bool TestEtherealAllySkipsBonus() {
    CSimulation sim;
    auto* attacker = sim.AdoptUnit(std::make_unique<CUnit>(631, MakeMagicType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* target = sim.AdoptUnit(std::make_unique<CUnit>(632, MakeMagicType(), 0, War3ID("hfoo"), 100, 0, 0));
    sim.Update();
    DamageResult plain = sim.GetCombatSystem()->CalculateDamage(
        attacker, target, false, false, CAttackType::MAGIC, CDamageType::MAGIC, 100, 0, 0, 0);
    target->SetEthereal(true);
    DamageResult allied = sim.GetCombatSystem()->CalculateDamage(
        attacker, target, false, false, CAttackType::MAGIC, CDamageType::MAGIC, 100, 0, 0, 0);
    TEST_NEAR(allied.damageDealt, plain.damageDealt, 0.5f);
    sim.GetCombatSystem()->ConfigureMagicRules(2.0f, 2.0f, true, true, true);
    DamageResult boosted = sim.GetCombatSystem()->CalculateDamage(
        attacker, target, false, false, CAttackType::MAGIC, CDamageType::MAGIC, 100, 0, 0, 0);
    TEST_ASSERT(boosted.damageDealt > allied.damageDealt);
    return true;
}

bool TestMagicImmuneResists() {
    CSimulation sim;
    auto* attacker = sim.AdoptUnit(std::make_unique<CUnit>(621, MakeMagicType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* target = sim.AdoptUnit(std::make_unique<CUnit>(622, MakeMagicType(), 1, War3ID("hfoo"), 100, 0, 0));
    sim.Update();
    target->SetMagicImmune(true);
    // MAGIC attack fizzles.
    DamageResult m = sim.GetCombatSystem()->CalculateDamage(
        attacker, target, false, false, CAttackType::MAGIC, CDamageType::FIRE, 100, 0, 0, 0);
    TEST_ASSERT(m.damageDealt == 0.0f);
    // SPELLS Schroeder: non-universal fizzles...
    DamageResult s = sim.GetCombatSystem()->CalculateDamage(
        attacker, target, false, false, CAttackType::SPELLS, CDamageType::FIRE, 100, 0, 0, 0);
    TEST_ASSERT(s.damageDealt == 0.0f);
    // ...but plain NORMAL physical passes through.
    DamageResult p = sim.GetCombatSystem()->CalculateDamage(
        attacker, target, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 100, 0, 0, 0);
    TEST_ASSERT(p.damageDealt > 0.0f);
    // UNIVERSAL ultimates pierce once the gate is off.
    CGameplayConstants gc;
    gc.magicImmuneResistsUltimates = false;
    sim.SetGameplayConstants(gc);
    DamageResult u = sim.GetCombatSystem()->CalculateDamage(
        attacker, target, false, false, CAttackType::SPELLS, CDamageType::UNIVERSAL, 100, 0, 0, 0);
    TEST_ASSERT(u.damageDealt > 0.0f);
    return true;
}

bool TestGameplayConstantsEtherealParsed() {
    const std::string text =
        "[Misc]\n"
        "EtherealDamageBonus=0,0,0,2.0,0,3.0\n"
        "EtherealDamageBonusAlly=1\n"
        "MagicImmunesResistDamage=0\n"
        "MagicImmunesResistUltimates=0\n";
    TxtTable misc;
    misc.Load(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    CGameplayConstants gc(misc);
    TEST_NEAR(gc.etherealDamageBonusMagic, 2.0f, 1e-6f);
    TEST_NEAR(gc.etherealDamageBonusSpells, 3.0f, 1e-6f);
    TEST_ASSERT(gc.etherealDamageBonusAlly);
    TEST_ASSERT(!gc.magicImmuneResistsDamage);
    TEST_ASSERT(!gc.magicImmuneResistsUltimates);
    // Defaults when absent.
    TxtTable empty;
    const std::string e2 = "[Misc]\nDefenseArmor=0.06\n";
    empty.Load(reinterpret_cast<const uint8_t*>(e2.data()), e2.size());
    CGameplayConstants dflt(empty);
    TEST_NEAR(dflt.etherealDamageBonusMagic, 1.6667f, 1e-4f);
    TEST_ASSERT(dflt.magicImmuneResistsDamage);
    return true;
}

bool TestUnitSoundsetResolve() {
    SlkTable slk = MakeAckSlk();
    UnitAckSoundDesc desc = UnitSoundset::Resolve(slk, "Footman", "What");
    TEST_ASSERT(desc.found);
    TEST_ASSERT(desc.files.size() == 2);
    TEST_ASSERT(desc.files[0] == "Units\\Human\\Footman\\FootmanWhat1.wav");
    TEST_NEAR(desc.volume, 100.0f / 127.0f, 1e-4f);
    TEST_ASSERT(UnitSoundset::PickFile(desc, 0) == desc.files[0]);
    TEST_ASSERT(UnitSoundset::PickFile(desc, 2) == desc.files[0]); // wraps
    UnitAckSoundDesc missing = UnitSoundset::Resolve(slk, "Footman", "Warcry");
    TEST_ASSERT(!missing.found);
    auto all = UnitSoundset::ResolveAll(slk, "Footman");
    TEST_ASSERT(all.size() == 6);
    TEST_ASSERT(all[0].found); // What
    return true;
}

bool TestUnitTypeSoundDefault() {
    CUnitType* t = MakeMagicType();
    TEST_ASSERT(t->GetUnitSound().empty());
    t->SetUnitSound("Footman");
    TEST_ASSERT(t->GetUnitSound() == "Footman");
    return true;
}
