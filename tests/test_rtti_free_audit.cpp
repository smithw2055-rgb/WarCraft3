// ============================================================================
// Dedicated Zero-RTTI Hardware Compilation Gate (`warsmash_rtti_free_tests`)
//
// Compiled strictly with `/GR-` (MSVC) and `-fno-rtti` (GCC/Clang) alongside
// all shared production `src/` + `engine/` + `projects/` sources.
// ============================================================================
#include <iostream>

bool TestStage1CoreTypeIdTagIdManifestAndEnginePurity();
bool TestStage2VfsMpqSecurityPkwareDclAndSchemaBinder();
bool TestStage3RhiBackendIncrementalOverlayAndDiscreteSampler();
bool TestStage4UiRttiFreeReactiveHudAndAudioUnification();
bool TestStage5WidgetRttiFreeOverflowSafeReaderAndEsav();
bool TestStage6MinimalRtsDualProjectReplayAndCloudMaps1000TickDeterminism();

int main() {
    struct Case {
        const char* name;
        bool (*fn)();
    };
    const Case cases[] = {
        {"[Zero-RTTI] Stage 1 Core TypeId, TagId, INI Backslash & Engine Purity",
         TestStage1CoreTypeIdTagIdManifestAndEnginePurity},
        {"[Zero-RTTI] Stage 2 Streaming MPQ Security, PKWARE DCL & SchemaBinder",
         TestStage2VfsMpqSecurityPkwareDclAndSchemaBinder},
        {"[Zero-RTTI] Stage 3 IRhiBackend, Dirty-Rect Overlay & Discrete Sampler",
         TestStage3RhiBackendIncrementalOverlayAndDiscreteSampler},
        {"[Zero-RTTI] Stage 4 UIFrame Bitmask, Live HudViewBinder & Unified AudioServer",
         TestStage4UiRttiFreeReactiveHudAndAudioUnification},
        {"[Zero-RTTI] Stage 5 WidgetKind, Overflow-Safe BinaryReader & ESAV Archive",
         TestStage5WidgetRttiFreeOverflowSafeReaderAndEsav},
        {"[Zero-RTTI] Stage 6 MinimalRTS Bounds/JSON & Dual-Project Determinism",
         TestStage6MinimalRtsDualProjectReplayAndCloudMaps1000TickDeterminism},
    };

    int failed = 0;
    for (const auto& c : cases) {
        if (c.fn()) {
            std::cout << "[PASS] " << c.name << "\n";
        } else {
            std::cerr << "[FAIL] " << c.name << "\n";
            ++failed;
        }
    }
    if (failed > 0) {
        std::cerr << failed << " Zero-RTTI test(s) failed.\n";
        return 1;
    }
    std::cout << "All 6 Zero-RTTI test suites passed.\n";
    return 0;
}
