#include "TerrainBrush.hpp"
#include "ChunksStorage.hpp"
#include "GameContext.hpp"
#include "TerrainMeshGenerator.hpp"

#include <algorithm>
#include <cmath>

namespace {

bool isNaturalAt(int x, int y, int z) {
    if (GameContext::s_chunkStorage == nullptr) { return false; }
    const Voxel *voxel = GameContext::s_chunkStorage->getVoxel(x, y, z);
    return voxel != nullptr && TerrainMeshGenerator::isNaturalType(voxel->type);
}

bool isAirAt(int x, int y, int z) {
    if (GameContext::s_chunkStorage == nullptr) { return false; }
    const Voxel *voxel = GameContext::s_chunkStorage->getVoxel(x, y, z);
    return voxel != nullptr && voxel->isAir();
}

// Верхний природный воксель столбца в вертикальном окне; false — столбец пуст в окне.
bool findSurfaceY(int x, int z, int windowBottom, int windowTop, int &outY) {
    for (int y = std::min(windowTop, ChunksStorage::WORLD_MAX_Y - 1);
         y >= std::max(windowBottom, ChunksStorage::WORLD_MIN_Y);
         y--) {
        if (isNaturalAt(x, y, z)) {
            outY = y;
            return true;
        }
    }
    return false;
}

void computeSphere(
    bool secondary, int centerX, int centerY, int centerZ, float radius, VoxelType paint,
    std::vector<TerrainBrushEdit> &outEdits
) {
    const int extent = std::max(1, static_cast<int>(std::ceil(radius)));
    for (int dy = -extent; dy <= extent; dy++) {
        for (int dx = -extent; dx <= extent; dx++) {
            for (int dz = -extent; dz <= extent; dz++) {
                if (dx * dx + dy * dy + dz * dz > radius * radius) { continue; }
                const int x = centerX + dx;
                const int y = centerY + dy;
                const int z = centerZ + dz;
                if (!ChunksStorage::isWorldCoordInBounds(x, y, z)) { continue; }
                if (secondary) {
                    // Вырез: только природный рельеф (постройки кисть не трогает).
                    if (isNaturalAt(x, y, z)) { outEdits.push_back({x, y, z, VoxelType::NOTHING}); }
                } else {
                    if (isAirAt(x, y, z)) { outEdits.push_back({x, y, z, paint}); }
                }
            }
        }
    }
}

void computeRaise(
    bool secondary, int hitX, int hitY, int hitZ, float radius, VoxelType paint,
    std::vector<TerrainBrushEdit> &outEdits
) {
    const int extent = std::max(0, static_cast<int>(std::ceil(radius)));
    const int windowBottom = hitY - extent - 4;
    const int windowTop = hitY + extent + 4;
    for (int dx = -extent; dx <= extent; dx++) {
        for (int dz = -extent; dz <= extent; dz++) {
            if (dx * dx + dz * dz > radius * radius) { continue; }
            const int x = hitX + dx;
            const int z = hitZ + dz;
            int surfaceY;
            if (!findSurfaceY(x, z, windowBottom, windowTop, surfaceY)) { continue; }
            if (secondary) {
                outEdits.push_back({x, surfaceY, z, VoxelType::NOTHING}); // опустить столбец
            } else if (surfaceY + 1 < ChunksStorage::WORLD_MAX_Y && isAirAt(x, surfaceY + 1, z)) {
                outEdits.push_back({x, surfaceY + 1, z, paint}); // нарастить столбец
            }
        }
    }
}

void computeFlatten(
    int hitX, int hitY, int hitZ, float radius, VoxelType paint, std::vector<TerrainBrushEdit> &outEdits
) {
    const int extent = std::max(0, static_cast<int>(std::ceil(radius)));
    const int windowBottom = hitY - extent - 6;
    const int windowTop = hitY + extent + 6;
    int targetY;
    if (!findSurfaceY(hitX, hitZ, windowBottom, windowTop, targetY)) { return; }

    for (int dx = -extent; dx <= extent; dx++) {
        for (int dz = -extent; dz <= extent; dz++) {
            if (dx * dx + dz * dz > radius * radius) { continue; }
            const int x = hitX + dx;
            const int z = hitZ + dz;
            int surfaceY;
            if (!findSurfaceY(x, z, windowBottom, windowTop, surfaceY)) { continue; }
            for (int y = targetY + 1; y <= surfaceY; y++) { // срезать выше цели
                if (isNaturalAt(x, y, z)) { outEdits.push_back({x, y, z, VoxelType::NOTHING}); }
            }
            for (int y = surfaceY + 1; y <= targetY; y++) { // досыпать ниже цели
                if (isAirAt(x, y, z)) { outEdits.push_back({x, y, z, paint}); }
            }
        }
    }
}

} // namespace

namespace TerrainBrush {

void computeEdits(
    TerrainBrushMode mode,
    bool secondary,
    int hitX,
    int hitY,
    int hitZ,
    int normalX,
    int normalY,
    int normalZ,
    float radius,
    VoxelType paint,
    std::vector<TerrainBrushEdit> &outEdits
) {
    outEdits.clear();
    switch (mode) {
        case TerrainBrushMode::Sphere: {
            // Насыпаем от соседнего воксела грани, вырезаем от воксела попадания.
            if (secondary) {
                computeSphere(true, hitX, hitY, hitZ, radius, paint, outEdits);
            } else {
                computeSphere(
                    false, hitX + normalX, hitY + normalY, hitZ + normalZ, radius, paint, outEdits
                );
            }
            break;
        }
        case TerrainBrushMode::Raise:
            computeRaise(secondary, hitX, hitY, hitZ, radius, paint, outEdits);
            break;
        case TerrainBrushMode::Flatten:
            computeFlatten(hitX, hitY, hitZ, radius, paint, outEdits);
            break;
    }
}

const char *modeName(TerrainBrushMode mode) {
    switch (mode) {
        case TerrainBrushMode::Sphere: return "Sphere";
        case TerrainBrushMode::Raise: return "Raise";
        case TerrainBrushMode::Flatten: return "Flatten";
    }
    return "";
}

} // namespace TerrainBrush
