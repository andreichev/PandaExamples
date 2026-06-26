//
// Created by Admin on 12.02.2022.
//

#include "ChunksStorage.hpp"
#include "Voxel.hpp"

#include <Bamboo/Logger.hpp>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>

namespace {

float lerpFloat(float left, float right, float t) {
    return left + (right - left) * t;
}

float smoothStep(float value) {
    return value * value * (3.0f - 2.0f * value);
}

int localFloorDiv(int value, int divisor) {
    int quotient = value / divisor;
    int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) { quotient--; }
    return quotient;
}

int localFloorMod(int value, int divisor) {
    int remainder = value % divisor;
    if (remainder < 0) { remainder += divisor; }
    return remainder;
}

float hashNoise(int x, int z) {
    uint32_t hash = static_cast<uint32_t>(x) * 374761393u;
    hash += static_cast<uint32_t>(z) * 668265263u;
    hash += 0x9e3779b9u;
    hash = (hash ^ (hash >> 13u)) * 1274126177u;
    hash ^= hash >> 16u;
    return static_cast<float>(hash & 0x00ffffffu) / static_cast<float>(0x00ffffffu);
}

float valueNoise(int x, int z, int cellSize) {
    const int cellX = localFloorDiv(x, cellSize);
    const int cellZ = localFloorDiv(z, cellSize);
    const float localX =
        static_cast<float>(localFloorMod(x, cellSize)) / static_cast<float>(cellSize);
    const float localZ =
        static_cast<float>(localFloorMod(z, cellSize)) / static_cast<float>(cellSize);
    const float tx = smoothStep(localX);
    const float tz = smoothStep(localZ);

    const float n00 = hashNoise(cellX, cellZ);
    const float n10 = hashNoise(cellX + 1, cellZ);
    const float n01 = hashNoise(cellX, cellZ + 1);
    const float n11 = hashNoise(cellX + 1, cellZ + 1);
    const float nx0 = lerpFloat(n00, n10, tx);
    const float nx1 = lerpFloat(n01, n11, tx);
    return lerpFloat(nx0, nx1, tz);
}

} // namespace

ChunksStorage::ChunksStorage() {
    LOG_INFO("DYNAMIC WORLD STORAGE READY, horizontal chunk limit: %d", MAX_HORIZONTAL_CHUNK_COORD);
}

void ChunksStorage::generateChunkData(Chunk &chunk) {
    const ChunkCoord &coord = chunk.getCoord();
    const int originX = coord.x * Chunk::SIZE_X;
    const int originY = coord.y * Chunk::SIZE_Y;
    const int originZ = coord.z * Chunk::SIZE_Z;

    for (int localX = 0; localX < Chunk::SIZE_X; localX++) {
        for (int localZ = 0; localZ < Chunk::SIZE_Z; localZ++) {
            const int worldX = originX + localX;
            const int worldZ = originZ + localZ;
            const int height = terrainHeight(worldX, worldZ);

            for (int localY = 0; localY < Chunk::SIZE_Y; localY++) {
                const int worldY = originY + localY;
                VoxelType voxelType = VoxelType::NOTHING;
                if (worldY < height - 4) {
                    voxelType = VoxelType::STONE;
                } else if (worldY < height) {
                    voxelType = VoxelType::GROUND;
                } else if (worldY == height) {
                    voxelType = VoxelType::GRASS;
                }
                chunk.data().voxels[ChunkData::index(localX, localY, localZ)].type = voxelType;
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
    return x >= -WORLD_HORIZONTAL_LIMIT && x < WORLD_HORIZONTAL_LIMIT &&
           z >= -WORLD_HORIZONTAL_LIMIT && z < WORLD_HORIZONTAL_LIMIT && y >= WORLD_MIN_Y &&
           y < WORLD_MAX_Y;
}

bool ChunksStorage::isChunkCoordInBounds(const ChunkCoord &coord) {
    return coord.x >= -MAX_HORIZONTAL_CHUNK_COORD && coord.x < MAX_HORIZONTAL_CHUNK_COORD &&
           coord.z >= -MAX_HORIZONTAL_CHUNK_COORD && coord.z < MAX_HORIZONTAL_CHUNK_COORD &&
           coord.y >= MIN_CHUNK_Y && coord.y < MAX_CHUNK_Y;
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

float ChunksStorage::terrainNoise(int x, int z) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float amplitudeSum = 0.0f;
    const int cellSizes[] = {128, 64, 32, 16};
    for (int cellSize : cellSizes) {
        value += valueNoise(x, z, cellSize) * amplitude;
        amplitudeSum += amplitude;
        amplitude *= 0.5f;
    }
    return value / amplitudeSum;
}

int ChunksStorage::terrainHeight(int x, int z) {
    const float noise = terrainNoise(x, z);
    const int minHeight = WORLD_MIN_Y + 8;
    const int maxHeight = WORLD_MAX_Y - 12;
    const int height = static_cast<int>(
        std::round(lerpFloat(static_cast<float>(minHeight), static_cast<float>(maxHeight), noise))
    );
    return std::clamp(height, WORLD_MIN_Y + 1, WORLD_MAX_Y - 2);
}

bool ChunksStorage::isInsideHorizontalDistance(
    const ChunkCoord &coord, const ChunkCoord &center, int horizontalDistance
) {
    return std::abs(coord.x - center.x) <= horizontalDistance &&
           std::abs(coord.z - center.z) <= horizontalDistance;
}

bool ChunksStorage::setVoxel(int x, int y, int z, VoxelType type) {
    if (!isWorldCoordInBounds(x, y, z)) { return false; }
    Chunk *chunk = ensureChunk(worldToChunkCoord(x, y, z));
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

Chunk *ChunksStorage::ensureChunk(const ChunkCoord &coord) {
    if (!isChunkCoordInBounds(coord)) { return nullptr; }
    auto existing = m_chunks.find(coord);
    if (existing != m_chunks.end()) { return existing->second.get(); }

    auto chunk = std::make_unique<Chunk>();
    chunk->setCoord(coord);
    generateChunkData(*chunk);
    Chunk *result = chunk.get();
    m_chunks.emplace(coord, std::move(chunk));
    return result;
}

void ChunksStorage::ensureChunksAround(const ChunkCoord &center, int horizontalDistance) {
    const int distance = std::max(horizontalDistance, 0);
    const int minX = std::max(center.x - distance, -MAX_HORIZONTAL_CHUNK_COORD);
    const int maxX = std::min(center.x + distance, MAX_HORIZONTAL_CHUNK_COORD - 1);
    const int minZ = std::max(center.z - distance, -MAX_HORIZONTAL_CHUNK_COORD);
    const int maxZ = std::min(center.z + distance, MAX_HORIZONTAL_CHUNK_COORD - 1);

    for (int chunkY = MIN_CHUNK_Y; chunkY < MAX_CHUNK_Y; chunkY++) {
        for (int chunkX = minX; chunkX <= maxX; chunkX++) {
            for (int chunkZ = minZ; chunkZ <= maxZ; chunkZ++) {
                ensureChunk({chunkX, chunkY, chunkZ});
            }
        }
    }
}

void ChunksStorage::unloadChunksOutside(const ChunkCoord &center, int horizontalDistance) {
    std::vector<ChunkCoord> chunksToErase;
    const int distance = std::max(horizontalDistance, 0);

    for (auto &[coord, chunk] : m_chunks) {
        if (isInsideHorizontalDistance(coord, center, distance)) { continue; }
        if (chunk->isMeshBuildQueued()) { continue; }
        if (chunk->isModifiedByPlayer()) {
            chunk->clearMesh();
            continue;
        }
        chunksToErase.emplace_back(coord);
    }

    for (const ChunkCoord &coord : chunksToErase) {
        m_chunks.erase(coord);
    }
}

Chunk *ChunksStorage::getChunk(const ChunkCoord &coord) {
    if (!isChunkCoordInBounds(coord)) { return nullptr; }
    auto iterator = m_chunks.find(coord);
    if (iterator == m_chunks.end()) { return nullptr; }
    return iterator->second.get();
}

const Chunk *ChunksStorage::getChunk(const ChunkCoord &coord) const {
    if (!isChunkCoordInBounds(coord)) { return nullptr; }
    auto iterator = m_chunks.find(coord);
    if (iterator == m_chunks.end()) { return nullptr; }
    return iterator->second.get();
}

Chunk *ChunksStorage::getChunk(int x, int y, int z) {
    if (!isWorldCoordInBounds(x, y, z)) { return nullptr; }
    return getChunk(worldToChunkCoord(x, y, z));
}

const Chunk *ChunksStorage::getChunk(int x, int y, int z) const {
    if (!isWorldCoordInBounds(x, y, z)) { return nullptr; }
    return getChunk(worldToChunkCoord(x, y, z));
}

bool ChunksStorage::hasMeshNeighborhood(const ChunkCoord &coord) const {
    for (int offsetY = -1; offsetY <= 1; offsetY++) {
        for (int offsetX = -1; offsetX <= 1; offsetX++) {
            for (int offsetZ = -1; offsetZ <= 1; offsetZ++) {
                const ChunkCoord neighbor{coord.x + offsetX, coord.y + offsetY, coord.z + offsetZ};
                if (!isChunkCoordInBounds(neighbor)) { continue; }
                if (getChunk(neighbor) == nullptr) { return false; }
            }
        }
    }
    return true;
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
                const int worldX = originX + localX;
                const int worldY = originY + localY;
                const int worldZ = originZ + localZ;
                if (!isWorldCoordInBounds(worldX, worldY, worldZ)) {
                    snapshot.setPadded(
                        localX + ChunkMeshSnapshot::PADDING,
                        localY + ChunkMeshSnapshot::PADDING,
                        localZ + ChunkMeshSnapshot::PADDING,
                        Voxel(VoxelType::NOTHING)
                    );
                    continue;
                }

                const Chunk *sourceChunk = getChunk(worldToChunkCoord(worldX, worldY, worldZ));
                if (sourceChunk == nullptr) { return false; }
                const Voxel *voxel = sourceChunk->get(
                    worldToLocalX(worldX), worldToLocalY(worldY), worldToLocalZ(worldZ)
                );
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
