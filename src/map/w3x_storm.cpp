#include "w3x_storm.h"
#include "w3e.h"
#ifndef __STORMLIB_NO_STATIC_LINK__
#define __STORMLIB_NO_STATIC_LINK__
#endif
#include <StormLib.h>
#include <iostream>
#include <vector>

bool W3XLoadTerrain(const std::string& mapPath, W3EMap& out) {
    HANDLE hMpq = nullptr;
    if (!SFileOpenArchive(mapPath.c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) {
        std::cerr << "W3X: failed to open " << mapPath << std::endl;
        return false;
    }

    HANDLE hFile = nullptr;
    if (!SFileOpenFileEx(hMpq, "war3map.w3e", SFILE_OPEN_BASE_FILE, &hFile)) {
        std::cerr << "W3X: war3map.w3e not found in " << mapPath << std::endl;
        SFileCloseArchive(hMpq);
        return false;
    }

    DWORD fileSize = SFileGetFileSize(hFile, nullptr);
    std::vector<uint8_t> buffer(fileSize);
    DWORD bytesRead = 0;

    if (!SFileReadFile(hFile, buffer.data(), fileSize, &bytesRead, nullptr)) {
        std::cerr << "W3X: failed to read war3map.w3e" << std::endl;
        SFileCloseFile(hFile);
        SFileCloseArchive(hMpq);
        return false;
    }

    SFileCloseFile(hFile);
    SFileCloseArchive(hMpq);

    return W3ELoad(buffer.data(), bytesRead, out);
}
