#pragma once

#include "ChunksStorage.hpp"

class GameContext final {
public:
    static ChunksStorage* s_chunkStorage;

    static void init();
    static void deinit();
};