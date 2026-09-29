// Batch-2 native tests: spell effects, ability art/sound lookup, images,
// trackables (+event interest), ubersplats (ref common.j visual natives).
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include "data/abilitydata.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "resource/mpq_storm.h"
#include "scripting/JassNatives.h"
#include "scripting/JassVM.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace fs = std::filesystem;

namespace {

CUnitType* MakeVisualType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Footman", "Footman", "icon", War3ID("hfoo"),
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

JassValue CallNative(JassVM& vm, const std::string& name, const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) return JassValue::Null();
    return it->second(args);
}

bool ResourcesPresent() {
    return fs::exists(fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3.mpq");
}

} // namespace

bool TestAbilityArtLookupFromProfiles() {
    if (!ResourcesPresent()) return true; // SKIP without game data
    MpqManager mpq;
    mpq.Open((fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3.mpq").string());
    mpq.Open((fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3x.mpq").string());
    CAbilityData data = CAbilityData::LoadFromMpq(mpq);

    // Storm Bolt has missile art; Frost Nova has effect art; building-fire
    // abilities carry TargetArt lists (all from the Func profiles).
    const CAbilityDefFull* bolt = data.GetAbility(War3ID("AHtb"));
    TEST_ASSERT(bolt != nullptr);
    TEST_ASSERT(!bolt->effectArtByType[5].empty());
    const CAbilityDefFull* nova = data.GetAbility(War3ID("AUfn"));
    TEST_ASSERT(nova != nullptr);
    TEST_ASSERT(!nova->effectArtByType[0].empty());

    JassVM vm;
    CSimulation sim;
    sim.SetAbilityData(&data);
    JassNatives natives(vm, &sim);
    natives.RegisterAll();

    // String and ById forms agree; out-of-range type reads empty.
    const std::string viaStr = CallNative(vm, "GetAbilityEffect",
        {JassValue::Str("AHtb"), JassValue::Int(5), JassValue::Int(0)}).strVal;
    TEST_ASSERT(!viaStr.empty());
    const int rawcode = ((int)'A' << 24) | ((int)'H' << 16) | ((int)'t' << 8) | (int)'b';
    const std::string viaId = CallNative(vm, "GetAbilityEffectById",
        {JassValue::Int(rawcode), JassValue::Int(5), JassValue::Int(0)}).strVal;
    TEST_ASSERT(viaId == viaStr);
    TEST_ASSERT(CallNative(vm, "GetAbilityEffectById",
        {JassValue::Int(rawcode), JassValue::Int(99), JassValue::Int(0)}).strVal.empty());
    TEST_ASSERT(CallNative(vm, "GetAbilityEffectById",
        {JassValue::Int(0), JassValue::Int(1), JassValue::Int(0)}).strVal.empty());
    // Sound lookup round-trips between forms (empty when unprofiled).
    const std::string sndStr = CallNative(vm, "GetAbilitySound",
        {JassValue::Str("AHtb"), JassValue::Int(1), JassValue::Int(0)}).strVal;
    const std::string sndId = CallNative(vm, "GetAbilitySoundById",
        {JassValue::Int(rawcode), JassValue::Int(1), JassValue::Int(0)}).strVal;
    TEST_ASSERT(sndStr == sndId);
    // Submerge carries a profiled Effectsound when present in the data.
    if (data.GetAbility(War3ID("Asb1")) != nullptr) {
        const int submerge = ((int)'A' << 24) | ((int)'s' << 16) | ((int)'b' << 8) | (int)'1';
        TEST_ASSERT(!CallNative(vm, "GetAbilitySoundById",
            {JassValue::Int(submerge), JassValue::Int(1), JassValue::Int(0)}).strVal.empty());
    }
    return true;
}

bool TestSpellEffectNatives() {
    if (!ResourcesPresent()) return true; // SKIP without game data
    MpqManager mpq;
    mpq.Open((fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3.mpq").string());
    mpq.Open((fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3x.mpq").string());
    CAbilityData data = CAbilityData::LoadFromMpq(mpq);

    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.SetAbilityData(&data);
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto ndata = natives.GetData();

    auto* u = sim.AdoptUnit(std::make_unique<CUnit>(8201, MakeVisualType(), 0, War3ID("hfoo"), 100, 200, 0));
    sim.Update();
    (void)u;
    const int uh = ndata->AllocHandle(JassHandleInfo::UNIT, 8201);
    const int rawcode = ((int)'A' << 24) | ((int)'H' << 16) | ((int)'t' << 8) | (int)'b';

    // ById point effect resolves art from the ability profile (Storm Bolt
    // carries missile art = type 5).
    const int fx = CallNative(vm, "AddSpellEffectById",
        {JassValue::Int(rawcode), JassValue::Int(5), JassValue::Real(100), JassValue::Real(200)}).intVal;
    TEST_ASSERT(fx != 0);
    {
        int fxSim = ndata->GetSimHandle(fx, JassHandleInfo::EFFECT);
        TEST_ASSERT(fxSim >= 0);
        auto it = ndata->effects.find(fxSim);
        TEST_ASSERT(it != ndata->effects.end());
        TEST_ASSERT(!it->second->modelPath.empty());
        TEST_ASSERT(it->second->x == 100.0f && it->second->y == 200.0f);
    }

    // Target effect records the target + attach point.
    const int tfx = CallNative(vm, "AddSpellEffectTarget",
        {JassValue::Str("Abilities\\Spells\\Human\\ThunderBolt\\ThunderBolt.mdx"),
         JassValue::Int(1), JassValue::Int(uh), JassValue::Str("chest")}).intVal;
    TEST_ASSERT(tfx != 0);
    {
        int fxSim = ndata->GetSimHandle(tfx, JassHandleInfo::EFFECT);
        auto it = ndata->effects.find(fxSim);
        TEST_ASSERT(it != ndata->effects.end());
        TEST_ASSERT(it->second->targetSimHandle == 8201);
        TEST_ASSERT(it->second->attachPoint == "chest");
    }

    // Destroy frees; double-destroy is safe.
    CallNative(vm, "DestroyEffect", {JassValue::Int(fx)});
    TEST_ASSERT(ndata->GetSimHandle(fx, JassHandleInfo::EFFECT) < 0);
    CallNative(vm, "DestroyEffect", {JassValue::Int(fx)});
    return true;
}

bool TestImageNatives() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto ndata = natives.GetData();

    const int img = CallNative(vm, "CreateImage",
        {JassValue::Str("UI\\Widgets\\Button.blp"),
         JassValue::Real(64), JassValue::Real(64), JassValue::Real(0),
         JassValue::Real(100), JassValue::Real(200), JassValue::Real(10),
         JassValue::Real(32), JassValue::Real(32), JassValue::Real(0),
         JassValue::Int(1)}).intVal;
    TEST_ASSERT(img != 0);
    const int imgSim = ndata->GetSimHandle(img, JassHandleInfo::IMAGE);
    TEST_ASSERT(imgSim >= 0);
    {
        auto it = ndata->images.find(imgSim);
        TEST_ASSERT(it != ndata->images.end());
        TEST_ASSERT(it->second->file == "UI\\Widgets\\Button.blp");
        TEST_ASSERT(!it->second->visible);
    }
    CallNative(vm, "ShowImage", {JassValue::Int(img), JassValue::Bool(true)});
    CallNative(vm, "SetImageColor", {JassValue::Int(img), JassValue::Int(255), JassValue::Int(0), JassValue::Int(0), JassValue::Int(128)});
    CallNative(vm, "SetImagePosition", {JassValue::Int(img), JassValue::Real(1), JassValue::Real(2), JassValue::Real(3)});
    CallNative(vm, "SetImageRender", {JassValue::Int(img), JassValue::Bool(false)});
    CallNative(vm, "SetImageRenderAlways", {JassValue::Int(img), JassValue::Bool(true)});
    CallNative(vm, "SetImageAboveWater", {JassValue::Int(img), JassValue::Bool(true), JassValue::Bool(true)});
    CallNative(vm, "SetImageConstantHeight", {JassValue::Int(img), JassValue::Bool(true), JassValue::Real(50)});
    CallNative(vm, "SetImageType", {JassValue::Int(img), JassValue::Int(2)});
    {
        auto it = ndata->images.find(imgSim);
        TEST_ASSERT(it->second->visible);
        TEST_ASSERT(it->second->red == 255 && it->second->green == 0 && it->second->alpha == 128);
        TEST_ASSERT(it->second->posX == 1.0f && it->second->posZ == 3.0f);
        TEST_ASSERT(!it->second->render && it->second->renderAlways);
        TEST_ASSERT(it->second->aboveWater && it->second->constantHeight);
        TEST_ASSERT(it->second->imageType == 2);
    }
    CallNative(vm, "DestroyImage", {JassValue::Int(img)});
    TEST_ASSERT(ndata->images.find(imgSim) == ndata->images.end());
    return true;
}

bool TestTrackableAndUbersplatNatives() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto ndata = natives.GetData();

    const int track = CallNative(vm, "CreateTrackable",
        {JassValue::Str("Doodads\\Cinematic\\Torch\\Torch.mdx"),
         JassValue::Real(500), JassValue::Real(600), JassValue::Real(90)}).intVal;
    TEST_ASSERT(track != 0);
    {
        const int tSim = ndata->GetSimHandle(track, JassHandleInfo::TRACKABLE);
        TEST_ASSERT(tSim >= 0);
        auto it = ndata->trackables.find(tSim);
        TEST_ASSERT(it != ndata->trackables.end());
        TEST_ASSERT(it->second->x == 500.0f && it->second->facing == 90.0f);
    }
    // Event interest is stored on the trigger for the picking path.
    const int trig = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterTrackableHitEvent", {JassValue::Int(trig), JassValue::Int(track)});
    CallNative(vm, "TriggerRegisterTrackableTrackEvent", {JassValue::Int(trig), JassValue::Int(track)});
    {
        auto it = ndata->triggers.find(trig);
        TEST_ASSERT(it != ndata->triggers.end());
        bool hit = false, trackEv = false;
        for (const auto& [handle, code] : it->second->registeredEvents) {
            if (code == 4101) hit = true;
            if (code == 4102) trackEv = true;
        }
        TEST_ASSERT(hit && trackEv);
    }

    const int splat = CallNative(vm, "CreateUbersplat",
        {JassValue::Real(1000), JassValue::Real(1000), JassValue::Str("UberSplat"),
         JassValue::Int(255), JassValue::Int(255), JassValue::Int(255), JassValue::Int(255),
         JassValue::Bool(false), JassValue::Bool(false)}).intVal;
    TEST_ASSERT(splat != 0);
    const int splatSim = ndata->GetSimHandle(splat, JassHandleInfo::UBERSPLAT);
    TEST_ASSERT(splatSim >= 0);
    CallNative(vm, "FinishUbersplat", {JassValue::Int(splat)});
    TEST_ASSERT(ndata->ubersplats[splatSim]->finished);
    CallNative(vm, "ResetUbersplat", {JassValue::Int(splat)});
    TEST_ASSERT(!ndata->ubersplats[splatSim]->finished);
    CallNative(vm, "ShowUbersplat", {JassValue::Int(splat), JassValue::Bool(false)});
    TEST_ASSERT(!ndata->ubersplats[splatSim]->render);
    CallNative(vm, "SetUbersplatRenderAlways", {JassValue::Int(splat), JassValue::Bool(true)});
    TEST_ASSERT(ndata->ubersplats[splatSim]->renderAlways);
    CallNative(vm, "DestroyUbersplat", {JassValue::Int(splat)});
    TEST_ASSERT(ndata->ubersplats.find(splatSim) == ndata->ubersplats.end());
    return true;
}

bool TestTrackableEnterAndHit() {
    const char* script =
        "globals\n"
        "integer enterCount = 0\n"
        "integer hitCount = 0\n"
        "integer lastTrack = 0\n"
        "endglobals\n"
        "function OnTrackEnter takes nothing returns nothing\n"
        "set enterCount = enterCount + 1\n"
        "set lastTrack = GetTriggeringTrackable()\n"
        "endfunction\n"
        "function OnTrackHit takes nothing returns nothing\n"
        "set hitCount = hitCount + 1\n"
        "set lastTrack = GetTriggeringTrackable()\n"
        "endfunction\n";

    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    TEST_ASSERT(vm.LoadScript(script));

    const int track = CallNative(vm, "CreateTrackable",
        {JassValue::Str("Doodads\\Cinematic\\Torch\\Torch.mdx"),
         JassValue::Real(500), JassValue::Real(600), JassValue::Real(0)}).intVal;
    TEST_ASSERT(track != 0);
    const int enterTrig = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterTrackableTrackEvent", {JassValue::Int(enterTrig), JassValue::Int(track)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(enterTrig), JassValue::Str("OnTrackEnter")});
    const int hitTrig = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterTrackableHitEvent", {JassValue::Int(hitTrig), JassValue::Int(track)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(hitTrig), JassValue::Str("OnTrackHit")});

    auto* unit = sim.AdoptUnit(std::make_unique<CUnit>(8301, MakeVisualType(), 0, War3ID("hfoo"), 0, 0, 0));
    TEST_ASSERT(unit != nullptr);
    sim.Update();

    natives.UpdateTimers(0.0f);
    natives.PumpQueuedThreads(64);
    TEST_ASSERT(vm.GetGlobal("enterCount").intVal == 0);

    unit->SetPosition(500.0f, 600.0f);
    natives.UpdateTimers(0.0f);
    natives.PumpQueuedThreads(64);
    TEST_ASSERT(vm.GetGlobal("enterCount").intVal == 1);
    TEST_ASSERT(vm.GetGlobal("lastTrack").intVal == track);

    natives.UpdateTimers(0.0f);
    natives.PumpQueuedThreads(64);
    TEST_ASSERT(vm.GetGlobal("enterCount").intVal == 1);

    unit->SetPosition(0.0f, 0.0f);
    natives.UpdateTimers(0.0f);
    unit->SetPosition(510.0f, 600.0f);
    natives.UpdateTimers(0.0f);
    natives.PumpQueuedThreads(64);
    TEST_ASSERT(vm.GetGlobal("enterCount").intVal == 2);

    natives.NotifyTrackableClick(500.0f, 600.0f);
    natives.PumpQueuedThreads(64);
    TEST_ASSERT(vm.GetGlobal("hitCount").intVal == 1);
    TEST_ASSERT(vm.GetGlobal("lastTrack").intVal == track);
    natives.NotifyTrackableClick(0.0f, 0.0f);
    natives.PumpQueuedThreads(64);
    TEST_ASSERT(vm.GetGlobal("hitCount").intVal == 1);
    return true;
}
