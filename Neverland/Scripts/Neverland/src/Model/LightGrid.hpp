#pragma once

#include <cstdint>
#include <vector>

class BuildingCellGrid;

// Воксельный свет мира (MC-модель, план «свет по вокселям»): два канала 0..15 на ячейку.
// Солнце: вниз без затухания (небо светит сквозь прозрачные), вбок/вверх −1 за шаг.
// Источники (лампы): уровень источника, затухание 1 за шаг. Прозрачность ячейки:
// рельеф — слой 0, постройки — по типу объекта (окна/двери/карниз пропускают).
// Пересчёт локальный: bbox правки + радиус распространения; граница bbox читает
// прежний свет (ошибка на краю ≤1 уровня, глазом не видна).
class LightGrid final {
public:
    static constexpr uint8_t SUN_MAX = 15;
    static constexpr uint8_t LAMP_LEVEL = 14;

    struct Edit {
        int x, y, z;
    };

    // Бокс фактически изменившихся ячеек (для точечного ремеша мешей).
    struct ChangedBox {
        bool any = false;
        int fromX = 0, fromY = 0, fromZ = 0;
        int toX = 0, toY = 0, toZ = 0; // эксклюзивно
    };

    void init(int minX, int minY, int minZ, int sizeX, int sizeY, int sizeZ);
    void shutdown();

    bool isReady() const {
        return !m_cells.empty();
    }

    // Прозрачность рельефа: заполняется при старте окном TerrainAccess и правится точечно.
    void setTerrainOpaque(int x, int y, int z, bool opaque);
    void fillTerrainOpaqueColumnRange(int x, int z, int fromY, int toY, bool opaque);

    // Полный пересчёт (старт мира) и локальный вокруг правок (bbox + радиус 16).
    ChangedBox recomputeAll(const BuildingCellGrid &buildings);
    ChangedBox recomputeAround(const BuildingCellGrid &buildings, const std::vector<Edit> &edits);

    // Свет ячейки (вне участка: солнце сверху, темнота снизу).
    uint8_t sunAt(int x, int y, int z) const;
    uint8_t blockAt(int x, int y, int z) const;

    // Поколение изменений: растёт при каждом фактическом изменении света (аплоад светокарты).
    uint32_t generation() const {
        return m_generation;
    }

    int minX() const { return m_minX; }
    int minY() const { return m_minY; }
    int minZ() const { return m_minZ; }
    int sizeX() const { return m_sizeX; }
    int sizeY() const { return m_sizeY; }
    int sizeZ() const { return m_sizeZ; }

private:
    bool isInside(int x, int y, int z) const;
    size_t cellIndex(int x, int y, int z) const;
    ChangedBox recomputeBox(
        const BuildingCellGrid &buildings, int fromX, int fromY, int fromZ, int toX, int toY, int toZ
    );
    bool isOpaque(const BuildingCellGrid &buildings, int x, int y, int z) const;

    int m_minX = 0, m_minY = 0, m_minZ = 0;
    int m_sizeX = 0, m_sizeY = 0, m_sizeZ = 0;
    uint32_t m_generation = 0;
    std::vector<uint8_t> m_cells;         // упаковка: sun << 4 | block
    std::vector<uint8_t> m_terrainOpaque; // 1 = ячейка рельефа непрозрачна
};
