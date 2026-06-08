#pragma once

#include <Bamboo/Base.hpp>

#include <cstdint>
#include <string>

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

struct Trigger {
    Bamboo::EntityHandle entity;
    Rect bounds;
    bool used = false;
};

struct Portal {
    Bamboo::EntityHandle entity;
    Rect bounds;
    bool used = false;
};

struct LevelInfo {
    std::string title;
    std::string description;
    uint32_t backgroundColor = 0x090E20FF;
    uint32_t groundColor = 0x202B3EFF;
    uint32_t groundGlowColor = 0x5EEAD4AA;
    uint32_t accentColor = 0xFFD166FF;
};

} // namespace ClawnDash
