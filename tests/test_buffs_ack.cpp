// Buff-driven ethereal / magic immunity + ack sound bank playback.
// Ref: ethereal/magic damage rules (previous round), UnitSoundset row
// resolution; Banish/Anti-magic buffs are new (ref never ported them).
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "audio/AckSoundBank.h"
#include "data/combat_types.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/combat/combat_system.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "resource/slk.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)

namespace {

CUnitType* MakeAckType() {
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

SlkTable MakeAckSlk2() {
    const std::string text =
        "ID;PWXL;N;E\n"
        "C;X1;Y1;K\"Name\"\n"
        "C;X2;Y1;K\"FileNames\"\n"
        "C;X3;Y1;K\"DirectoryBase\"\n"
        "C;X4;Y1;K\"Volume\"\n"
        "C;X1;Y2;K\"FootmanYes\"\n"
        "C;X2;Y2;K\"FootmanYes1.wav\"\n"
        "C;X3;Y2;K\"Units\\Human\\Footman\\\"\n"
        "C;X4;Y2;K\"127\"\n"
        "E\n";
    SlkTable slk;
    slk.Load(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    return slk;
}

} // namespace

bool TestEtherealBuffLifecycle() {
    CSimulation sim;
    auto* u = sim.AdoptUnit(std::make_unique<CUnit>(701, MakeAckType(), 0, War3ID("hfoo"), 0, 0, 0));
    sim.Update();
    TEST_ASSERT(!u->IsEthereal());
    u->AddBuff(std::make_unique<CBuffEthereal>(10.0f), sim);
    TEST_ASSERT(u->IsEthereal());
    // A second source (different id) stacks; removing one keeps the flag.
    u->AddBuff(std::make_unique<CBuffEthereal>(10.0f, War3ID("Bba2")), sim);
    u->RemoveBuff(War3ID("Bban"), sim);
    TEST_ASSERT(u->IsEthereal());
    u->RemoveBuff(War3ID("Bba2"), sim);
    TEST_ASSERT(!u->IsEthereal());
    return true;
}

bool TestMagicImmuneBuffLifecycle() {
    CSimulation sim;
    auto* u = sim.AdoptUnit(std::make_unique<CUnit>(711, MakeAckType(), 0, War3ID("hfoo"), 0, 0, 0));
    sim.Update();
    TEST_ASSERT(!u->IsMagicImmune());
    u->AddBuff(std::make_unique<CBuffMagicImmune>(10.0f), sim);
    TEST_ASSERT(u->IsMagicImmune());
    u->RemoveBuff(War3ID("Bam2"), sim);
    TEST_ASSERT(!u->IsMagicImmune());
    return true;
}

bool TestEtherealBuffBlocksPhysical() {
    CSimulation sim;
    auto* attacker = sim.AdoptUnit(std::make_unique<CUnit>(721, MakeAckType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* target = sim.AdoptUnit(std::make_unique<CUnit>(722, MakeAckType(), 1, War3ID("hfoo"), 100, 0, 0));
    sim.Update();
    target->AddBuff(std::make_unique<CBuffEthereal>(10.0f), sim);
    DamageResult r = sim.GetCombatSystem()->CalculateDamage(
        attacker, target, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 100, 0, 0, 0);
    TEST_ASSERT(r.damageDealt == 0.0f);
    target->RemoveBuff(War3ID("Bban"), sim);
    DamageResult r2 = sim.GetCombatSystem()->CalculateDamage(
        attacker, target, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 100, 0, 0, 0);
    TEST_ASSERT(r2.damageDealt > 0.0f);
    return true;
}

bool TestAckBankResolve() {
    AckSoundBank bank;
    TEST_ASSERT(!bank.HasData());
    bank.LoadFromSlk(MakeAckSlk2());
    TEST_ASSERT(bank.HasData());
    UnitAckSoundDesc desc = bank.Resolve("Footman", "Yes");
    TEST_ASSERT(desc.found);
    TEST_ASSERT(desc.files.size() == 1);
    TEST_ASSERT(desc.files[0] == "Units\\Human\\Footman\\FootmanYes1.wav");
    TEST_ASSERT(!bank.Resolve("Footman", "Warcry").found);
    return true;
}

bool TestAckBankPlaySafeWithoutAudio() {
    CSimulation sim; // no audio system attached: must no-op, not crash
    auto* u = sim.AdoptUnit(std::make_unique<CUnit>(731, MakeAckType(), 0, War3ID("hfoo"), 0, 0, 0));
    sim.Update();
    AckSoundBank bank;
    bank.LoadFromSlk(MakeAckSlk2());
    // No unitSound on the type and no audio backend: -1.
    TEST_ASSERT(bank.PlayResponse(sim, u, "Yes") == -1);
    MakeAckType()->SetUnitSound("Footman");
    TEST_ASSERT(bank.PlayResponse(sim, u, "Yes") == -1); // still no audio
    MakeAckType()->SetUnitSound("");
    TEST_ASSERT(bank.PlayResponse(sim, nullptr, "Yes") == -1);
    AckSoundBank empty;
    TEST_ASSERT(empty.PlayResponse(sim, u, "Yes") == -1);
    return true;
}
