#include "mdx_animator.h"
#include "render/rhi_device.h"
#include <iostream>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_set>

#define MAKE_TAG(A,B,C,D) ((A<<24)|(B<<16)|(C<<8)|D)
static const uint32_t TAG_KGTR = MAKE_TAG('K','G','T','R');
static const uint32_t TAG_KGRT = MAKE_TAG('K','G','R','T');
static const uint32_t TAG_KGSC = MAKE_TAG('K','G','S','C');
static const uint32_t TAG_KGAO = MAKE_TAG('K','G','A','O');
static const uint32_t TAG_KLAV = MAKE_TAG('K','L','A','V');
static const uint32_t TAG_KATV = MAKE_TAG('K','A','T','V');
static const uint32_t TAG_KPEV = MAKE_TAG('K','P','E','V');
static const uint32_t TAG_KP2V = MAKE_TAG('K','P','2','V');
static const uint32_t TAG_KRVS = MAKE_TAG('K','R','V','S');
static const uint32_t TAG_KEVT = MAKE_TAG('K','E','V','T');
static const uint32_t NODE_DONT_INHERIT_TRANSLATION = 0x1;
static const uint32_t NODE_DONT_INHERIT_SCALING = 0x2;
static const uint32_t NODE_DONT_INHERIT_ROTATION = 0x4;
static const uint32_t NODE_BILLBOARDED = 0x8;
static const uint32_t NODE_BILLBOARDED_LOCK_X = 0x10;
static const uint32_t NODE_BILLBOARDED_LOCK_Y = 0x20;
static const uint32_t NODE_BILLBOARDED_LOCK_Z = 0x40;
static constexpr bool kMdxAnimatorVerboseLog = false;

template <typename... Args>
static void MdxAnimatorLog(const char* format, Args... args) {
    if constexpr (kMdxAnimatorVerboseLog) {
        std::fprintf(stderr, format, args...);
    }
}

static void MdxAnimatorFlush() {
    if constexpr (kMdxAnimatorVerboseLog) {
        std::fflush(stderr);
    }
}

static bool IsFiniteFloat(float value) {
    return std::isfinite(value);
}

static bool IsFiniteVec3(const Vec3& value) {
    return IsFiniteFloat(value.x) && IsFiniteFloat(value.y) && IsFiniteFloat(value.z);
}

static bool IsFiniteVec4(const Vec4& value) {
    return IsFiniteFloat(value.x) && IsFiniteFloat(value.y)
        && IsFiniteFloat(value.z) && IsFiniteFloat(value.w);
}

static bool IsFiniteMat4(const Mat4& value) {
    for (float component : value.m) {
        if (!IsFiniteFloat(component)) {
            return false;
        }
    }
    return true;
}

static Mat4 RotateY(float radians) {
    Mat4 r = Mat4::Identity();
    float c = std::cos(radians);
    float s = std::sin(radians);
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

static Mat4 RotateX(float radians) {
    Mat4 r = Mat4::Identity();
    float c = std::cos(radians);
    float s = std::sin(radians);
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

static Mat4 RotateZ(float radians) {
    Mat4 r = Mat4::Identity();
    float c = std::cos(radians);
    float s = std::sin(radians);
    r.m[0] = c;
    r.m[1] = s;
    r.m[4] = -s;
    r.m[5] = c;
    return r;
}

static Vec4 NormalizeQuat(const Vec4& q) {
    if (!IsFiniteVec4(q)) {
        return {0,0,0,1};
    }
    float len = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    if (!std::isfinite(len) || len < 1e-7f) return {0,0,0,1};
    return {q.x / len, q.y / len, q.z / len, q.w / len};
}

static Vec4 SlerpQuat(const Vec4& q1, const Vec4& q2In, float t) {
    Vec4 q2 = q2In;
    float dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;
    if (dot < 0.0f) {
        dot = -dot;
        q2 = {-q2.x, -q2.y, -q2.z, -q2.w};
    }
    if (dot > 0.9995f) {
        return NormalizeQuat({
            q1.x + (q2.x - q1.x) * t,
            q1.y + (q2.y - q1.y) * t,
            q1.z + (q2.z - q1.z) * t,
            q1.w + (q2.w - q1.w) * t
        });
    }
    float theta_0 = std::acos(std::clamp(dot, -1.0f, 1.0f));
    float theta = theta_0 * t;
    float sin_theta = std::sin(theta);
    float sin_theta_0 = std::sin(theta_0);
    float s0 = std::cos(theta) - dot * sin_theta / sin_theta_0;
    float s1 = sin_theta / sin_theta_0;
    return NormalizeQuat({
        q1.x * s0 + q2.x * s1,
        q1.y * s0 + q2.y * s1,
        q1.z * s0 + q2.z * s1,
        q1.w * s0 + q2.w * s1
    });
}

static Vec4 MulQuat(const Vec4& a, const Vec4& b) {
    return NormalizeQuat({
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
    });
}

static Vec4 InverseQuat(const Vec4& q) {
    Vec4 n = NormalizeQuat(q);
    return {-n.x, -n.y, -n.z, n.w};
}

static const MdxTimeline* findTimeline(const std::vector<MdxTimeline>& timelines, uint32_t tag) {
    for (const auto& tl : timelines)
        if (tl.tag == tag) return &tl;
    return nullptr;
}

static bool timelineHasKeyValues(const MdxTimeline& timeline, int key, int elemCount) {
    if (key < 0 || elemCount < 1) {
        return false;
    }
    return timeline.values.size() >= ((size_t)key + 1u) * (size_t)elemCount;
}

static float timelineValueOrDefault(const MdxTimeline& timeline, int key, int elem, int elemCount,
                                    float defaultValue) {
    if (key < 0 || elem < 0 || elem >= elemCount || elemCount < 1) {
        return defaultValue;
    }
    const size_t index = (size_t)key * (size_t)elemCount + (size_t)elem;
    return index < timeline.values.size() ? timeline.values[index] : defaultValue;
}

static bool timelineHasTangents(const MdxTimeline& timeline, int keyA, int keyB, int elemCount) {
    if (keyA < 0 || keyB < 0 || elemCount < 1) {
        return false;
    }
    const size_t needed = std::max(((size_t)keyA + 1u) * (size_t)elemCount,
                                   ((size_t)keyB + 1u) * (size_t)elemCount);
    return timeline.inTans.size() >= needed && timeline.outTans.size() >= needed;
}

static float interpolateTimelineValue(const MdxTimeline& timeline, int elem, int elemCount,
                                      int keyA, int keyB, float t) {
    const float a = timelineValueOrDefault(timeline, keyA, elem, elemCount, 0.0f);
    const float b = timelineValueOrDefault(timeline, keyB, elem, elemCount, a);
    switch (timeline.interpolationType) {
    case MdxInterpolation::None:
        return a;
    case MdxInterpolation::Linear:
        return a + (b - a) * t;
    case MdxInterpolation::Hermite: {
        if (!timelineHasTangents(timeline, keyA, keyB, elemCount)) {
            return a + (b - a) * t;
        }
        const float outTan = timeline.outTans[(size_t)keyA * elemCount + elem];
        const float inTan = timeline.inTans[(size_t)keyB * elemCount + elem];
        const float t2 = t * t;
        const float t3 = t2 * t;
        return (2.0f * t3 - 3.0f * t2 + 1.0f) * a
            + (-2.0f * t3 + 3.0f * t2) * b
            + (t3 - 2.0f * t2 + t) * outTan
            + (t3 - t2) * inTan;
    }
    case MdxInterpolation::Bezier: {
        if (!timelineHasTangents(timeline, keyA, keyB, elemCount)) {
            return a + (b - a) * t;
        }
        const float outTan = timeline.outTans[(size_t)keyA * elemCount + elem];
        const float inTan = timeline.inTans[(size_t)keyB * elemCount + elem];
        const float mt = 1.0f - t;
        return mt * mt * mt * a
            + 3.0f * mt * mt * t * outTan
            + 3.0f * mt * t * t * inTan
            + t * t * t * b;
    }
    default:
        return a;
    }
}

static bool sampleGlobalTimelineLooped(const MdxTimeline& timeline, uint32_t frame,
                                       uint32_t duration, float* out, int elemCount) {
    if (!out || timeline.frames.empty() || elemCount < 1 || duration == 0) {
        return false;
    }
    if (timeline.frames.size() == 1) {
        if (!timelineHasKeyValues(timeline, 0, elemCount)) {
            return false;
        }
        for (int elem = 0; elem < elemCount; ++elem) {
            out[elem] = timelineValueOrDefault(timeline, 0, elem, elemCount, 0.0f);
        }
        return true;
    }
    if (!timelineHasKeyValues(timeline, (int)timeline.frames.size() - 1, elemCount)) {
        return false;
    }

    const int lastIndex = (int)timeline.frames.size() - 1;
    int keyA = lastIndex;
    int keyB = 0;
    if (frame >= timeline.frames[0] && frame < timeline.frames[(size_t)lastIndex]) {
        for (int i = 1; i <= lastIndex; ++i) {
            if (timeline.frames[(size_t)i] > frame) {
                keyA = i - 1;
                keyB = i;
                break;
            }
        }
    }

    uint32_t startFrame = timeline.frames[(size_t)keyA];
    const uint32_t endFrame = timeline.frames[(size_t)keyB];
    int64_t timeBetweenFrames = (int64_t)endFrame - (int64_t)startFrame;
    if (timeBetweenFrames < 0) {
        timeBetweenFrames += duration;
        if (frame < startFrame) {
            startFrame = endFrame;
        }
    }
    const float t = timeBetweenFrames == 0
        ? 0.0f
        : ((float)((int64_t)frame - (int64_t)startFrame) / (float)timeBetweenFrames);

    if (elemCount == 4 && timeline.tag == TAG_KGRT) {
        Vec4 qA = {timeline.values[(size_t)keyA * 4 + 0], timeline.values[(size_t)keyA * 4 + 1],
                   timeline.values[(size_t)keyA * 4 + 2], timeline.values[(size_t)keyA * 4 + 3]};
        Vec4 qB = {timeline.values[(size_t)keyB * 4 + 0], timeline.values[(size_t)keyB * 4 + 1],
                   timeline.values[(size_t)keyB * 4 + 2], timeline.values[(size_t)keyB * 4 + 3]};
        Vec4 q = (timeline.interpolationType == MdxInterpolation::None) ? qA : SlerpQuat(qA, qB, t);
        out[0] = q.x; out[1] = q.y; out[2] = q.z; out[3] = q.w;
        return true;
    }

    for (int elem = 0; elem < elemCount; ++elem) {
        out[elem] = interpolateTimelineValue(timeline, elem, elemCount, keyA, keyB, t);
    }
    return true;
}

// Stage Y1: Zero-allocation in-place timeline sampler with O(1) KeyframeCursorCache
static bool sampleTimelineForAnimator(const MdxTimeline& timeline,
                                      const std::vector<uint32_t>* globalSequences,
                                      const MdxSequence* currentSequence,
                                      uint32_t sequenceFrame,
                                      float elapsedMs,
                                      float* out,
                                      int elemCount,
                                      float defaultVal,
                                      int& ioCursor,
                                      uint64_t& cursorHits,
                                      uint64_t& cursorMisses) {
    if (timeline.globalSequenceId >= 0 && globalSequences
        && timeline.globalSequenceId < (int)globalSequences->size()) {
        const uint32_t duration = (*globalSequences)[(size_t)timeline.globalSequenceId];
        if (duration > 0) {
            const uint32_t elapsed = (uint32_t)std::max(0.0f, elapsedMs);
            return sampleGlobalTimelineLooped(timeline, elapsed % duration, duration, out, elemCount);
        }
    }
    if (!currentSequence) {
        return MdxSampleTimeline(timeline, sequenceFrame, out, elemCount, defaultVal);
    }

    const size_t validCount = (elemCount > 0)
        ? std::min(timeline.frames.size(), timeline.values.size() / (size_t)elemCount)
        : 0;
    if (validCount == 0) {
        for (int elem = 0; elem < elemCount; ++elem) {
            out[elem] = defaultVal;
        }
        return false;
    }

    const uint32_t seqStart = currentSequence->startFrame;
    const uint32_t seqEnd = currentSequence->endFrame;

    // Fast path: check cached cursor interval [ioCursor, ioCursor + 1] or single-step [ioCursor + 1, ioCursor + 2]
    int keyA = -1;
    int keyB = -1;
    if (ioCursor >= 0 && (size_t)(ioCursor + 1) < validCount) {
        const uint32_t f0 = timeline.frames[(size_t)ioCursor];
        const uint32_t f1 = timeline.frames[(size_t)ioCursor + 1];
        if (f0 >= seqStart && f1 <= seqEnd && f0 <= sequenceFrame && sequenceFrame < f1) {
            keyA = ioCursor;
            keyB = ioCursor + 1;
            ++cursorHits;
        } else if ((size_t)(ioCursor + 2) < validCount) {
            const uint32_t f2 = timeline.frames[(size_t)ioCursor + 2];
            if (f1 >= seqStart && f2 <= seqEnd && f1 <= sequenceFrame && sequenceFrame < f2) {
                ioCursor = ioCursor + 1;
                keyA = ioCursor;
                keyB = ioCursor + 1;
                ++cursorHits;
            }
        }
    }

    if (keyA < 0) {
        const auto beginIt = timeline.frames.begin();
        const auto validEndIt = beginIt + (std::ptrdiff_t)validCount;
        const auto firstIt = std::lower_bound(beginIt, validEndIt, seqStart);
        const auto lastEndIt = std::upper_bound(firstIt, validEndIt, seqEnd);
        if (firstIt == lastEndIt) {
            for (int elem = 0; elem < elemCount; ++elem) {
                out[elem] = defaultVal;
            }
            return false;
        }

        const int firstIdx = (int)(firstIt - beginIt);
        const int lastIdx = (int)(lastEndIt - beginIt) - 1;

        if (firstIdx == lastIdx || sequenceFrame <= timeline.frames[(size_t)firstIdx]) {
            if (ioCursor == firstIdx) ++cursorHits;
            else ++cursorMisses;
            ioCursor = firstIdx;
            const size_t baseOff = (size_t)firstIdx * (size_t)elemCount;
            for (int elem = 0; elem < elemCount; ++elem) {
                out[elem] = timeline.values[baseOff + (size_t)elem];
            }
            return true;
        }
        if (sequenceFrame >= timeline.frames[(size_t)lastIdx]) {
            if (ioCursor == lastIdx) ++cursorHits;
            else ++cursorMisses;
            ioCursor = lastIdx;
            const size_t baseOff = (size_t)lastIdx * (size_t)elemCount;
            for (int elem = 0; elem < elemCount; ++elem) {
                out[elem] = timeline.values[baseOff + (size_t)elem];
            }
            return true;
        }

        const auto upperIt = std::upper_bound(firstIt, lastEndIt, sequenceFrame);
        keyB = (int)(upperIt - beginIt);
        keyA = keyB - 1;
        ioCursor = keyA;
        ++cursorMisses;
    }

    const uint32_t fA = timeline.frames[(size_t)keyA];
    const uint32_t fB = timeline.frames[(size_t)keyB];
    const float t = (fB > fA) ? ((float)(sequenceFrame - fA) / (float)(fB - fA)) : 0.0f;

    if (elemCount == 4 && timeline.tag == TAG_KGRT) {
        const size_t offA = (size_t)keyA * 4;
        const size_t offB = (size_t)keyB * 4;
        Vec4 qA = {timeline.values[offA + 0], timeline.values[offA + 1],
                   timeline.values[offA + 2], timeline.values[offA + 3]};
        Vec4 qB = {timeline.values[offB + 0], timeline.values[offB + 1],
                   timeline.values[offB + 2], timeline.values[offB + 3]};
        Vec4 q = (timeline.interpolationType == MdxInterpolation::None) ? qA : SlerpQuat(qA, qB, t);
        out[0] = q.x; out[1] = q.y; out[2] = q.z; out[3] = q.w;
        return true;
    }

    for (int elem = 0; elem < elemCount; ++elem) {
        out[elem] = interpolateTimelineValue(timeline, elem, elemCount, keyA, keyB, t);
    }
    return true;
}

static bool sampleVisibilityTimelineForAnimator(const MdxTimeline& timeline,
                                                const std::vector<uint32_t>* globalSequences,
                                                const MdxSequence* currentSequence,
                                                uint32_t sequenceFrame,
                                                float elapsedMs,
                                                float* out,
                                                int& ioCursor,
                                                uint64_t& cursorHits,
                                                uint64_t& cursorMisses) {
    if (!out || timeline.frames.empty() || timeline.values.empty()) {
        return false;
    }
    if (timeline.globalSequenceId >= 0 && globalSequences
        && timeline.globalSequenceId < (int)globalSequences->size()) {
        const uint32_t duration = (*globalSequences)[(size_t)timeline.globalSequenceId];
        if (duration > 0) {
            const uint32_t frame = (uint32_t)std::max(0.0f, elapsedMs) % duration;
            const size_t validCount = std::min(timeline.frames.size(), timeline.values.size());
            size_t key = validCount - 1;
            if (frame >= timeline.frames[0] && frame < timeline.frames[validCount - 1]) {
                for (size_t i = 1; i < validCount; ++i) {
                    if (timeline.frames[i] > frame) {
                        key = i - 1;
                        break;
                    }
                }
            }
            out[0] = timelineValueOrDefault(timeline, (int)key, 0, 1, 1.0f);
            return true;
        }
    }

    const size_t validCount = std::min(timeline.frames.size(), timeline.values.size());
    if (currentSequence) {
        const uint32_t seqStart = currentSequence->startFrame;
        const uint32_t seqEnd = currentSequence->endFrame;
        if (ioCursor >= 0 && (size_t)ioCursor < validCount) {
            const uint32_t f0 = timeline.frames[(size_t)ioCursor];
            if (f0 >= seqStart && f0 <= seqEnd && f0 <= sequenceFrame) {
                const bool nextAfter = ((size_t)(ioCursor + 1) >= validCount) ||
                                       (timeline.frames[(size_t)ioCursor + 1] > sequenceFrame) ||
                                       (timeline.frames[(size_t)ioCursor + 1] > seqEnd);
                if (nextAfter) {
                    ++cursorHits;
                    out[0] = timelineValueOrDefault(timeline, ioCursor, 0, 1, 1.0f);
                    return true;
                }
            }
        }
        bool found = false;
        size_t key = 0;
        for (size_t i = 0; i < validCount; ++i) {
            const uint32_t keyFrame = timeline.frames[i];
            if (keyFrame < seqStart || keyFrame > seqEnd) {
                continue;
            }
            if (!found || keyFrame <= sequenceFrame) {
                key = i;
                found = true;
            }
        }
        if (!found) {
            out[0] = 1.0f;
            return false;
        }
        ioCursor = (int)key;
        ++cursorMisses;
        out[0] = timelineValueOrDefault(timeline, (int)key, 0, 1, 1.0f);
        return true;
    }

    size_t key = 0;
    for (size_t i = 1; i < validCount; ++i) {
        if (timeline.frames[i] > sequenceFrame) {
            break;
        }
        key = i;
    }
    ioCursor = (int)key;
    out[0] = timelineValueOrDefault(timeline, (int)key, 0, 1, 1.0f);
    return true;
}

static int eventKeyAtFrame(const MdxTimeline& timeline, uint32_t frame, uint32_t start, uint32_t end) {
    if (timeline.frames.empty() || frame < start || frame > end) {
        return -1;
    }
    for (int i = (int)timeline.frames.size() - 1; i >= 0; --i) {
        const uint32_t keyFrame = timeline.frames[(size_t)i];
        if (keyFrame < start) {
            return -1;
        }
        if (keyFrame <= frame) {
            return i;
        }
    }
    return -1;
}

MdxAnimator::MdxAnimator() = default;

int MdxAnimator::GetBoneIndexForObjectId(int objectId) const {
    for (const auto& entry : objectIdToBoneIndex_) {
        if (entry.first == objectId) {
            return entry.second;
        }
    }
    return -1;
}

bool MdxAnimator::IsObjectVisible(int objectId) const {
    for (const auto& node : nodes_) {
        if (node.objectId == objectId) {
            return node.worldVisible;
        }
    }
    return false;
}

bool MdxAnimator::GetObjectWorldTransformYUp(int objectId, Mat4& outMatrix, Vec3* outScale) const {
    for (const auto& node : nodes_) {
        if (node.objectId == objectId) {
            ConvertMatrixToYUp(node.worldMatrix, outMatrix);
            if (!IsFiniteMat4(outMatrix)) {
                outMatrix = Mat4::Identity();
                return false;
            }
            if (outScale) {
                *outScale = node.worldScale;
            }
            return true;
        }
    }
    return false;
}

bool MdxAnimator::GetObjectWorldLocationYUp(int objectId, Vec3& outPosition) const {
    for (const auto& node : nodes_) {
        if (node.objectId != objectId) {
            continue;
        }

        Vec3 pivot = {0, 0, 0};
        if (node.pivotIndex >= 0 && pivotPoints_ && node.pivotIndex < (int)pivotPoints_->size()) {
            const auto& mdxPivot = (*pivotPoints_)[(size_t)node.pivotIndex];
            pivot = {mdxPivot[0], mdxPivot[1], mdxPivot[2]};
        }

        const Mat4& matrix = node.worldMatrix;
        const Vec3 mdxPosition{
            matrix.m[0] * pivot.x + matrix.m[4] * pivot.y + matrix.m[8] * pivot.z + matrix.m[12],
            matrix.m[1] * pivot.x + matrix.m[5] * pivot.y + matrix.m[9] * pivot.z + matrix.m[13],
            matrix.m[2] * pivot.x + matrix.m[6] * pivot.y + matrix.m[10] * pivot.z + matrix.m[14]
        };
        outPosition = {mdxPosition.x, mdxPosition.z, -mdxPosition.y};
        return IsFiniteVec3(outPosition);
    }
    return false;
}

bool MdxAnimator::GetObjectPivotAndWorldTransformYUp(int objectId, Vec3& outPivot, Mat4& outMatrix) const {
    for (const auto& node : nodes_) {
        if (node.objectId != objectId) {
            continue;
        }
        if (node.pivotIndex >= 0 && pivotPoints_ && node.pivotIndex < (int)pivotPoints_->size()) {
            const auto& mdxPivot = (*pivotPoints_)[(size_t)node.pivotIndex];
            outPivot = {mdxPivot[0], mdxPivot[2], -mdxPivot[1]};
        } else {
            outPivot = {0, 0, 0};
        }
        return GetObjectWorldTransformYUp(objectId, outMatrix);
    }
    return false;
}

bool MdxAnimator::GetObjectWorldPositionYUp(int objectId, Vec3& outPosition) const {    Mat4 matrix;
    if (!GetObjectWorldTransformYUp(objectId, matrix)) {
        return false;
    }
    outPosition = {matrix.m[12], matrix.m[13], matrix.m[14]};
    return IsFiniteVec3(outPosition);
}

namespace {

std::string LowerAttachName(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) out.push_back((char)std::tolower(c));
    return out;
}

} // namespace

int MdxAnimator::FindAttachmentObjectId(const std::string& attachPoint) const {
    const std::string query = LowerAttachName(attachPoint);
    if (query.empty() || query == "origin") return -1;
    auto exact = attachNameToObjectId_.find(query);
    if (exact != attachNameToObjectId_.end()) return exact->second;
    for (const auto& [name, objectId] : attachNameToObjectId_) {
        if (name.find(query) != std::string::npos || query.find(name) != std::string::npos) {
            return objectId;
        }
    }
    return -1;
}

bool MdxAnimator::GetAttachmentWorldPositionYUp(const std::string& attachPoint,
                                                Vec3& outPosition) const {
    const int objectId = FindAttachmentObjectId(attachPoint);
    if (objectId < 0) return false;
    // Pivot-inclusive location: node world matrices carry rotation/scale
    // with translations canceled against pivots, so the bare translation
    // is not the attach point (see GetObjectWorldLocationYUp).
    return GetObjectWorldLocationYUp(objectId, outPosition);
}

bool MdxAnimator::GetAttachmentWorldMatrixYUp(const std::string& attachPoint,
                                              Mat4& outMatrix, Vec3& outPosition) const {
    const int objectId = FindAttachmentObjectId(attachPoint);
    if (objectId < 0) return false;
    return GetObjectWorldTransformYUp(objectId, outMatrix) &&
           GetObjectWorldLocationYUp(objectId, outPosition);
}

MdxAnimator::~MdxAnimator() {
    if (!Renderer::IsActive()) return;
    if (boneTexture_.id && sg_query_image_state(boneTexture_) == SG_RESOURCESTATE_VALID)
        sg_destroy_image(boneTexture_);
}

void MdxAnimator::ConvertMatrixToYUp(const Mat4& mdxMat, Mat4& out) {
    Mat4 basis = Mat4::Identity();
    basis.m[0] = 1.0f;  basis.m[1] = 0.0f;  basis.m[2] = 0.0f;  basis.m[3] = 0.0f;
    basis.m[4] = 0.0f;  basis.m[5] = 0.0f;  basis.m[6] = -1.0f; basis.m[7] = 0.0f;
    basis.m[8] = 0.0f;  basis.m[9] = 1.0f;  basis.m[10] = 0.0f; basis.m[11] = 0.0f;
    basis.m[12] = 0.0f; basis.m[13] = 0.0f; basis.m[14] = 0.0f; basis.m[15] = 1.0f;

    Mat4 basisInv = Mat4::Identity();
    basisInv.m[0] = 1.0f;  basisInv.m[1] = 0.0f; basisInv.m[2] = 0.0f;  basisInv.m[3] = 0.0f;
    basisInv.m[4] = 0.0f;  basisInv.m[5] = 0.0f; basisInv.m[6] = 1.0f;  basisInv.m[7] = 0.0f;
    basisInv.m[8] = 0.0f;  basisInv.m[9] = -1.0f; basisInv.m[10] = 0.0f; basisInv.m[11] = 0.0f;
    basisInv.m[12] = 0.0f; basisInv.m[13] = 0.0f; basisInv.m[14] = 0.0f; basisInv.m[15] = 1.0f;

    out = basis * mdxMat * basisInv;
}

void MdxAnimator::Init(const MdxModel& model) {
    sequences_ = &model.sequences;
    globalSequences_ = &model.globalSequences;
    pivotPoints_ = &model.pivotPoints;
    elapsedMs_ = 0.0f;
    BuildSkeleton(model);
    BuildEventTracks(model);
    triggeredEvents_.clear();
    MdxAnimatorLog( "MdxAnimator::Init skeleton nodes=%zu boneCount=%d\n", nodes_.size(), boneCount_);
    if (!nodes_.empty()) {
        MdxAnimatorLog( "MdxAnimator::Init sample frame 0\n");
        SampleAnimations(0);
        MdxAnimatorLog( "MdxAnimator::Init update skeleton\n");
        needsBoneUpload_ = true;
        UpdateSkeleton();
    }
    MdxAnimatorLog( "MdxAnimator::Init end\n");
    MdxAnimatorFlush();
}

void MdxAnimator::BuildEventTracks(const MdxModel& model) {
    eventTracks_.clear();
    for (const MdxEventObject& eventObject : model.eventObjects) {
        const MdxTimeline* eventTrack = findTimeline(eventObject.timelines, TAG_KEVT);
        if (!eventTrack || eventTrack->frames.empty()) {
            continue;
        }
        EventTrackState state;
        state.objectId = eventObject.objectId;
        state.name = eventObject.name;
        state.type = eventObject.type;
        state.id = eventObject.id;
        state.flags = eventObject.flags;
        state.timeline = eventTrack;
        eventTracks_.push_back(std::move(state));
    }
}

void MdxAnimator::ResetEventEmitters() {
    for (EventTrackState& eventTrack : eventTracks_) {
        eventTrack.lastEmissionKey = -1;
    }
    triggeredEvents_.clear();
}

void MdxAnimator::BuildSkeleton(const MdxModel& model) {
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton begin bones=%zu helpers=%zu attachments=%zu lights=%zu emit1=%zu emit2=%zu popcorn=%zu ribbons=%zu events=%zu collisions=%zu\n",
        model.bones.size(), model.helpers.size(), model.attachments.size(), model.lights.size(),
        model.particleEmitters1.size(), model.particleEmitters2.size(), model.particleEmittersPopcorn.size(),
        model.ribbonEmitters.size(), model.eventObjects.size(), model.collisionShapes.size());
    nodes_.clear();
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton after nodes clear\n");
    nodeOrder_.clear();
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton after order clear\n");
    objectIdToBoneIndex_.clear();
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton after bone map clear\n");
    attachNameToObjectId_.clear();
    boneCount_ = 0;
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton after bone count reset\n");
    struct Entry {
        int objectId;
        int parentId;
        int pivotIndex;
        uint32_t flags;
        const std::vector<MdxTimeline>* timelines;
        const MdxGeosetAnim* visibilityGeosetAnim;
        uint32_t visibilityTimelineTag;
    };
    std::vector<Entry> entries;
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton after entries ctor\n");
    entries.reserve(model.bones.size() + model.helpers.size() + model.attachments.size()
        + model.lights.size() + model.particleEmitters1.size() + model.particleEmitters2.size()
        + model.particleEmittersPopcorn.size() + model.ribbonEmitters.size()
        + model.eventObjects.size() + model.collisionShapes.size());
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton after entries reserve\n");
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton after entry map ctor\n");
    auto hasEntryObjectId = [&](int objectId) -> bool {
        for (const auto& entry : entries) {
            if (entry.objectId == objectId) {
                return true;
            }
        }
        return false;
    };
    auto addEntry = [&](int objectId, int parentId, uint32_t flags,
                        const std::vector<MdxTimeline>* timelines,
                        const MdxGeosetAnim* visibilityGeosetAnim = nullptr,
                        uint32_t visibilityTimelineTag = 0) {
        if (objectId < 0 || timelines == nullptr) {
            return;
        }
        if (hasEntryObjectId(objectId)) {
            return;
        }
        if (parentId == objectId) {
            parentId = -1;
        }
        int pivotIndex = -1;
        if (objectId < (int)model.pivotPoints.size()) {
            pivotIndex = objectId;
        }
        entries.push_back({objectId, parentId, pivotIndex, flags, timelines,
                           visibilityGeosetAnim, visibilityTimelineTag});
    };
    auto findGeosetAnimForBone = [&](const MdxNode& bone) -> const MdxGeosetAnim* {
        if (bone.geosetId >= 0) {
            for (const MdxGeosetAnim& geosetAnim : model.geosetAnims) {
                if (geosetAnim.geosetId == bone.geosetId) {
                    return &geosetAnim;
                }
            }
        }
        if (bone.geosetAnimationId >= 0 && bone.geosetAnimationId < (int)model.geosetAnims.size()) {
            return &model.geosetAnims[(size_t)bone.geosetAnimationId];
        }
        return nullptr;
    };
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton before add loops\n");

    for (const auto& b : model.bones) {
        MdxAnimatorLog( "MdxAnimator::BuildSkeleton bone objectId=%d parentId=%d name=%s\n",
            b.objectId, b.parentId, b.name.c_str());
        addEntry(b.objectId, b.parentId, b.flags, &b.timelines, findGeosetAnimForBone(b));
    }
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton after bones\n");
    for (const auto& h : model.helpers) {
        addEntry(h.objectId, h.parentId, h.flags, &h.timelines);
    }
    for (const auto& attachment : model.attachments) {
        addEntry(attachment.objectId, attachment.parentId, attachment.flags, &attachment.timelines, nullptr, TAG_KATV);
        if (!attachment.name.empty()) {
            attachNameToObjectId_[LowerAttachName(attachment.name)] = attachment.objectId;
        }
    }
    for (const auto& light : model.lights) {
        addEntry(light.objectId, light.parentId, light.flags, &light.timelines, nullptr, TAG_KLAV);
    }
    for (const auto& emitter : model.particleEmitters1) {
        addEntry(emitter.objectId, emitter.parentId, emitter.flags, &emitter.timelines, nullptr, TAG_KPEV);
    }
    for (const auto& emitter : model.particleEmitters2) {
        addEntry(emitter.objectId, emitter.parentId, (uint32_t)emitter.flags, &emitter.timelines, nullptr, TAG_KP2V);
    }
    for (const auto& emitter : model.particleEmittersPopcorn) {
        addEntry(emitter.objectId, emitter.parentId, emitter.flags, &emitter.timelines, nullptr, MAKE_TAG('K','P','P','V'));
    }
    for (const auto& ribbon : model.ribbonEmitters) {
        addEntry(ribbon.objectId, ribbon.parentId, ribbon.flags, &ribbon.timelines, nullptr, TAG_KRVS);
    }
    for (const auto& eventObject : model.eventObjects) {
        addEntry(eventObject.objectId, eventObject.parentId, eventObject.flags, &eventObject.timelines);
    }
    for (const auto& collisionShape : model.collisionShapes) {
        addEntry(collisionShape.objectId, collisionShape.parentId, collisionShape.flags, &collisionShape.timelines);
    }
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton entries=%zu\n", entries.size());

    if (entries.empty()) return;

    std::vector<size_t> sortedEntries;
    sortedEntries.reserve(entries.size());
    std::vector<uint8_t> visitState(entries.size(), 0);
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton sort setup ready\n");
    auto appendSubtree = [&](auto&& self, size_t entryIndex) -> void {
        if (visitState[entryIndex] == 2) {
            return;
        }
        if (visitState[entryIndex] == 1) {
            entries[entryIndex].parentId = -1;
            return;
        }
        visitState[entryIndex] = 1;
        sortedEntries.push_back(entryIndex);
        const int objectId = entries[entryIndex].objectId;
        for (size_t childIndex = 0; childIndex < entries.size(); ++childIndex) {
            if (entries[childIndex].parentId == objectId) {
                self(self, childIndex);
            }
        }
        visitState[entryIndex] = 2;
    };

    for (size_t i = 0; i < entries.size(); ++i) {
        const int parentId = entries[i].parentId;
        bool hasParent = false;
        if (parentId >= 0) {
            hasParent = hasEntryObjectId(parentId);
        }
        if (!hasParent) {
            appendSubtree(appendSubtree, i);
        }
    }
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton root walk complete sorted=%zu\n", sortedEntries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        appendSubtree(appendSubtree, i);
    }
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton full walk complete sorted=%zu\n", sortedEntries.size());

    std::vector<MdxAnimNode> builtNodes(sortedEntries.size());
    std::vector<int> builtNodeOrder(sortedEntries.size(), 0);
    std::vector<std::pair<int, int>> builtObjectIdToBoneIndex;
    builtObjectIdToBoneIndex.reserve(sortedEntries.size());
    std::vector<std::pair<int, int>> objectIdToNodeIndex;
    objectIdToNodeIndex.reserve(sortedEntries.size());
    for (size_t i = 0; i < sortedEntries.size(); ++i) {
        const Entry& e = entries[sortedEntries[i]];
        auto& n = builtNodes[i];
        n.objectId = e.objectId;
        n.pivotIndex = e.pivotIndex;
        n.flags = e.flags;
        n.timelines = e.timelines;
        n.visibilityGeosetAnim = e.visibilityGeosetAnim;
        n.visibilityTimelineTag = e.visibilityTimelineTag;
        if (n.timelines) {
            n.tlTranslation = findTimeline(*n.timelines, TAG_KGTR);
            n.tlRotation = findTimeline(*n.timelines, TAG_KGRT);
            n.tlScale = findTimeline(*n.timelines, TAG_KGSC);
            if (n.visibilityTimelineTag != 0) {
                n.tlVisibility = findTimeline(*n.timelines, n.visibilityTimelineTag);
            }
        }
        if (n.visibilityGeosetAnim) {
            n.tlGeosetAlpha = findTimeline(n.visibilityGeosetAnim->timelines, TAG_KGAO);
        }
        objectIdToNodeIndex.push_back({e.objectId, (int)i});
        builtNodeOrder[i] = (int)i;
    }
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton node table ready count=%zu\n", builtNodes.size());
    for (size_t i = 0; i < sortedEntries.size(); ++i) {
        const Entry& e = entries[sortedEntries[i]];
        auto& n = builtNodes[i];
        n.parentIndex = -1;
        for (const auto& parentEntry : objectIdToNodeIndex) {
            if (parentEntry.first == e.parentId) {
                n.parentIndex = parentEntry.second;
                break;
            }
        }
    }
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton parent links ready\n");

    int maxObjectId = -1;
    for (const auto& n : builtNodes) {
        if (n.objectId > maxObjectId) {
            maxObjectId = n.objectId;
        }
    }

    for (auto& n : builtNodes) {
        n.boneIndex = n.objectId;
        builtObjectIdToBoneIndex.push_back({n.objectId, n.boneIndex});
    }
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton bone indices ready count=%d\n", maxObjectId + 1);
    nodes_.swap(builtNodes);
    nodeOrder_.swap(builtNodeOrder);
    objectIdToBoneIndex_.swap(builtObjectIdToBoneIndex);
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton member swaps complete\n");
    boneCount_ = maxObjectId + 1;
    std::vector<Mat4> builtBoneMatrices(boneCount_, Mat4::Identity());
    boneMatrices_.swap(builtBoneMatrices);
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton bone matrices ready count=%zu\n", boneMatrices_.size());
    MdxAnimatorLog( "MdxAnimator::BuildSkeleton end nodes=%zu boneCount=%d\n", nodes_.size(), boneCount_);
}

bool MdxAnimator::SetSequence(int index, bool loop, float speed) {
    if (!sequences_ || index < 0 || index >= (int)sequences_->size()) return false;
    if (sequenceState_.sequenceIndex == index && sequenceState_.loop && loop && !sequenceState_.finished) {
        sequenceState_.speed = speed;
        return true;
    }
    std::vector<NodePose> previous;
    if (blendTime_ > 0.0f && sequenceState_.sequenceIndex >= 0 && !nodes_.empty()) {
        previous.reserve(nodes_.size());
        for (const auto& n : nodes_) {
            previous.push_back({n.localTranslation, n.localRotation, n.localScale});
        }
    }
    const auto& seq = (*sequences_)[index];
    sequenceState_.sequenceIndex = index;
    sequenceState_.currentFrame = seq.startFrame;
    sequenceState_.frameTime = 0;
    sequenceState_.speed = speed;
    sequenceState_.loop = loop;
    sequenceState_.finished = false;
    lodAccumulatedDt_ = 0.0f;
    ResetEventEmitters();
    if (!previous.empty() && previous.size() == nodes_.size()) {
        blendFromPoses_ = std::move(previous);
        blendDuration_ = blendTime_;
        blendLeft_ = blendTime_;
    } else {
        blendLeft_ = 0.0f;
        blendFromPoses_.clear();
    }
    if (!nodes_.empty()) {
        SampleAnimations(sequenceState_.currentFrame);
        if (blendLeft_ > 0.0f) {
            ApplyBlend();
        }
        needsBoneUpload_ = true;
        UpdateSkeleton();
    }
    return true;
}

void MdxAnimator::ApplyBlend() {
    if (blendLeft_ <= 0.0f || blendDuration_ <= 0.0f || blendFromPoses_.size() != nodes_.size()) return;
    const float t = std::clamp(1.0f - blendLeft_ / blendDuration_, 0.0f, 1.0f);
    for (size_t i = 0; i < nodes_.size(); ++i) {
        nodes_[i].localTranslation = {
            blendFromPoses_[i].translation.x * (1.0f - t) + nodes_[i].localTranslation.x * t,
            blendFromPoses_[i].translation.y * (1.0f - t) + nodes_[i].localTranslation.y * t,
            blendFromPoses_[i].translation.z * (1.0f - t) + nodes_[i].localTranslation.z * t
        };
        nodes_[i].localRotation = SlerpQuat(blendFromPoses_[i].rotation, nodes_[i].localRotation, t);
        nodes_[i].localScale = {
            blendFromPoses_[i].scale.x * (1.0f - t) + nodes_[i].localScale.x * t,
            blendFromPoses_[i].scale.y * (1.0f - t) + nodes_[i].localScale.y * t,
            blendFromPoses_[i].scale.z * (1.0f - t) + nodes_[i].localScale.z * t
        };
    }
}

int MdxAnimator::FindSequence(const std::string& name) const {
    if (!sequences_) return -1;
    for (int i = 0; i < (int)sequences_->size(); i++) {
        if ((*sequences_)[i].name == name) return i;
    }
    return -1;
}

bool MdxAnimator::PlaySequence(const std::string& name, bool loop, float speed) {
    int idx = FindSequence(name);
    if (idx < 0) return false;
    return SetSequence(idx, loop, speed);
}

bool MdxAnimator::CrossFadeToSequence(int index, bool loop, float blendSeconds, float speed) {
    const float prevBlendTime = blendTime_;
    blendTime_ = std::max(0.0f, blendSeconds);
    const bool ok = SetSequence(index, loop, speed);
    blendTime_ = prevBlendTime;
    return ok;
}

bool MdxAnimator::CrossFadeToSequence(const std::string& name, bool loop, float blendSeconds, float speed) {
    const int idx = FindSequence(name);
    if (idx < 0) return false;
    return CrossFadeToSequence(idx, loop, blendSeconds, speed);
}

bool MdxAnimator::IsTopologicalOrderValid() const {
    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].parentIndex >= (int)i) {
            return false;
        }
    }
    return true;
}

float MdxAnimator::GetSequenceProgress() const {
    if (sequenceState_.sequenceIndex < 0 || !sequences_) return 0;
    const auto& seq = (*sequences_)[sequenceState_.sequenceIndex];
    uint32_t range = seq.endFrame - seq.startFrame;
    if (range == 0) return 1.0f;
    return (float)(sequenceState_.currentFrame - seq.startFrame) / (float)range;
}

bool MdxAnimator::SetFrameByRatio(float ratio) {
    triggeredEvents_.clear();
    if (sequenceState_.sequenceIndex < 0 || !sequences_) return false;

    const auto& seq = (*sequences_)[sequenceState_.sequenceIndex];
    const uint32_t start = seq.startFrame;
    const uint32_t end = seq.endFrame;
    const uint32_t range = end - start;
    if (range == 0) {
        sequenceState_.currentFrame = start;
        sequenceState_.frameTime = 0.0f;
        return true;
    }

    const float clampedRatio = std::clamp(ratio, 0.0f, 1.0f);
    sequenceState_.currentFrame = start + (uint32_t)std::lround((float)range * clampedRatio);
    if (sequenceState_.currentFrame > end) {
        sequenceState_.currentFrame = end;
    }
    sequenceState_.frameTime = 0.0f;

    if (!nodes_.empty()) {
        SampleAnimations(sequenceState_.currentFrame);
        needsBoneUpload_ = true;
        UpdateSkeleton();
    }
    return true;
}

void MdxAnimator::Update(float dt) {
    triggeredEvents_.clear();
    if (sequenceState_.sequenceIndex < 0) {
        return;
    }
    if (!sequences_ || sequenceState_.sequenceIndex >= (int)sequences_->size()) return;

    if (lodStride_ > 1 && blendLeft_ <= 0.0f) {
        ++lodFrameCounter_;
        if ((lodFrameCounter_ % (uint32_t)lodStride_) != 0) {
            lodAccumulatedDt_ += dt;
            ++lodSkippedUpdates_;
            return;
        }
        dt += lodAccumulatedDt_;
        lodAccumulatedDt_ = 0.0f;
    }

    const auto& seq = (*sequences_)[sequenceState_.sequenceIndex];
    uint32_t start = seq.startFrame;
    uint32_t end = seq.endFrame;
    uint32_t range = end - start;
    if (range == 0) return;

    float advance = dt * 1000.0f * sequenceState_.speed;
    if (advance > 0.0f && std::isfinite(advance)) {
        elapsedMs_ += advance;
    }
    sequenceState_.frameTime += advance;
    int intAdv = (int)sequenceState_.frameTime;
    sequenceState_.frameTime -= (float)intAdv;
    sequenceState_.currentFrame += intAdv;

    const uint32_t animEnd = (end > start) ? (end - 1) : start;
    bool resetEvents = false;
    if (sequenceState_.currentFrame >= animEnd) {
        if (sequenceState_.loop) {
            sequenceState_.currentFrame = start;
            sequenceState_.frameTime = 0.0f;
            resetEvents = true;
        } else {
            sequenceState_.currentFrame = animEnd;
            sequenceState_.frameTime = 0.0f;
            sequenceState_.finished = true;
        }
    }
    if (resetEvents) {
        ResetEventEmitters();
    }

    if (!nodes_.empty()) {
        SampleAnimations(sequenceState_.currentFrame);
        if (blendLeft_ > 0.0f) {
            blendLeft_ = std::max(0.0f, blendLeft_ - dt);
            ApplyBlend();
        }
        needsBoneUpload_ = true;
        UpdateSkeleton();
    }
    UpdateEventEmitters();
}

void MdxAnimator::UpdateEventEmitters() {
    if (!sequences_ || sequenceState_.sequenceIndex < 0
        || sequenceState_.sequenceIndex >= (int)sequences_->size()) {
        return;
    }
    const MdxSequence& sequence = (*sequences_)[(size_t)sequenceState_.sequenceIndex];
    for (EventTrackState& eventTrack : eventTracks_) {
        if (!eventTrack.timeline) {
            continue;
        }

        uint32_t sampleFrame = sequenceState_.currentFrame;
        uint32_t start = sequence.startFrame;
        uint32_t end = sequence.endFrame;
        if (eventTrack.timeline->globalSequenceId >= 0 && globalSequences_
            && eventTrack.timeline->globalSequenceId < (int)globalSequences_->size()) {
            const uint32_t duration = (*globalSequences_)[(size_t)eventTrack.timeline->globalSequenceId];
            if (duration == 0) {
                continue;
            }
            sampleFrame = (uint32_t)std::max(0.0f, elapsedMs_) % duration;
            start = 0;
            end = duration;
        }

        const int keyframeIndex = eventKeyAtFrame(*eventTrack.timeline, sampleFrame, start, end);
        if (keyframeIndex < 0 || keyframeIndex == eventTrack.lastEmissionKey) {
            eventTrack.lastEmissionKey = keyframeIndex;
            continue;
        }

        MdxEventTrigger trigger;
        trigger.objectId = eventTrack.objectId;
        trigger.name = eventTrack.name;
        trigger.type = eventTrack.type;
        trigger.id = eventTrack.id;
        trigger.flags = eventTrack.flags;
        trigger.keyframeIndex = keyframeIndex;
        trigger.frame = eventTrack.timeline->frames[(size_t)keyframeIndex];
        Mat4 localTransform;
        if (GetObjectWorldTransformYUp(trigger.objectId, localTransform, &trigger.localScale)) {
            trigger.hasLocalTransform = true;
            trigger.localTransform = localTransform;
            if (!GetObjectWorldLocationYUp(trigger.objectId, trigger.localPosition)) {
                trigger.localPosition = {localTransform.m[12], localTransform.m[13], localTransform.m[14]};
            }
        }
        triggeredEvents_.push_back(std::move(trigger));
        eventTrack.lastEmissionKey = keyframeIndex;
    }
}

void MdxAnimator::SampleAnimations(uint32_t frame) {
    const MdxSequence* currentSequence = nullptr;
    if (sequences_ && sequenceState_.sequenceIndex >= 0
        && sequenceState_.sequenceIndex < (int)sequences_->size()) {
        currentSequence = &(*sequences_)[(size_t)sequenceState_.sequenceIndex];
    }

    for (auto& node : nodes_) {
        float tmp[4] = {};

        node.localTranslation = {0, 0, 0};
        node.localRotation = {0, 0, 0, 1};
        node.localScale = {1, 1, 1};
        node.localVisibility = 1.0f;

        if (node.visibilityGeosetAnim) {
            node.localVisibility = node.visibilityGeosetAnim->alpha;
            if (node.tlGeosetAlpha && !node.tlGeosetAlpha->frames.empty()
                && sampleTimelineForAnimator(*node.tlGeosetAlpha, globalSequences_, currentSequence, frame, elapsedMs_, tmp, 1,
                                             node.visibilityGeosetAnim->alpha, node.cursorGeosetAlpha, cursorHitCount_, cursorMissCount_)) {
                node.localVisibility = std::isfinite(tmp[0]) ? tmp[0] : node.visibilityGeosetAnim->alpha;
            }
        }

        if (!node.timelines || node.timelines->empty()) continue;

        if (node.tlTranslation && !node.tlTranslation->frames.empty()
            && sampleTimelineForAnimator(*node.tlTranslation, globalSequences_, currentSequence, frame, elapsedMs_, tmp, 3, 0.0f,
                                         node.cursorTranslation, cursorHitCount_, cursorMissCount_)) {
            Vec3 sampled = {tmp[0], tmp[1], tmp[2]};
            if (IsFiniteVec3(sampled)) {
                node.localTranslation = sampled;
            }
        }

        if (node.tlRotation && !node.tlRotation->frames.empty()
            && sampleTimelineForAnimator(*node.tlRotation, globalSequences_, currentSequence, frame, elapsedMs_, tmp, 4, 0.0f,
                                         node.cursorRotation, cursorHitCount_, cursorMissCount_)) {
            Vec4 sampled = NormalizeQuat({tmp[0], tmp[1], tmp[2], tmp[3]});
            if (IsFiniteVec4(sampled)) {
                node.localRotation = sampled;
            }
        }

        if (node.tlScale && !node.tlScale->frames.empty()
            && sampleTimelineForAnimator(*node.tlScale, globalSequences_, currentSequence, frame, elapsedMs_, tmp, 3, 1.0f,
                                         node.cursorScale, cursorHitCount_, cursorMissCount_)) {
            Vec3 sampled = {tmp[0], tmp[1], tmp[2]};
            if (IsFiniteVec3(sampled)) {
                node.localScale = sampled;
            }
        }

        if (!node.visibilityGeosetAnim && node.tlVisibility && !node.tlVisibility->frames.empty()) {
            if (sampleVisibilityTimelineForAnimator(*node.tlVisibility, globalSequences_, currentSequence, frame, elapsedMs_, tmp,
                                                    node.cursorVisibility, cursorHitCount_, cursorMissCount_)) {
                node.localVisibility = std::isfinite(tmp[0]) ? tmp[0] : 1.0f;
            }
        }
    }
}

void MdxAnimator::UpdateSkeleton() {
    MdxAnimatorLog( "MdxAnimator::UpdateSkeleton begin nodes=%zu order=%zu boneMatrices=%zu\n",
        nodes_.size(), nodeOrder_.size(), boneMatrices_.size());
    for (int idx : nodeOrder_) {
        auto& node = nodes_[idx];
        const MdxAnimNode* parent = (node.parentIndex >= 0) ? &nodes_[node.parentIndex] : nullptr;
        node.worldVisible = (!parent || parent->worldVisible) && node.localVisibility > 0.0f;

        const Vec3 localScale = IsFiniteVec3(node.localScale) ? node.localScale : Vec3{1, 1, 1};
        Vec3 computedScale = localScale;
        if ((node.flags & NODE_DONT_INHERIT_SCALING) && parent) {
            computedScale.x = parent->inverseWorldScale.x * localScale.x;
            computedScale.y = parent->inverseWorldScale.y * localScale.y;
            computedScale.z = parent->inverseWorldScale.z * localScale.z;
            node.worldScale = localScale;
        } else if (parent) {
            node.worldScale.x = parent->worldScale.x * localScale.x;
            node.worldScale.y = parent->worldScale.y * localScale.y;
            node.worldScale.z = parent->worldScale.z * localScale.z;
        } else {
            node.worldScale = localScale;
        }

        if (!IsFiniteVec3(computedScale)) {
            computedScale = {1, 1, 1};
            node.worldScale = computedScale;
        }

        Vec4 computedRotation = NormalizeQuat(node.localRotation);
        if ((node.flags & NODE_DONT_INHERIT_ROTATION) && parent) {
            computedRotation = MulQuat(parent->inverseWorldRotation, computedRotation);
        }

        // Build local matrix in MDX Z-up space
        // local = translate(location + pivot) * rotate * scale * translate(-pivot)
        Mat4 local = Mat4::Identity();
        Vec3 pivot = {0, 0, 0};
        bool hasPivot = false;

        // -pivot
        if (node.pivotIndex >= 0 && node.pivotIndex < (int)pivotPoints_->size()) {
            const auto& piv = (*pivotPoints_)[node.pivotIndex];
            pivot = {piv[0], piv[1], piv[2]};
            if (IsFiniteVec3(pivot)) {
                hasPivot = true;
                local = Mat4::Translate(-pivot.x, -pivot.y, -pivot.z);
            }
        }

        // S * R * T (applied right-to-left: first -pivot, then scale, then rotate, then translate)
        local = Mat4::Scale(computedScale.x, computedScale.y, computedScale.z) * local;
        local = Mat4::FromQuaternion(computedRotation) * local;
        Vec3 finalTranslation = IsFiniteVec3(node.localTranslation) ? node.localTranslation : Vec3{0, 0, 0};
        if (hasPivot) {
            finalTranslation = finalTranslation + pivot;
        }
        if ((node.flags & NODE_DONT_INHERIT_TRANSLATION) && parent) {
            finalTranslation = finalTranslation - Vec3{
                parent->worldMatrix.m[12],
                parent->worldMatrix.m[13],
                parent->worldMatrix.m[14]
            };
        }
        local = Mat4::Translate(finalTranslation.x, finalTranslation.y, finalTranslation.z) * local;

        // Parent
        if (node.parentIndex >= 0)
            local = nodes_[node.parentIndex].worldMatrix * local;

        if (!IsFiniteMat4(local)) {
            local = Mat4::Identity();
        }
        node.worldMatrix = local;
        node.worldRotation = parent ? MulQuat(parent->worldRotation, computedRotation) : NormalizeQuat(computedRotation);
        node.inverseWorldRotation = InverseQuat(node.worldRotation);
        node.inverseWorldScale = {
            std::fabs(node.worldScale.x) > 1e-7f ? 1.0f / node.worldScale.x : 0.0f,
            std::fabs(node.worldScale.y) > 1e-7f ? 1.0f / node.worldScale.y : 0.0f,
            std::fabs(node.worldScale.z) > 1e-7f ? 1.0f / node.worldScale.z : 0.0f
        };

        if (node.boneIndex >= 0 && node.boneIndex < boneCount_) {
            Mat4 yup;
            ConvertMatrixToYUp(local, yup);
            const uint32_t billboardFlags = node.flags & (NODE_BILLBOARDED | NODE_BILLBOARDED_LOCK_X
                                                        | NODE_BILLBOARDED_LOCK_Y | NODE_BILLBOARDED_LOCK_Z);
            if (billboardFlags != 0) {
                Vec3 cam = cameraForward_;
                float camLen2 = Dot(cam, cam);
                if (camLen2 > 1e-6f) {
                    cam = Normalize(cam);
                    float hLen = std::sqrt(cam.x * cam.x + cam.z * cam.z);
                    float yaw = (hLen > 1e-6f) ? std::atan2(-cam.x, -cam.z) : 0.0f;
                    float pitch = std::atan2(cam.y, hLen);

                    Mat4 rot = Mat4::Identity();
                    if (billboardFlags & NODE_BILLBOARDED_LOCK_Z) {
                        rot = RotateY(yaw);
                    } else if (billboardFlags & NODE_BILLBOARDED_LOCK_X) {
                        rot = RotateX(pitch);
                    } else if (billboardFlags & NODE_BILLBOARDED_LOCK_Y) {
                        float roll = std::atan2(cam.x, -cam.y);
                        rot = RotateZ(roll);
                    } else {
                        rot = RotateY(yaw) * RotateX(pitch);
                    }

                    Vec3 pivot = {0, 0, 0};
                    if (node.pivotIndex >= 0 && pivotPoints_ && node.pivotIndex < (int)pivotPoints_->size()) {
                        const auto& mdxPivot = (*pivotPoints_)[(size_t)node.pivotIndex];
                        pivot = {mdxPivot[0], mdxPivot[1], mdxPivot[2]};
                    }
                    const Vec3 mdxPivotPos{
                        local.m[0] * pivot.x + local.m[4] * pivot.y + local.m[8] * pivot.z + local.m[12],
                        local.m[1] * pivot.x + local.m[5] * pivot.y + local.m[9] * pivot.z + local.m[13],
                        local.m[2] * pivot.x + local.m[6] * pivot.y + local.m[10] * pivot.z + local.m[14]
                    };
                    Vec3 pos = {mdxPivotPos.x, mdxPivotPos.z, -mdxPivotPos.y};

                    Mat4 billboard = Mat4::Translate(pos.x, pos.y, pos.z)
                                   * rot
                                   * Mat4::Translate(-pos.x, -pos.y, -pos.z);
                    yup = billboard * yup;
                }
            }
            boneMatrices_[node.boneIndex] = IsFiniteMat4(yup) ? yup : Mat4::Identity();
        }
    }
    MdxAnimatorLog( "MdxAnimator::UpdateSkeleton matrices ready\n");
    needsBoneUpload_ = true;
    MdxAnimatorLog( "MdxAnimator::UpdateSkeleton end\n");
    MdxAnimatorFlush();
}

sg_image MdxAnimator::GetBoneTexture() {
    if (needsBoneUpload_) {
        UploadBoneTexture();
    }
    return boneTexture_;
}

void MdxAnimator::UploadBoneTexture() {
    if (!needsBoneUpload_) return;
    needsBoneUpload_ = false;
    if (boneMatrices_.empty()) return;
    if (!Renderer::IsActive()) return;

    int texWidth = boneCount_ * 4;
    std::vector<float> texData((size_t)texWidth * 4, 0.0f);

    for (int i = 0; i < boneCount_; i++) {
        const float* m = boneMatrices_[i].m;
        // 4 columns of vec4
        for (int j = 0; j < 4; j++) {
            texData[(size_t)i * 16 + j * 4 + 0] = m[j * 4 + 0];
            texData[(size_t)i * 16 + j * 4 + 1] = m[j * 4 + 1];
            texData[(size_t)i * 16 + j * 4 + 2] = m[j * 4 + 2];
            texData[(size_t)i * 16 + j * 4 + 3] = m[j * 4 + 3];
        }
    }

    const size_t byteSize = (size_t)texWidth * 4 * sizeof(float);
    if (boneTexture_.id && sg_query_image_state(boneTexture_) == SG_RESOURCESTATE_VALID
        && boneTexWidth_ == texWidth && boneTexHeight_ == 1) {
        sg_image_data data = {};
        data.mip_levels[0].ptr = texData.data();
        data.mip_levels[0].size = byteSize;
        sg_update_image(boneTexture_, &data);
        return;
    }

    if (boneTexture_.id && sg_query_image_state(boneTexture_) == SG_RESOURCESTATE_VALID) {
        sg_destroy_image(boneTexture_);
    }

    sg_image_desc desc = {};
    desc.type = SG_IMAGETYPE_2D;
    desc.width = texWidth;
    desc.height = 1;
    desc.pixel_format = SG_PIXELFORMAT_RGBA32F;
    desc.num_mipmaps = 1;
    desc.usage.stream_update = true;
    boneTexture_ = sg_make_image(&desc);
    boneTexWidth_ = texWidth;
    boneTexHeight_ = 1;

    if (sg_query_image_state(boneTexture_) == SG_RESOURCESTATE_VALID) {
        sg_image_data data = {};
        data.mip_levels[0].ptr = texData.data();
        data.mip_levels[0].size = byteSize;
        sg_update_image(boneTexture_, &data);
    } else {
        std::cerr << "MdxAnimator: bone texture upload failed" << std::endl;
        boneTexture_ = {0};
        boneTexWidth_ = 0;
        boneTexHeight_ = 0;
    }
}
