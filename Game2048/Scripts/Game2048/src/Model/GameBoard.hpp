#pragma once

#include "Model/GameTypes.hpp"

#include <random>

namespace Game2048 {

class GameBoard final {
public:
    GameBoard();

    void reset();
    void update(float deltaTime);
    bool move(Direction direction);

    int getScore() const {
        return m_score;
    }

    bool isGameOver() const {
        return m_gameOver;
    }

    bool isAnimating() const;

    const CellGrid &getCells() const {
        return m_cells;
    }

    const TileAnimations &getMoveAnimations() const {
        return m_moveAnimations;
    }

    const TileAnimations &getJoinAnimations() const {
        return m_joinAnimations;
    }

    const AppearAnimations &getAppearAnimations() const {
        return m_appearAnimations;
    }

private:
    CellGrid m_cells;
    TileAnimations m_moveAnimations;
    TileAnimations m_joinAnimations;
    AppearAnimations m_appearAnimations;
    std::mt19937 m_random;
    int m_score = 0;
    bool m_gameOver = false;

    void clear();
    void clearAnimations();
    bool canAcceptMove() const;
    bool hasFreeCell() const;
    Pos getFreePos();
    void addRandomNumber();
    void checkIfGameOver();
    bool movePass(int offsetX, int offsetY);
    bool moveCell(Pos pos, int offsetX, int offsetY, int value);
    void prepareForNextMove();
    void addMoveAnimation(TileAnimation animation);
    void addJoinAnimation(TileAnimation animation);
    void addAppearAnimation(AppearAnimation animation);
    void revealCell(Pos pos);
    void updateTileAnimation(TileAnimation &animation, float deltaTime);
    void updateAppearAnimation(AppearAnimation &animation, float deltaTime);
};

} // namespace Game2048
