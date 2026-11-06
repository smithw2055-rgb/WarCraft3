#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>

class FrameAllocator {
public:
    // Initialize the allocator with an initial block size (e.g., 2MB)
    explicit FrameAllocator(size_t initialBlockSize = 2 * 1024 * 1024);
    ~FrameAllocator();

    // Prevent copying
    FrameAllocator(const FrameAllocator&) = delete;
    FrameAllocator& operator=(const FrameAllocator&) = delete;

    // Allocate memory of the given size and alignment
    void* Allocate(size_t size, size_t alignment = 16);

    // Template helper for allocating typed arrays
    template<typename T>
    T* AllocateArray(size_t count) {
        return static_cast<T*>(Allocate(sizeof(T) * count, alignof(T)));
    }

    // Reset the allocator at the end of the frame.
    // This simply resets the pointers, making all previously allocated memory invalid.
    void Clear();

private:
    struct Block {
        uint8_t* data;
        size_t size;
        size_t used;

        explicit Block(size_t sz) : size(sz), used(0) {
            data = new uint8_t[size];
        }
        ~Block() {
            delete[] data;
        }
    };

    size_t defaultBlockSize_;
    std::vector<std::unique_ptr<Block>> blocks_;
    size_t currentBlockIndex_;
};

// Global instance getter for easy access across the engine.
// In a fully multi-threaded engine, this might be thread-local.
FrameAllocator& GetFrameAllocator();
