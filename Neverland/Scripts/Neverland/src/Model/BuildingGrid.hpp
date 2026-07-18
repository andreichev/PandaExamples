#pragma once

#include "Voxel.hpp"

#include <Bamboo/Assets/MeshAPI.hpp>
#include <Bamboo/Base.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

using namespace Bamboo;

// Постройки игрока: конечная сетка кубов поверх движкового terrain (совпадает с участком).
// Кубы намеренно с видимой сеткой — это «конструкции» (диздок); заменятся архитектурной
// системой на этапах BuildingCellGrid/WallRun. Меши — по чанкам 16³, ремеш синхронный.
class BuildingGrid final {
public:
    static constexpr int CHUNK = 16;

    struct RaycastHit {
        int x, y, z;              // воксель попадания (мировые)
        int normalX, normalY, normalZ; // грань входа
        float distance;
    };

    // Границы в мировых воксельных координатах: [minX..minX+sizeX) и т.д.
    void init(int minX, int minY, int minZ, int sizeX, int sizeY, int sizeZ, MaterialHandle material);
    void shutdown();

    VoxelType blockAt(int x, int y, int z) const; // вне сетки → NOTHING
    bool isSolidAt(int x, int y, int z) const {
        return blockAt(x, y, z) != VoxelType::NOTHING;
    }
    bool setBlock(int x, int y, int z, VoxelType type); // ремешит затронутые чанки

    // Луч по кубам (мировые координаты): DDA до maxDistance.
    std::optional<RaycastHit> raycast(Vec3 origin, Vec3 direction, float maxDistance) const;

    // Сейв: ненулевые блоки (мировые координаты + тип).
    void collectBlocks(std::vector<std::pair<uint64_t, uint8_t>> &outBlocks) const;
    void restoreBlocks(const std::vector<std::pair<uint64_t, uint8_t>> &blocks);

private:
    struct ChunkView {
        EntityHandle entity = 0;
        MeshHandle mesh = 0;
        bool dirty = true;
    };

    bool isInside(int x, int y, int z) const;
    size_t voxelIndex(int x, int y, int z) const;
    size_t chunkIndex(int chunkX, int chunkY, int chunkZ) const;
    void markDirtyAround(int x, int y, int z);
    void rebuildDirtyChunks();
    void rebuildChunk(int chunkX, int chunkY, int chunkZ);

    int m_minX = 0, m_minY = 0, m_minZ = 0;
    int m_sizeX = 0, m_sizeY = 0, m_sizeZ = 0;
    int m_chunksX = 0, m_chunksY = 0, m_chunksZ = 0;
    std::vector<uint8_t> m_blocks; // порядок (y, z, x); 0 = пусто
    std::vector<ChunkView> m_views;
    MaterialHandle m_material;
};
