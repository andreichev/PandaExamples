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

// Генератор меша чанка. Природные воксели (земля/камень/песок) рендерятся гладкой
// поверхностью marching cubes, рукотворные (доски/кирпичи/брёвна/листья) — кубами,
// tall grass — крестом. См. docs/technical_plan.md §3.
class TerrainMeshGenerator {
public:
    static ChunkMeshBuildResult
    makeOneChunkMesh(const ChunkMeshSnapshot &snapshot, bool ambientOcclusion);
    static void addFaceIndices(uint32_t offset, std::vector<uint32_t> &indices);
    static bool isAir(int x, int y, int z, const ChunkMeshSnapshot &snapshot);

    // Природные типы образуют гладкий рельеф; рукотворные остаются кубами.
    static bool isNaturalType(VoxelType type);

private:
    static void addMarchingCubesMesh(
        const ChunkMeshSnapshot &snapshot, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices
    );

    static constexpr float ambientOcclusionFactor = 0.2f;
};
