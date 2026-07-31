#pragma once

#include "Voxel.hpp"

class LightGrid;

#include <Bamboo/Assets/MeshAPI.hpp>
#include <Bamboo/Base.hpp>

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

using namespace Bamboo;

// Типы объектов. Персистятся в сейве (u8) — менять только с bump версии WorldSave.
enum class ArchObjectType : uint8_t {
    Block = 0,      // куб 1×1×1 из выбранного материала
    Lamp = 1,       // источник света: столбик, светит уровнем LAMP_LEVEL (LightGrid)
    ModelBlock = 2, // модельный блок: геометрия из Minecraft block model (BlockModel.hpp)
    COUNT = 3
};

// Объект занимает одну ячейку и владеет ею целиком (ставится/ломается как одно целое).
// Параметры-пресеты по типу:
// ModelBlock: [0] стабильный id модели (BlockModels::byId)
constexpr int ARCH_PARAM_COUNT = 4;

struct ArchitectureObject {
    uint32_t id = 0;       // выдаёт сетка при постановке
    ArchObjectType type = ArchObjectType::Block;
    int x = 0, y = 0, z = 0; // origin (мировые воксельные координаты)
    uint8_t rotation = 0;    // повороты по 90° вокруг Y (смысл зависит от типа)
    VoxelType material = VoxelType::STONE_BRICKS;
    uint8_t params[ARCH_PARAM_COUNT] = {0, 0, 0, 0}; // 0 = значение по умолчанию
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
    // Воксельный свет: мешер запекает каналы в вершины; правки объектов пересчитывают
    // сетку света и ремешат чанки изменившихся ячеек.
    void setLightGrid(LightGrid *lightGrid) {
        m_lightGrid = lightGrid;
    }
    // Дирти + ремеш чанков, пересекающих бокс (изменившийся свет).
    void markLightDirtyBox(int fromX, int fromY, int fromZ, int toX, int toY, int toZ);
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
    // Восстановление сейва: только раскладка по ячейкам, чанки остаются dirty — единый
    // ремеш зовёт вызывающий (rebuildDirtyChunks) после пересчёта света.
    void restoreObjects(const std::vector<ArchitectureObject> &objects);
    // Ремеш всех чанков, помеченных dirty (загрузка: один проход после света).
    void rebuildDirtyChunks();
    // Ячейка занята каким-либо объектом — плотный кеш без хеш-таблиц (горячий путь света).
    bool hasCellAt(int x, int y, int z) const {
        return isInside(x, y, z) && m_blocks[voxelIndex(x, y, z)] != 0;
    }
    // Объекты (id → объект): свет сидирует лампы списком, а не сканом всех ячеек.
    const std::unordered_map<uint32_t, ArchitectureObject> &objects() const {
        return m_objects;
    }
    // Диагностика: сколько чанков перестроено с прошлого опроса (сброс при чтении).
    uint32_t takeRemeshCount() {
        const uint32_t count = m_remeshCounter;
        m_remeshCounter = 0;
        return count;
    }

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
    // Свет: пересчёт вокруг изменённого объекта + ремеш затронутых чанков.
    void applyLightForObjectChange(const ArchitectureObject &object);
    void rebuildChunk(int chunkX, int chunkY, int chunkZ);
    // Объект семейства линии стен, владеющий ячейкой (nullptr, если ячейка не стенная).
    // Ячейка заполнена кубом целиком (прячет грани соседей и участвует в AO).
    bool isFullCellAt(int x, int y, int z) const;
    // Модельные блоки чанка: боксы Minecraft block model → квады меша.
    void appendModelBlockGeometry(
        int startX, int startY, int startZ, int endX, int endY, int endZ,
        std::vector<Vertex> &vertices, std::vector<uint32_t> &indices
    ) const;
    // Фонарик-куб (эмиссив) в основной меш.
    void appendLampCube(int x, int y, int z, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices) const;
    int m_minX = 0, m_minY = 0, m_minZ = 0;
    int m_sizeX = 0, m_sizeY = 0, m_sizeZ = 0;
    int m_chunksX = 0, m_chunksY = 0, m_chunksZ = 0;
    std::vector<uint8_t> m_blocks; // кеш материалов ячеек, порядок (y, z, x); 0 = пусто
    std::unordered_map<uint64_t, uint32_t> m_cellOwner;         // занятая ячейка → id объекта
    std::unordered_map<uint32_t, ArchitectureObject> m_objects; // id → объект
    uint32_t m_nextObjectId = 1;
    std::vector<ChunkView> m_views;
    MaterialHandle m_material;
    uint32_t m_remeshCounter = 0;
    LightGrid *m_lightGrid = nullptr;
};
