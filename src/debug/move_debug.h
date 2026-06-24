#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

inline std::filesystem::path MoveDebugLogPath() {
    return std::filesystem::path(PROJECT_SOURCE_DIR) / "move_debug.log";
}

inline void MoveDebugLogLine(const std::string& line) {
    std::cout << line << std::endl;
    std::ofstream logFile(MoveDebugLogPath(), std::ios::app);
    if (logFile) {
        logFile << line << '\n';
    }
}
