#pragma once

#include "../resource/slk.h"
#include "wts.h"
#include <cstdint>
#include <string>
#include <vector>

struct ObjectModification {
    enum VarType {
        INT = 0,
        REAL = 1,
        UNREAL = 2,
        STRING = 3,
        BOOLEAN = 4,
    };

    War3ID id;
    int varType = INT;
    int level = 0;
    int dataPointer = 0;
    int intValue = 0;
    float realValue = 0.0f;
    std::string stringValue;
};

struct ObjectModificationEntry {
    War3ID oldId;
    War3ID newId;
    bool custom = false;
    std::vector<ObjectModification> modifications;
};

struct ObjectModificationSet {
    int version = 0;
    std::vector<ObjectModificationEntry> original;
    std::vector<ObjectModificationEntry> custom;
};

bool LoadWar3MapUnitMods(const std::string& mapPath,
                         ObjectModificationSet& out,
                         WtsTable* wts = nullptr);

bool LoadWar3MapDestructableMods(const std::string& mapPath,
                                  ObjectModificationSet& out,
                                  WtsTable* wts = nullptr);

// Doodad object data (war3map.w3d): same non-extended encoding as w3b
// (ref Warcraft3MapObjectData doodadChangeset).
bool LoadWar3MapDoodadMods(const std::string& mapPath,
                           ObjectModificationSet& out,
                           WtsTable* wts = nullptr);

bool LoadWar3MapItemMods(const std::string& mapPath,
                          ObjectModificationSet& out,
                          WtsTable* wts = nullptr);

// Ability/upgrade/buff object data (war3map.w3a/.w3q/.w3h) use the extended
// per-modification level + data-pointer encoding (ref object data parsing).
bool LoadWar3MapAbilityMods(const std::string& mapPath,
                            ObjectModificationSet& out,
                            WtsTable* wts = nullptr);

bool LoadWar3MapUpgradeMods(const std::string& mapPath,
                            ObjectModificationSet& out,
                            WtsTable* wts = nullptr);

bool LoadWar3MapBuffMods(const std::string& mapPath,
                         ObjectModificationSet& out,
                         WtsTable* wts = nullptr);

// In-memory parse entry point (same code path as the archive loaders;
// exposed for tests and tools).
bool ParseObjectModsData(const uint8_t* data, size_t size,
                         bool extended, ObjectModificationSet& out,
                         WtsTable* wts = nullptr);

bool LoadWar3MapObjectMods(const std::string& mapPath,
                           const char* fileName,
                           bool extended,
                           ObjectModificationSet& out,
                           WtsTable* wts = nullptr);
