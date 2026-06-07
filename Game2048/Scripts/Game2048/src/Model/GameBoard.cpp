#include "Model/GameBoard.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>

namespace Game2048 {

namespace {

    constexpr float MOVE_SPEED = 10.f;
    constexpr float MOVE_STOP_DISTANCE = 0.06f;
    constexpr float APPEAR_DURATION = 0.3f;
    constexpr float APPEAR_DELAY = 0.3f;

    GridPoint pointFor(Pos pos) {
        return {static_cast<float>(pos.x), static_cast<float>(pos.y)};
    }

    float distance(GridPoint lhs, GridPoint rhs) {
        const float dx = lhs.x - rhs.x;
        const float dy = lhs.y - rhs.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    GridPoint moveTowards(GridPoint current, GridPoint target, float maxDistanceDelta) {
        const float dx = target.x - current.x;
        const float dy = target.y - current.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= maxDistanceDelta || length <= 0.0001f) {
            return target;
        }
        const float scale = maxDistanceDelta / length;
        return {current.x + dx * scale, current.y + dy * scale};
    }

} // namespace

GameBoard::GameBoard()
    : m_random(static_cast<unsigned int>(std::time(nullptr))) {
    reset();
}

void GameBoard::reset() {
    clear();
    addRandomNumber();
    addRandomNumber();
}

void GameBoard::clear() {
    for (auto &column : m_cells) {
        for (Cell &cell : column) {
            cell = {};
        }
    }
    m_score = 0;
    m_gameOver = false;
    clearAnimations();
}

void GameBoard::clearAnimations() {
    for (TileAnimation &animation : m_moveAnimations) {
        animation = {};
    }
    for (TileAnimation &animation : m_joinAnimations) {
        animation = {};
    }
    for (AppearAnimation &animation : m_appearAnimations) {
        animation = {};
    }
}

void GameBoard::update(float deltaTime) {
    for (TileAnimation &animation : m_moveAnimations) {
        updateTileAnimation(animation, deltaTime);
    }
    for (TileAnimation &animation : m_joinAnimations) {
        updateTileAnimation(animation, deltaTime);
    }
    for (AppearAnimation &animation : m_appearAnimations) {
        updateAppearAnimation(animation, deltaTime);
    }
}

bool GameBoard::move(Direction direction) {
    if (m_gameOver || !canAcceptMove()) {
        return false;
    }

    int offsetX = 0;
    int offsetY = 0;
    switch (direction) {
        case Direction::Up:
            offsetY = -1;
            break;
        case Direction::Down:
            offsetY = 1;
            break;
        case Direction::Left:
            offsetX = -1;
            break;
        case Direction::Right:
            offsetX = 1;
            break;
    }

    bool moved = false;
    for (int i = 0; i < BOARD_SIZE; ++i) {
        if (!movePass(offsetX, offsetY)) {
            break;
        }
        moved = true;
    }

    if (moved) {
        addRandomNumber();
    }
    prepareForNextMove();
    if (!hasFreeCell()) {
        checkIfGameOver();
    }
    return moved;
}

bool GameBoard::isAnimating() const {
    return !canAcceptMove();
}

bool GameBoard::canAcceptMove() const {
    for (const TileAnimation &animation : m_moveAnimations) {
        if (animation.active) {
            return false;
        }
    }
    for (const TileAnimation &animation : m_joinAnimations) {
        if (animation.active) {
            return false;
        }
    }
    for (const AppearAnimation &animation : m_appearAnimations) {
        if (animation.active) {
            return false;
        }
    }
    return true;
}

bool GameBoard::hasFreeCell() const {
    for (const auto &column : m_cells) {
        for (const Cell &cell : column) {
            if (cell.isZero()) {
                return true;
            }
        }
    }
    return false;
}

Pos GameBoard::getFreePos() {
    std::array<Pos, BOARD_SIZE * BOARD_SIZE> freePositions;
    int count = 0;
    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            if (m_cells[x][y].isZero()) {
                freePositions[count] = Pos(x, y);
                ++count;
            }
        }
    }
    if (count == 0) {
        return {};
    }
    std::uniform_int_distribution<int> distribution(0, count - 1);
    return freePositions[distribution(m_random)];
}

void GameBoard::addRandomNumber() {
    Pos freePos = getFreePos();
    if (!freePos.isValid()) {
        m_gameOver = true;
        return;
    }

    std::uniform_int_distribution<int> distribution(0, 1);
    const int value = distribution(m_random) == 0 ? 2 : 4;
    Cell &cell = m_cells[freePos.x][freePos.y];
    cell.value = value;
    cell.hidden = true;

    addAppearAnimation({freePos, value, APPEAR_DURATION, APPEAR_DELAY, APPEAR_DURATION, true});
}

void GameBoard::checkIfGameOver() {
    if (hasFreeCell()) {
        return;
    }
    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            const int value = m_cells[x][y].value;
            if (x + 1 < BOARD_SIZE && value == m_cells[x + 1][y].value) {
                return;
            }
            if (x - 1 >= 0 && value == m_cells[x - 1][y].value) {
                return;
            }
            if (y + 1 < BOARD_SIZE && value == m_cells[x][y + 1].value) {
                return;
            }
            if (y - 1 >= 0 && value == m_cells[x][y - 1].value) {
                return;
            }
        }
    }
    m_gameOver = true;
}

bool GameBoard::movePass(int offsetX, int offsetY) {
    bool moved = false;
    const int startY = offsetY > 0 ? BOARD_SIZE - 1 : 0;
    const int startX = offsetX > 0 ? BOARD_SIZE - 1 : 0;
    const int stepX = offsetX > 0 ? -1 : 1;
    const int stepY = offsetY > 0 ? -1 : 1;

    for (int y = startY; offsetY > 0 ? y >= 0 : y < BOARD_SIZE; y += stepY) {
        for (int x = startX; offsetX > 0 ? x >= 0 : x < BOARD_SIZE; x += stepX) {
            Cell cell = m_cells[x][y];
            if (cell.isZero()) {
                continue;
            }
            if (moveCell(Pos(x, y), offsetX, offsetY, cell.value)) {
                moved = true;
            }
        }
    }
    return moved;
}

bool GameBoard::moveCell(Pos pos, int offsetX, int offsetY, int value) {
    bool moved = false;
    Pos nextPos = pos;
    TileAnimation moveAnimation;

    while (true) {
        Pos lastPos = nextPos;
        nextPos.x += offsetX;
        nextPos.y += offsetY;
        if (nextPos.y < 0 || nextPos.y >= BOARD_SIZE || nextPos.x < 0 || nextPos.x >= BOARD_SIZE) {
            break;
        }

        Cell &nextCell = m_cells[nextPos.x][nextPos.y];
        Cell &lastCell = m_cells[lastPos.x][lastPos.y];
        if (nextCell.isZero()) {
            nextCell.value = value;
            nextCell.merged = lastCell.merged;
            lastCell.value = 0;
            lastCell.hidden = false;
            moveAnimation = {pointFor(pos), pointFor(nextPos), nextPos, value, true};
            moved = true;
            continue;
        }

        if (!nextCell.merged && !lastCell.merged && nextCell.value == value) {
            nextCell.value = value * 2;
            nextCell.merged = true;
            nextCell.hidden = true;
            lastCell.value = 0;
            lastCell.hidden = false;
            m_score += value * 2;
            addJoinAnimation({pointFor(pos), pointFor(nextPos), nextPos, value * 2, true});
            return true;
        }

        break;
    }

    if (moveAnimation.active) {
        addMoveAnimation(moveAnimation);
    }
    return moved;
}

void GameBoard::prepareForNextMove() {
    for (auto &column : m_cells) {
        for (Cell &cell : column) {
            cell.merged = false;
        }
    }
}

void GameBoard::addMoveAnimation(TileAnimation animation) {
    for (TileAnimation &slot : m_moveAnimations) {
        if (slot.active) {
            continue;
        }
        slot = animation;
        m_cells[animation.revealPos.x][animation.revealPos.y].hidden = true;
        return;
    }
    revealCell(animation.revealPos);
}

void GameBoard::addJoinAnimation(TileAnimation animation) {
    for (TileAnimation &slot : m_joinAnimations) {
        if (slot.active) {
            continue;
        }
        slot = animation;
        return;
    }
    revealCell(animation.revealPos);
}

void GameBoard::addAppearAnimation(AppearAnimation animation) {
    for (AppearAnimation &slot : m_appearAnimations) {
        if (slot.active) {
            continue;
        }
        slot = animation;
        return;
    }
    revealCell(animation.pos);
}

void GameBoard::revealCell(Pos pos) {
    if (!pos.isValid()) {
        return;
    }
    m_cells[pos.x][pos.y].hidden = false;
}

void GameBoard::updateTileAnimation(TileAnimation &animation, float deltaTime) {
    if (!animation.active) {
        return;
    }
    animation.current = moveTowards(animation.current, animation.target, deltaTime * MOVE_SPEED);
    if (distance(animation.current, animation.target) < MOVE_STOP_DISTANCE) {
        animation.active = false;
        animation.current = animation.target;
        revealCell(animation.revealPos);
    }
}

void GameBoard::updateAppearAnimation(AppearAnimation &animation, float deltaTime) {
    if (!animation.active) {
        return;
    }
    if (animation.delay > 0.f) {
        animation.delay = std::max(0.f, animation.delay - deltaTime);
        return;
    }
    animation.timeLeft -= deltaTime;
    if (animation.timeLeft <= 0.f) {
        animation.active = false;
        revealCell(animation.pos);
    }
}

} // namespace Game2048
