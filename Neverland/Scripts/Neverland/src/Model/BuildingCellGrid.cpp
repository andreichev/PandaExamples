#include "BuildingCellGrid.hpp"
#include "BlockModel.hpp"
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

constexpr int BLOCKS_ATLAS_GRID = 10;
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

// Каноническое UV-соответствие граней (как в Minecraft): доля uv-прямоугольника из
// флагов угла бокса (0/1 по осям). Индекс — позиция грани в FACES.
inline void faceUVFraction(int faceSpecIndex, const int corner[3], float &u, float &v) {
    const float fx = static_cast<float>(corner[0]);
    const float fy = static_cast<float>(corner[1]);
    const float fz = static_cast<float>(corner[2]);
    switch (faceSpecIndex) {
        case 0: u = fx; v = 1.f - fy; break;        // +Z (юг)
        case 1: u = 1.f - fx; v = 1.f - fy; break;  // -Z (север)
        case 2: u = fx; v = fz; break;              // +Y (верх)
        case 3: u = fx; v = 1.f - fz; break;        // -Y (низ)
        case 4: u = fz; v = 1.f - fy; break;        // -X (запад)
        default: u = 1.f - fz; v = 1.f - fy; break; // +X (восток)
    }
}

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
    outCells.push_back({object.x, object.y, object.z});
    if (object.type == ArchObjectType::ModelBlock) {
        const BlockModelData *model = BlockModels::byId(object.params[0]);
        if (model != nullptr && model->kind == 2) { // дверь: низ + верх
            outCells.push_back({object.x, object.y + 1, object.z});
        }
    }
}

bool BuildingCellGrid::isPhysicsSolidAt(int x, int y, int z) const {
    if (!isSolidAt(x, y, z)) { return false; }
    const ArchitectureObject *object = objectAt(x, y, z);
    // Лампа и модельные блоки — декор: не коллайдят (габарит меньше ячейки).
    if (object != nullptr &&
        (object->type == ArchObjectType::Lamp || object->type == ArchObjectType::ModelBlock)) {
        return false;
    }
    return true;
}

bool BuildingCellGrid::isFullCellAt(int x, int y, int z) const {
    if (!isSolidAt(x, y, z)) { return false; }
    const ArchitectureObject *object = objectAt(x, y, z);
    // Лампа и модельные блоки занимают ячейку топологически, но не заполняют её:
    // грань соседнего куба им не прятать, жёсткого AO не давать.
    return object == nullptr || object->type == ArchObjectType::Block;
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

// Модельные блоки: боксы Minecraft block model запекаются в меш чанка. Порядок граней
// модели: down, up, north(-Z), south(+Z), west(-X), east(+X) → индексы FACES.
// Connected-модели (ограды): столб + секции к стыкуемым соседям (полный куб или та же
// модель); поворот секции — от базовой ориентации «на север».
void BuildingCellGrid::appendModelBlockGeometry(
    int startX, int startY, int startZ, int endX, int endY, int endZ,
    std::vector<Vertex> &vertices, std::vector<uint32_t> &indices
) const {
    constexpr int MODEL_FACE_TO_SPEC[6] = {3, 2, 1, 0, 4, 5};
    for (const auto &[objectId, object] : m_objects) {
        if (object.type != ArchObjectType::ModelBlock) { continue; }
        if (object.x < startX || object.x >= endX || object.y < startY || object.y >= endY ||
            object.z < startZ || object.z >= endZ) {
            continue;
        }
        const BlockModelData *model = BlockModels::byId(object.params[0]);
        if (model == nullptr) { continue; } // модель убрана из реестра — ячейка занята, геометрии нет

        const Vec3 base(
            static_cast<float>(object.x), static_cast<float>(object.y),
            static_cast<float>(object.z)
        );

        // Эмит набора боксов с поворотом на rotation × 90° вокруг центра ячейки.
        const auto emitBoxes = [&](const BlockModelBox *boxes, uint16_t boxCount, int rotation) {
            const auto rotateY = [&](Vec3 p) {
                for (int k = 0; k < rotation; k++) {
                    const float x = p.x - 0.5f;
                    const float z = p.z - 0.5f;
                    p.x = 0.5f + z;
                    p.z = 0.5f - x;
                }
                return p;
            };
            const auto rotateDirY = [&](Vec3 n) {
                for (int k = 0; k < rotation; k++) {
                    const float x = n.x;
                    n.x = n.z;
                    n.z = -x;
                }
                return n;
            };
            for (uint16_t b = 0; b < boxCount; b++) {
                const BlockModelBox &box = boxes[b];
                const auto rotateElement = [&](Vec3 p) {
                    if (box.rotAngle == 0.f) { return p; }
                    const float rad = box.rotAngle * 3.1415926535f / 180.f;
                    const float s = std::sin(rad);
                    const float c = std::cos(rad);
                    const Vec3 origin(box.rotOrigin[0], box.rotOrigin[1], box.rotOrigin[2]);
                    const Vec3 d(p.x - origin.x, p.y - origin.y, p.z - origin.z);
                    Vec3 r = d;
                    if (box.rotAxis == 0) {
                        r.y = d.y * c - d.z * s;
                        r.z = d.y * s + d.z * c;
                    } else if (box.rotAxis == 1) {
                        r.x = d.x * c + d.z * s;
                        r.z = -d.x * s + d.z * c;
                    } else {
                        r.x = d.x * c - d.y * s;
                        r.y = d.x * s + d.y * c;
                    }
                    return Vec3(origin.x + r.x, origin.y + r.y, origin.z + r.z);
                };
                const auto rotateElementDir = [&](const Vec3 &n) {
                    const Vec3 a = rotateElement(Vec3(n.x, n.y, n.z));
                    const Vec3 zero = rotateElement(Vec3(0.f, 0.f, 0.f));
                    return Vec3(a.x - zero.x, a.y - zero.y, a.z - zero.z);
                };
                for (int f = 0; f < 6; f++) {
                    const BlockModelFace &face = box.faces[f];
                    if (!face.exists) { continue; }
                    const int specIndex = MODEL_FACE_TO_SPEC[f];
                    const FaceSpec &spec = FACES[specIndex];

                    Vec3 corners[4];
                    for (int corner = 0; corner < 4; corner++) {
                        Vec3 p(
                            box.from[0] + (box.to[0] - box.from[0]) * spec.corners[corner][0],
                            box.from[1] + (box.to[1] - box.from[1]) * spec.corners[corner][1],
                            box.from[2] + (box.to[2] - box.from[2]) * spec.corners[corner][2]
                        );
                        const Vec3 local = rotateY(rotateElement(p));
                        corners[corner] =
                            Vec3(base.x + local.x, base.y + local.y, base.z + local.z);
                    }
                    Vec3 normal(
                        static_cast<float>(spec.normal[0]), static_cast<float>(spec.normal[1]),
                        static_cast<float>(spec.normal[2])
                    );
                    normal = rotateDirY(rotateElementDir(normal));

                    // Шейд грани по доминанте итоговой нормали (значения — как у кубов FACES).
                    float shade = 0.9f;
                    const float ax = std::abs(normal.x);
                    const float ay = std::abs(normal.y);
                    const float az = std::abs(normal.z);
                    if (ay >= ax && ay >= az) {
                        shade = normal.y >= 0.f ? 0.95f : 0.85f;
                    } else if (az >= ax) {
                        shade = normal.z >= 0.f ? 1.0f : 0.75f;
                    } else {
                        shade = normal.x >= 0.f ? 0.8f : 0.9f;
                    }
                    const float packedLight =
                        packCellLight(m_lightGrid, object.x, object.y, object.z, shade);

                    const Vec2 uvBase = tileUV(face.tile);
                    const float uvSize = 1.f / BLOCKS_ATLAS_GRID - 0.001f;
                    const uint32_t first = static_cast<uint32_t>(vertices.size());
                    for (uint32_t index :
                         {first, first + 1u, first + 2u, first + 2u, first + 3u, first}) {
                        indices.emplace_back(index);
                    }
                    for (int corner = 0; corner < 4; corner++) {
                        float fu = 0.f, fv = 0.f;
                        faceUVFraction(specIndex, spec.corners[corner], fu, fv);
                        for (int r = 0; r < face.uvRot; r++) { // поворот uv по часовой
                            const float t = fu;
                            fu = fv;
                            fv = 1.f - t;
                        }
                        const float cu = face.u0 + (face.u1 - face.u0) * fu;
                        const float cv = face.v0 + (face.v1 - face.v0) * fv;
                        const Vec2 uv(uvBase.x + cu * uvSize, uvBase.y + cv * uvSize);
                        vertices.emplace_back(
                            Vertex(corners[corner], uv, normal, hexColor(0xFFFFFFFF), packedLight)
                        );
                    }
                }
            }
        };

        if (model->kind == 3) {
            // Ориентация по соседям: «тяжёлая» сторона тянется к полному кубу; два
            // перпендикулярных куба — внутренний угол; перпендикулярная пара таких же
            // моделей без кубов — внешний угол; иначе — поворот, заданный при установке.
            // Направления в k-последовательности поворотов: [N, W, S, E], диагонали
            // [NE, NW, SW, SE].
            const int DIRS[4][2] = {{0, -1}, {-1, 0}, {0, 1}, {1, 0}};
            // Fallback без соседей: «тяжёлая» сторона по направлению взгляда при установке
            // (rotation 0..3 = +X/+Z/−X/−Z → индексы E/S/W/N).
            const int lookIdx = (3 - (object.rotation & 3)) & 3;
            const int lookK = (lookIdx - model->backBase) & 3;
            bool solid[4];
            bool sameModel[4];
            int solidCount = 0;
            for (int d = 0; d < 4; d++) {
                const int nx = object.x + DIRS[d][0];
                const int nz = object.z + DIRS[d][1];
                solid[d] = isFullCellAt(nx, object.y, nz);
                if (solid[d]) { solidCount++; }
                const ArchitectureObject *other = objectAt(nx, object.y, nz);
                sameModel[d] = other != nullptr && other->type == ArchObjectType::ModelBlock &&
                               other->params[0] == object.params[0];
            }
            const auto diagIndex = [](int a, int b) { // два перпендикулярных направления
                const bool north = a == 0 || b == 0;
                const bool west = a == 1 || b == 1;
                const bool south = a == 2 || b == 2;
                if (north) { return west ? 1 : 0; } // NW : NE
                return west ? 2 : 3;                // SW : SE
            };
            int first = -1, second = -1;
            for (int d = 0; d < 4; d++) {
                if (solid[d]) { (first < 0 ? first : second) = d; }
            }
            // «Тяжёлая» сторона этой ячейки: один куб — к нему; три — противоположно
            // свободной; иначе — по взгляду при установке.
            int myBack = lookIdx;
            if (solidCount == 1) {
                myBack = first;
            } else if (solidCount == 3) {
                for (int d = 0; d < 4; d++) {
                    if (!solid[d]) { myBack = (d + 2) % 4; }
                }
            }
            // «Тяжёлая» сторона соседа той же модели (без углового шага): его куб или взгляд.
            const auto neighborBack = [&](int d) {
                const int nx = object.x + DIRS[d][0];
                const int nz = object.z + DIRS[d][1];
                int back = -1, backCount = 0;
                for (int nd = 0; nd < 4; nd++) {
                    if (isFullCellAt(nx + DIRS[nd][0], object.y, nz + DIRS[nd][1])) {
                        back = nd;
                        backCount++;
                    }
                }
                if (backCount == 1) { return back; }
                const ArchitectureObject *other = objectAt(nx, object.y, nz);
                return other != nullptr ? (3 - (other->rotation & 3)) & 3 : -1;
            };

            if (solidCount == 2 && (first + 2) % 4 != second && model->innerBoxCount > 0) {
                // Два перпендикулярных куба: внутренний угол (заполнение к кубам, +2).
                const int k = (diagIndex(first, second) + 2 - model->innerDiagBase) & 3;
                emitBoxes(model->innerBoxes, model->innerBoxCount, k);
            } else if (sameModel[myBack] && model->outerBoxCount > 0 &&
                       ((neighborBack(myBack) & 1) != (myBack & 1)) && neighborBack(myBack) >= 0) {
                // Сосед за высокой частью повёрнут поперёк → внешний угол (обход линии).
                const int diag = diagIndex(myBack, neighborBack(myBack));
                emitBoxes(model->outerBoxes, model->outerBoxCount, (diag - model->outerDiagBase) & 3);
            } else if (sameModel[(myBack + 2) % 4] && model->innerBoxCount > 0 &&
                       neighborBack((myBack + 2) % 4) >= 0 &&
                       ((neighborBack((myBack + 2) % 4) & 1) != (myBack & 1))) {
                // Сосед перед низкой частью повёрнут поперёк → внутренний угол.
                const int diag =
                    (diagIndex(myBack, neighborBack((myBack + 2) % 4)) + 2) & 3;
                emitBoxes(model->innerBoxes, model->innerBoxCount, (diag - model->innerDiagBase) & 3);
            } else if (solidCount == 1 || solidCount == 3) {
                emitBoxes(model->boxes, model->boxCount, (myBack - model->backBase) & 3);
            } else if (solidCount == 0 && model->outerBoxCount > 0) {
                int firstModel = -1, secondModel = -1;
                for (int d = 0; d < 4; d++) {
                    if (sameModel[d]) { (firstModel < 0 ? firstModel : secondModel) = d; }
                }
                if (secondModel >= 0 && (firstModel + 2) % 4 != secondModel) {
                    // Оба продолжения на месте: внешний угол по диагонали позиций.
                    const int diag = diagIndex(firstModel, secondModel);
                    emitBoxes(model->outerBoxes, model->outerBoxCount, (diag - model->outerDiagBase) & 3);
                } else {
                    emitBoxes(model->boxes, model->boxCount, lookK);
                }
            } else {
                emitBoxes(model->boxes, model->boxCount, lookK);
            }
        } else if (model->kind == 1) {
            emitBoxes(model->boxes, model->boxCount, 0);
            // Секции к стыкуемым соседям: k поворотов «северной» секции — north 0,
            // west 1, south 2, east 3 (по направлению rotateDirY).
            const int neighbors[4][3] = {
                {0, -1, 0}, {-1, 0, 1}, {0, 1, 2}, {1, 0, 3}
            }; // {dx, dz, k}
            for (const auto &neighbor : neighbors) {
                const int nx = object.x + neighbor[0];
                const int nz = object.z + neighbor[1];
                if (!hasCellAt(nx, object.y, nz)) { continue; }
                const ArchitectureObject *other = objectAt(nx, object.y, nz);
                const bool connects =
                    isFullCellAt(nx, object.y, nz) ||
                    (other != nullptr && other->type == ArchObjectType::ModelBlock &&
                     other->params[0] == object.params[0]);
                if (!connects) { continue; }
                emitBoxes(model->sideBoxes, model->sideBoxCount, (neighbor[2] + model->sideBase) & 3);
            }
        } else {
            emitBoxes(model->boxes, model->boxCount, object.rotation & 3);
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
    m_remeshCounter++;

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
                for (int faceIndex = 0; faceIndex < 6; faceIndex++) {
                    const FaceSpec &face = FACES[faceIndex];
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
                        float cu = 0.f, cv = 0.f;
                        faceUVFraction(faceIndex, face.corners[corner], cu, cv);
                        const Vec2 uv(uvBase.x + cu * uvSize, uvBase.y + cv * uvSize);
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

    appendModelBlockGeometry(startX, startY, startZ, endX, endY, endZ, vertices, indices);

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
    // Шейд по нормали — значения кубовых FACES.
    const float faceLight = normal.z > 0.f   ? 1.0f
                            : normal.z < 0.f ? 0.75f
                            : normal.y > 0.f ? 0.95f
                            : normal.y < 0.f ? 0.85f
                            : normal.x < 0.f ? 0.9f
                                             : 0.8f;
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

namespace {

} // namespace

namespace {


} // namespace

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

