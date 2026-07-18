#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Сохранение мира: дельта рельефа от .terrain-ассета (аккумулятор правок TerrainAccess),
// все блоки построек (BuildingGrid) и позиция игрока. Один бинарный файл в persistent
// data path. Формат v2 (движковый terrain); сейвы v1 (чанковые дельты) не читаются.
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
    // Блоки построек: packed мировой воксель → VoxelType.
    std::vector<std::pair<uint64_t, uint8_t>> buildingBlocks;

private:
    std::string m_path;
};
