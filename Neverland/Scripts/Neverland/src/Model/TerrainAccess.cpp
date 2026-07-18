#include "TerrainAccess.hpp"

#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/WorldAPI.hpp>

#include <algorithm>
#include <cmath>

namespace {

EntityHandle s_entity = 0;
bool s_ready = false;
TerrainAPI::Info s_info;
// Позиция terrain-сущности в мире (целые воксельные координаты угла участка).
int s_offsetX = 0;
int s_offsetY = 0;
int s_offsetZ = 0;
std::unordered_map<uint64_t, uint8_t> s_editAccumulator;

} // namespace

bool TerrainAccess::Window::contains(int x, int y, int z) const {
    return x >= minX && x <= maxX && y >= minY && y <= maxY && z >= minZ && z <= maxZ;
}

uint8_t TerrainAccess::Window::layerAt(int x, int y, int z) const {
    if (!contains(x, y, z)) { return 0; }
    const size_t sizeX = static_cast<size_t>(maxX - minX + 1);
    const size_t sizeZ = static_cast<size_t>(maxZ - minZ + 1);
    return layers
        [(static_cast<size_t>(y - minY) * sizeZ + static_cast<size_t>(z - minZ)) * sizeX +
         static_cast<size_t>(x - minX)];
}

bool TerrainAccess::init() {
    s_editAccumulator.clear();
    s_entity = WorldAPI::findByTag("Terrain");
    s_ready = false;
    if (!s_entity.isValid()) {
        LOG_ERROR("TerrainAccess: entity 'Terrain' not found");
        return false;
    }
    if (!TerrainAPI::getInfo(s_entity, s_info) || s_info.kind != TerrainAPI::Kind::Voxels3D) {
        LOG_ERROR("TerrainAccess: 'Terrain' has no voxel terrain asset");
        return false;
    }
    const Vec3 position = TransformComponentAPI::getPosition(s_entity);
    s_offsetX = static_cast<int>(std::lround(position.x));
    s_offsetY = static_cast<int>(std::lround(position.y));
    s_offsetZ = static_cast<int>(std::lround(position.z));
    s_ready = true;
    LOG_INFO(
        "TerrainAccess: %ux%ux%u at (%d, %d, %d)", s_info.cellsX, s_info.cellsY, s_info.cellsZ,
        s_offsetX, s_offsetY, s_offsetZ
    );
    return true;
}

void TerrainAccess::deinit() {
    s_entity = 0;
    s_ready = false;
    s_editAccumulator.clear();
}

bool TerrainAccess::isReady() {
    return s_ready;
}

EntityHandle TerrainAccess::entity() {
    return s_entity;
}

int TerrainAccess::worldMinX() {
    return s_offsetX;
}
int TerrainAccess::worldMinZ() {
    return s_offsetZ;
}
int TerrainAccess::worldMaxX() {
    return s_offsetX + static_cast<int>(s_info.cellsX);
}
int TerrainAccess::worldMaxZ() {
    return s_offsetZ + static_cast<int>(s_info.cellsZ);
}
int TerrainAccess::worldMaxY() {
    return s_offsetY + static_cast<int>(s_info.cellsY);
}

bool TerrainAccess::isInsideXZ(int x, int z) {
    return x >= worldMinX() && x < worldMaxX() && z >= worldMinZ() && z < worldMaxZ();
}

TerrainAccess::Window
TerrainAccess::readWindow(int minX, int minY, int minZ, int maxX, int maxY, int maxZ) {
    Window window;
    window.minX = minX;
    window.minY = minY;
    window.minZ = minZ;
    window.maxX = maxX;
    window.maxY = maxY;
    window.maxZ = maxZ;
    const size_t count = static_cast<size_t>(maxX - minX + 1) * (maxY - minY + 1) * (maxZ - minZ + 1);
    window.layers.assign(count, 0);
    if (!s_ready || maxX < minX || maxY < minY || maxZ < minZ) { return window; }

    // Пересечение запроса с участком (локаль ассета); вне участка остаются нули.
    const int localMinX = std::max(minX - s_offsetX, 0);
    const int localMinY = std::max(minY - s_offsetY, 0);
    const int localMinZ = std::max(minZ - s_offsetZ, 0);
    const int localMaxX = std::min(maxX - s_offsetX, static_cast<int>(s_info.cellsX) - 1);
    const int localMaxY = std::min(maxY - s_offsetY, static_cast<int>(s_info.cellsY) - 1);
    const int localMaxZ = std::min(maxZ - s_offsetZ, static_cast<int>(s_info.cellsZ) - 1);
    if (localMaxX < localMinX || localMaxY < localMinY || localMaxZ < localMinZ) { return window; }

    std::vector<uint8_t> localLayers;
    if (!TerrainAPI::getVoxels(
            s_entity, localMinX, localMinY, localMinZ, localMaxX, localMaxY, localMaxZ, localLayers
        )) {
        return window;
    }
    const size_t sizeX = static_cast<size_t>(maxX - minX + 1);
    const size_t sizeZ = static_cast<size_t>(maxZ - minZ + 1);
    const size_t localSizeX = static_cast<size_t>(localMaxX - localMinX + 1);
    const size_t localSizeZ = static_cast<size_t>(localMaxZ - localMinZ + 1);
    for (int y = localMinY; y <= localMaxY; y++) {
        for (int z = localMinZ; z <= localMaxZ; z++) {
            for (int x = localMinX; x <= localMaxX; x++) {
                const size_t localIndex =
                    (static_cast<size_t>(y - localMinY) * localSizeZ + (z - localMinZ)) * localSizeX +
                    (x - localMinX);
                const int worldX = x + s_offsetX;
                const int worldY = y + s_offsetY;
                const int worldZ = z + s_offsetZ;
                const size_t index =
                    (static_cast<size_t>(worldY - minY) * sizeZ + (worldZ - minZ)) * sizeX +
                    (worldX - minX);
                window.layers[index] = localLayers[localIndex];
            }
        }
    }
    return window;
}

void TerrainAccess::applyEdits(const std::vector<Edit> &edits) {
    if (!s_ready || edits.empty()) { return; }
    std::vector<TerrainAPI::VoxelEdit> localEdits;
    localEdits.reserve(edits.size());
    for (const Edit &edit : edits) {
        const int localX = edit.x - s_offsetX;
        const int localY = edit.y - s_offsetY;
        const int localZ = edit.z - s_offsetZ;
        if (localX < 0 || localY < 0 || localZ < 0 || localX >= static_cast<int>(s_info.cellsX) ||
            localY >= static_cast<int>(s_info.cellsY) || localZ >= static_cast<int>(s_info.cellsZ)) {
            continue;
        }
        const uint8_t layer = terrainLayerFor(edit.type);
        localEdits.push_back({localX, localY, localZ, layer});
        s_editAccumulator[packWorldVoxel(edit.x, edit.y, edit.z)] = layer;
    }
    if (localEdits.empty()) { return; }
    TerrainAPI::setVoxels(s_entity, localEdits.data(), static_cast<uint32_t>(localEdits.size()));
}

bool TerrainAccess::sampleSurface(float x, float z, float &outHeight, Vec3 &outNormal) {
    if (!s_ready) { return false; }
    return TerrainAPI::sampleSurface(s_entity, x, z, outHeight, outNormal);
}

bool TerrainAccess::raycast(
    Vec3 origin, Vec3 direction, float maxDistance, Vec3 &outPoint, Vec3 &outNormal
) {
    if (!s_ready) { return false; }
    return TerrainAPI::raycast(s_entity, origin, direction, maxDistance, outPoint, outNormal);
}

const std::unordered_map<uint64_t, uint8_t> &TerrainAccess::editAccumulator() {
    return s_editAccumulator;
}

void TerrainAccess::restoreEdits(const std::unordered_map<uint64_t, uint8_t> &edits) {
    if (!s_ready) { return; }
    std::vector<TerrainAPI::VoxelEdit> localEdits;
    localEdits.reserve(edits.size());
    for (const auto &[key, layer] : edits) {
        int x, y, z;
        unpackWorldVoxel(key, x, y, z);
        const int localX = x - s_offsetX;
        const int localY = y - s_offsetY;
        const int localZ = z - s_offsetZ;
        if (localX < 0 || localY < 0 || localZ < 0 || localX >= static_cast<int>(s_info.cellsX) ||
            localY >= static_cast<int>(s_info.cellsY) || localZ >= static_cast<int>(s_info.cellsZ)) {
            continue;
        }
        localEdits.push_back({localX, localY, localZ, layer});
    }
    s_editAccumulator = edits;
    if (localEdits.empty()) { return; }
    TerrainAPI::setVoxels(s_entity, localEdits.data(), static_cast<uint32_t>(localEdits.size()));
}

uint64_t TerrainAccess::packWorldVoxel(int x, int y, int z) {
    // Смещение +2^20 переводит в положительные; по 21 биту на ось.
    const uint64_t bx = static_cast<uint64_t>(x + (1 << 20)) & 0x1FFFFF;
    const uint64_t by = static_cast<uint64_t>(y + (1 << 20)) & 0x1FFFFF;
    const uint64_t bz = static_cast<uint64_t>(z + (1 << 20)) & 0x1FFFFF;
    return (bx << 42) | (by << 21) | bz;
}

void TerrainAccess::unpackWorldVoxel(uint64_t key, int &x, int &y, int &z) {
    x = static_cast<int>((key >> 42) & 0x1FFFFF) - (1 << 20);
    y = static_cast<int>((key >> 21) & 0x1FFFFF) - (1 << 20);
    z = static_cast<int>(key & 0x1FFFFF) - (1 << 20);
}
