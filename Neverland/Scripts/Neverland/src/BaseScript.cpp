#include "BaseScript.hpp"
#include "Model/VoxelMeshGenerator.hpp"
#include "Model/GameContext.hpp"

#include <Bamboo/Input.hpp>
#include <Bamboo/Math.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/Assets/MeshAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Components/MeshComponentAPI.hpp>

void BaseScript::start() {
    GameContext::init();

    // Build all chunk meshes in the background: makeOneChunkMesh (pure compute over
    // static voxel data) runs on a worker, the GPU upload runs on the main thread in
    // update(). This removes the start-of-world frame spike.
    PandaAsync::Scheduler& scheduler = *GameContext::s_scheduler;
    MaterialHandle chunkMaterial = material;
    for (int indexX = 0; indexX < ChunksStorage::SIZE_X; indexX++) {
        for (int indexY = 0; indexY < ChunksStorage::SIZE_Y; indexY++) {
            for (int indexZ = 0; indexZ < ChunksStorage::SIZE_Z; indexZ++) {
                Chunk& chunk =
                    GameContext::s_chunkStorage
                        ->chunks
                            [indexY * ChunksStorage::SIZE_X * ChunksStorage::SIZE_Z +
                             indexX * ChunksStorage::SIZE_X + indexZ];
                MeshHandle dynamicMesh = chunk.getMesh();
                EntityHandle meshEntity = chunk.getEntity();
                GameContext::s_pendingChunkJobs++;
                scheduler.submit<MeshData>(
                    PandaAsync::JobDesc{"chunk-mesh"},
                    [indexX, indexY, indexZ](PandaAsync::Context&) -> MeshData {
                        return VoxelMeshGenerator::makeOneChunkMesh(indexX, indexY, indexZ, true);
                    },
                    [dynamicMesh, meshEntity, chunkMaterial](MeshData&& meshData) {
                        MeshAPI::update(dynamicMesh, meshData);
                        MeshComponentAPI::setMaterial(meshEntity, chunkMaterial);
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
    if (Input::isKeyPressed(Key::L)) {
        LOG_INFO("Hello Panda! var: %d", var);
    }
}

void BaseScript::shutdown() {
    GameContext::deinit();
}
