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

// Decodes every DXT level present (base + mips) with full bounds checks;
// stops at the first truncated level. Uncompressed formats unsupported
// (false, like DecodeDDS). Never over-reads: truncated files fail clean.
bool DecodeDDSLevels(const uint8_t* data, size_t size, std::vector<DdsImage>& out);
