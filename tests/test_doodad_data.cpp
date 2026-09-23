// CDoodadData tests: retail DestructableData.slk doodad view (doodad
// stats live in the same table; there is no separate DoodadData.slk).
#include <filesystem>
#include <iostream>

#include "data/doodaddata.h"
#include "ecs/csimulation.h"
#include "resource/mpq_storm.h"

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace {

namespace fs = std::filesystem;

bool OpenGameMpqs(MpqManager& mpq) {
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    bool opened = false;
    for (auto a : {"War3x.mpq", "war3x.mpq", "War3Patch.mpq", "war3.mpq"}) {
        if (mpq.Open(res + a)) opened = true;
    }
    return opened;
}

} // namespace

bool TestDoodadTableLoads() {
    MpqManager mpq;
    if (!OpenGameMpqs(mpq)) return true;
    CDoodadData data = CDoodadData::LoadFromMpq(mpq);
    // Same 247-row retail table the destructable loader reads.
    TEST_ASSERT(data.Count() > 200);
    // Summer tree: model path resolves, not walkable.
    TEST_ASSERT(data.HasDoodad(War3ID("ATtr")));
    const CDoodadType* tree = data.GetDoodad(War3ID("ATtr"));
    TEST_ASSERT(tree != nullptr);
    TEST_ASSERT(tree->file.find("AshenTree") != std::string::npos);
    TEST_ASSERT(!tree->walkable);
    // Tree bridge doodads are walkable.
    const CDoodadType* bridge = data.GetDoodad(War3ID("ATt0"));
    TEST_ASSERT(bridge != nullptr);
    TEST_ASSERT(bridge->walkable);
    // Unknown ids miss cleanly.
    TEST_ASSERT(!data.HasDoodad(War3ID("ZZZ9")));
    TEST_ASSERT(data.GetDoodad(War3ID("ZZZ9")) == nullptr);
    return true;
}

bool TestDoodadSimWiring() {
    CSimulation sim;
    TEST_ASSERT(sim.GetDoodadData() == nullptr);
    CDoodadData data;
    sim.SetDoodadData(&data);
    TEST_ASSERT(sim.GetDoodadData() == &data);
    return true;
}
