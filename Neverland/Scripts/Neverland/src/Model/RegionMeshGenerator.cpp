#include "RegionMeshGenerator.hpp"

#include "Chunk.hpp"
#include "ChunksStorage.hpp"
#include "TerrainMeshGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

// Дальние LOD-регионы — гладкий heightfield по той же высотной функции, что и рельеф:
// непрерывная поверхность с нормалями из градиента высот вместо ступенчатых кубов, чтобы
// дальний план вписывался в marching-cubes-передний. Материал — terrain (веса в цвете).

namespace {

// Вес травы в вершинном цвете terrain-шейдера (r=1).
constexpr uint32_t GRASS_WEIGHT_COLOR = 0xFF000000;

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

// Высота видимой поверхности: верх верхнего воксела — на ровной земле совпадает с
// изоповерхностью marching cubes ближних чанков.
float surfaceHeight(int x, int z) {
    return static_cast<float>(ChunksStorage::terrainHeight(x, z) + 1);
}

Vec3 nodeNormal(int x, int z, int step) {
    const float dx = surfaceHeight(x - step, z) - surfaceHeight(x + step, z);
    const float dz = surfaceHeight(x, z - step) - surfaceHeight(x, z + step);
    const float dy = 2.f * step;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    return Vec3(dx / length, dy / length, dz / length);
}

// Зеркальный повтор нормализованного UV тайла; период 2*step — соседние узлы сетки дают
// пилу 0→1→0 (текстура вдали крупнее, метровая плотность там не нужна).
float mirrorWave(float value) {
    float f = std::fmod(value, 2.f);
    if (f < 0.f) { f += 2.f; }
    return f <= 1.f ? f : 2.f - f;
}

struct GridVertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
};

GridVertex makeGridVertex(int x, int z, int step) {
    GridVertex vertex;
    vertex.position = Vec3(static_cast<float>(x), surfaceHeight(x, z), static_cast<float>(z));
    vertex.normal = nodeNormal(x, z, step);
    vertex.uv = Vec2(
        mirrorWave(static_cast<float>(x) / step), mirrorWave(static_cast<float>(z) / step)
    );
    return vertex;
}

void addTriangle(
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices,
    const GridVertex &a,
    const GridVertex &b,
    const GridVertex &c,
    Color color
) {
    const uint32_t base = static_cast<uint32_t>(vertices.size());
    indices.emplace_back(base);
    indices.emplace_back(base + 1);
    indices.emplace_back(base + 2);
    vertices.emplace_back(Vertex(a.position, a.uv, a.normal, color, 1.f));
    vertices.emplace_back(Vertex(b.position, b.uv, b.normal, color, 1.f));
    vertices.emplace_back(Vertex(c.position, c.uv, c.normal, color, 1.f));
}

// Юбка по граничному ребру ячейки (сосед вне кольца): вертикальная стенка вниз, закрывает
// щели на стыке колец LOD между собой и с ближними чанками.
void addSkirt(
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices,
    const GridVertex &edgeA,
    const GridVertex &edgeB,
    const Vec3 &outwardNormal,
    int step,
    Color color
) {
    const float depth = static_cast<float>(std::max(step * 2, 8));
    const float bottomY =
        std::min(edgeA.position.y, edgeB.position.y) - depth;
    GridVertex bottomA = edgeA;
    GridVertex bottomB = edgeB;
    bottomA.position.y = bottomY;
    bottomB.position.y = bottomY;
    bottomA.normal = outwardNormal;
    bottomB.normal = outwardNormal;
    GridVertex topA = edgeA;
    GridVertex topB = edgeB;
    topA.normal = outwardNormal;
    topB.normal = outwardNormal;
    bottomA.uv = Vec2(edgeA.uv.x, 1.f);
    bottomB.uv = Vec2(edgeB.uv.x, 1.f);
    // Порядок вершин — чтобы лицевая сторона юбки смотрела наружу (в сторону отсутствующего соседа).
    addTriangle(vertices, indices, topA, bottomB, bottomA, color);
    addTriangle(vertices, indices, topA, topB, bottomB, color);
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
    vertices.reserve(static_cast<std::size_t>(cellsX * cellsZ * 6));
    indices.reserve(static_cast<std::size_t>(cellsX * cellsZ * 6));

    const Color color = toColor(GRASS_WEIGHT_COLOR);
    for (int worldX = worldMinX; worldX < worldMaxX; worldX += step) {
        for (int worldZ = worldMinZ; worldZ < worldMaxZ; worldZ += step) {
            if (!isCellInsideRing(worldX, worldZ, step, request)) { continue; }

            const GridVertex v00 = makeGridVertex(worldX, worldZ, step);
            const GridVertex v01 = makeGridVertex(worldX, worldZ + step, step);
            const GridVertex v11 = makeGridVertex(worldX + step, worldZ + step, step);
            const GridVertex v10 = makeGridVertex(worldX + step, worldZ, step);

            addTriangle(vertices, indices, v00, v01, v11, color);
            addTriangle(vertices, indices, v11, v10, v00, color);

            // Юбки по границам кольца (соседняя ячейка вне кольца).
            if (!isCellInsideRing(worldX - step, worldZ, step, request)) {
                addSkirt(vertices, indices, v01, v00, Vec3(-1.f, 0.f, 0.f), step, color);
            }
            if (!isCellInsideRing(worldX + step, worldZ, step, request)) {
                addSkirt(vertices, indices, v10, v11, Vec3(1.f, 0.f, 0.f), step, color);
            }
            if (!isCellInsideRing(worldX, worldZ - step, step, request)) {
                addSkirt(vertices, indices, v00, v10, Vec3(0.f, 0.f, -1.f), step, color);
            }
            if (!isCellInsideRing(worldX, worldZ + step, step, request)) {
                addSkirt(vertices, indices, v11, v01, Vec3(0.f, 0.f, 1.f), step, color);
            }
        }
    }

    return RegionMeshBuildResult{request.key, request.requestId, MeshData(vertices, indices)};
}
