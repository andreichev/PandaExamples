#pragma once

#include "Model/ChunksStorage.hpp"

#include <Bamboo/Script.hpp>
#include <Bamboo/Bamboo.hpp>

#include <deque>

using namespace Bamboo;

class BaseScript : public Script {
public:
    int var;
    int renderDistance = 5;
    MaterialHandle material;

    PANDA_FIELDS_BEGIN(BaseScript)
    PANDA_FIELD(var)
    PANDA_FIELD(renderDistance)
    PANDA_FIELD(material)
    PANDA_FIELDS_END

    void start() override;
    void update(float dt) override;
    void shutdown() override;

private:
    ChunkCoord getPlayerChunkCenter();
    int getEffectiveRenderDistance();
    void rebuildStreamingQueues(const ChunkCoord &center, int effectiveRenderDistance);
    void updateStreaming();
    void logStreamingStats(float deltaTime);
    bool loadNextChunk();
    bool scheduleNextChunkMesh(MaterialHandle chunkMaterial);
    bool scheduleChunkMesh(const ChunkCoord &coord, MaterialHandle chunkMaterial);

    EntityHandle m_streamingTarget;
    ChunkCoord m_streamingCenter;
    int m_streamingRenderDistance = -1;
    int m_framesUntilUnloadCleanup = 0;
    float m_statsLogTimer = 0.0f;
    bool m_hasStreamingCenter = false;
    bool m_warnedRenderDistanceClamp = false;
    std::deque<ChunkCoord> m_chunkLoadQueue;
    std::deque<ChunkCoord> m_meshBuildQueue;
};

REGISTER_SCRIPT(BaseScript)
