//
// Created by Admin on 12.02.2022.
//

#include "BlocksCreation.hpp"
#include "Model/VoxelMeshGenerator.hpp"
#include "Model/GameContext.hpp"

#include <Bamboo/Input.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>

void BlocksCreation::start() {
    m_selectedBlock = VoxelType::GROUND;
    m_cameraMove = EntityAPI::getScript<CameraMove>(getEntity());
}

void BlocksCreation::setSelectedBlock(VoxelType type) {
    if (type == VoxelType::NOTHING || type == VoxelType::COUNT) { return; }
    m_selectedBlock = type;
}

void BlocksCreation::updateSelectedBlock() {
    if (Input::isKeyJustPressed(Key::KEY_1)) { setSelectedBlock(VoxelType::GRASS); }
    if (Input::isKeyJustPressed(Key::KEY_2)) { setSelectedBlock(VoxelType::GROUND); }
    if (Input::isKeyJustPressed(Key::KEY_3)) { setSelectedBlock(VoxelType::TREE); }
    if (Input::isKeyJustPressed(Key::KEY_4)) { setSelectedBlock(VoxelType::BOARDS); }
    if (Input::isKeyJustPressed(Key::KEY_5)) { setSelectedBlock(VoxelType::STONE); }
    if (Input::isKeyJustPressed(Key::KEY_6)) { setSelectedBlock(VoxelType::STONE_BRICKS); }
    if (Input::isKeyJustPressed(Key::KEY_7)) { setSelectedBlock(VoxelType::SAND_STONE); }
    if (Input::isKeyJustPressed(Key::KEY_8)) { setSelectedBlock(VoxelType::SAND); }
}

void BlocksCreation::updateChunk(int chunkIndexX, int chunkIndexY, int chunkIndexZ) {
    LOG_INFO("UPDATE CHUNK %d %d %d", chunkIndexX, chunkIndexY, chunkIndexZ);
    MeshData data = VoxelMeshGenerator::makeOneChunkMesh(
        chunkIndexX, chunkIndexY, chunkIndexZ, true
    );
    MeshHandle mesh = GameContext::s_chunkStorage
        ->chunks
            [chunkIndexY * ChunksStorage::SIZE_X * ChunksStorage::SIZE_Z +
             chunkIndexX * ChunksStorage::SIZE_X + chunkIndexZ]
        .getMesh();
    MeshAPI::update(mesh, data);
}

void BlocksCreation::setVoxel(int x, int y, int z, VoxelType type) {
    if (x < 0 || y < 0 || z < 0 || x >= ChunksStorage::WORLD_SIZE_X ||
        y >= ChunksStorage::WORLD_SIZE_Y || z >= ChunksStorage::WORLD_SIZE_Z)
        return;

    GameContext::s_chunkStorage->setVoxel(x, y, z, type);
    int chunkIndexX = x / Chunk::SIZE_X;
    int chunkIndexY = y / Chunk::SIZE_Y;
    int chunkIndexZ = z / Chunk::SIZE_Z;
    updateChunk(chunkIndexX, chunkIndexY, chunkIndexZ);

    if (x % Chunk::SIZE_X == 0 && chunkIndexX > 0) {
        updateChunk(chunkIndexX - 1, chunkIndexY, chunkIndexZ);
    }
    if (x % Chunk::SIZE_X == Chunk::SIZE_X - 1 && chunkIndexX < ChunksStorage::SIZE_X - 1) {
        updateChunk(chunkIndexX + 1, chunkIndexY, chunkIndexZ);
    }
    if (z % Chunk::SIZE_Z == 0 && chunkIndexZ > 0) {
        updateChunk(chunkIndexX, chunkIndexY, chunkIndexZ - 1);
    }
    if (z % Chunk::SIZE_Z == Chunk::SIZE_Z - 1 && chunkIndexZ < ChunksStorage::SIZE_Z - 1) {
        updateChunk(chunkIndexX, chunkIndexY, chunkIndexZ + 1);
    }
    if (y % Chunk::SIZE_Y == 0 && chunkIndexY > 0) {
        updateChunk(chunkIndexX, chunkIndexY - 1, chunkIndexZ);
    }
    if (y % Chunk::SIZE_Y == Chunk::SIZE_Y - 1 && chunkIndexY < ChunksStorage::SIZE_Y - 1) {
        updateChunk(chunkIndexX, chunkIndexY + 1, chunkIndexZ);
    }
}

void BlocksCreation::update(float deltaTime) {
    updateSelectedBlock();

    bool leftPressed;
    bool rightPressed;
    if (Input::isKeyPressed(Key::E)) {
        leftPressed = Input::isMouseButtonPressed(MouseButton::LEFT);
        rightPressed = Input::isMouseButtonPressed(MouseButton::RIGHT);
    } else {
        leftPressed = Input::isMouseButtonJustPressed(MouseButton::LEFT);
        rightPressed = Input::isMouseButtonJustPressed(MouseButton::RIGHT);
    }
    if (!leftPressed && !rightPressed) { return; }
    Vec3 position = getPosition();
    Vec3 target = m_cameraMove->getFront();
    auto v = GameContext::s_chunkStorage->bresenham3D(
        position.x, position.y, position.z, target.x, target.y, target.z, MAXIMUM_DISTANCE
    );
    if (v && v->voxel != nullptr) {
        if (leftPressed) {
            int x = v->end.x + v->normal.x;
            int y = v->end.y + v->normal.y;
            int z = v->end.z + v->normal.z;
            setVoxel(x, y, z, m_selectedBlock);
        } else if (rightPressed) {
            int x = v->end.x;
            int y = v->end.y;
            int z = v->end.z;
            setVoxel(x, y, z, VoxelType::NOTHING);
        }
    }
}

Vec3 BlocksCreation::getPosition() {
    return TransformComponentAPI::getPosition(getEntity());
}
