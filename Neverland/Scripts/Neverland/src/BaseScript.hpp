#pragma once

#include <Bamboo/Script.hpp>
#include <Bamboo/Bamboo.hpp>

using namespace Bamboo;

// Владелец игрового мира: инициализирует GameContext (постройки/сейв), связывает игру с
// движковым terrain (сущность "Terrain"), восстанавливает сейв и автосейвит. Рельеф целиком
// на движке (Terrain3DComponent + TerrainAPI) — стриминга и генерации у игры больше нет.
class BaseScript : public Script {
public:
    int var;
    MaterialHandle material;        // рукотворные блоки/стены (base_materials_1)
    MaterialHandle terrainMaterial; // рельеф + рука природных (base_ground_1)
    MaterialHandle markerMaterial;  // подсветка кисти/блока (полупрозрачный unlit)

    PANDA_FIELDS_BEGIN(BaseScript)
    PANDA_FIELD(var)
    PANDA_FIELD(material)
    PANDA_FIELD(terrainMaterial)
    PANDA_FIELD(markerMaterial)
    PANDA_FIELDS_END

    void start() override;
    void update(float dt) override;
    void shutdown() override;

private:
    void updateAutosave(float dt);
    void saveWorld(); // правки рельефа + блоки + игрок → world.sav

    float m_autosaveTimer = 0.0f;
};

REGISTER_SCRIPT(BaseScript)
