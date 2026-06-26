//
// Created by Admin on 12.02.2022.
//

#pragma once

#include "Chunk.hpp"
#include "ChunkMeshSnapshot.hpp"
#include "RegionMesh.hpp"
#include "VoxelRaycastData.hpp"

#include <memory>
#include <optional>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <vector>

struct ChunksStorageStats {
    std::size_t loadedChunks = 0;
    std::size_t chunksWithView = 0;
    std::size_t regionsWithView = 0;
    std::size_t meshBuildQueuedChunks = 0;
    std::size_t regionMeshBuildQueued = 0;
    std::size_t dirtyChunks = 0;
    std::size_t dirtyRegions = 0;
    std::size_t modifiedChunks = 0;
    std::size_t vertices = 0;
    std::size_t indices = 0;
    std::size_t regionVertices = 0;
    std::size_t regionIndices = 0;
};

class ChunksStorage {
public:
    static constexpr int MIN_CHUNK_Y = 0;
    static constexpr int HEIGHT_CHUNKS = 1;
    static constexpr int MAX_CHUNK_Y = MIN_CHUNK_Y + HEIGHT_CHUNKS;
    static constexpr int WORLD_MIN_Y = MIN_CHUNK_Y * Chunk::SIZE_Y;
    static constexpr int WORLD_MAX_Y = MAX_CHUNK_Y * Chunk::SIZE_Y;
    static constexpr int WORLD_SIZE_Y = WORLD_MAX_Y - WORLD_MIN_Y;
    static constexpr int MAX_HORIZONTAL_CHUNK_COORD = 1'000'000;
    static constexpr int WORLD_HORIZONTAL_LIMIT = MAX_HORIZONTAL_CHUNK_COORD * Chunk::SIZE_X;

    ChunksStorage();
    ~ChunksStorage() = default;

    static bool isWorldCoordInBounds(int x, int y, int z);
    static bool isChunkCoordInBounds(const ChunkCoord &coord);
    static ChunkCoord worldToChunkCoord(int x, int y, int z);
    static int worldToLocalX(int x);
    static int worldToLocalY(int y);
    static int worldToLocalZ(int z);
    static int terrainHeight(int x, int z);

    bool setVoxel(int x, int y, int z, VoxelType type);
    Voxel *getVoxel(int x, int y, int z);
    const Voxel *getVoxel(int x, int y, int z) const;
    Chunk *ensureChunk(const ChunkCoord &coord);
    void ensureChunksAround(const ChunkCoord &center, int horizontalDistance);
    void unloadChunksOutside(const ChunkCoord &center, int horizontalDistance);
    void unloadChunksWithoutViewsOutside(const ChunkCoord &center, int horizontalDistance);
    void destroyChunkViewsOutside(const ChunkCoord &center, int horizontalDistance);
    void destroyChunkViewsOutsideIf(
        const ChunkCoord &center,
        int horizontalDistance,
        const std::function<bool(const ChunkCoord &)> &shouldDestroy
    );
    Chunk *getChunk(const ChunkCoord &coord);
    const Chunk *getChunk(const ChunkCoord &coord) const;
    Chunk *getChunk(int x, int y, int z);
    const Chunk *getChunk(int x, int y, int z) const;
    bool hasMeshNeighborhood(const ChunkCoord &coord) const;
    ChunksStorageStats collectStats() const;
    bool makeMeshSnapshot(const ChunkCoord &coord, ChunkMeshSnapshot &snapshot) const;
    RegionMesh *ensureRegion(const RegionMeshKey &key);
    RegionMesh *getRegion(const RegionMeshKey &key);
    const RegionMesh *getRegion(const RegionMeshKey &key) const;
    void unloadRegionsExcept(const std::unordered_set<RegionMeshKey, RegionMeshKeyHash> &activeRegions);
    void unloadRegionsExceptIf(
        const std::unordered_set<RegionMeshKey, RegionMeshKeyHash> &activeRegions,
        const std::function<bool(const RegionMeshKey &)> &shouldUnload
    );
    std::optional<VoxelRaycastData>
    bresenham3D(float x1, float y1, float z1, float x2, float y2, float z2, int maximumDistance);

private:
    static int floorDiv(int value, int divisor);
    static int floorMod(int value, int divisor);
    static float terrainNoise(int x, int z);
    static bool isInsideHorizontalDistance(
        const ChunkCoord &coord, const ChunkCoord &center, int horizontalDistance
    );
    void generateChunkData(Chunk &chunk);

    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> m_chunks;
    std::unordered_map<RegionMeshKey, std::unique_ptr<RegionMesh>, RegionMeshKeyHash> m_regions;
};
