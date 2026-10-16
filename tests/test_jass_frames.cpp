// Frame + LocalStore + ConvertColor alignment tests (ref Jass2 frame family).
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "scripting/JassNatives.h"
#include "scripting/JassVM.h"

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace {

JassValue CallNative(JassVM& vm, const std::string& name, const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) {
        std::cerr << "missing native " << name << "\n";
        return JassValue::Null();
    }
    return it->second(args);
}

struct Fixture {
    JassVM vm;
    CSimulation sim;
    std::unique_ptr<JassNatives> natives;
    std::shared_ptr<JassNativesData> data;
    Fixture() {
        sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
        natives = std::make_unique<JassNatives>(vm, &sim);
        natives->RegisterAll();
        data = natives->GetData();
    }
};

} // namespace

bool TestFrameNatives() {
    Fixture f;
    // ConvertColor packs ARGB like ref (255,0,0,0) -> 0xFF000000.
    {
        JassValue c = CallNative(f.vm, "ConvertColor",
            {JassValue::Int(255), JassValue::Int(0), JassValue::Int(0), JassValue::Int(0)});
        TEST_ASSERT(c.intVal == (int32_t)0xFF000000);
    }
    // Root + TOC + simple/frame + lookup.
    const int root = CallNative(f.vm, "CreateRootFrame", {JassValue::Str("Human")}).intVal;
    TEST_ASSERT(root != 0);
    TEST_ASSERT(CallNative(f.vm, "LoadTOCFile", {JassValue::Str("UI\\FrameDef\\FrameDef.toc")}).boolVal);
    TEST_ASSERT(f.data->loadedTocFiles.size() == 1);
    const int simple = CallNative(f.vm, "CreateSimpleFrame",
        {JassValue::Str("MyTemplate"), JassValue::Int(root), JassValue::Int(0)}).intVal;
    TEST_ASSERT(simple != 0);
    const int frame = CallNative(f.vm, "CreateFrame",
        {JassValue::Str("MyTemplate"), JassValue::Int(root), JassValue::Int(1), JassValue::Int(0)}).intVal;
    TEST_ASSERT(frame != 0);
    const int byName = CallNative(f.vm, "GetFrameByName",
        {JassValue::Str("MyTemplate"), JassValue::Int(0)}).intVal;
    TEST_ASSERT(byName != 0);
    // Text + color + anchors + position.
    CallNative(f.vm, "FrameSetText", {JassValue::Int(frame), JassValue::Str("Hello")});
    CallNative(f.vm, "FrameSetTextColor", {JassValue::Int(frame), JassValue::Int((int32_t)0xFFFF0000)});
    CallNative(f.vm, "FrameSetAnchor",
        {JassValue::Int(frame), JassValue::Int(0), JassValue::Real(0.1f), JassValue::Real(0.2f)});
    CallNative(f.vm, "FrameSetAbsPoint",
        {JassValue::Int(frame), JassValue::Int(1), JassValue::Real(0.3f), JassValue::Real(0.4f)});
    CallNative(f.vm, "FrameSetPoint", {JassValue::Int(frame), JassValue::Int(0),
                                       JassValue::Int(root), JassValue::Int(0),
                                       JassValue::Real(0), JassValue::Real(0)});
    CallNative(f.vm, "FramePositionBounds", {JassValue::Int(frame)});
    {
        int simId = f.data->GetSimHandle(frame, JassHandleInfo::FRAME);
        TEST_ASSERT(simId >= 0);
        auto it = f.data->frames.find(simId);
        TEST_ASSERT(it != f.data->frames.end());
        TEST_ASSERT(it->second->text == "Hello");
        TEST_ASSERT(it->second->textColor == 0xFFFF0000u);
        TEST_ASSERT(it->second->anchors.size() == 3);
        TEST_ASSERT(it->second->positioned);
    }
    TEST_ASSERT(CallNative(f.vm, "SkinGetField", {JassValue::Str("Unknown")}).strVal.empty());
    // Bad handles are safe no-ops.
    CallNative(f.vm, "FrameSetText", {JassValue::Int(999999), JassValue::Str("x")});
    return true;
}

bool TestLocalStoreNatives() {
    Fixture f;
    const int store = CallNative(f.vm, "CreateLocalStore", {}).intVal;
    TEST_ASSERT(store != 0);
    CallNative(f.vm, "SetLocalStoreInteger", {JassValue::Int(store), JassValue::Str("k"), JassValue::Int(7)});
    CallNative(f.vm, "SetLocalStoreReal",
        {JassValue::Int(store), JassValue::Str("r"), JassValue::Real(1.5f)});
    CallNative(f.vm, "SetLocalStoreString",
        {JassValue::Int(store), JassValue::Str("s"), JassValue::Str("v")});
    CallNative(f.vm, "SetLocalStoreBoolean",
        {JassValue::Int(store), JassValue::Str("b"), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(f.vm, "GetLocalStoreInteger",
        {JassValue::Int(store), JassValue::Str("k")}).intVal == 7);
    TEST_ASSERT(CallNative(f.vm, "GetLocalStoreReal",
        {JassValue::Int(store), JassValue::Str("r")}).realVal == 1.5f);
    TEST_ASSERT(CallNative(f.vm, "GetLocalStoreString",
        {JassValue::Int(store), JassValue::Str("s")}).strVal == "v");
    TEST_ASSERT(CallNative(f.vm, "GetLocalStoreBoolean",
        {JassValue::Int(store), JassValue::Str("b")}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "LocalStoreContainsKey",
        {JassValue::Int(store), JassValue::Str("k")}).boolVal);
    TEST_ASSERT(!CallNative(f.vm, "LocalStoreContainsKey",
        {JassValue::Int(store), JassValue::Str("nope")}).boolVal);
    // AbilityBuilder stubs survive (Warsmash-private maps).
    CallNative(f.vm, "RegisterABConf", {});
    TEST_ASSERT(CallNative(f.vm, "CreateAbilityBuilderConfiguration", {}).intVal == 0);
    TEST_ASSERT(!CallNative(f.vm, "IsOrderButtonDisabled", {JassValue::Int(0)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "GetGameObjectFieldAsString",
        {JassValue::Int(0), JassValue::Str("x")}).strVal.empty());
    CallNative(f.vm, "DestroyLocalStore", {JassValue::Int(store)});
    TEST_ASSERT(!CallNative(f.vm, "LocalStoreContainsKey",
        {JassValue::Int(store), JassValue::Str("k")}).boolVal);
    return true;
}

bool TestBlzFrameNatives() {
    Fixture f;
    // Create + lookup round-trip; repeated lookups alias the same frame.
    const int frame = CallNative(f.vm, "BlzCreateFrame",
        {JassValue::Str("MyBox"), JassValue::Int(0), JassValue::Int(1), JassValue::Int(0)}).intVal;
    TEST_ASSERT(frame != 0);
    const int simple = CallNative(f.vm, "BlzCreateSimpleFrame",
        {JassValue::Str("MyText"), JassValue::Int(frame), JassValue::Int(0)}).intVal;
    TEST_ASSERT(simple != 0 && simple != frame);
    const int byName = CallNative(f.vm, "BlzGetFrameByName",
        {JassValue::Str("MyBox"), JassValue::Int(0)}).intVal;
    TEST_ASSERT(byName != 0);
    TEST_ASSERT(CallNative(f.vm, "BlzFrameGetName", {JassValue::Int(frame)}).strVal == "MyBox");
    // Origin frames are stable singletons per (type, index).
    const int o1 = CallNative(f.vm, "BlzGetOriginFrame",
        {JassValue::Int(0), JassValue::Int(0)}).intVal;
    const int o2 = CallNative(f.vm, "BlzGetOriginFrame",
        {JassValue::Int(0), JassValue::Int(0)}).intVal;
    TEST_ASSERT(o1 != 0 && o2 != 0);
    {
        const int s1 = f.data->GetSimHandle(o1, JassHandleInfo::FRAME);
        const int s2 = f.data->GetSimHandle(o2, JassHandleInfo::FRAME);
        TEST_ASSERT(s1 >= 0 && s1 == s2);
    }
    // Text set/get/append + size limit.
    CallNative(f.vm, "BlzFrameSetText", {JassValue::Int(frame), JassValue::Str("Hello")});
    TEST_ASSERT(CallNative(f.vm, "BlzFrameGetText", {JassValue::Int(frame)}).strVal == "Hello");
    CallNative(f.vm, "BlzFrameAddText", {JassValue::Int(frame), JassValue::Str(" World")});
    TEST_ASSERT(CallNative(f.vm, "BlzFrameGetText", {JassValue::Int(frame)}).strVal == "Hello World");
    CallNative(f.vm, "BlzFrameSetTextSizeLimit", {JassValue::Int(frame), JassValue::Int(99)});
    TEST_ASSERT(CallNative(f.vm, "BlzFrameGetTextSizeLimit", {JassValue::Int(frame)}).intVal == 99);
    // Visible / enable / focus / click / alpha round-trips.
    TEST_ASSERT(CallNative(f.vm, "BlzFrameIsVisible", {JassValue::Int(frame)}).boolVal);
    CallNative(f.vm, "BlzFrameSetVisible", {JassValue::Int(frame), JassValue::Bool(false)});
    TEST_ASSERT(!CallNative(f.vm, "BlzFrameIsVisible", {JassValue::Int(frame)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "BlzFrameGetEnable", {JassValue::Int(frame)}).boolVal);
    CallNative(f.vm, "BlzFrameSetEnable", {JassValue::Int(frame), JassValue::Bool(false)});
    TEST_ASSERT(!CallNative(f.vm, "BlzFrameGetEnable", {JassValue::Int(frame)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "BlzFrameSetFocus", {JassValue::Int(frame), JassValue::Bool(true)}).boolVal);
    CallNative(f.vm, "BlzFrameClick", {JassValue::Int(frame)});
    CallNative(f.vm, "BlzFrameSetAlpha", {JassValue::Int(frame), JassValue::Int(128)});
    TEST_ASSERT(CallNative(f.vm, "BlzFrameGetAlpha", {JassValue::Int(frame)}).intVal == 128);
    // Points + size + value + min/max/step round-trips.
    CallNative(f.vm, "BlzFrameSetAbsPoint",
        {JassValue::Int(frame), JassValue::Int(0), JassValue::Real(0.1f), JassValue::Real(0.2f)});
    CallNative(f.vm, "BlzFrameSetPoint",
        {JassValue::Int(simple), JassValue::Int(0), JassValue::Int(frame),
         JassValue::Int(0), JassValue::Real(0), JassValue::Real(0)});
    CallNative(f.vm, "BlzFrameSetSize",
        {JassValue::Int(frame), JassValue::Real(0.3f), JassValue::Real(0.05f)});
    TEST_ASSERT(CallNative(f.vm, "BlzFrameGetWidth", {JassValue::Int(frame)}).realVal == 0.3f);
    TEST_ASSERT(CallNative(f.vm, "BlzFrameGetHeight", {JassValue::Int(frame)}).realVal == 0.05f);
    CallNative(f.vm, "BlzFrameSetValue", {JassValue::Int(frame), JassValue::Real(42.5f)});
    TEST_ASSERT(CallNative(f.vm, "BlzFrameGetValue", {JassValue::Int(frame)}).realVal == 42.5f);
    CallNative(f.vm, "BlzFrameSetMinMaxValue",
        {JassValue::Int(frame), JassValue::Real(0), JassValue::Real(100)});
    CallNative(f.vm, "BlzFrameSetStepSize", {JassValue::Int(frame), JassValue::Real(5)});
    CallNative(f.vm, "BlzFrameSetScale", {JassValue::Int(frame), JassValue::Real(1.5f)});
    CallNative(f.vm, "BlzFrameSetModel",
        {JassValue::Int(frame), JassValue::Str("UI\\Model.mdx"), JassValue::Int(0)});
    CallNative(f.vm, "BlzFrameSetTexture",
        {JassValue::Int(frame), JassValue::Str("UI\\Tex.blp"), JassValue::Int(0), JassValue::Bool(true)});
    CallNative(f.vm, "BlzFrameSetSpriteAnimate",
        {JassValue::Int(frame), JassValue::Int(1), JassValue::Int(0)});
    CallNative(f.vm, "BlzFrameSetVertexColor",
        {JassValue::Int(frame), JassValue::Int((int32_t)0xFF00FF00)});
    CallNative(f.vm, "BlzFrameSetLevel", {JassValue::Int(frame), JassValue::Int(3)});
    CallNative(f.vm, "BlzFrameCageMouse", {JassValue::Int(frame), JassValue::Bool(true)});
    // Parent / children / tooltip linkage.
    CallNative(f.vm, "BlzFrameSetParent", {JassValue::Int(simple), JassValue::Int(frame)});
    TEST_ASSERT(CallNative(f.vm, "BlzFrameGetChildrenCount", {JassValue::Int(frame)}).intVal == 1);
    const int kid = CallNative(f.vm, "BlzFrameGetChild", {JassValue::Int(frame), JassValue::Int(0)}).intVal;
    TEST_ASSERT(kid != 0);
    {
        const int ks = f.data->GetSimHandle(kid, JassHandleInfo::FRAME);
        const int ss = f.data->GetSimHandle(simple, JassHandleInfo::FRAME);
        TEST_ASSERT(ks >= 0 && ks == ss);
    }
    const int par = CallNative(f.vm, "BlzFrameGetParent", {JassValue::Int(simple)}).intVal;
    TEST_ASSERT(par != 0);
    {
        const int ps = f.data->GetSimHandle(par, JassHandleInfo::FRAME);
        const int fs = f.data->GetSimHandle(frame, JassHandleInfo::FRAME);
        TEST_ASSERT(ps >= 0 && ps == fs);
    }
    CallNative(f.vm, "BlzFrameSetTooltip", {JassValue::Int(frame), JassValue::Int(simple)});
    // Global toggles + TOC + clear-points.
    CallNative(f.vm, "BlzEnableUIAutoPosition", {JassValue::Bool(false)});
    TEST_ASSERT(!f.data->uiAutoPosition);
    CallNative(f.vm, "BlzHideOriginFrames", {JassValue::Bool(true)});
    TEST_ASSERT(f.data->hideOriginFrames);
    TEST_ASSERT(CallNative(f.vm, "BlzLoadTOCFile", {JassValue::Str("UI\\Custom.toc")}).boolVal);
    CallNative(f.vm, "BlzFrameClearAllPoints", {JassValue::Int(frame)});
    CallNative(f.vm, "BlzFrameSetAllPoints", {JassValue::Int(frame), JassValue::Int(o1)});
    // Destroy invalidates; getters go safe-zero, name becomes reusable.
    CallNative(f.vm, "BlzDestroyFrame", {JassValue::Int(simple)});
    TEST_ASSERT(!CallNative(f.vm, "BlzFrameIsVisible", {JassValue::Int(simple)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "BlzFrameGetText", {JassValue::Int(simple)}).strVal.empty());
    TEST_ASSERT(CallNative(f.vm, "BlzFrameGetChildrenCount", {JassValue::Int(frame)}).intVal == 0);
    TEST_ASSERT(CallNative(f.vm, "BlzFrameGetChild", {JassValue::Int(frame), JassValue::Int(0)}).intVal == 0);
    // Bad handles are safe no-ops.
    CallNative(f.vm, "BlzFrameSetText", {JassValue::Int(999999), JassValue::Str("x")});
    TEST_ASSERT(!CallNative(f.vm, "BlzFrameIsVisible", {JassValue::Int(999999)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "BlzFrameGetParent", {JassValue::Int(999999)}).intVal == 0);
    TEST_ASSERT(!CallNative(f.vm, "BlzFrameSetFocus", {JassValue::Int(999999), JassValue::Bool(true)}).boolVal);
    return true;
}
