#include "BaseScript.hpp"
#include "Model/VoxelMeshGenerator.hpp"
#include "Model/GameContext.hpp"

#include <Bamboo/Input.hpp>
#include <Bamboo/Math.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/Assets/MeshAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Components/MeshComponentAPI.hpp>

#include <cmath>

namespace {

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

bool hasNeighborhoodInsideDataDistance(
    const ChunkCoord &coord, const ChunkCoord &center, int dataDistance
) {
    for (int offsetY = -1; offsetY <= 1; offsetY++) {
        for (int offsetX = -1; offsetX <= 1; offsetX++) {
            for (int offsetZ = -1; offsetZ <= 1; offsetZ++) {
                ChunkCoord neighbor{coord.x + offsetX, coord.y + offsetY, coord.z + offsetZ};
                if (!ChunksStorage::isChunkCoordInBounds(neighbor)) {
                    // Finite world border is a real boundary and can be treated as air.
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

    // Build all chunk meshes in the background from immutable snapshots. The GPU
    // upload still runs on the main thread in update().
    PandaAsync::Scheduler &scheduler = *GameContext::s_scheduler;
    MaterialHandle chunkMaterial = material;

    const Vec3 position = TransformComponentAPI::getPosition(getEntity());
    const ChunkCoord center = ChunksStorage::worldToChunkCoord(
        toWorldCoord(position.x), toWorldCoord(position.y), toWorldCoord(position.z)
    );
    const int effectiveRenderDistance = nonNegative(renderDistance);
    const int dataDistance = effectiveRenderDistance + 1;

    for (int indexX = 0; indexX < ChunksStorage::SIZE_X; indexX++) {
        for (int indexY = 0; indexY < ChunksStorage::SIZE_Y; indexY++) {
            for (int indexZ = 0; indexZ < ChunksStorage::SIZE_Z; indexZ++) {
                ChunkCoord coord{indexX, indexY, indexZ};
                if (!isInsideHorizontalDistance(coord, center, effectiveRenderDistance)) {
                    continue;
                }
                if (!hasNeighborhoodInsideDataDistance(coord, center, dataDistance)) { continue; }

                ChunkMeshSnapshot snapshot;
                if (!GameContext::s_chunkStorage->makeMeshSnapshot(coord, snapshot)) { continue; }

                GameContext::s_pendingChunkJobs++;
                scheduler.submit<ChunkMeshBuildResult>(
                    PandaAsync::JobDesc{"chunk-mesh"},
                    [snapshot](PandaAsync::Context &) -> ChunkMeshBuildResult {
                        return VoxelMeshGenerator::makeOneChunkMesh(snapshot, true);
                    },
                    [chunkMaterial](ChunkMeshBuildResult &&result) {
                        Chunk *chunk = GameContext::s_chunkStorage != nullptr
                                           ? GameContext::s_chunkStorage->getChunk(result.coord)
                                           : nullptr;
                        if (chunk != nullptr && chunk->getVersion() == result.version) {
                            MeshAPI::update(chunk->getMesh(), result.meshData);
                            MeshComponentAPI::setMaterial(chunk->getEntity(), chunkMaterial);
                            chunk->clearNeedsRemesh();
                        }
                        GameContext::s_pendingChunkJobs--;
                    }
                );
            }
        }
    }
}

void BaseScript::update(float dt) {
    // Apply a bounded number of finished chunk meshes per frame to avoid GPU spikes.
    if (GameContext::s_scheduler != nullptr) {
        GameContext::s_scheduler->processCompletions(PandaAsync::CompletionBudget{4, 2.0});
    }
    if (Input::isKeyPressed(Key::L)) { LOG_INFO("Hello Panda! var: %d", var); }
}

void BaseScript::shutdown() {
    GameContext::deinit();
}
