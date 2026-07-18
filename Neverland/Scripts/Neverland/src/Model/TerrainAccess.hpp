#pragma once

#include "Voxel.hpp"

#include <Bamboo/Base.hpp>
#include <Bamboo/TerrainAPI.hpp>

#include <unordered_map>
#include <vector>

using namespace Bamboo;

// Доступ игры к движковому Terrain3D (сущность "Terrain"): конверсия мировых воксельных
// координат в локаль ассета, батч-чтение окон (кисти/физика семплят из окна, не через ABI
// по вокселю), запись правок с аккумулятором для сейва (WorldSave хранит дельту от ассета).
class TerrainAccess final {
public:
    struct Window {
        int minX = 0, minY = 0, minZ = 0; // мировые воксельные границы (включительно)
        int maxX = 0, maxY = 0, maxZ = 0;
        std::vector<uint8_t> layers; // порядок (y, z, x); вне окна/участка → 0

        bool contains(int x, int y, int z) const;
        uint8_t layerAt(int x, int y, int z) const; // вне окна → 0 (воздух)
        VoxelType typeAt(int x, int y, int z) const {
            return voxelTypeForTerrainLayer(layerAt(x, y, z));
        }
    };

    struct Edit {
        int x, y, z; // мировые воксельные координаты
        VoxelType type;
    };

    static bool init(); // найти сущность "Terrain", закэшировать инфо; false — нет terrain
    static void deinit();
    static bool isReady();

    static EntityHandle entity();
    // Мировые границы участка в вокселях: [minX..maxX) и т.д.
    static int worldMinX();
    static int worldMinZ();
    static int worldMaxX();
    static int worldMaxZ();
    static int worldMaxY();
    static bool isInsideXZ(int x, int z);

    // Чтение окна вокселей (мировые границы включительно; края клампятся участком).
    static Window readWindow(int minX, int minY, int minZ, int maxX, int maxY, int maxZ);
    // Батч-правка: применяется через TerrainAPI (ремеш чанков внутри движка) и копится
    // в аккумулятор для сейва.
    static void applyEdits(const std::vector<Edit> &edits);
    // Поверхность/луч в мировых координатах (обёртки TerrainAPI).
    static bool sampleSurface(float x, float z, float &outHeight, Vec3 &outNormal);
    static bool raycast(Vec3 origin, Vec3 direction, float maxDistance, Vec3 &outPoint, Vec3 &outNormal);

    // Дельта от ассета для сейва (последняя правка по вокселю выигрывает).
    static const std::unordered_map<uint64_t, uint8_t> &editAccumulator();
    static void restoreEdits(const std::unordered_map<uint64_t, uint8_t> &edits); // из сейва
    static uint64_t packWorldVoxel(int x, int y, int z);
    static void unpackWorldVoxel(uint64_t key, int &x, int &y, int &z);
};
