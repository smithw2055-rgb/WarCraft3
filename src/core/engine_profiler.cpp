#include "core/engine_profiler.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace core {

EngineProfiler::EngineProfiler()
    : epoch_(std::chrono::steady_clock::now()) {}

EngineProfiler& EngineProfiler::Instance() {
    static EngineProfiler s_instance;
    return s_instance;
}

void EngineProfiler::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    epoch_ = std::chrono::steady_clock::now();
    currentFrame_ = 0;
    currentFrameStartMicros_ = 0;
    completedFrames_ = 0;
    zones_.clear();
    counters_.clear();
    traceEvents_.clear();
}

uint64_t EngineProfiler::NowMicros() const {
    const auto now = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now - epoch_).count());
}

void EngineProfiler::BeginFrame(uint64_t frameNumber) {
    if (!enabled_) return;
    currentFrame_ = frameNumber;
    currentFrameStartMicros_ = NowMicros();
}

void EngineProfiler::EndFrame() {
    if (!enabled_) return;
    const uint64_t endMicros = NowMicros();
    const uint64_t dur = (endMicros >= currentFrameStartMicros_)
        ? (endMicros - currentFrameStartMicros_)
        : 0;
    RecordZoneSample("FrameTotal", "Frame", currentFrameStartMicros_, dur, 1, 0);
    completedFrames_++;
}

void EngineProfiler::RecordZoneSample(const std::string& name,
                                      const std::string& category,
                                      uint64_t startMicros,
                                      uint64_t durationMicros,
                                      uint32_t threadId,
                                      int depth) {
    if (!enabled_ || name.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);

    const double dur = static_cast<double>(durationMicros);
    ZoneBucket& b = zones_[name];
    if (b.callCount == 0) {
        b.name = name;
        b.minMicros = dur;
        b.maxMicros = dur;
    } else {
        b.minMicros = std::min(b.minMicros, dur);
        b.maxMicros = std::max(b.maxMicros, dur);
    }
    b.callCount++;
    b.totalMicros += dur;
    if (b.rollingSamples.size() >= kMaxRollingSamplesPerZone) {
        b.rollingSamples.erase(b.rollingSamples.begin());
    }
    b.rollingSamples.push_back(dur);

    if (traceEvents_.size() < kMaxTraceEvents) {
        TraceEventRecord rec;
        rec.name = name;
        rec.category = category;
        rec.startMicros = startMicros;
        rec.durationMicros = durationMicros;
        rec.threadId = threadId;
        rec.depth = depth;
        traceEvents_.push_back(std::move(rec));
    }
}

void EngineProfiler::SetCounter(const std::string& name, int64_t value) {
    std::lock_guard<std::mutex> lock(mutex_);
    counters_[name] = value;
}

void EngineProfiler::AddCounter(const std::string& name, int64_t delta) {
    std::lock_guard<std::mutex> lock(mutex_);
    counters_[name] += delta;
}

int64_t EngineProfiler::GetCounter(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = counters_.find(name);
    return (it != counters_.end()) ? it->second : 0;
}

ProfileZoneSummary EngineProfiler::BuildSummaryFromBucket(const ZoneBucket& b) {
    ProfileZoneSummary s;
    s.name = b.name;
    s.callCount = b.callCount;
    s.totalMicros = b.totalMicros;
    s.meanMicros = (b.callCount > 0) ? (b.totalMicros / static_cast<double>(b.callCount)) : 0.0;
    s.minMicros = b.minMicros;
    s.maxMicros = b.maxMicros;

    if (!b.rollingSamples.empty()) {
        std::vector<double> sorted = b.rollingSamples;
        std::sort(sorted.begin(), sorted.end());
        auto pickPercentile = [&](double p) -> double {
            const double rawIdx = p * static_cast<double>(sorted.size() - 1);
            const size_t idx = static_cast<size_t>(std::clamp(std::round(rawIdx), 0.0, static_cast<double>(sorted.size() - 1)));
            return sorted[idx];
        };
        s.p50Micros = pickPercentile(0.50);
        s.p95Micros = pickPercentile(0.95);
        s.p99Micros = pickPercentile(0.99);
    }
    return s;
}

bool EngineProfiler::GetZoneSummary(const std::string& name, ProfileZoneSummary& outSummary) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = zones_.find(name);
    if (it == zones_.end()) return false;
    outSummary = BuildSummaryFromBucket(it->second);
    return true;
}

std::vector<ProfileZoneSummary> EngineProfiler::GetAllZoneSummaries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ProfileZoneSummary> out;
    out.reserve(zones_.size());
    for (const auto& [name, bucket] : zones_) {
        (void)name;
        out.push_back(BuildSummaryFromBucket(bucket));
    }
    std::sort(out.begin(), out.end(), [](const ProfileZoneSummary& a, const ProfileZoneSummary& b) {
        return a.totalMicros > b.totalMicros;
    });
    return out;
}

std::string EngineProfiler::ExportChromeTraceJson(size_t maxEvents) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "{\"traceEvents\":[";
    const size_t count = std::min(maxEvents, traceEvents_.size());
    for (size_t i = 0; i < count; ++i) {
        const TraceEventRecord& ev = traceEvents_[i];
        if (i > 0) oss << ",";
        oss << "{\"name\":\"" << ev.name << "\""
            << ",\"cat\":\"" << ev.category << "\""
            << ",\"ph\":\"X\""
            << ",\"ts\":" << ev.startMicros
            << ",\"dur\":" << ev.durationMicros
            << ",\"pid\":1"
            << ",\"tid\":" << ev.threadId << "}";
    }
    oss << "]}";
    return oss.str();
}

} // namespace core
