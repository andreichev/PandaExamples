#pragma once

#include "Voxel.hpp"

#include <array>

// Палитра выбора игрока: строительные блоки и материалы терраформинга РАЗДЕЛЕНЫ
// (диздок): меню строительства — рукотворные, рельеф — своя секция.
namespace BlockPalette {

struct BlockEntry {
    VoxelType type;
    const char *name;
};

// Строительные кубы (раздел Blocks).
constexpr std::array<BlockEntry, 19> BUILDING_BLOCKS = {
    BlockEntry{VoxelType::BOARDS, "Plaster"},
    BlockEntry{VoxelType::STONE_BRICKS, "Bricks"},
    BlockEntry{VoxelType::SAND_STONE, "Stone Block"},
    BlockEntry{VoxelType::WHITE_PLASTER, "White Plaster"},
    BlockEntry{VoxelType::TERRACOTTA, "Terracotta"},
    BlockEntry{VoxelType::DARK_BRICK, "Dark Brick"},
    BlockEntry{VoxelType::DARK_STONE, "Dark Stone"},
    BlockEntry{VoxelType::SLATE, "Slate"},
    BlockEntry{VoxelType::MARBLE, "Marble"},
    BlockEntry{VoxelType::COLUMN, "Column"},
    BlockEntry{VoxelType::WOOD_PLANKS, "Wood"},
    BlockEntry{VoxelType::TIMBER_PLAIN, "Timber Plain"},
    BlockEntry{VoxelType::TIMBER_FRAME, "Timber Frame"},
    BlockEntry{VoxelType::TIMBER_CROSS, "Timber Cross"},
    BlockEntry{VoxelType::TIMBER_DIAG_L, "Timber Diag L"},
    BlockEntry{VoxelType::TIMBER_DIAG_R, "Timber Diag R"},
    BlockEntry{VoxelType::ROOF_TILES_RED, "Roof Red"},
    BlockEntry{VoxelType::ROOF_TILES_BROWN, "Roof Brown"},
    BlockEntry{VoxelType::ROOF_TILES_DARK, "Roof Dark"},
};

// Материалы рельефа (терраформинг кистью) — отдельная секция, с блоками не смешиваются.
constexpr std::array<BlockEntry, 4> TERRAIN_MATERIALS = {
    BlockEntry{VoxelType::GRASS, "Grass"},
    BlockEntry{VoxelType::GROUND, "Dirt"},
    BlockEntry{VoxelType::STONE, "Stone"},
    BlockEntry{VoxelType::SAND, "Sand"},
};

inline const char *nameFor(VoxelType type) {
    for (const BlockEntry &entry : BUILDING_BLOCKS) {
        if (entry.type == type) { return entry.name; }
    }
    for (const BlockEntry &entry : TERRAIN_MATERIALS) {
        if (entry.type == type) { return entry.name; }
    }
    return "";
}

} // namespace BlockPalette
