// VFS overlay tests: folder shadows archive, last-added wins (ref
// CompoundDataSource/FolderDataSource over MpqDataSource, map wins).
#include <cstdio>
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

bool MakeTinyMpq(const fs::path& mpqPath, const std::string& innerName,
                 const std::string& body) {
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
    MpqManager vfs;
    fs::path dir = UniqueTempDir("mpqs");
    fs::path first = dir / "first.mpq";
    fs::path second = dir / "second.mpq";
    if (!MakeTinyMpq(first, "test\\order.txt", "first-content")) {
        fs::remove_all(dir);
        return true; // SKIP: MPQ creation unsupported here
    }
    TEST_ASSERT(MakeTinyMpq(second, "test\\order.txt", "second-content"));
    TEST_ASSERT(vfs.Open(first.string()));
    // Game archives: first opened wins so patches beat base data.
    TEST_ASSERT(vfs.Open(second.string()));
    TEST_ASSERT(ReadAll(vfs, "test\\order.txt") == "first-content");
    // Map mounts shadow every game archive (ref CompoundDataSource).
    MpqManager withMap;
    TEST_ASSERT(withMap.Open(first.string()));
    TEST_ASSERT(withMap.OpenOnTop(second.string()));
    TEST_ASSERT(ReadAll(withMap, "test\\order.txt") == "second-content");
    TEST_ASSERT(ReadAll(withMap, "test/order.txt") == "second-content");
    fs::remove_all(dir);
    return true;
}
