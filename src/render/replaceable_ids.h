#pragma once
#include <string>

struct ReplaceableIds {
    static const char* GetPathString(int replaceableId);
    static const char* GetIdString(int id);
};
