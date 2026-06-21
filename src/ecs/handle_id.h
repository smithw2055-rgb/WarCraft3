#pragma once
#include <cstdint>

class HandleIdAllocator {
public:
    HandleIdAllocator() = default;
    int CreateId() { return nextId_++; }
private:
    int nextId_ = 0x100000; // start from a high base to avoid conflicts
};
