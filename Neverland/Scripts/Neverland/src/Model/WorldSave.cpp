#include "WorldSave.hpp"

#include <Bamboo/Logger.hpp>

#include <fstream>

namespace {

constexpr uint32_t SAVE_MAGIC = 0x4E565356; // 'NVSV'
constexpr uint32_t SAVE_VERSION = 3;        // архитектурные объекты построек
constexpr uint32_t LEGACY_BLOCKS_VERSION = 2; // одиночные кубы — мигрируются

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
    terrainEdits.clear();
    objects.clear();
    legacyBlocks.clear();
    player = {};

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) { return; } // сейва ещё нет — новый мир

    uint32_t magic = 0, version = 0;
    if (!readValue(file, magic) || magic != SAVE_MAGIC) {
        LOG_ERROR("WorldSave: bad magic in %s", path.c_str());
        return;
    }
    if (!readValue(file, version) ||
        (version != SAVE_VERSION && version != LEGACY_BLOCKS_VERSION)) {
        LOG_WARN("WorldSave: version %u unsupported (want %u) — starting fresh", version, SAVE_VERSION);
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

    uint32_t editCount = 0;
    if (!readValue(file, editCount)) { return; }
    terrainEdits.reserve(editCount);
    for (uint32_t i = 0; i < editCount; i++) {
        uint64_t key = 0;
        uint8_t layer = 0;
        if (!readValue(file, key) || !readValue(file, layer)) {
            LOG_ERROR("WorldSave: truncated terrain edits in %s", path.c_str());
            return;
        }
        terrainEdits[key] = layer;
    }

    if (version == LEGACY_BLOCKS_VERSION) {
        uint32_t blockCount = 0;
        if (!readValue(file, blockCount)) { return; }
        legacyBlocks.reserve(blockCount);
        for (uint32_t i = 0; i < blockCount; i++) {
            uint64_t key = 0;
            uint8_t type = 0;
            if (!readValue(file, key) || !readValue(file, type)) {
                LOG_ERROR("WorldSave: truncated blocks in %s", path.c_str());
                return;
            }
            legacyBlocks.emplace_back(key, type);
        }
        LOG_INFO(
            "WorldSave: loaded v2 — %u terrain edits, %u legacy blocks from %s", editCount,
            blockCount, path.c_str()
        );
        return;
    }

    uint32_t objectCount = 0;
    if (!readValue(file, objectCount)) { return; }
    objects.reserve(objectCount);
    for (uint32_t i = 0; i < objectCount; i++) {
        ArchitectureObject object;
        uint8_t type = 0, rotation = 0, material = 0;
        int32_t x = 0, y = 0, z = 0;
        if (!readValue(file, type) || !readValue(file, x) || !readValue(file, y) ||
            !readValue(file, z) || !readValue(file, rotation) || !readValue(file, material)) {
            LOG_ERROR("WorldSave: truncated objects in %s", path.c_str());
            return;
        }
        object.type = static_cast<ArchObjectType>(type);
        object.x = x;
        object.y = y;
        object.z = z;
        object.rotation = rotation;
        object.material = static_cast<VoxelType>(material);
        if (object.type >= ArchObjectType::COUNT ||
            object.material >= VoxelType::COUNT) { // сейв из более новой версии игры
            continue;
        }
        objects.push_back(object);
    }
    LOG_INFO(
        "WorldSave: loaded %u terrain edits, %u objects from %s", editCount, objectCount,
        path.c_str()
    );
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
    writeValue(file, static_cast<uint32_t>(terrainEdits.size()));
    for (const auto &[key, layer] : terrainEdits) {
        writeValue(file, key);
        writeValue(file, layer);
    }
    writeValue(file, static_cast<uint32_t>(objects.size()));
    for (const ArchitectureObject &object : objects) {
        writeValue(file, static_cast<uint8_t>(object.type));
        writeValue(file, static_cast<int32_t>(object.x));
        writeValue(file, static_cast<int32_t>(object.y));
        writeValue(file, static_cast<int32_t>(object.z));
        writeValue(file, object.rotation);
        writeValue(file, static_cast<uint8_t>(object.material));
    }
}
