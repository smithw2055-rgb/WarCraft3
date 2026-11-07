#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

inline std::filesystem::path MoveDebugLogPath() {
    return std::filesystem::path(PROJECT_SOURCE_DIR) / "move_debug.log";
}

inline bool& MoveDebugLoggingEnabledFlag() {
    static bool enabled = (std::getenv("WARSMASH_MOVE_DEBUG") != nullptr);
    return enabled;
}

inline void SetMoveDebugLoggingEnabled(bool enabled) {
    MoveDebugLoggingEnabledFlag() = enabled;
}

inline bool IsMoveDebugLoggingEnabled() {
    return MoveDebugLoggingEnabledFlag();
}

inline void MoveDebugLogLine(const std::string& line) {
    if (!MoveDebugLoggingEnabledFlag()) {
        return;
    }
    std::cout << line << std::endl;
    std::ofstream logFile(MoveDebugLogPath(), std::ios::app);
    if (logFile) {
        logFile << line << '\n';
    }
}
