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
    void updateTouchBlockInput(
        float deltaTime, bool &placePressed, bool &breakPressed, Vec2 &touchAim, bool &hasTouchAim
    );

    struct TouchActionState {
        int id = -1;
        float startX = 0.0f;
        float startY = 0.0f;
        float lastX = 0.0f;
        float lastY = 0.0f;
        float duration = 0.0f;
        float repeatTimer = 0.0f;
        bool active = false;
        bool moved = false;
    };

    Shared<PlayerController> m_playerController;
    TouchActionState m_touchAction;
    VoxelType m_selectedBlock;
};

REGISTER_SCRIPT(BlocksCreation)
