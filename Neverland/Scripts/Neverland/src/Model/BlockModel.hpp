#pragma once

#include <cstddef>
#include <cstdint>

// Модельные блоки: геометрия в формате Minecraft block model (боксы 0..16 + грани),
// сконвертированная тулом tools/import_blockmodels.py в сгенерированную таблицу
// (BlockModelsGenerated.inl). Источник истины — JSON в Assets/blockmodels/src/.
// Тайлы текстур модели запечены тем же тулом в свободные слоты base_materials_1.

// Порядок граней — как в Minecraft: down, up, north(-Z), south(+Z), west(-X), east(+X).
struct BlockModelFace {
    uint8_t exists;
    uint8_t tile;   // тайл blocks-атласа
    uint8_t uvRot;  // поворот UV, шаги по 90°
    float u0, v0, u1, v1; // прямоугольник в долях тайла (Y вниз)
};

struct BlockModelBox {
    float from[3]; // доли ячейки (исходные 0..16 поделены на 16; могут выходить за 0..1)
    float to[3];
    float rotAngle;      // градусы вокруг rotAxis через rotOrigin; 0 — без поворота
    uint8_t rotAxis;     // 0 = X, 1 = Y, 2 = Z
    float rotOrigin[3];
    BlockModelFace faces[6];
};

struct BlockModelData {
    uint8_t id; // стабильный id (персистится в ArchitectureObject::params[0])
    const char *name;
    // 0 одиночная; 1 connected (post+side); 2 tall (дверь, 2 ячейки);
    // 3 facing (ступень/карниз: ориентация по соседям + угловые варианты)
    uint8_t kind;
    uint8_t category; // раздел меню: 0 Fences, 1 Windows, 2 Doors, 3 Forms, 4 Decor, 5 Cornices
    uint8_t sideBase; // kind 1: базовая ориентация side (повороты приведения к северу)
    uint16_t boxCount; // kind 0/2/3: основная модель; kind 1: столб (post)
    const BlockModelBox *boxes;
    uint16_t sideBoxCount; // kind 1: секция к соседу
    const BlockModelBox *sideBoxes;
    // kind 3: базовые ориентации (порядок поворотов k: [N, W, S, E] и [NE, NW, SW, SE])
    uint8_t backBase;      // куда смотрит «тяжёлая» сторона straight-модели
    uint8_t innerDiagBase; // диагональ заполненного угла inner-модели
    uint8_t outerDiagBase; // диагональ углового остатка outer-модели
    uint16_t innerBoxCount;
    const BlockModelBox *innerBoxes;
    uint16_t outerBoxCount;
    const BlockModelBox *outerBoxes;
};

namespace BlockModels {

// Все импортированные модели (порядок — как в реестре тула).
const BlockModelData *all(size_t &outCount);
// Модель по стабильному id; nullptr, если id неизвестен (модель удалена из реестра).
const BlockModelData *byId(uint8_t id);

} // namespace BlockModels
