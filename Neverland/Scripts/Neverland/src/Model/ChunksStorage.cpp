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

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float smoothRange(float edge0, float edge1, float value) {
    if (edge0 == edge1) { return value >= edge1 ? 1.0f : 0.0f; }
    return smoothStep(clamp01((value - edge0) / (edge1 - edge0)));
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

float hashNoiseSalted(int x, int z, int salt) {
    return hashNoise(x + salt * 1299721, z - salt * 479001599);
}

float valueNoise(int x, int z, int cellSizeX, int cellSizeZ) {
    const int cellX = localFloorDiv(x, cellSizeX);
    const int cellZ = localFloorDiv(z, cellSizeZ);
    const float localX =
        static_cast<float>(localFloorMod(x, cellSizeX)) / static_cast<float>(cellSizeX);
    const float localZ =
        static_cast<float>(localFloorMod(z, cellSizeZ)) / static_cast<float>(cellSizeZ);
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

float valueNoise(int x, int z, int cellSize) {
    return valueNoise(x, z, cellSize, cellSize);
}

float ridged(float noise) {
    return 1.0f - std::abs(noise * 2.0f - 1.0f);
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
    const float continental = valueNoise(x - 8'719, z + 2'931, 520);
    const float mountainMask = smoothRange(0.48f, 0.78f, continental);

    const float plainRoll =
        valueNoise(x + 1'319, z - 6'127, 180) * 0.60f + valueNoise(x - 701, z + 907, 64) * 0.40f;

    const int diagonalA = x + z / 2;
    const int diagonalB = z - x / 3;
    const float ridgeA = std::pow(ridged(valueNoise(x + 14'003, z - 8'003, 420, 96)), 1.65f);
    const float ridgeB = std::pow(ridged(valueNoise(diagonalA - 3'101, diagonalB + 10'123, 320, 128)), 1.45f);
    const float ridgeField = clamp01(ridgeA * 0.72f + ridgeB * 0.36f);

    const float fine = valueNoise(x + 3'331, z + 1'337, 36);
    return clamp01(plainRoll * 0.28f + mountainMask * ridgeField * 0.66f + fine * 0.06f);
}

int ChunksStorage::terrainHeight(int x, int z) {
    const float continental = valueNoise(x - 8'719, z + 2'931, 520);
    const float mountainMask = smoothRange(0.48f, 0.78f, continental);
    const float plains = valueNoise(x + 1'319, z - 6'127, 180) * 0.60f +
                         valueNoise(x - 701, z + 907, 64) * 0.40f;
    const int diagonalA = x + z / 2;
    const int diagonalB = z - x / 3;
    const float ridgeA = std::pow(ridged(valueNoise(x + 14'003, z - 8'003, 420, 96)), 1.65f);
    const float ridgeB = std::pow(ridged(valueNoise(diagonalA - 3'101, diagonalB + 10'123, 320, 128)), 1.45f);
    const float ridgeField = clamp01(ridgeA * 0.72f + ridgeB * 0.36f);
    const float fine = valueNoise(x + 3'331, z + 1'337, 36);

    const float plainHeight = static_cast<float>(WORLD_MIN_Y + 13) + plains * 9.0f + fine * 2.0f;
    const float mountainHeight = ridgeField * 38.0f + std::pow(fine, 1.6f) * 5.0f;
    const int height = static_cast<int>(std::round(plainHeight + mountainMask * mountainHeight));
    return std::clamp(height, WORLD_MIN_Y + 1, WORLD_MAX_Y - 2);
}

bool ChunksStorage::isInsideHorizontalDistance(
    const ChunkCoord &coord, const ChunkCoord &center, int horizontalDistance
) {
    const long long dx = static_cast<long long>(coord.x) - center.x;
    const long long dz = static_cast<long long>(coord.z) - center.z;
    const long long radius = horizontalDistance;
    return dx * dx + dz * dz <= radius * radius;
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
                const ChunkCoord coord{chunkX, chunkY, chunkZ};
                if (!isInsideHorizontalDistance(coord, center, distance)) { continue; }
                ensureChunk(coord);
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

void ChunksStorage::unloadChunksWithoutViewsOutside(
    const ChunkCoord &center, int horizontalDistance
) {
    std::vector<ChunkCoord> chunksToErase;
    const int distance = std::max(horizontalDistance, 0);

    for (auto &[coord, chunk] : m_chunks) {
        if (isInsideHorizontalDistance(coord, center, distance)) { continue; }
        if (chunk->isMeshBuildQueued()) { continue; }
        if (chunk->hasView()) { continue; }
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

void ChunksStorage::destroyChunkViewsOutside(const ChunkCoord &center, int horizontalDistance) {
    const int distance = std::max(horizontalDistance, 0);
    for (auto &[coord, chunk] : m_chunks) {
        if (isInsideHorizontalDistance(coord, center, distance)) { continue; }
        if (chunk->isMeshBuildQueued()) { continue; }
        chunk->clearMesh();
    }
}

void ChunksStorage::destroyChunkViewsOutsideIf(
    const ChunkCoord &center,
    int horizontalDistance,
    const std::function<bool(const ChunkCoord &)> &shouldDestroy
) {
    const int distance = std::max(horizontalDistance, 0);
    for (auto &[coord, chunk] : m_chunks) {
        if (isInsideHorizontalDistance(coord, center, distance)) { continue; }
        if (chunk->isMeshBuildQueued()) { continue; }
        if (!chunk->hasView()) { continue; }
        if (!shouldDestroy(coord)) { continue; }
        chunk->clearMesh();
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

ChunksStorageStats ChunksStorage::collectStats() const {
    ChunksStorageStats stats;
    stats.loadedChunks = m_chunks.size();
    for (const auto &[coord, chunk] : m_chunks) {
        (void)coord;
        if (chunk->hasView()) { stats.chunksWithView++; }
        if (chunk->isMeshBuildQueued()) { stats.meshBuildQueuedChunks++; }
        if (chunk->needsRemesh()) { stats.dirtyChunks++; }
        if (chunk->isModifiedByPlayer()) { stats.modifiedChunks++; }
        stats.vertices += chunk->getVertexCount();
        stats.indices += chunk->getIndexCount();
    }
    for (const auto &[key, region] : m_regions) {
        (void)key;
        if (region->hasView()) { stats.regionsWithView++; }
        if (region->isMeshBuildQueued()) { stats.regionMeshBuildQueued++; }
        if (region->needsRemesh()) { stats.dirtyRegions++; }
        stats.regionVertices += region->getVertexCount();
        stats.regionIndices += region->getIndexCount();
    }
    return stats;
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

RegionMesh *ChunksStorage::ensureRegion(const RegionMeshKey &key) {
    auto existing = m_regions.find(key);
    if (existing != m_regions.end()) { return existing->second.get(); }

    auto region = std::make_unique<RegionMesh>();
    region->setKey(key);
    RegionMesh *result = region.get();
    m_regions.emplace(key, std::move(region));
    return result;
}

RegionMesh *ChunksStorage::getRegion(const RegionMeshKey &key) {
    auto iterator = m_regions.find(key);
    if (iterator == m_regions.end()) { return nullptr; }
    return iterator->second.get();
}

const RegionMesh *ChunksStorage::getRegion(const RegionMeshKey &key) const {
    auto iterator = m_regions.find(key);
    if (iterator == m_regions.end()) { return nullptr; }
    return iterator->second.get();
}

void ChunksStorage::unloadRegionsExcept(
    const std::unordered_set<RegionMeshKey, RegionMeshKeyHash> &activeRegions
) {
    std::vector<RegionMeshKey> regionsToErase;
    for (const auto &[key, region] : m_regions) {
        if (activeRegions.contains(key)) { continue; }
        if (region->isMeshBuildQueued()) { continue; }
        regionsToErase.emplace_back(key);
    }

    for (const RegionMeshKey &key : regionsToErase) {
        m_regions.erase(key);
    }
}

void ChunksStorage::unloadRegionsExceptIf(
    const std::unordered_set<RegionMeshKey, RegionMeshKeyHash> &activeRegions,
    const std::function<bool(const RegionMeshKey &)> &shouldUnload
) {
    std::vector<RegionMeshKey> regionsToErase;
    for (const auto &[key, region] : m_regions) {
        if (activeRegions.contains(key)) { continue; }
        if (region->isMeshBuildQueued()) { continue; }
        if (!shouldUnload(key)) { continue; }
        regionsToErase.emplace_back(key);
    }

    for (const RegionMeshKey &key : regionsToErase) {
        m_regions.erase(key);
    }
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
        if (voxel != nullptr && voxel->isSolid()) {
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
