//
// Created by Admin on 12.02.2022.
//

#pragma once

#include "Chunk.hpp"
#include "ChunkMeshSnapshot.hpp"
#include "VoxelRaycastData.hpp"

#include <optional>

class ChunksStorage {
public:
    static const int SIZE_X = 10;
    static const int SIZE_Y = 4;
    static const int SIZE_Z = 10;

    static const int WORLD_SIZE_X = ChunksStorage::SIZE_X * Chunk::SIZE_X;
    static const int WORLD_SIZE_Y = ChunksStorage::SIZE_Y * Chunk::SIZE_Y;
    static const int WORLD_SIZE_Z = ChunksStorage::SIZE_Z * Chunk::SIZE_Z;

    Chunk *chunks;

    ChunksStorage();
    ~ChunksStorage();

    static bool isWorldCoordInBounds(int x, int y, int z);
    static bool isChunkCoordInBounds(const ChunkCoord &coord);
    static ChunkCoord worldToChunkCoord(int x, int y, int z);
    static int worldToLocalX(int x);
    static int worldToLocalY(int y);
    static int worldToLocalZ(int z);

    bool setVoxel(int x, int y, int z, VoxelType type);
    Voxel *getVoxel(int x, int y, int z);
    const Voxel *getVoxel(int x, int y, int z) const;
    Chunk *getChunk(const ChunkCoord &coord);
    const Chunk *getChunk(const ChunkCoord &coord) const;
    Chunk *getChunk(int x, int y, int z);
    const Chunk *getChunk(int x, int y, int z) const;
    bool makeMeshSnapshot(const ChunkCoord &coord, ChunkMeshSnapshot &snapshot) const;
    std::optional<VoxelRaycastData>
    bresenham3D(float x1, float y1, float z1, float x2, float y2, float z2, int maximumDistance);

private:
    static int floorDiv(int value, int divisor);
    static int floorMod(int value, int divisor);
    static int chunkIndex(const ChunkCoord &coord);
};
