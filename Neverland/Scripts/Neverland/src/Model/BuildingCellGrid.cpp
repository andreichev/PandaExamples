#include "BuildingCellGrid.hpp"
#include "VoxelTextureMapper.hpp"

#include <Bamboo/Assets/AssetManagerAPI.hpp>
#include <Bamboo/Components/MeshComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/WorldAPI.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace {

constexpr int BLOCKS_ATLAS_GRID = 7;
constexpr float AO_FACTOR = 0.2f;

Vec2 tileUV(uint8_t tileIndex) {
    const float uvSize = 1.f / BLOCKS_ATLAS_GRID;
    return {tileIndex % BLOCKS_ATLAS_GRID * uvSize + 0.0005f, tileIndex / BLOCKS_ATLAS_GRID * uvSize + 0.0005f};
}

Color hexColor(uint32_t hex) {
    return Color(
        (hex >> 24) / 255.f, ((hex >> 16) & 0xFF) / 255.f, ((hex >> 8) & 0xFF) / 255.f,
        (hex & 0xFF) / 255.f
    );
}

// Грань куба: нормаль, свет, 4 вершины (offsets углов) и для каждой вершины — 3 направления
// соседей AO (два ребра + диагональ) в мировых смещениях от куба.
struct FaceSpec {
    int normal[3];
    float light;
    int corners[4][3];      // позиции вершин (0/1 по осям)
    int aoNeighbors[4][3][3]; // [вершина][сосед][xyz-смещение]
};

// Порядок вершин каждой грани — против часовой снаружи (как в старом генераторе).
constexpr FaceSpec FACES[6] = {
    // +Z (front), свет 1.0
    {{0, 0, 1}, 1.0f,
     {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
     {{{-1, 0, 1}, {0, -1, 1}, {-1, -1, 1}}, {{1, 0, 1}, {0, -1, 1}, {1, -1, 1}},
      {{1, 0, 1}, {0, 1, 1}, {1, 1, 1}}, {{-1, 0, 1}, {0, 1, 1}, {-1, 1, 1}}}},
    // -Z (back), свет 0.75
    {{0, 0, -1}, 0.75f,
     {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}},
     {{{-1, 0, -1}, {0, -1, -1}, {-1, -1, -1}}, {{-1, 0, -1}, {0, 1, -1}, {-1, 1, -1}},
      {{1, 0, -1}, {0, 1, -1}, {1, 1, -1}}, {{1, 0, -1}, {0, -1, -1}, {1, -1, -1}}}},
    // +Y (top), свет 0.95
    {{0, 1, 0}, 0.95f,
     {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}},
     {{{-1, 1, 0}, {0, 1, -1}, {-1, 1, -1}}, {{-1, 1, 0}, {0, 1, 1}, {-1, 1, 1}},
      {{1, 1, 0}, {0, 1, 1}, {1, 1, 1}}, {{1, 1, 0}, {0, 1, -1}, {1, 1, -1}}}},
    // -Y (bottom), свет 0.85
    {{0, -1, 0}, 0.85f,
     {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
     {{{-1, -1, 0}, {0, -1, -1}, {-1, -1, -1}}, {{1, -1, 0}, {0, -1, -1}, {1, -1, -1}},
      {{1, -1, 0}, {0, -1, 1}, {1, -1, 1}}, {{-1, -1, 0}, {0, -1, 1}, {-1, -1, 1}}}},
    // -X (left), свет 0.9
    {{-1, 0, 0}, 0.9f,
     {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},
     {{{-1, 0, -1}, {-1, -1, 0}, {-1, -1, -1}}, {{-1, 0, 1}, {-1, -1, 0}, {-1, -1, 1}},
      {{-1, 0, 1}, {-1, 1, 0}, {-1, 1, 1}}, {{-1, 0, -1}, {-1, 1, 0}, {-1, 1, -1}}}},
    // +X (right), свет 0.8
    {{1, 0, 0}, 0.8f,
     {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}},
     {{{1, 0, -1}, {1, -1, 0}, {1, -1, -1}}, {{1, 0, -1}, {1, 1, 0}, {1, 1, -1}},
      {{1, 0, 1}, {1, 1, 0}, {1, 1, 1}}, {{1, 0, 1}, {1, -1, 0}, {1, -1, 1}}}},
};

// UV углов грани в тайле (по порядку вершин): низ-лево, низ-право, верх-право, верх-лево —
// подгонка под старый генератор не критична (тайлы симметричны по художке).
constexpr float CORNER_UV[4][2] = {{0.f, 1.f}, {1.f, 1.f}, {1.f, 0.f}, {0.f, 0.f}};

} // namespace

void BuildingCellGrid::init(
    int minX, int minY, int minZ, int sizeX, int sizeY, int sizeZ, MaterialHandle material
) {
    shutdown();
    m_minX = minX;
    m_minY = minY;
    m_minZ = minZ;
    m_sizeX = sizeX;
    m_sizeY = sizeY;
    m_sizeZ = sizeZ;
    m_chunksX = (sizeX + CHUNK - 1) / CHUNK;
    m_chunksY = (sizeY + CHUNK - 1) / CHUNK;
    m_chunksZ = (sizeZ + CHUNK - 1) / CHUNK;
    m_blocks.assign(static_cast<size_t>(sizeX) * sizeY * sizeZ, 0);
    m_views.assign(static_cast<size_t>(m_chunksX) * m_chunksY * m_chunksZ, {});
    m_material = material;
}

void BuildingCellGrid::shutdown() {
    for (ChunkView &view : m_views) {
        if (view.mesh.isValid()) { AssetManagerAPI::deleteMesh(view.mesh); }
        if (view.entity.isValid()) { WorldAPI::destroyEntity(view.entity); }
        view = {};
    }
    m_views.clear();
    m_blocks.clear();
    m_cellOwner.clear();
    m_objects.clear();
    m_nextObjectId = 1;
}

bool BuildingCellGrid::isInside(int x, int y, int z) const {
    return x >= m_minX && y >= m_minY && z >= m_minZ && x < m_minX + m_sizeX &&
           y < m_minY + m_sizeY && z < m_minZ + m_sizeZ;
}

size_t BuildingCellGrid::voxelIndex(int x, int y, int z) const {
    return (static_cast<size_t>(y - m_minY) * m_sizeZ + static_cast<size_t>(z - m_minZ)) * m_sizeX +
           static_cast<size_t>(x - m_minX);
}

size_t BuildingCellGrid::chunkIndex(int chunkX, int chunkY, int chunkZ) const {
    return (static_cast<size_t>(chunkY) * m_chunksZ + chunkZ) * m_chunksX + chunkX;
}

VoxelType BuildingCellGrid::blockAt(int x, int y, int z) const {
    if (!isInside(x, y, z)) { return VoxelType::NOTHING; }
    return static_cast<VoxelType>(m_blocks[voxelIndex(x, y, z)]);
}

uint64_t BuildingCellGrid::packCell(int x, int y, int z) {
    const uint64_t bx = static_cast<uint64_t>(x + (1 << 20)) & 0x1FFFFF;
    const uint64_t by = static_cast<uint64_t>(y + (1 << 20)) & 0x1FFFFF;
    const uint64_t bz = static_cast<uint64_t>(z + (1 << 20)) & 0x1FFFFF;
    return (bx << 42) | (by << 21) | bz;
}

void BuildingCellGrid::cellsFor(
    const ArchitectureObject &object, std::vector<std::array<int, 3>> &outCells
) {
    outCells.clear();
    switch (object.type) {
        case ArchObjectType::Block:
            outCells.push_back({object.x, object.y, object.z});
            return;
        case ArchObjectType::Beam: {
            // Балка 3×1×1 от origin вдоль оси поворота.
            const int stepX = object.rotation == 0 ? 1 : object.rotation == 2 ? -1 : 0;
            const int stepZ = object.rotation == 1 ? 1 : object.rotation == 3 ? -1 : 0;
            for (int i = 0; i < 3; i++) {
                outCells.push_back({object.x + stepX * i, object.y, object.z + stepZ * i});
            }
            return;
        }
        case ArchObjectType::Wall:
        case ArchObjectType::Window:
        case ArchObjectType::Door:
            // Колонна 1×WALL_HEIGHT×1; rotation влияет только на геометрию плоскостей.
            for (int i = 0; i < WALL_HEIGHT; i++) {
                outCells.push_back({object.x, object.y + i, object.z});
            }
            return;
        case ArchObjectType::COUNT:
            return;
    }
}

const ArchitectureObject *BuildingCellGrid::wallAt(int x, int y, int z) const {
    const ArchitectureObject *object = objectAt(x, y, z);
    return object != nullptr && isWallFamilyType(object->type) ? object : nullptr;
}

bool BuildingCellGrid::isPhysicsSolidAt(int x, int y, int z) const {
    if (!isSolidAt(x, y, z)) { return false; }
    const ArchitectureObject *object = objectAt(x, y, z);
    // Дверной проём (нижние 2 ячейки модуля) — проходим; перемычка твёрдая.
    if (object != nullptr && object->type == ArchObjectType::Door && y < object->y + 2) {
        return false;
    }
    return true;
}

bool BuildingCellGrid::isFullCellAt(int x, int y, int z) const {
    if (!isSolidAt(x, y, z)) { return false; }
    const ArchitectureObject *object = objectAt(x, y, z);
    // Тонкие элементы (стены) занимают ячейку топологически, но не заполняют её:
    // грань соседнего куба им не прятать, жёсткого AO не давать.
    return object == nullptr || object->type == ArchObjectType::Block ||
           object->type == ArchObjectType::Beam;
}

bool BuildingCellGrid::canPlace(const ArchitectureObject &object) const {
    std::vector<std::array<int, 3>> cells;
    cellsFor(object, cells);
    if (cells.empty()) { return false; }
    for (const auto &cell : cells) {
        if (!isInside(cell[0], cell[1], cell[2])) { return false; }
        if (m_blocks[voxelIndex(cell[0], cell[1], cell[2])] != 0) { return false; }
    }
    return true;
}

void BuildingCellGrid::writeObjectCells(const ArchitectureObject &object, bool clear) {
    std::vector<std::array<int, 3>> cells;
    cellsFor(object, cells);
    for (const auto &cell : cells) {
        if (!isInside(cell[0], cell[1], cell[2])) { continue; }
        m_blocks[voxelIndex(cell[0], cell[1], cell[2])] =
            clear ? 0 : static_cast<uint8_t>(object.material);
        if (clear) {
            m_cellOwner.erase(packCell(cell[0], cell[1], cell[2]));
        } else {
            m_cellOwner[packCell(cell[0], cell[1], cell[2])] = object.id;
        }
        markDirtyAround(cell[0], cell[1], cell[2]);
    }
}

uint32_t BuildingCellGrid::placeInternal(ArchitectureObject object, bool rebuild) {
    if (!canPlace(object)) { return 0; }
    object.id = m_nextObjectId++;
    writeObjectCells(object, false);
    m_objects.emplace(object.id, object);
    if (rebuild) { rebuildDirtyChunks(); }
    return object.id;
}

uint32_t BuildingCellGrid::place(ArchitectureObject object) {
    return placeInternal(object, true);
}

bool BuildingCellGrid::removeObjectAt(int x, int y, int z) {
    auto ownerIt = m_cellOwner.find(packCell(x, y, z));
    if (ownerIt == m_cellOwner.end()) { return false; }
    auto objectIt = m_objects.find(ownerIt->second);
    if (objectIt == m_objects.end()) { // осиротевшая ячейка — чистим точечно
        m_cellOwner.erase(ownerIt);
        m_blocks[voxelIndex(x, y, z)] = 0;
        markDirtyAround(x, y, z);
        rebuildDirtyChunks();
        return true;
    }
    writeObjectCells(objectIt->second, true);
    m_objects.erase(objectIt);
    rebuildDirtyChunks();
    return true;
}

const ArchitectureObject *BuildingCellGrid::objectAt(int x, int y, int z) const {
    auto ownerIt = m_cellOwner.find(packCell(x, y, z));
    if (ownerIt == m_cellOwner.end()) { return nullptr; }
    auto objectIt = m_objects.find(ownerIt->second);
    return objectIt != m_objects.end() ? &objectIt->second : nullptr;
}

void BuildingCellGrid::markDirtyAround(int x, int y, int z) {
    // Блок на границе чанка меняет AO/грани соседа — пометить чанки клетки и соседей.
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            for (int dz = -1; dz <= 1; dz++) {
                const int nx = x + dx, ny = y + dy, nz = z + dz;
                if (!isInside(nx, ny, nz)) { continue; }
                const int chunkX = (nx - m_minX) / CHUNK;
                const int chunkY = (ny - m_minY) / CHUNK;
                const int chunkZ = (nz - m_minZ) / CHUNK;
                m_views[chunkIndex(chunkX, chunkY, chunkZ)].dirty = true;
            }
        }
    }
}

void BuildingCellGrid::rebuildDirtyChunks() {
    for (int chunkY = 0; chunkY < m_chunksY; chunkY++) {
        for (int chunkZ = 0; chunkZ < m_chunksZ; chunkZ++) {
            for (int chunkX = 0; chunkX < m_chunksX; chunkX++) {
                if (m_views[chunkIndex(chunkX, chunkY, chunkZ)].dirty) {
                    rebuildChunk(chunkX, chunkY, chunkZ);
                }
            }
        }
    }
}

void BuildingCellGrid::rebuildChunk(int chunkX, int chunkY, int chunkZ) {
    ChunkView &view = m_views[chunkIndex(chunkX, chunkY, chunkZ)];
    view.dirty = false;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    const int startX = m_minX + chunkX * CHUNK;
    const int startY = m_minY + chunkY * CHUNK;
    const int startZ = m_minZ + chunkZ * CHUNK;
    const int endX = std::min(startX + CHUNK, m_minX + m_sizeX);
    const int endY = std::min(startY + CHUNK, m_minY + m_sizeY);
    const int endZ = std::min(startZ + CHUNK, m_minZ + m_sizeZ);

    for (int y = startY; y < endY; y++) {
        for (int z = startZ; z < endZ; z++) {
            for (int x = startX; x < endX; x++) {
                const VoxelType type = blockAt(x, y, z);
                if (type == VoxelType::NOTHING) { continue; }
                if (wallAt(x, y, z) != nullptr) { continue; } // стены рисует WallRun-проход
                Voxel voxel(type);
                const VoxelTextureData &texture = VoxelTextureMapper::getTextureData(&voxel);
                for (const FaceSpec &face : FACES) {
                    // Грань прячет только соседняя ПОЛНАЯ ячейка построек: рельеф MC срезает
                    // поверхность, а тонкие элементы (стены) ячейку не заполняют.
                    if (isFullCellAt(x + face.normal[0], y + face.normal[1], z + face.normal[2])) {
                        continue;
                    }
                    const bool top = face.normal[1] > 0;
                    const bool bottom = face.normal[1] < 0;
                    const uint8_t tileIndex = top      ? texture.topTileIndex
                                              : bottom ? texture.bottomTileIndex
                                                       : texture.sideTileIndex;
                    const uint32_t tint = top      ? texture.topColor
                                          : bottom ? texture.bottomColor
                                                   : texture.sideColor;
                    const Vec2 uvBase = tileUV(tileIndex);
                    const float uvSize = 1.f / BLOCKS_ATLAS_GRID - 0.001f;
                    const Vec3 normal(
                        static_cast<float>(face.normal[0]), static_cast<float>(face.normal[1]),
                        static_cast<float>(face.normal[2])
                    );

                    const uint32_t base = static_cast<uint32_t>(vertices.size());
                    for (uint32_t index : {base, base + 1u, base + 2u, base + 2u, base + 3u, base}) {
                        indices.emplace_back(index);
                    }
                    for (int corner = 0; corner < 4; corner++) {
                        float occlusion = 0.f;
                        for (int neighbor = 0; neighbor < 3; neighbor++) {
                            const int *offset = face.aoNeighbors[corner][neighbor];
                            if (isFullCellAt(x + offset[0], y + offset[1], z + offset[2])) {
                                occlusion += AO_FACTOR;
                            }
                        }
                        const Vec3 position(
                            static_cast<float>(x + face.corners[corner][0]),
                            static_cast<float>(y + face.corners[corner][1]),
                            static_cast<float>(z + face.corners[corner][2])
                        );
                        const Vec2 uv(
                            uvBase.x + CORNER_UV[corner][0] * uvSize,
                            uvBase.y + CORNER_UV[corner][1] * uvSize
                        );
                        vertices.emplace_back(Vertex(
                            position, uv, normal, hexColor(tint), face.light * (1.f - occlusion)
                        ));
                    }
                }
            }
        }
    }

    appendWallGeometry(startX, startY, startZ, endX, endY, endZ, vertices, indices);

    if (vertices.empty()) {
        if (view.mesh.isValid()) {
            AssetManagerAPI::deleteMesh(view.mesh);
            view.mesh = {};
        }
        if (view.entity.isValid()) {
            WorldAPI::destroyEntity(view.entity);
            view.entity = {};
        }
        return;
    }

    if (!view.mesh.isValid()) { view.mesh = AssetManagerAPI::createMesh(); }
    MeshData meshData;
    meshData.vertices = std::move(vertices);
    meshData.indices = std::move(indices);
    MeshAPI::update(view.mesh, meshData);
    if (!view.entity.isValid()) {
        const std::string name = "Buildings " + std::to_string(chunkX) + "," +
                                 std::to_string(chunkY) + "," + std::to_string(chunkZ);
        view.entity = WorldAPI::createEntity(name.c_str());
        EntityAPI::addComponent(view.entity, ComponentType::MESH_COMPONENT);
        MeshComponentAPI::setMesh(view.entity, view.mesh);
        MeshComponentAPI::setMaterial(view.entity, m_material);
        TransformComponentAPI::setPosition(view.entity, {0.f, 0.f, 0.f});
    }
}

namespace {

// Толщина стенного полотна (визуальная; топологически стена занимает ячейку).
constexpr float WALL_THICKNESS = 0.3f;
// Проём окна/двери: высота подоконной стенки, низ перемычки, боковые простенки.
constexpr float WALL_SILL_HEIGHT = 0.9f;
constexpr float WALL_HEAD_HEIGHT = 2.1f;
constexpr float WALL_WINDOW_JAMB = 0.15f;
constexpr float WALL_DOOR_JAMB = 0.1f;
// Рама окна: сечение бруска и тайл дерева в blocks-атласе.
constexpr float WALL_FRAME_BAR = 0.08f;
constexpr uint8_t WALL_FRAME_TILE = 2;

// Свет граней — те же значения, что у кубов (FACES), но без AO: полотно едино.
float wallFaceLight(int normalX, int normalY, int normalZ) {
    if (normalZ > 0) { return 1.0f; }
    if (normalZ < 0) { return 0.75f; }
    if (normalY > 0) { return 0.95f; }
    if (normalY < 0) { return 0.85f; }
    if (normalX < 0) { return 0.9f; }
    return 0.8f;
}

// Квад стены: p0..p3 против часовой снаружи; UV — доли атласного тайла (v вниз, как у кубов).
void addWallQuad(
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices,
    const Vec3 &p0,
    const Vec3 &p1,
    const Vec3 &p2,
    const Vec3 &p3,
    const Vec3 &normal,
    uint8_t tileIndex,
    uint32_t tint,
    float u0,
    float v0,
    float u1,
    float v1
) {
    const Vec2 uvBase = tileUV(tileIndex);
    const float uvSize = 1.f / BLOCKS_ATLAS_GRID - 0.001f;
    const float light = wallFaceLight(
        static_cast<int>(normal.x), static_cast<int>(normal.y), static_cast<int>(normal.z)
    );
    const uint32_t base = static_cast<uint32_t>(vertices.size());
    for (uint32_t index : {base, base + 1u, base + 2u, base + 2u, base + 3u, base}) {
        indices.emplace_back(index);
    }
    const Vec2 uvs[4] = {
        {uvBase.x + u0 * uvSize, uvBase.y + v1 * uvSize},
        {uvBase.x + u1 * uvSize, uvBase.y + v1 * uvSize},
        {uvBase.x + u1 * uvSize, uvBase.y + v0 * uvSize},
        {uvBase.x + u0 * uvSize, uvBase.y + v0 * uvSize},
    };
    const Vec3 points[4] = {p0, p1, p2, p3};
    for (int i = 0; i < 4; i++) {
        vertices.emplace_back(Vertex(points[i], uvs[i], normal, hexColor(tint), light));
    }
}

} // namespace

// Раны линии стен: последовательности модулей семейства (Wall/Window/Door) одной
// оси/материала/базы сливаются в единое полотно — крупные плоскости без пер-ячеечного
// AO («не воксельная геометрия», диздок). Ран может пересекать границы чанков: каждый
// чанк рисует свои ячейки (UV мировой — стык бесшовный). Окно/дверь не рвут полотно:
// их ячейка генерит куски полотна вокруг проёма, откосы и раму. Внешний угол —
// продление полотна до дальней плоскости перпендикулярной стены того же материала.
void BuildingCellGrid::appendWallGeometry(
    int startX, int startY, int startZ, int endX, int endY, int endZ,
    std::vector<Vertex> &vertices, std::vector<uint32_t> &indices
) const {
    std::unordered_map<uint32_t, bool> processedRuns; // id стартового объекта рана

    const auto sameWall = [this](const ArchitectureObject &sample, int x, int z) -> const ArchitectureObject * {
        const ArchitectureObject *wall = wallAt(x, sample.y, z);
        if (wall == nullptr || wall->rotation != sample.rotation ||
            wall->material != sample.material || wall->y != sample.y) {
            return nullptr;
        }
        return wall;
    };
    // Перпендикулярная стена того же материала в угловой ячейке (для продления полотна).
    const auto cornerWall = [this](const ArchitectureObject &sample, int x, int z) {
        const ArchitectureObject *wall = wallAt(x, sample.y, z);
        return wall != nullptr && wall->rotation != sample.rotation &&
               wall->material == sample.material && wall->y == sample.y;
    };

    for (int y = startY; y < endY; y++) {
        for (int z = startZ; z < endZ; z++) {
            for (int x = startX; x < endX; x++) {
                const ArchitectureObject *wall = wallAt(x, y, z);
                if (wall == nullptr || y != wall->y) { continue; } // ран ведём от базовых ячеек
                const int stepX = wall->rotation == 0 ? 1 : 0;
                const int stepZ = wall->rotation == 0 ? 0 : 1;

                const ArchitectureObject *runStart = wall;
                while (const ArchitectureObject *previous =
                           sameWall(*wall, runStart->x - stepX, runStart->z - stepZ)) {
                    runStart = previous;
                }
                if (processedRuns.count(runStart->id)) { continue; }
                processedRuns.emplace(runStart->id, true);

                int runLength = 1;
                const ArchitectureObject *runEnd = runStart;
                while (const ArchitectureObject *next =
                           sameWall(*wall, runEnd->x + stepX, runEnd->z + stepZ)) {
                    runEnd = next;
                    runLength++;
                }

                const bool extendStart =
                    cornerWall(*wall, runStart->x - stepX, runStart->z - stepZ);
                const bool extendEnd = cornerWall(*wall, runEnd->x + stepX, runEnd->z + stepZ);
                const float extend = 0.5f + WALL_THICKNESS * 0.5f;

                const float baseY = static_cast<float>(wall->y);
                const float topY = baseY + static_cast<float>(WALL_HEIGHT);
                const Voxel voxel(wall->material);
                const VoxelTextureData &texture = VoxelTextureMapper::getTextureData(&voxel);
                const uint8_t tile = texture.sideTileIndex;
                const uint32_t tint = texture.sideColor;
                const uint8_t topTile = texture.topTileIndex;
                const uint32_t topTint = texture.topColor;

                for (int i = 0; i < runLength; i++) {
                    const int cellX = runStart->x + stepX * i;
                    const int cellZ = runStart->z + stepZ * i;
                    if (cellX < startX || cellX >= endX || cellZ < startZ || cellZ >= endZ) {
                        continue;
                    }
                    const ArchitectureObject *cellModule = wallAt(cellX, wall->y, cellZ);
                    if (cellModule == nullptr) { continue; }

                    const float cellU = static_cast<float>(wall->rotation == 0 ? cellX : cellZ);
                    float begin = cellU;
                    float finish = cellU + 1.f;
                    if (i == 0 && extendStart) { begin -= extend; }
                    if (i == runLength - 1 && extendEnd) { finish += extend; }

                    const float center =
                        (wall->rotation == 0 ? static_cast<float>(cellZ) : static_cast<float>(cellX)) + 0.5f;
                    const float sideA = center - WALL_THICKNESS * 0.5f;
                    const float sideB = center + WALL_THICKNESS * 0.5f;

                    // Все квады строятся в осях (along, y, side); rot1 отражает порядок
                    // вершин (свап осей меняет хиральность). Нормаль — тоже в (a, y, s).
                    const auto P = [&](float a, float yy, float s) {
                        return wall->rotation == 0 ? Vec3(a, yy, s) : Vec3(s, yy, a);
                    };
                    const auto emitQuad = [&](const Vec3 &p0, const Vec3 &p1, const Vec3 &p2,
                                              const Vec3 &p3, float na, float ny, float ns,
                                              uint8_t quadTile, uint32_t quadTint, float u0,
                                              float v0, float u1, float v1) {
                        const Vec3 normal = wall->rotation == 0 ? Vec3(na, ny, ns) : Vec3(ns, ny, na);
                        if (wall->rotation == 0) {
                            addWallQuad(vertices, indices, p0, p1, p2, p3, normal, quadTile,
                                        quadTint, u0, v0, u1, v1);
                        } else {
                            addWallQuad(vertices, indices, p0, p3, p2, p1, normal, quadTile,
                                        quadTint, u0, v0, u1, v1);
                        }
                    };

                    // Кусок полотна [u0..u1]×[y0..y1]: обе стороны, нарезка по метрам,
                    // мировой UV (полный тайл на метр — стыки метров и чанков бесшовны).
                    const auto emitBand = [&](float u0, float u1, float y0, float y1) {
                        if (u1 - u0 < 1e-4f || y1 - y0 < 1e-4f) { return; }
                        for (float u = u0; u < u1 - 1e-4f;) {
                            const float uNext = std::min(std::floor(u + 1e-4f) + 1.f, u1);
                            const float texU0 = u - std::floor(u + 1e-4f);
                            const float texU1 = texU0 + (uNext - u);
                            for (float yy = y0; yy < y1 - 1e-4f;) {
                                const float yNext = std::min(std::floor(yy + 1e-4f) + 1.f, y1);
                                const float metre = std::floor(yy + 1e-4f);
                                const float texV1 = 1.f - (yy - metre);
                                const float texV0 = 1.f - (yNext - metre);
                                emitQuad(
                                    P(u, yy, sideB), P(uNext, yy, sideB), P(uNext, yNext, sideB),
                                    P(u, yNext, sideB), 0.f, 0.f, 1.f, tile, tint, texU0, texV0,
                                    texU1, texV1
                                );
                                emitQuad(
                                    P(uNext, yy, sideA), P(u, yy, sideA), P(u, yNext, sideA),
                                    P(uNext, yNext, sideA), 0.f, 0.f, -1.f, tile, tint, texU0,
                                    texV0, texU1, texV1
                                );
                                yy = yNext;
                            }
                            u = uNext;
                        }
                    };

                    // Откосы проёма [u0..u1]×[y0..y1]: внутренние поверхности толщины стены.
                    const auto emitReveals = [&](float u0, float u1, float y0, float y1, bool withSill) {
                        if (withSill) { // подоконник (нормаль вверх)
                            emitQuad(
                                P(u0, y0, sideA), P(u0, y0, sideB), P(u1, y0, sideB),
                                P(u1, y0, sideA), 0.f, 1.f, 0.f, topTile, topTint, 0.f, 0.f,
                                u1 - u0, WALL_THICKNESS
                            );
                        }
                        emitQuad( // верх проёма (нормаль вниз)
                            P(u0, y1, sideA), P(u1, y1, sideA), P(u1, y1, sideB), P(u0, y1, sideB),
                            0.f, -1.f, 0.f, topTile, topTint, 0.f, 0.f, u1 - u0, WALL_THICKNESS
                        );
                        emitQuad( // левый откос (нормаль внутрь проёма, +a)
                            P(u0, y0, sideB), P(u0, y0, sideA), P(u0, y1, sideA), P(u0, y1, sideB),
                            1.f, 0.f, 0.f, tile, tint, 0.f, 0.f, WALL_THICKNESS, y1 - y0
                        );
                        emitQuad( // правый откос (нормаль внутрь проёма, -a)
                            P(u1, y0, sideA), P(u1, y0, sideB), P(u1, y1, sideB), P(u1, y1, sideA),
                            -1.f, 0.f, 0.f, tile, tint, 0.f, 0.f, WALL_THICKNESS, y1 - y0
                        );
                    };

                    // Брусок рамы: бокс в (a, y, s) без торцов (упираются в откосы/бруски).
                    const auto emitBar = [&](float a0, float a1, float y0, float y1, float s0, float s1) {
                        emitQuad( // сторона s1
                            P(a0, y0, s1), P(a1, y0, s1), P(a1, y1, s1), P(a0, y1, s1), 0.f, 0.f,
                            1.f, WALL_FRAME_TILE, 0xFFFFFFFF, 0.f, 0.f, 1.f, 1.f
                        );
                        emitQuad( // сторона s0
                            P(a1, y0, s0), P(a0, y0, s0), P(a0, y1, s0), P(a1, y1, s0), 0.f, 0.f,
                            -1.f, WALL_FRAME_TILE, 0xFFFFFFFF, 0.f, 0.f, 1.f, 1.f
                        );
                        emitQuad( // грань +a
                            P(a1, y0, s0), P(a1, y0, s1), P(a1, y1, s1), P(a1, y1, s0), 1.f, 0.f,
                            0.f, WALL_FRAME_TILE, 0xFFFFFFFF, 0.f, 0.f, 1.f, 1.f
                        );
                        emitQuad( // грань -a
                            P(a0, y0, s1), P(a0, y0, s0), P(a0, y1, s0), P(a0, y1, s1), -1.f, 0.f,
                            0.f, WALL_FRAME_TILE, 0xFFFFFFFF, 0.f, 0.f, 1.f, 1.f
                        );
                        emitQuad( // верх
                            P(a0, y1, s0), P(a0, y1, s1), P(a1, y1, s1), P(a1, y1, s0), 0.f, 1.f,
                            0.f, WALL_FRAME_TILE, 0xFFFFFFFF, 0.f, 0.f, 1.f, 1.f
                        );
                        emitQuad( // низ
                            P(a0, y0, s0), P(a1, y0, s0), P(a1, y0, s1), P(a0, y0, s1), 0.f, -1.f,
                            0.f, WALL_FRAME_TILE, 0xFFFFFFFF, 0.f, 0.f, 1.f, 1.f
                        );
                    };

                    switch (cellModule->type) {
                        case ArchObjectType::Wall:
                            emitBand(begin, finish, baseY, topY);
                            break;
                        case ArchObjectType::Window: {
                            const float sillY = baseY + WALL_SILL_HEIGHT;
                            const float headY = baseY + WALL_HEAD_HEIGHT;
                            const float u0 = cellU + WALL_WINDOW_JAMB;
                            const float u1 = cellU + 1.f - WALL_WINDOW_JAMB;
                            emitBand(begin, finish, baseY, sillY); // подоконная стенка
                            emitBand(begin, finish, headY, topY); // перемычка
                            emitBand(begin, u0, sillY, headY);    // простенок слева
                            emitBand(u1, finish, sillY, headY);   // простенок справа
                            emitReveals(u0, u1, sillY, headY, true);
                            // Рама-крест по центру толщины стены.
                            const float barHalf = WALL_FRAME_BAR * 0.5f;
                            const float uMid = (u0 + u1) * 0.5f;
                            const float yMid = (sillY + headY) * 0.5f;
                            emitBar(uMid - barHalf, uMid + barHalf, sillY, headY, center - barHalf, center + barHalf);
                            emitBar(u0, u1, yMid - barHalf, yMid + barHalf, center - barHalf, center + barHalf);
                            break;
                        }
                        case ArchObjectType::Door: {
                            const float headY = baseY + WALL_HEAD_HEIGHT;
                            const float u0 = cellU + WALL_DOOR_JAMB;
                            const float u1 = cellU + 1.f - WALL_DOOR_JAMB;
                            emitBand(begin, u0, baseY, headY);    // простенок слева
                            emitBand(u1, finish, baseY, headY);   // простенок справа
                            emitBand(begin, finish, headY, topY); // перемычка
                            emitReveals(u0, u1, baseY, headY, false); // порог — пол, без откоса
                            break;
                        }
                        default:
                            break;
                    }

                    // Верх/низ: полосы толщиной стены (пропуск при вертикальном продолжении).
                    const bool wallAbove =
                        wallAt(cellX, wall->y + WALL_HEIGHT, cellZ) != nullptr &&
                        wallAt(cellX, wall->y + WALL_HEIGHT, cellZ)->material == wall->material;
                    const bool wallBelow =
                        wallAt(cellX, wall->y - 1, cellZ) != nullptr &&
                        wallAt(cellX, wall->y - 1, cellZ)->material == wall->material;
                    for (float u = begin; u < finish - 1e-4f;) {
                        const float next = std::min(std::floor(u + 1e-4f) + 1.f, finish);
                        const float texU0 = u - std::floor(u + 1e-4f);
                        const float texU1 = texU0 + (next - u);
                        if (!wallAbove) {
                            emitQuad(
                                P(u, topY, sideA), P(u, topY, sideB), P(next, topY, sideB),
                                P(next, topY, sideA), 0.f, 1.f, 0.f, topTile, topTint, texU0, 0.f,
                                texU1, WALL_THICKNESS
                            );
                        }
                        if (!wallBelow) {
                            emitQuad(
                                P(u, baseY, sideA), P(next, baseY, sideA), P(next, baseY, sideB),
                                P(u, baseY, sideB), 0.f, -1.f, 0.f, topTile, topTint, texU0, 0.f,
                                texU1, WALL_THICKNESS
                            );
                        }
                        u = next;
                    }

                    // Торцы — только на непродлённых концах рана.
                    const auto addCap = [&](float at, bool negative) {
                        const float capNear = negative ? sideA : sideB;
                        const float capFar = negative ? sideB : sideA;
                        emitQuad(
                            P(at, baseY, capNear), P(at, baseY, capFar), P(at, topY, capFar),
                            P(at, topY, capNear), negative ? -1.f : 1.f, 0.f, 0.f, tile, tint, 0.f,
                            0.f, WALL_THICKNESS, 1.f
                        );
                    };
                    if (i == 0 && !extendStart) { addCap(begin, true); }
                    if (i == runLength - 1 && !extendEnd) { addCap(finish, false); }
                }
            }
        }
    }
}

std::optional<BuildingCellGrid::RaycastHit>
BuildingCellGrid::raycast(Vec3 origin, Vec3 direction, float maxDistance) const {
    // Воксельный DDA (Amanatides & Woo) по кубам построек.
    const float length = std::sqrt(
        direction.x * direction.x + direction.y * direction.y + direction.z * direction.z
    );
    if (length < 1e-6f) { return std::nullopt; }
    const float dirX = direction.x / length;
    const float dirY = direction.y / length;
    const float dirZ = direction.z / length;

    int x = static_cast<int>(std::floor(origin.x));
    int y = static_cast<int>(std::floor(origin.y));
    int z = static_cast<int>(std::floor(origin.z));
    const int stepX = dirX > 0 ? 1 : -1;
    const int stepY = dirY > 0 ? 1 : -1;
    const int stepZ = dirZ > 0 ? 1 : -1;
    auto boundaryDistance = [](float originValue, float dir, int voxel, int step) {
        if (std::abs(dir) < 1e-8f) { return 1e30f; }
        const float boundary = step > 0 ? voxel + 1.f : static_cast<float>(voxel);
        return (boundary - originValue) / dir;
    };
    float tMaxX = boundaryDistance(origin.x, dirX, x, stepX);
    float tMaxY = boundaryDistance(origin.y, dirY, y, stepY);
    float tMaxZ = boundaryDistance(origin.z, dirZ, z, stepZ);
    const float tDeltaX = std::abs(dirX) < 1e-8f ? 1e30f : std::abs(1.f / dirX);
    const float tDeltaY = std::abs(dirY) < 1e-8f ? 1e30f : std::abs(1.f / dirY);
    const float tDeltaZ = std::abs(dirZ) < 1e-8f ? 1e30f : std::abs(1.f / dirZ);

    int normalX = 0, normalY = 0, normalZ = 0;
    float distance = 0.f;
    while (distance <= maxDistance) {
        if (isSolidAt(x, y, z)) {
            return RaycastHit{x, y, z, normalX, normalY, normalZ, distance};
        }
        if (tMaxX <= tMaxY && tMaxX <= tMaxZ) {
            distance = tMaxX;
            tMaxX += tDeltaX;
            x += stepX;
            normalX = -stepX;
            normalY = 0;
            normalZ = 0;
        } else if (tMaxY <= tMaxZ) {
            distance = tMaxY;
            tMaxY += tDeltaY;
            y += stepY;
            normalX = 0;
            normalY = -stepY;
            normalZ = 0;
        } else {
            distance = tMaxZ;
            tMaxZ += tDeltaZ;
            z += stepZ;
            normalX = 0;
            normalY = 0;
            normalZ = -stepZ;
        }
    }
    return std::nullopt;
}

void BuildingCellGrid::collectObjects(std::vector<ArchitectureObject> &outObjects) const {
    outObjects.clear();
    outObjects.reserve(m_objects.size());
    for (const auto &[id, object] : m_objects) {
        outObjects.push_back(object);
    }
}

void BuildingCellGrid::restoreObjects(const std::vector<ArchitectureObject> &objects) {
    for (const ArchitectureObject &object : objects) {
        placeInternal(object, false); // id пересоздаются, занятые/вне-границ пропускаются
    }
    rebuildDirtyChunks();
}

void BuildingCellGrid::restoreLegacyBlocks(const std::vector<std::pair<uint64_t, uint8_t>> &blocks) {
    for (const auto &[key, value] : blocks) {
        ArchitectureObject object;
        object.type = ArchObjectType::Block;
        object.x = static_cast<int>((key >> 42) & 0x1FFFFF) - (1 << 20);
        object.y = static_cast<int>((key >> 21) & 0x1FFFFF) - (1 << 20);
        object.z = static_cast<int>(key & 0x1FFFFF) - (1 << 20);
        object.material = static_cast<VoxelType>(value);
        placeInternal(object, false);
    }
    rebuildDirtyChunks();
}
