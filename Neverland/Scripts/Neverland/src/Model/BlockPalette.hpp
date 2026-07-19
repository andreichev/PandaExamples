#pragma once

#include "BuildingCellGrid.hpp"
#include "Voxel.hpp"

#include <array>

// Палитра выбора игрока: строительные блоки и материалы терраформинга РАЗДЕЛЕНЫ
// (диздок): хотбар и меню строительства — только рукотворные, рельеф — своя секция.
namespace BlockPalette {

struct BlockEntry {
    VoxelType type;
    const char *name;
};

struct ElementEntry {
    ArchObjectType type;
    const char *name;
    const char *hint; // короткое описание для карточки меню
};

// Хотбар (клавиши 1-7) — быстрый доступ к строительным блокам.
constexpr std::array<BlockEntry, 7> BUILDING_HOTBAR = {
    BlockEntry{VoxelType::BOARDS, "Plaster"},
    BlockEntry{VoxelType::STONE_BRICKS, "Bricks"},
    BlockEntry{VoxelType::SAND_STONE, "Stone Block"},
    BlockEntry{VoxelType::WHITE_PLASTER, "White Plaster"},
    BlockEntry{VoxelType::TERRACOTTA, "Terracotta"},
    BlockEntry{VoxelType::DARK_BRICK, "Dark Brick"},
    BlockEntry{VoxelType::DARK_STONE, "Dark Stone"},
};

// Полный список строительных блоков (меню).
constexpr std::array<BlockEntry, 8> BUILDING_BLOCKS = {
    BlockEntry{VoxelType::BOARDS, "Plaster"},
    BlockEntry{VoxelType::STONE_BRICKS, "Bricks"},
    BlockEntry{VoxelType::SAND_STONE, "Stone Block"},
    BlockEntry{VoxelType::WHITE_PLASTER, "White Plaster"},
    BlockEntry{VoxelType::TERRACOTTA, "Terracotta"},
    BlockEntry{VoxelType::DARK_BRICK, "Dark Brick"},
    BlockEntry{VoxelType::DARK_STONE, "Dark Stone"},
    BlockEntry{VoxelType::SLATE, "Slate"},
};

// Многоклеточные элементы (меню, секция Elements). Материал — текущий строительный.
constexpr std::array<ElementEntry, 7> ELEMENTS = {
    ElementEntry{ArchObjectType::Block, "Basic", "1 x 1"},
    ElementEntry{ArchObjectType::Beam, "Beam", "3 x 1"},
    ElementEntry{ArchObjectType::Wall, "Wall", "1 x 3"},
    ElementEntry{ArchObjectType::Window, "Window", "1 x 3"},
    ElementEntry{ArchObjectType::Door, "Door", "1 x 3"},
    ElementEntry{ArchObjectType::Cornice, "Cornice", "1 x 1"},
    ElementEntry{ArchObjectType::Roof, "Roof", "area"},
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
