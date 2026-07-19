#include "BaseScript.hpp"
#include "Model/GameContext.hpp"
#include "Model/TerrainAccess.hpp"
#include "Model/WorldSave.hpp"

#include <Bamboo/Input.hpp>
#include <Bamboo/Logger.hpp>

void BaseScript::start() {
    GameContext::init();
    GameContext::s_blocksMaterial = material;
    GameContext::s_terrainMaterial = terrainMaterial;
    GameContext::s_markerMaterial = markerMaterial;
    GameContext::s_roofMaterial = roofMaterial;

    if (!TerrainAccess::init()) {
        LOG_ERROR("Neverland: engine terrain is missing — add a 'Terrain' entity with Terrain3D");
        return;
    }
    // Постройки покрывают тот же участок, что и рельеф.
    GameContext::s_buildingGrid->init(
        TerrainAccess::worldMinX(), 0, TerrainAccess::worldMinZ(),
        TerrainAccess::worldMaxX() - TerrainAccess::worldMinX(), TerrainAccess::worldMaxY(),
        TerrainAccess::worldMaxZ() - TerrainAccess::worldMinZ(), material, roofMaterial
    );

    // Восстановление сейва: правки рельефа поверх ассета + архитектурные объекты
    // (сейвы v2 несут одиночные кубы — мигрируются в Block-объекты).
    if (WorldSave *save = GameContext::s_worldSave) {
        TerrainAccess::restoreEdits(save->terrainEdits);
        GameContext::s_buildingGrid->restoreObjects(save->objects);
        GameContext::s_buildingGrid->restoreLegacyBlocks(save->legacyBlocks);
        save->legacyBlocks.clear(); // после миграции сейв живёт объектами
    }

    // Воксельный свет: прозрачность рельефа одним окном, полный пересчёт, полный ремеш
    // построек (старт мира — единственное место полного прохода).
    const int minX = TerrainAccess::worldMinX();
    const int minZ = TerrainAccess::worldMinZ();
    const int sizeX = TerrainAccess::worldMaxX() - minX;
    const int sizeZ = TerrainAccess::worldMaxZ() - minZ;
    const int sizeY = TerrainAccess::worldMaxY();
    GameContext::s_lightGrid->init(minX, 0, minZ, sizeX, sizeY, sizeZ);
    const TerrainAccess::Window window =
        TerrainAccess::readWindow(minX, 0, minZ, minX + sizeX, sizeY, minZ + sizeZ);
    for (int y = 0; y < sizeY; y++) {
        for (int z = minZ; z < minZ + sizeZ; z++) {
            for (int x = minX; x < minX + sizeX; x++) {
                if (window.layerAt(x, y, z) != 0) {
                    GameContext::s_lightGrid->setTerrainOpaque(x, y, z, true);
                }
            }
        }
    }
    GameContext::s_lightGrid->recomputeAll(*GameContext::s_buildingGrid);
    GameContext::s_buildingGrid->markLightDirtyBox(
        minX, 0, minZ, minX + sizeX, sizeY, minZ + sizeZ
    );
}

void BaseScript::update(float dt) {
    updateAutosave(dt);
    if (Input::isKeyPressed(Key::L)) { LOG_INFO("Hello Panda! var: %d", var); }
}

void BaseScript::updateAutosave(float dt) {
    constexpr float AUTOSAVE_INTERVAL_SECONDS = 30.0f;
    m_autosaveTimer += dt;
    if (m_autosaveTimer < AUTOSAVE_INTERVAL_SECONDS) { return; }
    m_autosaveTimer = 0.0f;
    saveWorld();
}

void BaseScript::saveWorld() {
    WorldSave *save = GameContext::s_worldSave;
    if (save == nullptr || GameContext::s_buildingGrid == nullptr) { return; }
    save->terrainEdits = TerrainAccess::editAccumulator();
    GameContext::s_buildingGrid->collectObjects(save->objects);
    save->saveToDisk();
}

void BaseScript::shutdown() {
    saveWorld();
    TerrainAccess::deinit();
    GameContext::deinit();
}
