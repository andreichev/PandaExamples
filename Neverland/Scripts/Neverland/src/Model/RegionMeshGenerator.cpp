#include "RegionMeshGenerator.hpp"

#include "Chunk.hpp"
#include "ChunksStorage.hpp"
#include "TerrainMeshGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr uint8_t GRASS_SIDE_TILE = 2;
constexpr uint8_t GRASS_TOP_TILE = 3;
constexpr uint32_t GRASS_SIDE_COLOR = 0xFFFFFFFF;
constexpr uint32_t GRASS_TOP_COLOR = 0x9EEA6CFF;
constexpr float LIGHT_TOP = 0.95f;
constexpr float LIGHT_FRONT_Z_POSITIVE = 1.0f;
constexpr float LIGHT_BACK_Z_NEGATIVE = 0.75f;
constexpr float LIGHT_X_NEGATIVE = 0.9f;
constexpr float LIGHT_X_POSITIVE = 0.8f;
constexpr float ATLAS_TILE_SIZE = 1.f / 16.f;
constexpr float ATLAS_UV_INSET = 0.0005f;
constexpr float ATLAS_UV_SIZE = ATLAS_TILE_SIZE - ATLAS_UV_INSET * 2.f;

struct SideBottom {
    bool visible = false;
    float y = 0.f;
};

Color toColor(uint32_t hex) {
    Color color;
    uint8_t r = hex >> 24;
    uint8_t g = hex >> 16;
    uint8_t b = hex >> 8;
    uint8_t a = hex >> 0;
    color.r = r / 255.f;
    color.g = g / 255.f;
    color.b = b / 255.f;
    color.a = a / 255.f;
    return color;
}

int floorDiv(int value, int divisor) {
    int result = value / divisor;
    int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) { result--; }
    return result;
}

bool isInsideRing(int worldX, int worldZ, const RegionMeshBuildRequest &request) {
    const int chunkX = floorDiv(worldX, Chunk::SIZE_X);
    const int chunkZ = floorDiv(worldZ, Chunk::SIZE_Z);
    const long long dx = static_cast<long long>(chunkX) - request.centerChunkX;
    const long long dz = static_cast<long long>(chunkZ) - request.centerChunkZ;
    const long long distanceSquared = dx * dx + dz * dz;
    const long long minDistance = request.minChunkDistance;
    const long long maxDistance = request.maxChunkDistance;
    return distanceSquared > minDistance * minDistance && distanceSquared <= maxDistance * maxDistance;
}

bool isCellInsideRing(int worldX, int worldZ, int step, const RegionMeshBuildRequest &request) {
    return isInsideRing(worldX + step / 2, worldZ + step / 2, request);
}

int cellHeight(int worldX, int worldZ, int step) {
    const int h00 = ChunksStorage::terrainHeight(worldX, worldZ);
    const int h10 = ChunksStorage::terrainHeight(worldX + step, worldZ);
    const int h01 = ChunksStorage::terrainHeight(worldX, worldZ + step);
    const int h11 = ChunksStorage::terrainHeight(worldX + step, worldZ + step);
    const int height = std::max(std::max(h00, h10), std::max(h01, h11));
    const int verticalStep = std::max(1, step / 2);
    if (verticalStep == 1) { return height; }

    const int quantizedHeight = ((height + verticalStep / 2) / verticalStep) * verticalStep;
    return std::clamp(quantizedHeight, ChunksStorage::WORLD_MIN_Y + 1, ChunksStorage::WORLD_MAX_Y - 2);
}

Vec2 getTileUV(uint8_t tileIndex, float u, float v) {
    const float baseU = static_cast<float>(tileIndex % 16) * ATLAS_TILE_SIZE + ATLAS_UV_INSET;
    const float baseV = static_cast<float>(tileIndex / 16) * ATLAS_TILE_SIZE + ATLAS_UV_INSET;
    return Vec2(baseU + u * ATLAS_UV_SIZE, baseV + v * ATLAS_UV_SIZE);
}

void addQuad(
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices,
    const Vec3 &p0,
    const Vec3 &p1,
    const Vec3 &p2,
    const Vec3 &p3,
    const Vec3 &normal,
    const Color &color,
    uint8_t tileIndex,
    float light
) {
    TerrainMeshGenerator::addFaceIndices(static_cast<uint32_t>(vertices.size()), indices);
    vertices.emplace_back(Vertex(p0, getTileUV(tileIndex, 0.f, 1.f), normal, color, light));
    vertices.emplace_back(Vertex(p1, getTileUV(tileIndex, 1.f, 1.f), normal, color, light));
    vertices.emplace_back(Vertex(p2, getTileUV(tileIndex, 1.f, 0.f), normal, color, light));
    vertices.emplace_back(Vertex(p3, getTileUV(tileIndex, 0.f, 0.f), normal, color, light));
}

void addQuadWithUv(
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices,
    const Vec3 &p0,
    const Vec3 &p1,
    const Vec3 &p2,
    const Vec3 &p3,
    const Vec3 &normal,
    const Color &color,
    uint8_t tileIndex,
    float light,
    const Vec2 &uv0,
    const Vec2 &uv1,
    const Vec2 &uv2,
    const Vec2 &uv3
) {
    TerrainMeshGenerator::addFaceIndices(static_cast<uint32_t>(vertices.size()), indices);
    vertices.emplace_back(Vertex(p0, getTileUV(tileIndex, uv0.x, uv0.y), normal, color, light));
    vertices.emplace_back(Vertex(p1, getTileUV(tileIndex, uv1.x, uv1.y), normal, color, light));
    vertices.emplace_back(Vertex(p2, getTileUV(tileIndex, uv2.x, uv2.y), normal, color, light));
    vertices.emplace_back(Vertex(p3, getTileUV(tileIndex, uv3.x, uv3.y), normal, color, light));
}

float skirtBottomY(int height, int step) {
    const int skirtDepth = std::max(step * 2, 8);
    return static_cast<float>(std::max(ChunksStorage::WORLD_MIN_Y, height + 1 - skirtDepth));
}

SideBottom getSideBottom(
    int height,
    int neighborWorldX,
    int neighborWorldZ,
    int step,
    const RegionMeshBuildRequest &request
) {
    if (!isCellInsideRing(neighborWorldX, neighborWorldZ, step, request)) {
        return SideBottom{true, skirtBottomY(height, step)};
    }

    const int neighborHeight = cellHeight(neighborWorldX, neighborWorldZ, step);
    if (neighborHeight >= height) { return SideBottom{}; }

    return SideBottom{true, static_cast<float>(neighborHeight + 1)};
}

void addHeightfieldCell(
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices,
    int worldX,
    int worldZ,
    int step,
    const RegionMeshBuildRequest &request
) {
    if (!isCellInsideRing(worldX, worldZ, step, request)) { return; }

    const int height = cellHeight(worldX, worldZ, step);
    const float topY = static_cast<float>(height + 1);
    const float x0 = static_cast<float>(worldX);
    const float x1 = static_cast<float>(worldX + step);
    const float z0 = static_cast<float>(worldZ);
    const float z1 = static_cast<float>(worldZ + step);

    const Color topColor = toColor(GRASS_TOP_COLOR);
    const Color sideColor = toColor(GRASS_SIDE_COLOR);

    addQuad(
        vertices,
        indices,
        Vec3(x0, topY, z0),
        Vec3(x0, topY, z1),
        Vec3(x1, topY, z1),
        Vec3(x1, topY, z0),
        Vec3(0.f, 1.f, 0.f),
        topColor,
        GRASS_TOP_TILE,
        LIGHT_TOP
    );

    const SideBottom left = getSideBottom(height, worldX - step, worldZ, step, request);
    if (left.visible && left.y < topY) {
        addQuadWithUv(
            vertices,
            indices,
            Vec3(x0, left.y, z0),
            Vec3(x0, left.y, z1),
            Vec3(x0, topY, z1),
            Vec3(x0, topY, z0),
            Vec3(-1.f, 0.f, 0.f),
            sideColor,
            GRASS_SIDE_TILE,
            LIGHT_X_NEGATIVE,
            Vec2(0.f, 1.f),
            Vec2(1.f, 1.f),
            Vec2(1.f, 0.f),
            Vec2(0.f, 0.f)
        );
    }

    const SideBottom right = getSideBottom(height, worldX + step, worldZ, step, request);
    if (right.visible && right.y < topY) {
        addQuadWithUv(
            vertices,
            indices,
            Vec3(x1, right.y, z0),
            Vec3(x1, topY, z0),
            Vec3(x1, topY, z1),
            Vec3(x1, right.y, z1),
            Vec3(1.f, 0.f, 0.f),
            sideColor,
            GRASS_SIDE_TILE,
            LIGHT_X_POSITIVE,
            Vec2(1.f, 1.f),
            Vec2(1.f, 0.f),
            Vec2(0.f, 0.f),
            Vec2(0.f, 1.f)
        );
    }

    const SideBottom back = getSideBottom(height, worldX, worldZ - step, step, request);
    if (back.visible && back.y < topY) {
        addQuadWithUv(
            vertices,
            indices,
            Vec3(x0, back.y, z0),
            Vec3(x0, topY, z0),
            Vec3(x1, topY, z0),
            Vec3(x1, back.y, z0),
            Vec3(0.f, 0.f, -1.f),
            sideColor,
            GRASS_SIDE_TILE,
            LIGHT_BACK_Z_NEGATIVE,
            Vec2(1.f, 1.f),
            Vec2(1.f, 0.f),
            Vec2(0.f, 0.f),
            Vec2(0.f, 1.f)
        );
    }

    const SideBottom front = getSideBottom(height, worldX, worldZ + step, step, request);
    if (front.visible && front.y < topY) {
        addQuadWithUv(
            vertices,
            indices,
            Vec3(x0, front.y, z1),
            Vec3(x1, front.y, z1),
            Vec3(x1, topY, z1),
            Vec3(x0, topY, z1),
            Vec3(0.f, 0.f, 1.f),
            sideColor,
            GRASS_SIDE_TILE,
            LIGHT_FRONT_Z_POSITIVE,
            Vec2(0.f, 1.f),
            Vec2(1.f, 1.f),
            Vec2(1.f, 0.f),
            Vec2(0.f, 0.f)
        );
    }
}

} // namespace

RegionMeshBuildResult RegionMeshGenerator::makeRegionMesh(const RegionMeshBuildRequest &request) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const int step = std::max(1, request.sampleStepBlocks);
    const int regionOriginChunkX = request.key.x * request.regionSizeChunks;
    const int regionOriginChunkZ = request.key.z * request.regionSizeChunks;
    const int worldMinX = regionOriginChunkX * Chunk::SIZE_X;
    const int worldMinZ = regionOriginChunkZ * Chunk::SIZE_Z;
    const int worldSizeX = request.regionSizeChunks * Chunk::SIZE_X;
    const int worldSizeZ = request.regionSizeChunks * Chunk::SIZE_Z;
    const int worldMaxX = worldMinX + worldSizeX;
    const int worldMaxZ = worldMinZ + worldSizeZ;

    const int cellsX = (worldSizeX + step - 1) / step;
    const int cellsZ = (worldSizeZ + step - 1) / step;
    vertices.reserve(static_cast<std::size_t>(cellsX * cellsZ * 12));
    indices.reserve(static_cast<std::size_t>(cellsX * cellsZ * 18));

    for (int worldX = worldMinX; worldX < worldMaxX; worldX += step) {
        for (int worldZ = worldMinZ; worldZ < worldMaxZ; worldZ += step) {
            addHeightfieldCell(vertices, indices, worldX, worldZ, step, request);
        }
    }

    return RegionMeshBuildResult{request.key, request.requestId, MeshData(vertices, indices)};
}
