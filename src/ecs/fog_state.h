#pragma once
#include <cstdint>
#include <vector>

// Fog states use the common.j ConvertFogState ordinals
// (FOG_OF_WAR_MASKED=0, FOG_OF_WAR_FOGGED=1, FOG_OF_WAR_VISIBLE=2).
enum class CFogState : uint8_t {
    MASKED = 0,
    FOGGED = 1,
    VISIBLE = 2,
};

// A rect (x1<=x2, y1<=y2) or circle (center + radius in x2) fog area.
struct CFogArea {
    bool circle = false;
    float x1 = 0, y1 = 0, x2 = 0, y2 = 0;

    static CFogArea Rect(float minX, float minY, float maxX, float maxY);
    static CFogArea Circle(float x, float y, float radius);
    bool Contains(float x, float y) const;
};

struct CFogModifier {
    int id = 0;
    int player = -1;
    CFogArea area;
    CFogState state = CFogState::VISIBLE;
    bool enabled = false;
};

struct CFogOverride {
    int player = -1;
    CFogArea area;
    CFogState state = CFogState::VISIBLE;
};

// A unit sight source for the vision pass (radius chosen by the caller
// for day/night).
struct CFogSightSource {
    int player = -1;
    float x = 0, y = 0, radius = 0;
};

// ============================================================
// CFogSystem - fog of war: scripted entries plus unit sight
//
// Two layers feed every query:
//   1. Scripted (exact): JASS fog modifiers (Far Sight, black-mask
//      cinematics) and SetFogState* paints. Paints apply first, then
//      enabled modifiers; later entries win ties.
//   2. Sight grid (per player): unit sight circles splat VISIBLE and
//      stick explored memory; a seen-then-left cell reads FOGGED, a
//      never-seen cell reads MASKED.
//
// Scripts win over the grid, so Far Sight punches through unexplored
// black mask exactly.
// ============================================================
class CFogSystem {
public:
    static constexpr int kMaxPlayers = 12;
    // Recompute cadence for the sight pass (sim ticks).
    static constexpr int kSightTickPeriod = 10;

    CFogSystem();

    // Modifiers (FogModifierStart/Stop/Destroy).
    int AddModifier(int player, const CFogArea& area, CFogState state);
    bool RemoveModifier(int id);
    bool SetModifierEnabled(int id, bool enabled);

    // Forced paints (SetFogStateRect/Radius); permanent until cleared.
    void AddOverride(int player, const CFogArea& area, CFogState state);
    void Clear();

    // Sight grid: resized when a real map is known, splatted with unit
    // sight circles on the sight cadence.
    void EnsureGrid(int width, int height,
                    float minX, float minY, float maxX, float maxY);
    void UpdateSight(const CFogSightSource* sources, int count);

    CFogState GetState(int player, float x, float y) const;
    bool IsVisibleTo(int player, float x, float y) const;
    bool IsMaskedTo(int player, float x, float y) const;

    // Rasterize one player's fog for the renderer: 0 = visible,
    // 127 = fogged, 255 = masked (matches the terrain shader).
    void Rasterize(int player, int width, int height,
                   float minX, float minY, float maxX, float maxY,
                   unsigned char* out) const;

    // Bumped on every mutation; the renderer re-uploads on change.
    uint64_t Version() const { return version_; }

private:
    bool ScriptedState(int player, float x, float y, CFogState& out) const;
    int CellIndex(int player, float x, float y) const;

    std::vector<CFogModifier> modifiers_;
    std::vector<CFogOverride> overrides_;
    int nextId_ = 1;
    uint64_t version_ = 0;

    int gridW_ = 0, gridH_ = 0;
    float gridMinX_ = 0, gridMinY_ = 0, gridMaxX_ = 0, gridMaxY_ = 0;
    std::vector<uint8_t> explored_;
    std::vector<uint8_t> visible_;
};
