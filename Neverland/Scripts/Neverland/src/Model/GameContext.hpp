#pragma once

#include "ChunksStorage.hpp"

#include <PandaAsync/PandaAsync.hpp>

class GameContext final {
public:
    static ChunksStorage *s_chunkStorage;
    // Owns the chunk-mesh background jobs. Lives for the world's lifetime and is
    // torn down in deinit() before the script module is unloaded.
    static PandaAsync::Scheduler *s_scheduler;
    // Chunk mesh jobs still in flight (touched only on the main thread).
    static int s_pendingChunkJobs;
    static int s_pendingRegionJobs;
    static MaterialHandle s_terrainMaterial; // гладкий рельеф (base_ground_1)
    static MaterialHandle s_blocksMaterial;  // рукотворные блоки/стены (base_materials_1)

    static void init();
    static void deinit();

    // True once scheduled chunk meshes have all been uploaded. Voxel edits are
    // still gated while streaming is catching up.
    static bool isWorldLoaded();
};
