#include "GameContext.hpp"
#include "WorldSave.hpp"

#include <Bamboo/ApplicationAPI.hpp>

ChunksStorage *GameContext::s_chunkStorage = nullptr;
WorldSave *GameContext::s_worldSave = nullptr;
PandaAsync::Scheduler *GameContext::s_scheduler = nullptr;
int GameContext::s_pendingChunkJobs = 0;
int GameContext::s_pendingRegionJobs = 0;
MaterialHandle GameContext::s_terrainMaterial = {};
MaterialHandle GameContext::s_blocksMaterial = {};

void GameContext::init() {
    // Сейв грузится ДО создания хранилища: первые же ensureChunk накладывают дельты.
    s_worldSave = new WorldSave();
    const std::string saveDirectory = Bamboo::ApplicationAPI::getPersistentDataPath();
    if (!saveDirectory.empty()) {
        s_worldSave->load(saveDirectory + "/neverland_world.sav");
    }
    s_chunkStorage = new ChunksStorage();
    s_scheduler = new PandaAsync::Scheduler(PandaAsync::SchedulerDesc{1, "NeverlandChunks"});
    s_scheduler->registerMainThread();
    s_pendingChunkJobs = 0;
    s_pendingRegionJobs = 0;
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
    delete s_worldSave;
    s_worldSave = nullptr;
    s_pendingChunkJobs = 0;
    s_pendingRegionJobs = 0;
    s_terrainMaterial = {};
    s_blocksMaterial = {};
}

bool GameContext::isWorldLoaded() {
    return s_chunkStorage != nullptr && s_pendingChunkJobs == 0 && s_pendingRegionJobs == 0;
}
