//
// Created by Admin on 12.02.2022.
//

#include "BlocksCreation.hpp"
#include "GameMenu.hpp"
#include "Model/BlockPalette.hpp"
#include "Model/GameContext.hpp"
#include "Model/WorldSave.hpp"
#include "Physics/VoxelCharacterController.hpp"

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

// Меш подсветки набора вокселей: ПОЛУПРОЗРАЧНЫЕ квады внешних граней (грань видна, если
// соседний воксель не в наборе). Координаты — относительно base; материал маркера —
// unlit с блендом (s_markerMaterial), цвет и альфа — в вершинах.
MeshData makeVoxelHighlightMesh(
    const std::vector<TerrainAccess::Edit> &edits, int baseX, int baseY, int baseZ
) {
    MeshData data;
    const Vec2 uv(0.f, 0.f);                  // маркер-шейдер текстуру не семплит
    const Color color(1.f, 0.85f, 0.25f, 0.05f); // маленькая прозрачность заливки
    constexpr float OFFSET = 0.01f;           // отступ от грани наружу (против z-fighting)

    std::unordered_set<uint64_t> occupied;
    occupied.reserve(edits.size() * 2);
    for (const TerrainAccess::Edit &edit : edits) {
        occupied.insert(packVoxelKey(edit.x, edit.y, edit.z));
    }

    // Двусторонний полный квад грани воксела.
    auto addFaceQuad = [&](const TerrainAccess::Edit &edit, int axis, int direction) {
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
        auto pointAt = [&](float u, float v) {
            if (axis == 0) { return Vec3(origin.x + plane, origin.y + u, origin.z + v); }
            if (axis == 1) { return Vec3(origin.x + u, origin.y + plane, origin.z + v); }
            return Vec3(origin.x + u, origin.y + v, origin.z + plane);
        };
        const uint32_t base = static_cast<uint32_t>(data.vertices.size());
        for (const Vec3 &p : {pointAt(0.f, 0.f), pointAt(1.f, 0.f), pointAt(1.f, 1.f), pointAt(0.f, 1.f)}) {
            data.vertices.emplace_back(Vertex(p, uv, normal, color, 1.f));
        }
        for (uint32_t idx : {base, base + 1u, base + 2u, base + 2u, base + 3u, base,
                             base + 2u, base + 1u, base, base, base + 3u, base + 2u}) {
            data.indices.emplace_back(idx);
        }
    };

    for (const TerrainAccess::Edit &edit : edits) {
        if (!occupied.count(packVoxelKey(edit.x - 1, edit.y, edit.z))) { addFaceQuad(edit, 0, -1); }
        if (!occupied.count(packVoxelKey(edit.x + 1, edit.y, edit.z))) { addFaceQuad(edit, 0, 1); }
        if (!occupied.count(packVoxelKey(edit.x, edit.y - 1, edit.z))) { addFaceQuad(edit, 1, -1); }
        if (!occupied.count(packVoxelKey(edit.x, edit.y + 1, edit.z))) { addFaceQuad(edit, 1, 1); }
        if (!occupied.count(packVoxelKey(edit.x, edit.y, edit.z - 1))) { addFaceQuad(edit, 2, -1); }
        if (!occupied.count(packVoxelKey(edit.x, edit.y, edit.z + 1))) { addFaceQuad(edit, 2, 1); }
    }
    return data;
}

// Сигнатура ФОРМЫ набора (координаты относительно первого воксела): при сдвиге прицела
// без изменения формы меш не перестраивается — двигается только transform сущности.
uint64_t editsSignature(const std::vector<TerrainAccess::Edit> &edits) {
    uint64_t signature = 0x9E3779B97F4A7C15ull + edits.size();
    for (const TerrainAccess::Edit &edit : edits) {
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
    m_selectedElement = ArchObjectType::Block;
    if (WorldSave *save = GameContext::s_worldSave) {
        save->player.selectedBlock = static_cast<uint8_t>(type);
    }
}

void BlocksCreation::setSelectedElement(ArchObjectType element) {
    if (element >= ArchObjectType::COUNT) { return; }
    m_selectedElement = element;
    // Материал элемента — строительный: терраформ-материал в балку не превращается.
    if (isNaturalVoxelType(m_selectedBlock)) { m_selectedBlock = VoxelType::STONE_BRICKS; }
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
        m_brushMode = static_cast<GameBrushMode>((static_cast<int>(m_brushMode) + 1) % 3);
    }
}

void BlocksCreation::updateSelectedBlock() {
    // Клавиши 1-7 — строительный хотбар; терраформ выбирается в панели/меню.
    constexpr Key HOTKEYS[] = {Key::KEY_1, Key::KEY_2, Key::KEY_3, Key::KEY_4,
                               Key::KEY_5, Key::KEY_6, Key::KEY_7};
    for (size_t i = 0; i < BlockPalette::BUILDING_HOTBAR.size(); i++) {
        if (Input::isKeyJustPressed(HOTKEYS[i])) {
            setSelectedBlock(BlockPalette::BUILDING_HOTBAR[i].type);
        }
    }
}

BlocksCreation::AimHit BlocksCreation::aim(const Vec3 &origin, const Vec3 &direction) {
    AimHit hit;

    // Кубы построек — воксельный DDA.
    const auto blockHit = GameContext::s_buildingGrid->raycast(origin, direction, MAXIMUM_DISTANCE);
    float blockDistance = blockHit ? blockHit->distance : MAXIMUM_DISTANCE + 1.f;

    // Рельеф — точный луч по MC-поверхности через движок.
    Vec3 surfacePoint, surfaceNormal;
    float surfaceDistance = MAXIMUM_DISTANCE + 1.f;
    const bool surfaceHit =
        TerrainAccess::raycast(origin, direction, MAXIMUM_DISTANCE, surfacePoint, surfaceNormal);
    if (surfaceHit) {
        const float dx = surfacePoint.x - origin.x;
        const float dy = surfacePoint.y - origin.y;
        const float dz = surfacePoint.z - origin.z;
        surfaceDistance = std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    if (blockHit && blockDistance <= surfaceDistance) {
        hit.valid = true;
        hit.isBuilding = true;
        hit.x = blockHit->x;
        hit.y = blockHit->y;
        hit.z = blockHit->z;
        hit.normalX = blockHit->normalX;
        hit.normalY = blockHit->normalY;
        hit.normalZ = blockHit->normalZ;
        return hit;
    }
    if (surfaceHit) {
        hit.valid = true;
        hit.isBuilding = false;
        // Воксель рельефа: чуть внутрь поверхности вдоль нормали; грань — по доминанте нормали.
        hit.x = static_cast<int>(std::floor(surfacePoint.x - surfaceNormal.x * 0.05f));
        hit.y = static_cast<int>(std::floor(surfacePoint.y - surfaceNormal.y * 0.05f));
        hit.z = static_cast<int>(std::floor(surfacePoint.z - surfaceNormal.z * 0.05f));
        const float ax = std::abs(surfaceNormal.x);
        const float ay = std::abs(surfaceNormal.y);
        const float az = std::abs(surfaceNormal.z);
        if (ay >= ax && ay >= az) {
            hit.normalY = surfaceNormal.y > 0.f ? 1 : -1;
        } else if (ax >= az) {
            hit.normalX = surfaceNormal.x > 0.f ? 1 : -1;
        } else {
            hit.normalZ = surfaceNormal.z > 0.f ? 1 : -1;
        }
    }
    return hit;
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

void BlocksCreation::updateBrushMarker(const std::vector<TerrainAccess::Edit> &edits) {
#if defined(NEVERLAND_MOBILE)
    (void)edits;
#else
    MaterialHandle markerMaterial = GameContext::s_markerMaterial.isValid()
                                        ? GameContext::s_markerMaterial
                                        : GameContext::s_blocksMaterial;
    const bool wantVisible = !edits.empty() && markerMaterial.isValid();
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
        MeshComponentAPI::setMaterial(m_markerEntity, markerMaterial);
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

    if (!GameContext::isWorldLoaded() || !TerrainAccess::isReady()) { return; }

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
    const Vec3 position = getPosition();
    const Vec3 target = hasTouchAim
                            ? m_playerController->getRayDirectionForScreenPoint(touchAim.x, touchAim.y)
                            : m_playerController->getFront();
    const AimHit hit = aim(position, target);

    // Кисть работает только по природным материалам (без элемента в руке);
    // конструкции — архитектурные объекты, ставятся и ломаются целиком.
    const bool brushActive = !isElementSelected() && isNaturalVoxelType(m_selectedBlock);
    m_previewEdits.clear();
    if (hit.valid) {
        if (brushActive) {
            // Превью основного действия (ЛКМ): те же правки подсвечиваются и применяются.
            GameBrush::computeEdits(
                m_brushMode, /*secondary*/ false, hit.x, hit.y, hit.z, hit.normalX, hit.normalY,
                hit.normalZ, currentBrushRadius(), m_selectedBlock, *GameContext::s_buildingGrid,
                m_previewEdits
            );
        } else {
            // Объект: подсветка всех ячеек будущей установки.
            const ArchitectureObject object = pendingObject(hit);
            std::vector<std::array<int, 3>> cells;
            BuildingCellGrid::cellsFor(object, cells);
            for (const auto &cell : cells) {
                m_previewEdits.push_back({cell[0], cell[1], cell[2], object.material});
            }
        }
    }
    updateBrushMarker(m_previewEdits);

    if (!hit.valid || (!placePressed && !breakPressed)) { return; }
    if (placePressed) {
        if (brushActive) {
            TerrainAccess::applyEdits(m_previewEdits);
            VoxelCharacterController::invalidateFieldCache();
        } else {
            GameContext::s_buildingGrid->place(pendingObject(hit));
        }
    } else if (breakPressed) {
        if (hit.isBuilding) {
            // Объект исчезает целиком (владение ячейками — у объекта).
            GameContext::s_buildingGrid->removeObjectAt(hit.x, hit.y, hit.z);
        } else if (brushActive) {
            // Обратное действие кисти: вырезать/опустить.
            GameBrush::computeEdits(
                m_brushMode, /*secondary*/ true, hit.x, hit.y, hit.z, hit.normalX, hit.normalY,
                hit.normalZ, currentBrushRadius(), m_selectedBlock, *GameContext::s_buildingGrid,
                m_previewEdits
            );
            TerrainAccess::applyEdits(m_previewEdits);
            VoxelCharacterController::invalidateFieldCache();
        } else {
            // Строительный тип в руке, но целимся в рельеф — одиночный вырез воксела.
            std::vector<TerrainAccess::Edit> single = {{hit.x, hit.y, hit.z, VoxelType::NOTHING}};
            TerrainAccess::applyEdits(single);
            VoxelCharacterController::invalidateFieldCache();
        }
    }
}

ArchitectureObject BlocksCreation::pendingObject(const AimHit &hit) const {
    ArchitectureObject object;
    object.type = m_selectedElement;
    object.x = hit.x + hit.normalX;
    object.y = hit.y + hit.normalY;
    object.z = hit.z + hit.normalZ;
    object.material = m_selectedBlock;
    if (object.type == ArchObjectType::Beam && m_playerController) {
        // Балка растёт от прицела в направлении взгляда (доминантная ось XZ).
        const Vec3 front = m_playerController->getFront();
        if (std::abs(front.x) >= std::abs(front.z)) {
            object.rotation = front.x >= 0.f ? 0 : 2;
        } else {
            object.rotation = front.z >= 0.f ? 1 : 3;
        }
    }
    return object;
}

Vec3 BlocksCreation::getPosition() {
    return TransformComponentAPI::getPosition(getEntity());
}
