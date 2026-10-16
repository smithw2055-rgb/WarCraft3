#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct PathTexImage;

struct WpmData {
    enum Flags : uint8_t {
        UNWALKABLE = 0x02,
        UNFLYABLE = 0x04,
        UNBUILDABLE = 0x08,
        BLOCKVISION = 0x10,
        BLIGHTED = 0x20,
        UNSWIMABLE = 0x40,
        // Map-edge ring (ref PathingGrid BOUNDARY 0xF0): outside the
        // playable area. All movement types fail on it.
        BOUNDARY = 0xF0,
    };

    int width = 0;
    int height = 0;
    std::vector<uint8_t> pathing; // width * height bytes
    bool IsValid() const { return width > 0 && height > 0 && pathing.size() == static_cast<size_t>(width) * static_cast<size_t>(height); }

    // ---- Dynamic footprint stamps (ref RemovablePathingMapInstance) ----
    // Buildings and destructables OR their pathTex footprints here;
    // removal rebuilds from the pristine base copy.
    struct Stamp {
        float x = 0, y = 0;
        int rotationDeg = 0;
        std::shared_ptr<const PathTexImage> image;
        bool blocksVision = false;
        bool alive = true;
    };
    std::vector<uint8_t> basePathing; // pristine copy, filled by WpmLoad
    std::vector<Stamp> stamps;
    // World origin of cell (0,0) in world units (32-unit cells, truncation
    // toward zero like the retail cell math).
    float originX = 0, originY = 0;
    static constexpr int kCellSize = 32;

    // Stamp a footprint; returns the stamp id, or -1 when there is
    // nothing to stamp (null/empty image, invalid grid).
    int StampFootprint(float x, float y, int rotationDeg,
                       std::shared_ptr<const PathTexImage> image,
                       bool blocksVision = false);
    void UnstampFootprint(int id);
    void SetStampBlocksVision(int id, bool blocksVision);
    void RebuildDynamic();

    bool IsWalkable(int x, int y) const;
    bool IsFlyable(int x, int y) const;
    bool IsBuildable(int x, int y) const;
    bool IsBlight(int x, int y) const;
    // Naval sense (ref PathingGrid FLOAT-pathable): clear UNSWIMABLE means
    // ships can navigate the cell (deep water). This is NOT the w3e tile
    // water flag (shallow/shore rendering); see W3ETileCorner::IsWater.
    //
    // Water-definition map (32px WPM cells vs 128px W3E corners):
    // - naval movement, FLOAT/AMPH pathing and JASS float queries read
    //   THIS grid (UNSWIMABLE clear = navigable deep water);
    // - rendering, minimap blue and shore visuals read the W3E 0x40 corner
    //   bit (includes shallow/shore rings where UNSWIMABLE stays set).
    bool IsWater(int x, int y) const;
    // Playable-area ring (BOUNDARY bits set, or off-grid entirely).
    bool IsBoundary(int x, int y) const;
};

bool WpmLoad(const uint8_t* data, size_t size, WpmData& out);
bool WpmLoadFromMpq(const std::string& mapPath, WpmData& out);
