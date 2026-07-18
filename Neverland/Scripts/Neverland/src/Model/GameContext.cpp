#include "GameContext.hpp"
#include "WorldSave.hpp"

#include <Bamboo/ApplicationAPI.hpp>

BuildingGrid *GameContext::s_buildingGrid = nullptr;
WorldSave *GameContext::s_worldSave = nullptr;
MaterialHandle GameContext::s_terrainMaterial = {};
MaterialHandle GameContext::s_blocksMaterial = {};

void GameContext::init() {
    s_worldSave = new WorldSave();
    const std::string saveDirectory = Bamboo::ApplicationAPI::getPersistentDataPath();
    if (!saveDirectory.empty()) {
        s_worldSave->load(saveDirectory + "/neverland_world.sav");
    }
    s_buildingGrid = new BuildingGrid();
}

void GameContext::deinit() {
    if (s_buildingGrid != nullptr) {
        s_buildingGrid->shutdown();
        delete s_buildingGrid;
        s_buildingGrid = nullptr;
    }
    delete s_worldSave;
    s_worldSave = nullptr;
    s_terrainMaterial = {};
    s_blocksMaterial = {};
}

bool GameContext::isWorldLoaded() {
    return s_buildingGrid != nullptr;
}
