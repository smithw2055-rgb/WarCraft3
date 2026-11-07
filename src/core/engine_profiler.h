#pragma once
// ============================================================================
// Stage U: Hierarchical EngineProfiler, Rolling P50/P95/P99 Frame Telemetry
//          & Chrome Trace / Perfetto JSON Exporter
//          (Inspired by Unity ProfilerMarker & Godot 4 Performance/Monitors)
// ============================================================================
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace core {

struct ProfileZoneSummary {
    std::string name;
    uint64_t callCount = 0;
    double totalMicros = 0.0;
    double meanMicros = 0.0;
    double minMicros = 0.0;
    double maxMicros = 0.0;
    double p50Micros = 0.0;
    double p95Micros = 0.0;
    double p99Micros = 0.0;
};

struct TraceEventRecord {
    std::string name;
    std::string category;
    uint64_t startMicros = 0;
    uint64_t durationMicros = 0;
    uint32_t threadId = 1;
    int depth = 0;
};

class EngineProfiler {
public:
    static constexpr size_t kMaxRollingSamplesPerZone = 256;
    static constexpr size_t kMaxTraceEvents = 8192;

    EngineProfiler();

    static EngineProfiler& Instance();

    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool IsEnabled() const { return enabled_; }

    void Reset();

    void BeginFrame(uint64_t frameNumber);
    void EndFrame();

    uint64_t NowMicros() const;

    void RecordZoneSample(const std::string& name,
                          const std::string& category,
                          uint64_t startMicros,
                          uint64_t durationMicros,
                          uint32_t threadId = 1,
                          int depth = 0);

    void SetCounter(const std::string& name, int64_t value);
    void AddCounter(const std::string& name, int64_t delta);
    int64_t GetCounter(const std::string& name) const;

    bool GetZoneSummary(const std::string& name, ProfileZoneSummary& outSummary) const;
    std::vector<ProfileZoneSummary> GetAllZoneSummaries() const;

    uint64_t GetCompletedFrameCount() const { return completedFrames_; }
    size_t GetRecordedTraceEventCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return traceEvents_.size();
    }

    // Export recorded trace events in Chrome Tracing / Perfetto JSON format
    std::string ExportChromeTraceJson(size_t maxEvents = 2048) const;

private:
    struct ZoneBucket {
        std::string name;
        uint64_t callCount = 0;
        double totalMicros = 0.0;
        double minMicros = 0.0;
        double maxMicros = 0.0;
        std::vector<double> rollingSamples;
    };

    static ProfileZoneSummary BuildSummaryFromBucket(const ZoneBucket& b);

    bool enabled_ = true;
    std::chrono::steady_clock::time_point epoch_;
    uint64_t currentFrame_ = 0;
    uint64_t currentFrameStartMicros_ = 0;
    uint64_t completedFrames_ = 0;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ZoneBucket> zones_;
    std::unordered_map<std::string, int64_t> counters_;
    std::vector<TraceEventRecord> traceEvents_;
};

class ScopedProfileZone {
public:
    ScopedProfileZone(EngineProfiler& profiler,
                      const char* name,
                      const char* category = "Engine",
                      uint32_t threadId = 1,
                      int depth = 0)
        : profiler_(profiler),
          name_(name ? name : "Zone"),
          category_(category ? category : "Engine"),
          threadId_(threadId),
          depth_(depth),
          startMicros_(profiler.IsEnabled() ? profiler.NowMicros() : 0) {}

    ~ScopedProfileZone() {
        if (profiler_.IsEnabled()) {
            const uint64_t endMicros = profiler_.NowMicros();
            const uint64_t dur = (endMicros >= startMicros_) ? (endMicros - startMicros_) : 0;
            profiler_.RecordZoneSample(name_, category_, startMicros_, dur, threadId_, depth_);
        }
    }

    ScopedProfileZone(const ScopedProfileZone&) = delete;
    ScopedProfileZone& operator=(const ScopedProfileZone&) = delete;

private:
    EngineProfiler& profiler_;
    const char* name_;
    const char* category_;
    uint32_t threadId_;
    int depth_;
    uint64_t startMicros_;
};

} // namespace core

#define WARSMASH_PROFILE_SCOPE(profilerRef, zoneName, categoryName) \
    ::core::ScopedProfileZone _warsmash_prof_zone_##__LINE__((profilerRef), (zoneName), (categoryName))
