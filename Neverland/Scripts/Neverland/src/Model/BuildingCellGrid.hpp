#pragma once

#include "Voxel.hpp"

#include <Bamboo/Assets/MeshAPI.hpp>
#include <Bamboo/Base.hpp>

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

using namespace Bamboo;

// Типы архитектурных объектов. Значения персистятся в сейве (u8) — только в конец.
enum class ArchObjectType : uint8_t {
    Block = 0, // одиночный куб 1×1×1
    Beam = 1,  // балка 3×1×1, поворот задаёт ось (0:+X, 1:+Z, 2:-X, 3:-Z)
    COUNT = 2
};

// Архитектурный объект: занимает набор ячеек, владеет ими целиком (ставится/ломается
// как одно целое). Геометрия v1 — кубы занятых ячеек; крупные плоскости — этап WallRun.
struct ArchitectureObject {
    uint32_t id = 0;       // выдаёт сетка при постановке
    ArchObjectType type = ArchObjectType::Block;
    int x = 0, y = 0, z = 0; // origin (мировые воксельные координаты)
    uint8_t rotation = 0;    // повороты по 90° вокруг Y (смысл зависит от типа)
    VoxelType material = VoxelType::STONE_BRICKS;
};

// Строительный слой игрока (этап 2 плана): ячейки хранят занятость и владельца,
// истина о составе — объекты. Плотный массив материалов ячеек остаётся кешем для
// мешера/физики/рейкаста (кубы с видимой сеткой, чанки 16³, синхронный ремеш).
class BuildingCellGrid final {
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

    // Ячейки, которые займёт объект (не проверяет границы/занятость).
    static void cellsFor(const ArchitectureObject &object, std::vector<std::array<int, 3>> &outCells);
    // Все ячейки в границах и свободны.
    bool canPlace(const ArchitectureObject &object) const;
    // Постановка целиком; 0 при отказе (canPlace). id объекта присваивается сеткой.
    uint32_t place(ArchitectureObject object);
    // Удаление объекта, владеющего ячейкой (объект исчезает целиком).
    bool removeObjectAt(int x, int y, int z);
    // Объект-владелец ячейки (nullptr, если ячейка пуста).
    const ArchitectureObject *objectAt(int x, int y, int z) const;

    // Луч по кубам (мировые координаты): DDA до maxDistance.
    std::optional<RaycastHit> raycast(Vec3 origin, Vec3 direction, float maxDistance) const;

    // Сейв v3: все объекты. Порядок полей — см. WorldSave.
    void collectObjects(std::vector<ArchitectureObject> &outObjects) const;
    void restoreObjects(const std::vector<ArchitectureObject> &objects);
    // Миграция сейвов v2: одиночные кубы (packed воксель + VoxelType) → Block-объекты.
    void restoreLegacyBlocks(const std::vector<std::pair<uint64_t, uint8_t>> &blocks);

private:
    struct ChunkView {
        EntityHandle entity = 0;
        MeshHandle mesh = 0;
        bool dirty = true;
    };

    bool isInside(int x, int y, int z) const;
    size_t voxelIndex(int x, int y, int z) const;
    size_t chunkIndex(int chunkX, int chunkY, int chunkZ) const;
    static uint64_t packCell(int x, int y, int z);
    // Запись/очистка ячеек объекта в кеше материалов и карте владельцев (+dirty).
    void writeObjectCells(const ArchitectureObject &object, bool clear);
    uint32_t placeInternal(ArchitectureObject object, bool rebuild);
    void markDirtyAround(int x, int y, int z);
    void rebuildDirtyChunks();
    void rebuildChunk(int chunkX, int chunkY, int chunkZ);

    int m_minX = 0, m_minY = 0, m_minZ = 0;
    int m_sizeX = 0, m_sizeY = 0, m_sizeZ = 0;
    int m_chunksX = 0, m_chunksY = 0, m_chunksZ = 0;
    std::vector<uint8_t> m_blocks; // кеш материалов ячеек, порядок (y, z, x); 0 = пусто
    std::unordered_map<uint64_t, uint32_t> m_cellOwner;         // занятая ячейка → id объекта
    std::unordered_map<uint32_t, ArchitectureObject> m_objects; // id → объект
    uint32_t m_nextObjectId = 1;
    std::vector<ChunkView> m_views;
    MaterialHandle m_material;
};
