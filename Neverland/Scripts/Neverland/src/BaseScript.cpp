#include "BaseScript.hpp"
#include "Model/VoxelMeshGenerator.hpp"
#include "Model/GameContext.hpp"

#include <Bamboo/Input.hpp>
#include <Bamboo/Math.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/WorldAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Components/MeshComponentAPI.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr int MAX_RENDER_DISTANCE = 8;
constexpr int CHUNK_LOAD_BUDGET_PER_FRAME = 12;
constexpr int MESH_SCHEDULE_BUDGET_PER_FRAME = 4;
constexpr int MESH_SCHEDULE_ATTEMPT_BUDGET_PER_FRAME = 16;
constexpr int MAX_PENDING_MESH_JOBS = 8;
constexpr int UNLOAD_CLEANUP_INTERVAL_FRAMES = 30;

int nonNegative(int value) {
    return value < 0 ? 0 : value;
}

int absInt(int value) {
    return value < 0 ? -value : value;
}

bool isInsideHorizontalDistance(const ChunkCoord &coord, const ChunkCoord &center, int distance) {
    return absInt(coord.x - center.x) <= distance && absInt(coord.z - center.z) <= distance;
}

int toWorldCoord(float value) {
    return static_cast<int>(std::floor(value));
}

int horizontalDistanceSquared(const ChunkCoord &coord, const ChunkCoord &center) {
    const int dx = coord.x - center.x;
    const int dz = coord.z - center.z;
    return dx * dx + dz * dz;
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
    return ChunksStorage::worldToChunkCoord(
        toWorldCoord(position.x), toWorldCoord(position.y), toWorldCoord(position.z)
    );
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

void BaseScript::rebuildStreamingQueues(const ChunkCoord &center, int effectiveRenderDistance) {
    const int dataDistance = effectiveRenderDistance + 1;
    m_chunkLoadQueue.clear();
    m_meshBuildQueue.clear();
    m_streamingCenter = center;
    m_streamingRenderDistance = effectiveRenderDistance;
    m_hasStreamingCenter = true;

    std::vector<ChunkCoord> loadCoords;
    std::vector<ChunkCoord> meshCoords;

    for (int indexY = ChunksStorage::MIN_CHUNK_Y; indexY < ChunksStorage::MAX_CHUNK_Y; indexY++) {
        for (int offsetX = -dataDistance; offsetX <= dataDistance; offsetX++) {
            for (int offsetZ = -dataDistance; offsetZ <= dataDistance; offsetZ++) {
                ChunkCoord coord{center.x + offsetX, indexY, center.z + offsetZ};
                if (!ChunksStorage::isChunkCoordInBounds(coord)) { continue; }
                loadCoords.emplace_back(coord);
                if (isInsideHorizontalDistance(coord, center, effectiveRenderDistance) &&
                    hasNeighborhoodInsideDataDistance(coord, center, dataDistance)) {
                    meshCoords.emplace_back(coord);
                }
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

    for (const ChunkCoord &coord : loadCoords) {
        m_chunkLoadQueue.emplace_back(coord);
    }
    for (const ChunkCoord &coord : meshCoords) {
        m_meshBuildQueue.emplace_back(coord);
    }

    GameContext::s_chunkStorage->unloadChunksOutside(center, dataDistance);
    m_framesUntilUnloadCleanup = UNLOAD_CLEANUP_INTERVAL_FRAMES;
}

void BaseScript::updateStreaming() {
    if (GameContext::s_chunkStorage == nullptr || GameContext::s_scheduler == nullptr) { return; }

    const ChunkCoord center = getPlayerChunkCenter();
    const int effectiveRenderDistance = getEffectiveRenderDistance();
    if (!m_hasStreamingCenter || !(center == m_streamingCenter) ||
        effectiveRenderDistance != m_streamingRenderDistance) {
        rebuildStreamingQueues(center, effectiveRenderDistance);
    }

    for (int i = 0; i < CHUNK_LOAD_BUDGET_PER_FRAME; i++) {
        if (!loadNextChunk()) { break; }
    }
    for (int i = 0; i < MESH_SCHEDULE_BUDGET_PER_FRAME &&
                    GameContext::s_pendingChunkJobs < MAX_PENDING_MESH_JOBS;
         i++) {
        if (!scheduleNextChunkMesh(material)) { break; }
    }

    m_framesUntilUnloadCleanup--;
    if (m_framesUntilUnloadCleanup <= 0) {
        GameContext::s_chunkStorage->unloadChunksOutside(center, effectiveRenderDistance + 1);
        m_framesUntilUnloadCleanup = UNLOAD_CLEANUP_INTERVAL_FRAMES;
    }
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

void BaseScript::update(float dt) {
    // Apply a bounded number of finished chunk meshes per frame to avoid GPU spikes.
    if (GameContext::s_scheduler != nullptr) {
        GameContext::s_scheduler->processCompletions(PandaAsync::CompletionBudget{4, 2.0});
    }
    updateStreaming();
    if (Input::isKeyPressed(Key::L)) { LOG_INFO("Hello Panda! var: %d", var); }
}

void BaseScript::shutdown() {
    GameContext::deinit();
}
