#include "mdx.h"
#include <cstring>
#include <iostream>
#include <algorithm>
#include <cassert>

static uint32_t R32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
static int32_t RS32(const uint8_t* p) { return (int32_t)R32(p); }
static float RF32(const uint8_t* p) { float v; memcpy(&v, p, 4); return v; }

#define MAKE_TAG(A,B,C,D) ((A<<24)|(B<<16)|(C<<8)|D)
#define T_MDLX MAKE_TAG('M','D','L','X')
#define T_VERS MAKE_TAG('V','E','R','S')
#define T_MODL MAKE_TAG('M','O','D','L')
#define T_SEQS MAKE_TAG('S','E','Q','S')
#define T_GLBS MAKE_TAG('G','L','B','S')
#define T_MTLS MAKE_TAG('M','T','L','S')
#define T_TEXS MAKE_TAG('T','E','X','S')
#define T_TXAN MAKE_TAG('T','X','A','N')
#define T_GEOS MAKE_TAG('G','E','O','S')
#define T_GEOA MAKE_TAG('G','E','O','A')
#define T_BONE MAKE_TAG('B','O','N','E')
#define T_PIVT MAKE_TAG('P','I','V','T')
#define T_HELP MAKE_TAG('H','E','L','P')
#define T_ATCH MAKE_TAG('A','T','C','H')
#define T_LITE MAKE_TAG('L','I','T','E')
#define T_CAMS MAKE_TAG('C','A','M','S')
#define T_EVTS MAKE_TAG('E','V','T','S')
#define T_CLID MAKE_TAG('C','L','I','D')
#define T_PREM MAKE_TAG('P','R','E','M')
#define T_PRE2 MAKE_TAG('P','R','E','2')
#define T_RIBB MAKE_TAG('R','I','B','B')
#define T_FAFX MAKE_TAG('F','A','F','X')
#define T_BPOS MAKE_TAG('B','P','O','S')
#define T_CORN MAKE_TAG('C','O','R','N')
#define T_VRTX MAKE_TAG('V','R','T','X')
#define T_NRMS MAKE_TAG('N','R','M','S')
#define T_PTYP MAKE_TAG('P','T','Y','P')
#define T_PCNT MAKE_TAG('P','C','N','T')
#define T_PVTX MAKE_TAG('P','V','T','X')
#define T_GNDX MAKE_TAG('G','N','D','X')
#define T_MTGC MAKE_TAG('M','T','G','C')
#define T_MATS MAKE_TAG('M','A','T','S')
#define T_TANG MAKE_TAG('T','A','N','G')
#define T_SKIN MAKE_TAG('S','K','I','N')
#define T_UVAS MAKE_TAG('U','V','A','S')
#define T_UVBS MAKE_TAG('U','V','B','S')

static constexpr uint32_t TAG_KEVT = MAKE_TAG('K','E','V','T');
static constexpr uint32_t TAG_KPES = MAKE_TAG('K','P','E','S');
static constexpr uint32_t TAG_KPEL = MAKE_TAG('K','P','E','L');
static constexpr uint32_t TAG_KPEG = MAKE_TAG('K','P','E','G');
static constexpr uint32_t TAG_KPLN = MAKE_TAG('K','P','L','N');
static constexpr uint32_t TAG_KPLT = MAKE_TAG('K','P','L','T');
static constexpr uint32_t TAG_KPEE = MAKE_TAG('K','P','E','E');
static constexpr uint32_t TAG_KPEV = MAKE_TAG('K','P','E','V');
static constexpr uint32_t TAG_KP2S = MAKE_TAG('K','P','2','S');
static constexpr uint32_t TAG_KP2R = MAKE_TAG('K','P','2','R');
static constexpr uint32_t TAG_KP2L = MAKE_TAG('K','P','2','L');
static constexpr uint32_t TAG_KP2G = MAKE_TAG('K','P','2','G');
static constexpr uint32_t TAG_KP2E = MAKE_TAG('K','P','2','E');
static constexpr uint32_t TAG_KP2N = MAKE_TAG('K','P','2','N');
static constexpr uint32_t TAG_KP2W = MAKE_TAG('K','P','2','W');
static constexpr uint32_t TAG_KP2V = MAKE_TAG('K','P','2','V');

struct MdxReader {
    const uint8_t* d;
    size_t sz;
    size_t off = 0;

    uint32_t u32() { uint32_t v = R32(d + off); off += 4; return v; }
    int32_t s32() { int32_t v = RS32(d + off); off += 4; return v; }
    float f32() { float v = RF32(d + off); off += 4; return v; }
    uint16_t u16() { uint16_t v = uint16_t(d[off]) | (uint16_t(d[off+1]) << 8); off += 2; return v; }
    uint8_t u8() { return d[off++]; }
    uint32_t tag() {
        uint32_t v = (uint32_t(d[off]) << 24) | (uint32_t(d[off+1]) << 16) |
                     (uint32_t(d[off+2]) << 8) | uint32_t(d[off+3]);
        off += 4;
        return v;
    }
    uint32_t peekTag() const {
        return (uint32_t(d[off]) << 24) | (uint32_t(d[off+1]) << 16) |
               (uint32_t(d[off+2]) << 8) | uint32_t(d[off+3]);
    }
    void skip(size_t n) { off += n; }
    bool ok() const { return off <= sz; }

    void readFloatArray(float* out, int n) {
        for (int i = 0; i < n; i++) out[i] = f32();
    }

    std::string readStr(int maxLen) {
        std::string s;
        int read = 0;
        for (; read < maxLen && off < sz && d[off]; read++) s += (char)d[off++];
        off += maxLen - read;
        return s;
    }
};

// ---- Timeline element size based on tag ----
static int TimelineElemSize(uint32_t tag) {
    switch (tag) {
        case MAKE_TAG('K','G','T','R'): return 3; // translation
        case MAKE_TAG('K','C','T','R'): return 3; // camera translation
        case MAKE_TAG('K','T','T','R'): return 3; // camera target
        case MAKE_TAG('K','G','S','C'): return 3; // scale
        case MAKE_TAG('K','T','A','S'): return 3; // tex anim scale
        case MAKE_TAG('K','T','A','T'): return 3; // tex anim translation
        case MAKE_TAG('K','G','A','C'): return 3; // geoset anim color (BGR)
        case MAKE_TAG('K','L','A','C'): return 3; // light color
        case MAKE_TAG('K','L','B','C'): return 3; // light ambient color
        case MAKE_TAG('K','P','P','C'): return 3; // popcorn emitter color
        case MAKE_TAG('K','R','C','O'): return 3; // ribbon emitter color
        case MAKE_TAG('K','F','C','3'): return 3; // layer fresnel color
        case MAKE_TAG('K','G','R','T'): return 4; // rotation (quaternion)
        case MAKE_TAG('K','T','A','R'): return 4; // tex anim rotation
        case TAG_KEVT: return 1; // event trigger keyframes
        default: return 1;                          // scalar (float)
    }
}

static bool IsIntegerTimeline(uint32_t tag) {
    switch (tag) {
        case MAKE_TAG('K','M','T','F'): // material texture id
        case MAKE_TAG('K','R','T','X'): // ribbon texture slot
        case MAKE_TAG('K','F','T','C'): // layer fresnel team color
            return true;
        default:
            return false;
    }
}

static bool IsTangential(MdxInterpolation interp) {
    return interp == MdxInterpolation::Hermite || interp == MdxInterpolation::Bezier;
}

// ---- Read a single timeline from current offset ----
static void ReadTimeline(MdxReader& r, MdxTimeline& tl) {
    tl.tag = r.tag();
    uint32_t kfCount = r.u32();

    if (tl.tag == TAG_KEVT) {
        tl.interpolationType = MdxInterpolation::None;
        tl.globalSequenceId = r.s32();
        if (kfCount > 100000) {
            r.skip((size_t)kfCount * 4);
            return;
        }
        tl.frames.resize(kfCount);
        for (uint32_t i = 0; i < kfCount; i++) {
            tl.frames[i] = r.u32();
        }
        return;
    }

    tl.interpolationType = (MdxInterpolation)r.u32();
    tl.globalSequenceId = r.s32();

    if (kfCount == 0) return;
    if (kfCount > 100000) { r.skip((size_t)kfCount * 4 * 4); return; }

    const int elemSize = TimelineElemSize(tl.tag);
    const bool integerTimeline = IsIntegerTimeline(tl.tag);
    const bool hasTangents = IsTangential(tl.interpolationType);

    tl.frames.resize(kfCount);
    tl.values.resize(kfCount * elemSize);
    if (hasTangents) {
        tl.inTans.resize(kfCount * elemSize);
        tl.outTans.resize(kfCount * elemSize);
    }

    auto readValue = [&]() -> float {
        return integerTimeline ? (float)r.u32() : r.f32();
    };

    for (uint32_t i = 0; i < kfCount; i++) {
        tl.frames[i] = r.u32();
        for (int elem = 0; elem < elemSize; ++elem) {
            tl.values[(size_t)i * elemSize + elem] = readValue();
        }
        if (hasTangents) {
            for (int elem = 0; elem < elemSize; ++elem) {
                tl.inTans[(size_t)i * elemSize + elem] = readValue();
            }
            for (int elem = 0; elem < elemSize; ++elem) {
                tl.outTans[(size_t)i * elemSize + elem] = readValue();
            }
        }
    }
}

// ---- Read a node entry's generic data (name, objId, parentId, flags) ----
static void ReadNodeGeneric(MdxReader& r, MdxNode& node) {
    node.name = r.readStr(80);
    node.objectId = r.s32();
    node.parentId = r.s32();
    node.flags = r.u32();
}

// ---- Read timelines from current offset until a limit or non-timeline tag ----
// Note: timeline tags are valid 4-char codes; we keep reading until we hit
// an invalid tag or run out of data. MDX ensures all remaining bytes are timelines.
static void ReadTimelines(MdxReader& r, size_t entryEnd, std::vector<MdxTimeline>& out) {
    while (r.off + 16 <= entryEnd) {
        // Peek at the next tag - timeline tags start with 'K'
        uint32_t peek = r.peekTag();
        uint8_t firstByte = (uint8_t)(peek >> 24);
        if (firstByte != 'K') break;
        int es = TimelineElemSize(peek);
        if (es <= 0) break;
        MdxTimeline tl;
        ReadTimeline(r, tl);
        out.push_back(std::move(tl));
    }
    r.off = entryEnd;
}

static void ReadSizedGenericObject(MdxReader& r, size_t entryEnd, MdxNode& node,
                                   std::vector<MdxTimeline>& timelines) {
    size_t genericStart = r.off;
    uint32_t genericSize = r.u32();
    size_t genericEnd = genericStart + genericSize;
    if (genericSize < 96 || genericEnd > entryEnd) {
        genericEnd = entryEnd;
    }
    ReadNodeGeneric(r, node);
    ReadTimelines(r, genericEnd, timelines);
}

// ---- Read timelines for the old-style PRE2 entry (generic timelines come first) ----
static void ReadTimelinesWithSize(MdxReader& r, size_t entryEnd, uint32_t genericSize,
                                    std::vector<MdxTimeline>& out) {
    // genericSize includes the 4-byte size field itself
    // Generic timelines (KGTR, KGRT, KGSC) + non-generic timelines follow
    // After generic timelines, the type-specific data begins, then non-generic timelines
    // We skip type-specific data and read all timelines
    r.off = entryEnd;
}

// ---- Parse SEQS ----
static void ParseSeqs(MdxReader& r, MdxModel& out) {
    // Each sequence entry is 132 bytes
    int count = (int)((/*chunkEnd*/0) / 132); // caller must compute
    // Fixed size per entry: 80(name) + 4(start) + 4(end) + 4(moveSpeed) + 4(flags)
    //   + 4(rarity) + 4(syncPoint) + 12(min) + 12(max) + 4(boundsRadius) = 132
}

// ---- Main loading function ----
bool MdxLoad(const uint8_t* data, size_t size, MdxModel& out) {
    MdxReader r = {data, size, 0};

    if (r.tag() != T_MDLX) {
        std::cerr << "MDX: bad magic" << std::endl;
        return false;
    }

    out.version = 800;

    while (r.ok()) {
        if (r.off + 8 > r.sz) break;
        uint32_t tag = r.tag();
        uint32_t chunkSize = r.u32();
        size_t chunkEnd = r.off + chunkSize;
        if (chunkEnd > r.sz) break;

        switch (tag) {
        case T_VERS:
            out.version = (int)r.u32();
            break;

        case T_MODL: {
            out.name = r.readStr(80);
            r.skip(260); // animation file
            out.boundsMin[0] = r.f32(); out.boundsMin[1] = r.f32(); out.boundsMin[2] = r.f32();
            out.boundsMax[0] = r.f32(); out.boundsMax[1] = r.f32(); out.boundsMax[2] = r.f32();
            out.boundsRadius = r.f32();
            r.skip(4); // blendTime
            break;
        }

        // ---- SEQS ----
        case T_SEQS: {
            int count = (int)(chunkSize / 132);
            out.sequences.resize(count);
            for (int i = 0; i < count; i++) {
                auto& seq = out.sequences[i];
                seq.name = r.readStr(80);
                seq.startFrame = r.u32();
                seq.endFrame = r.u32();
                seq.moveSpeed = r.f32();
                seq.flags = r.u32();
                seq.rarity = r.f32();
                seq.syncPoint = r.u32();
                r.readFloatArray(seq.boundsMin, 3);
                r.readFloatArray(seq.boundsMax, 3);
                seq.boundsRadius = r.f32();
            }
            break;
        }

        // ---- GLBS ----
        case T_GLBS: {
            int count = (int)(chunkSize / 4);
            out.globalSequences.resize(count);
            for (int i = 0; i < count; i++)
                out.globalSequences[i] = r.u32();
            break;
        }

        // ---- TEXS ----
        case T_TEXS: {
            int count = (int)(chunkSize / 268);
            out.textures.resize(count);
            for (int i = 0; i < count; i++) {
                out.textures[i].replaceableId = (int)r.u32();
                out.textures[i].path = r.readStr(260);
                r.skip(4); // wrapFlags
            }
            break;
        }

        // ---- TXAN (texture animations) ----
        case T_TXAN: {
            while (r.off + 4 <= chunkEnd) {
                size_t entryStart = r.off;
                uint32_t entrySize = r.u32();
                if (entrySize < 4 || entryStart + entrySize > chunkEnd) break;
                size_t entryEndLocal = entryStart + entrySize;

                MdxTexAnim ta;
                ReadTimelines(r, entryEndLocal, ta.timelines);
                out.texAnims.push_back(std::move(ta));
            }
            r.off = chunkEnd;
            break;
        }

        // ---- MTLS ----
        case T_MTLS: {
            size_t totalRead = 0;
            while (totalRead < chunkSize) {
                size_t matSize = r.u32(); // total material byte length
                MdxMaterial mat;
                mat.priorityPlane = (int)r.s32();
                mat.flags = r.s32();

                r.u32(); // LAYS tag (big-endian)
                int layerCount = (int)r.u32();
                mat.layers.resize(layerCount);
                for (int li = 0; li < layerCount; li++) {
                    auto& layer = mat.layers[li];
                    size_t layerSize = r.u32();
                    size_t dataStart = r.off;
                    layer.filterMode = (int)r.s32();
                    layer.flags = r.s32();
                    layer.textureId = (int)r.s32();
                    layer.textureAnimationId = r.s32();
                    layer.coordId = r.s32();
                    layer.alpha = r.f32();
                    // Remaining bytes in layer may contain KMTA (alpha) and KMTF (texture swap) timelines
                    size_t layerDataEnd = dataStart + layerSize - 4;
                    ReadTimelines(r, layerDataEnd, layer.timelines);
                    r.off = layerDataEnd;
                }

                totalRead += matSize ? matSize : (r.off - (totalRead + 8));
                out.materials.push_back(mat);
            }
            break;
        }

        // ---- GEOS ----
        case T_GEOS: {
            while (r.off + 4 <= chunkEnd) {
                size_t geosetStart = r.off;
                uint32_t geosetSize = r.u32();
                if (geosetSize < 4 || geosetStart + geosetSize > chunkEnd) break;
                size_t geosetEnd = geosetStart + geosetSize;
                MdxGeoset geo;

                // VRTX
                if (r.peekTag() == T_VRTX) {
                    r.tag(); int vc = (int)r.u32();
                    geo.vertices.resize(vc * 3);
                    for (int i = 0; i < vc * 3; i++) geo.vertices[i] = r.f32();
                }

                // NRMS
                if (r.off + 4 <= geosetEnd && r.peekTag() == T_NRMS) {
                    r.tag(); // consume NRMS
                    int nc = (int)r.u32();
                    geo.normals.resize(nc * 3);
                    for (int i = 0; i < nc * 3; i++) geo.normals[i] = r.f32();
                }

                // PTYP
                std::vector<uint32_t> faceTypes;
                if (r.off + 4 <= geosetEnd && r.peekTag() == T_PTYP) {
                    r.tag(); int fc = (int)r.u32();
                    faceTypes.resize(fc);
                    for (int i = 0; i < fc; i++) faceTypes[i] = r.u32();
                }

                // PCNT
                std::vector<uint32_t> faceCounts;
                if (r.off + 4 <= geosetEnd && r.peekTag() == T_PCNT) {
                    r.tag(); int fc = (int)r.u32();
                    faceCounts.resize(fc);
                    for (int i = 0; i < fc; i++) faceCounts[i] = r.u32();
                }

                // PVTX
                std::vector<uint16_t> rawFaces;
                if (r.off + 4 <= geosetEnd && r.peekTag() == T_PVTX) {
                    r.tag(); int fcnt = (int)r.u32();
                    rawFaces.resize(fcnt);
                    for (int i = 0; i < fcnt; i++) rawFaces[i] = r.u16();
                }

                // Warcraft III geosets are consumed by the reference implementation as a
                // flat face index buffer. Rebuilding triangles here was dropping or
                // rearranging parts of some models (trees/units appearing like only half
                // the shell existed). Keep the raw face order.
                geo.indices.reserve(rawFaces.size());
                for (uint16_t face : rawFaces) {
                    geo.indices.push_back(face);
                }

                // GNDX
                if (r.off + 4 <= geosetEnd && r.peekTag() == T_GNDX) {
                    r.tag(); int gcount = (int)r.u32();
                    geo.vertexGroups.resize(gcount);
                    for (int i = 0; i < gcount; i++) geo.vertexGroups[i] = r.u8();
                }

                // MTGC
                if (r.off + 4 <= geosetEnd && r.peekTag() == T_MTGC) {
                    r.tag(); int mcount = (int)r.u32();
                    geo.matrixGroups.resize(mcount);
                    for (int i = 0; i < mcount; i++) geo.matrixGroups[i] = r.u32();
                }

                // MATS - store matrix indices for skinning
                if (r.off + 4 <= geosetEnd && r.peekTag() == T_MATS) {
                    r.tag(); int mic = (int)r.u32();
                    geo.matrixIndices.resize(mic);
                    for (int i = 0; i < mic; i++) geo.matrixIndices[i] = r.u32();
                }

                geo.materialId = (int)r.u32();
                r.skip(8); // selection group, selection flags

                if (out.version > 800) {
                    r.skip(84); // lod + lodName
                }

                // Extent
                r.skip(12); // min
                r.skip(12); // max
                r.skip(4);  // boundsRadius

                int numSeqExt = (int)r.u32();
                r.skip(numSeqExt * 28);

                // Version > 800 may have TANG/SKIN/UVAS
                if (r.off + 4 <= geosetEnd) {
                    uint32_t nextTag = r.tag();
                    if (nextTag == T_TANG) {
                        int tc = (int)r.u32();
                        r.skip(tc * 16);
                        if (r.off + 4 <= geosetEnd) nextTag = r.tag();
                    }
                    if (nextTag == T_SKIN) {
                        int sc = (int)r.u32();
                        // Skin data: uint16 per vertex (boneIndex<<8 | weight)
                        int skinCount = sc / 2;
                        if (sc > 0 && skinCount <= (int)geo.vertices.size() / 3 * 4) {
                            geo.skin.resize(skinCount);
                            for (int si = 0; si < skinCount; si++)
                                geo.skin[si] = r.u16();
                        } else {
                            r.skip(sc);
                        }
                        if (r.off + 4 <= geosetEnd) nextTag = r.tag();
                    }
                    if (nextTag == T_UVAS) {
                        int numUV = (int)r.u32();
                        geo.uvSets.clear();
                        geo.uvSets.resize(numUV);
                        for (int uvi = 0; uvi < numUV; uvi++) {
                            if (r.off + 4 <= geosetEnd && r.tag() == T_UVBS) {
                                int uvcount = (int)r.u32();
                                auto& uvSet = geo.uvSets[uvi];
                                uvSet.resize(uvcount * 2);
                                for (int k = 0; k < uvcount * 2; k++) uvSet[k] = r.f32();
                            }
                        }
                        if (!geo.uvSets.empty()) {
                            geo.uvs = geo.uvSets[0];
                        }
                    }
                }

                if (!geo.vertices.empty() && !geo.indices.empty())
                    out.geosets.push_back(geo);

                r.off = geosetEnd;
            }
            break;
        }

        // ---- GEOA (geoset animations) ----
        case T_GEOA: {
            while (r.off + 4 <= chunkEnd) {
                size_t entryStart = r.off;
                uint32_t entrySize = r.u32();
                if (entrySize < 4 || entryStart + entrySize > chunkEnd) break;
                size_t entryEndLocal = entryStart + entrySize;

                MdxGeosetAnim ga;
                ga.alpha = r.f32();
                ga.flags = r.u32();
                r.readFloatArray(ga.color.data(), 3);
                ga.geosetId = r.s32();

                // Remaining bytes are timelines (KGAO, KGAC)
                ReadTimelines(r, entryEndLocal, ga.timelines);
                out.geosetAnims.push_back(std::move(ga));
            }
            r.off = chunkEnd;
            break;
        }

        // ---- PIVT ----
        case T_PIVT: {
            int count = (int)(chunkSize / 12);
            out.pivotPoints.resize(count);
            for (int i = 0; i < count; i++) {
                out.pivotPoints[i][0] = r.f32();
                out.pivotPoints[i][1] = r.f32();
                out.pivotPoints[i][2] = r.f32();
            }
            break;
        }

        // ---- BONE ----
        case T_BONE: {
            while (r.off + 4 <= chunkEnd) {
                size_t entryStart = r.off;
                uint32_t genericSize = r.u32();
                if (genericSize < 96) break;
                size_t genericEnd = entryStart + genericSize;
                size_t objectEnd = genericEnd + 8;
                if (objectEnd > chunkEnd) break;
                r.off = entryStart;
                MdxNode node;
                ReadSizedGenericObject(r, genericEnd, node, node.timelines);
                node.geosetId = r.s32();
                node.geosetAnimationId = r.s32();
                out.bones.push_back(std::move(node));
                r.off = objectEnd;
            }
            r.off = chunkEnd;
            break;
        }

        // ---- HELP ----
        case T_HELP: {
            while (r.off + 4 <= chunkEnd) {
                size_t entryStart = r.off;
                uint32_t genericSize = r.u32();
                if (genericSize < 96) break;
                size_t genericEnd = entryStart + genericSize;
                if (genericEnd > chunkEnd) break;
                r.off = entryStart;
                MdxNode node;
                ReadSizedGenericObject(r, genericEnd, node, node.timelines);
                out.helpers.push_back(std::move(node));
                r.off = genericEnd;
            }
            r.off = chunkEnd;
            break;
        }

        // ---- ATCH (attachments) ----
        case T_ATCH: {
            while (r.off + 4 <= chunkEnd) {
                size_t entryStart = r.off;
                uint32_t entrySize = r.u32();
                if (entrySize < 4 || entryStart + entrySize > chunkEnd) break;
                size_t entryEndLocal = entryStart + entrySize;

                MdxAttachment att;
                MdxNode genericNode;
                ReadSizedGenericObject(r, entryEndLocal, genericNode, att.timelines);
                att.name = genericNode.name;
                att.objectId = genericNode.objectId;
                att.parentId = genericNode.parentId;
                att.flags = genericNode.flags;
                att.path = r.readStr(260);
                att.attachmentId = r.s32();
                ReadTimelines(r, entryEndLocal, att.timelines);
                out.attachments.push_back(std::move(att));
            }
            r.off = chunkEnd;
            break;
        }

        // ---- LITE (lights) ----
        case T_LITE: {
            while (r.off + 4 <= chunkEnd) {
                size_t entryStart = r.off;
                uint32_t entrySize = r.u32();
                if (entrySize < 4 || entryStart + entrySize > chunkEnd) break;
                size_t entryEndLocal = entryStart + entrySize;

                MdxLight lt;
                MdxNode genericNode;
                ReadSizedGenericObject(r, entryEndLocal, genericNode, lt.timelines);
                lt.name = genericNode.name;
                lt.objectId = genericNode.objectId;
                lt.parentId = genericNode.parentId;
                lt.flags = genericNode.flags;
                lt.textureId = r.s32();
                lt.attenuationStart = r.f32();
                lt.attenuationEnd = r.f32();
                r.readFloatArray(lt.color, 3);
                lt.intensity = r.f32();
                r.readFloatArray(lt.ambientColor, 3);
                lt.ambientIntensity = r.f32();
                ReadTimelines(r, entryEndLocal, lt.timelines);
                out.lights.push_back(std::move(lt));
            }
            r.off = chunkEnd;
            break;
        }

        // ---- CAMS (cameras) ----
        case T_CAMS: {
            while (r.off + 4 <= chunkEnd) {
                size_t entryStart = r.off;
                uint32_t entrySize = r.u32();
                if (entrySize < 4 || entryStart + entrySize > chunkEnd) break;
                size_t entryEndLocal = entryStart + entrySize;

                MdxCamera cam;
                cam.name = r.readStr(80);
                r.readFloatArray(cam.pos, 3);
                cam.fov = r.f32();
                cam.farClip = r.f32();
                cam.nearClip = r.f32();
                r.readFloatArray(cam.target, 3);
                ReadTimelines(r, entryEndLocal, cam.timelines);
                out.cameras.push_back(std::move(cam));
            }
            r.off = chunkEnd;
            break;
        }

        // ---- PREM (ParticleEmitter1) ----
        case T_PREM: {
            while (r.off + 4 <= chunkEnd) {
                size_t entryStart = r.off;
                uint32_t entrySize = r.u32();
                if (entrySize < 100 || entryStart + entrySize > chunkEnd) break;
                size_t entryEndLocal = entryStart + entrySize;

                MdxParticleEmitter1 em;
                MdxNode genericNode;
                ReadSizedGenericObject(r, entryEndLocal, genericNode, em.timelines);
                em.name = genericNode.name;
                em.objectId = genericNode.objectId;
                em.parentId = genericNode.parentId;
                em.flags = genericNode.flags;
                em.emissionRate = r.f32();
                em.gravity = r.f32();
                em.longitude = r.f32();
                em.latitude = r.f32();
                em.internalModelPath = r.readStr(260);
                em.lifeSpan = r.f32();
                em.speed = r.f32();
                ReadTimelines(r, entryEndLocal, em.timelines);
                r.off = entryEndLocal;
                out.particleEmitters1.push_back(std::move(em));
            }
            r.off = chunkEnd;
            break;
        }

        // ---- PRE2 (ParticleEmitter2) ----
        case T_PRE2: {
            while (r.off + 8 <= chunkEnd) {
                size_t start = r.off;
                uint32_t outerSize = r.u32();
                if (outerSize < 100 || start + outerSize > chunkEnd) break;
                size_t entryEndLocal = start + outerSize;

                MdxParticleEmitter2 em;
                MdxNode genericNode;
                ReadSizedGenericObject(r, entryEndLocal, genericNode, em.timelines);
                em.name = genericNode.name;
                em.objectId = genericNode.objectId;
                em.parentId = genericNode.parentId;
                em.flags = (int)genericNode.flags;

                em.speed = r.f32();
                em.variation = r.f32();
                em.latitude = r.f32();
                em.gravity = r.f32();
                em.lifeSpan = r.f32();
                em.emissionRate = r.f32();
                em.length = r.f32();
                em.width = r.f32();
                em.filterMode = (int)r.s32();
                em.rows = r.u32();
                em.columns = r.u32();
                em.headOrTail = (int)r.s32();
                em.tailLength = r.f32();
                em.timeMiddle = r.f32();
                r.readFloatArray(em.segmentColors[0], 3);
                r.readFloatArray(em.segmentColors[1], 3);
                r.readFloatArray(em.segmentColors[2], 3);
                for (int i = 0; i < 3; i++) em.segmentAlphas[i] = r.u8();
                r.readFloatArray(em.segmentScaling, 3);
                for (int i = 0; i < 3; i++) em.headIntervals[0][i] = r.u32();
                for (int i = 0; i < 3; i++) em.headIntervals[1][i] = r.u32();
                for (int i = 0; i < 3; i++) em.tailIntervals[0][i] = r.u32();
                for (int i = 0; i < 3; i++) em.tailIntervals[1][i] = r.u32();
                em.textureId = r.s32();
                em.squirt = r.u32();
                em.priorityPlane = r.s32();
                em.replaceableId = r.u32();

                // Read non-generic timelines (KPEE, KPEV, KPEG, KPLN, KPLT, KPLE, KPLS)
                ReadTimelines(r, entryEndLocal, em.timelines);
                r.off = entryEndLocal;

                out.particleEmitters2.push_back(em);
            }
            r.off = chunkEnd;
            break;
        }

        // ---- RIBB (RibbonEmitter) ----
        case T_RIBB: {
            while (r.off + 4 <= chunkEnd) {
                size_t entryStart = r.off;
                uint32_t entrySize = r.u32();
                if (entrySize < 64 || entryStart + entrySize > chunkEnd) break;
                size_t entryEndLocal = entryStart + entrySize;

                MdxRibbonEmitter rb;
                MdxNode genericNode;
                ReadSizedGenericObject(r, entryEndLocal, genericNode, rb.timelines);
                rb.name = genericNode.name;
                rb.objectId = genericNode.objectId;
                rb.parentId = genericNode.parentId;
                rb.flags = genericNode.flags;
                rb.heightAbove = r.f32();
                rb.heightBelow = r.f32();
                rb.alpha = r.f32();
                r.readFloatArray(rb.color, 3);
                rb.lifeSpan = r.f32();
                rb.textureSlot = (int)r.u32();
                rb.emissionRate = (float)r.u32();
                rb.rows = r.u32();
                rb.columns = r.u32();
                rb.priorityPlane = r.s32();
                rb.gravity = r.f32();
                ReadTimelines(r, entryEndLocal, rb.timelines);
                r.off = entryEndLocal;
                out.ribbonEmitters.push_back(std::move(rb));
            }
            r.off = chunkEnd;
            break;
        }

        // ---- EVTS (Event objects SPL/SPN/UBR/SND) ----
        case T_EVTS: {
            while (r.off + 4 <= chunkEnd) {
                size_t entryStart = r.off;
                uint32_t genericSize = R32(r.d + r.off);
                if (genericSize < 96 || entryStart + genericSize > chunkEnd) break;

                MdxEventObject ev;
                MdxNode genericNode;
                ReadSizedGenericObject(r, chunkEnd, genericNode, ev.timelines);
                ev.name = genericNode.name;
                ev.objectId = genericNode.objectId;
                ev.parentId = genericNode.parentId;
                ev.flags = genericNode.flags;
                // Parse type from name prefix
                if (ev.name.size() >= 3) {
                    ev.type = ev.name.substr(0, 3);
                    if (ev.name.size() > 4)
                        ev.id = ev.name.substr(4);
                }
                if (r.off + 12 <= chunkEnd && r.peekTag() == TAG_KEVT) {
                    MdxTimeline eventTrack;
                    ReadTimeline(r, eventTrack);
                    ev.timelines.push_back(std::move(eventTrack));
                }
                out.eventObjects.push_back(std::move(ev));
            }
            r.off = chunkEnd;
            break;
        }

        // ---- CLID (collision shapes) ----
        case T_CLID: {
            int count = (int)(chunkSize / 4);
            out.collisionShapes.resize(count);
            for (int i = 0; i < count; i++)
                out.collisionShapes[i] = r.u32();
            r.off = chunkEnd;
            break;
        }

        default:
            r.off = chunkEnd;
            break;
        }
    }

    // Log summary
    std::cout << "MDX loaded: " << out.name
              << " (seqs=" << out.sequences.size()
              << ", bones=" << out.bones.size()
              << ", geosets=" << out.geosets.size()
              << ", textures=" << out.textures.size()
              << ", pre2=" << out.particleEmitters2.size()
              << ")" << std::endl;

    return true;
}

// ---- Keyframe sampling implementation ----
struct SampleHelper {
    const MdxTimeline* tl;
    int elemCount;

    static int findKey(const std::vector<uint32_t>& frames, uint32_t frame) {
        if (frames.empty()) return -1;
        if (frame <= frames[0]) return 0;
        if (frame >= frames.back()) return (int)frames.size() - 2;
        // Binary search
        int lo = 0, hi = (int)frames.size() - 1;
        while (lo + 1 < hi) {
            int mid = (lo + hi) / 2;
            if (frames[mid] <= frame) lo = mid;
            else hi = mid;
        }
        return lo;
    }

    static float lerp(float a, float b, float t) { return a + (b - a) * t; }

    static float hermite(float a, float outTan, float b, float inTan, float t) {
        float t2 = t * t, t3 = t2 * t;
        float h1 = 2*t3 - 3*t2 + 1;
        float h2 = -2*t3 + 3*t2;
        float h3 = t3 - 2*t2 + t;
        float h4 = t3 - t2;
        return h1*a + h2*b + h3*outTan + h4*inTan;
    }

    static float bezier(float a, float outTan, float b, float inTan, float t) {
        float t2 = t * t, t3 = t2 * t;
        float mt = 1 - t, mt2 = mt*mt, mt3 = mt2*mt;
        return mt3*a + 3*mt2*t*outTan + 3*mt*t2*inTan + t3*b;
    }

    static float interpValue(const float* values, int elemIdx, int elemCount,
                              int keyA, int keyB, float t,
                              MdxInterpolation interp,
                              const float* inTans, const float* outTans,
                              bool hasTangents) {
        float a = values[keyA * elemCount + elemIdx];
        float b = values[keyB * elemCount + elemIdx];

        switch (interp) {
        case MdxInterpolation::None:
            return a;
        case MdxInterpolation::Linear:
            return lerp(a, b, t);
        case MdxInterpolation::Hermite: {
            if (!hasTangents) {
                return lerp(a, b, t);
            }
            float outTan = outTans[keyA * elemCount + elemIdx];
            float inTan = inTans[keyB * elemCount + elemIdx];
            return hermite(a, outTan, b, inTan, t);
        }
        case MdxInterpolation::Bezier: {
            if (!hasTangents) {
                return lerp(a, b, t);
            }
            float outTan = outTans[keyA * elemCount + elemIdx];
            float inTan = inTans[keyB * elemCount + elemIdx];
            return bezier(a, outTan, b, inTan, t);
        }
        default:
            return a;
        }
    }
};

bool MdxSampleTimeline(const MdxTimeline& tl, uint32_t frame, float* out, int elemCount, float defaultVal) {
    if (!out || tl.frames.empty() || elemCount < 1) return false;

    const size_t keyCount = tl.frames.size();
    const size_t valuesNeeded = keyCount * (size_t)elemCount;
    if (tl.values.size() < valuesNeeded) {
        for (int i = 0; i < elemCount; i++) out[i] = defaultVal;
        return false;
    }

    if (frame <= tl.frames.front() || keyCount == 1) {
        for (int i = 0; i < elemCount; i++) {
            out[i] = tl.values[(size_t)i];
        }
        return true;
    }
    if (frame >= tl.frames.back()) {
        const size_t lastValue = (keyCount - 1u) * (size_t)elemCount;
        for (int i = 0; i < elemCount; i++) {
            out[i] = tl.values[lastValue + (size_t)i];
        }
        return true;
    }

    int keyA = SampleHelper::findKey(tl.frames, frame);
    if (keyA < 0) {
        for (int i = 0; i < elemCount; i++) out[i] = tl.values[i];
        return true;
    }
    int keyB = keyA + 1;
    if (keyB >= (int)tl.frames.size()) {
        keyB = keyA;
        keyA = (keyB > 0) ? keyB - 1 : keyB;
    }

    float t = 0;
    if (keyA != keyB && tl.frames[keyB] != tl.frames[keyA]) {
        t = (float)(frame - tl.frames[keyA]) / (float)(tl.frames[keyB] - tl.frames[keyA]);
    }

    const bool needsTangents = tl.interpolationType == MdxInterpolation::Hermite
        || tl.interpolationType == MdxInterpolation::Bezier;
    const bool hasTangents = !needsTangents
        || (tl.inTans.size() >= valuesNeeded && tl.outTans.size() >= valuesNeeded);

    for (int i = 0; i < elemCount; i++) {
        out[i] = SampleHelper::interpValue(
            tl.values.data(), i, elemCount, keyA, keyB, t,
            tl.interpolationType,
            tl.inTans.empty() ? nullptr : tl.inTans.data(),
            tl.outTans.empty() ? nullptr : tl.outTans.data(),
            hasTangents);
    }
    return true;
}

static bool MdxSampleGlobalTimelineLooped(const MdxTimeline& tl, uint32_t frame, uint32_t duration,
                                          float* out, int elemCount, float defaultVal) {
    if (!out || tl.frames.empty() || elemCount < 1 || duration == 0) {
        return false;
    }

    const size_t keyCount = tl.frames.size();
    const size_t valuesNeeded = keyCount * (size_t)elemCount;
    if (tl.values.size() < valuesNeeded) {
        for (int i = 0; i < elemCount; i++) out[i] = defaultVal;
        return false;
    }
    if (keyCount == 1) {
        for (int i = 0; i < elemCount; i++) out[i] = tl.values[(size_t)i];
        return true;
    }

    const int lastIndex = (int)keyCount - 1;
    int keyA = lastIndex;
    int keyB = 0;
    if (frame >= tl.frames.front() && frame < tl.frames.back()) {
        for (int i = 1; i <= lastIndex; ++i) {
            if (tl.frames[(size_t)i] > frame) {
                keyA = i - 1;
                keyB = i;
                break;
            }
        }
    }

    uint32_t startFrame = tl.frames[(size_t)keyA];
    const uint32_t endFrame = tl.frames[(size_t)keyB];
    int64_t span = (int64_t)endFrame - (int64_t)startFrame;
    if (span < 0) {
        span += duration;
        if (frame < startFrame) {
            startFrame = endFrame;
        }
    }
    const float t = span == 0
        ? 0.0f
        : (float)((int64_t)frame - (int64_t)startFrame) / (float)span;

    const bool needsTangents = tl.interpolationType == MdxInterpolation::Hermite
        || tl.interpolationType == MdxInterpolation::Bezier;
    const bool hasTangents = !needsTangents
        || (tl.inTans.size() >= valuesNeeded && tl.outTans.size() >= valuesNeeded);

    for (int i = 0; i < elemCount; i++) {
        out[i] = SampleHelper::interpValue(
            tl.values.data(), i, elemCount, keyA, keyB, t,
            tl.interpolationType,
            tl.inTans.empty() ? nullptr : tl.inTans.data(),
            tl.outTans.empty() ? nullptr : tl.outTans.data(),
            hasTangents);
    }
    return true;
}

static const MdxSequence* MdxResolveSequence(const MdxModel* model, int sequenceIndex) {
    if (!model || sequenceIndex < 0 || sequenceIndex >= (int)model->sequences.size()) {
        return nullptr;
    }
    return &model->sequences[(size_t)sequenceIndex];
}

static bool MdxBuildSequenceTimeline(const MdxTimeline& source,
                                     const MdxSequence* sequence,
                                     int elemCount,
                                     MdxTimeline& out) {
    if (!sequence || elemCount < 1) {
        return false;
    }

    out.tag = source.tag;
    out.interpolationType = source.interpolationType;
    out.globalSequenceId = source.globalSequenceId;

    const bool needsTangents = source.interpolationType == MdxInterpolation::Hermite
        || source.interpolationType == MdxInterpolation::Bezier;
    for (size_t i = 0; i < source.frames.size(); ++i) {
        const uint32_t keyFrame = source.frames[i];
        if (keyFrame < sequence->startFrame || keyFrame > sequence->endFrame) {
            continue;
        }

        const size_t valueOffset = i * (size_t)elemCount;
        if (source.values.size() < valueOffset + (size_t)elemCount) {
            continue;
        }

        out.frames.push_back(keyFrame);
        for (int elem = 0; elem < elemCount; ++elem) {
            out.values.push_back(source.values[valueOffset + (size_t)elem]);
        }

        if (needsTangents
            && source.inTans.size() >= valueOffset + (size_t)elemCount
            && source.outTans.size() >= valueOffset + (size_t)elemCount) {
            for (int elem = 0; elem < elemCount; ++elem) {
                out.inTans.push_back(source.inTans[valueOffset + (size_t)elem]);
                out.outTans.push_back(source.outTans[valueOffset + (size_t)elem]);
            }
        }
    }

    return !out.frames.empty();
}

static bool MdxSampleTimelineForModel(const MdxTimeline& tl, const MdxModel* model,
                                      uint32_t frame, float elapsedMs,
                                      float* out, int elemCount, float defaultVal,
                                      int sequenceIndex) {
    if (tl.globalSequenceId >= 0 && model
        && tl.globalSequenceId < (int)model->globalSequences.size()) {
        const uint32_t duration = model->globalSequences[(size_t)tl.globalSequenceId];
        if (duration > 0) {
            const uint32_t globalFrame = (uint32_t)std::max(0.0f, elapsedMs) % duration;
            return MdxSampleGlobalTimelineLooped(tl, globalFrame, duration, out, elemCount, defaultVal);
        }
    }

    if (const MdxSequence* sequence = MdxResolveSequence(model, sequenceIndex)) {
        MdxTimeline sequenceTimeline;
        if (!MdxBuildSequenceTimeline(tl, sequence, elemCount, sequenceTimeline)) {
            if (out) {
                for (int i = 0; i < elemCount; ++i) {
                    out[i] = defaultVal;
                }
            }
            return false;
        }
        return MdxSampleTimeline(sequenceTimeline, frame, out, elemCount, defaultVal);
    }

    return MdxSampleTimeline(tl, frame, out, elemCount, defaultVal);
}

static int MdxTimelineKeyAtOrBeforeFrame(const MdxTimeline& tl, const MdxModel* model,
                                         uint32_t frame, float elapsedMs,
                                         int sequenceIndex) {
    if (tl.frames.empty()) {
        return -1;
    }
    uint32_t sampleFrame = frame;
    const bool global = tl.globalSequenceId >= 0 && model
        && tl.globalSequenceId < (int)model->globalSequences.size()
        && model->globalSequences[(size_t)tl.globalSequenceId] > 0;
    if (global) {
        const uint32_t duration = model->globalSequences[(size_t)tl.globalSequenceId];
        sampleFrame = (uint32_t)std::max(0.0f, elapsedMs) % duration;
    }

    const MdxSequence* sequence = global ? nullptr : MdxResolveSequence(model, sequenceIndex);
    if (sequence) {
        int firstInSequence = -1;
        int best = -1;
        for (int i = 0; i < (int)tl.frames.size(); ++i) {
            const uint32_t keyFrame = tl.frames[(size_t)i];
            if (keyFrame < sequence->startFrame || keyFrame > sequence->endFrame) {
                continue;
            }
            if (firstInSequence < 0) {
                firstInSequence = i;
            }
            if (keyFrame <= sampleFrame) {
                best = i;
            }
        }
        return best >= 0 ? best : firstInSequence;
    }

    if (sampleFrame < tl.frames.front()) {
        return global ? (int)tl.frames.size() - 1 : 0;
    }
    for (int i = (int)tl.frames.size() - 1; i >= 0; --i) {
        if (sampleFrame >= tl.frames[(size_t)i]) {
            return i;
        }
    }
    return 0;
}

static bool MdxSampleSteppedScalarTimelineForModel(const MdxTimeline& tl, const MdxModel* model,
                                                   uint32_t frame, float elapsedMs,
                                                   float& out, float defaultValue,
                                                   int sequenceIndex) {
    const int key = MdxTimelineKeyAtOrBeforeFrame(tl, model, frame, elapsedMs, sequenceIndex);
    if (key < 0 || (size_t)key >= tl.values.size()) {
        out = defaultValue;
        return false;
    }
    out = tl.values[(size_t)key];
    return true;
}

bool MdxSampleParticleEmitter1(const MdxParticleEmitter1& emitter,
                               const MdxModel* model,
                               uint32_t frame,
                               float elapsedMs,
                               MdxParticleEmitter1Sample& out,
                               int sequenceIndex) {
    out.emissionRate = emitter.emissionRate;
    out.gravity = emitter.gravity;
    out.longitude = emitter.longitude;
    out.latitude = emitter.latitude;
    out.lifeSpan = emitter.lifeSpan;
    out.speed = emitter.speed;
    out.visibility = 1.0f;

    auto sampleScalar = [&](uint32_t tag, float defaultValue, float& value) {
        for (const MdxTimeline& timeline : emitter.timelines) {
            if (timeline.tag != tag) {
                continue;
            }
            float sampled = defaultValue;
            if (MdxSampleTimelineForModel(timeline, model, frame, elapsedMs, &sampled, 1,
                                          defaultValue, sequenceIndex)) {
                value = sampled;
                return true;
            }
            return false;
        }
        return false;
    };

    auto sampleSteppedScalar = [&](uint32_t tag, float defaultValue, float& value) {
        for (const MdxTimeline& timeline : emitter.timelines) {
            if (timeline.tag != tag) {
                continue;
            }
            float sampled = defaultValue;
            if (MdxSampleSteppedScalarTimelineForModel(timeline, model, frame, elapsedMs,
                                                       sampled, defaultValue, sequenceIndex)) {
                value = sampled;
                return true;
            }
            return false;
        }
        return false;
    };

    bool sampledAny = false;
    sampledAny = sampleScalar(TAG_KPEE, emitter.emissionRate, out.emissionRate) || sampledAny;
    sampledAny = sampleScalar(TAG_KPEG, emitter.gravity, out.gravity) || sampledAny;
    sampledAny = sampleScalar(TAG_KPLN, emitter.longitude, out.longitude) || sampledAny;
    sampledAny = sampleScalar(TAG_KPLT, emitter.latitude, out.latitude) || sampledAny;
    sampledAny = sampleScalar(TAG_KPEL, emitter.lifeSpan, out.lifeSpan) || sampledAny;
    sampledAny = sampleScalar(TAG_KPES, emitter.speed, out.speed) || sampledAny;
    sampledAny = sampleSteppedScalar(TAG_KPEV, 1.0f, out.visibility) || sampledAny;
    return sampledAny;
}

bool MdxSampleParticleEmitter2(const MdxParticleEmitter2& emitter,
                               const MdxModel* model,
                               uint32_t frame,
                               float elapsedMs,
                               MdxParticleEmitter2Sample& out,
                               int sequenceIndex) {
    out.speed = emitter.speed;
    out.variation = emitter.variation;
    out.latitude = emitter.latitude;
    out.gravity = emitter.gravity;
    out.emissionRate = emitter.emissionRate;
    out.length = emitter.length;
    out.width = emitter.width;
    out.visibility = 1.0f;
    out.emissionRateKey = -1;

    auto sampleScalar = [&](uint32_t tag, float defaultValue, float& value) {
        for (const MdxTimeline& timeline : emitter.timelines) {
            if (timeline.tag != tag) {
                continue;
            }
            float sampled = defaultValue;
            if (MdxSampleTimelineForModel(timeline, model, frame, elapsedMs, &sampled, 1,
                                          defaultValue, sequenceIndex)) {
                value = sampled;
                return true;
            }
            return false;
        }
        return false;
    };

    auto sampleSteppedScalar = [&](uint32_t tag, float defaultValue, float& value) {
        for (const MdxTimeline& timeline : emitter.timelines) {
            if (timeline.tag != tag) {
                continue;
            }
            float sampled = defaultValue;
            if (MdxSampleSteppedScalarTimelineForModel(timeline, model, frame, elapsedMs,
                                                       sampled, defaultValue, sequenceIndex)) {
                value = sampled;
                return true;
            }
            return false;
        }
        return false;
    };

    bool sampledAny = false;
    sampledAny = sampleScalar(TAG_KP2S, emitter.speed, out.speed) || sampledAny;
    sampledAny = sampleScalar(TAG_KP2R, emitter.variation, out.variation) || sampledAny;
    sampledAny = sampleScalar(TAG_KP2L, emitter.latitude, out.latitude) || sampledAny;
    sampledAny = sampleScalar(TAG_KP2G, emitter.gravity, out.gravity) || sampledAny;
    sampledAny = sampleScalar(TAG_KP2E, emitter.emissionRate, out.emissionRate) || sampledAny;
    sampledAny = sampleScalar(TAG_KP2N, emitter.length, out.length) || sampledAny;
    sampledAny = sampleScalar(TAG_KP2W, emitter.width, out.width) || sampledAny;
    sampledAny = sampleSteppedScalar(TAG_KP2V, 1.0f, out.visibility) || sampledAny;

    for (const MdxTimeline& timeline : emitter.timelines) {
        if (timeline.tag == TAG_KP2E) {
            out.emissionRateKey = MdxTimelineKeyAtOrBeforeFrame(timeline, model, frame, elapsedMs, sequenceIndex);
            break;
        }
    }
    return sampledAny;
}

void MdxConvertAnimatedColorBgrToRgb(const float bgr[3], float rgb[3]) {
    if (!rgb) {
        return;
    }
    if (!bgr) {
        rgb[0] = 1.0f;
        rgb[1] = 1.0f;
        rgb[2] = 1.0f;
        return;
    }
    rgb[0] = bgr[2];
    rgb[1] = bgr[1];
    rgb[2] = bgr[0];
}
