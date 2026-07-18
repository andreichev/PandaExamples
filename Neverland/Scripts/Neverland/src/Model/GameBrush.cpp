#include "GameBrush.hpp"

#include <algorithm>
#include <cmath>

namespace {

// Окно рельефа + гейт построек: насыпать можно только в настоящий воздух.
struct BrushContext {
    TerrainAccess::Window window;
    const BuildingGrid *buildings = nullptr;

    bool isNatural(int x, int y, int z) const {
        return window.layerAt(x, y, z) != 0;
    }
    bool isAir(int x, int y, int z) const {
        return window.layerAt(x, y, z) == 0 && !buildings->isSolidAt(x, y, z);
    }
};

// Верхний природный воксель столбца в вертикальном окне; false — столбец пуст в окне.
bool findSurfaceY(const BrushContext &context, int x, int z, int windowBottom, int windowTop, int &outY) {
    for (int y = windowTop; y >= windowBottom; y--) {
        if (context.isNatural(x, y, z)) {
            outY = y;
            return true;
        }
    }
    return false;
}

void computeSphere(
    const BrushContext &context, bool secondary, int centerX, int centerY, int centerZ, float radius,
    VoxelType paint, std::vector<TerrainAccess::Edit> &outEdits
) {
    const int extent = std::max(1, static_cast<int>(std::ceil(radius)));
    for (int dy = -extent; dy <= extent; dy++) {
        for (int dx = -extent; dx <= extent; dx++) {
            for (int dz = -extent; dz <= extent; dz++) {
                if (dx * dx + dy * dy + dz * dz > radius * radius) { continue; }
                const int x = centerX + dx;
                const int y = centerY + dy;
                const int z = centerZ + dz;
                if (secondary) {
                    // Вырез: только природный рельеф (постройки кисть не трогает).
                    if (context.isNatural(x, y, z)) {
                        outEdits.push_back({x, y, z, VoxelType::NOTHING});
                    }
                } else {
                    if (context.isAir(x, y, z)) { outEdits.push_back({x, y, z, paint}); }
                }
            }
        }
    }
}

void computeRaise(
    const BrushContext &context, bool secondary, int hitX, int hitY, int hitZ, float radius,
    VoxelType paint, std::vector<TerrainAccess::Edit> &outEdits
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
            if (!findSurfaceY(context, x, z, windowBottom, windowTop, surfaceY)) { continue; }
            if (secondary) {
                outEdits.push_back({x, surfaceY, z, VoxelType::NOTHING}); // опустить столбец
            } else if (context.isAir(x, surfaceY + 1, z)) {
                outEdits.push_back({x, surfaceY + 1, z, paint}); // нарастить столбец
            }
        }
    }
}

void computeFlatten(
    const BrushContext &context, int hitX, int hitY, int hitZ, float radius, VoxelType paint,
    std::vector<TerrainAccess::Edit> &outEdits
) {
    const int extent = std::max(0, static_cast<int>(std::ceil(radius)));
    const int windowBottom = hitY - extent - 6;
    const int windowTop = hitY + extent + 6;
    int targetY;
    if (!findSurfaceY(context, hitX, hitZ, windowBottom, windowTop, targetY)) { return; }

    for (int dx = -extent; dx <= extent; dx++) {
        for (int dz = -extent; dz <= extent; dz++) {
            if (dx * dx + dz * dz > radius * radius) { continue; }
            const int x = hitX + dx;
            const int z = hitZ + dz;
            int surfaceY;
            if (!findSurfaceY(context, x, z, windowBottom, windowTop, surfaceY)) { continue; }
            for (int y = targetY + 1; y <= surfaceY; y++) { // срезать выше цели
                if (context.isNatural(x, y, z)) { outEdits.push_back({x, y, z, VoxelType::NOTHING}); }
            }
            for (int y = surfaceY + 1; y <= targetY; y++) { // досыпать ниже цели
                if (context.isAir(x, y, z)) { outEdits.push_back({x, y, z, paint}); }
            }
        }
    }
}

} // namespace

namespace GameBrush {

void computeEdits(
    GameBrushMode mode,
    bool secondary,
    int hitX,
    int hitY,
    int hitZ,
    int normalX,
    int normalY,
    int normalZ,
    float radius,
    VoxelType paint,
    const BuildingGrid &buildings,
    std::vector<TerrainAccess::Edit> &outEdits
) {
    outEdits.clear();
    // Одно окно на весь мазок кисти: правки/поверхности читаются из него без ABI-обращений.
    const int extent = std::max(1, static_cast<int>(std::ceil(radius))) + 8;
    BrushContext context;
    context.window = TerrainAccess::readWindow(
        hitX - extent, hitY - extent, hitZ - extent, hitX + extent, hitY + extent, hitZ + extent
    );
    context.buildings = &buildings;

    switch (mode) {
        case GameBrushMode::Sphere: {
            // Насыпаем от соседнего воксела грани, вырезаем от воксела попадания.
            if (secondary) {
                computeSphere(context, true, hitX, hitY, hitZ, radius, paint, outEdits);
            } else {
                computeSphere(
                    context, false, hitX + normalX, hitY + normalY, hitZ + normalZ, radius, paint,
                    outEdits
                );
            }
            break;
        }
        case GameBrushMode::Raise:
            computeRaise(context, secondary, hitX, hitY, hitZ, radius, paint, outEdits);
            break;
        case GameBrushMode::Flatten:
            computeFlatten(context, hitX, hitY, hitZ, radius, paint, outEdits);
            break;
    }
}

const char *modeName(GameBrushMode mode) {
    switch (mode) {
        case GameBrushMode::Sphere: return "Sphere";
        case GameBrushMode::Raise: return "Raise";
        case GameBrushMode::Flatten: return "Flatten";
    }
    return "";
}

} // namespace GameBrush
