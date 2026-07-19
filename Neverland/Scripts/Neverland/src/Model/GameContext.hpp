#pragma once

#include "BuildingCellGrid.hpp"
#include "LightGrid.hpp"
#include "LightmapTexture.hpp"
#include "TerrainAccess.hpp"

class WorldSave;

class GameContext final {
public:
    // Постройки игрока (кубы поверх движкового terrain). Живёт от init до deinit.
    static BuildingCellGrid *s_buildingGrid;
    // Сейв мира: правки рельефа (дельта от .terrain-ассета) + блоки + игрок.
    static WorldSave *s_worldSave;
    static MaterialHandle s_terrainMaterial; // рельеф/рука природных (base_ground_1)
    static MaterialHandle s_blocksMaterial;  // рукотворные блоки/стены (base_materials_1)
    static MaterialHandle s_markerMaterial;  // подсветка кисти/блока (полупрозрачный unlit)
    static MaterialHandle s_roofMaterial;    // черепица крыш (base_roof_1)
    // Воксельный свет мира (солнце + лампы); мешер построек читает при ремеше.
    static LightGrid *s_lightGrid;
    // Светокарта рельефа (атлас LightGrid → terrain-шейдер).
    static LightmapTexture *s_lightmap;

    static void init();
    static void deinit();

    // Движковый terrain готов сразу после загрузки мира (меши строятся лениво в рендере).
    static bool isWorldLoaded();

    // Правки рельефа применены: обновить прозрачность света и ремешить затронутое.
    static void onTerrainEditsApplied(const std::vector<TerrainAccess::Edit> &edits);
};
