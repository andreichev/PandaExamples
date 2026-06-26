//
// Created by Admin on 12.02.2022.
//

#include "BlocksCreation.hpp"
#include "Model/VoxelMeshGenerator.hpp"
#include "Model/GameContext.hpp"

#include <Bamboo/Input.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Components/MeshComponentAPI.hpp>

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

void BlocksCreation::updateChunk(const ChunkCoord &coord) {
    if (!ChunksStorage::isChunkCoordInBounds(coord)) { return; }

    LOG_INFO("UPDATE CHUNK %d %d %d", coord.x, coord.y, coord.z);
    ChunkMeshSnapshot snapshot;
    if (!GameContext::s_chunkStorage->makeMeshSnapshot(coord, snapshot)) { return; }

    ChunkMeshBuildResult result = VoxelMeshGenerator::makeOneChunkMesh(snapshot, true);
    Chunk *chunk = GameContext::s_chunkStorage->getChunk(result.coord);
    if (chunk == nullptr || chunk->getVersion() != result.version) { return; }

    chunk->updateMesh(result.meshData);
    if (chunk->hasView() && GameContext::s_chunkMaterial.id != BAMBOO_INVALID_HANDLE) {
        MeshComponentAPI::setMaterial(chunk->getEntity(), GameContext::s_chunkMaterial);
    }
    chunk->clearNeedsRemesh();
}

void BlocksCreation::setVoxel(int x, int y, int z, VoxelType type) {
    if (!ChunksStorage::isWorldCoordInBounds(x, y, z)) { return; }

    if (!GameContext::s_chunkStorage->setVoxel(x, y, z, type)) { return; }
    ChunkCoord coord = ChunksStorage::worldToChunkCoord(x, y, z);
    const int localX = ChunksStorage::worldToLocalX(x);
    const int localY = ChunksStorage::worldToLocalY(y);
    const int localZ = ChunksStorage::worldToLocalZ(z);

    updateChunk(coord);

    if (localX == 0) { updateChunk({coord.x - 1, coord.y, coord.z}); }
    if (localX == Chunk::SIZE_X - 1) { updateChunk({coord.x + 1, coord.y, coord.z}); }
    if (localZ == 0) { updateChunk({coord.x, coord.y, coord.z - 1}); }
    if (localZ == Chunk::SIZE_Z - 1) { updateChunk({coord.x, coord.y, coord.z + 1}); }
    if (localY == 0) { updateChunk({coord.x, coord.y - 1, coord.z}); }
    if (localY == Chunk::SIZE_Y - 1) { updateChunk({coord.x, coord.y + 1, coord.z}); }
}

void BlocksCreation::update(float deltaTime) {
    updateSelectedBlock();

    // Keep voxel edits disabled until the initial world is visible.
    if (!GameContext::isWorldLoaded()) { return; }

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
