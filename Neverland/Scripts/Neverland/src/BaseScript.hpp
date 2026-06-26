#pragma once

#include "Model/ChunksStorage.hpp"

#include <Bamboo/Script.hpp>
#include <Bamboo/Bamboo.hpp>

#include <deque>
#include <unordered_set>

using namespace Bamboo;

class BaseScript : public Script {
public:
    int var;
    int renderDistance = 5;
    int nearChunkDistance = 8;
    MaterialHandle material;

    PANDA_FIELDS_BEGIN(BaseScript)
    PANDA_FIELD(var)
    PANDA_FIELD(renderDistance)
    PANDA_FIELD(nearChunkDistance)
    PANDA_FIELD(material)
    PANDA_FIELDS_END

    void start() override;
    void update(float dt) override;
    void shutdown() override;

private:
    ChunkCoord getPlayerChunkCenter();
    int getEffectiveRenderDistance();
    int getEffectiveNearDistance(int effectiveRenderDistance);
    int getExactBuildDistance(int effectiveRenderDistance, int effectiveNearDistance);
    void rebuildStreamingQueues(
        const ChunkCoord &center, int effectiveRenderDistance, int effectiveNearDistance
    );
    void cleanupStreamingViews(
        const ChunkCoord &center,
        int effectiveRenderDistance,
        int effectiveNearDistance,
        int exactBuildDistance
    );
    bool isChunkCoveredByReadyRegionLod(
        const ChunkCoord &coord,
        const ChunkCoord &center,
        int effectiveRenderDistance,
        int effectiveNearDistance
    ) const;
    bool shouldKeepInactiveRegionForExactFallback(
        const RegionMeshKey &key, const ChunkCoord &center, int exactBuildDistance
    ) const;
    bool hasExactChunkView(const ChunkCoord &coord) const;
    void updateStreaming();
    void logStreamingStats(float deltaTime);
    bool loadNextChunk();
    bool scheduleNextChunkMesh(MaterialHandle chunkMaterial);
    bool scheduleChunkMesh(const ChunkCoord &coord, MaterialHandle chunkMaterial);
    bool scheduleNextRegionMesh(MaterialHandle chunkMaterial);
    bool scheduleRegionMesh(const RegionMeshBuildRequest &request, MaterialHandle chunkMaterial);

    EntityHandle m_streamingTarget;
    ChunkCoord m_streamingCenter;
    int m_streamingRenderDistance = -1;
    int m_streamingNearDistance = -1;
    float m_statsLogTimer = 0.0f;
    bool m_hasStreamingCenter = false;
    bool m_warnedRenderDistanceClamp = false;
    std::deque<ChunkCoord> m_chunkLoadQueue;
    std::deque<ChunkCoord> m_meshBuildQueue;
    std::deque<RegionMeshBuildRequest> m_regionMeshBuildQueue;
    std::unordered_set<RegionMeshKey, RegionMeshKeyHash> m_activeRegionKeys;
};

REGISTER_SCRIPT(BaseScript)
