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

bool LoadWar3MapItemMods(const std::string& mapPath,
                         ObjectModificationSet& out,
                         WtsTable* wts = nullptr);

bool LoadWar3MapObjectMods(const std::string& mapPath,
                           const char* fileName,
                           bool extended,
                           ObjectModificationSet& out,
                           WtsTable* wts = nullptr);
