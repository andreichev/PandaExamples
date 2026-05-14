//
// Created by Admin on 12.02.2022.
//

#pragma once

#include "ChunksStorage.hpp"

#include <Bamboo/Assets/MeshAPI.hpp>

class VoxelMeshGenerator {
public:
    static MeshData makeOneChunkMesh(
        int chunkIndexX,
        int chunkIndexY,
        int chunkIndexZ,
        bool ambientOcclusion
    );
    static void addFaceIndices(uint32_t offset, std::vector<uint32_t>& indices);
    static bool isAir(int x, int y, int z, ChunksStorage &chunks);

private:
    static constexpr float ambientOcclusionFactor = 0.2f;
};
