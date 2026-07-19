//
// Created by Michael Andreichev on 21.09.2023.
//

#pragma once

#include "Voxel.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

struct VoxelTextureData {
    VoxelType type;
    uint8_t sideTileIndex;
    uint32_t sideColor;
    uint8_t topTileIndex;
    uint32_t topColor;
    uint8_t bottomTileIndex;
    uint32_t bottomColor;

    VoxelTextureData(VoxelType type, uint8_t tileIndex)
        : type(type)
        , sideTileIndex(tileIndex)
        , sideColor(0xFFFFFFFF)
        , topTileIndex(tileIndex)
        , topColor(0xFFFFFFFF)
        , bottomTileIndex(tileIndex)
        , bottomColor(0xFFFFFFFF) {}
};

class VoxelTextureMapper final {
public:
    static VoxelTextureData &getTextureData(const Voxel *voxel) {
        VoxelTextureData &data = s_data[static_cast<std::size_t>(voxel->type)];
        return data;
    }

private:
    // Индекс тайла (row * grid + col): природные — в base_ground_1 (6×6),
    // рукотворные — в base_materials_1 (7×7). Текстуры цветные, тинтов нет.
    inline static std::array<VoxelTextureData, static_cast<std::size_t>(VoxelType::COUNT)> s_data =
        {
            // ВОЗДУХ
            VoxelTextureData(VoxelType::NOTHING, 0),
            // ТРАВА (ground 0,0)
            VoxelTextureData(VoxelType::GRASS, 0),
            // ЗЕМЛЯ (ground 4,0)
            VoxelTextureData(VoxelType::GROUND, 4),
            // ДЕРЕВО (materials: серая штукатурка 4,0)
            VoxelTextureData(VoxelType::TREE, 4),
            // ШТУКАТУРКА (materials 1,0)
            VoxelTextureData(VoxelType::BOARDS, 1),
            // КАМЕНЬ (ground 0,3 — каменистая земля)
            VoxelTextureData(VoxelType::STONE, 18),
            // КИРПИЧ (materials 2,4)
            VoxelTextureData(VoxelType::STONE_BRICKS, 30),
            // КАМЕННЫЙ БЛОК (materials 1,6)
            VoxelTextureData(VoxelType::SAND_STONE, 43),
            // ПЕСОК (ground 0,2)
            VoxelTextureData(VoxelType::SAND, 12),
            // БЕРЕЗОВЫЙ СТВОЛ (materials 3,0)
            VoxelTextureData(VoxelType::BIRCH_LOG, 3),
            // ЛИСТВА БЕРЕЗЫ (materials 6,0)
            VoxelTextureData(VoxelType::BIRCH_LEAVES, 6),
            // ВЫСОКАЯ ТРАВА (не рендерится)
            VoxelTextureData(VoxelType::TALL_GRASS, 0),
            // БЕЛАЯ ШТУКАТУРКА (materials 0,0)
            VoxelTextureData(VoxelType::WHITE_PLASTER, 0),
            // ТЕРРАКОТА (materials 0,4)
            VoxelTextureData(VoxelType::TERRACOTTA, 28),
            // ТЁМНЫЙ КИРПИЧ (materials 3,4)
            VoxelTextureData(VoxelType::DARK_BRICK, 31),
            // ТЁМНЫЙ КАМЕНЬ (materials 2,6)
            VoxelTextureData(VoxelType::DARK_STONE, 44),
            // СЛАНЕЦ (materials 5,0)
            VoxelTextureData(VoxelType::SLATE, 5)
    };
};
