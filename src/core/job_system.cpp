#include "core/job_system.h"

#include <algorithm>

namespace core {

void JobHandle::Wait() const {
    if (!counter_) {
        return;
    }
    while (counter_->load(std::memory_order_acquire) > 0) {
        if (owner_ && owner_->AssistOneJob()) {
            continue;
        }
        std::this_thread::yield();
    }
}

JobSystem& JobSystem::Instance() {
    static JobSystem instance;
    return instance;
}

JobSystem::JobSystem(uint32_t numThreads) {
    Initialize(numThreads);
}

JobSystem::~JobSystem() {
    Shutdown();
}

void JobSystem::Initialize(uint32_t numThreads) {
    if (running_.load(std::memory_order_acquire)) {
        return;
    }
    if (numThreads == 0) {
        const uint32_t hw = std::thread::hardware_concurrency();
        numThreads = (hw > 1u) ? std::min(8u, hw - 1u) : 2u;
    }
    running_.store(true, std::memory_order_release);
    workers_.reserve(numThreads);
    for (uint32_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this]() { WorkerLoop(); });
    }
}

void JobSystem::Shutdown() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        return;
    }
    cvWork_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) {
            w.join();
        }
    }
    workers_.clear();
}

JobHandle JobSystem::Submit(std::function<void()> task) {
    auto counter = std::make_shared<std::atomic<int>>(1);
    if (!task) {
        counter->store(0, std::memory_order_release);
        return JobHandle(this, counter);
    }

    if (!running_.load(std::memory_order_acquire) || workers_.empty()) {
        totalJobsSubmitted_.fetch_add(1, std::memory_order_relaxed);
        task();
        totalJobsCompleted_.fetch_add(1, std::memory_order_relaxed);
        counter->store(0, std::memory_order_release);
        return JobHandle(this, counter);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        inFlightJobs_.fetch_add(1, std::memory_order_relaxed);
        totalJobsSubmitted_.fetch_add(1, std::memory_order_relaxed);
        queue_.push_back(QueuedJob{std::move(task), counter});
    }
    cvWork_.notify_one();
    return JobHandle(this, counter);
}

JobHandle JobSystem::ScheduleParallelFor(
    int count,
    int minBatchSize,
    const std::function<void(int startIdx, int endIdx)>& rangeFn) {
    if (count <= 0 || !rangeFn) {
        auto done = std::make_shared<std::atomic<int>>(0);
        return JobHandle(this, done);
    }

    const int effectiveBatch = std::max(1, minBatchSize);
    const int numBatches = (count + effectiveBatch - 1) / effectiveBatch;

    parallelForDispatches_.fetch_add(1, std::memory_order_relaxed);
    parallelForBatchesDispatched_.fetch_add(static_cast<uint64_t>(numBatches), std::memory_order_relaxed);
    parallelForElementsProcessed_.fetch_add(static_cast<uint64_t>(count), std::memory_order_relaxed);

    auto counter = std::make_shared<std::atomic<int>>(numBatches);

    if (!running_.load(std::memory_order_acquire) || workers_.empty() || numBatches == 1) {
        totalJobsSubmitted_.fetch_add(static_cast<uint64_t>(numBatches), std::memory_order_relaxed);
        for (int b = 0; b < numBatches; ++b) {
            const int startIdx = b * effectiveBatch;
            const int endIdx = std::min(count, startIdx + effectiveBatch);
            rangeFn(startIdx, endIdx);
            counter->fetch_sub(1, std::memory_order_release);
        }
        totalJobsCompleted_.fetch_add(static_cast<uint64_t>(numBatches), std::memory_order_relaxed);
        return JobHandle(this, counter);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        inFlightJobs_.fetch_add(numBatches, std::memory_order_relaxed);
        totalJobsSubmitted_.fetch_add(static_cast<uint64_t>(numBatches), std::memory_order_relaxed);
        for (int b = 0; b < numBatches; ++b) {
            const int startIdx = b * effectiveBatch;
            const int endIdx = std::min(count, startIdx + effectiveBatch);
            queue_.push_back(QueuedJob{
                [rangeFn, startIdx, endIdx]() {
                    rangeFn(startIdx, endIdx);
                },
                counter
            });
        }
    }
    cvWork_.notify_all();
    return JobHandle(this, counter);
}

void JobSystem::ParallelForRange(
    int count,
    int minBatchSize,
    const std::function<void(int startIdx, int endIdx)>& rangeFn) {
    JobHandle handle = ScheduleParallelFor(count, minBatchSize, rangeFn);
    handle.Wait();
}

void JobSystem::ParallelFor(
    int count,
    int minBatchSize,
    const std::function<void(int index)>& elementFn) {
    if (count <= 0 || !elementFn) {
        return;
    }
    ParallelForRange(count, minBatchSize, [&elementFn](int startIdx, int endIdx) {
        for (int i = startIdx; i < endIdx; ++i) {
            elementFn(i);
        }
    });
}

bool JobSystem::AssistOneJob() {
    QueuedJob job;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        job = std::move(queue_.front());
        queue_.pop_front();
    }

    if (job.fn) {
        job.fn();
    }
    if (job.counter) {
        job.counter->fetch_sub(1, std::memory_order_release);
    }
    totalJobsCompleted_.fetch_add(1, std::memory_order_relaxed);
    if (inFlightJobs_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        cvDone_.notify_all();
    }
    return true;
}

void JobSystem::WaitForAll() {
    while (inFlightJobs_.load(std::memory_order_acquire) > 0) {
        if (!AssistOneJob()) {
            std::unique_lock<std::mutex> lock(mutex_);
            if (inFlightJobs_.load(std::memory_order_acquire) > 0 && queue_.empty()) {
                cvDone_.wait_for(lock, std::chrono::milliseconds(1));
            }
        }
    }
}

void JobSystem::WorkerLoop() {
    while (true) {
        QueuedJob job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cvWork_.wait(lock, [this]() {
                return !running_.load(std::memory_order_acquire) || !queue_.empty();
            });
            if (!running_.load(std::memory_order_acquire) && queue_.empty()) {
                return;
            }
            job = std::move(queue_.front());
            queue_.pop_front();
        }

        if (job.fn) {
            job.fn();
        }
        if (job.counter) {
            job.counter->fetch_sub(1, std::memory_order_release);
        }
        totalJobsCompleted_.fetch_add(1, std::memory_order_relaxed);
        if (inFlightJobs_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            cvDone_.notify_all();
        }
    }
}

JobSystemStats JobSystem::GetStats() const {
    JobSystemStats s;
    s.workerThreadCount = static_cast<uint32_t>(workers_.size());
    s.totalJobsSubmitted = totalJobsSubmitted_.load(std::memory_order_relaxed);
    s.totalJobsCompleted = totalJobsCompleted_.load(std::memory_order_relaxed);
    s.parallelForDispatches = parallelForDispatches_.load(std::memory_order_relaxed);
    s.parallelForBatchesDispatched = parallelForBatchesDispatched_.load(std::memory_order_relaxed);
    s.parallelForElementsProcessed = parallelForElementsProcessed_.load(std::memory_order_relaxed);
    return s;
}

void JobSystem::ResetStats() {
    totalJobsSubmitted_.store(0, std::memory_order_relaxed);
    totalJobsCompleted_.store(0, std::memory_order_relaxed);
    parallelForDispatches_.store(0, std::memory_order_relaxed);
    parallelForBatchesDispatched_.store(0, std::memory_order_relaxed);
    parallelForElementsProcessed_.store(0, std::memory_order_relaxed);
}

} // namespace core
