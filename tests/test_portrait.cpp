// P3-21 portrait binding: model-variant rule (ref
// RenderWidgetTypeData.getPortraitModel).
#include <iostream>
#include <string>

#include "core/map_path_resolver.h"

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

bool TestPortraitPathRule() {
    // "<base>_portrait.mdx", extension-agnostic; empty stays empty.
    TEST_ASSERT(PortraitPathForModel("Units\\Human\\Footman\\Footman.mdx") ==
                "Units\\Human\\Footman\\Footman_portrait.mdx");
    TEST_ASSERT(PortraitPathForModel("Units/Human/Footman/Footman.mdl") ==
                "Units/Human/Footman/Footman_portrait.mdx");
    TEST_ASSERT(PortraitPathForModel("Footman") == "Footman_portrait.mdx");
    TEST_ASSERT(PortraitPathForModel("").empty());
    // Dots in directories do not confuse the suffix strip.
    TEST_ASSERT(PortraitPathForModel("Units\\V1.2\\Foo.mdx") == "Units\\V1.2\\Foo_portrait.mdx");
    return true;
}
