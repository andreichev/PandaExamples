//
// Created by Admin on 12.02.2022.
//

#include "ChunksStorage.hpp"
#include "PerlinNoise.hpp"
#include "Voxel.hpp"

#include <Bamboo/Logger.hpp>

#include <cmath>
#include <limits>

ChunksStorage::~ChunksStorage() {
    delete[] chunks;
}

ChunksStorage::ChunksStorage() {
    LOG_INFO("NOISE GENERATION STARTED, number of chunks: %d", SIZE_X * SIZE_Y * SIZE_Z);
    chunks = new Chunk[SIZE_X * SIZE_Y * SIZE_Z];
    for (int chunkY = 0; chunkY < SIZE_Y; chunkY++) {
        for (int chunkX = 0; chunkX < SIZE_X; chunkX++) {
            for (int chunkZ = 0; chunkZ < SIZE_Z; chunkZ++) {
                ChunkCoord coord{chunkX, chunkY, chunkZ};
                chunks[chunkIndex(coord)].setCoord(coord);
            }
        }
    }

    float terrain[WORLD_SIZE_X * WORLD_SIZE_Z];
    PerlinNoise::generate2DCustom(2, 4, 1.0f, terrain, WORLD_SIZE_X, WORLD_SIZE_Z);

    for (int x = 0; x < WORLD_SIZE_X; x++) {
        for (int y = 0; y < WORLD_SIZE_Y; y++) {
            for (int z = 0; z < WORLD_SIZE_Z; z++) {
                VoxelType voxelType;
                int height = (int)(terrain[x * WORLD_SIZE_Z + z] * WORLD_SIZE_Y);
                if (y < height) {
                    voxelType = y <= 2 ? VoxelType::STONE : VoxelType::GROUND;
                } else if (y == height) {
                    voxelType = VoxelType::GRASS;
                } else {
                    voxelType = VoxelType::NOTHING;
                }
                int chunkIndexX = x / Chunk::SIZE_X;
                int chunkIndexY = y / Chunk::SIZE_Y;
                int chunkIndexZ = z / Chunk::SIZE_Z;
                int voxelIndexX = x % Chunk::SIZE_X;
                int voxelIndexY = y % Chunk::SIZE_Y;
                int voxelIndexZ = z % Chunk::SIZE_Z;
                ChunkCoord coord{chunkIndexX, chunkIndexY, chunkIndexZ};
                chunks[chunkIndex(coord)]
                    .data()
                    .voxels[ChunkData::index(voxelIndexX, voxelIndexY, voxelIndexZ)]
                    .type = voxelType;
            }
        }
    }
}

int ChunksStorage::floorDiv(int value, int divisor) {
    int quotient = value / divisor;
    int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) { quotient--; }
    return quotient;
}

int ChunksStorage::floorMod(int value, int divisor) {
    int remainder = value % divisor;
    if (remainder < 0) { remainder += divisor; }
    return remainder;
}

bool ChunksStorage::isWorldCoordInBounds(int x, int y, int z) {
    return x >= 0 && y >= 0 && z >= 0 && x < WORLD_SIZE_X && y < WORLD_SIZE_Y && z < WORLD_SIZE_Z;
}

bool ChunksStorage::isChunkCoordInBounds(const ChunkCoord &coord) {
    return coord.x >= 0 && coord.y >= 0 && coord.z >= 0 && coord.x < SIZE_X && coord.y < SIZE_Y &&
           coord.z < SIZE_Z;
}

ChunkCoord ChunksStorage::worldToChunkCoord(int x, int y, int z) {
    return {floorDiv(x, Chunk::SIZE_X), floorDiv(y, Chunk::SIZE_Y), floorDiv(z, Chunk::SIZE_Z)};
}

int ChunksStorage::worldToLocalX(int x) {
    return floorMod(x, Chunk::SIZE_X);
}

int ChunksStorage::worldToLocalY(int y) {
    return floorMod(y, Chunk::SIZE_Y);
}

int ChunksStorage::worldToLocalZ(int z) {
    return floorMod(z, Chunk::SIZE_Z);
}

int ChunksStorage::chunkIndex(const ChunkCoord &coord) {
    return coord.y * SIZE_X * SIZE_Z + coord.x * SIZE_Z + coord.z;
}

bool ChunksStorage::setVoxel(int x, int y, int z, VoxelType type) {
    if (!isWorldCoordInBounds(x, y, z)) { return false; }
    Chunk *chunk = getChunk(worldToChunkCoord(x, y, z));
    if (chunk == nullptr) { return false; }
    return chunk->set(worldToLocalX(x), worldToLocalY(y), worldToLocalZ(z), type);
}

Voxel *ChunksStorage::getVoxel(int x, int y, int z) {
    if (!isWorldCoordInBounds(x, y, z)) { return nullptr; }
    Chunk *chunk = getChunk(worldToChunkCoord(x, y, z));
    if (chunk == nullptr) { return nullptr; }
    return chunk->get(worldToLocalX(x), worldToLocalY(y), worldToLocalZ(z));
}

const Voxel *ChunksStorage::getVoxel(int x, int y, int z) const {
    if (!isWorldCoordInBounds(x, y, z)) { return nullptr; }
    const Chunk *chunk = getChunk(worldToChunkCoord(x, y, z));
    if (chunk == nullptr) { return nullptr; }
    return chunk->get(worldToLocalX(x), worldToLocalY(y), worldToLocalZ(z));
}

Chunk *ChunksStorage::getChunk(const ChunkCoord &coord) {
    if (!isChunkCoordInBounds(coord)) { return nullptr; }
    return &chunks[chunkIndex(coord)];
}

const Chunk *ChunksStorage::getChunk(const ChunkCoord &coord) const {
    if (!isChunkCoordInBounds(coord)) { return nullptr; }
    return &chunks[chunkIndex(coord)];
}

Chunk *ChunksStorage::getChunk(int x, int y, int z) {
    if (!isWorldCoordInBounds(x, y, z)) { return nullptr; }
    return getChunk(worldToChunkCoord(x, y, z));
}

const Chunk *ChunksStorage::getChunk(int x, int y, int z) const {
    if (!isWorldCoordInBounds(x, y, z)) { return nullptr; }
    return getChunk(worldToChunkCoord(x, y, z));
}

bool ChunksStorage::makeMeshSnapshot(const ChunkCoord &coord, ChunkMeshSnapshot &snapshot) const {
    const Chunk *chunk = getChunk(coord);
    if (chunk == nullptr) { return false; }

    snapshot.coord = coord;
    snapshot.version = chunk->getVersion();

    const int originX = coord.x * Chunk::SIZE_X;
    const int originY = coord.y * Chunk::SIZE_Y;
    const int originZ = coord.z * Chunk::SIZE_Z;

    for (int localY = -ChunkMeshSnapshot::PADDING;
         localY < Chunk::SIZE_Y + ChunkMeshSnapshot::PADDING;
         localY++) {
        for (int localX = -ChunkMeshSnapshot::PADDING;
             localX < Chunk::SIZE_X + ChunkMeshSnapshot::PADDING;
             localX++) {
            for (int localZ = -ChunkMeshSnapshot::PADDING;
                 localZ < Chunk::SIZE_Z + ChunkMeshSnapshot::PADDING;
                 localZ++) {
                const Voxel *voxel = getVoxel(originX + localX, originY + localY, originZ + localZ);
                snapshot.setPadded(
                    localX + ChunkMeshSnapshot::PADDING,
                    localY + ChunkMeshSnapshot::PADDING,
                    localZ + ChunkMeshSnapshot::PADDING,
                    voxel != nullptr ? *voxel : Voxel(VoxelType::NOTHING)
                );
            }
        }
    }
    return true;
}

std::optional<VoxelRaycastData> ChunksStorage::bresenham3D(
    float x1, float y1, float z1, float x2, float y2, float z2, int maximumDistance
) {
    float t = 0.0f;
    int ix = (int)floor(x1);
    int iy = (int)floor(y1);
    int iz = (int)floor(z1);

    float stepx = (x2 > 0.0f) ? 1.0f : -1.0f;
    float stepy = (y2 > 0.0f) ? 1.0f : -1.0f;
    float stepz = (z2 > 0.0f) ? 1.0f : -1.0f;

    float infinity = std::numeric_limits<float>::infinity();

    float txDelta = (x2 == 0.0f) ? infinity : abs(1.0f / x2);
    float tyDelta = (y2 == 0.0f) ? infinity : abs(1.0f / y2);
    float tzDelta = (z2 == 0.0f) ? infinity : abs(1.0f / z2);

    float xdist = (stepx > 0) ? (ix + 1 - x1) : (x1 - ix);
    float ydist = (stepy > 0) ? (iy + 1 - y1) : (y1 - iy);
    float zdist = (stepz > 0) ? (iz + 1 - z1) : (z1 - iz);

    float txMax = (txDelta < infinity) ? txDelta * xdist : infinity;
    float tyMax = (tyDelta < infinity) ? tyDelta * ydist : infinity;
    float tzMax = (tzDelta < infinity) ? tzDelta * zdist : infinity;

    int steppedIndex = -1;

    Vec3 end;
    Vec3 normal;

    while (t <= maximumDistance) {
        Voxel *voxel = getVoxel(ix, iy, iz);
        if (voxel != nullptr && voxel->type != VoxelType::NOTHING) {
            end.x = ix;
            end.y = iy;
            end.z = iz;

            normal.x = 0;
            normal.y = 0;
            normal.z = 0;
            if (steppedIndex == 0) normal.x = (int)-stepx;
            if (steppedIndex == 1) normal.y = (int)-stepy;
            if (steppedIndex == 2) normal.z = (int)-stepz;
            return VoxelRaycastData(voxel, end, normal);
        }
        if (txMax < tyMax) {
            if (txMax < tzMax) {
                ix += stepx;
                t = txMax;
                txMax += txDelta;
                steppedIndex = 0;
            } else {
                iz += stepz;
                t = tzMax;
                tzMax += tzDelta;
                steppedIndex = 2;
            }
        } else {
            if (tyMax < tzMax) {
                iy += stepy;
                t = tyMax;
                tyMax += tyDelta;
                steppedIndex = 1;
            } else {
                iz += stepz;
                t = tzMax;
                tzMax += tzDelta;
                steppedIndex = 2;
            }
        }
    }
    return {};
}
