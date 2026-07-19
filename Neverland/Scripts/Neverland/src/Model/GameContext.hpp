#pragma once

#include "BuildingGrid.hpp"

class WorldSave;

class GameContext final {
public:
    // Постройки игрока (кубы поверх движкового terrain). Живёт от init до deinit.
    static BuildingGrid *s_buildingGrid;
    // Сейв мира: правки рельефа (дельта от .terrain-ассета) + блоки + игрок.
    static WorldSave *s_worldSave;
    static MaterialHandle s_terrainMaterial; // рельеф/рука природных (base_ground_1)
    static MaterialHandle s_blocksMaterial;  // рукотворные блоки/стены (base_materials_1)
    static MaterialHandle s_markerMaterial;  // подсветка кисти/блока (полупрозрачный unlit)

    static void init();
    static void deinit();

    // Движковый terrain готов сразу после загрузки мира (меши строятся лениво в рендере).
    static bool isWorldLoaded();
};
