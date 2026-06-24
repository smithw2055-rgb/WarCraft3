#pragma once

enum class GameState : int {
    BOOTSTRAP,
    MENU,
    MAP_CONFIG,
    LOADING,
    IN_GAME,
    EXIT_GAME,
};
