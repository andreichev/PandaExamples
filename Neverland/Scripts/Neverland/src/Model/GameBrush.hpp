#pragma once

#include "BuildingGrid.hpp"
#include "TerrainAccess.hpp"

#include <vector>

// Кисти терраформинга игрока — ЧИСТАЯ логика: computeEdits возвращает список правок,
// вызывающий применяет их батчем (TerrainAccess) и использует тот же список для
// превью-подсветки. Читает рельеф из окна TerrainAccess, постройки не трогает.
enum class GameBrushMode {
    Sphere,  // насыпать/вырезать шар
    Raise,   // поднять/опустить поверхность в круге на один воксель
    Flatten, // выровнять поверхность круга по высоте столбца прицела
};

namespace GameBrush {

// Правки действия кисти. secondary = ПКМ (обратное действие: вырезать/опустить).
// hit — воксель попадания луча (мировой), normal — нормаль грани, paint — природный тип.
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
);

const char *modeName(GameBrushMode mode);

} // namespace GameBrush
