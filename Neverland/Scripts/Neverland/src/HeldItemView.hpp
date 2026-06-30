#pragma once

#include "Model/Voxel.hpp"

#include <Bamboo/Bamboo.hpp>

using namespace Bamboo;

class HeldItemView final {
public:
    void update(
        EntityHandle cameraEntity, VoxelType selectedBlock, bool moving, bool sprinting, float deltaTime
    );
    void shutdown();

private:
    void ensureView();
    void updateMesh(VoxelType selectedBlock);
    void syncTransform(EntityHandle cameraEntity, bool moving, bool sprinting, float deltaTime);

    EntityHandle m_blockEntity;
    EntityHandle m_handEntity;
    MeshHandle m_blockMesh;
    MeshHandle m_handMesh;
    VoxelType m_displayedBlock = VoxelType::NOTHING;
    uint32_t m_appliedMaterialId = BAMBOO_INVALID_HANDLE;
    float m_time = 0.0f;
    float m_motionBlend = 0.0f;
    float m_sprintBlend = 0.0f;
};
