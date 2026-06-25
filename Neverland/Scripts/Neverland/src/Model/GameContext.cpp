#include "GameContext.hpp"

ChunksStorage* GameContext::s_chunkStorage = nullptr;
PandaAsync::Scheduler* GameContext::s_scheduler = nullptr;
int GameContext::s_pendingChunkJobs = 0;

void GameContext::init() {
    s_chunkStorage = new ChunksStorage();
    s_scheduler = new PandaAsync::Scheduler(PandaAsync::SchedulerDesc{3, "NeverlandChunks"});
    s_scheduler->registerMainThread();
    s_pendingChunkJobs = 0;
}

void GameContext::deinit() {
    // Cancel pending jobs and join workers before the voxel data they read is freed
    // and before the script module is unloaded.
    if (s_scheduler != nullptr) {
        s_scheduler->cancelAll();
        delete s_scheduler;
        s_scheduler = nullptr;
    }
    delete s_chunkStorage;
    s_chunkStorage = nullptr;
    s_pendingChunkJobs = 0;
}

bool GameContext::isWorldLoaded() {
    return s_chunkStorage != nullptr && s_pendingChunkJobs == 0;
}
