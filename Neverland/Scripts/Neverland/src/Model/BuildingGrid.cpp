#include "BuildingGrid.hpp"
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

void BuildingGrid::init(
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

void BuildingGrid::shutdown() {
    for (ChunkView &view : m_views) {
        if (view.mesh.isValid()) { AssetManagerAPI::deleteMesh(view.mesh); }
        if (view.entity.isValid()) { WorldAPI::destroyEntity(view.entity); }
        view = {};
    }
    m_views.clear();
    m_blocks.clear();
}

bool BuildingGrid::isInside(int x, int y, int z) const {
    return x >= m_minX && y >= m_minY && z >= m_minZ && x < m_minX + m_sizeX &&
           y < m_minY + m_sizeY && z < m_minZ + m_sizeZ;
}

size_t BuildingGrid::voxelIndex(int x, int y, int z) const {
    return (static_cast<size_t>(y - m_minY) * m_sizeZ + static_cast<size_t>(z - m_minZ)) * m_sizeX +
           static_cast<size_t>(x - m_minX);
}

size_t BuildingGrid::chunkIndex(int chunkX, int chunkY, int chunkZ) const {
    return (static_cast<size_t>(chunkY) * m_chunksZ + chunkZ) * m_chunksX + chunkX;
}

VoxelType BuildingGrid::blockAt(int x, int y, int z) const {
    if (!isInside(x, y, z)) { return VoxelType::NOTHING; }
    return static_cast<VoxelType>(m_blocks[voxelIndex(x, y, z)]);
}

bool BuildingGrid::setBlock(int x, int y, int z, VoxelType type) {
    if (!isInside(x, y, z)) { return false; }
    uint8_t &slot = m_blocks[voxelIndex(x, y, z)];
    const uint8_t value = static_cast<uint8_t>(type);
    if (slot == value) { return false; }
    slot = value;
    markDirtyAround(x, y, z);
    rebuildDirtyChunks();
    return true;
}

void BuildingGrid::markDirtyAround(int x, int y, int z) {
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

void BuildingGrid::rebuildDirtyChunks() {
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

void BuildingGrid::rebuildChunk(int chunkX, int chunkY, int chunkZ) {
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
                Voxel voxel(type);
                const VoxelTextureData &texture = VoxelTextureMapper::getTextureData(&voxel);
                for (const FaceSpec &face : FACES) {
                    // Грань прячет только соседний куб построек: рельеф MC срезает поверхность,
                    // природный воксель не укрытие (иначе дыры на стыке).
                    if (isSolidAt(x + face.normal[0], y + face.normal[1], z + face.normal[2])) {
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
                            if (isSolidAt(x + offset[0], y + offset[1], z + offset[2])) {
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

std::optional<BuildingGrid::RaycastHit>
BuildingGrid::raycast(Vec3 origin, Vec3 direction, float maxDistance) const {
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

void BuildingGrid::collectBlocks(std::vector<std::pair<uint64_t, uint8_t>> &outBlocks) const {
    outBlocks.clear();
    for (int y = m_minY; y < m_minY + m_sizeY; y++) {
        for (int z = m_minZ; z < m_minZ + m_sizeZ; z++) {
            for (int x = m_minX; x < m_minX + m_sizeX; x++) {
                const uint8_t value = m_blocks[voxelIndex(x, y, z)];
                if (value == 0) { continue; }
                const uint64_t bx = static_cast<uint64_t>(x + (1 << 20)) & 0x1FFFFF;
                const uint64_t by = static_cast<uint64_t>(y + (1 << 20)) & 0x1FFFFF;
                const uint64_t bz = static_cast<uint64_t>(z + (1 << 20)) & 0x1FFFFF;
                outBlocks.emplace_back((bx << 42) | (by << 21) | bz, value);
            }
        }
    }
}

void BuildingGrid::restoreBlocks(const std::vector<std::pair<uint64_t, uint8_t>> &blocks) {
    for (const auto &[key, value] : blocks) {
        const int x = static_cast<int>((key >> 42) & 0x1FFFFF) - (1 << 20);
        const int y = static_cast<int>((key >> 21) & 0x1FFFFF) - (1 << 20);
        const int z = static_cast<int>(key & 0x1FFFFF) - (1 << 20);
        if (!isInside(x, y, z)) { continue; }
        m_blocks[voxelIndex(x, y, z)] = value;
        markDirtyAround(x, y, z);
    }
    rebuildDirtyChunks();
}
