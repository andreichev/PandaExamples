#pragma once

#include <Bamboo/Base.hpp>

namespace ClawnDash {

struct Rect {
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;

    bool intersects(const Rect &other) const {
        return x - width * 0.5f < other.x + other.width * 0.5f &&
               x + width * 0.5f > other.x - other.width * 0.5f &&
               y - height * 0.5f < other.y + other.height * 0.5f &&
               y + height * 0.5f > other.y - other.height * 0.5f;
    }
};

struct Obstacle {
    Bamboo::EntityHandle entity;
    Rect bounds;
};

} // namespace ClawnDash
