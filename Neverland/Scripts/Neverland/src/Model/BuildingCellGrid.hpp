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
    Block = 0,  // одиночный куб 1×1×1
    Beam = 1,   // балка 3×1×1, поворот задаёт ось (0:+X, 1:+Z, 2:-X, 3:-Z)
    Wall = 2,   // стена 1×WALL_HEIGHT×1; rotation 0 — плоскость вдоль X, 1 — вдоль Z
    Window = 3,  // модуль линии стен: подоконная стенка + проём с рамой + перемычка
    Door = 4,    // модуль линии стен: открытый проём до перемычки
    Cornice = 5, // непрерывный карниз: ячейка пояса, соседние сливаются в профиль по пути
    Roof = 6,    // двускатная крыша: область ячеек, конёк вдоль длинной оси, скаты/фронтоны
    COUNT = 7
};

// Высота стенного модуля в ячейках (классический этаж).
constexpr int WALL_HEIGHT = 3;

// Семейство линии стен: участвуют в ранах WallRun (полотно не рвётся на окнах/дверях).
inline bool isWallFamilyType(ArchObjectType type) {
    return type == ArchObjectType::Wall || type == ArchObjectType::Window ||
           type == ArchObjectType::Door;
}

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
    // roofMaterial — атлас черепицы: скаты/конёк рисуются вторым мешем чанка.
    void init(
        int minX, int minY, int minZ, int sizeX, int sizeY, int sizeZ, MaterialHandle material,
        MaterialHandle roofMaterial
    );
    void shutdown();

    VoxelType blockAt(int x, int y, int z) const; // вне сетки → NOTHING
    bool isSolidAt(int x, int y, int z) const {
        return blockAt(x, y, z) != VoxelType::NOTHING;
    }
    // Твёрдость для персонажа: дверной проём проходим (нижние 2 ячейки Door).
    bool isPhysicsSolidAt(int x, int y, int z) const;

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
        EntityHandle roofEntity = 0; // скаты/конёк — материал черепицы
        MeshHandle roofMesh = 0;
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
    // Объект семейства линии стен, владеющий ячейкой (nullptr, если ячейка не стенная).
    const ArchitectureObject *wallAt(int x, int y, int z) const;
    // Ячейка заполнена кубом целиком (прячет грани соседей и участвует в AO).
    bool isFullCellAt(int x, int y, int z) const;
    // WallRun-проход: раны стен, пересекающие чанк, → крупные тонкие поверхности.
    void appendWallGeometry(
        int startX, int startY, int startZ, int endX, int endY, int endZ,
        std::vector<Vertex> &vertices, std::vector<uint32_t> &indices
    ) const;
    // Карнизы: цепочки ячеек → профиль по пути (ExtrudedProfile), митра-углы.
    void appendCorniceGeometry(
        int startX, int startY, int startZ, int endX, int endY, int endZ,
        std::vector<Vertex> &vertices, std::vector<uint32_t> &indices
    ) const;
    // Крыши: связная область ячеек → скаты/фронтоны/конёк (двускатная). Фронтоны —
    // в основной меш (материал стен), скаты и конёк — в roof-буферы (черепица).
    void appendRoofGeometry(
        int startX, int startY, int startZ, int endX, int endY, int endZ,
        std::vector<Vertex> &vertices, std::vector<uint32_t> &indices,
        std::vector<Vertex> &roofVertices, std::vector<uint32_t> &roofIndices
    ) const;
    // Область крыши, связная с ячейкой (flood 4-соседство, тот же материал и высота).
    void collectRoofRegion(int x, int y, int z, std::vector<std::array<int, 3>> &outCells) const;
    // Полоса крыши зависит от дальних краёв области — правка дирти́т всю область.
    void markRoofRegionDirty(const ArchitectureObject &object);

    int m_minX = 0, m_minY = 0, m_minZ = 0;
    int m_sizeX = 0, m_sizeY = 0, m_sizeZ = 0;
    int m_chunksX = 0, m_chunksY = 0, m_chunksZ = 0;
    std::vector<uint8_t> m_blocks; // кеш материалов ячеек, порядок (y, z, x); 0 = пусто
    std::unordered_map<uint64_t, uint32_t> m_cellOwner;         // занятая ячейка → id объекта
    std::unordered_map<uint32_t, ArchitectureObject> m_objects; // id → объект
    uint32_t m_nextObjectId = 1;
    std::vector<ChunkView> m_views;
    MaterialHandle m_material;
    MaterialHandle m_roofMaterial;
};
