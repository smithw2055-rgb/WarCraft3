// Lightning tests: segment math, retail param table, jitter bounds and
// determinism, bolt bookkeeping, SLK override loading. No GL context is
// created (Render is never called).
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "render/lightning.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) { std::cerr << "NEAR @" << __LINE__ << ": " #a " vs " #b "\n"; return false; } } while (0)

namespace {

float DistToSegment(float px, float py, float pz,
                    float ax, float ay, float az,
                    float bx, float by, float bz) {
    const float abx = bx - ax, aby = by - ay, abz = bz - az;
    const float len2 = abx * abx + aby * aby + abz * abz;
    float t = (len2 > 0) ? ((px - ax) * abx + (py - ay) * aby + (pz - az) * abz) / len2 : 0;
    t = std::max(0.0f, std::min(1.0f, t));
    const float dx = px - (ax + abx * t);
    const float dy = py - (ay + aby * t);
    const float dz = pz - (az + abz * t);
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace

bool TestLightningSegmentCount() {
    TEST_ASSERT(LightningSegmentCount(350.0f, 100.0f) == 4);
    TEST_ASSERT(LightningSegmentCount(100.0f, 100.0f) == 1);
    TEST_ASSERT(LightningSegmentCount(0.0f, 100.0f) == 1);
    TEST_ASSERT(LightningSegmentCount(-5.0f, 100.0f) == 1);
    TEST_ASSERT(LightningSegmentCount(100.0f, 0.0f) == 1);
    TEST_ASSERT(LightningSegmentCount(100000.0f, 100.0f) == 64); // clamped
    return true;
}

bool TestLightningDefaultParams() {
    // Retail Splats\LightningData.slk values (dumped from the MPQs).
    LightningBoltParams clpb = DefaultBoltParams("CLPB");
    TEST_NEAR(clpb.avgSegLen, 100.0f, 1e-4f);
    TEST_NEAR(clpb.width, 50.0f, 1e-4f);
    TEST_NEAR(clpb.noiseScale, 0.05f, 1e-6f);
    LightningBoltParams fork = DefaultBoltParams("FORK");
    TEST_NEAR(fork.width, 50.0f, 1e-4f);
    LightningBoltParams drab = DefaultBoltParams("DRAB");
    TEST_NEAR(drab.avgSegLen, 50.0f, 1e-4f);
    TEST_NEAR(drab.width, 40.0f, 1e-4f);
    TEST_NEAR(drab.noiseScale, 0.01f, 1e-6f);
    LightningBoltParams leas = DefaultBoltParams("LEAS");
    TEST_NEAR(leas.width, 80.0f, 1e-4f);
    // Unknown codes fall back to the CLPB row.
    LightningBoltParams unknown = DefaultBoltParams("ZZZZ");
    TEST_NEAR(unknown.width, clpb.width, 1e-4f);
    TEST_NEAR(unknown.avgSegLen, clpb.avgSegLen, 1e-4f);
    return true;
}

bool TestLightningEndpointsAndBounds() {
    LightningPoint p1{0, 0, 0}, p2{400, 0, 0};
    const float length = 400.0f;
    const float noise = 0.05f;
    uint32_t rng = 12345u;
    std::vector<LightningPoint> pts;
    GenerateBoltPoints(p1, p2, 4, noise, rng, pts);
    TEST_ASSERT(pts.size() == 5);
    TEST_NEAR(pts.front().x, 0.0f, 1e-4f);
    TEST_NEAR(pts.front().y, 0.0f, 1e-4f);
    TEST_NEAR(pts.back().x, 400.0f, 1e-4f);
    TEST_NEAR(pts.back().z, 0.0f, 1e-4f);
    // Interior stays within noise * length of the straight segment.
    for (size_t i = 1; i + 1 < pts.size(); i++) {
        const float d = DistToSegment(pts[i].x, pts[i].y, pts[i].z,
                                      0, 0, 0, 400, 0, 0);
        TEST_ASSERT(d <= noise * length + 1e-3f);
    }
    // Zero noise: dead straight.
    uint32_t rng2 = 999u;
    GenerateBoltPoints(p1, p2, 4, 0.0f, rng2, pts);
    for (const auto& p : pts) {
        TEST_NEAR(p.y, 0.0f, 1e-4f);
        TEST_NEAR(p.z, 0.0f, 1e-4f);
    }
    return true;
}

bool TestLightningDeterminism() {
    LightningPoint p1{0, 0, 100}, p2{300, 200, 50};
    uint32_t a = 777u, b = 777u;
    std::vector<LightningPoint> pa, pb;
    GenerateBoltPoints(p1, p2, 6, 0.05f, a, pa);
    GenerateBoltPoints(p1, p2, 6, 0.05f, b, pb);
    TEST_ASSERT(pa.size() == pb.size());
    for (size_t i = 0; i < pa.size(); i++) {
        TEST_NEAR(pa[i].x, pb[i].x, 1e-6f);
        TEST_NEAR(pa[i].y, pb[i].y, 1e-6f);
        TEST_NEAR(pa[i].z, pb[i].z, 1e-6f);
    }
    // Advancing the stream reshapes the interior (flicker works).
    std::vector<LightningPoint> pc;
    GenerateBoltPoints(p1, p2, 6, 0.05f, a, pc);
    bool differs = false;
    for (size_t i = 1; i + 1 < pc.size(); i++) {
        if (std::fabs(pc[i].x - pa[i].x) > 1e-6f) { differs = true; break; }
    }
    TEST_ASSERT(differs);
    return true;
}

bool TestLightningBoltBookkeeping() {
    LightningSystem sys;
    TEST_ASSERT(sys.BoltCount() == 0);
    sys.SetBolt(7, 0, 0, 0, 400, 0, 0, 1, 0.5f, 0.25f, 0.8f, "CLPB");
    TEST_ASSERT(sys.HasBolt(7));
    TEST_ASSERT(sys.BoltCount() == 1);
    const auto* bolt = sys.GetBolt(7);
    TEST_ASSERT(bolt != nullptr);
    TEST_ASSERT(bolt->visible);
    TEST_NEAR(bolt->params.width, 50.0f, 1e-4f);
    // 400 units / AvgSegLen 100 -> 4 segments -> 5 points, ends pinned.
    TEST_ASSERT(bolt->points.size() == 5);
    TEST_NEAR(bolt->points.front().x, 0.0f, 1e-4f);
    TEST_NEAR(bolt->points.back().x, 400.0f, 1e-4f);
    sys.SetBoltVisible(7, false);
    TEST_ASSERT(!sys.GetBolt(7)->visible);
    sys.SetBoltVisible(7, true);
    // Re-setting endpoints regenerates on flicker; removal drops it.
    TEST_ASSERT(sys.RemoveBolt(7));
    TEST_ASSERT(!sys.HasBolt(7));
    TEST_ASSERT(!sys.RemoveBolt(7));
    sys.SetBolt(8, 0, 0, 0, 10, 0, 0, 1, 1, 1, 1, "NOPE");
    sys.Clear();
    TEST_ASSERT(sys.BoltCount() == 0);
    // Update with no GL context must not crash (Render untested here).
    sys.SetBolt(9, 0, 0, 0, 10, 0, 0, 1, 1, 1, 1, "CLPB");
    sys.Update(1.0f);
    TEST_ASSERT(sys.HasBolt(9));
    return true;
}

bool TestLightningSlkOverride() {
    const char* slk =
        "ID;PWXL;N;E\n"
        "C;Y1;X1;K\"Name\"\n"
        "C;Y1;X5;K\"AvgSegLen\"\n"
        "C;Y1;X6;K\"Width\"\n"
        "C;Y1;X11;K\"NoiseScale\"\n"
        "C;Y1;X13;K\"Duration\"\n"
        "C;Y2;X1;K\"CLPB\"\n"
        "C;Y2;X5;K\"100\"\n"
        "C;Y2;X6;K\"77\"\n"
        "C;Y2;X11;K\"0.02\"\n"
        "C;Y2;X13;K\"3\"\n"
        "E\n";
    LightningSystem sys;
    TEST_ASSERT(sys.LoadParamsFromSlk((const uint8_t*)slk, strlen(slk)));
    sys.SetBolt(3, 0, 0, 0, 100, 0, 0, 1, 1, 1, 1, "CLPB");
    TEST_ASSERT(sys.HasBolt(3));
    TEST_NEAR(sys.GetBolt(3)->params.width, 77.0f, 1e-4f);
    TEST_NEAR(sys.GetBolt(3)->params.noiseScale, 0.02f, 1e-6f);
    // Garbage in: no override, false out.
    const char* bad = "not a slk file at all";
    TEST_ASSERT(!sys.LoadParamsFromSlk((const uint8_t*)bad, strlen(bad)));
    TEST_ASSERT(!sys.LoadParamsFromSlk(nullptr, 0));
    return true;
}

bool TestImageBillboardPoseAndCorners() {
    const ImageBillboardPose hidden = MakeImageBillboard(
        10, 20, 30, 64, 32, false, true, false, false, 100, 0, 255, 0, 0, 255);
    TEST_ASSERT(!hidden.visible);

    const ImageBillboardPose follow = MakeImageBillboard(
        10, 20, 30, 64, 32, true, true, false, false, 100, 0, 255, 0, 0, 128);
    TEST_ASSERT(follow.visible);
    TEST_NEAR(follow.x, 10.0f, 1e-4f);
    TEST_NEAR(follow.z, 20.0f, 1e-4f);
    TEST_NEAR(follow.y, 130.0f, 1e-4f);
    TEST_NEAR(follow.halfW, 32.0f, 1e-4f);
    TEST_NEAR(follow.halfH, 16.0f, 1e-4f);
    TEST_NEAR(follow.r, 1.0f, 1e-4f);
    TEST_NEAR(follow.g, 0.0f, 1e-4f);
    TEST_NEAR(follow.a, 128.0f / 255.0f, 1e-4f);

    const ImageBillboardPose absolute = MakeImageBillboard(
        0, 0, 40, 10, 10, true, true, true, true, 100, 80, 0, 255, 0, 255);
    TEST_NEAR(absolute.y, 80.0f, 1e-4f);

    const float camRight[3] = {0.0f, 0.2f, 1.0f};
    float right[3], up[3];
    BillboardAxes(camRight, right, up);
    TEST_NEAR(right[0], 0.0f, 1e-4f);
    TEST_NEAR(right[1], 0.0f, 1e-4f);
    TEST_NEAR(right[2], 1.0f, 1e-4f);
    TEST_NEAR(up[1], 1.0f, 1e-4f);
    float corners[4][3];
    BillboardCorners(0, 10, 0, 2, 4, right, up, corners);
    TEST_NEAR(corners[0][2], 2.0f, 1e-4f);
    TEST_NEAR(corners[0][1], 14.0f, 1e-4f);
    TEST_NEAR(corners[2][2], -2.0f, 1e-4f);
    TEST_NEAR(corners[2][1], 6.0f, 1e-4f);

    LightningSystem sys;
    sys.SetBillboard(7, follow.x, follow.y, follow.z, follow.halfW, follow.halfH,
                     follow.r, follow.g, follow.b, follow.a);
    const ImageBillboardPose* stored = sys.GetBillboard(7);
    TEST_ASSERT(stored != nullptr);
    TEST_NEAR(stored->y, 130.0f, 1e-4f);
    TEST_ASSERT(sys.RemoveBillboard(7));
    TEST_ASSERT(sys.GetBillboard(7) == nullptr);
    return true;
}
