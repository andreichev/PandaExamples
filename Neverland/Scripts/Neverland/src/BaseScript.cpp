#include "BaseScript.hpp"
#include "Model/VoxelMeshGenerator.hpp"
#include "Model/RegionMeshGenerator.hpp"
#include "Model/GameContext.hpp"

#include <Bamboo/Input.hpp>
#include <Bamboo/Math.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/WorldAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Components/MeshComponentAPI.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace {

constexpr int MAX_RENDER_DISTANCE = 200;
constexpr int MAX_NEAR_CHUNK_DISTANCE = 12;
constexpr int REGION_LOD_CENTER_SNAP_CHUNKS = 4;
constexpr int DATA_RADIUS_PADDING = 2;
constexpr int CHUNK_LOAD_BUDGET_PER_FRAME = 12;
constexpr int MESH_SCHEDULE_BUDGET_PER_FRAME = 4;
constexpr int MESH_SCHEDULE_ATTEMPT_BUDGET_PER_FRAME = 16;
constexpr int MAX_PENDING_MESH_JOBS = 8;
constexpr int REGION_MESH_SCHEDULE_BUDGET_PER_FRAME = 2;
constexpr int REGION_MESH_SCHEDULE_ATTEMPT_BUDGET_PER_FRAME = 12;
constexpr int MAX_PENDING_REGION_JOBS = 4;
constexpr float STATS_LOG_INTERVAL_SECONDS = 1.0f;

struct RegionLodBand {
    int lod = 0;
    int minDistance = 0;
    int maxDistance = 0;
    int regionSizeChunks = 1;
    int sampleStepBlocks = 1;
};

int nonNegative(int value) {
    return value < 0 ? 0 : value;
}

bool isInsideHorizontalDistance(const ChunkCoord &coord, const ChunkCoord &center, int distance) {
    const long long dx = static_cast<long long>(coord.x) - center.x;
    const long long dz = static_cast<long long>(coord.z) - center.z;
    const long long radius = distance;
    return dx * dx + dz * dz <= radius * radius;
}

int toWorldCoord(float value) {
    return static_cast<int>(std::floor(value));
}

int floorDiv(int value, int divisor) {
    int quotient = value / divisor;
    int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) { quotient--; }
    return quotient;
}

int horizontalDistanceSquared(const ChunkCoord &coord, const ChunkCoord &center) {
    const int dx = coord.x - center.x;
    const int dz = coord.z - center.z;
    return dx * dx + dz * dz;
}

int regionDistanceSquared(const RegionMeshBuildRequest &request, const ChunkCoord &center) {
    const int regionCenterX = request.key.x * request.regionSizeChunks + request.regionSizeChunks / 2;
    const int regionCenterZ = request.key.z * request.regionSizeChunks + request.regionSizeChunks / 2;
    const int dx = regionCenterX - center.x;
    const int dz = regionCenterZ - center.z;
    return dx * dx + dz * dz;
}

uint64_t hashRegionRequest(const RegionMeshBuildRequest &request) {
    uint64_t seed = 1469598103934665603ULL;
    auto combine = [&seed](int value) {
        seed ^= static_cast<uint64_t>(static_cast<uint32_t>(value));
        seed *= 1099511628211ULL;
    };
    combine(request.key.x);
    combine(request.key.z);
    combine(request.key.lod);
    combine(request.regionSizeChunks);
    combine(request.sampleStepBlocks);
    combine(request.minChunkDistance);
    combine(request.maxChunkDistance);
    combine(request.centerChunkX);
    combine(request.centerChunkZ);
    return seed;
}

RegionMeshBuildRequest makeRegionRequest(
    const RegionLodBand &band, const RegionMeshKey &key, const ChunkCoord &center
) {
    RegionMeshBuildRequest request;
    request.key = key;
    request.regionSizeChunks = band.regionSizeChunks;
    request.sampleStepBlocks = band.sampleStepBlocks;
    request.minChunkDistance = band.minDistance;
    request.maxChunkDistance = band.maxDistance;
    request.centerChunkX = center.x;
    request.centerChunkZ = center.z;
    request.requestId = hashRegionRequest(request);
    return request;
}

int regionSizeChunksForLod(int lod) {
    switch (lod) {
        case 1:
            return 4;
        case 2:
            return 8;
        case 3:
            return 16;
        default:
            return 0;
    }
}

std::vector<RegionLodBand> buildRegionLodBands(int nearDistance, int renderDistance) {
    std::vector<RegionLodBand> bands;
    int start = nearDistance;

    const int mediumMax = std::min(renderDistance, std::max(nearDistance, 24));
    if (mediumMax > start) {
        bands.emplace_back(RegionLodBand{1, start, mediumMax, 4, 4});
        start = mediumMax;
    }

    const int farMax = std::min(renderDistance, 64);
    if (farMax > start) {
        bands.emplace_back(RegionLodBand{2, start, farMax, 8, 8});
        start = farMax;
    }

    if (renderDistance > start) {
        bands.emplace_back(RegionLodBand{3, start, renderDistance, 16, 16});
    }

    return bands;
}

bool hasNeighborhoodInsideDataDistance(
    const ChunkCoord &coord, const ChunkCoord &center, int dataDistance
) {
    for (int offsetY = -1; offsetY <= 1; offsetY++) {
        for (int offsetX = -1; offsetX <= 1; offsetX++) {
            for (int offsetZ = -1; offsetZ <= 1; offsetZ++) {
                ChunkCoord neighbor{coord.x + offsetX, coord.y + offsetY, coord.z + offsetZ};
                if (!ChunksStorage::isChunkCoordInBounds(neighbor)) {
                    // Vertical and numeric world borders are real boundaries and can be treated as
                    // air.
                    continue;
                }
                if (!isInsideHorizontalDistance(neighbor, center, dataDistance)) { return false; }
            }
        }
    }
    return true;
}

} // namespace

void BaseScript::start() {
    GameContext::init();
    GameContext::s_chunkMaterial = material;
    m_streamingTarget = WorldAPI::findByTag("Camera");
    if (!m_streamingTarget.isValid()) {
        LOG_WARN(
            "Neverland streaming target 'Camera' was not found. Falling back to BaseScript entity."
        );
    }
    updateStreaming();
}

ChunkCoord BaseScript::getPlayerChunkCenter() {
    if (!m_streamingTarget.isValid()) { m_streamingTarget = WorldAPI::findByTag("Camera"); }
    const EntityHandle target = m_streamingTarget.isValid() ? m_streamingTarget : getEntity();
    const Vec3 position = TransformComponentAPI::getPosition(target);
    ChunkCoord center = ChunksStorage::worldToChunkCoord(
        toWorldCoord(position.x), toWorldCoord(position.y), toWorldCoord(position.z)
    );
    center.y = ChunksStorage::MIN_CHUNK_Y;
    return center;
}

ChunkCoord BaseScript::getRegionLodCenter(const ChunkCoord &center) {
    if (!m_hasRegionLodCenter) {
        m_regionLodCenter = center;
        m_hasRegionLodCenter = true;
        return m_regionLodCenter;
    }

    const int snapChunks = std::max(1, REGION_LOD_CENTER_SNAP_CHUNKS);
    if (std::abs(center.x - m_regionLodCenter.x) >= snapChunks ||
        std::abs(center.z - m_regionLodCenter.z) >= snapChunks) {
        m_regionLodCenter = center;
    }
    m_regionLodCenter.y = center.y;
    return m_regionLodCenter;
}

int BaseScript::getEffectiveRenderDistance() {
    const int requestedDistance = nonNegative(renderDistance);
    const int effectiveDistance = std::min(requestedDistance, MAX_RENDER_DISTANCE);
    if (requestedDistance != effectiveDistance && !m_warnedRenderDistanceClamp) {
        LOG_WARN(
            "Neverland renderDistance=%d is too high for current streaming prototype. Clamped to "
            "%d.",
            requestedDistance,
            MAX_RENDER_DISTANCE
        );
        m_warnedRenderDistanceClamp = true;
    }
    if (requestedDistance == effectiveDistance) { m_warnedRenderDistanceClamp = false; }
    return effectiveDistance;
}

int BaseScript::getEffectiveNearDistance(int effectiveRenderDistance) {
    const int requestedDistance = nonNegative(nearChunkDistance);
    return std::min({requestedDistance, effectiveRenderDistance, MAX_NEAR_CHUNK_DISTANCE});
}

int BaseScript::getExactBuildDistance(int effectiveRenderDistance, int effectiveNearDistance) {
    return std::min(effectiveRenderDistance, effectiveNearDistance + nonNegative(lodHandoffChunks));
}

void BaseScript::rebuildStreamingQueues(
    const ChunkCoord &center,
    const ChunkCoord &regionCenter,
    int effectiveRenderDistance,
    int effectiveNearDistance
) {
    const int exactBuildDistance =
        getExactBuildDistance(effectiveRenderDistance, effectiveNearDistance);
    const int dataDistance = exactBuildDistance + DATA_RADIUS_PADDING;
    m_chunkLoadQueue.clear();
    m_meshBuildQueue.clear();
    m_regionMeshBuildQueue.clear();
    m_activeRegionKeys.clear();
    m_streamingCenter = center;
    m_streamingRenderDistance = effectiveRenderDistance;
    m_streamingNearDistance = effectiveNearDistance;
    m_streamingExactBuildDistance = exactBuildDistance;
    m_hasStreamingCenter = true;

    std::vector<ChunkCoord> loadCoords;
    std::vector<ChunkCoord> meshCoords;
    std::vector<RegionMeshBuildRequest> regionRequests;
    std::unordered_set<RegionMeshKey, RegionMeshKeyHash> activeRegions;

    for (int indexY = ChunksStorage::MIN_CHUNK_Y; indexY < ChunksStorage::MAX_CHUNK_Y; indexY++) {
        for (int offsetX = -dataDistance; offsetX <= dataDistance; offsetX++) {
            for (int offsetZ = -dataDistance; offsetZ <= dataDistance; offsetZ++) {
                ChunkCoord coord{center.x + offsetX, indexY, center.z + offsetZ};
                if (!ChunksStorage::isChunkCoordInBounds(coord)) { continue; }
                if (!isInsideHorizontalDistance(coord, center, dataDistance)) { continue; }
                loadCoords.emplace_back(coord);
                if (isInsideHorizontalDistance(coord, center, exactBuildDistance) &&
                    hasNeighborhoodInsideDataDistance(coord, center, dataDistance)) {
                    meshCoords.emplace_back(coord);
                }
            }
        }
    }

    for (const RegionLodBand &band : buildRegionLodBands(effectiveNearDistance, effectiveRenderDistance)) {
        for (int offsetX = -band.maxDistance; offsetX <= band.maxDistance; offsetX++) {
            for (int offsetZ = -band.maxDistance; offsetZ <= band.maxDistance; offsetZ++) {
                ChunkCoord coord{regionCenter.x + offsetX, regionCenter.y, regionCenter.z + offsetZ};
                if (!ChunksStorage::isChunkCoordInBounds(coord)) { continue; }
                if (!isInsideHorizontalDistance(coord, regionCenter, band.maxDistance)) { continue; }
                if (isInsideHorizontalDistance(coord, regionCenter, band.minDistance)) { continue; }

                RegionMeshKey key{
                    floorDiv(coord.x, band.regionSizeChunks),
                    floorDiv(coord.z, band.regionSizeChunks),
                    band.lod
                };
                if (!activeRegions.emplace(key).second) { continue; }

                regionRequests.emplace_back(makeRegionRequest(band, key, regionCenter));
            }
        }
    }

    auto sortByDistance = [center](const ChunkCoord &left, const ChunkCoord &right) {
        const int leftDistance = horizontalDistanceSquared(left, center);
        const int rightDistance = horizontalDistanceSquared(right, center);
        if (leftDistance != rightDistance) { return leftDistance < rightDistance; }
        return left.y < right.y;
    };
    std::sort(loadCoords.begin(), loadCoords.end(), sortByDistance);
    std::sort(meshCoords.begin(), meshCoords.end(), sortByDistance);
    std::sort(
        regionRequests.begin(),
        regionRequests.end(),
        [regionCenter](const RegionMeshBuildRequest &left, const RegionMeshBuildRequest &right) {
            const int leftDistance = regionDistanceSquared(left, regionCenter);
            const int rightDistance = regionDistanceSquared(right, regionCenter);
            if (leftDistance != rightDistance) { return leftDistance < rightDistance; }
            return left.key.lod < right.key.lod;
        }
    );

    for (const ChunkCoord &coord : loadCoords) {
        m_chunkLoadQueue.emplace_back(coord);
    }
    for (const ChunkCoord &coord : meshCoords) {
        m_meshBuildQueue.emplace_back(coord);
    }
    for (const RegionMeshBuildRequest &request : regionRequests) {
        RegionMesh *region = GameContext::s_chunkStorage->ensureRegion(request.key);
        region->setDesiredRequest(request.requestId);
        if (region->needsRemesh()) { m_regionMeshBuildQueue.emplace_back(request); }
    }

    m_activeRegionKeys = activeRegions;
    cleanupStreamingViews(
        center, regionCenter, effectiveRenderDistance, effectiveNearDistance, exactBuildDistance
    );
}

void BaseScript::cleanupStreamingViews(
    const ChunkCoord &center,
    const ChunkCoord &regionCenter,
    int effectiveRenderDistance,
    int effectiveNearDistance,
    int exactBuildDistance
) {
    if (GameContext::s_chunkStorage == nullptr) { return; }

    GameContext::s_chunkStorage->destroyChunkViewsOutsideIf(
        center,
        exactBuildDistance,
        [this, center, regionCenter, effectiveRenderDistance, effectiveNearDistance](
            const ChunkCoord &coord
        ) {
            if (!isInsideHorizontalDistance(coord, center, effectiveRenderDistance)) { return true; }
            return isChunkCoveredByReadyRegionLod(
                coord, regionCenter, effectiveRenderDistance, effectiveNearDistance
            );
        }
    );
    GameContext::s_chunkStorage->unloadChunksWithoutViewsOutside(
        center, exactBuildDistance + DATA_RADIUS_PADDING
    );
    GameContext::s_chunkStorage->unloadRegionsExceptIf(
        m_activeRegionKeys,
        [this, center, exactBuildDistance](const RegionMeshKey &key) {
            return !shouldKeepInactiveRegionForExactFallback(key, center, exactBuildDistance);
        }
    );
}

bool BaseScript::isChunkCoveredByReadyRegionLod(
    const ChunkCoord &coord,
    const ChunkCoord &center,
    int effectiveRenderDistance,
    int effectiveNearDistance
) const {
    if (GameContext::s_chunkStorage == nullptr) { return false; }

    for (const RegionLodBand &band : buildRegionLodBands(effectiveNearDistance, effectiveRenderDistance)) {
        if (!isInsideHorizontalDistance(coord, center, band.maxDistance)) { continue; }
        if (isInsideHorizontalDistance(coord, center, band.minDistance)) { continue; }

        RegionMeshKey key{
            floorDiv(coord.x, band.regionSizeChunks),
            floorDiv(coord.z, band.regionSizeChunks),
            band.lod
        };
        const RegionMeshBuildRequest request = makeRegionRequest(band, key, center);
        const RegionMesh *region = GameContext::s_chunkStorage->getRegion(key);
        return region != nullptr && region->hasView() && !region->needsRemesh() &&
               !region->isMeshBuildQueued() && region->getRequestId() == request.requestId;
    }

    return false;
}

bool BaseScript::shouldKeepInactiveRegionForExactFallback(
    const RegionMeshKey &key, const ChunkCoord &center, int exactBuildDistance
) const {
    if (GameContext::s_chunkStorage == nullptr) { return false; }
    const RegionMesh *region = GameContext::s_chunkStorage->getRegion(key);
    if (region == nullptr || !region->hasView()) { return false; }

    const int regionSizeChunks = regionSizeChunksForLod(key.lod);
    if (regionSizeChunks <= 0) { return false; }

    const int minX = key.x * regionSizeChunks;
    const int minZ = key.z * regionSizeChunks;
    for (int x = minX; x < minX + regionSizeChunks; x++) {
        for (int z = minZ; z < minZ + regionSizeChunks; z++) {
            const ChunkCoord coord{x, ChunksStorage::MIN_CHUNK_Y, z};
            if (!ChunksStorage::isChunkCoordInBounds(coord)) { continue; }
            if (!isInsideHorizontalDistance(coord, center, exactBuildDistance)) { continue; }
            if (!hasExactChunkView(coord)) { return true; }
        }
    }

    return false;
}

bool BaseScript::hasExactChunkView(const ChunkCoord &coord) const {
    if (GameContext::s_chunkStorage == nullptr) { return false; }
    const Chunk *chunk = GameContext::s_chunkStorage->getChunk(coord);
    return chunk != nullptr && chunk->hasView();
}

void BaseScript::updateStreaming() {
    if (GameContext::s_chunkStorage == nullptr || GameContext::s_scheduler == nullptr) { return; }

    const ChunkCoord center = getPlayerChunkCenter();
    const ChunkCoord regionCenter = getRegionLodCenter(center);
    const int effectiveRenderDistance = getEffectiveRenderDistance();
    const int effectiveNearDistance = getEffectiveNearDistance(effectiveRenderDistance);
    const int exactBuildDistance =
        getExactBuildDistance(effectiveRenderDistance, effectiveNearDistance);
    if (!m_hasStreamingCenter || !(center == m_streamingCenter) ||
        effectiveRenderDistance != m_streamingRenderDistance ||
        effectiveNearDistance != m_streamingNearDistance ||
        exactBuildDistance != m_streamingExactBuildDistance) {
        rebuildStreamingQueues(center, regionCenter, effectiveRenderDistance, effectiveNearDistance);
    }

    for (int i = 0; i < CHUNK_LOAD_BUDGET_PER_FRAME; i++) {
        if (!loadNextChunk()) { break; }
    }
    for (int i = 0; i < MESH_SCHEDULE_BUDGET_PER_FRAME &&
                    GameContext::s_pendingChunkJobs < MAX_PENDING_MESH_JOBS;
         i++) {
        if (!scheduleNextChunkMesh(material)) { break; }
    }
    for (int i = 0; i < REGION_MESH_SCHEDULE_BUDGET_PER_FRAME &&
                    GameContext::s_pendingRegionJobs < MAX_PENDING_REGION_JOBS;
         i++) {
        if (!scheduleNextRegionMesh(material)) { break; }
    }

    cleanupStreamingViews(
        center, regionCenter, effectiveRenderDistance, effectiveNearDistance, exactBuildDistance
    );
}

bool BaseScript::loadNextChunk() {
    while (!m_chunkLoadQueue.empty()) {
        const ChunkCoord coord = m_chunkLoadQueue.front();
        m_chunkLoadQueue.pop_front();
        if (GameContext::s_chunkStorage->getChunk(coord) != nullptr) { continue; }
        GameContext::s_chunkStorage->ensureChunk(coord);
        return true;
    }
    return false;
}

bool BaseScript::scheduleNextChunkMesh(MaterialHandle chunkMaterial) {
    int attempts = 0;
    while (!m_meshBuildQueue.empty() && attempts < MESH_SCHEDULE_ATTEMPT_BUDGET_PER_FRAME) {
        attempts++;
        const ChunkCoord coord = m_meshBuildQueue.front();
        m_meshBuildQueue.pop_front();

        Chunk *chunk = GameContext::s_chunkStorage->getChunk(coord);
        if (chunk == nullptr) {
            m_meshBuildQueue.emplace_back(coord);
            continue;
        }
        if (!chunk->needsRemesh() || chunk->isMeshBuildQueued()) { continue; }
        if (!GameContext::s_chunkStorage->hasMeshNeighborhood(coord)) {
            m_meshBuildQueue.emplace_back(coord);
            continue;
        }
        if (scheduleChunkMesh(coord, chunkMaterial)) { return true; }
        m_meshBuildQueue.emplace_back(coord);
    }
    return false;
}

bool BaseScript::scheduleChunkMesh(const ChunkCoord &coord, MaterialHandle chunkMaterial) {
    Chunk *chunk = GameContext::s_chunkStorage->getChunk(coord);
    if (chunk == nullptr || !chunk->needsRemesh() || chunk->isMeshBuildQueued()) { return false; }

    ChunkMeshSnapshot snapshot;
    if (!GameContext::s_chunkStorage->makeMeshSnapshot(coord, snapshot)) { return false; }

    chunk->setMeshBuildQueued(true);
    GameContext::s_pendingChunkJobs++;
    GameContext::s_scheduler->submit<ChunkMeshBuildResult>(
        PandaAsync::JobDesc{"chunk-mesh"},
        [snapshot](PandaAsync::Context &) -> ChunkMeshBuildResult {
            return VoxelMeshGenerator::makeOneChunkMesh(snapshot, true);
        },
        [chunkMaterial](ChunkMeshBuildResult &&result) {
            Chunk *chunk = GameContext::s_chunkStorage != nullptr
                               ? GameContext::s_chunkStorage->getChunk(result.coord)
                               : nullptr;
            if (chunk != nullptr) {
                chunk->setMeshBuildQueued(false);
                if (chunk->getVersion() == result.version) {
                    chunk->updateMesh(result.meshData);
                    if (chunk->hasView()) {
                        MeshComponentAPI::setMaterial(
                            chunk->getEntity(),
                            chunkMaterial.id != BAMBOO_INVALID_HANDLE ? chunkMaterial
                                                                      : GameContext::s_chunkMaterial
                        );
                    }
                    chunk->clearNeedsRemesh();
                }
            }
            GameContext::s_pendingChunkJobs--;
        }
    );
    return true;
}

bool BaseScript::scheduleNextRegionMesh(MaterialHandle chunkMaterial) {
    int attempts = 0;
    while (!m_regionMeshBuildQueue.empty() && attempts < REGION_MESH_SCHEDULE_ATTEMPT_BUDGET_PER_FRAME) {
        attempts++;
        const RegionMeshBuildRequest request = m_regionMeshBuildQueue.front();
        m_regionMeshBuildQueue.pop_front();

        RegionMesh *region = GameContext::s_chunkStorage->getRegion(request.key);
        if (region == nullptr) { continue; }
        if (!region->needsRemesh()) { continue; }
        if (region->getRequestId() != request.requestId) { continue; }
        if (region->isMeshBuildQueued()) {
            m_regionMeshBuildQueue.emplace_back(request);
            continue;
        }
        if (scheduleRegionMesh(request, chunkMaterial)) { return true; }
        m_regionMeshBuildQueue.emplace_back(request);
    }
    return false;
}

bool BaseScript::scheduleRegionMesh(const RegionMeshBuildRequest &request, MaterialHandle chunkMaterial) {
    RegionMesh *region = GameContext::s_chunkStorage->getRegion(request.key);
    if (region == nullptr || !region->needsRemesh() || region->isMeshBuildQueued()) { return false; }
    if (region->getRequestId() != request.requestId) { return false; }

    region->setMeshBuildQueued(true);
    GameContext::s_pendingRegionJobs++;
    GameContext::s_scheduler->submit<RegionMeshBuildResult>(
        PandaAsync::JobDesc{"region-lod-mesh"},
        [request](PandaAsync::Context &) -> RegionMeshBuildResult {
            return RegionMeshGenerator::makeRegionMesh(request);
        },
        [chunkMaterial](RegionMeshBuildResult &&result) {
            RegionMesh *region = GameContext::s_chunkStorage != nullptr
                                     ? GameContext::s_chunkStorage->getRegion(result.key)
                                     : nullptr;
            if (region != nullptr) {
                region->setMeshBuildQueued(false);
                if (region->getRequestId() == result.requestId) {
                    region->updateMesh(result.meshData);
                    if (region->hasView()) {
                        MeshComponentAPI::setMaterial(
                            region->getEntity(),
                            chunkMaterial.id != BAMBOO_INVALID_HANDLE ? chunkMaterial
                                                                      : GameContext::s_chunkMaterial
                        );
                    }
                    region->clearNeedsRemesh();
                }
            }
            GameContext::s_pendingRegionJobs--;
        }
    );
    return true;
}

void BaseScript::logStreamingStats(float deltaTime) {
    if (GameContext::s_chunkStorage == nullptr) { return; }
    m_statsLogTimer += deltaTime;
    if (m_statsLogTimer < STATS_LOG_INTERVAL_SECONDS) { return; }
    m_statsLogTimer = 0.0f;

    const ChunksStorageStats stats = GameContext::s_chunkStorage->collectStats();
    const int exactBuildDistance =
        getExactBuildDistance(m_streamingRenderDistance, m_streamingNearDistance);
    LOG_INFO(
        "Neverland chunks: loaded=%zu chunkView=%zu regionView=%zu dirty=%zu regionDirty=%zu "
        "queued=%zu regionQueued=%zu modified=%zu pendingJobs=%d pendingRegionJobs=%d "
        "loadQueue=%zu meshQueue=%zu regionQueue=%zu vertices=%zu indices=%zu "
        "regionVertices=%zu regionIndices=%zu center=(%d,%d,%d) lodCenter=(%d,%d,%d) "
        "rd=%d near=%d exact=%d",
        stats.loadedChunks,
        stats.chunksWithView,
        stats.regionsWithView,
        stats.dirtyChunks,
        stats.dirtyRegions,
        stats.meshBuildQueuedChunks,
        stats.regionMeshBuildQueued,
        stats.modifiedChunks,
        GameContext::s_pendingChunkJobs,
        GameContext::s_pendingRegionJobs,
        m_chunkLoadQueue.size(),
        m_meshBuildQueue.size(),
        m_regionMeshBuildQueue.size(),
        stats.vertices,
        stats.indices,
        stats.regionVertices,
        stats.regionIndices,
        m_streamingCenter.x,
        m_streamingCenter.y,
        m_streamingCenter.z,
        m_regionLodCenter.x,
        m_regionLodCenter.y,
        m_regionLodCenter.z,
        m_streamingRenderDistance,
        m_streamingNearDistance,
        exactBuildDistance
    );
}

void BaseScript::update(float dt) {
    // Apply a bounded number of finished chunk meshes per frame to avoid GPU spikes.
    if (GameContext::s_scheduler != nullptr) {
        GameContext::s_scheduler->processCompletions(PandaAsync::CompletionBudget{6, 2.5});
    }
    updateStreaming();
    logStreamingStats(dt);
    if (Input::isKeyPressed(Key::L)) { LOG_INFO("Hello Panda! var: %d", var); }
}

void BaseScript::shutdown() {
    GameContext::deinit();
}
