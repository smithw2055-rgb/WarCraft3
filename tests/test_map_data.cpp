// Map data tests: w3i players/forces/tech/random tables, units.doo drop
// sets / inventory / modified abilities / random tables (ref
// War3MapW3i.load, War3MapUnitsDoo Unit.load). Synthetic buffers, no MPQ.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "map/unitsdoo.h"
#include "map/w3d.h"
#include "map/w3i.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)

namespace {

void Push8(std::vector<uint8_t>& out, const char* tag4) {
    for (int i = 0; i < 4; i++) out.push_back((uint8_t)tag4[i]);
}

void Push32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)(v & 0xffu));
    out.push_back((uint8_t)((v >> 8) & 0xffu));
    out.push_back((uint8_t)((v >> 16) & 0xffu));
    out.push_back((uint8_t)((v >> 24) & 0xffu));
}

void PushFloat(std::vector<uint8_t>& out, float v) {
    uint32_t u;
    std::memcpy(&u, &v, 4);
    Push32(out, u);
}

void PushStr(std::vector<uint8_t>& out, const char* s) {
    while (*s) out.push_back((uint8_t)*s++);
    out.push_back(0);
}

} // namespace

bool TestW3iParsesPlayersAndForces() {
    std::vector<uint8_t> data;
    Push32(data, 25); // version
    Push32(data, 1);  // saves
    Push32(data, 6059); // editor version
    PushStr(data, "TestMap");
    PushStr(data, "Author");
    PushStr(data, "Desc");
    PushStr(data, "2v2");
    for (int i = 0; i < 8; i++) PushFloat(data, (float)i); // camera bounds
    for (int i = 0; i < 4; i++) Push32(data, 0);           // complements
    Push32(data, 128); // playable w
    Push32(data, 128); // playable h
    Push32(data, 0);   // flags
    data.push_back('A'); // tileset
    Push32(data, 0);   // campaign bg
    PushStr(data, "load.mdx"); // loading model (v>24)
    PushStr(data, "load text");
    PushStr(data, "load title");
    PushStr(data, "load sub");
    Push32(data, 1); // game data set
    PushStr(data, "pro.mdx");
    PushStr(data, "pro text");
    PushStr(data, "pro title");
    PushStr(data, "pro sub");
    Push32(data, 1); // use terrain fog
    PushFloat(data, 10.0f);
    PushFloat(data, 20.0f);
    PushFloat(data, 0.5f); // density
    data.push_back(1); data.push_back(2); data.push_back(3); data.push_back(4);
    Push32(data, 0); // global weather
    PushStr(data, "Default");
    data.push_back('A'); // light tileset
    data.push_back(5); data.push_back(6); data.push_back(7); data.push_back(8);
    // 2 players
    Push32(data, 2);
    Push32(data, 0); Push32(data, 1); Push32(data, 1); Push32(data, 1);
    PushStr(data, "Player 1");
    PushFloat(data, 100.0f); PushFloat(data, 200.0f);
    Push32(data, 0); Push32(data, 0);
    Push32(data, 1); Push32(data, 2); Push32(data, 2); Push32(data, 1);
    PushStr(data, "Player 2");
    PushFloat(data, 300.0f); PushFloat(data, 400.0f);
    Push32(data, 0); Push32(data, 0);
    // 1 force
    Push32(data, 1);
    Push32(data, 3); Push32(data, 3);
    PushStr(data, "Force 1");
    // no trailing sections

    W3iInfo info;
    TEST_ASSERT(W3ILoad(data.data(), data.size(), info));
    TEST_ASSERT(info.name == "TestMap");
    TEST_ASSERT(info.tileset == 'A');
    TEST_ASSERT(info.gameDataSet == 1);
    TEST_ASSERT(info.loadingScreenModel == "load.mdx");
    TEST_ASSERT(info.useTerrainFog == 1);
    TEST_ASSERT(info.players.size() == 2);
    TEST_ASSERT(info.players[0].name == "Player 1");
    TEST_ASSERT(info.players[0].race == 1);
    TEST_ASSERT(info.players[1].startX == 300.0f);
    TEST_ASSERT(info.forces.size() == 1);
    TEST_ASSERT(info.forces[0].name == "Force 1");
    TEST_ASSERT(info.forces[0].playerMask == 3);
    TEST_ASSERT(info.upgradeAvailability.empty());
    TEST_ASSERT(info.randomUnitTables.empty());
    return true;
}

bool TestW3iParsesTechAndRandomTables() {
    std::vector<uint8_t> data;
    Push32(data, 25);
    Push32(data, 1);
    Push32(data, 6059);
    PushStr(data, "M"); PushStr(data, "A"); PushStr(data, "D"); PushStr(data, "R");
    for (int i = 0; i < 8; i++) PushFloat(data, 0.0f);
    for (int i = 0; i < 4; i++) Push32(data, 0);
    Push32(data, 64); Push32(data, 64);
    Push32(data, 0);
    data.push_back('A');
    Push32(data, 0);
    PushStr(data, ""); PushStr(data, ""); PushStr(data, ""); PushStr(data, "");
    Push32(data, 0);
    PushStr(data, ""); PushStr(data, ""); PushStr(data, ""); PushStr(data, "");
    Push32(data, 0);
    PushFloat(data, 0.0f); PushFloat(data, 0.0f); PushFloat(data, 0.0f);
    data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(0);
    Push32(data, 0);
    PushStr(data, "");
    data.push_back('A');
    data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(0);
    Push32(data, 0); // no players
    Push32(data, 0); // no forces
    // 1 upgrade availability: players 0x3, Rhme, level 2, available 1
    Push32(data, 1);
    Push32(data, 3);
    Push8(data, "Rhme");
    Push32(data, 2);
    Push32(data, 1);
    // 1 tech availability: players 0x1, Hfoo
    Push32(data, 1);
    Push32(data, 1);
    Push8(data, "Hfoo");
    // 1 random unit table: id 7, name, 1 position, 1 row (50%, hfoo)
    Push32(data, 1);
    Push32(data, 7);
    PushStr(data, "Table1");
    Push32(data, 1);
    Push32(data, 0);
    Push32(data, 1);
    Push32(data, 50);
    Push8(data, "hfoo");
    // 1 random item table: id 'I000', name, 1 set with 1 item (25%, I001)
    Push32(data, 1);
    Push8(data, "I000");
    PushStr(data, "Items1");
    Push32(data, 1);
    Push32(data, 1);
    Push32(data, 25);
    Push8(data, "I001");

    W3iInfo info;
    TEST_ASSERT(W3ILoad(data.data(), data.size(), info));
    TEST_ASSERT(info.upgradeAvailability.size() == 1);
    TEST_ASSERT(info.upgradeAvailability[0].playerFlags == 3);
    TEST_ASSERT(info.upgradeAvailability[0].levelAffected == 2);
    TEST_ASSERT(info.techAvailability.size() == 1);
    TEST_ASSERT(info.techAvailability[0].playerFlags == 1);
    TEST_ASSERT(info.randomUnitTables.size() == 1);
    TEST_ASSERT(info.randomUnitTables[0].name == "Table1");
    TEST_ASSERT(info.randomUnitTables[0].units.size() == 1);
    TEST_ASSERT(info.randomUnitTables[0].units[0].chance == 50);
    TEST_ASSERT(info.randomItemTables.size() == 1);
    TEST_ASSERT(info.randomItemTables[0].sets.size() == 1);
    TEST_ASSERT(info.randomItemTables[0].sets[0].size() == 1);
    TEST_ASSERT(info.randomItemTables[0].sets[0][0].chance == 25);
    return true;
}

bool TestUnitsDooRetainsDropsAndInventory() {
    std::vector<uint8_t> data;
    Push8(data, "W3do");
    Push32(data, 8); // version
    Push32(data, 8); // subversion/unknown
    Push32(data, 1); // 1 unit
    Push8(data, "hfoo"); // id
    Push32(data, 3);     // variation
    PushFloat(data, 10.0f); PushFloat(data, 20.0f); PushFloat(data, 30.0f);
    PushFloat(data, 1.5f); // angle
    PushFloat(data, 1.0f); PushFloat(data, 1.0f); PushFloat(data, 1.0f);
    data.push_back(7); // flags
    Push32(data, 2);   // player
    data.push_back(0); data.push_back(0); // unknown
    Push32(data, 500); // hp
    Push32(data, 100); // mana
    Push32(data, 0);   // droppedItemTable
    Push32(data, 1);   // 1 drop set
    Push32(data, 2);   // 2 items
    Push8(data, "I001"); Push32(data, 25);
    Push8(data, "I002"); Push32(data, 75);
    Push32(data, 0);   // gold
    PushFloat(data, 600.0f); // target acquisition
    Push32(data, 5);   // hero level
    Push32(data, 10); Push32(data, 11); Push32(data, 12); // str/agi/int
    Push32(data, 1);   // 1 inventory item
    Push32(data, 2);   // slot
    Push8(data, "I003");
    Push32(data, 1);   // 1 modified ability
    Push8(data, "AHtb"); Push32(data, 1); Push32(data, 3);
    Push32(data, 2);   // randomFlag == 2
    Push32(data, 1);   // 1 random entry
    Push8(data, "hpea"); Push32(data, 100);
    Push32(data, 0);   // team color
    Push32(data, 0);   // waygate
    Push32(data, 42);  // creation number

    UnitsDooData doo;
    TEST_ASSERT(UnitsDooLoad(data.data(), data.size(), doo, nullptr));
    TEST_ASSERT(doo.units.size() == 1);
    const UnitData& u = doo.units[0];
    TEST_ASSERT(u.heroLevel == 5);
    TEST_ASSERT(u.targetAcquisition == 600.0f);
    TEST_ASSERT(u.heroStrength == 10 && u.heroIntelligence == 12);
    TEST_ASSERT(u.dropSets.size() == 1);
    TEST_ASSERT(u.dropSets[0].items.size() == 2);
    TEST_ASSERT(u.dropSets[0].items[0].chance == 25);
    TEST_ASSERT(u.dropSets[0].items[1].chance == 75);
    TEST_ASSERT(u.inventory.size() == 1);
    TEST_ASSERT(u.inventory[0].slot == 2);
    TEST_ASSERT(u.modifiedAbilities.size() == 1);
    TEST_ASSERT(u.modifiedAbilities[0].level == 3);
    TEST_ASSERT(u.modifiedAbilities[0].autocastActive == 1);
    TEST_ASSERT(u.randomFlag == 2);
    TEST_ASSERT(u.randomTable.size() == 1);
    TEST_ASSERT(u.randomTable[0].chance == 100);
    TEST_ASSERT(u.creationNumber == 42);
    return true;
}

bool TestW3dParsesTerrainDoodads() {
    // war3map.doo with one plain doodad (v8, nested drop set) followed by
    // two terrain doodads (ref War3MapDoo: 16 bytes each). Guards the old
    // bug where any terrain doodad failed the whole parse.
    std::vector<uint8_t> data;
    Push8(data, "W3do");
    Push32(data, 8); // version
    Push32(data, 0); // u1
    Push32(data, 1); // 1 doodad
    Push8(data, "BOcn"); // id
    Push32(data, 0); // variation
    PushFloat(data, 10.0f); PushFloat(data, 20.0f); PushFloat(data, 30.0f);
    PushFloat(data, 1.5f); // angle
    PushFloat(data, 1.0f); PushFloat(data, 1.0f); PushFloat(data, 1.0f);
    data.push_back(0); // flags
    data.push_back(255); // life
    Push32(data, 0); // itemTable
    Push32(data, 1); // 1 drop set
    Push32(data, 2); // 2 items in the set
    Push8(data, "I001"); Push32(data, 25);
    Push8(data, "I002"); Push32(data, 75);
    Push32(data, 7); // editorId
    Push32(data, 0); // u2
    Push32(data, 2); // 2 terrain doodads
    Push8(data, "YOtb"); Push32(data, 1); Push32(data, 11); Push32(data, 22);
    Push8(data, "YOtc"); Push32(data, 2); Push32(data, 33); Push32(data, 44);

    W3dData w3d;
    TEST_ASSERT(W3dLoad(data.data(), data.size(), w3d));
    TEST_ASSERT(w3d.doodads.size() == 1);
    TEST_ASSERT(w3d.terrainDoodads.size() == 2);
    TEST_ASSERT(w3d.terrainDoodadCount == 2);
    TEST_ASSERT(w3d.terrainDoodads[0].location[0] == 11);
    TEST_ASSERT(w3d.terrainDoodads[0].location[1] == 22);
    TEST_ASSERT(w3d.terrainDoodads[1].location[0] == 33);
    TEST_ASSERT(w3d.terrainDoodads[1].location[1] == 44);
    // ref Doodad.itemTable + RandomItemSet: drop tables must be retained,
    // not skipped (parity with unitsdoo DroppedItemSet).
    TEST_ASSERT(w3d.doodads[0].itemTable == 0);
    TEST_ASSERT(w3d.doodads[0].itemSets.size() == 1);
    TEST_ASSERT(w3d.doodads[0].itemSets[0].items.size() == 2);
    TEST_ASSERT(w3d.doodads[0].itemSets[0].items[0].chance == 25);
    TEST_ASSERT(w3d.doodads[0].itemSets[0].items[1].chance == 75);
    const uint32_t expectI001 =
        (uint32_t('I') << 24) | (uint32_t('0') << 16) | (uint32_t('0') << 8) | uint32_t('1');
    const uint32_t expectI002 =
        (uint32_t('I') << 24) | (uint32_t('0') << 16) | (uint32_t('0') << 8) | uint32_t('2');
    TEST_ASSERT(w3d.doodads[0].itemSets[0].items[0].id == expectI001);
    TEST_ASSERT(w3d.doodads[0].itemSets[0].items[1].id == expectI002);
    return true;
}
