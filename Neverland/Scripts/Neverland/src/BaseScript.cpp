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

    if (!TerrainAccess::init()) {
        LOG_ERROR("Neverland: engine terrain is missing — add a 'Terrain' entity with Terrain3D");
        return;
    }
    // Постройки покрывают тот же участок, что и рельеф.
    GameContext::s_buildingGrid->init(
        TerrainAccess::worldMinX(), 0, TerrainAccess::worldMinZ(),
        TerrainAccess::worldMaxX() - TerrainAccess::worldMinX(), TerrainAccess::worldMaxY(),
        TerrainAccess::worldMaxZ() - TerrainAccess::worldMinZ(), material
    );

    // Восстановление сейва: правки рельефа поверх ассета + блоки построек.
    if (WorldSave *save = GameContext::s_worldSave) {
        TerrainAccess::restoreEdits(save->terrainEdits);
        GameContext::s_buildingGrid->restoreBlocks(save->buildingBlocks);
    }
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
    GameContext::s_buildingGrid->collectBlocks(save->buildingBlocks);
    save->saveToDisk();
}

void BaseScript::shutdown() {
    saveWorld();
    TerrainAccess::deinit();
    GameContext::deinit();
}
