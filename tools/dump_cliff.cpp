#include "../src/resource/mpq_storm.h"
#include "../src/resource/slk.h"
#include <cstdio>
#include <vector>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: %s <mpq_dir>\n", argv[0]); return 1; }

    const char* mpqDir = argv[1];
    const char* mpqFiles[] = {"war3.mpq", "war3x.mpq", "War3Patch.mpq", "War3xLocal.mpq"};

    MpqManager mpq;
    for (size_t i = 0; i < sizeof(mpqFiles)/sizeof(mpqFiles[0]); i++) {
        std::string path = std::string(mpqDir) + "/" + mpqFiles[i];
        if (mpq.Open(path))
            printf("Opened: %s\n", path.c_str());
    }

    std::vector<uint8_t> buf;
    if (!mpq.ReadFile("TerrainArt\\CliffTypes.slk", buf)) {
        fprintf(stderr, "Failed to read CliffTypes.slk\n");
        return 1;
    }

    SlkTable slk;
    if (!slk.Load(buf.data(), buf.size())) {
        fprintf(stderr, "Failed to parse CliffTypes.slk\n");
        return 1;
    }

    printf("CliffTypes.slk loaded: %d cols x %d rows\n\n", slk.Cols(), slk.Rows());
    for (int c = 0; c < slk.Cols(); c++)
        printf("  col[%d] = \"%s\"\n", c, slk.colNames_[c].c_str());

    printf("\n--- All rows ---\n");
    for (int r = 0; r < slk.Rows(); r++) {
        printf("\nRow %d:\n", r);
        for (int c = 0; c < slk.Cols(); c++) {
            std::string val = slk.data_[r][c];
            if (!val.empty())
                printf("  %s = \"%s\"\n", slk.colNames_[c].c_str(), val.c_str());
        }
    }
    return 0;
}
