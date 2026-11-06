#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace core {

class JobSystem;

// ============================================================================
// JobHandle — Waitable handle for single tasks or ParallelFor task groups
// (Inspired by Godot 4 WorkerThreadPool::GroupID & Unity JobHandle)
// ============================================================================
class JobHandle {
public:
    JobHandle() = default;
    JobHandle(JobSystem* owner, std::shared_ptr<std::atomic<int>> counter)
        : owner_(owner), counter_(std::move(counter)) {}

    bool IsValid() const { return counter_ != nullptr; }
    bool IsCompleted() const {
        return !counter_ || counter_->load(std::memory_order_acquire) <= 0;
    }

    // Actively helps execute queued jobs on the calling thread until this
    // JobHandle's counter reaches zero (prevents main-thread starvation/deadlock).
    void Wait() const;

private:
    JobSystem* owner_ = nullptr;
    std::shared_ptr<std::atomic<int>> counter_;
};

struct JobSystemStats {
    uint32_t workerThreadCount = 0;
    uint64_t totalJobsSubmitted = 0;
    uint64_t totalJobsCompleted = 0;
    uint64_t parallelForDispatches = 0;
    uint64_t parallelForBatchesDispatched = 0;
    uint64_t parallelForElementsProcessed = 0;
};

// ============================================================================
// JobSystem — Multi-Threaded Worker Pool & Data-Parallel Dispatcher
// ============================================================================
class JobSystem {
public:
    static JobSystem& Instance();

    explicit JobSystem(uint32_t numThreads = 0);
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    void Initialize(uint32_t numThreads = 0);
    void Shutdown();

    uint32_t GetWorkerCount() const { return static_cast<uint32_t>(workers_.size()); }
    bool IsInitialized() const { return running_.load(std::memory_order_acquire); }

    // Submit an asynchronous job and return a waitable JobHandle
    JobHandle Submit(std::function<void()> task);

    // Schedule a range-based parallel loop without blocking immediately
    JobHandle ScheduleParallelFor(
        int count,
        int minBatchSize,
        const std::function<void(int startIdx, int endIdx)>& rangeFn);

    // Execute a range-based parallel loop and wait for completion
    void ParallelForRange(
        int count,
        int minBatchSize,
        const std::function<void(int startIdx, int endIdx)>& rangeFn);

    // Execute an element-wise parallel loop and wait for completion
    void ParallelFor(
        int count,
        int minBatchSize,
        const std::function<void(int index)>& elementFn);

    // Help execute one pending job from the queue on the calling thread.
    // Returns true if a job was popped and executed.
    bool AssistOneJob();

    // Block until all currently queued jobs in the system have completed.
    void WaitForAll();

    JobSystemStats GetStats() const;
    void ResetStats();

private:
    struct QueuedJob {
        std::function<void()> fn;
        std::shared_ptr<std::atomic<int>> counter;
    };

    void WorkerLoop();

    mutable std::mutex mutex_;
    std::condition_variable cvWork_;
    std::condition_variable cvDone_;
    std::deque<QueuedJob> queue_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{false};
    std::atomic<int> inFlightJobs_{0};

    std::atomic<uint64_t> totalJobsSubmitted_{0};
    std::atomic<uint64_t> totalJobsCompleted_{0};
    std::atomic<uint64_t> parallelForDispatches_{0};
    std::atomic<uint64_t> parallelForBatchesDispatched_{0};
    std::atomic<uint64_t> parallelForElementsProcessed_{0};
};

} // namespace core
