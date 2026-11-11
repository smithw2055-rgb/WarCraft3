#pragma once
#include "engine/core/tag_id.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

struct TerrainTileInfo {
    std::string tileID;    // War3ID like "Ldrt"
    std::string dir;       // e.g. "TerrainArt\LordaeronSummer"
    std::string file;      // e.g. "Lords_Dirt"
    std::string fullPath;  // dir + "\\" + file + ".blp"
    int blightPri = 0;
    std::string convertTo;
};

// Key-value sectioned TXT file (War3 TXT profile format)
// [SectionID]\n Key=Value\n ...
class TxtTable {
public:
    bool Load(const uint8_t* data, size_t size);

    std::string GetString(const std::string& section, const std::string& key,
                          const std::string& def = "") const;
    int GetInt(const std::string& section, const std::string& key, int def = 0) const;
    float GetFloat(const std::string& section, const std::string& key, float def = 0) const;

    bool HasSection(const std::string& section) const;
    std::vector<std::string> Sections() const;
    void SetString(const std::string& section, const std::string& key,
                   const std::string& value);

    // Direct access to the key-value pairs of a section
    std::unordered_map<std::string, std::string> GetSection(const std::string& section) const;

private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sections_;
};

class SlkTable {
public:
    bool Load(const uint8_t* data, size_t size);
    bool LoadFile(const std::string& path);

    int GetInt(int row, int col, int def = 0) const;
    std::string GetString(int row, int col, const std::string& def = "") const;
    int FindRow(const std::string& colKey, int colIndex) const;
    int FindColumn(const std::string& colName) const;

    int Rows() const { return (int)data_.size(); }
    int Cols() const { return colNames_.size(); }

    std::vector<std::string> colNames_;
    std::vector<std::vector<std::string>> data_;
};

// War3ID type (4-char code as uint32_t)
struct War3ID {
    uint32_t value = 0;

    War3ID() = default;
    explicit War3ID(uint32_t v) : value(v) {}
    War3ID(const char* str); // e.g. War3ID("hfoo")
    constexpr War3ID(engine::TagId tag) noexcept : value(tag.Value()) {}
    constexpr operator engine::TagId() const noexcept { return engine::TagId(value); }

    bool operator==(War3ID o) const { return value == o.value; }
    bool operator!=(War3ID o) const { return value != o.value; }
    bool operator<(War3ID o) const { return value < o.value; }

    std::string ToString() const;

    static War3ID FromString(const std::string& s);
};

struct War3IDHash {
    size_t operator()(War3ID id) const noexcept { return std::hash<uint32_t>()(id.value); }
};

namespace std {
    template<> struct hash<War3ID> {
        size_t operator()(War3ID id) const noexcept { return hash<uint32_t>()(id.value); }
    };
}
