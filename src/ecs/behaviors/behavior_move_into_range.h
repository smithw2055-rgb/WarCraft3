#pragma once
// Shared "move into range" helper (ref CAbstractRangedBehavior /
// CBehaviorMoveIntoRangeFor pairing machinery, simplified).
//
// Several behaviors (spell casting, Load/BoardTransport, harvest) share one
// pattern: if within range do the effect now, else step toward the target.
// Previously each file inlined its own distance math; these inline helpers
// give them a single tested implementation without a new behavior class.
#include <cmath>

class CSimulation;
class CUnit;

namespace MoveIntoRange {

inline float Distance2D(float ax, float ay, float bx, float by) {
    const float dx = bx - ax;
    const float dy = by - ay;
    return std::sqrt(dx * dx + dy * dy);
}

inline bool IsWithinRange(float ax, float ay, float bx, float by, float range) {
    return Distance2D(ax, ay, bx, by) <= range;
}

// Step (ax,ay) toward (tx,ty) by at most stepLen; writes nx/ny.
// Returns false when already there (dist <= 0).
inline bool StepToward(float ax, float ay, float tx, float ty, float stepLen,
                       float& nx, float& ny) {
    const float dx = tx - ax;
    const float dy = ty - ay;
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= 0.0f || stepLen <= 0.0f) return false;
    const float step = std::min(stepLen, dist);
    nx = ax + (dx / dist) * step;
    ny = ay + (dy / dist) * step;
    return true;
}

} // namespace MoveIntoRange
