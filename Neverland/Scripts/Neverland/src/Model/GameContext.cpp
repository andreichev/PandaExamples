#include "GameContext.hpp"

ChunksStorage* GameContext::s_chunkStorage = nullptr;

void GameContext::init() {
    s_chunkStorage = new ChunksStorage();
}

void GameContext::deinit() {
    delete s_chunkStorage;
}

