#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class WtsTable {
public:
    bool Load(const uint8_t* data, size_t size);
    bool LoadFile(const std::string& path);

    std::string Resolve(const std::string& key) const;
    int Size() const { return (int)strings_.size(); }

private:
    std::unordered_map<int, std::string> strings_;
};
