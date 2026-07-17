#include "WorldSave.hpp"
#include "ChunksStorage.hpp"

#include <Bamboo/Logger.hpp>

#include <cstdio>
#include <fstream>

namespace {

constexpr uint32_t SAVE_MAGIC = 0x4E565356; // 'NVSV'
constexpr uint32_t SAVE_VERSION = 1;

template<typename T>
bool readValue(std::ifstream &file, T &value) {
    file.read(reinterpret_cast<char *>(&value), sizeof(T));
    return file.good();
}

template<typename T>
void writeValue(std::ofstream &file, const T &value) {
    file.write(reinterpret_cast<const char *>(&value), sizeof(T));
}

} // namespace

void WorldSave::load(const std::string &path) {
    m_path = path;
    m_chunks.clear();
    player = {};

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) { return; } // сейва ещё нет — новый мир

    uint32_t magic = 0, version = 0;
    if (!readValue(file, magic) || magic != SAVE_MAGIC) {
        LOG_ERROR("WorldSave: bad magic in %s", path.c_str());
        return;
    }
    if (!readValue(file, version) || version != SAVE_VERSION) {
        LOG_ERROR("WorldSave: unsupported version in %s", path.c_str());
        return;
    }

    uint8_t playerValid = 0;
    readValue(file, playerValid);
    readValue(file, player.x);
    readValue(file, player.y);
    readValue(file, player.z);
    readValue(file, player.pitch);
    readValue(file, player.yaw);
    readValue(file, player.selectedBlock);
    player.valid = playerValid != 0;

    uint32_t chunkCount = 0;
    if (!readValue(file, chunkCount)) { return; }
    for (uint32_t i = 0; i < chunkCount; i++) {
        ChunkCoord coord;
        uint32_t editCount = 0;
        if (!readValue(file, coord.x) || !readValue(file, coord.y) || !readValue(file, coord.z) ||
            !readValue(file, editCount)) {
            LOG_ERROR("WorldSave: truncated file %s", path.c_str());
            return;
        }
        std::vector<VoxelEdit> edits(editCount);
        file.read(reinterpret_cast<char *>(edits.data()), editCount * sizeof(VoxelEdit));
        if (!file.good()) {
            LOG_ERROR("WorldSave: truncated chunk in %s", path.c_str());
            return;
        }
        m_chunks.emplace(coord, std::move(edits));
    }
    LOG_INFO("WorldSave: loaded %u modified chunks from %s", chunkCount, path.c_str());
}

void WorldSave::saveToDisk() const {
    if (m_path.empty()) { return; }
    std::ofstream file(m_path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        LOG_ERROR("WorldSave: can't write %s", m_path.c_str());
        return;
    }
    writeValue(file, SAVE_MAGIC);
    writeValue(file, SAVE_VERSION);
    writeValue(file, static_cast<uint8_t>(player.valid ? 1 : 0));
    writeValue(file, player.x);
    writeValue(file, player.y);
    writeValue(file, player.z);
    writeValue(file, player.pitch);
    writeValue(file, player.yaw);
    writeValue(file, player.selectedBlock);
    writeValue(file, static_cast<uint32_t>(m_chunks.size()));
    for (const auto &[coord, edits] : m_chunks) {
        writeValue(file, coord.x);
        writeValue(file, coord.y);
        writeValue(file, coord.z);
        writeValue(file, static_cast<uint32_t>(edits.size()));
        file.write(reinterpret_cast<const char *>(edits.data()), edits.size() * sizeof(VoxelEdit));
    }
}

void WorldSave::captureChunk(const Chunk &chunk) {
    // Эталон — свежая процедурная генерация того же чанка; дельта = отличия текущего состояния.
    static Chunk reference; // только главный поток
    reference.setCoord(chunk.getCoord());
    ChunksStorage::generateChunkData(reference);

    std::vector<VoxelEdit> edits;
    for (int index = 0; index < Chunk::VOXEL_COUNT; index++) {
        const VoxelType current = chunk.data().voxels[index].type;
        if (current != reference.data().voxels[index].type) {
            edits.push_back({static_cast<uint16_t>(index), static_cast<uint8_t>(current)});
        }
    }
    if (edits.empty()) {
        m_chunks.erase(chunk.getCoord()); // правки вернули чанк к исходному виду
    } else {
        m_chunks[chunk.getCoord()] = std::move(edits);
    }
}

void WorldSave::applyToChunk(Chunk &chunk) const {
    auto it = m_chunks.find(chunk.getCoord());
    if (it == m_chunks.end()) { return; }
    for (const VoxelEdit &edit : it->second) {
        chunk.data().voxels[edit.index].type = static_cast<VoxelType>(edit.type);
    }
    chunk.data().modifiedByPlayer = true; // правленый чанк не должен выгружаться стримингом
}
