#include "unitsdoo.h"
#include <cstring>
#include <iostream>
#include "resource/mpq_storm.h"

static uint32_t R32LE(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
static float RF32(const uint8_t* p) { float v; memcpy(&v, p, 4); return v; }
static uint32_t TagBE(const uint8_t* p) {
    return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];
}

#define CHK(n) do { if (off + (n) > size) return false; } while(0)

bool UnitsDooLoad(const uint8_t* data, size_t size, UnitsDooData& out, const W3iInfo* mapInfo) {
    size_t off = 0;
    CHK(4);
    const bool hasSkinId = mapInfo && (((mapInfo->gameVersionMajor * 100) + mapInfo->gameVersionMinor) >= 132);

    uint32_t magic = TagBE(data + off); off += 4;
    if (magic != TagBE((const uint8_t*)"W3do")) return false;

    CHK(8);
    out.version = (int)R32LE(data + off); off += 4;
    off += 4; // unknown

    int unitCount = (int)R32LE(data + off); off += 4;
    if (unitCount < 0 || unitCount > 50000) return false;
    out.units.resize(unitCount);

    for (int i = 0; i < unitCount; i++) {
        auto& u = out.units[i];

        CHK(36);
        u.id = TagBE(data + off); off += 4;
        u.variation = (int)R32LE(data + off); off += 4;
        u.pos[0] = RF32(data + off); off += 4;
        u.pos[1] = RF32(data + off); off += 4;
        u.pos[2] = RF32(data + off); off += 4;
        u.angle = RF32(data + off); off += 4;
        u.scale[0] = RF32(data + off); off += 4;
        u.scale[1] = RF32(data + off); off += 4;
        u.scale[2] = RF32(data + off); off += 4;
        if (hasSkinId) {
            CHK(4);
            u.skinId = TagBE(data + off); off += 4;
        } else {
            u.skinId = u.id;
        }

        // flags(1) + player(4) + unknown(2) + hp(4) + mana(4)
        CHK(15);
        u.flags = data[off++];
        u.player = (int)R32LE(data + off); off += 4;
        off += 2;
        u.hitpoints = (int)R32LE(data + off); off += 4;
        u.mana = (int)R32LE(data + off); off += 4;

        if (out.version > 7) { CHK(4); u.droppedItemTable = (int)R32LE(data + off); off += 4; }

        CHK(4);
        int dropSetCount = (int)R32LE(data + off); off += 4;
        if (dropSetCount < 0 || dropSetCount > 10000) return false;
        u.dropSets.resize((size_t)dropSetCount);
        for (int ds = 0; ds < dropSetCount; ++ds) {
            CHK(4);
            int itemCount = (int)R32LE(data + off); off += 4;
            if (itemCount < 0 || itemCount > 10000) return false;
            auto& set = u.dropSets[(size_t)ds].items;
            set.resize((size_t)itemCount);
            for (int k = 0; k < itemCount; ++k) {
                CHK(8);
                set[(size_t)k].id = TagBE(data + off); off += 4;
                set[(size_t)k].chance = (int)R32LE(data + off); off += 4;
            }
        }

        CHK(8);
        u.goldAmount = (int)R32LE(data + off); off += 4;
        u.targetAcquisition = RF32(data + off); off += 4;
        CHK(4);
        u.heroLevel = (int)R32LE(data + off); off += 4;
        if (out.version > 7) {
            CHK(12);
            u.heroStrength = (int)R32LE(data + off); off += 4;
            u.heroAgility = (int)R32LE(data + off); off += 4;
            u.heroIntelligence = (int)R32LE(data + off); off += 4;
        }

        CHK(4);
        int invCount = (int)R32LE(data + off); off += 4;
        if (invCount < 0 || invCount > 10000) return false;
        u.inventory.resize((size_t)invCount);
        for (int k = 0; k < invCount; ++k) {
            CHK(8);
            u.inventory[(size_t)k].slot = (int)R32LE(data + off); off += 4;
            u.inventory[(size_t)k].id = TagBE(data + off); off += 4;
        }

        CHK(4);
        int abilityCount = (int)R32LE(data + off); off += 4;
        if (abilityCount < 0 || abilityCount > 10000) return false;
        u.modifiedAbilities.resize((size_t)abilityCount);
        for (int k = 0; k < abilityCount; ++k) {
            CHK(12);
            u.modifiedAbilities[(size_t)k].id = TagBE(data + off); off += 4;
            u.modifiedAbilities[(size_t)k].autocastActive = (int)R32LE(data + off); off += 4;
            u.modifiedAbilities[(size_t)k].level = (int)R32LE(data + off); off += 4;
        }

        CHK(4);
        int randomFlag = (int)R32LE(data + off); off += 4;
        u.randomFlag = randomFlag;
        if (randomFlag == 0) {
            CHK(4);
            u.randomLevel[0] = data[off++];
            u.randomLevel[1] = data[off++];
            u.randomLevel[2] = data[off++];
            u.randomItemClass = data[off++];
        }
        else if (randomFlag == 1) { CHK(8); u.randomGroup = R32LE(data + off); off += 4; u.randomPosition = R32LE(data + off); off += 4; }
        else if (randomFlag == 2) {
            CHK(4);
            int randCount = (int)R32LE(data + off); off += 4;
            if (randCount < 0 || randCount > 10000) return false;
            u.randomTable.resize((size_t)randCount);
            for (int k = 0; k < randCount; ++k) {
                CHK(8);
                u.randomTable[(size_t)k].id = TagBE(data + off); off += 4;
                u.randomTable[(size_t)k].chance = (int)R32LE(data + off); off += 4;
            }
        }

        CHK(12);
        u.customTeamColor = (int)R32LE(data + off); off += 4;
        u.waygate = (int)R32LE(data + off); off += 4;
        u.creationNumber = (int)R32LE(data + off); off += 4;
    }
    return true;
}

#undef CHK

bool UnitsDooLoadFromMpq(const std::string& mapPath, UnitsDooData& out, const W3iInfo* mapInfo) {
    std::vector<uint8_t> buf;
    if (!MpqManager::ReadSingleArchiveFile(mapPath, "war3mapUnits.doo", buf)) return false;
    return UnitsDooLoad(buf.data(), buf.size(), out, mapInfo);
}
