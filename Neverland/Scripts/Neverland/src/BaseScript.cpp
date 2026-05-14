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
    for (int indexX = 0; indexX < ChunksStorage::SIZE_X; indexX++) {
        for (int indexY = 0; indexY < ChunksStorage::SIZE_Y; indexY++) {
            for (int indexZ = 0; indexZ < ChunksStorage::SIZE_Z; indexZ++) {
                MeshData meshData = VoxelMeshGenerator::makeOneChunkMesh(
                    indexX, indexY, indexZ, true
                );
                Chunk& chunk =
                    GameContext::s_chunkStorage
                        ->chunks
                            [indexY * ChunksStorage::SIZE_X * ChunksStorage::SIZE_Z +
                             indexX * ChunksStorage::SIZE_X + indexZ];
                MeshHandle dynamicMesh = chunk.getMesh();
                MeshAPI::update(dynamicMesh, meshData);
                EntityHandle meshEntity = chunk.getEntity();
                MeshComponentAPI::setMaterial(meshEntity, material);
            }
        }
    }
}

void BaseScript::update(float dt) {
    if (Input::isKeyPressed(Key::L)) {
        LOG_INFO("Hello Panda! var: %d", var);
    }
}

void BaseScript::shutdown() {
    GameContext::deinit();
}
