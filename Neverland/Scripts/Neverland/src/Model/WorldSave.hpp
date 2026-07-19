#pragma once

#include "BuildingCellGrid.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Сохранение мира: дельта рельефа от .terrain-ассета (аккумулятор правок TerrainAccess),
// архитектурные объекты (BuildingCellGrid) и позиция игрока. Один бинарный файл в
// persistent data path. Формат v3 (объекты); v2 (одиночные кубы) читается как legacy
// и мигрируется в Block-объекты при восстановлении; v1 не читается.
class WorldSave final {
public:
    struct PlayerData {
        bool valid = false;
        float x = 0.f, y = 0.f, z = 0.f;
        float pitch = 0.f, yaw = 0.f;
        uint8_t selectedBlock = 0;
    };

    void load(const std::string &path); // читает файл в память (или пустое состояние)
    void saveToDisk() const;            // перезапись файла целиком

    PlayerData player;
    // Правки рельефа: packed мировой воксель → слой (0 = вырезан). См. TerrainAccess.
    std::unordered_map<uint64_t, uint8_t> terrainEdits;
    // Архитектурные объекты построек (v3); id не персистятся (пересоздаются сеткой).
    std::vector<ArchitectureObject> objects;
    // Кубы из сейва v2 — только для миграции при загрузке (restoreLegacyBlocks).
    std::vector<std::pair<uint64_t, uint8_t>> legacyBlocks;

private:
    std::string m_path;
};
