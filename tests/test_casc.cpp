// CASC storage tests: graceful failure without a storage, manager
// plumbing, and a live exercise whenever a CASC-era install is present
// (this repo's fixtures are MPQ-era, so live coverage is best-effort).
#include <filesystem>
#include <iostream>

#include "resource/casc.h"
#include "resource/mpq_storm.h"

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace {

namespace fs = std::filesystem;

bool LooksLikeCascStorage(const fs::path& dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return false;
    // Local CASC roots carry .build.info (newer layouts nest under Data/).
    return fs::exists(dir / ".build.info", ec) || fs::exists(dir / "Data" / ".build.info", ec);
}

std::string FindLocalCascStorage() {
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    const char* candidates[] = {
        "Warcraft III", "Warcraft III/_retail_", "Data",
        "Program Files/Warcraft III", nullptr,
    };
    // Same directory, different casings (Windows installs vary).
    for (int i = 0; candidates[i]; ++i) {
        fs::path p = fs::path(res) / candidates[i];
        if (LooksLikeCascStorage(p)) return p.string();
        fs::path lower = fs::path(res);
        std::string c = candidates[i];
        for (auto& ch : c) ch = (char)tolower(ch);
        lower /= c;
        if (LooksLikeCascStorage(lower)) return lower.string();
    }
    return "";
}

} // namespace

bool TestCascGracefulFailure() {
    CascStorage storage;
    TEST_ASSERT(!storage.IsOpen());
    TEST_ASSERT(!storage.Open("/nonexistent/casc-storage-xyz"));
    TEST_ASSERT(!storage.IsOpen());
    std::vector<uint8_t> out;
    TEST_ASSERT(!storage.ReadFile("units\\human\\footman\\footman.mdx", out));
    TEST_ASSERT(out.empty());
    TEST_ASSERT(!storage.FileExists("units\\human\\footman\\footman.mdx"));
    TEST_ASSERT(storage.ListFiles("*.mdx").empty());
    storage.Close(); // safe on a closed storage
    storage.Close();
    return true;
}

bool TestCascManagerPlumbing() {
    MpqManager mpq;
    TEST_ASSERT(mpq.CascCount() == 0);
    TEST_ASSERT(!mpq.OpenCasc("/nonexistent/casc-storage-xyz"));
    TEST_ASSERT(mpq.CascCount() == 0);
    // Failed opens change nothing about MPQ behavior.
    TEST_ASSERT(!mpq.FileExists("units\\human\\footman\\footman.mdx"));
    std::vector<uint8_t> out;
    TEST_ASSERT(!mpq.ReadFile("units\\human\\footman\\footman.mdx", out));
    mpq.CloseAll();
    return true;
}

bool TestCascLiveIfPresent() {
    const std::string storage = FindLocalCascStorage();
    if (storage.empty()) return true; // MPQ-era fixtures: nothing to exercise
    MpqManager mpq;
    TEST_ASSERT(mpq.OpenCasc(storage));
    TEST_ASSERT(mpq.CascCount() == 1);
    TEST_ASSERT(mpq.OpenCasc(storage)); // idempotent re-open
    TEST_ASSERT(mpq.CascCount() == 1);
    auto names = mpq.ListAllFiles("*.mdx");
    TEST_ASSERT(!names.empty());
    std::vector<uint8_t> out;
    TEST_ASSERT(mpq.ReadFile(names.front(), out));
    TEST_ASSERT(!out.empty());
    TEST_ASSERT(mpq.FileExists(names.front()));
    mpq.CloseAll();
    TEST_ASSERT(mpq.CascCount() == 0);
    return true;
}
