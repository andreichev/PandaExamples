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
