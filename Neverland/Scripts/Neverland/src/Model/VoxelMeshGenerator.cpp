//
// Created by Admin on 12.02.2022.
//

#include "VoxelMeshGenerator.hpp"
#include "VoxelTextureMapper.hpp"

Vec2 getUV(uint8_t tileIndex) {
    // Размер одной текстуры на карте uv
    float uvSize = 1.f / 16.f;
    float u = tileIndex % 16 * uvSize;
    float v = tileIndex / 16 * uvSize;
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

void addDoubleSidedQuad(
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices,
    const Vec3 &p0,
    const Vec3 &p1,
    const Vec3 &p2,
    const Vec3 &p3,
    const Vec2 &uv,
    float uvSize,
    Color color,
    float light
) {
    const Vec3 normal(0.f, 1.f, 0.f);
    VoxelMeshGenerator::addFaceIndices(static_cast<uint32_t>(vertices.size()), indices);
    vertices.emplace_back(Vertex(p0, Vec2(uv.x, uv.y + uvSize), normal, color, light));
    vertices.emplace_back(Vertex(p1, Vec2(uv.x + uvSize, uv.y + uvSize), normal, color, light));
    vertices.emplace_back(Vertex(p2, Vec2(uv.x + uvSize, uv.y), normal, color, light));
    vertices.emplace_back(Vertex(p3, Vec2(uv.x, uv.y), normal, color, light));

    VoxelMeshGenerator::addFaceIndices(static_cast<uint32_t>(vertices.size()), indices);
    const Vec3 backNormal(0.f, -1.f, 0.f);
    vertices.emplace_back(Vertex(p3, Vec2(uv.x, uv.y), backNormal, color, light));
    vertices.emplace_back(Vertex(p2, Vec2(uv.x + uvSize, uv.y), backNormal, color, light));
    vertices.emplace_back(Vertex(p1, Vec2(uv.x + uvSize, uv.y + uvSize), backNormal, color, light));
    vertices.emplace_back(Vertex(p0, Vec2(uv.x, uv.y + uvSize), backNormal, color, light));
}

void addTallGrassMesh(
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices,
    int x,
    int y,
    int z,
    VoxelTextureData &textureData,
    float uvSize
) {
    const Vec2 uv = getUV(textureData.sideTileIndex);
    const Color color = toColor(textureData.sideColor);
    const float light = 0.98f;
    const float minX = static_cast<float>(x) + 0.08f;
    const float maxX = static_cast<float>(x) + 0.92f;
    const float minZ = static_cast<float>(z) + 0.08f;
    const float maxZ = static_cast<float>(z) + 0.92f;
    const float bottomY = static_cast<float>(y);
    const float topY = static_cast<float>(y) + 0.92f;

    addDoubleSidedQuad(
        vertices,
        indices,
        Vec3(minX, bottomY, minZ),
        Vec3(maxX, bottomY, maxZ),
        Vec3(maxX, topY, maxZ),
        Vec3(minX, topY, minZ),
        uv,
        uvSize,
        color,
        light
    );
    addDoubleSidedQuad(
        vertices,
        indices,
        Vec3(maxX, bottomY, minZ),
        Vec3(minX, bottomY, maxZ),
        Vec3(minX, topY, maxZ),
        Vec3(maxX, topY, minZ),
        uv,
        uvSize,
        color,
        light
    );
}

ChunkMeshBuildResult
VoxelMeshGenerator::makeOneChunkMesh(const ChunkMeshSnapshot &snapshot, bool ambientOcclusion) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
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

                float uvSize = 1.f / 16.f;
                uvSize -= 0.001f;

                if (currentVoxel->type == VoxelType::TALL_GRASS) {
                    addTallGrassMesh(vertices, indices, x, y, z, textureData, uvSize);
                    continue;
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
                    Vec2 uv = getUV(textureData.sideTileIndex);
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
                    Vec2 uv = getUV(textureData.sideTileIndex);
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
                    Vec2 uv = getUV(textureData.topTileIndex);
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
                    Vec2 uv = getUV(textureData.bottomTileIndex);
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
                    Vec2 uv = getUV(textureData.sideTileIndex);
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
                    Vec2 uv = getUV(textureData.sideTileIndex);
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
    return ChunkMeshBuildResult{snapshot.coord, snapshot.version, MeshData(vertices, indices)};
}

void VoxelMeshGenerator::addFaceIndices(uint32_t offset, std::vector<uint32_t> &indices) {
    indices.emplace_back(offset);
    indices.emplace_back(offset + 1);
    indices.emplace_back(offset + 2);
    indices.emplace_back(offset + 2);
    indices.emplace_back(offset + 3);
    indices.emplace_back(offset);
}

bool VoxelMeshGenerator::isAir(int x, int y, int z, const ChunkMeshSnapshot &snapshot) {
    return snapshot.isAirWorld(x, y, z);
}
