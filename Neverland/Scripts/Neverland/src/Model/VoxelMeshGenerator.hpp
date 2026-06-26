//
// Created by Admin on 12.02.2022.
//

#pragma once

#include "ChunkMeshSnapshot.hpp"

#include <Bamboo/Assets/MeshAPI.hpp>

struct ChunkMeshBuildResult {
    ChunkCoord coord;
    uint32_t version = 0;
    MeshData meshData;
};

class VoxelMeshGenerator {
public:
    static ChunkMeshBuildResult
    makeOneChunkMesh(const ChunkMeshSnapshot &snapshot, bool ambientOcclusion);
    static void addFaceIndices(uint32_t offset, std::vector<uint32_t> &indices);
    static bool isAir(int x, int y, int z, const ChunkMeshSnapshot &snapshot);

private:
    static constexpr float ambientOcclusionFactor = 0.2f;
};
