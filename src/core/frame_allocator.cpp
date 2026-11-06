#include "frame_allocator.h"
#include <algorithm>
#include <cassert>

FrameAllocator::FrameAllocator(size_t initialBlockSize)
    : defaultBlockSize_(initialBlockSize), currentBlockIndex_(0) {
    blocks_.push_back(std::make_unique<Block>(defaultBlockSize_));
}

FrameAllocator::~FrameAllocator() = default;

void* FrameAllocator::Allocate(size_t size, size_t alignment) {
    if (size == 0) return nullptr;

    Block* currentBlock = blocks_[currentBlockIndex_].get();

    // Calculate aligned offset
    size_t unalignedPtr = reinterpret_cast<size_t>(currentBlock->data + currentBlock->used);
    size_t alignedPtr = (unalignedPtr + alignment - 1) & ~(alignment - 1);
    size_t padding = alignedPtr - unalignedPtr;

    // Check if the current block has enough space
    if (currentBlock->used + padding + size <= currentBlock->size) {
        currentBlock->used += padding + size;
        return reinterpret_cast<void*>(alignedPtr);
    }

    // Not enough space, move to the next block or allocate a new one
    currentBlockIndex_++;

    // If we need a new block
    if (currentBlockIndex_ >= blocks_.size()) {
        size_t newBlockSize = std::max(defaultBlockSize_, size + alignment);
        blocks_.push_back(std::make_unique<Block>(newBlockSize));
    }

    currentBlock = blocks_[currentBlockIndex_].get();

    // The new block is guaranteed to be empty, just align from the start
    unalignedPtr = reinterpret_cast<size_t>(currentBlock->data);
    alignedPtr = (unalignedPtr + alignment - 1) & ~(alignment - 1);
    padding = alignedPtr - unalignedPtr;

    assert(currentBlock->used + padding + size <= currentBlock->size);

    currentBlock->used += padding + size;
    return reinterpret_cast<void*>(alignedPtr);
}

void FrameAllocator::Clear() {
    for (auto& block : blocks_) {
        block->used = 0;
    }
    currentBlockIndex_ = 0;
}

// Global instance for convenience
static std::unique_ptr<FrameAllocator> g_frameAllocator;

FrameAllocator& GetFrameAllocator() {
    if (!g_frameAllocator) {
        g_frameAllocator = std::make_unique<FrameAllocator>();
    }
    return *g_frameAllocator;
}
