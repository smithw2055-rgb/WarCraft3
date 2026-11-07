#include "fog_state.h"
#include "core/job_system.h"
#include <algorithm>
#include <cmath>
#include <cstring>

CFogArea CFogArea::Rect(float minX, float minY, float maxX, float maxY) {
    CFogArea area;
    area.circle = false;
    area.x1 = std::min(minX, maxX);
    area.y1 = std::min(minY, maxY);
    area.x2 = std::max(minX, maxX);
    area.y2 = std::max(minY, maxY);
    return area;
}

CFogArea CFogArea::Circle(float x, float y, float radius) {
    CFogArea area;
    area.circle = true;
    area.x1 = x;
    area.y1 = y;
    area.x2 = std::max(0.0f, radius);
    area.y2 = 0.0f;
    return area;
}

bool CFogArea::Contains(float x, float y) const {
    if (circle) {
        const float dx = x - x1;
        const float dy = y - y1;
        return dx * dx + dy * dy <= x2 * x2;
    }
    return x >= x1 && x <= x2 && y >= y1 && y <= y2;
}

CFogSystem::CFogSystem() {
    EnsureGrid(64, 64, -4096.0f, -4096.0f, 4096.0f, 4096.0f);
}

void CFogSystem::MarkAllTilesDirty(int player) {
    const int tileStride = tilesX_ * tilesY_;
    if (tileStride <= 0) return;
    if (player < 0) {
        std::fill(dirtyTiles_.begin(), dirtyTiles_.end(), 1);
        return;
    }
    if (player >= kMaxPlayers) return;
    std::fill_n(dirtyTiles_.begin() + (size_t)player * tileStride, tileStride, 1);
}

void CFogSystem::MarkAreaTilesDirty(int player, const CFogArea& area) {
    if (player < 0 || player >= kMaxPlayers || tilesX_ <= 0 || tilesY_ <= 0) return;
    const float spanX = (gridMaxX_ > gridMinX_) ? (gridMaxX_ - gridMinX_) : 1.0f;
    const float spanY = (gridMaxY_ > gridMinY_) ? (gridMaxY_ - gridMinY_) : 1.0f;
    const float cellX = spanX / (float)gridW_;
    const float cellY = spanY / (float)gridH_;

    const float minX = area.circle ? (area.x1 - area.x2) : area.x1;
    const float maxX = area.circle ? (area.x1 + area.x2) : area.x2;
    const float minY = area.circle ? (area.y1 - area.x2) : area.y1;
    const float maxY = area.circle ? (area.y1 + area.x2) : area.y2;

    const int minCol = std::clamp((int)((minX - gridMinX_) / cellX), 0, gridW_ - 1);
    const int maxCol = std::clamp((int)((maxX - gridMinX_) / cellX), 0, gridW_ - 1);
    const int minRow = std::clamp((int)((minY - gridMinY_) / cellY), 0, gridH_ - 1);
    const int maxRow = std::clamp((int)((maxY - gridMinY_) / cellY), 0, gridH_ - 1);

    const int minTileX = minCol / kFogTileSize;
    const int maxTileX = maxCol / kFogTileSize;
    const int minTileY = minRow / kFogTileSize;
    const int maxTileY = maxRow / kFogTileSize;
    const size_t tileBase = (size_t)player * tilesX_ * tilesY_;
    for (int ty = minTileY; ty <= maxTileY; ++ty) {
        for (int tx = minTileX; tx <= maxTileX; ++tx) {
            dirtyTiles_[tileBase + (size_t)ty * tilesX_ + tx] = 1;
        }
    }
}

int CFogSystem::AddModifier(int player, const CFogArea& area, CFogState state) {
    CFogModifier modifier;
    modifier.id = nextId_++;
    modifier.player = player;
    modifier.area = area;
    modifier.state = state;
    modifier.enabled = false; // retail: Start() activates
    modifiers_.push_back(modifier);
    version_++;
    return modifier.id;
}

bool CFogSystem::RemoveModifier(int id) {
    for (auto it = modifiers_.begin(); it != modifiers_.end(); ++it) {
        if (it->id == id) {
            MarkAreaTilesDirty(it->player, it->area);
            modifiers_.erase(it);
            version_++;
            return true;
        }
    }
    return false;
}

bool CFogSystem::SetModifierEnabled(int id, bool enabled) {
    for (auto& modifier : modifiers_) {
        if (modifier.id == id) {
            modifier.enabled = enabled;
            MarkAreaTilesDirty(modifier.player, modifier.area);
            version_++;
            return true;
        }
    }
    return false;
}

int CFogSystem::AddTimedModifier(int player, const CFogArea& area, CFogState state,
                                 float durationSeconds) {
    const int id = AddModifier(player, area, state);
    for (auto& modifier : modifiers_) {
        if (modifier.id == id) {
            modifier.enabled = true;
            modifier.remaining = durationSeconds;
            MarkAreaTilesDirty(modifier.player, modifier.area);
            break;
        }
    }
    version_++;
    return id;
}

void CFogSystem::Tick(float dt) {
    if (dt <= 0.0f) return;
    bool changed = false;
    for (auto it = modifiers_.begin(); it != modifiers_.end();) {
        if (it->remaining >= 0.0f) {
            it->remaining -= dt;
            if (it->remaining <= 0.0f) {
                MarkAreaTilesDirty(it->player, it->area);
                it = modifiers_.erase(it);
                changed = true;
                continue;
            }
        }
        ++it;
    }
    if (changed) version_++;
}

void CFogSystem::AddOverride(int player, const CFogArea& area, CFogState state) {
    CFogOverride paint;
    paint.player = player;
    paint.area = area;
    paint.state = state;
    overrides_.push_back(paint);
    MarkAreaTilesDirty(player, area);
    version_++;
}

void CFogSystem::Clear() {
    modifiers_.clear();
    overrides_.clear();
    std::fill(explored_.begin(), explored_.end(), 0);
    std::fill(visible_.begin(), visible_.end(), 0);
    std::fill(prevVisible_.begin(), prevVisible_.end(), 0);
    MarkAllTilesDirty(-1);
    version_++;
}

void CFogSystem::EnsureGrid(int width, int height,
                            float minX, float minY, float maxX, float maxY) {
    width = std::clamp(width, 1, 512);
    height = std::clamp(height, 1, 512);
    if (width == gridW_ && height == gridH_ &&
        minX == gridMinX_ && minY == gridMinY_ &&
        maxX == gridMaxX_ && maxY == gridMaxY_) {
        return;
    }
    gridW_ = width;
    gridH_ = height;
    tilesX_ = (gridW_ + kFogTileSize - 1) / kFogTileSize;
    tilesY_ = (gridH_ + kFogTileSize - 1) / kFogTileSize;
    gridMinX_ = minX;
    gridMinY_ = minY;
    gridMaxX_ = maxX;
    gridMaxY_ = maxY;
    explored_.assign((size_t)kMaxPlayers * gridW_ * gridH_, 0);
    visible_.assign((size_t)kMaxPlayers * gridW_ * gridH_, 0);
    prevVisible_.assign((size_t)kMaxPlayers * gridW_ * gridH_, 0);
    dirtyTiles_.assign((size_t)kMaxPlayers * tilesX_ * tilesY_, 1);
    version_++;
}

void CFogSystem::UpdateSightForPlayer(int player, const CFogSightSource* sources, int count) {
    if (player < 0 || player >= kMaxPlayers || gridW_ <= 0 || gridH_ <= 0) return;
    const size_t cellCount = (size_t)gridW_ * gridH_;
    const size_t base = (size_t)player * cellCount;

    // Copy current visibility to prevVisible_ and clear visible_ slice for this player
    std::memcpy(prevVisible_.data() + base, visible_.data() + base, cellCount);
    std::memset(visible_.data() + base, 0, cellCount);

    if (sources && count > 0) {
        const float spanX = (gridMaxX_ > gridMinX_) ? (gridMaxX_ - gridMinX_) : 1.0f;
        const float spanY = (gridMaxY_ > gridMinY_) ? (gridMaxY_ - gridMinY_) : 1.0f;
        const float cellX = spanX / (float)gridW_;
        const float cellY = spanY / (float)gridH_;
        const float invCellX = 1.0f / cellX;

        for (int i = 0; i < count; i++) {
            const CFogSightSource& src = sources[i];
            if (src.player != player || src.radius <= 0.0f) continue;

            const int minRow = std::max(0, (int)((src.y - src.radius - gridMinY_) / cellY));
            const int maxRow = std::min(gridH_ - 1, (int)((src.y + src.radius - gridMinY_) / cellY));
            const int bboxMinCol = std::max(0, (int)((src.x - src.radius - gridMinX_) / cellX));
            const int bboxMaxCol = std::min(gridW_ - 1, (int)((src.x + src.radius - gridMinX_) / cellX));
            const float r2 = src.radius * src.radius;

            // Stage X1: Scanline chord span calculation eliminates corner checks
            for (int row = minRow; row <= maxRow; row++) {
                const float cy = gridMinY_ + cellY * (row + 0.5f);
                const float dy = cy - src.y;
                const float rem2 = r2 - dy * dy;
                if (rem2 < 0.0f) continue;

                const float halfChordX = std::sqrt(rem2);
                const int spanMinCol = std::max(bboxMinCol,
                    (int)std::ceil((src.x - halfChordX - gridMinX_) * invCellX - 0.5f));
                const int spanMaxCol = std::min(bboxMaxCol,
                    (int)std::floor((src.x + halfChordX - gridMinX_) * invCellX - 0.5f));

                const size_t rowBase = base + (size_t)row * gridW_;
                if (!occlusionChecker_) {
                    for (int col = spanMinCol; col <= spanMaxCol; col++) {
                        visible_[rowBase + col] = 1;
                        explored_[rowBase + col] = 1;
                    }
                } else {
                    for (int col = spanMinCol; col <= spanMaxCol; col++) {
                        const float cx = gridMinX_ + cellX * (col + 0.5f);
                        if (occlusionChecker_(src.x, src.y, cx, cy, src.isFlying)) {
                            continue;
                        }
                        visible_[rowBase + col] = 1;
                        explored_[rowBase + col] = 1;
                    }
                }
            }
        }
    }

    // Update 16x16 dirty tiles where visibility changed
    const size_t tileBase = (size_t)player * tilesX_ * tilesY_;
    for (int row = 0; row < gridH_; ++row) {
        const size_t rowOffset = base + (size_t)row * gridW_;
        const int ty = row / kFogTileSize;
        for (int col = 0; col < gridW_; ++col) {
            if (visible_[rowOffset + col] != prevVisible_[rowOffset + col]) {
                dirtyTiles_[tileBase + (size_t)ty * tilesX_ + (col / kFogTileSize)] = 1;
            }
        }
    }
}

void CFogSystem::UpdateSight(const CFogSightSource* sources, int count) {
    for (int player = 0; player < kMaxPlayers; ++player) {
        UpdateSightForPlayer(player, sources, count);
    }
    version_++;
}

void CFogSystem::UpdateSightParallel(const CFogSightSource* sources, int count,
                                     core::JobSystem* jobSystem) {
    if (!jobSystem || kMaxPlayers <= 1) {
        UpdateSight(sources, count);
        return;
    }
    jobSystem->ParallelFor((size_t)kMaxPlayers, 1, [&](size_t playerIdx) {
        UpdateSightForPlayer(static_cast<int>(playerIdx), sources, count);
    });
    version_++;
}

int CFogSystem::GetDirtyTileCount(int player) const {
    if (player < 0 || player >= kMaxPlayers || tilesX_ <= 0 || tilesY_ <= 0) return 0;
    const size_t tileCount = (size_t)tilesX_ * tilesY_;
    const size_t tileBase = (size_t)player * tileCount;
    int dirty = 0;
    for (size_t i = 0; i < tileCount; ++i) {
        if (dirtyTiles_[tileBase + i] != 0) ++dirty;
    }
    return dirty;
}

void CFogSystem::ClearDirtyTiles(int player) {
    if (player < 0 || player >= kMaxPlayers || tilesX_ <= 0 || tilesY_ <= 0) return;
    const size_t tileCount = (size_t)tilesX_ * tilesY_;
    const size_t tileBase = (size_t)player * tileCount;
    std::memset(dirtyTiles_.data() + tileBase, 0, tileCount);
}

bool CFogSystem::ScriptedState(int player, float x, float y, CFogState& out) const {
    bool covered = false;
    for (const auto& paint : overrides_) {
        if (paint.player == player && paint.area.Contains(x, y)) {
            out = paint.state;
            covered = true;
        }
    }
    for (const auto& modifier : modifiers_) {
        if (modifier.enabled && modifier.player == player && modifier.area.Contains(x, y)) {
            out = modifier.state;
            covered = true;
        }
    }
    return covered;
}

int CFogSystem::CellIndex(int player, float x, float y) const {
    if (player < 0 || player >= kMaxPlayers) return -1;
    if (x < gridMinX_ || x >= gridMaxX_ || y < gridMinY_ || y >= gridMaxY_) return -1;
    const float spanX = (gridMaxX_ > gridMinX_) ? (gridMaxX_ - gridMinX_) : 1.0f;
    const float spanY = (gridMaxY_ > gridMinY_) ? (gridMaxY_ - gridMinY_) : 1.0f;
    const int col = std::min(gridW_ - 1, (int)((x - gridMinX_) / spanX * gridW_));
    const int row = std::min(gridH_ - 1, (int)((y - gridMinY_) / spanY * gridH_));
    if (col < 0 || row < 0) return -1;
    return (int)((size_t)player * gridW_ * gridH_ + (size_t)row * gridW_ + col);
}

CFogState CFogSystem::GetState(int player, float x, float y) const {
    CFogState scripted = CFogState::VISIBLE;
    if (ScriptedState(player, x, y, scripted)) return scripted;
    const int idx = CellIndex(player, x, y);
    if (idx < 0) return CFogState::MASKED;
    if (visible_[(size_t)idx]) return CFogState::VISIBLE;
    if (explored_[(size_t)idx]) return CFogState::FOGGED;
    return CFogState::MASKED;
}

bool CFogSystem::IsVisibleTo(int player, float x, float y) const {
    return GetState(player, x, y) == CFogState::VISIBLE;
}

bool CFogSystem::IsMaskedTo(int player, float x, float y) const {
    return GetState(player, x, y) == CFogState::MASKED;
}

static inline unsigned char FogStateToByte(CFogState st) {
    switch (st) {
        case CFogState::MASKED:  return 255;
        case CFogState::FOGGED:  return 127;
        case CFogState::VISIBLE: return 0;
    }
    return 0;
}

void CFogSystem::Rasterize(int player, int width, int height,
                           float minX, float minY, float maxX, float maxY,
                           unsigned char* out) const {
    lastRasterizeModifierChecks_ = 0;
    if (!out || width <= 0 || height <= 0) return;
    if (player < 0 || player >= kMaxPlayers || gridW_ <= 0 || gridH_ <= 0) {
        std::memset(out, 255, (size_t)width * height);
        return;
    }

    const float spanX = (maxX > minX) ? (maxX - minX) : 1.0f;
    const float spanY = (maxY > minY) ? (maxY - minY) : 1.0f;
    const size_t playerBase = (size_t)player * gridW_ * gridH_;

    // Pass 1: Single-pass base grid synthesis (0 per-pixel modifier loop!)
    const bool exactGridMatch = (width == gridW_ && height == gridH_ &&
                                 minX == gridMinX_ && minY == gridMinY_ &&
                                 maxX == gridMaxX_ && maxY == gridMaxY_);
    if (exactGridMatch) {
        const size_t total = (size_t)width * height;
        const uint8_t* visPtr = visible_.data() + playerBase;
        const uint8_t* expPtr = explored_.data() + playerBase;
        for (size_t i = 0; i < total; ++i) {
            out[i] = visPtr[i] ? 0 : (expPtr[i] ? 127 : 255);
        }
    } else {
        const float gridSpanX = (gridMaxX_ > gridMinX_) ? (gridMaxX_ - gridMinX_) : 1.0f;
        const float gridSpanY = (gridMaxY_ > gridMinY_) ? (gridMaxY_ - gridMinY_) : 1.0f;
        for (int row = 0; row < height; row++) {
            const float y = minY + spanY * (row + 0.5f) / (float)height;
            const bool yInGrid = (y >= gridMinY_ && y < gridMaxY_);
            const int gRow = yInGrid ? std::min(gridH_ - 1, (int)((y - gridMinY_) / gridSpanY * gridH_)) : -1;
            unsigned char* outRow = out + (size_t)row * width;
            if (!yInGrid || gRow < 0) {
                std::memset(outRow, 255, (size_t)width);
                continue;
            }
            const size_t gRowBase = playerBase + (size_t)gRow * gridW_;
            for (int col = 0; col < width; col++) {
                const float x = minX + spanX * (col + 0.5f) / (float)width;
                if (x < gridMinX_ || x >= gridMaxX_) {
                    outRow[col] = 255;
                    continue;
                }
                const int gCol = std::min(gridW_ - 1, (int)((x - gridMinX_) / gridSpanX * gridW_));
                const size_t gIdx = gRowBase + (size_t)gCol;
                outRow[col] = visible_[gIdx] ? 0 : (explored_[gIdx] ? 127 : 255);
            }
        }
    }

    // Pass 2: Stamp overrides and active modifiers ONLY inside their clamped AABB!
    const float pixX = spanX / (float)width;
    const float pixY = spanY / (float)height;
    const float invPixX = 1.0f / pixX;
    const float invPixY = 1.0f / pixY;

    auto stampArea = [&](const CFogArea& area, CFogState state) {
        const float aMinX = area.circle ? (area.x1 - area.x2) : area.x1;
        const float aMaxX = area.circle ? (area.x1 + area.x2) : area.x2;
        const float aMinY = area.circle ? (area.y1 - area.x2) : area.y1;
        const float aMaxY = area.circle ? (area.y1 + area.x2) : area.y2;

        if (aMaxX < minX || aMinX > maxX || aMaxY < minY || aMinY > maxY) {
            return;
        }
        const int minCol = std::clamp((int)std::floor((aMinX - minX) * invPixX - 0.5f), 0, width - 1);
        const int maxCol = std::clamp((int)std::ceil((aMaxX - minX) * invPixX - 0.5f), 0, width - 1);
        const int minRow = std::clamp((int)std::floor((aMinY - minY) * invPixY - 0.5f), 0, height - 1);
        const int maxRow = std::clamp((int)std::ceil((aMaxY - minY) * invPixY - 0.5f), 0, height - 1);

        const unsigned char byteVal = FogStateToByte(state);
        for (int row = minRow; row <= maxRow; ++row) {
            const float y = minY + pixY * (row + 0.5f);
            unsigned char* outRow = out + (size_t)row * width;
            for (int col = minCol; col <= maxCol; ++col) {
                const float x = minX + pixX * (col + 0.5f);
                ++lastRasterizeModifierChecks_;
                if (area.Contains(x, y)) {
                    outRow[col] = byteVal;
                }
            }
        }
    };

    for (const auto& paint : overrides_) {
        if (paint.player == player) {
            stampArea(paint.area, paint.state);
        }
    }
    for (const auto& modifier : modifiers_) {
        if (modifier.enabled && modifier.player == player) {
            stampArea(modifier.area, modifier.state);
        }
    }
}
