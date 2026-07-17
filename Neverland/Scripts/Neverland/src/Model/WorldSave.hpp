#pragma once

#include "Chunk.hpp"

#include <string>
#include <unordered_map>
#include <vector>

// Сохранение мира: только дельты изменённых игроком чанков относительно процедурной
// генерации (мир детерминирован — при загрузке чанк генерится заново и поверх
// накладывается дельта) + позиция игрока. Один бинарный файл в persistent data path.
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

    // Снять дельту чанка от эталонной генерации (звать для чанков с modifiedByPlayer).
    void captureChunk(const Chunk &chunk);
    // Наложить сохранённую дельту (после generateChunkData).
    void applyToChunk(Chunk &chunk) const;

    PlayerData player;

private:
    struct VoxelEdit {
        uint16_t index;
        uint8_t type;
    };

    std::unordered_map<ChunkCoord, std::vector<VoxelEdit>, ChunkCoordHash> m_chunks;
    std::string m_path;
};
