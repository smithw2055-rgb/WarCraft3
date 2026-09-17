#include "../src/resource/mpq_storm.h"
#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: %s <mpq_dir>\n", argv[0]); return 1; }

    const char* mpqDir = argv[1];
    const char* mpqFiles[] = {"war3.mpq", "war3x.mpq", "War3Patch.mpq", "War3xLocal.mpq"};

    MpqManager mpq;
    for (size_t i = 0; i < sizeof(mpqFiles)/sizeof(mpqFiles[0]); i++) {
        mpq.Open(std::string(mpqDir) + "/" + mpqFiles[i]);
    }

    printf("Listing all files under Cliffs/ and Doodads/Terrain/CliffTrans:\n\n");

    auto allFiles = mpq.ListAllFiles("*");
    for (auto& f : allFiles) {
        if (f.find("Cliffs\\") == 0 || f.find("Doodads\\Terrain\\") == 0)
            printf("  %s\n", f.c_str());
    }

    printf("\nTotal files in MPQ: %zu\n", allFiles.size());
    return 0;
}
