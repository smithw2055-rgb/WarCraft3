#pragma once
#include <cstdint>
#include <functional>
#include <vector>

namespace core {
class JobSystem;
}

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
    // Timed reveal (ref CTimedCircleFogModifier / Far Sight duration):
    // remaining < 0 means permanent; otherwise counts down in sim seconds
    // via Tick() and the modifier is removed at zero.
    float remaining = -1.0f;
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
    bool isFlying = false;
    // Phase 25: 32-bit player bitmask allowing a single sight source to serve
    // the owner + all shared-vision allies without duplicating CFogSightSource structs.
    uint32_t playerMask = 0;

    bool MatchesPlayer(int p) const {
        if (p < 0 || p >= 32) return false;
        if (playerMask != 0) return (playerMask & (1u << p)) != 0;
        return player == p;
    }
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
    // Tile size for dirty-region tracking (Stage X1).
    static constexpr int kFogTileSize = 16;

    CFogSystem();

    // Modifiers (FogModifierStart/Stop/Destroy).
    int AddModifier(int player, const CFogArea& area, CFogState state);
    // Timed variant (ref CTimedCircleFogModifier): starts enabled and
    // expires after durationSeconds of Tick() time.
    int AddTimedModifier(int player, const CFogArea& area, CFogState state,
                         float durationSeconds);
    bool RemoveModifier(int id);
    bool SetModifierEnabled(int id, bool enabled);
    // Advance timed modifiers by dt seconds; expired ones are removed.
    // Called once per sim tick from CSimulation::Update.
    void Tick(float dt);

    // Forced paints (SetFogStateRect/Radius); permanent until cleared.
    void AddOverride(int player, const CFogArea& area, CFogState state);
    void Clear();

    // Sight grid: resized when a real map is known, splatted with unit
    // sight circles on the sight cadence.
    void EnsureGrid(int width, int height,
                    float minX, float minY, float maxX, float maxY);
    void UpdateSight(const CFogSightSource* sources, int count);
    // Stage X2: Multi-player parallel sight update via JobSystem::ParallelFor.
    // Produces bit-identical results to UpdateSight().
    void UpdateSightParallel(const CFogSightSource* sources, int count,
                             core::JobSystem* jobSystem = nullptr);

    CFogState GetState(int player, float x, float y) const;
    bool IsVisibleTo(int player, float x, float y) const;
    bool IsMaskedTo(int player, float x, float y) const;

    // Rasterize one player's fog for the renderer: 0 = visible,
    // 127 = fogged, 255 = masked (matches the terrain shader).
    // Stage X1: Single-pass base bitmap synthesis + AABB-clipped modifier stamp.
    void Rasterize(int player, int width, int height,
                   float minX, float minY, float maxX, float maxY,
                   unsigned char* out) const;

    // Bumped on every mutation; the renderer re-uploads on change.
    uint64_t Version() const { return version_; }

    // Stage X1: Dirty 16x16 tile tracking and rasterization telemetry.
    int GetDirtyTileCount(int player) const;
    void ClearDirtyTiles(int player);
    int GetTilesX() const { return tilesX_; }
    int GetTilesY() const { return tilesY_; }
    uint64_t GetLastRasterizeModifierChecks() const { return lastRasterizeModifierChecks_; }

    // Line of sight occlusion checker (returns true if LoS between from and to is blocked).
    using FogOcclusionChecker = std::function<bool(float fromX, float fromY, float toX, float toY, bool isFlying)>;
    void SetOcclusionChecker(FogOcclusionChecker checker) { occlusionChecker_ = std::move(checker); }
    const FogOcclusionChecker& GetOcclusionChecker() const { return occlusionChecker_; }

private:
    bool ScriptedState(int player, float x, float y, CFogState& out) const;
    int CellIndex(int player, float x, float y) const;
    void UpdateSightForPlayer(int player, const CFogSightSource* sources, int count);
    void MarkAreaTilesDirty(int player, const CFogArea& area);
    void MarkAllTilesDirty(int player);

    FogOcclusionChecker occlusionChecker_;
    std::vector<CFogModifier> modifiers_;
    std::vector<CFogOverride> overrides_;
    int nextId_ = 1;
    uint64_t version_ = 0;

    int gridW_ = 0, gridH_ = 0;
    int tilesX_ = 0, tilesY_ = 0;
    float gridMinX_ = 0, gridMinY_ = 0, gridMaxX_ = 0, gridMaxY_ = 0;
    std::vector<uint8_t> explored_;
    std::vector<uint8_t> visible_;
    std::vector<uint8_t> prevVisible_;
    std::vector<uint8_t> dirtyTiles_;
    mutable uint64_t lastRasterizeModifierChecks_ = 0;
};
