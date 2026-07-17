//
// Created by Admin on 12.02.2022.
//

#include "BlocksCreation.hpp"
#include "GameMenu.hpp"
#include "Model/TerrainMeshGenerator.hpp"
#include "Model/GameContext.hpp"
#include "NeverlandTouchControls.hpp"

#include <Bamboo/Input.hpp>
#include <Bamboo/ApplicationAPI.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Components/MeshComponentAPI.hpp>

#include <cmath>

namespace {

bool findTouchById(int id, Input::Touch &outTouch) {
    for (int index = 0; index < Input::touchCount(); index++) {
        Input::Touch touch;
        if (!Input::tryGetTouch(index, touch)) { continue; }
        if (touch.id != id) { continue; }
        outTouch = touch;
        return true;
    }
    return false;
}

float distanceSquared(float ax, float ay, float bx, float by) {
    const float dx = ax - bx;
    const float dy = ay - by;
    return dx * dx + dy * dy;
}

bool isKeyboardMovingInput() {
    return Input::isKeyPressed(Key::W) || Input::isKeyPressed(Key::A) ||
           Input::isKeyPressed(Key::S) || Input::isKeyPressed(Key::D);
}

bool isMovingInput() {
    const NeverlandTouchControls::MoveAxes touchAxes = NeverlandTouchControls::getMoveAxes();
    return isKeyboardMovingInput() || std::abs(touchAxes.x) > 0.001f ||
           std::abs(touchAxes.y) > 0.001f;
}

bool isSprintingInput(bool moving) {
    return moving && Input::isKeyPressed(Key::LEFT_SHIFT);
}

} // namespace

void BlocksCreation::start() {
    m_selectedBlock = VoxelType::GROUND;
    m_playerController = EntityAPI::getScript<PlayerController>(getEntity());
}

void BlocksCreation::shutdown() {
    m_heldItemView.shutdown();
    m_playerController.reset();
    m_touchAction = {};
}

void BlocksCreation::setSelectedBlock(VoxelType type) {
    if (type == VoxelType::NOTHING || type == VoxelType::COUNT) { return; }
    m_selectedBlock = type;
}

void BlocksCreation::updateSelectedBlock() {
    if (Input::isKeyJustPressed(Key::KEY_1)) { setSelectedBlock(VoxelType::GRASS); }
    if (Input::isKeyJustPressed(Key::KEY_2)) { setSelectedBlock(VoxelType::GROUND); }
    if (Input::isKeyJustPressed(Key::KEY_3)) { setSelectedBlock(VoxelType::STONE); }
    if (Input::isKeyJustPressed(Key::KEY_4)) { setSelectedBlock(VoxelType::SAND); }
    if (Input::isKeyJustPressed(Key::KEY_5)) { setSelectedBlock(VoxelType::BOARDS); }
    if (Input::isKeyJustPressed(Key::KEY_6)) { setSelectedBlock(VoxelType::STONE_BRICKS); }
    if (Input::isKeyJustPressed(Key::KEY_7)) { setSelectedBlock(VoxelType::SAND_STONE); }
}

void BlocksCreation::updateChunk(const ChunkCoord &coord) {
    if (!ChunksStorage::isChunkCoordInBounds(coord)) { return; }

    LOG_INFO("UPDATE CHUNK %d %d %d", coord.x, coord.y, coord.z);
    ChunkMeshSnapshot snapshot;
    if (!GameContext::s_chunkStorage->makeMeshSnapshot(coord, snapshot)) { return; }

    ChunkMeshBuildResult result = TerrainMeshGenerator::makeOneChunkMesh(snapshot, true);
    Chunk *chunk = GameContext::s_chunkStorage->getChunk(result.coord);
    if (chunk == nullptr || chunk->getVersion() != result.version) { return; }

    chunk->updateMeshes(result.terrainMesh, result.blocksMesh);
    chunk->applyMaterials(GameContext::s_terrainMaterial, GameContext::s_blocksMaterial);
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

void BlocksCreation::updateTouchBlockInput(
    float deltaTime, bool &placePressed, bool &breakPressed, Vec2 &touchAim, bool &hasTouchAim
) {
    const float width = static_cast<float>(ApplicationAPI::getWidth());
    const float height = static_cast<float>(ApplicationAPI::getHeight());
    if (width <= 0.0f || height <= 0.0f) { return; }
    NeverlandTouchControls::updateIgnoredTouches(width, height);

    constexpr float MOVE_THRESHOLD_SQUARED = 24.0f * 24.0f;
    constexpr float TAP_MAX_SECONDS = 0.32f;
    constexpr float HOLD_BREAK_SECONDS = 0.45f;
    constexpr float HOLD_REPEAT_SECONDS = 0.18f;

    if (m_touchAction.active) {
        if (NeverlandTouchControls::isTouchIgnored(m_touchAction.id)) {
            m_touchAction = {};
            return;
        }

        Input::Touch touch;
        if (findTouchById(m_touchAction.id, touch)) {
            m_touchAction.duration += deltaTime;
            m_touchAction.repeatTimer -= deltaTime;
            m_touchAction.lastX = touch.x;
            m_touchAction.lastY = touch.y;
            if (distanceSquared(touch.x, touch.y, m_touchAction.startX, m_touchAction.startY) >
                MOVE_THRESHOLD_SQUARED) {
                m_touchAction.moved = true;
            }
            if (!m_touchAction.moved && m_touchAction.duration >= HOLD_BREAK_SECONDS &&
                m_touchAction.repeatTimer <= 0.0f) {
                breakPressed = true;
                touchAim = Vec2(touch.x, touch.y);
                hasTouchAim = true;
                m_touchAction.repeatTimer = HOLD_REPEAT_SECONDS;
            }
            return;
        }

        if (!m_touchAction.moved && m_touchAction.duration <= TAP_MAX_SECONDS) {
            placePressed = true;
            touchAim = Vec2(m_touchAction.lastX, m_touchAction.lastY);
            hasTouchAim = true;
        }
        m_touchAction = {};
    }

    for (int index = 0; index < Input::touchCount(); index++) {
        Input::Touch touch;
        if (!Input::tryGetTouch(index, touch)) { continue; }
        if (NeverlandTouchControls::isTouchIgnored(touch.id)) { continue; }
        if (touch.x < width * 0.5f) { continue; }

        m_touchAction.id = touch.id;
        m_touchAction.startX = touch.x;
        m_touchAction.startY = touch.y;
        m_touchAction.lastX = touch.x;
        m_touchAction.lastY = touch.y;
        m_touchAction.duration = 0.0f;
        m_touchAction.repeatTimer = HOLD_BREAK_SECONDS;
        m_touchAction.active = true;
        m_touchAction.moved = false;
        break;
    }
}

void BlocksCreation::update(float deltaTime) {
    if (!GameMenu::isGameplayActive()) { return; } // меню: выбор блока и копание выключены
    updateSelectedBlock();
    const bool moving = isMovingInput();
    m_heldItemView.update(getEntity(), m_selectedBlock, moving, isSprintingInput(moving), deltaTime);

    // Keep voxel edits disabled until the initial world is visible.
    if (!GameContext::isWorldLoaded()) { return; }

    bool placePressed = false;
    bool breakPressed = false;
    if (Input::isKeyPressed(Key::E)) {
        placePressed = Input::isMouseButtonPressed(MouseButton::LEFT);
        breakPressed = Input::isMouseButtonPressed(MouseButton::RIGHT);
    } else {
        placePressed = Input::isMouseButtonJustPressed(MouseButton::LEFT);
        breakPressed = Input::isMouseButtonJustPressed(MouseButton::RIGHT);
    }
    Vec2 touchAim;
    bool hasTouchAim = false;
    updateTouchBlockInput(deltaTime, placePressed, breakPressed, touchAim, hasTouchAim);
    if (!placePressed && !breakPressed) { return; }
    if (!m_playerController) { return; }
    Vec3 position = getPosition();
    Vec3 target = hasTouchAim
                      ? m_playerController->getRayDirectionForScreenPoint(touchAim.x, touchAim.y)
                      : m_playerController->getFront();
    auto v = GameContext::s_chunkStorage->bresenham3D(
        position.x, position.y, position.z, target.x, target.y, target.z, MAXIMUM_DISTANCE
    );
    if (v && v->voxel != nullptr) {
        if (placePressed) {
            int x = v->end.x + v->normal.x;
            int y = v->end.y + v->normal.y;
            int z = v->end.z + v->normal.z;
            setVoxel(x, y, z, m_selectedBlock);
        } else if (breakPressed) {
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
