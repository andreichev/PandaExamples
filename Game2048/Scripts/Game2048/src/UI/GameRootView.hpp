#pragma once

#include "Model/GameBoard.hpp"

#include <PandaUI/PandaUI.hpp>

#include <memory>
#include <optional>

namespace Game2048 {

class BoardView;
class HeaderView;

class GameRootView final : public PandaUI::Panel {
public:
    GameRootView();

    void update(double deltaTime) override;
    void move(Direction direction);
    void reset();

private:
    GameBoard m_board;
    std::shared_ptr<HeaderView> m_headerView;
    std::shared_ptr<BoardView> m_boardView;
    std::shared_ptr<PandaUI::Label> m_hintLabel;
    std::optional<Direction> m_pendingDirection;
    float m_lastContentWidth = 0.f;

    float calculateBoardSize() const;
    void applyResponsiveLayout();
    void applyPendingMoveIfReady();
};

} // namespace Game2048
