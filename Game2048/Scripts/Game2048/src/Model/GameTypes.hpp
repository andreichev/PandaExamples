#pragma once

#include <array>

namespace Game2048 {

constexpr int BOARD_SIZE = 4;
constexpr int MAX_TILE_ANIMATIONS = BOARD_SIZE * BOARD_SIZE;

struct Pos {
    int x = -1;
    int y = -1;

    Pos() = default;
    Pos(int x, int y)
        : x(x)
        , y(y) {}

    bool isValid() const {
        return x >= 0 && y >= 0;
    }
};

struct GridPoint {
    float x = 0.f;
    float y = 0.f;

    GridPoint() = default;
    GridPoint(float x, float y)
        : x(x)
        , y(y) {}
};

enum class Direction {
    Up,
    Down,
    Left,
    Right,
};

struct Cell {
    int value = 0;
    bool merged = false;
    bool hidden = false;

    bool isZero() const {
        return value == 0;
    }
};

struct TileAnimation {
    GridPoint current;
    GridPoint target;
    Pos revealPos;
    int value = 0;
    bool active = false;
};

struct AppearAnimation {
    Pos pos;
    int value = 0;
    float timeLeft = 0.f;
    float delay = 0.f;
    float duration = 0.f;
    bool active = false;
};

using CellGrid = std::array<std::array<Cell, BOARD_SIZE>, BOARD_SIZE>;
using TileAnimations = std::array<TileAnimation, MAX_TILE_ANIMATIONS>;
using AppearAnimations = std::array<AppearAnimation, BOARD_SIZE>;

} // namespace Game2048
