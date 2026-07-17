//
// Created by Admin on 12.02.2022.
//

#include "TerrainMeshGenerator.hpp"
#include "MarchingCubesTables.hpp"
#include "VoxelTextureMapper.hpp"

#include <cmath>

// Атласы: base_ground_1 — 6×6 (рельеф), base_materials_1 — 7×7 (кубы). Индекс тайла = row * grid + col.
constexpr int GROUND_ATLAS_GRID = 6;
constexpr int BLOCKS_ATLAS_GRID = 7;
constexpr uint8_t GRASS_GROUND_TILE = 0; // единая трава всей поверхности рельефа

Vec2 getUV(uint8_t tileIndex, int atlasGrid) {
    const float uvSize = 1.f / atlasGrid;
    float u = tileIndex % atlasGrid * uvSize;
    float v = tileIndex / atlasGrid * uvSize;
    // Небольшой сдвиг от краев текстуры для того,
    // чтобы при mipmapping не было сливания с соседними текстурами
    u += 0.0005f;
    v += 0.0005f;
    return {u, v};
}

Color toColor(uint32_t hex) {
    Color color;
    uint8_t r = hex >> 24;
    uint8_t g = hex >> 16;
    uint8_t b = hex >> 8;
    uint8_t a = hex >> 0;
    color.r = (r) / 255.f;
    color.g = (g) / 255.f;
    color.b = (b) / 255.f;
    color.a = (a) / 255.f;
    return color;
}

ChunkMeshBuildResult
TerrainMeshGenerator::makeOneChunkMesh(const ChunkMeshSnapshot &snapshot, bool ambientOcclusion) {
    std::vector<Vertex> vertices; // рукотворные кубы (blocksMesh)
    std::vector<uint32_t> indices;
    std::vector<Vertex> terrainVertices;
    std::vector<uint32_t> terrainIndices;
    for (int voxelIndexX = 0; voxelIndexX < Chunk::SIZE_X; voxelIndexX++) {
        for (int voxelIndexY = 0; voxelIndexY < Chunk::SIZE_Y; voxelIndexY++) {
            for (int voxelIndexZ = 0; voxelIndexZ < Chunk::SIZE_Z; voxelIndexZ++) {
                int x = voxelIndexX + snapshot.coord.x * Chunk::SIZE_X;
                int y = voxelIndexY + snapshot.coord.y * Chunk::SIZE_Y;
                int z = voxelIndexZ + snapshot.coord.z * Chunk::SIZE_Z;

                const Voxel *currentVoxel = snapshot.getPadded(
                    voxelIndexX + ChunkMeshSnapshot::PADDING,
                    voxelIndexY + ChunkMeshSnapshot::PADDING,
                    voxelIndexZ + ChunkMeshSnapshot::PADDING
                );
                if (currentVoxel == nullptr || currentVoxel->isAir()) { continue; }
                VoxelTextureData &textureData = VoxelTextureMapper::getTextureData(currentVoxel);

                float uvSize = 1.f / BLOCKS_ATLAS_GRID;
                uvSize -= 0.001f;

                if (currentVoxel->type == VoxelType::TALL_GRASS) {
                    continue; // декор старого атласа удалён
                }
                if (TerrainMeshGenerator::isNaturalType(currentVoxel->type)) {
                    continue; // природный рельеф строится гладкой поверхностью (см. addMarchingCubesMesh)
                }

                float light;
                float a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0, h = 0;

                // Front
                if (isAir(x, y, z + 1, snapshot)) {
                    addFaceIndices(static_cast<uint32_t>(vertices.size()), indices);
                    const Vec3 normal(0.f, 0.f, 1.f);
                    light = 1.0f;
                    if (ambientOcclusion) {
                        // top
                        a = (isAir(x, y + 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // bottom
                        b = (isAir(x, y - 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // left
                        c = (isAir(x + 1, y, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // right
                        d = (isAir(x - 1, y, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // top left
                        e = (isAir(x + 1, y + 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // top right
                        f = (isAir(x - 1, y + 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // bottom left
                        g = (isAir(x + 1, y - 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // bottom right
                        h = (isAir(x - 1, y - 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                    }
                    Vec2 uv = getUV(textureData.sideTileIndex, BLOCKS_ATLAS_GRID);
                    vertices.emplace_back(Vertex(
                        Vec3(x, y, z + 1.0f),
                        Vec2(uv.x, uv.y + uvSize),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - b - d - h)
                    ));
                    vertices.emplace_back(Vertex(
                        Vec3(x + 1.0f, y, z + 1.0f),
                        Vec2(uv.x + uvSize, uv.y + uvSize),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - b - c - g)
                    )); // 1
                    vertices.emplace_back(Vertex(
                        Vec3(x + 1.0f, y + 1.0f, z + 1.0f),
                        Vec2(uv.x + uvSize, uv.y),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - a - c - e)
                    )); // 2
                    vertices.emplace_back(Vertex(
                        Vec3(x, y + 1.0f, z + 1.0f),
                        Vec2(uv.x, uv.y),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - a - d - f)
                    )); // 3
                }
                // Back
                if (isAir(x, y, z - 1, snapshot)) {
                    addFaceIndices(static_cast<uint32_t>(vertices.size()), indices);
                    const Vec3 normal(0.f, 0.f, -1.f);
                    light = 0.75f;
                    if (ambientOcclusion) {
                        // top
                        a = (isAir(x, y + 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // bottom
                        b = (isAir(x, y - 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // right
                        c = (isAir(x - 1, y, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // left
                        d = (isAir(x + 1, y, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // top right
                        e = (isAir(x - 1, y + 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // bottom right
                        f = (isAir(x - 1, y - 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // top left
                        g = (isAir(x + 1, y + 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // bottom left
                        h = (isAir(x + 1, y - 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                    }
                    Vec2 uv = getUV(textureData.sideTileIndex, BLOCKS_ATLAS_GRID);
                    vertices.emplace_back(Vertex(
                        Vec3(x, y, z),
                        Vec2(uv.x + uvSize, uv.y + uvSize),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - b - c - f)
                    )); // 4
                    vertices.emplace_back(Vertex(
                        Vec3(x, y + 1.0f, z),
                        Vec2(uv.x + uvSize, uv.y),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - a - c - e)
                    )); // 5
                    vertices.emplace_back(Vertex(
                        Vec3(x + 1.0f, y + 1.0f, z),
                        Vec2(uv.x, uv.y),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - a - d - g)
                    )); // 6
                    vertices.emplace_back(Vertex(
                        Vec3(x + 1.0f, y, z),
                        Vec2(uv.x, uv.y + uvSize),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - b - d - h)
                    )); // 7
                }
                // Top
                if (isAir(x, y + 1, z, snapshot)) {
                    addFaceIndices(static_cast<uint32_t>(vertices.size()), indices);
                    const Vec3 normal(0.f, 1.f, 0.f);
                    light = 0.95f;
                    if (ambientOcclusion) {
                        // left
                        a = (isAir(x + 1, y + 1, z, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // right
                        b = (isAir(x - 1, y + 1, z, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // back
                        c = (isAir(x, y + 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // front
                        d = (isAir(x, y + 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // left back
                        e = (isAir(x + 1, y + 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // left front
                        f = (isAir(x + 1, y + 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // right back
                        g = (isAir(x - 1, y + 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // right front
                        h = (isAir(x - 1, y + 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                    }
                    Vec2 uv = getUV(textureData.topTileIndex, BLOCKS_ATLAS_GRID);
                    vertices.emplace_back(Vertex(
                        Vec3(x, y + 1.0f, z),
                        Vec2(uv.x, uv.y + uvSize),
                        normal,
                        toColor(textureData.topColor),
                        light * (1.f - b - d - h)
                    )); // 8
                    vertices.emplace_back(Vertex(
                        Vec3(x, y + 1.0f, z + 1.0f),
                        Vec2(uv.x + uvSize, uv.y + uvSize),
                        normal,
                        toColor(textureData.topColor),
                        light * (1.f - b - c - g)
                    )); // 11
                    vertices.emplace_back(Vertex(
                        Vec3(x + 1.0f, y + 1.0f, z + 1.0f),
                        Vec2(uv.x + uvSize, uv.y),
                        normal,
                        toColor(textureData.topColor),
                        light * (1.f - a - c - e)
                    )); // 10
                    vertices.emplace_back(Vertex(
                        Vec3(x + 1.0f, y + 1.0f, z),
                        Vec2(uv.x, uv.y),
                        normal,
                        toColor(textureData.topColor),
                        light * (1.f - a - d - f)
                    )); // 9
                }
                // Bottom
                if (isAir(x, y - 1, z, snapshot)) {
                    addFaceIndices(static_cast<uint32_t>(vertices.size()), indices);
                    const Vec3 normal(0.f, -1.f, 0.f);
                    light = 0.85f;
                    if (ambientOcclusion) {
                        // left
                        a = (isAir(x + 1, y - 1, z, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // right
                        b = (isAir(x - 1, y - 1, z, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // back
                        c = (isAir(x, y - 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // front
                        d = (isAir(x, y - 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // left back
                        e = (isAir(x + 1, y - 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // left front
                        f = (isAir(x + 1, y - 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // right back
                        g = (isAir(x - 1, y - 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // right front
                        h = (isAir(x - 1, y - 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                    }
                    Vec2 uv = getUV(textureData.bottomTileIndex, BLOCKS_ATLAS_GRID);
                    vertices.emplace_back(Vertex(
                        Vec3(x, y, z),
                        Vec2(uv.x, uv.y + uvSize),
                        normal,
                        toColor(textureData.bottomColor),
                        light * (1.f - b - d - h)
                    )); // 12
                    vertices.emplace_back(Vertex(
                        Vec3(x + 1.0f, y, z),
                        Vec2(uv.x + uvSize, uv.y + uvSize),
                        normal,
                        toColor(textureData.bottomColor),
                        light * (1.f - a - d - f)
                    )); // 13
                    vertices.emplace_back(Vertex(
                        Vec3(x + 1.0f, y, z + 1.0f),
                        Vec2(uv.x + uvSize, uv.y),
                        normal,
                        toColor(textureData.bottomColor),
                        light * (1.f - a - c - e)
                    )); // 14
                    vertices.emplace_back(Vertex(
                        Vec3(x, y, z + 1.0f),
                        Vec2(uv.x, uv.y),
                        normal,
                        toColor(textureData.bottomColor),
                        light * (1.f - b - c - g)
                    )); // 15
                }
                // Right
                if (isAir(x - 1, y, z, snapshot)) {
                    addFaceIndices(static_cast<uint32_t>(vertices.size()), indices);
                    const Vec3 normal(-1.f, 0.f, 0.f);
                    light = 0.9f;
                    if (ambientOcclusion) {
                        // top
                        a = (isAir(x - 1, y + 1, z, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // bottom
                        b = (isAir(x - 1, y - 1, z, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // front
                        c = (isAir(x - 1, y, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // back
                        d = (isAir(x - 1, y, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // top front
                        e = (isAir(x - 1, y + 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // bottom front
                        f = (isAir(x - 1, y - 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // top back
                        g = (isAir(x - 1, y + 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // bottom back
                        h = (isAir(x - 1, y - 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                    }
                    Vec2 uv = getUV(textureData.sideTileIndex, BLOCKS_ATLAS_GRID);
                    vertices.emplace_back(Vertex(
                        Vec3(x, y, z),
                        Vec2(uv.x, uv.y + uvSize),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - b - d - h)
                    )); // 16
                    vertices.emplace_back(Vertex(
                        Vec3(x, y, z + 1.0f),
                        Vec2(uv.x + uvSize, uv.y + uvSize),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - b - c - f)
                    )); // 17
                    vertices.emplace_back(Vertex(
                        Vec3(x, y + 1.0f, z + 1.0f),
                        Vec2(uv.x + uvSize, uv.y),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - a - c - e)
                    )); // 18
                    vertices.emplace_back(Vertex(
                        Vec3(x, y + 1.0f, z),
                        Vec2(uv.x, uv.y),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - a - d - g)
                    )); // 19
                }
                // Left
                if (isAir(x + 1, y, z, snapshot)) {
                    addFaceIndices(static_cast<uint32_t>(vertices.size()), indices);
                    const Vec3 normal(1.f, 0.f, 0.f);
                    light = 0.8f;
                    if (ambientOcclusion) {
                        // top
                        a = (isAir(x + 1, y + 1, z, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // bottom
                        b = (isAir(x + 1, y - 1, z, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // front
                        c = (isAir(x + 1, y, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // back
                        d = (isAir(x + 1, y, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // top front
                        e = (isAir(x + 1, y + 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // top back
                        f = (isAir(x + 1, y + 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // bottom front
                        g = (isAir(x + 1, y - 1, z + 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                        // bottom back
                        h = (isAir(x + 1, y - 1, z - 1, snapshot) ? 0.0f : 1.0f) *
                            ambientOcclusionFactor;
                    }
                    Vec2 uv = getUV(textureData.sideTileIndex, BLOCKS_ATLAS_GRID);
                    vertices.emplace_back(Vertex(
                        Vec3(x + 1.0f, y, z),
                        Vec2(uv.x + uvSize, uv.y + uvSize),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - b - d - h)
                    )); // 20
                    vertices.emplace_back(Vertex(
                        Vec3(x + 1.0f, y + 1.0f, z),
                        Vec2(uv.x + uvSize, uv.y),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - a - d - f)
                    )); // 23
                    vertices.emplace_back(Vertex(
                        Vec3(x + 1.0f, y + 1.0f, z + 1.0f),
                        Vec2(uv.x, uv.y),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - a - c - e)
                    )); // 22
                    vertices.emplace_back(Vertex(
                        Vec3(x + 1.0f, y, z + 1.0f),
                        Vec2(uv.x, uv.y + uvSize),
                        normal,
                        toColor(textureData.sideColor),
                        light * (1.f - b - c - g)
                    )); // 21
                }
            }
        }
    }
    addMarchingCubesMesh(snapshot, terrainVertices, terrainIndices);
    return ChunkMeshBuildResult{
        snapshot.coord, snapshot.version, MeshData(terrainVertices, terrainIndices), MeshData(vertices, indices)
    };
}

bool TerrainMeshGenerator::isNaturalType(VoxelType type) {
    switch (type) {
        case VoxelType::GRASS:
        case VoxelType::GROUND:
        case VoxelType::STONE:
        case VoxelType::SAND: return true;
        default: return false;
    }
}

namespace {

// Узловое поле marching cubes: узел (i,j,k) — угол воксела (i,j,k). density — доля твёрдых
// вокселей среди 8 смежных (рукотворные участвуют, чтобы земля прилегала к постройкам без
// щелей), naturalNear — есть ли среди них природный: ячейки без природного вклада
// пропускаются, иначе гладкая «кожа» накрывала бы дома из блоков.
struct NodeField {
    static constexpr int MARGIN = 1; // узлы за границей чанка — для градиентов на границе
    static constexpr int NX = Chunk::SIZE_X + 1 + MARGIN * 2;
    static constexpr int NY = Chunk::SIZE_Y + 1 + MARGIN * 2;
    static constexpr int NZ = Chunk::SIZE_Z + 1 + MARGIN * 2;

    std::vector<float> density;
    std::vector<uint8_t> naturalNear;

    static int index(int i, int j, int k) {
        return ((j + MARGIN) * NX + (i + MARGIN)) * NZ + (k + MARGIN);
    }

    float d(int i, int j, int k) const {
        return density[index(i, j, k)];
    }

    bool natural(int i, int j, int k) const {
        return naturalNear[index(i, j, k)] != 0;
    }

    Vec3 gradient(int i, int j, int k) const {
        return Vec3(
            (d(i + 1, j, k) - d(i - 1, j, k)) * 0.5f,
            (d(i, j + 1, k) - d(i, j - 1, k)) * 0.5f,
            (d(i, j, k + 1) - d(i, j, k - 1)) * 0.5f
        );
    }

    void fill(const ChunkMeshSnapshot &snapshot) {
        density.assign(NX * NY * NZ, 0.f);
        naturalNear.assign(NX * NY * NZ, 0);
        for (int j = -MARGIN; j <= Chunk::SIZE_Y + MARGIN; j++) {
            for (int i = -MARGIN; i <= Chunk::SIZE_X + MARGIN; i++) {
                for (int k = -MARGIN; k <= Chunk::SIZE_Z + MARGIN; k++) {
                    int solid = 0;
                    bool nat = false;
                    for (int dy = -1; dy <= 0; dy++) {
                        for (int dx = -1; dx <= 0; dx++) {
                            for (int dz = -1; dz <= 0; dz++) {
                                const Voxel *voxel = snapshot.getPadded(
                                    i + dx + ChunkMeshSnapshot::PADDING,
                                    j + dy + ChunkMeshSnapshot::PADDING,
                                    k + dz + ChunkMeshSnapshot::PADDING
                                );
                                if (voxel == nullptr) { continue; }
                                if (voxel->isSolid()) { solid++; }
                                if (TerrainMeshGenerator::isNaturalType(voxel->type)) { nat = true; }
                            }
                        }
                    }
                    density[index(i, j, k)] = solid / 8.f;
                    if (nat) { naturalNear[index(i, j, k)] = 1; }
                }
            }
        }
    }
};

void addFaceTriangleIndices(uint32_t offset, std::vector<uint32_t> &indices) {
    indices.emplace_back(offset);
    indices.emplace_back(offset + 1);
    indices.emplace_back(offset + 2);
}

// Зеркальный повтор координаты внутри тайла атласа: непрерывен (нет wrap-скачков UV на
// границах метра), не выходит за тайл (нет bleeding в соседние тайлы).
float mirrorWave(float value) {
    float f = std::fmod(value, 2.f);
    if (f < 0.f) { f += 2.f; }
    return f <= 1.f ? f : 2.f - f;
}

} // namespace

void TerrainMeshGenerator::addMarchingCubesMesh(
    const ChunkMeshSnapshot &snapshot, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices
) {
    static thread_local NodeField field;
    field.fill(snapshot);

    const float originX = static_cast<float>(snapshot.coord.x * Chunk::SIZE_X);
    const float originY = static_cast<float>(snapshot.coord.y * Chunk::SIZE_Y);
    const float originZ = static_cast<float>(snapshot.coord.z * Chunk::SIZE_Z);
    constexpr float ISO = 0.5f;
    // Вся поверхность — единая трава (задача этапа: без грубых границ между материалами;
    // блендинг нескольких природных материалов — следующий шаг).
    const Vec2 uvBase = getUV(GRASS_GROUND_TILE, GROUND_ATLAS_GRID);
    const Color color = toColor(0xFFFFFFFF);
    float uvSize = 1.f / GROUND_ATLAS_GRID;
    uvSize -= 0.001f;

    for (int y = 0; y < Chunk::SIZE_Y; y++) {
        for (int x = 0; x < Chunk::SIZE_X; x++) {
            for (int z = 0; z < Chunk::SIZE_Z; z++) {
                float cornerDensity[8];
                bool anyNatural = false;
                int config = 0;
                for (int corner = 0; corner < 8; corner++) {
                    const int i = x + MarchingCubes::CORNER_OFFSETS[corner][0];
                    const int j = y + MarchingCubes::CORNER_OFFSETS[corner][1];
                    const int k = z + MarchingCubes::CORNER_OFFSETS[corner][2];
                    cornerDensity[corner] = field.d(i, j, k);
                    if (cornerDensity[corner] > ISO) { config |= 1 << corner; }
                    anyNatural = anyNatural || field.natural(i, j, k);
                }
                if (config == 0 || config == 255 || !anyNatural) { continue; }

                const int8_t *tri = MarchingCubes::TRI_TABLE[config];
                for (int t = 0; tri[t] != -1; t += 3) {
                    Vec3 positions[3];
                    Vec3 normals[3];
                    for (int vert = 0; vert < 3; vert++) {
                        const int edge = tri[t + vert];
                        const int c0 = MarchingCubes::EDGE_CORNERS[edge][0];
                        const int c1 = MarchingCubes::EDGE_CORNERS[edge][1];
                        const int i0 = x + MarchingCubes::CORNER_OFFSETS[c0][0];
                        const int j0 = y + MarchingCubes::CORNER_OFFSETS[c0][1];
                        const int k0 = z + MarchingCubes::CORNER_OFFSETS[c0][2];
                        const int i1 = x + MarchingCubes::CORNER_OFFSETS[c1][0];
                        const int j1 = y + MarchingCubes::CORNER_OFFSETS[c1][1];
                        const int k1 = z + MarchingCubes::CORNER_OFFSETS[c1][2];
                        const float d0 = cornerDensity[c0];
                        const float d1 = cornerDensity[c1];
                        float lerp = (ISO - d0) / (d1 - d0);
                        lerp = lerp < 0.f ? 0.f : (lerp > 1.f ? 1.f : lerp);
                        positions[vert] = Vec3(
                            originX + i0 + (i1 - i0) * lerp,
                            originY + j0 + (j1 - j0) * lerp,
                            originZ + k0 + (k1 - k0) * lerp
                        );
                        const Vec3 g0 = field.gradient(i0, j0, k0);
                        const Vec3 g1 = field.gradient(i1, j1, k1);
                        Vec3 normal(
                            -(g0.x + (g1.x - g0.x) * lerp),
                            -(g0.y + (g1.y - g0.y) * lerp),
                            -(g0.z + (g1.z - g0.z) * lerp)
                        );
                        const float length = std::sqrt(
                            normal.x * normal.x + normal.y * normal.y + normal.z * normal.z
                        );
                        if (length > 0.0001f) {
                            normal.x /= length;
                            normal.y /= length;
                            normal.z /= length;
                        } else {
                            normal = Vec3(0.f, 1.f, 0.f);
                        }
                        normals[vert] = normal;
                    }

                    // Вырожденные треугольники (вершины рёбер схлопнулись в общий узел при
                    // density ровно на изоуровне) — пропускаем.
                    const Vec3 edge1(
                        positions[1].x - positions[0].x, positions[1].y - positions[0].y,
                        positions[1].z - positions[0].z
                    );
                    const Vec3 edge2(
                        positions[2].x - positions[0].x, positions[2].y - positions[0].y,
                        positions[2].z - positions[0].z
                    );
                    const Vec3 crossProduct(
                        edge1.y * edge2.z - edge1.z * edge2.y, edge1.z * edge2.x - edge1.x * edge2.z,
                        edge1.x * edge2.y - edge1.y * edge2.x
                    );
                    const float areaSquared = crossProduct.x * crossProduct.x +
                                              crossProduct.y * crossProduct.y +
                                              crossProduct.z * crossProduct.z;
                    if (areaSquared < 1e-10f) { continue; }

                    // Проекция UV — по доминантной оси средней нормали треугольника.
                    const Vec3 avgNormal(
                        (normals[0].x + normals[1].x + normals[2].x) / 3.f,
                        (normals[0].y + normals[1].y + normals[2].y) / 3.f,
                        (normals[0].z + normals[1].z + normals[2].z) / 3.f
                    );
                    const float absX = std::abs(avgNormal.x);
                    const float absY = std::abs(avgNormal.y);
                    const float absZ = std::abs(avgNormal.z);

                    addFaceTriangleIndices(static_cast<uint32_t>(vertices.size()), indices);
                    for (int vert = 0; vert < 3; vert++) {
                        const Vec3 &pos = positions[vert];
                        float u, v;
                        if (absY >= absX && absY >= absZ) {
                            u = pos.x;
                            v = pos.z;
                        } else if (absX >= absZ) {
                            u = pos.z;
                            v = pos.y;
                        } else {
                            u = pos.x;
                            v = pos.y;
                        }
                        const Vec2 uv(
                            uvBase.x + mirrorWave(u) * uvSize, uvBase.y + mirrorWave(v) * uvSize
                        );
                        vertices.emplace_back(Vertex(pos, uv, normals[vert], color, 1.f));
                    }
                }
            }
        }
    }
}

void TerrainMeshGenerator::addFaceIndices(uint32_t offset, std::vector<uint32_t> &indices) {
    indices.emplace_back(offset);
    indices.emplace_back(offset + 1);
    indices.emplace_back(offset + 2);
    indices.emplace_back(offset + 2);
    indices.emplace_back(offset + 3);
    indices.emplace_back(offset);
}

bool TerrainMeshGenerator::isAir(int x, int y, int z, const ChunkMeshSnapshot &snapshot) {
    return snapshot.isAirWorld(x, y, z);
}
