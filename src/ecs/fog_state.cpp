#include "fog_state.h"
#include <algorithm>
#include <cmath>

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
            version_++;
            return true;
        }
    }
    return false;
}

void CFogSystem::AddOverride(int player, const CFogArea& area, CFogState state) {
    CFogOverride paint;
    paint.player = player;
    paint.area = area;
    paint.state = state;
    overrides_.push_back(paint);
    version_++;
}

void CFogSystem::Clear() {
    modifiers_.clear();
    overrides_.clear();
    std::fill(explored_.begin(), explored_.end(), 0);
    std::fill(visible_.begin(), visible_.end(), 0);
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
    gridMinX_ = minX;
    gridMinY_ = minY;
    gridMaxX_ = maxX;
    gridMaxY_ = maxY;
    explored_.assign((size_t)kMaxPlayers * gridW_ * gridH_, 0);
    visible_.assign((size_t)kMaxPlayers * gridW_ * gridH_, 0);
    version_++;
}

void CFogSystem::UpdateSight(const CFogSightSource* sources, int count) {
    std::fill(visible_.begin(), visible_.end(), 0);
    if (!sources || count <= 0) {
        version_++;
        return;
    }
    const float spanX = (gridMaxX_ > gridMinX_) ? (gridMaxX_ - gridMinX_) : 1.0f;
    const float spanY = (gridMaxY_ > gridMinY_) ? (gridMaxY_ - gridMinY_) : 1.0f;
    const float cellX = spanX / (float)gridW_;
    const float cellY = spanY / (float)gridH_;
    for (int i = 0; i < count; i++) {
        const CFogSightSource& src = sources[i];
        if (src.player < 0 || src.player >= kMaxPlayers || src.radius <= 0) continue;
        const size_t base = (size_t)src.player * gridW_ * gridH_;
        const int minCol = std::max(0, (int)((src.x - src.radius - gridMinX_) / cellX));
        const int maxCol = std::min(gridW_ - 1, (int)((src.x + src.radius - gridMinX_) / cellX));
        const int minRow = std::max(0, (int)((src.y - src.radius - gridMinY_) / cellY));
        const int maxRow = std::min(gridH_ - 1, (int)((src.y + src.radius - gridMinY_) / cellY));
        const float r2 = src.radius * src.radius;
        for (int row = minRow; row <= maxRow; row++) {
            const float cy = gridMinY_ + cellY * (row + 0.5f);
            for (int col = minCol; col <= maxCol; col++) {
                const float cx = gridMinX_ + cellX * (col + 0.5f);
                const float dx = cx - src.x;
                const float dy = cy - src.y;
                if (dx * dx + dy * dy <= r2) {
                    const size_t idx = base + (size_t)row * gridW_ + col;
                    visible_[idx] = 1;
                    explored_[idx] = 1;
                }
            }
        }
    }
    version_++;
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

void CFogSystem::Rasterize(int player, int width, int height,
                           float minX, float minY, float maxX, float maxY,
                           unsigned char* out) const {
    if (!out || width <= 0 || height <= 0) return;
    const float spanX = (maxX > minX) ? (maxX - minX) : 1.0f;
    const float spanY = (maxY > minY) ? (maxY - minY) : 1.0f;
    for (int row = 0; row < height; row++) {
        const float y = minY + spanY * (row + 0.5f) / (float)height;
        for (int col = 0; col < width; col++) {
            const float x = minX + spanX * (col + 0.5f) / (float)width;
            unsigned char value = 0;
            switch (GetState(player, x, y)) {
            case CFogState::MASKED: value = 255; break;
            case CFogState::FOGGED: value = 127; break;
            case CFogState::VISIBLE: value = 0; break;
            }
            out[row * width + col] = value;
        }
    }
}
