#include "BuildingCellGrid.hpp"
#include "ExtrudedProfile.hpp"
#include "LightGrid.hpp"
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

// Упаковка воксельного света в vertex.light: старший байт — солнце, младший — источники
// (шейдер blocks_lit распаковывает и гасит солнечный канал полем daylight). Каналы уже
// умножены на faceLight; АО едет отдельно (альфа тинта), поэтому интерполяция пака
// по грани безопасна — он константен.
float packChannels(float sunEffective, float blockEffective) {
    return std::floor(std::clamp(sunEffective, 0.f, 1.f) * 255.f) * 256.f +
           std::floor(std::clamp(blockEffective, 0.f, 1.f) * 255.f);
}

float packCellLight(const LightGrid *grid, int x, int y, int z, float faceLight) {
    float sun = 1.f;
    float block = 0.f;
    if (grid != nullptr && grid->isReady()) {
        sun = grid->sunAt(x, y, z) / 15.f;
        block = grid->blockAt(x, y, z) / 15.f;
    }
    return packChannels(sun * faceLight, block * faceLight);
}

// Свет грани куба: из ячейки перед гранью; если она в толще тонкого элемента (стены) —
// максимум по 4 соседям в плоскости грани (иначе кубы у стен всегда чёрные).
// Пер-блочная вариативность тона: лёгкий джиттер яркости по хешу координат ломает
// «обои» на больших плоскостях одного материала (кладка из чуть разных камней).
float blockToneVariation(int x, int y, int z) {
    uint32_t h = 374761393u * static_cast<uint32_t>(x) + 668265263u * static_cast<uint32_t>(y) +
                 974634617u * static_cast<uint32_t>(z);
    h = (h ^ (h >> 13)) * 1274126177u;
    return 0.93f + static_cast<float>((h >> 16) & 0xFF) / 255.f * 0.07f;
}

float packFaceCellLight(
    const LightGrid *grid, int x, int y, int z, int normalX, int normalY, int normalZ,
    float faceLight
) {
    if (grid == nullptr || !grid->isReady()) { return packChannels(faceLight, 0.f); }
    float sun = grid->sunAt(x, y, z) / 15.f;
    float block = grid->blockAt(x, y, z) / 15.f;
    if (sun <= 0.f && block <= 0.f) {
        // Соседи в плоскости грани: касательные — две оси, перпендикулярные нормали.
        int offsets[4][3] = {};
        int count = 0;
        if (normalX == 0) {
            offsets[count][0] = 1;
            offsets[count + 1][0] = -1;
            count += 2;
        }
        if (normalY == 0) {
            offsets[count][1] = 1;
            offsets[count + 1][1] = -1;
            count += 2;
        }
        if (normalZ == 0 && count < 4) {
            offsets[count][2] = 1;
            offsets[count + 1][2] = -1;
            count += 2;
        }
        for (int i = 0; i < count; i++) {
            sun = std::max(sun, grid->sunAt(x + offsets[i][0], y + offsets[i][1], z + offsets[i][2]) / 15.f);
            block = std::max(
                block, grid->blockAt(x + offsets[i][0], y + offsets[i][1], z + offsets[i][2]) / 15.f
            );
        }
    }
    return packChannels(sun * faceLight, block * faceLight);
}

// Свет сегмента полотна: максимум каналов по боковой ячейке и её соседям вдоль рана —
// у перпендикулярного примыкания боковая ячейка лежит в толще стены (свет 0), иначе
// углы коробки всегда чёрные.
float packBestCellLight(
    const LightGrid *grid, int x, int y, int z, int alongStepX, int alongStepZ, float faceLight
) {
    float sun = 1.f;
    float block = 0.f;
    if (grid != nullptr && grid->isReady()) {
        sun = 0.f;
        for (int offset = -1; offset <= 1; offset++) {
            sun = std::max(sun, grid->sunAt(x + alongStepX * offset, y, z + alongStepZ * offset) / 15.f);
            block = std::max(
                block, grid->blockAt(x + alongStepX * offset, y, z + alongStepZ * offset) / 15.f
            );
        }
    }
    return packChannels(sun * faceLight, block * faceLight);
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
    int minX, int minY, int minZ, int sizeX, int sizeY, int sizeZ, MaterialHandle material,
    MaterialHandle roofMaterial
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
    m_roofMaterial = roofMaterial;
}

void BuildingCellGrid::shutdown() {
    for (ChunkView &view : m_views) {
        if (view.mesh.isValid()) { AssetManagerAPI::deleteMesh(view.mesh); }
        if (view.entity.isValid()) { WorldAPI::destroyEntity(view.entity); }
        if (view.roofMesh.isValid()) { AssetManagerAPI::deleteMesh(view.roofMesh); }
        if (view.roofEntity.isValid()) { WorldAPI::destroyEntity(view.roofEntity); }
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
        case ArchObjectType::Cornice:
        case ArchObjectType::Roof:
        case ArchObjectType::Lamp:
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
    // Карниз и лампа — декор: не коллайдят (габарит много меньше ячейки).
    if (object != nullptr &&
        (object->type == ArchObjectType::Cornice || object->type == ArchObjectType::Lamp)) {
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

void BuildingCellGrid::markLightDirtyBox(int fromX, int fromY, int fromZ, int toX, int toY, int toZ) {
    for (int y = std::max(fromY, m_minY); y < std::min(toY, m_minY + m_sizeY); y += CHUNK) {
        for (int z = std::max(fromZ, m_minZ); z < std::min(toZ, m_minZ + m_sizeZ); z += CHUNK) {
            for (int x = std::max(fromX, m_minX); x < std::min(toX, m_minX + m_sizeX); x += CHUNK) {
                markDirtyAround(x, y, z);
            }
        }
    }
    // Крайние ячейки бокса (шаг CHUNK может перескочить край).
    markDirtyAround(toX - 1, toY - 1, toZ - 1);
    rebuildDirtyChunks();
}

// Свет: пересчёт вокруг изменённых ячеек объекта + ремеш чанков изменившегося света.
void BuildingCellGrid::applyLightForObjectChange(const ArchitectureObject &object) {
    if (m_lightGrid == nullptr || !m_lightGrid->isReady()) { return; }
    std::vector<std::array<int, 3>> cells;
    cellsFor(object, cells);
    std::vector<LightGrid::Edit> edits;
    edits.reserve(cells.size());
    for (const auto &cell : cells) {
        edits.push_back({cell[0], cell[1], cell[2]});
    }
    const LightGrid::ChangedBox changed = m_lightGrid->recomputeAround(*this, edits);
    if (changed.any) {
        markLightDirtyBox(
            changed.fromX, changed.fromY, changed.fromZ, changed.toX, changed.toY, changed.toZ
        );
    }
}

uint32_t BuildingCellGrid::placeInternal(ArchitectureObject object, bool rebuild) {
    if (!canPlace(object)) { return 0; }
    object.id = m_nextObjectId++;
    writeObjectCells(object, false);
    m_objects.emplace(object.id, object);
    if (object.type == ArchObjectType::Roof) { markRoofRegionDirty(object); }
    if (rebuild) { applyLightForObjectChange(object); }
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
    const ArchitectureObject removed = objectIt->second;
    writeObjectCells(removed, true);
    m_objects.erase(objectIt);
    applyLightForObjectChange(removed);
    if (removed.type == ArchObjectType::Roof) {
        // Область уже без удалённой ячейки: дирти́м соседей-крыши (их полосы пересчитаются).
        constexpr int NEIGHBORS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto &offset : NEIGHBORS) {
            const ArchitectureObject *neighbor =
                objectAt(removed.x + offset[0], removed.y, removed.z + offset[1]);
            if (neighbor != nullptr && neighbor->type == ArchObjectType::Roof) {
                markRoofRegionDirty(*neighbor);
                break;
            }
        }
    }
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
                if (const ArchitectureObject *lamp = objectAt(x, y, z);
                    lamp != nullptr && lamp->type == ArchObjectType::Lamp) {
                    appendLampCube(x, y, z, vertices, indices);
                    continue;
                }
                if (!isFullCellAt(x, y, z)) { continue; } // тонкие элементы рисуют свои проходы
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
                    // Свет грани — из ячейки перед ней; АО — в альфу тинта (пер-вершинно).
                    const float packedLight = packFaceCellLight(
                        m_lightGrid, x + face.normal[0], y + face.normal[1], z + face.normal[2],
                        face.normal[0], face.normal[1], face.normal[2], face.light
                    );
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
                        Color vertexColor = hexColor(tint);
                        const float tone = blockToneVariation(x, y, z);
                        vertexColor.r *= tone;
                        vertexColor.g *= tone;
                        vertexColor.b *= tone;
                        vertexColor.a *= 1.f - occlusion;
                        vertices.emplace_back(Vertex(position, uv, normal, vertexColor, packedLight));
                    }
                }
            }
        }
    }

    std::vector<Vertex> roofVertices;
    std::vector<uint32_t> roofIndices;
    appendWallGeometry(startX, startY, startZ, endX, endY, endZ, vertices, indices);
    appendCorniceGeometry(startX, startY, startZ, endX, endY, endZ, vertices, indices);
    appendRoofGeometry(
        startX, startY, startZ, endX, endY, endZ, vertices, indices, roofVertices, roofIndices
    );

    const auto applyMesh = [&](std::vector<Vertex> &&meshVertices, std::vector<uint32_t> &&meshIndices,
                               MeshHandle &mesh, EntityHandle &entity, MaterialHandle material,
                               const char *namePrefix) {
        if (meshVertices.empty()) {
            if (mesh.isValid()) {
                AssetManagerAPI::deleteMesh(mesh);
                mesh = {};
            }
            if (entity.isValid()) {
                WorldAPI::destroyEntity(entity);
                entity = {};
            }
            return;
        }
        if (!mesh.isValid()) { mesh = AssetManagerAPI::createMesh(); }
        MeshData meshData;
        meshData.vertices = std::move(meshVertices);
        meshData.indices = std::move(meshIndices);
        MeshAPI::update(mesh, meshData);
        if (!entity.isValid()) {
            const std::string name = std::string(namePrefix) + " " + std::to_string(chunkX) + "," +
                                     std::to_string(chunkY) + "," + std::to_string(chunkZ);
            entity = WorldAPI::createEntity(name.c_str());
            EntityAPI::addComponent(entity, ComponentType::MESH_COMPONENT);
            MeshComponentAPI::setMesh(entity, mesh);
            MeshComponentAPI::setMaterial(entity, material);
            TransformComponentAPI::setPosition(entity, {0.f, 0.f, 0.f});
        }
    };
    applyMesh(std::move(vertices), std::move(indices), view.mesh, view.entity, m_material, "Buildings");
    applyMesh(
        std::move(roofVertices), std::move(roofIndices), view.roofMesh, view.roofEntity,
        m_roofMaterial.isValid() ? m_roofMaterial : m_material, "Roofs"
    );
}

namespace {

// Фонарик: светящийся столбик по центру ячейки (эмиссив — полный канал источников,
// ночь его не гасит).
void appendLampCubeQuads(
    float centerX, float baseY, float centerZ, std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices
) {
    constexpr float HALF = 0.14f;
    constexpr float HEIGHT = 0.7f;
    constexpr uint8_t LAMP_TILE = 0;      // светлый тайл атласа
    constexpr uint32_t LAMP_TINT = 0xFFE2A8FF; // тёплое свечение
    const float emissive = 255.f;         // pack(0, 1): только канал источников
    const float x0 = centerX - HALF, x1 = centerX + HALF;
    const float z0 = centerZ - HALF, z1 = centerZ + HALF;
    const float y0 = baseY, y1 = baseY + HEIGHT;
    const auto quad = [&](const Vec3 &p0, const Vec3 &p1, const Vec3 &p2, const Vec3 &p3,
                          const Vec3 &normal) {
        const Vec2 uv(0.02f, 0.02f);
        const uint32_t base = static_cast<uint32_t>(vertices.size());
        for (uint32_t index : {base, base + 1u, base + 2u, base + 2u, base + 3u, base}) {
            indices.emplace_back(index);
        }
        (void)LAMP_TILE;
        for (const Vec3 &point : {p0, p1, p2, p3}) {
            vertices.emplace_back(Vertex(point, uv, normal, hexColor(LAMP_TINT), emissive));
        }
    };
    quad({x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, {0.f, 0.f, 1.f});
    quad({x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, {0.f, 0.f, -1.f});
    quad({x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {1.f, 0.f, 0.f});
    quad({x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, {-1.f, 0.f, 0.f});
    quad({x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0}, {0.f, 1.f, 0.f});
    quad({x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, {0.f, -1.f, 0.f});
}

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

// Накладные детали фасада: наличник окна (планки вокруг проёма + подоконная доска +
// сандрик-полочка) и каменный цоколь низа стены. Тайлы — из общего blocks-атласа.
constexpr uint8_t WALL_CASING_TILE = 0;  // светлая штукатурка
constexpr float WALL_CASING_WIDTH = 0.12f;
constexpr float WALL_CASING_DEPTH = 0.045f;
constexpr uint8_t WALL_PLINTH_TILE = 44; // тёмный камень
constexpr float WALL_PLINTH_HEIGHT = 0.55f;
constexpr float WALL_PLINTH_DEPTH = 0.06f;

// Фахверк (стиль стены 1): тёмный деревянный каркас поверх полотна — стойки по границам
// ячеек, обвязки, ригель, случайные раскосы. Брус — накладка на обеих сторонах полотна.
constexpr float WALL_TIMBER_WIDTH = 0.11f;
constexpr float WALL_TIMBER_DEPTH = 0.035f;
constexpr uint32_t WALL_TIMBER_TINT = 0xB2A392FF; // приглушение светлого тайла дерева

// Свет граней — те же значения, что у кубов (FACES), но без AO: полотно едино.
float wallFaceLight(int normalX, int normalY, int normalZ) {
    if (normalZ > 0) { return 1.0f; }
    if (normalZ < 0) { return 0.75f; }
    if (normalY > 0) { return 0.95f; }
    if (normalY < 0) { return 0.85f; }
    if (normalX < 0) { return 0.9f; }
    return 0.8f;
}

// Квад стены: p0..p3 против часовой снаружи; UV — доли атласного тайла (v вниз, как у
// кубов). packedLight < 0 — «на открытом солнце» (полный sun-канал × faceLight).
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
    float v1,
    float packedLight = -1.f
) {
    const Vec2 uvBase = tileUV(tileIndex);
    const float uvSize = 1.f / BLOCKS_ATLAS_GRID - 0.001f;
    const float faceLight = wallFaceLight(
        static_cast<int>(normal.x), static_cast<int>(normal.y), static_cast<int>(normal.z)
    );
    const float light = packedLight >= 0.f ? packedLight : packChannels(faceLight, 0.f);
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

void BuildingCellGrid::appendLampCube(
    int x, int y, int z, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices
) const {
    appendLampCubeQuads(x + 0.5f, static_cast<float>(y), z + 0.5f, vertices, indices);
}

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
            wall->material != sample.material || wall->y != sample.y ||
            wall->params[0] != sample.params[0]) { // толщина различает полотна
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

                // Толщина полотна — пресет стены (0.2 лёгкая, 0.3 стандарт, 0.5 крепостная).
                const float wallThickness =
                    wall->params[0] == 1 ? 0.2f : wall->params[0] == 2 ? 0.5f : WALL_THICKNESS;
                const bool extendStart =
                    cornerWall(*wall, runStart->x - stepX, runStart->z - stepZ);
                const bool extendEnd = cornerWall(*wall, runEnd->x + stepX, runEnd->z + stepZ);
                const float extend = 0.5f + wallThickness * 0.5f;

                // Обратная сторона угла: перпендикуляр СБОКУ торцевой ячейки продлевает
                // своё полотно в неё — наш торец укорачивается до ближней грани этого
                // полотна и не рисуется (иначе хвост торчит перед перпендикуляром).
                const auto sideCorner = [&](const ArchitectureObject *endCell) {
                    return cornerWall(*wall, endCell->x + stepZ, endCell->z + stepX) ||
                           cornerWall(*wall, endCell->x - stepZ, endCell->z - stepX);
                };
                const bool retractStart = !extendStart && sideCorner(runStart);
                const bool retractEnd = !extendEnd && sideCorner(runEnd);
                const float retract = 0.5f - wallThickness * 0.5f;

                const float baseY = static_cast<float>(wall->y);
                const float topY = baseY + static_cast<float>(WALL_HEIGHT);
                const Voxel voxel(wall->material);
                const VoxelTextureData &texture = VoxelTextureMapper::getTextureData(&voxel);
                const uint8_t tile = texture.sideTileIndex;
                const uint32_t tint = texture.sideColor;
                const uint8_t topTile = texture.topTileIndex;
                const uint32_t topTint = texture.topColor;

                // Цоколь рана — настройка первой Wall-ячейки (тянется под окна и двери).
                bool runPlinth = false;
                for (int i = 0; i < runLength; i++) {
                    const ArchitectureObject *module =
                        wallAt(runStart->x + stepX * i, wall->y, runStart->z + stepZ * i);
                    if (module != nullptr && module->type == ArchObjectType::Wall) {
                        runPlinth = module->params[1] == 0; // 0 = каменный цоколь (дефолт)
                        break;
                    }
                }

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
                    if (i == 0 && retractStart) { begin += retract; }
                    if (i == runLength - 1 && retractEnd) { finish -= retract; }

                    const float center =
                        (wall->rotation == 0 ? static_cast<float>(cellZ) : static_cast<float>(cellX)) + 0.5f;
                    const float sideA = center - wallThickness * 0.5f;
                    const float sideB = center + wallThickness * 0.5f;

                    // Все квады строятся в осях (along, y, side); rot1 отражает порядок
                    // вершин (свап осей меняет хиральность). Нормаль — тоже в (a, y, s).
                    const auto P = [&](float a, float yy, float s) {
                        return wall->rotation == 0 ? Vec3(a, yy, s) : Vec3(s, yy, a);
                    };
                    const auto emitQuadLit = [&](const Vec3 &p0, const Vec3 &p1, const Vec3 &p2,
                                                 const Vec3 &p3, float na, float ny, float ns,
                                                 uint8_t quadTile, uint32_t quadTint, float u0,
                                                 float v0, float u1, float v1, float packedLight) {
                        const Vec3 normal = wall->rotation == 0 ? Vec3(na, ny, ns) : Vec3(ns, ny, na);
                        if (wall->rotation == 0) {
                            addWallQuad(vertices, indices, p0, p1, p2, p3, normal, quadTile,
                                        quadTint, u0, v0, u1, v1, packedLight);
                        } else {
                            addWallQuad(vertices, indices, p0, p3, p2, p1, normal, quadTile,
                                        quadTint, u0, v0, u1, v1, packedLight);
                        }
                    };
                    const auto emitQuad = [&](const Vec3 &p0, const Vec3 &p1, const Vec3 &p2,
                                              const Vec3 &p3, float na, float ny, float ns,
                                              uint8_t quadTile, uint32_t quadTint, float u0,
                                              float v0, float u1, float v1) {
                        emitQuadLit(p0, p1, p2, p3, na, ny, ns, quadTile, quadTint, u0, v0, u1, v1, -1.f);
                    };

                    // Ячейки света по сторонам полотна (мировые оси); свет per-уровнево.
                    const int lightCellBX = wall->rotation == 0 ? cellX : cellX + 1;
                    const int lightCellBZ = wall->rotation == 0 ? cellZ + 1 : cellZ;
                    const int lightCellAX = wall->rotation == 0 ? cellX : cellX - 1;
                    const int lightCellAZ = wall->rotation == 0 ? cellZ - 1 : cellZ;
                    const float faceLightB = wall->rotation == 0 ? 1.0f : 0.8f;
                    const float faceLightA = wall->rotation == 0 ? 0.75f : 0.9f;
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
                                const int lightY = static_cast<int>(metre);
                                const float packedB = packBestCellLight(
                                    m_lightGrid, lightCellBX, lightY, lightCellBZ, stepX, stepZ,
                                    faceLightB
                                );
                                const float packedA = packBestCellLight(
                                    m_lightGrid, lightCellAX, lightY, lightCellAZ, stepX, stepZ,
                                    faceLightA
                                );
                                emitQuadLit(
                                    P(u, yy, sideB), P(uNext, yy, sideB), P(uNext, yNext, sideB),
                                    P(u, yNext, sideB), 0.f, 0.f, 1.f, tile, tint, texU0, texV0,
                                    texU1, texV1, packedB
                                );
                                emitQuadLit(
                                    P(uNext, yy, sideA), P(u, yy, sideA), P(u, yNext, sideA),
                                    P(uNext, yNext, sideA), 0.f, 0.f, -1.f, tile, tint, texU0,
                                    texV0, texU1, texV1, packedA
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
                                u1 - u0, wallThickness
                            );
                        }
                        emitQuad( // верх проёма (нормаль вниз)
                            P(u0, y1, sideA), P(u1, y1, sideA), P(u1, y1, sideB), P(u0, y1, sideB),
                            0.f, -1.f, 0.f, topTile, topTint, 0.f, 0.f, u1 - u0, wallThickness
                        );
                        emitQuad( // левый откос (нормаль внутрь проёма, +a)
                            P(u0, y0, sideB), P(u0, y0, sideA), P(u0, y1, sideA), P(u0, y1, sideB),
                            1.f, 0.f, 0.f, tile, tint, 0.f, 0.f, wallThickness, y1 - y0
                        );
                        emitQuad( // правый откос (нормаль внутрь проёма, -a)
                            P(u1, y0, sideA), P(u1, y0, sideB), P(u1, y1, sideB), P(u1, y1, sideA),
                            -1.f, 0.f, 0.f, tile, tint, 0.f, 0.f, wallThickness, y1 - y0
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

                    // Бокс в (a, y, s): 6 граней, UV пропорционален размеру грани.
                    // Грани, легшие в плоскость полотна, прячет backface culling.
                    const auto emitBox = [&](float a0, float a1, float y0, float y1, float s0,
                                             float s1, uint8_t boxTile, uint32_t boxTint) {
                        emitQuad(
                            P(a0, y0, s1), P(a1, y0, s1), P(a1, y1, s1), P(a0, y1, s1), 0.f, 0.f,
                            1.f, boxTile, boxTint, 0.f, 0.f, a1 - a0, y1 - y0
                        );
                        emitQuad(
                            P(a1, y0, s0), P(a0, y0, s0), P(a0, y1, s0), P(a1, y1, s0), 0.f, 0.f,
                            -1.f, boxTile, boxTint, 0.f, 0.f, a1 - a0, y1 - y0
                        );
                        emitQuad(
                            P(a1, y0, s0), P(a1, y0, s1), P(a1, y1, s1), P(a1, y1, s0), 1.f, 0.f,
                            0.f, boxTile, boxTint, 0.f, 0.f, s1 - s0, y1 - y0
                        );
                        emitQuad(
                            P(a0, y0, s1), P(a0, y0, s0), P(a0, y1, s0), P(a0, y1, s1), -1.f, 0.f,
                            0.f, boxTile, boxTint, 0.f, 0.f, s1 - s0, y1 - y0
                        );
                        emitQuad(
                            P(a0, y1, s0), P(a0, y1, s1), P(a1, y1, s1), P(a1, y1, s0), 0.f, 1.f,
                            0.f, boxTile, boxTint, 0.f, 0.f, a1 - a0, s1 - s0
                        );
                        emitQuad(
                            P(a0, y0, s0), P(a1, y0, s0), P(a1, y0, s1), P(a0, y0, s1), 0.f, -1.f,
                            0.f, boxTile, boxTint, 0.f, 0.f, a1 - a0, s1 - s0
                        );
                    };

                    // Вертикальное продолжение рана: сосед сверху/снизу того же материала.
                    const bool wallAbove =
                        wallAt(cellX, wall->y + WALL_HEIGHT, cellZ) != nullptr &&
                        wallAt(cellX, wall->y + WALL_HEIGHT, cellZ)->material == wall->material;
                    const bool wallBelow =
                        wallAt(cellX, wall->y - 1, cellZ) != nullptr &&
                        wallAt(cellX, wall->y - 1, cellZ)->material == wall->material;

                    // Цоколь: накладные плиты обеих сторон полотна (только нижний этаж рана).
                    const auto emitPlinth = [&](float a0, float a1) {
                        if (!runPlinth || wallBelow || a1 - a0 < 1e-4f) { return; }
                        emitBox(
                            a0, a1, baseY, baseY + WALL_PLINTH_HEIGHT, sideB,
                            sideB + WALL_PLINTH_DEPTH, WALL_PLINTH_TILE, 0xFFFFFFFF
                        );
                        emitBox(
                            a0, a1, baseY, baseY + WALL_PLINTH_HEIGHT,
                            sideA - WALL_PLINTH_DEPTH, sideA, WALL_PLINTH_TILE, 0xFFFFFFFF
                        );
                    };

                    // Наклонный брус-накладка в плоскости полотна: от (a0,y0) к (a1,y1),
                    // сечение WALL_TIMBER_WIDTH; торцы упираются в стойки/обвязки.
                    const auto emitDiagonal = [&](float a0, float y0, float a1, float y1,
                                                  float s0, float s1) {
                        const float da = a1 - a0;
                        const float dy = y1 - y0;
                        const float length = std::sqrt(da * da + dy * dy);
                        if (length < 1e-3f) { return; }
                        const float px = -dy / length * (WALL_TIMBER_WIDTH * 0.5f);
                        const float py = da / length * (WALL_TIMBER_WIDTH * 0.5f);
                        const float nx = px / (WALL_TIMBER_WIDTH * 0.5f);
                        const float ny = py / (WALL_TIMBER_WIDTH * 0.5f);
                        emitQuad( // плоскость +s
                            P(a0 - px, y0 - py, s1), P(a1 - px, y1 - py, s1),
                            P(a1 + px, y1 + py, s1), P(a0 + px, y0 + py, s1), 0.f, 0.f, 1.f,
                            WALL_FRAME_TILE, WALL_TIMBER_TINT, 0.f, 0.f, length, WALL_TIMBER_WIDTH
                        );
                        emitQuad( // плоскость -s
                            P(a1 - px, y1 - py, s0), P(a0 - px, y0 - py, s0),
                            P(a0 + px, y0 + py, s0), P(a1 + px, y1 + py, s0), 0.f, 0.f, -1.f,
                            WALL_FRAME_TILE, WALL_TIMBER_TINT, 0.f, 0.f, length, WALL_TIMBER_WIDTH
                        );
                        emitQuad( // верхняя кромка
                            P(a0 + px, y0 + py, s0), P(a0 + px, y0 + py, s1),
                            P(a1 + px, y1 + py, s1), P(a1 + px, y1 + py, s0), nx, ny, 0.f,
                            WALL_FRAME_TILE, WALL_TIMBER_TINT, 0.f, 0.f, length, s1 - s0
                        );
                        emitQuad( // нижняя кромка
                            P(a0 - px, y0 - py, s0), P(a1 - px, y1 - py, s0),
                            P(a1 - px, y1 - py, s1), P(a0 - px, y0 - py, s1), -nx, -ny, 0.f,
                            WALL_FRAME_TILE, WALL_TIMBER_TINT, 0.f, 0.f, length, s1 - s0
                        );
                    };

                    // Фахверк: каркас этой ячейки. Стойка на левой границе (общая с соседом),
                    // правая — только на конце цепочки; обвязки и ригель между стойками;
                    // раскосы — в половине панелей, направление по хешу мировых координат.
                    const auto emitTimberFrame = [&]() {
                        const float bw = WALL_TIMBER_WIDTH;
                        const float lowY =
                            (runPlinth && !wallBelow) ? baseY + WALL_PLINTH_HEIGHT : baseY;
                        const float midY = baseY + 1.5f;
                        const float sides[2][2] = {
                            {sideB, sideB + WALL_TIMBER_DEPTH},
                            {sideA - WALL_TIMBER_DEPTH, sideA}
                        };
                        const auto timberNeighbor = [&](int nx, int nz) {
                            const ArchitectureObject *neighbor = wallAt(nx, wall->y, nz);
                            return neighbor != nullptr && neighbor->type == ArchObjectType::Wall &&
                                   neighbor->rotation == wall->rotation &&
                                   neighbor->params[2] == 1;
                        };
                        const bool timberRight = timberNeighbor(cellX + stepX, cellZ + stepZ);
                        const float a0 = cellU + bw;
                        const float a1 = cellU + 1.f - (timberRight ? 0.f : bw);
                        for (const auto &side : sides) {
                            emitBox(
                                cellU, cellU + bw, lowY, topY, side[0], side[1], WALL_FRAME_TILE,
                                WALL_TIMBER_TINT
                            );
                            if (!timberRight) {
                                emitBox(
                                    cellU + 1.f - bw, cellU + 1.f, lowY, topY, side[0], side[1],
                                    WALL_FRAME_TILE, WALL_TIMBER_TINT
                                );
                            }
                            emitBox(
                                a0, a1, lowY, lowY + bw, side[0], side[1], WALL_FRAME_TILE,
                                WALL_TIMBER_TINT
                            );
                            emitBox(
                                a0, a1, topY - bw, topY, side[0], side[1], WALL_FRAME_TILE,
                                WALL_TIMBER_TINT
                            );
                            emitBox(
                                a0, a1, midY - bw * 0.5f, midY + bw * 0.5f, side[0], side[1],
                                WALL_FRAME_TILE, WALL_TIMBER_TINT
                            );
                        }
                        const float panels[2][2] = {
                            {lowY + bw, midY - bw * 0.5f}, {midY + bw * 0.5f, topY - bw}
                        };
                        const float d0 = cellU + bw;
                        const float d1 = cellU + 1.f - bw;
                        for (int panel = 0; panel < 2; panel++) {
                            uint32_t h = 2246822519u * static_cast<uint32_t>(cellX) +
                                         3266489917u *
                                             static_cast<uint32_t>(wall->y * 2 + panel) +
                                         668265263u * static_cast<uint32_t>(cellZ);
                            h = (h ^ (h >> 15)) * 2654435761u;
                            const uint32_t kind = (h >> 8) & 3u; // 0 «/», 1 «\», 2-3 — без
                            if (kind > 1) { continue; }
                            const float y0 = panels[panel][0];
                            const float y1 = panels[panel][1];
                            if (y1 - y0 < 0.4f) { continue; }
                            for (const auto &side : sides) {
                                if (kind == 0) {
                                    emitDiagonal(d0, y0, d1, y1, side[0], side[1]);
                                } else {
                                    emitDiagonal(d0, y1, d1, y0, side[0], side[1]);
                                }
                            }
                        }
                    };

                    switch (cellModule->type) {
                        case ArchObjectType::Wall: {
                            emitBand(begin, finish, baseY, topY);
                            if (cellModule->params[2] == 1) { emitTimberFrame(); }
                            // Угловое продление закрывает стык цоколей перпендикулярных стен.
                            float plinth0 = begin;
                            float plinth1 = finish;
                            if (i == 0 && extendStart) { plinth0 -= WALL_PLINTH_DEPTH; }
                            if (i == runLength - 1 && extendEnd) { plinth1 += WALL_PLINTH_DEPTH; }
                            emitPlinth(plinth0, plinth1);
                            break;
                        }
                        case ArchObjectType::Window: {
                            // Пресеты модуля: ширина проёма, высота подоконника, раскладка рамы.
                            const float jamb = cellModule->params[0] == 1   ? 0.25f
                                               : cellModule->params[0] == 2 ? 0.08f
                                                                            : WALL_WINDOW_JAMB;
                            const float sillHeight = cellModule->params[1] == 1   ? 0.6f
                                                     : cellModule->params[1] == 2 ? 1.2f
                                                                                  : WALL_SILL_HEIGHT;
                            const uint8_t frameKind = cellModule->params[2]; // 0 крест, 1 без, 2 вертикаль
                            const float sillY = baseY + sillHeight;
                            const float headY = baseY + WALL_HEAD_HEIGHT;
                            const float u0 = cellU + jamb;
                            const float u1 = cellU + 1.f - jamb;
                            emitBand(begin, finish, baseY, sillY); // подоконная стенка
                            emitBand(begin, finish, headY, topY); // перемычка
                            emitBand(begin, u0, sillY, headY);    // простенок слева
                            emitBand(u1, finish, sillY, headY);   // простенок справа
                            emitReveals(u0, u1, sillY, headY, cellModule->params[3] == 1);
                            const float barHalf = WALL_FRAME_BAR * 0.5f;
                            const float uMid = (u0 + u1) * 0.5f;
                            const float yMid = (sillY + headY) * 0.5f;
                            if (frameKind != 1) { // вертикальная планка есть у креста и вертикали
                                emitBar(uMid - barHalf, uMid + barHalf, sillY, headY, center - barHalf, center + barHalf);
                            }
                            if (frameKind == 0) { // горизонтальная — только у креста
                                emitBar(u0, u1, yMid - barHalf, yMid + barHalf, center - barHalf, center + barHalf);
                            }
                            // Наличник (params[3]: 0 наличник, 1 без, 2 +сандрик): планки
                            // вокруг проёма на обеих сторонах полотна + подоконная доска.
                            // Планки клампятся в ячейку — соседние окна не пересекаются.
                            const uint8_t trimKind = cellModule->params[3];
                            if (trimKind != 1) {
                                const float cw = WALL_CASING_WIDTH;
                                const float cd = WALL_CASING_DEPTH;
                                const float left = std::max(u0 - cw, cellU);
                                const float right = std::min(u1 + cw, cellU + 1.f);
                                const float casingSides[2][2] = {
                                    {sideB, sideB + cd}, {sideA - cd, sideA}
                                };
                                for (const auto &side : casingSides) {
                                    emitBox(
                                        left, u0, sillY, headY, side[0], side[1],
                                        WALL_CASING_TILE, 0xFFFFFFFF
                                    );
                                    emitBox(
                                        u1, right, sillY, headY, side[0], side[1],
                                        WALL_CASING_TILE, 0xFFFFFFFF
                                    );
                                    emitBox(
                                        left, right, headY, headY + cw, side[0], side[1],
                                        WALL_CASING_TILE, 0xFFFFFFFF
                                    );
                                }
                                emitBox( // подоконная доска — шире и глубже планок
                                    left, right, sillY - 0.07f, sillY, sideA - cd - 0.02f,
                                    sideB + cd + 0.02f, WALL_CASING_TILE, 0xFFFFFFFF
                                );
                                if (trimKind == 2) { // сандрик-полочка над окном
                                    emitBox(
                                        std::max(left - 0.06f, cellU), std::min(right + 0.06f, cellU + 1.f),
                                        headY + cw, headY + cw + 0.09f, sideA - cd - 0.03f,
                                        sideB + cd + 0.03f, WALL_CASING_TILE, 0xFFFFFFFF
                                    );
                                }
                            }
                            emitPlinth(begin, finish);
                            break;
                        }
                        case ArchObjectType::Door: {
                            // Пресет ширины проёма: стандарт / узкий / портал во всю ячейку.
                            const float doorJamb = cellModule->params[0] == 1   ? 0.25f
                                                   : cellModule->params[0] == 2 ? 0.02f
                                                                                : WALL_DOOR_JAMB;
                            const float headY = baseY + WALL_HEAD_HEIGHT;
                            const float u0 = cellU + doorJamb;
                            const float u1 = cellU + 1.f - doorJamb;
                            emitBand(begin, u0, baseY, headY);    // простенок слева
                            emitBand(u1, finish, baseY, headY);   // простенок справа
                            emitBand(begin, finish, headY, topY); // перемычка
                            emitReveals(u0, u1, baseY, headY, false); // порог — пол, без откоса
                            emitPlinth(begin, u0);
                            emitPlinth(u1, finish);
                            break;
                        }
                        default:
                            break;
                    }

                    // Верх/низ: полосы толщиной стены (пропуск при вертикальном продолжении).
                    for (float u = begin; u < finish - 1e-4f;) {
                        const float next = std::min(std::floor(u + 1e-4f) + 1.f, finish);
                        const float texU0 = u - std::floor(u + 1e-4f);
                        const float texU1 = texU0 + (next - u);
                        if (!wallAbove) {
                            emitQuad(
                                P(u, topY, sideA), P(u, topY, sideB), P(next, topY, sideB),
                                P(next, topY, sideA), 0.f, 1.f, 0.f, topTile, topTint, texU0, 0.f,
                                texU1, wallThickness
                            );
                        }
                        if (!wallBelow) {
                            emitQuad(
                                P(u, baseY, sideA), P(next, baseY, sideA), P(next, baseY, sideB),
                                P(u, baseY, sideB), 0.f, -1.f, 0.f, topTile, topTint, texU0, 0.f,
                                texU1, wallThickness
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
                            0.f, wallThickness, 1.f
                        );
                    };
                    if (i == 0 && !extendStart && !retractStart) { addCap(begin, true); }
                    if (i == runLength - 1 && !extendEnd && !retractEnd) { addCap(finish, false); }
                }
            }
        }
    }
}

namespace {

// Сечение карниза: двусторонний ступенчатый профиль (примыкание ±0.15 = толщина стены,
// выступ до ±0.38, высота 0.3). Обход — от правого примыкания через верх к левому:
// нормали (путь × профиль) выходят наружу.
const std::vector<ExtrudedProfile::ProfilePoint> CORNICE_PROFILE = {
    {0.15f, 0.f}, {0.28f, 0.f}, {0.28f, 0.12f}, {0.38f, 0.12f}, {0.38f, 0.30f},
    {-0.38f, 0.30f}, {-0.38f, 0.12f}, {-0.28f, 0.12f}, {-0.28f, 0.f}, {-0.15f, 0.f},
};

} // namespace

// Цепочки карнизных ячеек (соседство по 4 сторонам, тот же материал и высота) →
// непрерывный профиль по пути: кольца на границах ячеек, митра в центрах угловых
// (масштаб поперечника 1/cos 45°), замкнутый контур здания — кольцо без торцов.
// Чанко-клип: отрезок рисует чанк ячейки его середины.
void BuildingCellGrid::appendCorniceGeometry(
    int startX, int startY, int startZ, int endX, int endY, int endZ,
    std::vector<Vertex> &vertices, std::vector<uint32_t> &indices
) const {
    std::unordered_map<uint32_t, bool> processedCells;

    const auto corniceAt = [this](int x, int y, int z) -> const ArchitectureObject * {
        const ArchitectureObject *object = objectAt(x, y, z);
        return object != nullptr && object->type == ArchObjectType::Cornice ? object : nullptr;
    };
    const auto sameCornice = [&](const ArchitectureObject &sample, int x, int z) -> const ArchitectureObject * {
        const ArchitectureObject *cornice = corniceAt(x, sample.y, z);
        if (cornice == nullptr || cornice->material != sample.material) { return nullptr; }
        return cornice;
    };

    constexpr int DIRECTIONS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    for (int y = startY; y < endY; y++) {
        for (int z = startZ; z < endZ; z++) {
            for (int x = startX; x < endX; x++) {
                const ArchitectureObject *seed = corniceAt(x, y, z);
                if (seed == nullptr || processedCells.count(seed->id)) { continue; }

                // Обход цепочки: прямо, при повороте — единственный боковой; развилка = разрыв.
                const auto step = [&](std::pair<int, int> cell, int &dirX, int &dirZ) -> bool {
                    if (sameCornice(*seed, cell.first + dirX, cell.second + dirZ)) { return true; }
                    int turnX = 0, turnZ = 0, options = 0;
                    for (const auto &direction : DIRECTIONS) {
                        if (direction[0] == -dirX && direction[1] == -dirZ) { continue; } // назад
                        if (direction[0] == dirX && direction[1] == dirZ) { continue; }   // прямо (нет)
                        if (sameCornice(*seed, cell.first + direction[0], cell.second + direction[1])) {
                            turnX = direction[0];
                            turnZ = direction[1];
                            options++;
                        }
                    }
                    if (options != 1) { return false; }
                    dirX = turnX;
                    dirZ = turnZ;
                    return true;
                };
                const auto walk = [&](int dirX, int dirZ, std::vector<std::pair<int, int>> &out) {
                    std::pair<int, int> cell{seed->x, seed->z};
                    while (step(cell, dirX, dirZ)) {
                        cell = {cell.first + dirX, cell.second + dirZ};
                        if (cell.first == seed->x && cell.second == seed->z) { return true; } // цикл
                        out.push_back(cell);
                        if (out.size() > 4096) { break; } // страховка
                    }
                    return false;
                };

                // Первое направление с соседом; одиночка — вдоль rotation взгляда.
                int firstDirX = 0, firstDirZ = 0;
                for (const auto &direction : DIRECTIONS) {
                    if (sameCornice(*seed, seed->x + direction[0], seed->z + direction[1])) {
                        firstDirX = direction[0];
                        firstDirZ = direction[1];
                        break;
                    }
                }
                std::vector<std::pair<int, int>> chain;
                bool closed = false;
                if (firstDirX == 0 && firstDirZ == 0) {
                    chain.push_back({seed->x, seed->z}); // одиночная ячейка
                } else {
                    std::vector<std::pair<int, int>> forward;
                    closed = walk(firstDirX, firstDirZ, forward);
                    std::vector<std::pair<int, int>> backward;
                    if (!closed) { walk(-firstDirX, -firstDirZ, backward); }
                    for (auto it = backward.rbegin(); it != backward.rend(); ++it) {
                        chain.push_back(*it);
                    }
                    chain.push_back({seed->x, seed->z});
                    for (const auto &cell : forward) {
                        chain.push_back(cell);
                    }
                }
                for (const auto &cell : chain) {
                    if (const ArchitectureObject *object = corniceAt(cell.first, y, cell.second)) {
                        processedCells.emplace(object->id, true);
                    }
                }

                // Кольца пути: внешние края (открытые концы), границы ячеек (целые
                // метры), центры угловых с митрой. Продольная координата — накопленная
                // дистанция между кольцами; владелец отрезка — ячейка его середины.
                const float baseHeight = static_cast<float>(seed->y);
                const auto centerOf = [&](const std::pair<int, int> &cell) {
                    return Vec3(cell.first + 0.5f, baseHeight, cell.second + 0.5f);
                };
                const auto rightVec = [](const Vec3 &direction) {
                    return Vec3(-direction.z, 0.f, direction.x); // dir × up, Y-вверх
                };
                const auto directionBetween = [&](size_t indexA, size_t indexB) {
                    const Vec3 a = centerOf(chain[indexA]);
                    const Vec3 b = centerOf(chain[indexB]);
                    return Vec3(b.x - a.x, 0.f, b.z - a.z);
                };

                struct RawRing {
                    Vec3 base;
                    Vec3 side;
                    Vec3 ownerMid; // середина отрезка ЗА кольцом (чанко-клип)
                };
                std::vector<RawRing> raw;

                if (chain.size() == 1) {
                    // Одиночка: путь через ячейку вдоль rotation (0 — X, 1 — Z).
                    const Vec3 center = centerOf(chain[0]);
                    const Vec3 direction = seed->rotation == 0 ? Vec3(1.f, 0.f, 0.f) : Vec3(0.f, 0.f, 1.f);
                    const Vec3 side = rightVec(direction);
                    raw.push_back(
                        {Vec3(center.x - direction.x * 0.5f, baseHeight, center.z - direction.z * 0.5f),
                         side, center}
                    );
                    raw.push_back(
                        {Vec3(center.x + direction.x * 0.5f, baseHeight, center.z + direction.z * 0.5f),
                         side, center}
                    );
                } else {
                    const size_t count = chain.size();
                    for (size_t i = 0; i < count; i++) {
                        const Vec3 center = centerOf(chain[i]);
                        const size_t previous = (i + count - 1) % count;
                        const size_t next = (i + 1) % count;
                        const bool hasPrevious = closed || i > 0;
                        const bool hasNext = closed || i + 1 < count;
                        const Vec3 inDir = hasPrevious ? directionBetween(previous, i) : directionBetween(i, next);
                        const Vec3 outDir = hasNext ? directionBetween(i, next) : inDir;
                        const bool corner =
                            hasPrevious && hasNext && (inDir.x != outDir.x || inDir.z != outDir.z);

                        if (!hasPrevious) { // открытое начало: внешний край ячейки
                            raw.push_back(
                                {Vec3(center.x - outDir.x * 0.5f, baseHeight, center.z - outDir.z * 0.5f),
                                 rightVec(outDir), center}
                            );
                        }
                        if (corner) { // митра в центре угловой (биссектриса, 1/cos 45°)
                            const Vec3 rightIn = rightVec(inDir);
                            const Vec3 rightOut = rightVec(outDir);
                            Vec3 miter(rightIn.x + rightOut.x, 0.f, rightIn.z + rightOut.z);
                            const float miterLength =
                                std::sqrt(miter.x * miter.x + miter.z * miter.z);
                            if (miterLength > 1e-5f) {
                                const float miterScale = std::sqrt(2.f) / miterLength;
                                miter = Vec3(miter.x * miterScale, 0.f, miter.z * miterScale);
                            }
                            raw.push_back(
                                {center, miter,
                                 Vec3(center.x + outDir.x * 0.25f, baseHeight, center.z + outDir.z * 0.25f)}
                            );
                        }
                        if (hasNext) { // граница со следующей ячейкой: отрезок за ней — её
                            raw.push_back(
                                {Vec3(center.x + outDir.x * 0.5f, baseHeight, center.z + outDir.z * 0.5f),
                                 rightVec(outDir), centerOf(chain[next])}
                            );
                        } else { // открытый конец: внешний край ячейки
                            raw.push_back(
                                {Vec3(center.x + inDir.x * 0.5f, baseHeight, center.z + inDir.z * 0.5f),
                                 rightVec(inDir), center}
                            );
                        }
                    }
                }

                std::vector<ExtrudedProfile::PathRing> rings;
                rings.reserve(raw.size());
                float along = 0.f;
                for (size_t i = 0; i < raw.size(); i++) {
                    if (i > 0) {
                        const Vec3 &previousBase = raw[i - 1].base;
                        const float dx = raw[i].base.x - previousBase.x;
                        const float dz = raw[i].base.z - previousBase.z;
                        along += std::sqrt(dx * dx + dz * dz);
                    }
                    ExtrudedProfile::PathRing ring;
                    ring.base = raw[i].base;
                    ring.side = raw[i].side;
                    ring.along = along;
                    const int ownerX = static_cast<int>(std::floor(raw[i].ownerMid.x));
                    const int ownerZ = static_cast<int>(std::floor(raw[i].ownerMid.z));
                    ring.chunkOwned = ownerX >= startX && ownerX < endX && ownerZ >= startZ &&
                                      ownerZ < endZ && seed->y >= startY && seed->y < endY;
                    rings.push_back(ring);
                }

                const Voxel voxel(seed->material);
                const VoxelTextureData &texture = VoxelTextureMapper::getTextureData(&voxel);
                ExtrudedProfile::extrude(
                    CORNICE_PROFILE, rings, closed, texture.sideTileIndex, texture.sideColor,
                    vertices, indices
                );
            }
        }
    }
}

namespace {

// Двускатная крыша: уклон (высота на метр поперечника), свес стрехи, толщина пластины
// ската, толщина фронтонной стенки, полусечение конькового бруса.
constexpr float ROOF_SLOPE = 0.7f;
constexpr float ROOF_OVERHANG = 0.25f;
constexpr float ROOF_THICKNESS = 0.12f;
constexpr float GABLE_THICKNESS = 0.2f;
constexpr float RIDGE_HALF_WIDTH = 0.16f;
constexpr float RIDGE_HEIGHT = 0.1f;

} // namespace

void BuildingCellGrid::collectRoofRegion(
    int x, int y, int z, std::vector<std::array<int, 3>> &outCells
) const {
    outCells.clear();
    const ArchitectureObject *seed = objectAt(x, y, z);
    if (seed == nullptr || seed->type != ArchObjectType::Roof) { return; }
    std::unordered_map<uint64_t, bool> visited;
    std::vector<std::array<int, 3>> frontier = {{x, y, z}};
    visited.emplace(packCell(x, y, z), true);
    constexpr int NEIGHBORS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    while (!frontier.empty()) {
        const std::array<int, 3> cell = frontier.back();
        frontier.pop_back();
        outCells.push_back(cell);
        for (const auto &offset : NEIGHBORS) {
            const int nx = cell[0] + offset[0];
            const int nz = cell[2] + offset[1];
            if (visited.count(packCell(nx, y, nz))) { continue; }
            const ArchitectureObject *neighbor = objectAt(nx, y, nz);
            if (neighbor == nullptr || neighbor->type != ArchObjectType::Roof ||
                neighbor->material != seed->material || neighbor->y != seed->y ||
                std::memcmp(neighbor->params, seed->params, ARCH_PARAM_COUNT) != 0) {
                continue;
            }
            visited.emplace(packCell(nx, y, nz), true);
            frontier.push_back({nx, y, nz});
        }
    }
}

void BuildingCellGrid::markRoofRegionDirty(const ArchitectureObject &object) {
    std::vector<std::array<int, 3>> region;
    collectRoofRegion(object.x, object.y, object.z, region);
    for (const auto &cell : region) {
        markDirtyAround(cell[0], cell[1], cell[2]);
    }
}

// Область крышных ячеек → двускатная крыша: конёк вдоль длинной оси области, каждая
// полоса поперёк конька строит два ската от своих краёв (свес + пластина с толщиной),
// фронтоны на торцах вдоль конька, коньковый брус «домиком». Полосу целиком рисует
// чанк её левой ячейки (регион-dirty при правках гарантирует пересбор).
void BuildingCellGrid::appendRoofGeometry(
    int startX, int startY, int startZ, int endX, int endY, int endZ,
    std::vector<Vertex> &vertices, std::vector<uint32_t> &indices,
    std::vector<Vertex> &roofVertices, std::vector<uint32_t> &roofIndices
) const {
    std::unordered_map<uint32_t, bool> processedRegions;

    for (int y = startY; y < endY; y++) {
        for (int z = startZ; z < endZ; z++) {
            for (int x = startX; x < endX; x++) {
                const ArchitectureObject *seed = objectAt(x, y, z);
                if (seed == nullptr || seed->type != ArchObjectType::Roof) { continue; }

                std::vector<std::array<int, 3>> region;
                collectRoofRegion(x, y, z, region);
                if (region.empty()) { continue; }
                uint32_t canonicalId = UINT32_MAX;
                for (const auto &cell : region) {
                    const ArchitectureObject *object = objectAt(cell[0], y, cell[2]);
                    if (object != nullptr && object->id < canonicalId) { canonicalId = object->id; }
                }
                if (processedRegions.count(canonicalId)) { continue; }
                processedRegions.emplace(canonicalId, true);

                // Конёк — вдоль оси с бОльшим экстентом области.
                int minX = region[0][0], maxX = region[0][0];
                int minZ = region[0][2], maxZ = region[0][2];
                for (const auto &cell : region) {
                    minX = std::min(minX, cell[0]);
                    maxX = std::max(maxX, cell[0]);
                    minZ = std::min(minZ, cell[2]);
                    maxZ = std::max(maxZ, cell[2]);
                }
                const bool alongX = (maxX - minX) >= (maxZ - minZ);
                const int minA = alongX ? minX : minZ;
                const int maxA = alongX ? maxX : maxZ;

                // Диапазон поперечника каждой полосы вдоль конька.
                struct Strip {
                    bool present = false;
                    int tMin = 0;
                    int tMax = 0;
                    int ownerT = 0; // ячейка-владелец (клип по чанку)
                };
                std::vector<Strip> strips(static_cast<size_t>(maxA - minA + 1));
                for (const auto &cell : region) {
                    const int a = alongX ? cell[0] : cell[2];
                    const int t = alongX ? cell[2] : cell[0];
                    Strip &strip = strips[static_cast<size_t>(a - minA)];
                    if (!strip.present) {
                        strip.present = true;
                        strip.tMin = strip.tMax = strip.ownerT = t;
                    } else {
                        strip.tMin = std::min(strip.tMin, t);
                        strip.tMax = std::max(strip.tMax, t);
                        strip.ownerT = strip.tMin;
                    }
                }

                const float baseY = static_cast<float>(y);
                const Voxel voxel(seed->material);
                const VoxelTextureData &texture = VoxelTextureMapper::getTextureData(&voxel);
                const uint8_t wallTile = texture.sideTileIndex;
                const uint32_t tint = texture.sideColor;
                // Параметры-пресеты области (у всех ячеек области равны — критерий flood).
                const bool flatRoof = seed->params[0] == 1;
                const float roofSlope =
                    seed->params[1] == 1 ? 0.4f : seed->params[1] == 2 ? 1.0f : ROOF_SLOPE;
                const float roofOverhang =
                    seed->params[3] == 1 ? 0.f : seed->params[3] == 2 ? 0.5f : ROOF_OVERHANG;
                // Черепица: явная колонка атласа или авто по материалу объекта (ряд 0).
                const uint8_t roofTile = [&]() -> uint8_t {
                    if (seed->params[2] >= 1 && seed->params[2] <= 4) {
                        return static_cast<uint8_t>(seed->params[2] - 1);
                    }
                    switch (seed->material) {
                        case VoxelType::BOARDS:
                        case VoxelType::TREE:
                        case VoxelType::DARK_BRICK: return 1; // коричневая
                        case VoxelType::SAND_STONE:
                        case VoxelType::WHITE_PLASTER: return 2; // серая
                        case VoxelType::DARK_STONE:
                        case VoxelType::SLATE: return 3; // тёмная
                        default: return 0; // красная
                    }
                }();

                // Все квады — в осях (a, y, t); конёк вдоль Z отражает порядок вершин.
                // Скаты/конёк — в roof-буферы (черепица), фронтоны — в основной (стены).
                const auto P = [&](float a, float yy, float t) {
                    return alongX ? Vec3(a, yy, t) : Vec3(t, yy, a);
                };
                const auto emitQuadTo = [&](std::vector<Vertex> &outVertices,
                                            std::vector<uint32_t> &outIndices, uint8_t quadTile,
                                            const Vec3 &p0, const Vec3 &p1, const Vec3 &p2,
                                            const Vec3 &p3, float na, float ny, float nt, float u0,
                                            float v0, float u1, float v1) {
                    const Vec3 normal = alongX ? Vec3(na, ny, nt) : Vec3(nt, ny, na);
                    if (alongX) {
                        addWallQuad(outVertices, outIndices, p0, p1, p2, p3, normal, quadTile, tint, u0, v0, u1, v1);
                    } else {
                        addWallQuad(outVertices, outIndices, p0, p3, p2, p1, normal, quadTile, tint, u0, v0, u1, v1);
                    }
                };
                const auto emitQuad = [&](const Vec3 &p0, const Vec3 &p1, const Vec3 &p2,
                                          const Vec3 &p3, float na, float ny, float nt, float u0,
                                          float v0, float u1, float v1) {
                    emitQuadTo(roofVertices, roofIndices, roofTile, p0, p1, p2, p3, na, ny, nt, u0, v0, u1, v1);
                };
                const auto emitTriangleTo = [&](std::vector<Vertex> &outVertices,
                                                std::vector<uint32_t> &outIndices, uint8_t triangleTile,
                                                const Vec3 &p0, const Vec3 &p1, const Vec3 &p2,
                                                float na, float ny, float nt) {
                    const Vec3 normal = alongX ? Vec3(na, ny, nt) : Vec3(nt, ny, na);
                    const float uvSize = 1.f / BLOCKS_ATLAS_GRID;
                    const Vec2 uv(
                        triangleTile % BLOCKS_ATLAS_GRID * uvSize + uvSize * 0.5f,
                        triangleTile / BLOCKS_ATLAS_GRID * uvSize + uvSize * 0.5f
                    );
                    const uint32_t base = static_cast<uint32_t>(outVertices.size());
                    const Color color = hexColor(tint);
                    outVertices.emplace_back(Vertex(p0, uv, normal, color, 1.f));
                    if (alongX) {
                        outVertices.emplace_back(Vertex(p1, uv, normal, color, 1.f));
                        outVertices.emplace_back(Vertex(p2, uv, normal, color, 1.f));
                    } else {
                        outVertices.emplace_back(Vertex(p2, uv, normal, color, 1.f));
                        outVertices.emplace_back(Vertex(p1, uv, normal, color, 1.f));
                    }
                    for (uint32_t index : {base, base + 1u, base + 2u}) {
                        outIndices.emplace_back(index);
                    }
                };

                const float effectiveSlope = flatRoof ? 0.f : roofSlope;
                const float slopeAngleCos = 1.f / std::sqrt(1.f + effectiveSlope * effectiveSlope);
                const float slopeAngleSin = effectiveSlope * slopeAngleCos;

                for (size_t stripIndex = 0; stripIndex < strips.size(); stripIndex++) {
                    const Strip &strip = strips[stripIndex];
                    if (!strip.present) { continue; }
                    const int stripA = minA + static_cast<int>(stripIndex);
                    // Клип: полосу целиком рисует чанк её левой ячейки.
                    const int ownerX = alongX ? stripA : strip.ownerT;
                    const int ownerZ = alongX ? strip.ownerT : stripA;
                    if (ownerX < startX || ownerX >= endX || ownerZ < startZ || ownerZ >= endZ) {
                        continue;
                    }

                    const float a0 = static_cast<float>(stripA);
                    const float a1 = a0 + 1.f;
                    const float tEdge0 = static_cast<float>(strip.tMin) - roofOverhang;
                    const float tEdge1 = static_cast<float>(strip.tMax) + 1.f + roofOverhang;
                    const float tMid = (tEdge0 + tEdge1) * 0.5f;
                    const float eavesY = baseY - roofOverhang * effectiveSlope;
                    const float ridgeY = eavesY + (tMid - tEdge0) * effectiveSlope;
                    const float slopeLength = (tMid - tEdge0) / slopeAngleCos;

                    // Скат: right=false — подъём в +t (нормаль в −t), true — зеркально.
                    const auto emitSlope = [&](bool right) {
                        const float tStart = right ? tEdge1 : tEdge0;
                        const float tDirection = right ? -1.f : 1.f;
                        const float normalT = right ? slopeAngleSin : -slopeAngleSin;
                        const auto surfacePoint = [&](float along, float s, bool lower) {
                            const float t = tStart + tDirection * s * slopeAngleCos;
                            const float height = eavesY + s * slopeAngleSin;
                            const float offsetY = lower ? -ROOF_THICKNESS * slopeAngleCos : 0.f;
                            const float offsetT = lower ? -normalT * ROOF_THICKNESS : 0.f;
                            return P(along, height + offsetY, t + offsetT);
                        };
                        for (float s = 0.f; s < slopeLength - 1e-4f;) {
                            const float sNext = std::min(std::floor(s + 1e-4f) + 1.f, slopeLength);
                            const float texU0 = s - std::floor(s + 1e-4f);
                            const float texU1 = texU0 + (sNext - s);
                            const float texV0 = a0 - std::floor(a0);
                            // Верхняя поверхность.
                            if (right) {
                                emitQuad(
                                    surfacePoint(a0, s, false), surfacePoint(a1, s, false),
                                    surfacePoint(a1, sNext, false), surfacePoint(a0, sNext, false),
                                    0.f, slopeAngleCos, normalT, texU0, texV0, texU1, texV0 + 1.f
                                );
                                emitQuad(
                                    surfacePoint(a1, s, true), surfacePoint(a0, s, true),
                                    surfacePoint(a0, sNext, true), surfacePoint(a1, sNext, true),
                                    0.f, -slopeAngleCos, -normalT, texU0, texV0, texU1, texV0 + 1.f
                                );
                            } else {
                                emitQuad(
                                    surfacePoint(a1, s, false), surfacePoint(a0, s, false),
                                    surfacePoint(a0, sNext, false), surfacePoint(a1, sNext, false),
                                    0.f, slopeAngleCos, normalT, texU0, texV0, texU1, texV0 + 1.f
                                );
                                emitQuad(
                                    surfacePoint(a0, s, true), surfacePoint(a1, s, true),
                                    surfacePoint(a1, sNext, true), surfacePoint(a0, sNext, true),
                                    0.f, -slopeAngleCos, -normalT, texU0, texV0, texU1, texV0 + 1.f
                                );
                            }
                            s = sNext;
                        }
                        // Стреха: торец пластины (нормаль вниз-наружу вдоль ската).
                        const float capNormalT = right ? slopeAngleCos : -slopeAngleCos;
                        if (right) {
                            emitQuad(
                                surfacePoint(a1, 0.f, false), surfacePoint(a0, 0.f, false),
                                surfacePoint(a0, 0.f, true), surfacePoint(a1, 0.f, true), 0.f,
                                -slopeAngleSin, capNormalT, 0.f, 0.f, 1.f, ROOF_THICKNESS
                            );
                        } else {
                            emitQuad(
                                surfacePoint(a0, 0.f, false), surfacePoint(a1, 0.f, false),
                                surfacePoint(a1, 0.f, true), surfacePoint(a0, 0.f, true), 0.f,
                                -slopeAngleSin, capNormalT, 0.f, 0.f, 1.f, ROOF_THICKNESS
                            );
                        }
                    };
                    emitSlope(false);
                    emitSlope(true);

                    // Фронтоны: на торцах области вдоль конька (границы без соседней полосы).
                    const bool gableFront =
                        stripIndex == 0 || !strips[stripIndex - 1].present;
                    const bool gableBack =
                        stripIndex + 1 >= strips.size() || !strips[stripIndex + 1].present;
                    const float wallT0 = static_cast<float>(strip.tMin);
                    const float wallT1 = static_cast<float>(strip.tMax) + 1.f;
                    const float wallMid = (wallT0 + wallT1) * 0.5f;
                    const float wallRidgeY = baseY + (wallMid - wallT0) * effectiveSlope;
                    const auto emitGable = [&](float atA, float inwardSign) {
                        const float innerA = atA + inwardSign * GABLE_THICKNESS;
                        // Наружная и внутренняя треугольные грани (материал стен).
                        emitTriangleTo(
                            vertices, indices, wallTile, P(atA, baseY, wallT0), P(atA, baseY, wallT1),
                            P(atA, wallRidgeY, wallMid), -inwardSign, 0.f, 0.f
                        );
                        emitTriangleTo(
                            vertices, indices, wallTile, P(innerA, baseY, wallT0),
                            P(innerA, wallRidgeY, wallMid), P(innerA, baseY, wallT1), inwardSign,
                            0.f, 0.f
                        );
                    };
                    if (!flatRoof && gableFront) { emitGable(a0, 1.f); }
                    if (!flatRoof && gableBack) { emitGable(a1, -1.f); }

                    // Коньковый брус «домиком» поверх стыка скатов (у плоской нет конька).
                    if (flatRoof) { continue; }
                    const float ridgeBaseY = ridgeY - RIDGE_HALF_WIDTH * effectiveSlope;
                    emitQuad(
                        P(a1, ridgeBaseY, tMid - RIDGE_HALF_WIDTH), P(a0, ridgeBaseY, tMid - RIDGE_HALF_WIDTH),
                        P(a0, ridgeY + RIDGE_HEIGHT, tMid), P(a1, ridgeY + RIDGE_HEIGHT, tMid), 0.f,
                        slopeAngleCos, -slopeAngleSin, 0.f, 0.f, 1.f, 0.4f
                    );
                    emitQuad(
                        P(a0, ridgeBaseY, tMid + RIDGE_HALF_WIDTH), P(a1, ridgeBaseY, tMid + RIDGE_HALF_WIDTH),
                        P(a1, ridgeY + RIDGE_HEIGHT, tMid), P(a0, ridgeY + RIDGE_HEIGHT, tMid), 0.f,
                        slopeAngleCos, slopeAngleSin, 0.f, 0.f, 1.f, 0.4f
                    );
                    if (gableFront) {
                        emitTriangleTo(
                            roofVertices, roofIndices, roofTile,
                            P(a0, ridgeBaseY, tMid - RIDGE_HALF_WIDTH),
                            P(a0, ridgeBaseY, tMid + RIDGE_HALF_WIDTH),
                            P(a0, ridgeY + RIDGE_HEIGHT, tMid), -1.f, 0.f, 0.f
                        );
                    }
                    if (gableBack) {
                        emitTriangleTo(
                            roofVertices, roofIndices, roofTile,
                            P(a1, ridgeBaseY, tMid + RIDGE_HALF_WIDTH),
                            P(a1, ridgeBaseY, tMid - RIDGE_HALF_WIDTH),
                            P(a1, ridgeY + RIDGE_HEIGHT, tMid), 1.f, 0.f, 0.f
                        );
                    }
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
}
