#pragma once

#include "ChunksStorage.hpp"

#include <PandaAsync/PandaAsync.hpp>

class GameContext final {
public:
    static ChunksStorage* s_chunkStorage;
    // Owns the chunk-mesh background jobs. Lives for the world's lifetime and is
    // torn down in deinit() before the script module is unloaded.
    static PandaAsync::Scheduler* s_scheduler;
    // Initial-build jobs still in flight (touched only on the main thread).
    static int s_pendingChunkJobs;

    static void init();
    static void deinit();

    // True once the initial chunk meshes have all been uploaded. Voxel edits are
    // disabled until then so worker threads never read voxels being mutated.
    static bool isWorldLoaded();
};
