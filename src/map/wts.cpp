#include "wts.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>

bool WtsTable::Load(const uint8_t* data, size_t size) {
    std::string text((const char*)data, size);
    std::istringstream stream(text);
    std::string line;

    while (std::getline(stream, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        if (line[start] == '{' || line[start] == '}' || line[start] == '/') continue;

        if (line.substr(start, 6) == "STRING" || line.substr(start, 6) == "string") {
            int id = 0;
            char extra;
            if (sscanf(line.c_str() + start + 6, "%d%c", &id, &extra) >= 1) {
                std::string value;
                while (std::getline(stream, line)) {
                    start = line.find_first_not_of(" \t\r\n");
                    if (start == std::string::npos) continue;
                    if (line[start] == '{') continue;
                    if (line[start] == '}') break;
                    if (!value.empty()) value += "\n";
                    value += line.substr(start);
                }
                strings_[id] = value;
            }
        }
    }
    return !strings_.empty();
}

bool WtsTable::LoadFile(const std::string& path) {
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

std::string WtsTable::Resolve(const std::string& key) const {
    if (key.substr(0, 8) != "TRIGSTR_" && key.substr(0, 8) != "trigstr_")
        return key;
    int id = atoi(key.c_str() + 8);
    auto it = strings_.find(id);
    if (it != strings_.end())
        return it->second;
    return key;
}
