#pragma once
// ============================================================================
// Phase 12 Engine Foundation: engine::FixedTimestepStepper
//
// Inspired by Godot's MainLoop physics_ticks_per_second + interpolation
// fraction and Unity's fixedDeltaTime PlayerLoop accumulator.
// Decouples deterministic fixed-frequency simulation (e.g., 20Hz RTS lockstep
// or 60Hz physics) from variable high-refresh-rate display rendering, while
// automatically computing the sub-tick interpolation factor alpha in [0, 1].
// ============================================================================
#include <algorithm>
#include <cstdint>

namespace engine {

class FixedTimestepStepper {
public:
    explicit FixedTimestepStepper(float fixedStepSec = 0.05f, int maxSubStepsPerFrame = 3)
        : fixedStepSec_(std::max(0.0001f, fixedStepSec))
        , maxSubStepsPerFrame_(std::max(1, maxSubStepsPerFrame)) {}

    void Configure(float fixedStepSec, int maxSubStepsPerFrame = 3) {
        fixedStepSec_ = std::max(0.0001f, fixedStepSec);
        maxSubStepsPerFrame_ = std::max(1, maxSubStepsPerFrame);
    }

    void Reset() {
        accumulator_ = 0.0f;
        interpolationAlpha_ = 0.0f;
        lastFrameSubSteps_ = 0;
        totalFixedTicks_ = 0;
        totalFrames_ = 0;
        spiralClampCount_ = 0;
    }

    template <typename FixedStepFn>
    int AdvanceFrame(float frameDt, FixedStepFn&& onFixedStep) {
        ++totalFrames_;
        lastFrameSubSteps_ = 0;
        if (frameDt < 0.0f) frameDt = 0.0f;

        const float maxAccum = fixedStepSec_ * static_cast<float>(maxSubStepsPerFrame_);
        const float unclamped = accumulator_ + frameDt;
        if (unclamped > maxAccum) {
            accumulator_ = maxAccum;
            ++spiralClampCount_;
        } else {
            accumulator_ = unclamped;
        }

        while (accumulator_ >= fixedStepSec_ && lastFrameSubSteps_ < maxSubStepsPerFrame_) {
            accumulator_ -= fixedStepSec_;
            ++lastFrameSubSteps_;
            ++totalFixedTicks_;
            onFixedStep(fixedStepSec_);
        }

        interpolationAlpha_ = std::clamp(accumulator_ / fixedStepSec_, 0.0f, 1.0f);
        return lastFrameSubSteps_;
    }

    float GetFixedStepSec() const { return fixedStepSec_; }
    int GetMaxSubStepsPerFrame() const { return maxSubStepsPerFrame_; }
    float GetAccumulator() const { return accumulator_; }
    float GetInterpolationAlpha() const { return interpolationAlpha_; }
    int GetLastFrameSubSteps() const { return lastFrameSubSteps_; }
    uint64_t GetTotalFixedTicks() const { return totalFixedTicks_; }
    uint64_t GetTotalFrames() const { return totalFrames_; }
    uint64_t GetSpiralClampCount() const { return spiralClampCount_; }

private:
    float fixedStepSec_ = 0.05f;
    int maxSubStepsPerFrame_ = 3;
    float accumulator_ = 0.0f;
    float interpolationAlpha_ = 0.0f;
    int lastFrameSubSteps_ = 0;
    uint64_t totalFixedTicks_ = 0;
    uint64_t totalFrames_ = 0;
    uint64_t spiralClampCount_ = 0;
};

} // namespace engine
