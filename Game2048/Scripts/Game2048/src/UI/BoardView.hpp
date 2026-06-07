#pragma once

#include "Model/GameBoard.hpp"

#include <PandaUI/PandaUI.hpp>

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace Game2048 {

class GameOverView;
class TileView;

class BoardView final : public PandaUI::Panel {
public:
    using MoveCallback = std::function<void(Direction)>;
    using Action = std::function<void()>;

    BoardView(GameBoard &board, MoveCallback onMove, Action onRestart);

    void update(double deltaTime) override;
    bool pointerDown(PandaUI::PointerEvent &event) override;
    bool pointerUp(PandaUI::PointerEvent &event) override;

private:
    struct Metrics {
        float boardSize = 0.f;
        float gap = 0.f;
        float cellSize = 0.f;
    };

    GameBoard &m_board;
    MoveCallback m_onMove;
    std::shared_ptr<PandaUI::Panel> m_cellLayer;
    std::shared_ptr<PandaUI::Panel> m_tileLayer;
    std::shared_ptr<GameOverView> m_gameOverView;
    std::array<std::shared_ptr<PandaUI::Panel>, BOARD_SIZE * BOARD_SIZE> m_cells;
    std::vector<std::shared_ptr<TileView>> m_tilePool;
    PandaUI::Point m_pointerDownPosition;
    int m_pointerId = 0;
    bool m_isPointerDown = false;
    float m_lastBoardSize = 0.f;

    Metrics currentMetrics() const;
    PandaUI::Point originFor(GridPoint point, const Metrics &metrics) const;
    PandaUI::Point originFor(Pos pos, const Metrics &metrics) const;
    void syncCells(const Metrics &metrics);
    void syncTiles(const Metrics &metrics);
    void addTile(int &usedTiles, int value, GridPoint point, float scale, const Metrics &metrics);
    std::shared_ptr<TileView> acquireTile(int index);
    float appearScale(const AppearAnimation &animation) const;
    bool acceptsPointer(const PandaUI::PointerEvent &event) const;
    void handleSwipe(PandaUI::Point from, PandaUI::Point to);
};

} // namespace Game2048
