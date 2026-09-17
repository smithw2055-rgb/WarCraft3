#include "resource/mpq_storm.h"

#ifndef __STORMLIB_NO_STATIC_LINK__
#define __STORMLIB_NO_STATIC_LINK__
#endif
#include <StormLib.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void Usage(const char* exe) {
    std::cerr
        << "Usage:\n"
        << "  " << exe << " <mpq_dir> --list <pattern>\n"
        << "  " << exe << " <mpq_dir> --extract <mpq_path> <output_path>\n"
        << "  " << exe << " --archive <archive_path> --extract <mpq_path> <output_path>\n";
}

static bool OpenDefaultArchives(MpqManager& mpq, const fs::path& mpqDir) {
    const char* archives[] = {
        "War3Patch.mpq",
        "War3xLocal.mpq",
        "war3x.mpq",
        "war3.mpq"
    };

    bool openedAny = false;
    for (const char* archive : archives) {
        const fs::path path = mpqDir / archive;
        if (mpq.Open(path.string())) {
            openedAny = true;
            std::cerr << "opened " << path.string() << "\n";
        }
    }
    return openedAny;
}

int main(int argc, char** argv) {
    if (argc >= 6 && std::string(argv[1]) == "--archive" && std::string(argv[3]) == "--extract") {
        HANDLE hArchive = nullptr;
        if (!SFileOpenArchive(argv[2], 0, MPQ_OPEN_READ_ONLY, &hArchive)) {
            std::cerr << "failed to open archive " << argv[2] << "\n";
            return 2;
        }

        HANDLE hFile = nullptr;
        if (!SFileOpenFileEx(hArchive, argv[4], SFILE_OPEN_BASE_FILE, &hFile)) {
            std::cerr << "failed to read " << argv[4] << "\n";
            SFileCloseArchive(hArchive);
            return 3;
        }

        DWORD size = SFileGetFileSize(hFile, nullptr);
        std::vector<uint8_t> bytes(size);
        DWORD read = 0;
        const bool ok = SFileReadFile(hFile, bytes.data(), size, &read, nullptr) && read == size;
        SFileCloseFile(hFile);
        SFileCloseArchive(hArchive);
        if (!ok) {
            std::cerr << "failed to read bytes for " << argv[4] << "\n";
            return 3;
        }

        const fs::path outputPath = argv[5];
        const fs::path parent = outputPath.parent_path();
        if (!parent.empty()) {
            fs::create_directories(parent);
        }

        std::ofstream out(outputPath, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "failed to write " << outputPath.string() << "\n";
            return 4;
        }
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out.good()) {
            std::cerr << "write failed for " << outputPath.string() << "\n";
            return 5;
        }

        std::cout << "extracted " << argv[4] << " from " << argv[2]
                  << " -> " << outputPath.string()
                  << " bytes=" << bytes.size() << "\n";
        return 0;
    }

    if (argc < 4) {
        Usage(argv[0]);
        return 1;
    }

    MpqManager mpq;
    if (!OpenDefaultArchives(mpq, argv[1])) {
        std::cerr << "No MPQ archives opened from " << argv[1] << "\n";
        return 2;
    }

    const std::string command = argv[2];
    if (command == "--list") {
        const std::string pattern = argv[3];
        std::vector<std::string> files = mpq.ListAllFiles(pattern);
        for (const auto& file : files) {
            std::cout << file << "\n";
        }
        std::cerr << "matches=" << files.size() << "\n";
        return 0;
    }

    if (command == "--extract") {
        if (argc < 5) {
            Usage(argv[0]);
            return 1;
        }

        const std::string mpqPath = argv[3];
        std::vector<uint8_t> bytes;
        if (!mpq.ReadFile(mpqPath, bytes)) {
            std::cerr << "failed to read " << mpqPath << "\n";
            return 3;
        }

        const fs::path outputPath = argv[4];
        const fs::path parent = outputPath.parent_path();
        if (!parent.empty()) {
            fs::create_directories(parent);
        }

        std::ofstream out(outputPath, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "failed to write " << outputPath.string() << "\n";
            return 4;
        }
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out.good()) {
            std::cerr << "write failed for " << outputPath.string() << "\n";
            return 5;
        }

        std::cout << "extracted " << mpqPath << " -> " << outputPath.string()
                  << " bytes=" << bytes.size() << "\n";
        return 0;
    }

    Usage(argv[0]);
    return 1;
}
