#pragma once
#include <cstdint>
#include <vector>
#include <string>

struct DdsImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels; // RGBA8
};

bool DecodeDDS(const uint8_t* data, size_t size, DdsImage& out);
