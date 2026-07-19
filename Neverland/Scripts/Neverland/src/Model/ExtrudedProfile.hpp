#pragma once

#include <Bamboo/Assets/MeshAPI.hpp>
#include <Bamboo/Base.hpp>

#include <vector>

using namespace Bamboo;

// IR архитектурных примитивов (техплан, вводится на карнизе): 2D-профиль,
// экструдированный вдоль полилинии пути. Повороты пути — митра (поперечник по
// биссектрисе, масштаб 1/cos(θ/2)); нормали фасетные (чёткие грани профиля);
// продольный UV — накопленная длина пути (метр = тайл).
namespace ExtrudedProfile {

// Точка сечения: offset — поперёк пути (вправо по ходу), height — вверх от базы.
struct ProfilePoint {
    float offset;
    float height;
};

// Кольцо экструзии: позиция базы и направления в точке пути.
struct PathRing {
    Vec3 base;      // точка пути (низ профиля)
    Vec3 side;      // поперечное направление (митра, с масштабом)
    float along;    // накопленная длина (продольный UV)
    bool chunkOwned; // отрезок ЗА этим кольцом рисует текущий чанк
};

// Экструзия профиля по готовым кольцам. closed — путь замкнут (без торцов);
// торцы открытых концов рисуются, если их отрезок принадлежит чанку.
// Тайл — из blocks-атласа (7×7), tint — вершинный цвет.
void extrude(
    const std::vector<ProfilePoint> &profile,
    const std::vector<PathRing> &rings,
    bool closed,
    uint8_t tileIndex,
    uint32_t tint,
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices
);

} // namespace ExtrudedProfile
