#include "LightGrid.hpp"
#include "BuildingCellGrid.hpp"

#include <algorithm>
#include <deque>

namespace {

constexpr int LIGHT_SPREAD_RADIUS = 16; // максимум распространения уровня 15/14

} // namespace

void LightGrid::init(int minX, int minY, int minZ, int sizeX, int sizeY, int sizeZ) {
    m_minX = minX;
    m_minY = minY;
    m_minZ = minZ;
    m_sizeX = sizeX;
    m_sizeY = sizeY;
    m_sizeZ = sizeZ;
    m_cells.assign(static_cast<size_t>(sizeX) * sizeY * sizeZ, 0);
    m_terrainOpaque.assign(static_cast<size_t>(sizeX) * sizeY * sizeZ, 0);
}

void LightGrid::shutdown() {
    m_cells.clear();
    m_terrainOpaque.clear();
}

bool LightGrid::isInside(int x, int y, int z) const {
    return x >= m_minX && y >= m_minY && z >= m_minZ && x < m_minX + m_sizeX &&
           y < m_minY + m_sizeY && z < m_minZ + m_sizeZ;
}

size_t LightGrid::cellIndex(int x, int y, int z) const {
    return (static_cast<size_t>(y - m_minY) * m_sizeZ + static_cast<size_t>(z - m_minZ)) * m_sizeX +
           static_cast<size_t>(x - m_minX);
}

void LightGrid::setTerrainOpaque(int x, int y, int z, bool opaque) {
    if (!isInside(x, y, z)) { return; }
    m_terrainOpaque[cellIndex(x, y, z)] = opaque ? 1 : 0;
}

void LightGrid::fillTerrainOpaqueColumnRange(int x, int z, int fromY, int toY, bool opaque) {
    for (int y = fromY; y < toY; y++) {
        setTerrainOpaque(x, y, z, opaque);
    }
}

uint8_t LightGrid::sunAt(int x, int y, int z) const {
    if (!isInside(x, y, z)) {
        // Вне участка: выше мира — небо, ниже — темнота.
        return y >= m_minY + m_sizeY ? SUN_MAX : (y < m_minY ? 0 : SUN_MAX);
    }
    return m_cells[cellIndex(x, y, z)] >> 4;
}

uint8_t LightGrid::blockAt(int x, int y, int z) const {
    if (!isInside(x, y, z)) { return 0; }
    return m_cells[cellIndex(x, y, z)] & 0x0F;
}

bool LightGrid::isOpaque(const BuildingCellGrid &buildings, int x, int y, int z) const {
    if (isInside(x, y, z) && m_terrainOpaque[cellIndex(x, y, z)] != 0) { return true; }
    const ArchitectureObject *object = buildings.objectAt(x, y, z);
    if (object == nullptr) { return false; }
    switch (object->type) {
        case ArchObjectType::Block:
        case ArchObjectType::Beam:
        case ArchObjectType::Wall:
        case ArchObjectType::Roof:
            return true;
        case ArchObjectType::Window: // проём пропускает свет (стекла нет)
        case ArchObjectType::Door:
        case ArchObjectType::Cornice:
        case ArchObjectType::Lamp:
        case ArchObjectType::COUNT:
            return false;
    }
    return false;
}

LightGrid::ChangedBox LightGrid::recomputeAll(const BuildingCellGrid &buildings) {
    return recomputeBox(
        buildings, m_minX, m_minY, m_minZ, m_minX + m_sizeX, m_minY + m_sizeY, m_minZ + m_sizeZ
    );
}

LightGrid::ChangedBox
LightGrid::recomputeAround(const BuildingCellGrid &buildings, const std::vector<Edit> &edits) {
    if (edits.empty() || m_cells.empty()) { return {}; }
    int fromX = edits[0].x, toX = edits[0].x;
    int fromY = edits[0].y, toY = edits[0].y;
    int fromZ = edits[0].z, toZ = edits[0].z;
    for (const Edit &edit : edits) {
        fromX = std::min(fromX, edit.x);
        toX = std::max(toX, edit.x);
        fromY = std::min(fromY, edit.y);
        toY = std::max(toY, edit.y);
        fromZ = std::min(fromZ, edit.z);
        toZ = std::max(toZ, edit.z);
    }
    return recomputeBox(
        buildings, fromX - LIGHT_SPREAD_RADIUS, m_minY, fromZ - LIGHT_SPREAD_RADIUS,
        toX + LIGHT_SPREAD_RADIUS + 1, m_minY + m_sizeY, toZ + LIGHT_SPREAD_RADIUS + 1
    );
}

// Пересчёт бокса: солнечные колонки (по всей высоте — правка меняет тень под собой),
// затем волна распространения обоих каналов. Границы бокса читают прежний свет.
LightGrid::ChangedBox LightGrid::recomputeBox(
    const BuildingCellGrid &buildings, int fromX, int fromY, int fromZ, int toX, int toY, int toZ
) {
    fromX = std::max(fromX, m_minX);
    fromY = std::max(fromY, m_minY);
    fromZ = std::max(fromZ, m_minZ);
    toX = std::min(toX, m_minX + m_sizeX);
    toY = std::min(toY, m_minY + m_sizeY);
    toZ = std::min(toZ, m_minZ + m_sizeZ);
    ChangedBox changed;
    if (fromX >= toX || fromY >= toY || fromZ >= toZ) { return changed; }
    const auto markChanged = [&](int x, int y, int z) {
        if (!changed.any) {
            changed.any = true;
            changed.fromX = x;
            changed.toX = x + 1;
            changed.fromY = y;
            changed.toY = y + 1;
            changed.fromZ = z;
            changed.toZ = z + 1;
            return;
        }
        changed.fromX = std::min(changed.fromX, x);
        changed.toX = std::max(changed.toX, x + 1);
        changed.fromY = std::min(changed.fromY, y);
        changed.toY = std::max(changed.toY, y + 1);
        changed.fromZ = std::min(changed.fromZ, z);
        changed.toZ = std::max(changed.toZ, z + 1);
    };

    struct Node {
        int x, y, z;
    };
    std::deque<Node> queue;

    // 1) Солнечные колонки: вертикаль без затухания; остальное обнуляется.
    for (int z = fromZ; z < toZ; z++) {
        for (int x = fromX; x < toX; x++) {
            uint8_t sun = SUN_MAX; // верх мира — небо
            for (int y = toY - 1; y >= fromY; y--) {
                if (isOpaque(buildings, x, y, z)) { sun = 0; }
                const size_t index = cellIndex(x, y, z);
                const uint8_t previous = m_cells[index];
                m_cells[index] = static_cast<uint8_t>(sun << 4);
                if (previous != m_cells[index]) { markChanged(x, y, z); }
            }
        }
    }

    // 2) Сиды волны: лампы + границы бокса (прежний свет снаружи) + колонны света.
    const auto pushIfBrighter = [&](int x, int y, int z, uint8_t sun, uint8_t block) {
        if (x < fromX || x >= toX || y < fromY || y >= toY || z < fromZ || z >= toZ) { return; }
        if (isOpaque(buildings, x, y, z)) { return; }
        const size_t index = cellIndex(x, y, z);
        const uint8_t currentSun = m_cells[index] >> 4;
        const uint8_t currentBlock = m_cells[index] & 0x0F;
        const uint8_t newSun = std::max(currentSun, sun);
        const uint8_t newBlock = std::max(currentBlock, block);
        if (newSun == currentSun && newBlock == currentBlock) { return; }
        m_cells[index] = static_cast<uint8_t>((newSun << 4) | newBlock);
        markChanged(x, y, z);
        queue.push_back({x, y, z});
    };

    for (int z = fromZ; z < toZ; z++) {
        for (int x = fromX; x < toX; x++) {
            for (int y = fromY; y < toY; y++) {
                const ArchitectureObject *object = buildings.objectAt(x, y, z);
                if (object != nullptr && object->type == ArchObjectType::Lamp) {
                    pushIfBrighter(x, y, z, 0, LAMP_LEVEL);
                }
                // Ячейки со светом — сиды волны (солнечные колонны растекаются вбок).
                const size_t index = cellIndex(x, y, z);
                if (m_cells[index] != 0) { queue.push_back({x, y, z}); }
            }
        }
    }
    // Границы бокса: свет из-за границы затекает внутрь.
    const auto seedBoundary = [&](int x, int y, int z, int nx, int ny, int nz) {
        const uint8_t outsideSun = sunAt(nx, ny, nz);
        const uint8_t outsideBlock = blockAt(nx, ny, nz);
        const uint8_t sunIn = outsideSun > 0 ? static_cast<uint8_t>(outsideSun - 1) : 0;
        const uint8_t blockIn = outsideBlock > 0 ? static_cast<uint8_t>(outsideBlock - 1) : 0;
        if (sunIn > 0 || blockIn > 0) { pushIfBrighter(x, y, z, sunIn, blockIn); }
    };
    for (int y = fromY; y < toY; y++) {
        for (int z = fromZ; z < toZ; z++) {
            if (fromX > m_minX) { seedBoundary(fromX, y, z, fromX - 1, y, z); }
            if (toX < m_minX + m_sizeX) { seedBoundary(toX - 1, y, z, toX, y, z); }
        }
        for (int x = fromX; x < toX; x++) {
            if (fromZ > m_minZ) { seedBoundary(x, y, fromZ, x, y, fromZ - 1); }
            if (toZ < m_minZ + m_sizeZ) { seedBoundary(x, y, toZ - 1, x, y, toZ); }
        }
    }

    // 3) Волна: солнце вниз без затухания, прочие направления −1; источники −1 всюду.
    constexpr int DIRECTIONS[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };
    while (!queue.empty()) {
        const Node node = queue.front();
        queue.pop_front();
        const size_t index = cellIndex(node.x, node.y, node.z);
        const uint8_t sun = m_cells[index] >> 4;
        const uint8_t block = m_cells[index] & 0x0F;
        for (const auto &direction : DIRECTIONS) {
            const int nx = node.x + direction[0];
            const int ny = node.y + direction[1];
            const int nz = node.z + direction[2];
            const bool down = direction[1] < 0;
            const uint8_t sunOut =
                sun == 0 ? 0 : (down && sun == SUN_MAX ? SUN_MAX : static_cast<uint8_t>(sun - 1));
            const uint8_t blockOut = block == 0 ? 0 : static_cast<uint8_t>(block - 1);
            if (sunOut > 0 || blockOut > 0) { pushIfBrighter(nx, ny, nz, sunOut, blockOut); }
        }
    }
    if (changed.any) { m_generation++; }
    return changed;
}
