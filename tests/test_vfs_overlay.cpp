// VFS overlay tests: folder shadows archive, last-added wins (ref
// CompoundDataSource/FolderDataSource over MpqDataSource, map wins).
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "resource/mpq_storm.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)

namespace fs = std::filesystem;

namespace {

fs::path UniqueTempDir(const std::string& tag) {
    static int counter = 0;
    fs::path dir = fs::temp_directory_path() /
        ("wc3vfs_" + tag + "_" + std::to_string(counter++));
    fs::create_directories(dir);
    return dir;
}

void WriteFile(const fs::path& p, const std::string& body) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary);
    f.write(body.data(), (std::streamsize)body.size());
}

std::string ReadAll(MpqManager& vfs, const std::string& name) {
    std::vector<uint8_t> out;
    if (!vfs.ReadFile(name, out)) return {};
    return std::string(out.begin(), out.end());
}

#ifdef WITHOUT_STORMLIB
const uint32_t* TestMpqCryptTable() {
    static uint32_t table[0x500];
    static bool init = false;
    if (!init) {
        uint32_t seed = 0x00100001u;
        for (uint32_t idx1 = 0; idx1 < 0x100u; ++idx1) {
            uint32_t idx2 = idx1;
            for (int i = 0; i < 5; ++i, idx2 += 0x100u) {
                seed = (seed * 125u + 3u) % 0x2AAAABu;
                uint32_t temp1 = (seed & 0xFFFFu) << 16u;
                seed = (seed * 125u + 3u) % 0x2AAAABu;
                uint32_t temp2 = (seed & 0xFFFFu);
                table[idx2] = temp1 | temp2;
            }
        }
        init = true;
    }
    return table;
}

uint32_t TestMpqHash(const std::string& str, uint32_t hashType) {
    const uint32_t* crypt = TestMpqCryptTable();
    uint32_t seed1 = 0x7FED7FEDu;
    uint32_t seed2 = 0xEEEEEEEEu;
    for (unsigned char c : str) {
        if (c == '/') c = '\\';
        c = static_cast<unsigned char>(std::toupper(c));
        seed1 = crypt[(hashType << 8u) + c] ^ (seed1 + seed2);
        seed2 = c + seed1 + seed2 + (seed2 << 5u) + 3u;
    }
    return seed1;
}

void TestMpqEncryptBlock(void* buf, size_t byteLen, uint32_t key) {
    const uint32_t* crypt = TestMpqCryptTable();
    uint32_t seed = 0xEEEEEEEEu;
    uint32_t* dw = static_cast<uint32_t*>(buf);
    for (size_t i = 0; i < byteLen / 4u; ++i) {
        seed += crypt[0x400u + (key & 0xFFu)];
        uint32_t plain = dw[i];
        dw[i] = plain ^ (key + seed);
        key = (((~key) << 0x15u) + 0x11111111u) | (key >> 0x0Bu);
        seed = plain + seed + (seed << 5u) + 3u;
    }
}
#endif

bool MakeTinyMpq(const fs::path& mpqPath, const std::string& innerName,
                 const std::string& body) {
#ifdef WITHOUT_STORMLIB
    struct HEntry { uint32_t n1, n2; uint16_t loc, plat; uint32_t blk; };
    struct BEntry { uint32_t pos, csize, fsize, flags; };
    constexpr uint32_t kHashCount = 16;
    const uint32_t dataOffset = 32;
    const uint32_t hashOffset = dataOffset + static_cast<uint32_t>((body.size() + 3u) & ~3u);
    const uint32_t blockOffset = hashOffset + kHashCount * 16u;
    const uint32_t archiveSize = blockOffset + 16u;

    std::vector<uint8_t> out(archiveSize, 0);
    std::memcpy(out.data(), "MPQ\x1a", 4);
    uint32_t hdrSize = 32;
    uint32_t archSz = archiveSize;
    uint16_t fmtVer = 0;
    uint16_t blkShift = 3;
    uint32_t hCnt = kHashCount;
    uint32_t bCnt = 1;
    std::memcpy(out.data() + 4, &hdrSize, 4);
    std::memcpy(out.data() + 8, &archSz, 4);
    std::memcpy(out.data() + 12, &fmtVer, 2);
    std::memcpy(out.data() + 14, &blkShift, 2);
    std::memcpy(out.data() + 16, &hashOffset, 4);
    std::memcpy(out.data() + 20, &blockOffset, 4);
    std::memcpy(out.data() + 24, &hCnt, 4);
    std::memcpy(out.data() + 28, &bCnt, 4);
    if (!body.empty()) {
        std::memcpy(out.data() + dataOffset, body.data(), body.size());
    }

    std::vector<HEntry> ht(kHashCount, {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFFFFFu});
    uint32_t slot = TestMpqHash(innerName, 0) & (kHashCount - 1u);
    ht[slot] = {TestMpqHash(innerName, 1), TestMpqHash(innerName, 2), 0, 0, 0};
    TestMpqEncryptBlock(ht.data(), ht.size() * sizeof(HEntry), TestMpqHash("(hash table)", 3));
    std::memcpy(out.data() + hashOffset, ht.data(), ht.size() * sizeof(HEntry));

    BEntry bt = {dataOffset, static_cast<uint32_t>(body.size()), static_cast<uint32_t>(body.size()), 0x81000000u};
    TestMpqEncryptBlock(&bt, sizeof(BEntry), TestMpqHash("(block table)", 3));
    std::memcpy(out.data() + blockOffset, &bt, sizeof(BEntry));

    std::ofstream f(mpqPath, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return f.good();
#else
    HANDLE hMpq = nullptr;
    if (!SFileCreateArchive(mpqPath.string().c_str(), MPQ_CREATE_ARCHIVE_V1, 16, &hMpq)) {
        return false;
    }
    bool ok = false;
    HANDLE hFile = nullptr;
    DWORD size = (DWORD)body.size();
    if (SFileCreateFile(hMpq, innerName.c_str(), 0, size, 0, MPQ_FILE_SINGLE_UNIT, &hFile)) {
        DWORD written = 0;
        ok = SFileWriteFile(hFile, body.data(), size, 0) &&
             SFileFinishFile(hFile);
    }
    SFileCloseArchive(hMpq);
    return ok;
#endif
}

} // namespace

bool TestVfsFolderLastAddedWins() {
    MpqManager vfs;
    fs::path base = UniqueTempDir("base");
    fs::path over = UniqueTempDir("over");
    WriteFile(base / "Units" / "overlay_test.txt", "base-content");
    WriteFile(over / "Units" / "overlay_test.txt", "over-content");
    TEST_ASSERT(vfs.AddFolder(base.string()));
    TEST_ASSERT(vfs.AddFolder(over.string()));
    TEST_ASSERT(vfs.FolderCount() == 2);
    // Backslash (archive-style) and slash spellings both resolve.
    TEST_ASSERT(ReadAll(vfs, "Units\\overlay_test.txt") == "over-content");
    TEST_ASSERT(ReadAll(vfs, "Units/overlay_test.txt") == "over-content");
    TEST_ASSERT(vfs.FileExists("Units/overlay_test.txt"));
    TEST_ASSERT(!vfs.FileExists("Units/does_not_exist_xyz.txt"));
    fs::remove_all(base);
    fs::remove_all(over);
    return true;
}

bool TestVfsFolderShadowsArchive() {
    fs::path resources = fs::path(PROJECT_SOURCE_DIR) / "resources";
    if (!fs::exists(resources / "war3.mpq")) return true; // SKIP without data
    MpqManager vfs;
    TEST_ASSERT(vfs.Open((resources / "war3.mpq").string()));
    std::vector<uint8_t> archived;
    TEST_ASSERT(vfs.ReadFile("Units\\MiscData.txt", archived));
    TEST_ASSERT(!archived.empty());
    fs::path over = UniqueTempDir("shadow");
    WriteFile(over / "Units" / "MiscData.txt", "shadow-content");
    TEST_ASSERT(vfs.AddFolder(over.string()));
    TEST_ASSERT(ReadAll(vfs, "Units\\MiscData.txt") == "shadow-content");
    fs::remove_all(over);
    return true;
}

bool TestVfsLastOpenedArchiveWins() {
    fs::path dir = UniqueTempDir("mpqs");
    fs::path first = dir / "first.mpq";
    fs::path second = dir / "second.mpq";
    if (!MakeTinyMpq(first, "test\\order.txt", "first-content")) {
        std::error_code ec;
        fs::remove_all(dir, ec);
        return true; // SKIP: MPQ creation unsupported here
    }
    TEST_ASSERT(MakeTinyMpq(second, "test\\order.txt", "second-content"));
    {
        MpqManager vfs;
        TEST_ASSERT(vfs.Open(first.string()));
        // Game archives: first opened wins so patches beat base data.
        TEST_ASSERT(vfs.Open(second.string()));
        TEST_ASSERT(ReadAll(vfs, "test\\order.txt") == "first-content");
    }
    {
        // Map mounts shadow every game archive (ref CompoundDataSource).
        MpqManager withMap;
        TEST_ASSERT(withMap.Open(first.string()));
        TEST_ASSERT(withMap.OpenOnTop(second.string()));
        TEST_ASSERT(ReadAll(withMap, "test\\order.txt") == "second-content");
        TEST_ASSERT(ReadAll(withMap, "test/order.txt") == "second-content");
    }
    std::error_code ec;
    fs::remove_all(dir, ec);
    return true;
}
