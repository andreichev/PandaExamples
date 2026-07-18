//
// Created by Admin on 12.02.2022.
//

#include "BlocksCreation.hpp"
#include "GameMenu.hpp"
#include "Model/TerrainMeshGenerator.hpp"
#include "Model/GameContext.hpp"
#include "Model/WorldSave.hpp"

#include <unordered_set>
#include "NeverlandTouchControls.hpp"

#include <Bamboo/Input.hpp>
#include <Bamboo/ApplicationAPI.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Components/MeshComponentAPI.hpp>
#include <Bamboo/Assets/AssetManagerAPI.hpp>
#include <Bamboo/Assets/MeshAPI.hpp>
#include <Bamboo/WorldAPI.hpp>

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

// Размеры кисти терраформа: 0 — одиночный воксель, дальше сферы растущего радиуса.
constexpr float BRUSH_RADII[] = {0.0f, 1.0f, 1.6f, 2.5f};
constexpr int BRUSH_SIZE_COUNT = 4;

// Ключ воксела для set: правки кисти лежат в малой окрестности, старшие биты не пересекаются.
uint64_t packVoxelKey(int x, int y, int z) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 42) ^
           (static_cast<uint64_t>(static_cast<uint32_t>(y)) << 21) ^
           static_cast<uint64_t>(static_cast<uint32_t>(z));
}

// Меш подсветки набора вокселей: рамки внешних граней (грань видна, если соседний воксель
// не в наборе). Координаты — относительно base; красится вершинным цветом поверх белого
// тайла блочного атласа (как рука в HeldItemView).
MeshData makeVoxelHighlightMesh(
    const std::vector<TerrainBrushEdit> &edits, int baseX, int baseY, int baseZ
) {
    MeshData data;
    const Vec2 whiteUV(0.5f / 7.f, 0.5f / 7.f);
    const Color color(1.f, 0.85f, 0.25f, 1.f);
    constexpr float FRAME = 0.07f;  // толщина рамки
    constexpr float OFFSET = 0.01f; // отступ от грани наружу (против z-fighting с terrain)

    std::unordered_set<uint64_t> occupied;
    occupied.reserve(edits.size() * 2);
    for (const TerrainBrushEdit &edit : edits) {
        occupied.insert(packVoxelKey(edit.x, edit.y, edit.z));
    }

    // Двусторонний квад в плоскости грани; (u0..u1, v0..v1) — прямоугольник в осях грани.
    auto addFrameQuad = [&](int axis, const Vec3 &origin, const Vec3 &normal, float plane,
                            float u0, float v0, float u1, float v1) {
        auto pointAt = [&](float u, float v) {
            if (axis == 0) { return Vec3(origin.x + plane, origin.y + u, origin.z + v); }
            if (axis == 1) { return Vec3(origin.x + u, origin.y + plane, origin.z + v); }
            return Vec3(origin.x + u, origin.y + v, origin.z + plane);
        };
        const uint32_t base = static_cast<uint32_t>(data.vertices.size());
        for (const Vec3 &p : {pointAt(u0, v0), pointAt(u1, v0), pointAt(u1, v1), pointAt(u0, v1)}) {
            data.vertices.emplace_back(Vertex(p, whiteUV, normal, color, 1.f));
        }
        for (uint32_t idx : {base, base + 1u, base + 2u, base + 2u, base + 3u, base,
                             base + 2u, base + 1u, base, base, base + 3u, base + 2u}) {
            data.indices.emplace_back(idx);
        }
    };

    // Рамка грани: 4 полосы по периметру единичного квадрата.
    auto addFaceFrame = [&](const TerrainBrushEdit &edit, int axis, int direction) {
        const Vec3 origin(
            static_cast<float>(edit.x - baseX),
            static_cast<float>(edit.y - baseY),
            static_cast<float>(edit.z - baseZ)
        );
        const float plane = direction > 0 ? 1.f + OFFSET : -OFFSET;
        const Vec3 normal(
            axis == 0 ? static_cast<float>(direction) : 0.f,
            axis == 1 ? static_cast<float>(direction) : 0.f,
            axis == 2 ? static_cast<float>(direction) : 0.f
        );
        addFrameQuad(axis, origin, normal, plane, 0.f, 0.f, 1.f, FRAME);
        addFrameQuad(axis, origin, normal, plane, 0.f, 1.f - FRAME, 1.f, 1.f);
        addFrameQuad(axis, origin, normal, plane, 0.f, FRAME, FRAME, 1.f - FRAME);
        addFrameQuad(axis, origin, normal, plane, 1.f - FRAME, FRAME, 1.f, 1.f - FRAME);
    };

    for (const TerrainBrushEdit &edit : edits) {
        if (!occupied.count(packVoxelKey(edit.x - 1, edit.y, edit.z))) { addFaceFrame(edit, 0, -1); }
        if (!occupied.count(packVoxelKey(edit.x + 1, edit.y, edit.z))) { addFaceFrame(edit, 0, 1); }
        if (!occupied.count(packVoxelKey(edit.x, edit.y - 1, edit.z))) { addFaceFrame(edit, 1, -1); }
        if (!occupied.count(packVoxelKey(edit.x, edit.y + 1, edit.z))) { addFaceFrame(edit, 1, 1); }
        if (!occupied.count(packVoxelKey(edit.x, edit.y, edit.z - 1))) { addFaceFrame(edit, 2, -1); }
        if (!occupied.count(packVoxelKey(edit.x, edit.y, edit.z + 1))) { addFaceFrame(edit, 2, 1); }
    }
    return data;
}

// Сигнатура ФОРМЫ набора (координаты относительно первого воксела): при сдвиге прицела
// без изменения формы меш не перестраивается — двигается только transform сущности.
uint64_t editsSignature(const std::vector<TerrainBrushEdit> &edits) {
    uint64_t signature = 0x9E3779B97F4A7C15ull + edits.size();
    for (const TerrainBrushEdit &edit : edits) {
        signature ^=
            packVoxelKey(edit.x - edits[0].x, edit.y - edits[0].y, edit.z - edits[0].z) *
            0x100000001B3ull;
    }
    return signature;
}

} // namespace

void BlocksCreation::start() {
    m_selectedBlock = VoxelType::GROUND;
    if (const WorldSave *save = GameContext::s_worldSave;
        save != nullptr && save->player.valid && save->player.selectedBlock < static_cast<uint8_t>(VoxelType::COUNT)) {
        const VoxelType saved = static_cast<VoxelType>(save->player.selectedBlock);
        if (saved != VoxelType::NOTHING) { m_selectedBlock = saved; }
    }
    m_playerController = EntityAPI::getScript<PlayerController>(getEntity());
}

void BlocksCreation::shutdown() {
    destroyBrushMarker();
    m_heldItemView.shutdown();
    m_playerController.reset();
    m_touchAction = {};
}

void BlocksCreation::setSelectedBlock(VoxelType type) {
    if (type == VoxelType::NOTHING || type == VoxelType::COUNT) { return; }
    m_selectedBlock = type;
    if (WorldSave *save = GameContext::s_worldSave) {
        save->player.selectedBlock = static_cast<uint8_t>(type);
    }
}

float BlocksCreation::currentBrushRadius() const {
    return BRUSH_RADII[std::clamp(m_brushSize, 0, BRUSH_SIZE_COUNT - 1)];
}

void BlocksCreation::setBrushSize(int size) {
    m_brushSize = std::clamp(size, 0, BRUSH_SIZE_COUNT - 1);
}

int BlocksCreation::brushSizeCount() {
    return BRUSH_SIZE_COUNT;
}

void BlocksCreation::updateBrushSize() {
    if (Input::isKeyJustPressed(Key::LEFT_BRACKET)) { setBrushSize(m_brushSize - 1); }
    if (Input::isKeyJustPressed(Key::RIGHT_BRACKET)) { setBrushSize(m_brushSize + 1); }
    if (Input::isKeyJustPressed(Key::B)) {
        // Циклер режима кисти (дублирует панель).
        m_brushMode = static_cast<TerrainBrushMode>(
            (static_cast<int>(m_brushMode) + 1) % 3
        );
    }
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

    // Воксель в ≤2 клетках от границы влияет на узлы/градиенты соседнего чанка (PADDING = 2).
    if (localX <= 1) { updateChunk({coord.x - 1, coord.y, coord.z}); }
    if (localX >= Chunk::SIZE_X - 2) { updateChunk({coord.x + 1, coord.y, coord.z}); }
    if (localZ <= 1) { updateChunk({coord.x, coord.y, coord.z - 1}); }
    if (localZ >= Chunk::SIZE_Z - 2) { updateChunk({coord.x, coord.y, coord.z + 1}); }
    if (localY <= 1) { updateChunk({coord.x, coord.y - 1, coord.z}); }
    if (localY >= Chunk::SIZE_Y - 2) { updateChunk({coord.x, coord.y + 1, coord.z}); }
}

void BlocksCreation::applyEdits(const std::vector<TerrainBrushEdit> &edits) {
    std::unordered_set<ChunkCoord, ChunkCoordHash> affected;
    for (const TerrainBrushEdit &edit : edits) {
        if (!ChunksStorage::isWorldCoordInBounds(edit.x, edit.y, edit.z)) { continue; }
        if (!GameContext::s_chunkStorage->setVoxel(edit.x, edit.y, edit.z, edit.type)) { continue; }

        const ChunkCoord coord = ChunksStorage::worldToChunkCoord(edit.x, edit.y, edit.z);
        const int localX = ChunksStorage::worldToLocalX(edit.x);
        const int localY = ChunksStorage::worldToLocalY(edit.y);
        const int localZ = ChunksStorage::worldToLocalZ(edit.z);
        affected.insert(coord);
        // Воксель в ≤2 клетках от границы влияет на узлы/градиенты соседнего чанка (PADDING = 2).
        if (localX <= 1) { affected.insert({coord.x - 1, coord.y, coord.z}); }
        if (localX >= Chunk::SIZE_X - 2) { affected.insert({coord.x + 1, coord.y, coord.z}); }
        if (localZ <= 1) { affected.insert({coord.x, coord.y, coord.z - 1}); }
        if (localZ >= Chunk::SIZE_Z - 2) { affected.insert({coord.x, coord.y, coord.z + 1}); }
        if (localY <= 1) { affected.insert({coord.x, coord.y - 1, coord.z}); }
        if (localY >= Chunk::SIZE_Y - 2) { affected.insert({coord.x, coord.y + 1, coord.z}); }
    }
    for (const ChunkCoord &coord : affected) { updateChunk(coord); }
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

void BlocksCreation::updateBrushMarker(const std::vector<TerrainBrushEdit> &edits) {
#if defined(NEVERLAND_MOBILE)
    (void)edits;
#else
    const bool wantVisible = !edits.empty() && GameContext::s_blocksMaterial.isValid();
    if (!wantVisible) {
        if (m_markerVisible && m_markerEntity.isValid()) {
            // Прячем за горизонт вместо destroy: маркер нужен почти каждый кадр.
            TransformComponentAPI::setPosition(m_markerEntity, {0.f, -10000.f, 0.f});
            m_markerVisible = false;
            m_markerSignature = 0;
        }
        return;
    }

    if (!m_markerMesh.isValid()) { m_markerMesh = AssetManagerAPI::createMesh(); }
    if (!m_markerEntity.isValid() && m_markerMesh.isValid()) {
        m_markerEntity = WorldAPI::createEntity("Brush Marker");
        EntityAPI::addComponent(m_markerEntity, ComponentType::MESH_COMPONENT);
        MeshComponentAPI::setMesh(m_markerEntity, m_markerMesh);
        MeshComponentAPI::setMaterial(m_markerEntity, GameContext::s_blocksMaterial);
    }
    // Меш в координатах относительно первого воксела набора, сущность — в его мировой позиции:
    // при сдвиге прицела без изменения формы двигается только transform.
    const uint64_t signature = editsSignature(edits);
    if (m_markerSignature != signature && m_markerMesh.isValid()) {
        MeshAPI::update(
            m_markerMesh, makeVoxelHighlightMesh(edits, edits[0].x, edits[0].y, edits[0].z)
        );
        m_markerSignature = signature;
    }
    TransformComponentAPI::setPosition(
        m_markerEntity,
        {static_cast<float>(edits[0].x), static_cast<float>(edits[0].y), static_cast<float>(edits[0].z)}
    );
    m_markerVisible = true;
#endif
}

void BlocksCreation::destroyBrushMarker() {
    if (m_markerMesh.isValid()) {
        AssetManagerAPI::deleteMesh(m_markerMesh);
        m_markerMesh = {};
    }
    if (m_markerEntity.isValid()) {
        WorldAPI::destroyEntity(m_markerEntity);
        m_markerEntity = {};
    }
    m_markerSignature = 0;
    m_markerVisible = false;
}

void BlocksCreation::update(float deltaTime) {
    if (!GameMenu::isGameplayActive()) {
        m_previewEdits.clear();
        updateBrushMarker(m_previewEdits); // меню: маркер прячем
        return;
    }
    updateSelectedBlock();
    updateBrushSize();
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
    // Пока зажат Alt (свободный курсор для панели кисти) клики уходят в UI, не в мир.
    if (GameMenu::isUIModifierHeld()) {
        placePressed = false;
        breakPressed = false;
    }
    // Рейкаст каждый кадр: маркер кисти показывает точку редактирования и без кликов.
    if (!m_playerController) { return; }
    Vec3 position = getPosition();
    Vec3 target = hasTouchAim
                      ? m_playerController->getRayDirectionForScreenPoint(touchAim.x, touchAim.y)
                      : m_playerController->getFront();
    auto v = GameContext::s_chunkStorage->bresenham3D(
        position.x, position.y, position.z, target.x, target.y, target.z, MAXIMUM_DISTANCE
    );
    const bool hasHit = v && v->voxel != nullptr;
    // Кисть работает только по природным типам; конструкции ставятся/ломаются по одному вокселю.
    const bool brushActive = TerrainMeshGenerator::isNaturalType(m_selectedBlock);
    const int hitX = hasHit ? static_cast<int>(v->end.x) : 0;
    const int hitY = hasHit ? static_cast<int>(v->end.y) : 0;
    const int hitZ = hasHit ? static_cast<int>(v->end.z) : 0;
    const int normalX = hasHit ? static_cast<int>(v->normal.x) : 0;
    const int normalY = hasHit ? static_cast<int>(v->normal.y) : 0;
    const int normalZ = hasHit ? static_cast<int>(v->normal.z) : 0;
    m_previewEdits.clear();
    if (hasHit) {
        if (brushActive) {
            // Превью основного действия (ЛКМ): те же правки подсвечиваются и применяются.
            TerrainBrush::computeEdits(
                m_brushMode, /*secondary*/ false, hitX, hitY, hitZ, normalX, normalY, normalZ,
                currentBrushRadius(), m_selectedBlock, m_previewEdits
            );
        } else {
            // Конструкция: подсветка одного воксела будущей установки, без учёта кисти.
            m_previewEdits.push_back(
                {hitX + normalX, hitY + normalY, hitZ + normalZ, m_selectedBlock}
            );
        }
    }
    updateBrushMarker(m_previewEdits);
    if (hasHit && (placePressed || breakPressed)) {
        if (placePressed) {
            if (brushActive) {
                applyEdits(m_previewEdits);
            } else {
                setVoxel(hitX + normalX, hitY + normalY, hitZ + normalZ, m_selectedBlock);
            }
        } else if (breakPressed) {
            if (brushActive && TerrainMeshGenerator::isNaturalType(v->voxel->type)) {
                // Обратное действие кисти: вырезать/опустить.
                TerrainBrush::computeEdits(
                    m_brushMode, /*secondary*/ true, hitX, hitY, hitZ, normalX, normalY, normalZ,
                    currentBrushRadius(), m_selectedBlock, m_previewEdits
                );
                applyEdits(m_previewEdits);
            } else {
                setVoxel(hitX, hitY, hitZ, VoxelType::NOTHING);
            }
        }
    }
}

Vec3 BlocksCreation::getPosition() {
    return TransformComponentAPI::getPosition(getEntity());
}
