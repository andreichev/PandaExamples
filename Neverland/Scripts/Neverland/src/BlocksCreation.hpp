//
// Created by Admin on 12.02.2022.
//

#pragma once

#include "Model/ChunksStorage.hpp"
#include "CameraMove.hpp"

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
    void updateChunk(int chunkIndexX, int chunkIndexY, int chunkIndexZ);
    void updateSelectedBlock();

    Shared<CameraMove> m_cameraMove;
    VoxelType m_selectedBlock;
};

REGISTER_SCRIPT(BlocksCreation)
