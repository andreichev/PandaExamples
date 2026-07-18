//
// Created by Admin on 12.02.2022.
//

#pragma once

#include <cstdint>

enum class VoxelType {
    NOTHING = 0,
    GRASS = 1,
    GROUND = 2,
    TREE = 3,
    BOARDS = 4,
    STONE = 5,
    STONE_BRICKS = 6,
    SAND_STONE = 7,
    SAND = 8,
    BIRCH_LOG = 9,
    BIRCH_LEAVES = 10,
    TALL_GRASS = 11,
    COUNT = 12
};

struct Voxel {
    VoxelType type;

    Voxel()
        : type(VoxelType::NOTHING) {}

    explicit Voxel(VoxelType type)
        : type(type) {}

    inline bool isAir() const {
        return type == VoxelType::NOTHING;
    }

    inline bool isSolid() const {
        return type != VoxelType::NOTHING && type != VoxelType::TALL_GRASS;
    }

    inline bool occludesFaces() const {
        return type != VoxelType::NOTHING && type != VoxelType::TALL_GRASS;
    }
};

// Природные типы — рельеф движкового Terrain3D; остальные твёрдые — постройки (BuildingGrid).
inline bool isNaturalVoxelType(VoxelType type) {
    switch (type) {
        case VoxelType::GRASS:
        case VoxelType::GROUND:
        case VoxelType::STONE:
        case VoxelType::SAND: return true;
        default: return false;
    }
}

// Маппинг природных типов на слои движкового terrain (веса r/g/b/a шейдера).
inline uint8_t terrainLayerFor(VoxelType type) {
    switch (type) {
        case VoxelType::GRASS: return 1;
        case VoxelType::GROUND: return 2;
        case VoxelType::STONE: return 3;
        case VoxelType::SAND: return 4;
        default: return 0;
    }
}

inline VoxelType voxelTypeForTerrainLayer(uint8_t layer) {
    switch (layer) {
        case 1: return VoxelType::GRASS;
        case 2: return VoxelType::GROUND;
        case 3: return VoxelType::STONE;
        case 4: return VoxelType::SAND;
        default: return VoxelType::NOTHING;
    }
}
