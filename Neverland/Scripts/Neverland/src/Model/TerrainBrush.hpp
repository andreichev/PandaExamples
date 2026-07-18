#pragma once

#include "Voxel.hpp"

#include <vector>

// Кисти редактирования рельефа — ЧИСТАЯ логика без применения: computeEdits возвращает
// список правок вокселей, вызывающий применяет их батчем и использует тот же список для
// подсветки-превью. Отдельно от скриптов — кандидат на перенос в редактор Panda.
enum class TerrainBrushMode {
    Sphere,  // насыпать/вырезать шар
    Raise,   // поднять/опустить поверхность в круге на один воксель
    Flatten, // выровнять поверхность круга по высоте точки прицела
};

struct TerrainBrushEdit {
    int x;
    int y;
    int z;
    VoxelType type;
};

namespace TerrainBrush {

// Правки действия кисти. secondary = ПКМ (обратное действие: вырезать/опустить).
// hit — воксель попадания луча, normal — нормаль грани попадания, paint — природный тип.
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
);

const char *modeName(TerrainBrushMode mode);

} // namespace TerrainBrush
