//
// Created by Admin on 12.02.2022.
//

#pragma once

#include "Model/ChunksStorage.hpp"
#include "PlayerController.hpp"

#include <Bamboo/Bamboo.hpp>

class BlocksCreation final : public Script {
public:
    const int MAXIMUM_DISTANCE = 100;

    void start() override;
    void update(float deltaTime) override;
    VoxelType getSelectedBlock() const {
        return m_selectedBlock;
    }
    void setSelectedBlock(VoxelType type);

private:
    Vec3 getPosition();
    void setVoxel(int x, int y, int z, VoxelType type);
    void updateChunk(const ChunkCoord &coord);
    void updateSelectedBlock();

    Shared<PlayerController> m_playerController;
    VoxelType m_selectedBlock;
};

REGISTER_SCRIPT(BlocksCreation)
