#pragma once
#include <cstdint>

class HandleIdAllocator {
public:
    HandleIdAllocator() = default;
    int CreateId() { return nextId_++; }
    int PeekNextId() const noexcept { return nextId_; }
    void SetNextId(int nextId) noexcept {
        nextId_ = (nextId > 0x100000) ? nextId : 0x100000;
    }
    void AdvanceAtLeast(int minNextId) noexcept {
        if (minNextId > nextId_) {
            nextId_ = minNextId;
        }
    }
private:
    int nextId_ = 0x100000; // start from a high base to avoid conflicts
};
