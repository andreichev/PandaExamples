#include "GameContext.hpp"
#include "WorldSave.hpp"

#include <Bamboo/ApplicationAPI.hpp>

BuildingCellGrid *GameContext::s_buildingGrid = nullptr;
WorldSave *GameContext::s_worldSave = nullptr;
MaterialHandle GameContext::s_terrainMaterial = {};
MaterialHandle GameContext::s_blocksMaterial = {};
MaterialHandle GameContext::s_markerMaterial = {};
MaterialHandle GameContext::s_roofMaterial = {};
LightGrid *GameContext::s_lightGrid = nullptr;
LightmapTexture *GameContext::s_lightmap = nullptr;

void GameContext::init() {
    s_worldSave = new WorldSave();
    const std::string saveDirectory = Bamboo::ApplicationAPI::getPersistentDataPath();
    if (!saveDirectory.empty()) {
        s_worldSave->load(saveDirectory + "/neverland_world.sav");
    }
    s_buildingGrid = new BuildingCellGrid();
    s_lightGrid = new LightGrid();
    s_buildingGrid->setLightGrid(s_lightGrid);
    s_lightmap = new LightmapTexture();
}

void GameContext::deinit() {
    if (s_buildingGrid != nullptr) {
        s_buildingGrid->shutdown();
        delete s_buildingGrid;
        s_buildingGrid = nullptr;
    }
    delete s_worldSave;
    s_worldSave = nullptr;
    if (s_lightmap != nullptr) { s_lightmap->shutdown(); }
    delete s_lightmap;
    s_lightmap = nullptr;
    delete s_lightGrid;
    s_lightGrid = nullptr;
    s_terrainMaterial = {};
    s_blocksMaterial = {};
    s_markerMaterial = {};
    s_roofMaterial = {};
}

bool GameContext::isWorldLoaded() {
    return s_buildingGrid != nullptr;
}

void GameContext::onTerrainEditsApplied(const std::vector<TerrainAccess::Edit> &edits) {
    if (s_lightGrid == nullptr || !s_lightGrid->isReady() || s_buildingGrid == nullptr) { return; }
    std::vector<LightGrid::Edit> lightEdits;
    lightEdits.reserve(edits.size());
    for (const TerrainAccess::Edit &edit : edits) {
        s_lightGrid->setTerrainOpaque(edit.x, edit.y, edit.z, edit.type != VoxelType::NOTHING);
        lightEdits.push_back({edit.x, edit.y, edit.z});
    }
    const LightGrid::ChangedBox changed = s_lightGrid->recomputeAround(*s_buildingGrid, lightEdits);
    if (changed.any) {
        s_buildingGrid->markLightDirtyBox(
            changed.fromX, changed.fromY, changed.fromZ, changed.toX, changed.toY, changed.toZ
        );
    }
}
