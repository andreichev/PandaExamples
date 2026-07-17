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
    int lodHandoffChunks = 2;
    MaterialHandle material;        // рукотворные блоки/стены (base_materials_1)
    MaterialHandle terrainMaterial; // гладкий рельеф (base_ground_1)

    PANDA_FIELDS_BEGIN(BaseScript)
    PANDA_FIELD(var)
    PANDA_FIELD(renderDistance)
    PANDA_FIELD(nearChunkDistance)
    PANDA_FIELD(lodHandoffChunks)
    PANDA_FIELD(material)
    PANDA_FIELD(terrainMaterial)
    PANDA_FIELDS_END

    void start() override;
    void update(float dt) override;
    void shutdown() override;

private:
    ChunkCoord getPlayerChunkCenter();
    ChunkCoord getRegionLodCenter(const ChunkCoord &center);
    int getEffectiveRenderDistance();
    int getEffectiveNearDistance(int effectiveRenderDistance);
    int getExactBuildDistance(int effectiveRenderDistance, int effectiveNearDistance);
    void rebuildStreamingQueues(
        const ChunkCoord &center,
        const ChunkCoord &regionCenter,
        int effectiveRenderDistance,
        int effectiveNearDistance
    );
    void cleanupStreamingViews(
        const ChunkCoord &center,
        const ChunkCoord &regionCenter,
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
    bool scheduleNextChunkMesh();
    bool scheduleChunkMesh(const ChunkCoord &coord);
    bool scheduleNextRegionMesh();
    bool scheduleRegionMesh(const RegionMeshBuildRequest &request);

    EntityHandle m_streamingTarget;
    ChunkCoord m_streamingCenter;
    int m_streamingRenderDistance = -1;
    int m_streamingNearDistance = -1;
    int m_streamingExactBuildDistance = -1;
    float m_statsLogTimer = 0.0f;
    bool m_hasStreamingCenter = false;
    bool m_hasRegionLodCenter = false;
    bool m_warnedRenderDistanceClamp = false;
    ChunkCoord m_regionLodCenter;
    std::deque<ChunkCoord> m_chunkLoadQueue;
    std::deque<ChunkCoord> m_meshBuildQueue;
    std::deque<RegionMeshBuildRequest> m_regionMeshBuildQueue;
    std::unordered_set<RegionMeshKey, RegionMeshKeyHash> m_activeRegionKeys;
};

REGISTER_SCRIPT(BaseScript)
