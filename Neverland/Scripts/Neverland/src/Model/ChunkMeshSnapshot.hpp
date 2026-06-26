#pragma once

#include "Chunk.hpp"

#include <array>
#include <cstdint>

struct ChunkMeshSnapshot {
    static constexpr int PADDING = 1;
    static constexpr int SIZE_X = Chunk::SIZE_X + PADDING * 2;
    static constexpr int SIZE_Y = Chunk::SIZE_Y + PADDING * 2;
    static constexpr int SIZE_Z = Chunk::SIZE_Z + PADDING * 2;
    static constexpr int VOXEL_COUNT = SIZE_X * SIZE_Y * SIZE_Z;

    ChunkCoord coord;
    uint32_t version = 0;
    std::array<Voxel, VOXEL_COUNT> voxels;

    static int index(int x, int y, int z) {
        return y * SIZE_X * SIZE_Z + x * SIZE_Z + z;
    }

    void setPadded(int x, int y, int z, Voxel voxel) {
        voxels[index(x, y, z)] = voxel;
    }

    const Voxel *getPadded(int x, int y, int z) const {
        if (x < 0 || y < 0 || z < 0 || x >= SIZE_X || y >= SIZE_Y || z >= SIZE_Z) {
            return nullptr;
        }
        return &voxels[index(x, y, z)];
    }

    const Voxel *getWorld(int x, int y, int z) const {
        const int originX = coord.x * Chunk::SIZE_X;
        const int originY = coord.y * Chunk::SIZE_Y;
        const int originZ = coord.z * Chunk::SIZE_Z;
        return getPadded(x - originX + PADDING, y - originY + PADDING, z - originZ + PADDING);
    }

    bool isAirWorld(int x, int y, int z) const {
        const Voxel *voxel = getWorld(x, y, z);
        return voxel == nullptr || !voxel->occludesFaces();
    }
};
