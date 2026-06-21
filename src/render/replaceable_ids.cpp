#include "replaceable_ids.h"

const char* ReplaceableIds::GetPathString(int replaceableId) {
    switch (replaceableId) {
        case 1:  return "TeamColor\\TeamColor";
        case 2:  return "TeamGlow\\TeamGlow";
        case 11: return "Cliff\\Cliff0";
        case 21: return ""; // cursor models - skip
        case 31: return "LordaeronTree\\LordaeronSummerTree";
        case 32: return "AshenvaleTree\\AshenTree";
        case 33: return "BarrensTree\\BarrensTree";
        case 34: return "NorthrendTree\\NorthTree";
        case 35: return "Mushroom\\MushroomTree";
        case 36: return "RuinsTree\\RuinsTree";
        case 37: return "OutlandMushroomTree\\MushroomTree";
        default: return nullptr;
    }
}

const char* ReplaceableIds::GetIdString(int id) {
    // Returns zero-padded 2-digit string (e.g., "00", "01", ... "23")
    // Reuses a static buffer; caller should copy if needed
    static char buf[3];
    if (id < 0) id = 0;
    if (id > 23) id = 23;
    buf[0] = '0' + (id / 10);
    buf[1] = '0' + (id % 10);
    buf[2] = '\0';
    return buf;
}
