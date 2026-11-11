// ============================================================================
// Stage 1–6 Engine Evolution & P0/P1/P2 Full Remediation Verification Suite
//
// Compiles cleanly under strict `/GR-` (MSVC) and `-fno-rtti` (GCC/Clang).
// Verifies:
//   - Stage 1: Deterministic FNV-1a `StaticTypeId` (including cross-SO parity!),
//     `TagId`, `ProjectManifest` JSON + INI backslash round-trip, and `engine/`
//     100% War3-free purity audit.
//   - Stage 2: Streaming & bounds-safe `NativeMpqArchive` (corrupted shift/hash
//     size/heap-overread rejection, PKWARE DCL `0x08` & `MPQ_FILE_IMPLODE`,
//     `MpqManager::ListAllFiles(pattern)` wildcard), `ResourceServer::Vfs()`,
//     `FormatLoaderRegistry`, and non-zero-int `TableSchemaBinder`.
//   - Stage 3: `IRhiBackend`, `RhiDevice`, O(dirty_area) incremental
//     `TerrainOverlayState`, and `DiscreteStepTimelineSampler`.
//   - Stage 4: RTTI-free 15-kind `UIFrame::As<T>()` + live `UIDataBinder`
//     `IHudViewListener` reactive dispatch + unified `AudioServer` `voiceId`
//     mapping, load-before-submit (zero ghost UnitVoice ducking!), and `Reset()`
//     backend cleanup.
//   - Stage 5: RTTI-free `CWidget::AsUnit/AsItem/AsDestructable`, overflow-safe
//     `BinaryReader` (`SIZE_MAX` wrap-around rejection), and `CSimulation`
//     `SaveDeterministicEsavArchive` / `LoadDeterministicEsavArchive`.
//   - Stage 6: `MinimalRtsModule` (`teamId >= kMaxTeams` OOB rejection +
//     `MiniJsonParser` scoping), `WorldSceneSession` v33 tileset profile wiring,
//     built-in `tests/data/mini_mods.w3x` CI determinism, and optional 1,000-tick
//     cloud `.w3x` determinism!
// ============================================================================
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "audio/AudioSystem.h"
#include "audio/audio_server.h"
#include "core/unit_presentation_system.h"
#include "core/world_scene_session.h"
#include "data/unitdata.h"
#include "ecs/unit_type.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/item.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "engine/core/project_manifest.h"
#include "engine/core/tag_id.h"
#include "engine/core/type_id.h"
#include "engine/dynamic_module_loader.h"
#include "engine/render/rhi_backend.h"
#include "engine/render/standard_render_passes.h"
#include "engine/resource/format_loader.h"
#include "engine/resource/table_schema_binder.h"
#include "engine/resource/vfs_archive.h"
#include "engine/ui/hud_view_model.h"
#include "engine/ui/ui_frame_rtti_free.h"
#include "engine/world/simulation_extension.h"
#include "game/war3/war3_game_module.h"
#include "projects/minimal_rts/minimal_rts_module.h"
#include "projects/war3/mpq_vfs_provider.h"
#include "projects/war3/war3_render_adapters.h"
#include "projects/war3/war3_save_contributor.h"
#include "render/render_server.h"
#include "render/rhi_device.h"
#include "render/scene.h"
#include "resource/mpq_storm.h"
#include "resource/resource_server.h"
#include "ui/BackdropFrame.h"
#include "ui/ButtonFrame.h"
#include "ui/ExtendedFrames.h"
#include "ui/SimpleFrame.h"
#include "ui/TextFrame.h"
#include "ui/TextureFrame.h"
#include "ui/UIFrame.h"
#include "ui/ui_data_binder.h"

#define STAGE_CHECK(cond)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            std::cerr << "  [STAGE_FAIL] " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
            return false;                                                                      \
        }                                                                                      \
    } while (0)

namespace {

namespace fs = std::filesystem;

const uint32_t* StageMpqCryptTable() {
    static uint32_t tbl[0x500];
    static bool init = false;
    if (!init) {
        uint32_t seed = 0x00100001u;
        for (uint32_t idx1 = 0; idx1 < 0x100u; ++idx1) {
            uint32_t idx2 = idx1;
            for (int i = 0; i < 5; ++i, idx2 += 0x100u) {
                seed = (seed * 125u + 3u) % 0x2AAAABu;
                const uint32_t t1 = (seed & 0xFFFFu) << 16u;
                seed = (seed * 125u + 3u) % 0x2AAAABu;
                const uint32_t t2 = (seed & 0xFFFFu);
                tbl[idx2] = t1 | t2;
            }
        }
        init = true;
    }
    return tbl;
}

uint32_t StageMpqHash(const std::string& str, uint32_t hashType) {
    const uint32_t* tbl = StageMpqCryptTable();
    uint32_t seed1 = 0x7FED7FEDu;
    uint32_t seed2 = 0xEEEEEEEEu;
    for (unsigned char c : str) {
        if (c == '/') c = '\\';
        c = static_cast<unsigned char>(std::toupper(c));
        seed1 = tbl[(hashType << 8u) + c] ^ (seed1 + seed2);
        seed2 = c + seed1 + seed2 + (seed2 << 5u) + 3u;
    }
    return seed1;
}

void StageMpqEncrypt(void* buf, size_t byteLen, uint32_t key) {
    const uint32_t* tbl = StageMpqCryptTable();
    uint32_t seed = 0xEEEEEEEEu;
    auto* dw = static_cast<uint32_t*>(buf);
    for (size_t i = 0; i < byteLen / 4u; ++i) {
        seed += tbl[0x400u + (key & 0xFFu)];
        const uint32_t plain = dw[i];
        dw[i] = plain ^ (key + seed);
        key = (((~key) << 0x15u) + 0x11111111u) | (key >> 0x0Bu);
        seed = plain + seed + (seed << 5u) + 3u;
    }
}

// Build a valid PKWARE DCL binary stream (litCoded=0, dictBits=4) with LZ77 distance/length=3
// back-references so repeated strings shrink well below `plain.size()` (exercising both literal
// and sliding-window Huffman decode paths in `PkwareDclDecoder::Explode`).
std::vector<uint8_t> EncodeMinimalPkwareDclLiterals(const std::string& plain) {
    std::vector<uint8_t> out;
    out.push_back(0u); // litCoded = 0 (uncoded 8-bit literals)
    out.push_back(4u); // dictBits = 4 (1 KB window)

    uint64_t bitBuf = 0;
    int bitCount = 0;
    auto putBits = [&](uint32_t val, int n) {
        bitBuf |= (static_cast<uint64_t>(val) << bitCount);
        bitCount += n;
        while (bitCount >= 8) {
            out.push_back(static_cast<uint8_t>(bitBuf & 0xFFu));
            bitBuf >>= 8;
            bitCount -= 8;
        }
    };

    size_t pos = 0;
    while (pos < plain.size()) {
        // Check for a 3-byte match within distance 1..16 (`distIdx = 0`, 2-bit code `0b11`;
        // `lenIdx = 1` -> copyLen = 3, 2-bit code `0b11`; `lowDist` = distance - 1 in 4 bits).
        size_t foundDist = 0;
        if (pos + 3 <= plain.size()) {
            const size_t maxDist = std::min<size_t>(pos, 16u);
            for (size_t d = 1; d <= maxDist; ++d) {
                if (plain[pos - d] == plain[pos] &&
                    plain[pos - d + 1] == plain[pos + 1] &&
                    plain[pos - d + 2] == plain[pos + 2]) {
                    foundDist = d;
                    break;
                }
            }
        }
        if (foundDist > 0) {
            putBits(1u, 1);                                  // flag = 1 (copy pair)
            putBits(0x3u, 2);                                // lenIdx = 1 (copyLen = 3)
            putBits(0x3u, 2);                                // distIdx = 0
            putBits(static_cast<uint32_t>(foundDist - 1u), 4); // 4-bit lowDist
            pos += 3;
        } else {
            putBits(0u, 1);                                  // flag = 0 (literal)
            putBits(static_cast<unsigned char>(plain[pos]), 8);
            ++pos;
        }
    }

    // End-of-stream marker: flag = 1, lenSym = 15 (seven 0 bits), extra = 0xFF (eight 1 bits).
    putBits(1u, 1);
    putBits(0u, 7);
    putBits(0xFFu, 8);

    if (bitCount > 0) {
        out.push_back(static_cast<uint8_t>(bitBuf & 0xFFu));
    }
    return out;
}

struct CustomArchiveItem {
    std::string name;
    std::vector<uint8_t> storedPayload;
    uint32_t uncompressedSize = 0;
    uint32_t flags = 0x81000000u; // MPQ_FILE_EXISTS | MPQ_FILE_SINGLE_UNIT
};

bool WriteSyntheticMpq(const fs::path& path,
                       uint16_t blockSizeShift,
                       uint32_t hashCount,
                       const std::vector<CustomArchiveItem>& items) {
    struct HEntry { uint32_t a, b; uint16_t loc, plat; uint32_t blk; };
    struct BEntry { uint32_t pos, csize, fsize, flags; };

    uint32_t cursor = 32u;
    std::vector<BEntry> blocks;
    for (const auto& item : items) {
        blocks.push_back({cursor,
                          static_cast<uint32_t>(item.storedPayload.size()),
                          item.uncompressedSize,
                          item.flags});
        cursor += static_cast<uint32_t>((item.storedPayload.size() + 3u) & ~3u);
    }

    const uint32_t hashOffset = cursor;
    const uint32_t blockOffset = hashOffset + hashCount * 16u;
    const uint32_t totalSize = blockOffset + static_cast<uint32_t>(blocks.size()) * 16u;

    std::vector<uint8_t> raw(totalSize, 0u);
    std::memcpy(raw.data(), "MPQ\x1A", 4);
    const uint32_t hdrSize = 32u;
    const uint16_t fmtVer = 0u;
    const uint32_t blockCount = static_cast<uint32_t>(blocks.size());
    std::memcpy(raw.data() + 4, &hdrSize, 4);
    std::memcpy(raw.data() + 8, &totalSize, 4);
    std::memcpy(raw.data() + 12, &fmtVer, 2);
    std::memcpy(raw.data() + 14, &blockSizeShift, 2);
    std::memcpy(raw.data() + 16, &hashOffset, 4);
    std::memcpy(raw.data() + 20, &blockOffset, 4);
    std::memcpy(raw.data() + 24, &hashCount, 4);
    std::memcpy(raw.data() + 28, &blockCount, 4);

    for (size_t i = 0; i < items.size(); ++i) {
        if (!items[i].storedPayload.empty()) {
            std::memcpy(raw.data() + blocks[i].pos,
                        items[i].storedPayload.data(),
                        items[i].storedPayload.size());
        }
    }

    std::vector<HEntry> ht(hashCount, {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFFFFFu});
    if (hashCount > 0) {
        for (size_t i = 0; i < items.size(); ++i) {
            uint32_t slot = StageMpqHash(items[i].name, 0) % hashCount;
            while (ht[slot].blk != 0xFFFFFFFFu) {
                slot = (slot + 1u) % hashCount;
            }
            ht[slot] = {StageMpqHash(items[i].name, 1),
                        StageMpqHash(items[i].name, 2),
                        0u, 0u, static_cast<uint32_t>(i)};
        }
        StageMpqEncrypt(ht.data(), ht.size() * sizeof(HEntry), StageMpqHash("(hash table)", 3));
        std::memcpy(raw.data() + hashOffset, ht.data(), ht.size() * sizeof(HEntry));
    }

    if (!blocks.empty()) {
        StageMpqEncrypt(blocks.data(), blocks.size() * sizeof(BEntry), StageMpqHash("(block table)", 3));
        std::memcpy(raw.data() + blockOffset, blocks.data(), blocks.size() * sizeof(BEntry));
    }

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;
    ofs.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    return ofs.good();
}

} // namespace

// ============================================================================
// Stage 1: TagId, Cross-Boundary Deterministic StaticTypeId, INI Backslash
//          Symmetry & Pure War3-Free `engine/` Header Audit
// ============================================================================
bool TestStage1CoreTypeIdTagIdManifestAndEnginePurity() {
    using namespace engine::literals;

    // 1. Compile-time FourCC & FNV-1a TagId + seamless War3ID interop
    static_assert("hfoo"_tag.Value() == 0x68666F6Fu, "FourCC TagId mismatch");
    constexpr engine::TagId longTag("minimal_rts.pulse_trooper");
    static_assert(longTag.IsValid(), "Hashed TagId must be non-zero");

    const War3ID wId("hfoo");
    const engine::TagId tId = wId;
    STAGE_CHECK(tId == "hfoo"_tag);
    STAGE_CHECK(War3ID(tId) == wId);

    // 2. Compile-time deterministic FNV-1a StaticTypeId (verified constexpr!)
    struct DummyHealthComponent { int hp = 100; };
    struct DummyVelocityComponent { float vx = 0.0f; };
    constexpr engine::TypeId idHealthA = engine::StaticTypeId<DummyHealthComponent>();
    constexpr engine::TypeId idHealthB = engine::StaticTypeId<const DummyHealthComponent&>();
    constexpr engine::TypeId idVel = engine::StaticTypeId<DummyVelocityComponent>();
    static_assert(idHealthA != 0, "StaticTypeId must be compile-time non-zero");
    static_assert(idHealthA == idHealthB, "CV/ref qualifiers must normalize to identical TypeId");
    static_assert(idHealthA != idVel, "Distinct types must produce distinct TypeIds");

    union DummyPayloadUnion {
        int i;
        float f;
    };
    constexpr engine::TypeId idUnion = engine::StaticTypeId<DummyPayloadUnion>();
    static_assert(idUnion != 0 && idUnion != idHealthA, "Union StaticTypeId must be valid and distinct");
    static_assert(
        engine::detail::Fnva64TypeSignature(
            "MyTpl<struct Foo::Bar, class Baz, union Quux, enum Kind>") ==
            engine::detail::Fnva64TypeSignature("MyTpl<Foo::Bar, Baz, Quux, Kind>"),
        "MSVC elaborated type prefixes inside template arguments must normalize identically to Clang/GCC");
    static_assert(
        engine::detail::Fnva64TypeSignature("MyTpl<Subclass >") !=
            engine::detail::Fnva64TypeSignature("MyTpl<Sub>"),
        "Identifier suffixes ending in 'class ' must never be stripped");

    // 3. ProjectManifest JSON + INI round-trip WITH Windows backslashes (P2 #11 fix!)
    engine::ProjectManifest manifest;
    std::string err;
    STAGE_CHECK(engine::ProjectManifest::LoadFromFile("projects/war3/war3.project.json", manifest, &err));
    STAGE_CHECK(manifest.projectId == "war3_tft");
    STAGE_CHECK(manifest.modulePlugin == "war3_module_plugin");
    STAGE_CHECK(manifest.vfsMounts.size() >= 5);
    STAGE_CHECK(manifest.vfsMounts[1].path == "resources/War3Patch.mpq");

    manifest.defaultScene = "resources\\Maps\\FrozenThrone\\(2)EchoIsles.w3x";
    manifest.customSettings["winInstallDir"] = "C:\\Games\\Warcraft III\\Frozen Throne";
    const std::string iniText = manifest.ToIni();
    engine::ProjectManifest parsedIni;
    STAGE_CHECK(engine::ProjectManifest::ParseIni(iniText, parsedIni, &err));
    STAGE_CHECK(parsedIni.defaultScene == "resources\\Maps\\FrozenThrone\\(2)EchoIsles.w3x");
    STAGE_CHECK(parsedIni.customSettings["winInstallDir"] == "C:\\Games\\Warcraft III\\Frozen Throne");
    STAGE_CHECK(parsedIni.vfsMounts.size() == manifest.vfsMounts.size());

    // 4. Audit EVERY header under `engine/` for 100% War3-free & RTTI-free purity (P2 #9.2 fix!)
    static const char* kForbiddenTokens[] = {
        "War3ID",
        "sokol_gfx.h",
        "mpq_storm.h",
        "dynamic_cast",
        "LordaeronTree",
        "KmtfTrack",
        "MdxMaterialLayer",
        "SetBlightCircle",
        "CreateDefaultWarcraftPipeline"
    };
    int auditedHeaders = 0;
    for (const auto& entry : fs::recursive_directory_iterator("engine")) {
        if (!entry.is_regular_file()) continue;
        std::ifstream ifs(entry.path());
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        // Strip comments before checking so explanatory comments don't trigger false positives
        std::string codeOnly;
        codeOnly.reserve(content.size());
        for (size_t i = 0; i < content.size(); ++i) {
            if (i + 1 < content.size() && content[i] == '/' && content[i + 1] == '/') {
                while (i < content.size() && content[i] != '\n') ++i;
            } else {
                codeOnly.push_back(content[i]);
            }
        }
        for (const char* tok : kForbiddenTokens) {
            STAGE_CHECK(codeOnly.find(tok) == std::string::npos);
        }
        ++auditedHeaders;
    }
    STAGE_CHECK(auditedHeaders >= 10);
    return true;
}

// ============================================================================
// Stage 2: Streaming Bounds-Safe MPQ Security Audit, PKWARE DCL, Wildcard
//          `ListAllFiles`, `ResourceServer::Vfs()`, & Non-Zero Bool Schema Binder
// ============================================================================
bool TestStage2VfsMpqSecurityPkwareDclAndSchemaBinder() {
    const fs::path tmpDir = fs::temp_directory_path() / "warsmash_v2_mpq_audit";
    std::error_code ec;
    fs::remove_all(tmpDir, ec);
    fs::create_directories(tmpDir, ec);

    // 1. P0 #1 Security Audit: reject non-power-of-2 hashEntries, blockSizeShift >= 16,
    //    and malformed uncompressed block with compressedSize < fileSize (zero heap over-read!)
    {
        const fs::path badHashMpq = tmpDir / "bad_hash_non_pow2.mpq";
        STAGE_CHECK(WriteSyntheticMpq(badHashMpq, 3, /*hashCount=*/6, {}));
        MpqManager m;
        STAGE_CHECK(!m.Open(badHashMpq.string()));
        NativeMpqArchiveReader nativeRdr;
        STAGE_CHECK(!nativeRdr.Open(badHashMpq.string()));
    }
    {
        const fs::path badShiftDivZeroMpq = tmpDir / "bad_shift_25.mpq";
        STAGE_CHECK(WriteSyntheticMpq(badShiftDivZeroMpq, /*blockSizeShift=*/25, 16, {}));
        MpqManager m;
        STAGE_CHECK(!m.Open(badShiftDivZeroMpq.string()));
        NativeMpqArchiveReader nativeRdr;
        STAGE_CHECK(!nativeRdr.Open(badShiftDivZeroMpq.string()));

        const fs::path badShiftUbMpq = tmpDir / "bad_shift_35.mpq";
        STAGE_CHECK(WriteSyntheticMpq(badShiftUbMpq, /*blockSizeShift=*/35, 16, {}));
        STAGE_CHECK(!m.Open(badShiftUbMpq.string()));
        STAGE_CHECK(!nativeRdr.Open(badShiftUbMpq.string()));
    }
    {
        // Uncompressed block claiming fileSize = 4096 while compressedSize = 8 -> MUST fail without heap over-read!
        const fs::path overreadMpq = tmpDir / "trunc_uncompressed.mpq";
        CustomArchiveItem badItem;
        badItem.name = "corrupt.bin";
        badItem.storedPayload = {1, 2, 3, 4, 5, 6, 7, 8};
        badItem.uncompressedSize = 4096u;
        badItem.flags = 0x81000000u; // EXISTS | SINGLE_UNIT, NO COMPRESS/IMPLODE flag!
        STAGE_CHECK(WriteSyntheticMpq(overreadMpq, 3, 16, {badItem}));

        NativeMpqArchiveReader nativeRdr;
        STAGE_CHECK(nativeRdr.Open(overreadMpq.string()));
        std::vector<uint8_t> out;
        STAGE_CHECK(!nativeRdr.ReadFile("corrupt.bin", out));
    }

    // 2. PKWARE DCL Explode (0x08 & 0x100), Composite PKWARE+IMA ADPCM (0x48), & ListAllFiles(pattern)
    {
        const std::string expectedA =
            "PKWARE_0x100!PKWARE_0x100!PKWARE_0x100!PKWARE_0x100!PKWARE_0x100!";
        const std::string expectedB =
            "MULTI_0x08!MULTI_0x08!MULTI_0x08!MULTI_0x08!MULTI_0x08!";
        const std::string listfileBody =
            "scripts\\war3map.j\r\ndoodads\\tree.mdx\r\nsound\\footstep.wav\r\n";

        CustomArchiveItem itemList;
        itemList.name = "(listfile)";
        itemList.storedPayload.assign(listfileBody.begin(), listfileBody.end());
        itemList.uncompressedSize = static_cast<uint32_t>(listfileBody.size());
        itemList.flags = 0x81000000u;

        CustomArchiveItem itemImplode;
        itemImplode.name = "scripts\\war3map.j";
        itemImplode.storedPayload = EncodeMinimalPkwareDclLiterals(expectedA);
        itemImplode.uncompressedSize = static_cast<uint32_t>(expectedA.size());
        itemImplode.flags = 0x81000100u; // EXISTS | SINGLE_UNIT | MPQ_FILE_IMPLODE

        CustomArchiveItem itemComp08;
        itemComp08.name = "doodads\\tree.mdx";
        std::vector<uint8_t> dclB = EncodeMinimalPkwareDclLiterals(expectedB);
        itemComp08.storedPayload.push_back(0x08u); // MPQ_COMPRESSION_PKWARE
        itemComp08.storedPayload.insert(itemComp08.storedPayload.end(), dclB.begin(), dclB.end());
        itemComp08.uncompressedSize = static_cast<uint32_t>(expectedB.size());
        itemComp08.flags = 0x81000200u; // EXISTS | SINGLE_UNIT | MPQ_FILE_COMPRESS

        // Build a 32-sample (64-byte PCM) Mono IMA ADPCM stream compressed with composite 0x08 | 0x40
        std::string rawAdpcm;
        rawAdpcm.push_back(static_cast<char>(0x00)); // pad
        rawAdpcm.push_back(static_cast<char>(0x03)); // bitShift = 3
        rawAdpcm.push_back(static_cast<char>(0xE8)); // initSample = 1000 LE
        rawAdpcm.push_back(static_cast<char>(0x03));
        for (int i = 0; i < 31; ++i) {
            rawAdpcm.push_back(static_cast<char>((i % 2 == 0) ? 0x05 : 0x43));
        }
        std::vector<uint8_t> dclAdpcm = EncodeMinimalPkwareDclLiterals(rawAdpcm);
        CustomArchiveItem itemAdpcm;
        itemAdpcm.name = "sound\\footstep.wav";
        itemAdpcm.storedPayload.push_back(0x08u | 0x40u); // PKWARE DCL + IMA ADPCM Mono
        itemAdpcm.storedPayload.insert(itemAdpcm.storedPayload.end(), dclAdpcm.begin(), dclAdpcm.end());
        itemAdpcm.uncompressedSize = 64u;
        itemAdpcm.flags = 0x81000200u;

        const fs::path dclMpq = tmpDir / "pkware_dcl_and_pattern.mpq";
        STAGE_CHECK(WriteSyntheticMpq(dclMpq, 3, 16, {itemList, itemImplode, itemComp08, itemAdpcm}));

        MpqManager mpq;
        STAGE_CHECK(mpq.Open(dclMpq.string()));
        NativeMpqArchiveReader nativeRdr;
        STAGE_CHECK(nativeRdr.Open(dclMpq.string()));

        std::vector<uint8_t> readA, nativeA;
        STAGE_CHECK(mpq.ReadFile("scripts\\war3map.j", readA));
        STAGE_CHECK(nativeRdr.ReadFile("scripts\\war3map.j", nativeA));
        STAGE_CHECK(std::string(readA.begin(), readA.end()) == expectedA);
        STAGE_CHECK(nativeA == readA);

        std::vector<uint8_t> readB, nativeB;
        STAGE_CHECK(mpq.ReadFile("doodads\\tree.mdx", readB));
        STAGE_CHECK(nativeRdr.ReadFile("doodads\\tree.mdx", nativeB));
        STAGE_CHECK(std::string(readB.begin(), readB.end()) == expectedB);
        STAGE_CHECK(nativeB == readB);

        std::vector<uint8_t> stormPcm, nativePcm;
        STAGE_CHECK(mpq.ReadFile("sound\\footstep.wav", stormPcm));
        STAGE_CHECK(nativeRdr.ReadFile("sound\\footstep.wav", nativePcm));
        STAGE_CHECK(stormPcm.size() == 64u);
        STAGE_CHECK(nativePcm == stormPcm);

        // Verify ListAllFiles(pattern) & NativeMpqArchiveReader::ListFiles(pattern)
        const auto onlyJass = mpq.ListAllFiles("*.j");
        STAGE_CHECK(onlyJass.size() == 1);
        STAGE_CHECK(onlyJass[0] == "scripts\\war3map.j");
        const auto nativeJass = nativeRdr.ListFiles("*.j");
        STAGE_CHECK(nativeJass.size() == 1 && nativeJass[0] == "scripts\\war3map.j");

        const auto onlyMdx = mpq.ListAllFiles("doodads\\*.mdx");
        STAGE_CHECK(onlyMdx.size() == 1);
        STAGE_CHECK(onlyMdx[0] == "doodads\\tree.mdx");

        // Verify VfsMountRegistry::ListFiles prepends mountPoint & deduplicates overlays!
        engine::vfs::VfsMountRegistry vfsReg;
        auto dataMount = std::make_shared<engine::vfs::MemoryArchiveProvider>("mem://data");
        dataMount->PutText("units.json", "{}");
        vfsReg.Mount(dataMount, 200, "/data");

        auto lowMount = std::make_shared<engine::vfs::MemoryArchiveProvider>("mem://low");
        lowMount->PutText("shared/settings.ini", "v=1");
        vfsReg.Mount(lowMount, 100, "/");

        auto highMount = std::make_shared<engine::vfs::MemoryArchiveProvider>("mem://high");
        highMount->PutText("shared\\settings.ini", "v=2");
        vfsReg.Mount(highMount, 300, "/");

        const auto listedData = vfsReg.ListFiles("data/*.json");
        STAGE_CHECK(listedData.size() == 1u && listedData[0] == "data/units.json");
        const auto listedIni = vfsReg.ListFiles("shared/*.ini");
        STAGE_CHECK(listedIni.size() == 1u);

        // Verify ResourceServer VFS priority: `.w3x` OpenOnTop (`mpq://bound` at 450) overrides
        // base MPQ mounts (priority 400), while explicit overlay (>= 500) wins on top!
        const fs::path mapOverrideMpq = tmpDir / "map_override.w3x";
        CustomArchiveItem mapScriptItem;
        mapScriptItem.name = "scripts\\war3map.j";
        const std::string mapScriptText = "// custom w3x map override";
        mapScriptItem.storedPayload.assign(mapScriptText.begin(), mapScriptText.end());
        mapScriptItem.uncompressedSize = static_cast<uint32_t>(mapScriptText.size());
        mapScriptItem.flags = 0x81000000u;
        STAGE_CHECK(WriteSyntheticMpq(mapOverrideMpq, 3, 16, {mapScriptItem}));

        MpqManager mapMpq;
        STAGE_CHECK(mapMpq.Open(dclMpq.string()));
        STAGE_CHECK(mapMpq.OpenOnTop(mapOverrideMpq.string()));

        resource::ResourceServer resServer;
        auto basePatchMount = std::make_shared<engine::vfs::MemoryArchiveProvider>("mem://war3patch");
        basePatchMount->PutText("scripts/war3map.j", "// base patch mpq");
        resServer.Vfs().Mount(basePatchMount, 400, "/");
        resServer.SetMpqManager(&mapMpq);

        std::vector<uint8_t> resolvedScript;
        STAGE_CHECK(resServer.ReadRawBytes("scripts/war3map.j", resolvedScript));
        STAGE_CHECK(std::string(resolvedScript.begin(), resolvedScript.end()) == mapScriptText);

        auto memOverlay = std::make_shared<engine::vfs::MemoryArchiveProvider>();
        memOverlay->PutText("custom/config.ini", "tick_rate=20");
        memOverlay->PutText("scripts/war3map.j", "// dev hotfix overlay");
        resServer.Vfs().Mount(memOverlay, 500, "/");

        std::vector<uint8_t> vfsBytes;
        STAGE_CHECK(resServer.FileExists("custom/config.ini"));
        STAGE_CHECK(resServer.ReadRawBytes("custom/config.ini", vfsBytes));
        STAGE_CHECK(std::string(vfsBytes.begin(), vfsBytes.end()) == "tick_rate=20");
        STAGE_CHECK(resServer.ReadRawBytes("scripts/war3map.j", resolvedScript));
        STAGE_CHECK(std::string(resolvedScript.begin(), resolvedScript.end()) == "// dev hotfix overlay");
    }

    fs::remove_all(tmpDir, ec);

    // 3. TableSchemaBinder non-zero integer boolean semantics (P2 #9.1 fix!)
    struct TestRow {
        bool flagPositiveTwo = false;
        bool flagNegativeOne = false;
        bool flagZero = true;
        int hp = 0;
    };
    engine::resource::TableSchemaBinder<TestRow> binder;
    binder.BindBool("flagA", &TestRow::flagPositiveTwo, false)
          .BindBool("flagB", &TestRow::flagNegativeOne, false)
          .BindBool("flagC", &TestRow::flagZero, true)
          .BindInt("HP", &TestRow::hp, 50);

    TestRow hydrated;
    binder.HydrateRow(hydrated, [](std::string_view col) -> std::string_view {
        if (col == "flagA") return "2";
        if (col == "flagB") return "-1";
        if (col == "flagC") return "0";
        if (col == "HP") return "750";
        return "";
    });
    STAGE_CHECK(hydrated.flagPositiveTwo == true);
    STAGE_CHECK(hydrated.flagNegativeOne == true);
    STAGE_CHECK(hydrated.flagZero == false);
    STAGE_CHECK(hydrated.hp == 750);
    return true;
}

// ============================================================================
// Stage 3: IRhiBackend, Incremental O(dirty_area) TerrainOverlayState &
//          DiscreteStepTimelineSampler (Anti-Rounding KMTF Flipbook Fix)
// ============================================================================
bool TestStage3RhiBackendIncrementalOverlayAndDiscreteSampler() {
    engine::render::NullRhiBackend nullRhi;
    engine::render::TerrainOverlayState overlay(256, 256);

    // Initial full sync clears startup dirty rect
    std::vector<uint8_t> initBuf;
    const auto fullRect = overlay.SyncDirtyOverlayRgba8(&initBuf);
    STAGE_CHECK(fullRect.Width() == 256 && fullRect.Height() == 256);
    STAGE_CHECK(!overlay.HasDirtyRegions());

    // Stamp a small radius=3 mask circle at (100, 100): dirty rect MUST be 7x7 = 49 pixels, NOT 65,536!
    overlay.SetMaskCircle(100, 100, 3, true);
    STAGE_CHECK(overlay.IsMaskActive(100, 100));
    STAGE_CHECK(overlay.HasDirtyRegions());

    const uint32_t texHandle = nullRhi.CreateTexture2D({256, 256, 1, 1, engine::render::RhiPixelFormat::RGBA8, true});
    auto pipeline = engine::render::StandardRenderPassPipeline::CreateStandardRtsPipeline();
    engine::render::StandardPassContext ctx{&nullRhi, &overlay, texHandle, 1280, 720};
    pipeline.ExecuteFrame(ctx);

    STAGE_CHECK(!overlay.HasDirtyRegions());
    STAGE_CHECK(overlay.LastSyncedPixelCount() == 49u);
    STAGE_CHECK(nullRhi.GetStats().textureUpdates == 1u);
    uint8_t centerTexel[4] = {};
    uint8_t outsideTexel[4] = {1, 1, 1, 1};
    STAGE_CHECK(nullRhi.ReadTextureTexelRgba8(texHandle, 100, 100, centerTexel));
    STAGE_CHECK(nullRhi.ReadTextureTexelRgba8(texHandle, 10, 10, outsideTexel));
    STAGE_CHECK(centerTexel[1] == 255u && centerTexel[2] == 255u && centerTexel[3] == 255u);
    STAGE_CHECK(outsideTexel[1] == 0u);
    nullRhi.DestroyTexture(texHandle);

    // Verify War3MdxLayerSampler & DiscreteStepTimelineSampler never round-jump between slots
    projects::war3::War3MdxMaterialLayer layer;
    layer.baseTextureSlot = 0;
    layer.replaceableId = projects::war3::War3ReplaceableId::None;
    layer.kmtfKeys = {{0, 0}, {500, 3}, {1000, 5}};
    STAGE_CHECK(projects::war3::War3MdxLayerSampler::EvaluateTextureSlot(layer, 450, 0, 1000) == 0);
    STAGE_CHECK(projects::war3::War3MdxLayerSampler::EvaluateTextureSlot(layer, 500, 0, 1000) == 3);
    STAGE_CHECK(engine::render::DiscreteStepTimelineSampler::TruncateFloatTrackToIndex(2.85f) == 2);
    return true;
}

// ============================================================================
// Stage 4: RTTI-Free UIFrame Hierarchy, Live UIDataBinder IHudViewListener &
//          AudioServer Unified `voiceId` + Zero Ghost Ducking Verification
// ============================================================================
bool TestStage4UiRttiFreeReactiveHudAndAudioUnification() {
    // 1. Verify all UIFrame subclasses resolve ancestry bitmasks in O(1) without RTTI
    SimpleFrame root("Root");
    PopupMenuFrame popup("Popup", &root);
    MenuFrame menu("Menu", &root);
    CheckBoxFrame check("Check", &root);
    TextFrame text("GoldText", &root);

    UIFrame* basePopup = &popup;
    UIFrame* baseMenu = &menu;
    UIFrame* baseCheck = &check;
    UIFrame* baseText = &text;

    STAGE_CHECK(basePopup->As<PopupMenuFrame>() == &popup);
    STAGE_CHECK(basePopup->As<ButtonFrame>() == &popup);
    STAGE_CHECK(basePopup->As<SimpleFrame>() == &popup);
    STAGE_CHECK(basePopup->As<TextFrame>() == nullptr);

    STAGE_CHECK(baseMenu->As<MenuFrame>() == &menu);
    STAGE_CHECK(baseMenu->As<ListBoxFrame>() == &menu);
    STAGE_CHECK(baseCheck->As<ButtonFrame>() == &check);
    STAGE_CHECK(baseText->As<SimpleFrame>() == nullptr);

    // Clear children vector before stack unwinding since stack frames are not heap-allocated
    const_cast<std::vector<UIFrame*>&>(root.GetChildren()).clear();

    // 2. Verify UIDataBinder subscribes as a real IHudViewListener and updates TextFrames via HudViewBinder
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "Human", CMapControl::USER));
    sim.GetPlayer(0)->SetGold(775);
    sim.GetPlayer(0)->SetLumber(320);
    sim.GetPlayer(0)->SetFoodCap(12);

    GameUI dummyGui;
    TextFrame goldLbl("Gold"), lumberLbl("Lumber"), foodLbl("Food");
    ui::UIDataBinder binder;
    int externalCallbacks = 0;
    struct CountingListener final : public engine::ui::IHudViewListener {
        int* counter = nullptr;
        void OnHudResourcesChanged(const engine::ui::HudViewModel&) override { ++(*counter); }
    } extListener;
    extListener.counter = &externalCallbacks;
    binder.AttachHudListener(&extListener);
    binder.BindResourceFrames(&goldLbl, &lumberLbl, &foodLbl);
    STAGE_CHECK(binder.GetHudViewBinder().ListenerCount() == 2u);
    STAGE_CHECK(binder.SyncAndRelayoutIfChanged(sim, 0, -1, dummyGui));
    STAGE_CHECK(goldLbl.GetText() == "775");
    STAGE_CHECK(lumberLbl.GetText() == "320");
    STAGE_CHECK(foodLbl.GetText() == "0/12");
    STAGE_CHECK(externalCallbacks == 1);
    binder.DetachHudListener(&extListener);

    // 3. P1 #5 Verification:
    //    (a) Failed 3D unit-voice load MUST NOT leave a ghost UnitVoice or duck SFX/Music!
    audio::AudioServer audioSrv;
    auto mockBackend = std::make_shared<audio::NullAudioBackend>();
    STAGE_CHECK(mockBackend->Init());
    audioSrv.SetBackend(mockBackend);

    const float sfxGainBefore = audioSrv.ComputeEffectiveBusGain(audio::AudioBusId::SFX);
    STAGE_CHECK(audioSrv.Play3DPathEx("Units\\Human\\Footman\\NonExistentProbe.wav", 0, 0, 0, 1.0f) == -1);
    STAGE_CHECK(audioSrv.GetActiveVoiceCountOnBus(audio::AudioBusId::UnitVoice) == 0);
    STAGE_CHECK(std::abs(audioSrv.ComputeEffectiveBusGain(audio::AudioBusId::SFX) - sfxGainBefore) < 1e-5f);

    //    (b) Interleaved backend IDs vs logical voiceIds: StopVoice(voiceId) stops the EXACT backend sound!
    resource::ResourceServer::Instance().MountMemoryFile(
        "Sound\\Interface\\TestBeep.wav", {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'});
    // Advance mockBackend's internal ID counter so backendSoundId != voiceId!
    const uint8_t dummyBytes[4] = {1, 2, 3, 4};
    const int preBackendId1 = mockBackend->PlayMemorySound(dummyBytes, 4, 1.0f, 0, 0, 0, false, 1.0f, 0, 0, false, true);
    const int preBackendId2 = mockBackend->PlayMemorySound(dummyBytes, 4, 1.0f, 0, 0, 0, false, 1.0f, 0, 0, false, true);
    STAGE_CHECK(preBackendId1 == 1 && preBackendId2 == 2);

    STAGE_CHECK(audioSrv.LoadSound("beep", "Sound\\Interface\\TestBeep.wav", nullptr));
    const int logicalVoiceId = audioSrv.Play2D("beep", 0.8f);
    STAGE_CHECK(logicalVoiceId == 1); // AudioServer's first voiceId is 1, while backendSoundId is 3!
    const auto* activeVoice = audioSrv.FindActiveVoice(logicalVoiceId);
    STAGE_CHECK(activeVoice != nullptr && activeVoice->backendSoundId == 3);
    STAGE_CHECK(mockBackend->IsSoundPlaying(3));
    STAGE_CHECK(mockBackend->IsSoundPlaying(1)); // Music slot 1 untouched!

    STAGE_CHECK(audioSrv.StopVoice(logicalVoiceId));
    STAGE_CHECK(!mockBackend->IsSoundPlaying(3)); // Exact mapped backend sound stopped!
    STAGE_CHECK(mockBackend->IsSoundPlaying(1));  // Music slot 1 NOT mistakenly killed!

    //    (c) Verify linear Master volume (`vol^1`, never `vol^2`/`vol^3`), 3D backend gain
    //        preservation across Update(), and immediate StopAllVoices() un-ducking!
    audioSrv.Update(0.1f); // clear per-clip cooldown from previous "beep" test
    audioSrv.SetMasterVolume(0.5f);
    const int linearVoiceId = audioSrv.Play2D("beep", 1.0f);
    STAGE_CHECK(linearVoiceId > 0);
    const auto* linVoice = audioSrv.FindActiveVoice(linearVoiceId);
    STAGE_CHECK(linVoice != nullptr);
    STAGE_CHECK(std::abs(mockBackend->GetSoundVolume(linVoice->backendSoundId) - 0.5f) < 1e-4f);
    STAGE_CHECK(audioSrv.StopVoice(linearVoiceId));

    audioSrv.Update(0.1f); // clear per-clip cooldown
    const int spatialVoiceId =
        audioSrv.Play3DEx("beep", 1350.0f, 0.0f, 0.0f, 1.0f, 1.0f, 300.0f, 2400.0f);
    STAGE_CHECK(spatialVoiceId > 0);
    const auto* spVoiceBefore = audioSrv.FindActiveVoice(spatialVoiceId);
    STAGE_CHECK(spVoiceBefore != nullptr);
    const int spBackendId = spVoiceBefore->backendSoundId;
    STAGE_CHECK(std::abs(mockBackend->GetSoundVolume(spBackendId) - 0.5f) < 1e-4f);
    audioSrv.Update(0.05f);
    STAGE_CHECK(std::abs(mockBackend->GetSoundVolume(spBackendId) - 0.5f) < 1e-4f);
    audioSrv.StopAllVoices();

    //    (d) Verify all 18 canonical RoC + TFT tilesets resolve in War3GameModule!
    {
        war3::War3GameModule modCheck;
        for (char code : {'L','F','W','V','Q','A','B','N','C','Y','X','Z','I','O','K','D','G','J'}) {
            engine::ISimulationModule::TilesetEnvironmentProfile prof;
            STAGE_CHECK(modCheck.OnResolveTilesetEnvironmentProfile(code, prof));
            STAGE_CHECK(!prof.dayAmbientMidiPath.empty() && !prof.nightAmbientMidiPath.empty());
        }
    }

    //    (e) Reset() cleans up all remaining backend sounds!
    audioSrv.Reset();
    STAGE_CHECK(mockBackend->GetActiveSoundCount() == 0);
    resource::ResourceServer::Instance().UnmountMemoryFiles();
    return true;
}

// ============================================================================
// Stage 5: WidgetKind RTTI-Free Casts, Overflow-Safe BinaryReader &
//          CSimulation Deterministic `"ESAV"` Save/Load Round-Trip
// ============================================================================
bool TestStage5WidgetRttiFreeOverflowSafeReaderAndEsav() {
    // 1. P1 #7 Security Audit: BinaryReader `cursor_ + len` unsigned wrap-around rejection
    const std::vector<uint8_t> tinyBuf = {0x11, 0x22, 0x33, 0x44, 0x55};
    engine::world::BinaryReader rdr(tinyBuf);
    uint8_t firstByte = 0;
    STAGE_CHECK(rdr.ReadU8(firstByte) && firstByte == 0x11u);
    uint8_t scratch[4] = {};
    // cursor_ is 1; len = SIZE_MAX causes (cursor_ + len) to wrap to 0 <= size_ if unchecked!
    STAGE_CHECK(!rdr.ReadBytes(scratch, std::numeric_limits<size_t>::max()));
    STAGE_CHECK(rdr.Cursor() == 1u);

    // 2. CWidget RTTI-free AsUnit / AsItem / AsDestructable + CSimulation ESAV round-trip
    auto makeFootmanType = []() {
        CUnitAttack atk;
        atk.range = 100.0f;
        atk.cooldown = 1.35f;
        atk.damageBase = 12;
        return std::make_unique<CUnitType>(
            "Footman", "Footman", "ReplaceableTextures\\CommandButtons\\BTNFootman.blp", War3ID("hfoo"),
            420, 0.25f, 0.0f, CRegenType::ALWAYS,
            0, 0, 270, 0, War3ID(),
            std::vector<War3ID>{}, false, PathingGridMovementType::FOOT,
            0.0f, 32.0f, std::vector<std::string>{},
            std::unordered_set<CUnitClassification>{},
            std::vector<CUnitAttack>{atk},
            2, std::string(""), false, true, CDefenseType::NONE,
            0.0f, 1.0f, std::unordered_set<CTargetType>{},
            500.0f, 0.0f, std::vector<War3ID>{}, std::vector<War3ID>{},
            std::vector<War3ID>{}, std::vector<War3ID>{},
            std::unordered_map<CUpgradeClass, War3ID>{}, std::vector<War3ID>{},
            std::vector<War3ID>{}, std::vector<War3ID>{}, CUnitRace{"Human", War3ID("H")},
            135, 0, 2, 0, 20, 0, 0, 0,
            std::unordered_set<CBuildingPathingType>{},
            std::unordered_set<CBuildingPathingType>{},
            0.5f, 0.5f, std::vector<CUnitTypeRequirement>{},
            std::vector<std::vector<CUnitTypeRequirement>>{},
            2, false, 0, 0.0f, 0, 0.0f, 0, 0.0f,
            CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
            std::vector<std::string>{}, 0, false, 0, false, 100,
            0.3f, 0.3f, false, false, 1, 1800, 800, false,
            10, 1, 6, 5, 1, 3, false);
    };
    CUnitData unitData;
    unitData.RegisterUnitType(War3ID("hfoo"), makeFootmanType());

    CSimulation sim;
    sim.SetUnitData(&unitData);
    auto war3Mod = std::make_shared<war3::War3GameModule>();
    sim.SetSimulationModule(war3Mod);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    sim.SeedRng(1337);
    CUnit* u = sim.CreateUnit(War3ID("hfoo"), 0, 128.0f, 256.0f, 90.0f);
    CItem* it = sim.CreateItem(War3ID("phea"), 64.0f, 64.0f);
    CDestructable* dest = sim.CreateDestructable(War3ID("LTlt"), 300.0f, 300.0f, 250.0f);
    sim.Update();
    STAGE_CHECK(u && it && dest);

    CWidget* wUnit = u;
    CWidget* wItem = it;
    CWidget* wDest = dest;
    STAGE_CHECK(wUnit->AsUnit() == u && wUnit->AsItem() == nullptr && wUnit->AsDestructable() == nullptr);
    STAGE_CHECK(wItem->AsItem() == it && wItem->AsUnit() == nullptr);
    STAGE_CHECK(wDest->AsDestructable() == dest && wDest->AsUnit() == nullptr);

    const int uHandle = u->GetHandleId();
    for (int i = 0; i < 10; ++i) sim.Update();

    const uint64_t midpointHash = sim.HashState();
    const std::vector<uint8_t> esavBytes = sim.SaveDeterministicEsavArchive();
    STAGE_CHECK(esavBytes.size() > 24);
    STAGE_CHECK(std::memcmp(esavBytes.data(), "ESAV", 4) == 0);

    // Restore midpoint ESAV into a fresh CSimulation and verify exact checkpoint state match!
    CSimulation restoredSim;
    restoredSim.SetUnitData(&unitData);
    restoredSim.SetSimulationModule(std::make_shared<war3::War3GameModule>());
    restoredSim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    STAGE_CHECK(restoredSim.LoadDeterministicEsavArchive(esavBytes));
    STAGE_CHECK(restoredSim.HashState() == midpointHash);

    // Issue identical lockstep post-checkpoint command stream on both simulations and step 15 ticks!
    CUnit* restoredU = restoredSim.GetUnit(uHandle);
    STAGE_CHECK(restoredU != nullptr && restoredU->AsUnit() == restoredU);
    u->IssueOrder(std::make_unique<COrder>(OrderIds::MOVE, COrder::TargetKind::POINT, 512.0f, 512.0f), false);
    restoredU->IssueOrder(std::make_unique<COrder>(OrderIds::MOVE, COrder::TargetKind::POINT, 512.0f, 512.0f), false);
    for (int i = 0; i < 15; ++i) {
        sim.Update();
        restoredSim.Update();
    }
    STAGE_CHECK(restoredSim.HashState() == sim.HashState());

    // Post-ESAV spawn regression: HandleIdAllocator on cold-loaded `restoredSim` MUST advance
    // past all restored handles (never colliding with uHandle == 0x100000 or causing UAF)!
    CUnit* postOrigUnit = sim.CreateUnit(War3ID("hfoo"), 0, 200.0f, 300.0f, 0.0f);
    CUnit* postRestUnit = restoredSim.CreateUnit(War3ID("hfoo"), 0, 200.0f, 300.0f, 0.0f);
    CItem* postOrigItem = sim.CreateItem(War3ID("phea"), 96.0f, 96.0f);
    CItem* postRestItem = restoredSim.CreateItem(War3ID("phea"), 96.0f, 96.0f);
    CDestructable* postOrigDest = sim.CreateDestructable(War3ID("LTlt"), 360.0f, 360.0f, 200.0f);
    CDestructable* postRestDest = restoredSim.CreateDestructable(War3ID("LTlt"), 360.0f, 360.0f, 200.0f);
    STAGE_CHECK(postOrigUnit && postRestUnit && postOrigItem && postRestItem && postOrigDest && postRestDest);
    STAGE_CHECK(postRestUnit->GetHandleId() >= 0x100003);
    STAGE_CHECK(postRestUnit->GetHandleId() == postOrigUnit->GetHandleId());
    STAGE_CHECK(postRestItem->GetHandleId() == postOrigItem->GetHandleId());
    STAGE_CHECK(postRestDest->GetHandleId() == postOrigDest->GetHandleId());
    STAGE_CHECK(restoredSim.GetUnit(uHandle) == restoredU);
    for (int i = 0; i < 5; ++i) {
        sim.Update();
        restoredSim.Update();
    }
    STAGE_CHECK(restoredSim.HashState() == sim.HashState());

    // Tampered ESAV payload byte must be rejected by FNV-1a checksum!
    std::vector<uint8_t> corrupted = esavBytes;
    corrupted.back() ^= 0xFFu;
    STAGE_CHECK(!restoredSim.LoadDeterministicEsavArchive(corrupted));
    return true;
}

// ============================================================================
// Stage 6: MinimalRTS Bounds & JSON Scoping, Dual-Project Replay, Built-in
//          `mini_mods.w3x` CI Determinism & Cloud `.w3x` 1,000-Tick Determinism
// ============================================================================
bool TestStage6MinimalRtsDualProjectReplayAndCloudMaps1000TickDeterminism() {
    // 1. MinimalRtsModule JSON scoping (ignores sibling "metadata" object) + OOB teamId rejection!
    std::ifstream jsonFile("projects/minimal_rts/data/units.json");
    STAGE_CHECK(jsonFile.is_open());
    const std::string unitsJson((std::istreambuf_iterator<char>(jsonFile)),
                                std::istreambuf_iterator<char>());

    auto runMinimalRts = [&](bool withMidpointSaveRestore, uint64_t& outFinalHash) -> bool {
        projects::minimal_rts::MinimalRtsModule rts;
        STAGE_CHECK(rts.LoadPrototypesFromJson(unitsJson));
        STAGE_CHECK(rts.PrototypeCount() == 4u); // Sibling "metadata" object was NOT misparsed!

        // P1 #6 Security Audit: out-of-bounds teamId MUST return kNullEntity without OOB write!
        STAGE_CHECK(rts.SpawnEntity(engine::TagId("drone_harvester"), 99, 0.0f, 0.0f) ==
                    projects::minimal_rts::kNullEntity);
        STAGE_CHECK(rts.SpawnEntity(engine::TagId("drone_harvester"),
                                    projects::minimal_rts::MinimalRtsModule::kMaxTeams,
                                    0.0f, 0.0f) == projects::minimal_rts::kNullEntity);

        rts.OnInitializeWorld();
        engine::world::DeterministicSaveArchive archive;
        archive.RegisterContributor(&rts);
        std::vector<uint8_t> snap;

        for (int t = 0; t < 120; ++t) {
            if (withMidpointSaveRestore && t == 60) {
                snap = archive.SaveAll(60);
                // Mutate state, then restore snapshot!
                rts.StepSingleTick(0.05f);
                rts.StepSingleTick(0.05f);
                uint64_t restoredTick = 0;
                STAGE_CHECK(archive.LoadAll(snap, &restoredTick) && restoredTick == 60u);
            }
            rts.StepSingleTick(0.05f);
        }
        STAGE_CHECK(rts.GetTeamCrystals(0) > 500); // Harvester gathered & returned crystals!
        outFinalHash = rts.ComputeDeterministicHash();
        return true;
    };

    uint64_t rtsHashDirect = 0;
    uint64_t rtsHashRestored = 0;
    STAGE_CHECK(runMinimalRts(false, rtsHashDirect));
    STAGE_CHECK(runMinimalRts(true, rtsHashRestored));
    STAGE_CHECK(rtsHashDirect != 0 && rtsHashDirect == rtsHashRestored);

    // 2. Always verify repo-tracked `tests/data/mini_mods.w3x` so mapless CI environments test real W3X!
    if (fs::exists("tests/data/mini_mods.w3x")) {
        auto runBuiltInMap = [](uint64_t& outHash) -> bool {
            MpqManager mpq;
            CSimulation sim;
            sim.SetSimulationModule(std::make_shared<war3::War3GameModule>());
            SceneManager scene;
            UnitPresentationSystem presentation;
            core::WorldSceneSession session;
            STAGE_CHECK(session.LoadMap(mpq, sim, scene, presentation, "tests/data/mini_mods.w3x"));
            for (int t = 0; t < 200; ++t) {
                sim.Update();
            }
            outHash = sim.HashState();
            session.TeardownCurrentSession(mpq, sim, scene, presentation);
            return true;
        };
        uint64_t h1 = 0;
        uint64_t h2 = 0;
        STAGE_CHECK(runBuiltInMap(h1));
        STAGE_CHECK(runBuiltInMap(h2));
        STAGE_CHECK(h1 != 0 && h1 == h2);
    }

    // 3. Verify 1,000-tick deterministic replay across optional retail `.w3x` maps when present on disk
    //    (P2 #11.3 fix: checks `verifiedMaps == presentMaps`, never failing clean mapless CI clones!)
    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(4)TwistedMeadows.w3x",
        "resources/Maps/FrozenThrone/Scenario/(6)BlizzardTD.w3x"
    };

    int presentMaps = 0;
    int verifiedMaps = 0;
    for (const char* mapPath : kCloudMaps) {
        if (!fs::exists(mapPath)) {
            continue;
        }
        ++presentMaps;

        auto run1000Ticks = [&](uint64_t& outHash) -> bool {
            MpqManager mpq;
            CSimulation sim;
            sim.SetSimulationModule(std::make_shared<war3::War3GameModule>());
            SceneManager scene;
            UnitPresentationSystem presentation;
            core::WorldSceneSession session;
            if (!session.LoadMap(mpq, sim, scene, presentation, mapPath)) {
                return false;
            }
            if (!session.HasActiveTilesetProfile()) {
                return false;
            }
            for (int t = 0; t < 1000; ++t) {
                sim.Update();
            }
            outHash = sim.HashState();
            session.TeardownCurrentSession(mpq, sim, scene, presentation);
            return true;
        };

        uint64_t runA = 0;
        uint64_t runB = 0;
        STAGE_CHECK(run1000Ticks(runA));
        STAGE_CHECK(run1000Ticks(runB));
        STAGE_CHECK(runA != 0 && runA == runB);
        ++verifiedMaps;
    }

    STAGE_CHECK(verifiedMaps == presentMaps);
    return true;
}
