#include "slk.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

// ============================================================
// War3ID
// ============================================================
War3ID::War3ID(const char* str) : value(0) {
    for (int i = 0; i < 4 && str[i]; i++)
        value |= ((uint32_t)(unsigned char)str[i]) << ((3 - i) * 8);
}

std::string War3ID::ToString() const {
    char s[5] = {
        (char)(value & 0xFF),
        (char)((value >> 8) & 0xFF),
        (char)((value >> 16) & 0xFF),
        (char)((value >> 24) & 0xFF),
        0
    };
    return std::string(s);
}

War3ID War3ID::FromString(const std::string& s) {
    War3ID id;
    for (int i = 0; i < 4 && i < (int)s.size(); i++)
        id.value |= ((uint32_t)(unsigned char)s[i]) << ((3 - i) * 8);
    return id;
}

// ============================================================
// TxtTable - War3 TXT profile format
// ============================================================
bool TxtTable::Load(const uint8_t* data, size_t size) {
    std::string text((const char*)data, size);
    std::istringstream stream(text);
    std::string line;
    std::string currentSection;

    while (std::getline(stream, line)) {
        // Strip \r
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Skip empty and comment lines
        if (line.empty() || line[0] == '/' || line[0] == '#') continue;

        // Section header: [SectionName]
        if (line[0] == '[') {
            auto end = line.find(']');
            if (end != std::string::npos) {
                currentSection = line.substr(1, end - 1);
            }
            continue;
        }

        if (currentSection.empty()) continue;

        // Key=Value
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        // Trim whitespace
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\r"));
            s.erase(s.find_last_not_of(" \t\r") + 1);
        };
        trim(key);
        trim(value);

        if (key.empty()) continue;

        sections_[currentSection][key] = value;
    }

    return !sections_.empty();
}

std::string TxtTable::GetString(const std::string& section, const std::string& key,
                                 const std::string& def) const {
    auto sit = sections_.find(section);
    if (sit == sections_.end()) return def;
    auto kit = sit->second.find(key);
    if (kit == sit->second.end()) return def;
    return kit->second;
}

int TxtTable::GetInt(const std::string& section, const std::string& key, int def) const {
    std::string s = GetString(section, key);
    if (s.empty()) return def;
    return std::atoi(s.c_str());
}

float TxtTable::GetFloat(const std::string& section, const std::string& key, float def) const {
    std::string s = GetString(section, key);
    if (s.empty()) return def;
    return (float)std::atof(s.c_str());
}

bool TxtTable::HasSection(const std::string& section) const {
    return sections_.find(section) != sections_.end();
}

std::vector<std::string> TxtTable::Sections() const {
    std::vector<std::string> result;
    for (auto& p : sections_) result.push_back(p.first);
    return result;
}

std::unordered_map<std::string, std::string> TxtTable::GetSection(const std::string& section) const {
    auto it = sections_.find(section);
    if (it != sections_.end()) return it->second;
    return {};
}

// ============================================================
// SlkTable
// ============================================================
bool SlkTable::Load(const uint8_t* data, size_t size) {
    std::string text((const char*)data, size);
    std::istringstream stream(text);
    std::string line;

    int curY = 0;  // persistent Y across C lines in same row batch

    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        if (line[0] == 'I' || line[0] == 'P' || line[0] == 'F' ||
            line[0] == 'B' || line[0] == 'O') continue;
        if (line[0] == 'E') break;

        if (line[0] == 'C') {
            int y = 0, x = 0;
            std::string value;
            char valCmd = 0;

            size_t pos = 1;
            while (pos < line.size()) {
                char cmd = line[pos++];
                if (cmd == ';') continue;

                if (cmd == 'Y') {
                    y = atoi(&line[pos]);
                    while (pos < line.size() && line[pos] != ';') pos++;
                }
                else if (cmd == 'X') {
                    x = atoi(&line[pos]);
                    while (pos < line.size() && line[pos] != ';') pos++;
                }
                else if (cmd == 'K' || cmd == 'A') {
                    valCmd = cmd;
                    value = std::string(line.c_str() + pos);
                    // Strip trailing \r or \n
                    while (!value.empty() && (value.back() == '\r' || value.back() == '\n'))
                        value.pop_back();
                    // Strip surrounding quotes
                    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                        value = value.substr(1, value.size() - 2);
                    break;
                }
                else {
                    while (pos < line.size() && line[pos] != ';') pos++;
                }
            }

            if (y > 0) curY = y;
            else y = curY;
            if (y < 1 || x < 1 || value.empty()) continue;

            // Row 1 is column names (only K values are names)
            if (y == 1 && valCmd == 'K') {
                while ((int)colNames_.size() <= x) colNames_.push_back("");
                colNames_[x - 1] = value;
            }
            else if (y == 1) {
                // A-value descriptions in row 1: skip
                continue;
            }
            else {
                int dataRow = y - 2;
                while ((int)data_.size() <= dataRow) data_.push_back({});
                while ((int)data_[dataRow].size() <= x) data_[dataRow].push_back("");
                data_[dataRow][x - 1] = value;
            }
        }
    }

    // Remove trailing empty column names (from A-only description columns)
    while (!colNames_.empty() && colNames_.back().empty()) colNames_.pop_back();

    std::cout << "SLK: loaded " << data_.size() << " rows, " << colNames_.size() << " cols" << std::endl;
    return !data_.empty();
}

bool SlkTable::LoadFile(const std::string& path) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { fclose(fp); return false; }
    std::vector<uint8_t> buf(sz);
    if (fread(buf.data(), 1, sz, fp) != (size_t)sz) { fclose(fp); return false; }
    fclose(fp);
    return Load(buf.data(), sz);
}

int SlkTable::GetInt(int row, int col, int def) const {
    if (row < 0 || row >= (int)data_.size()) return def;
    if (col < 0 || col >= (int)data_[row].size()) return def;
    return atoi(data_[row][col].c_str());
}

std::string SlkTable::GetString(int row, int col, const std::string& def) const {
    if (row < 0 || row >= (int)data_.size()) return def;
    if (col < 0 || col >= (int)data_[row].size()) return def;
    return data_[row][col];
}

int SlkTable::FindRow(const std::string& colKey, int colIndex) const {
    for (int i = 0; i < (int)data_.size(); ++i) {
        if (colIndex < (int)data_[i].size() && data_[i][colIndex] == colKey)
            return i;
    }
    return -1;
}

int SlkTable::FindColumn(const std::string& colName) const {
    for (int i = 0; i < (int)colNames_.size(); i++) {
        if (colNames_[i] == colName) return i;
    }
    return -1;
}
