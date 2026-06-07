#include "UI/GameRootView.hpp"

#include "UI/BoardView.hpp"
#include "UI/Game2048Style.hpp"
#include "UI/HeaderView.hpp"

#include <algorithm>

namespace Game2048 {

GameRootView::GameRootView() {
    setBackgroundColor(PandaUI::Color(0x17120EFF));
    style().setFlexDirection(PandaUI::FlexDirection::Column);

    auto safeArea = std::make_shared<PandaUI::SafeAreaView>();
    safeArea->setBackgroundColor(transparent());
    safeArea->style().setFlexGrow(1.f);
    safeArea->style().setWidth(PandaUI::Length::percent(100.f));
    safeArea->style().setFlexDirection(PandaUI::FlexDirection::Column);
    safeArea->style().setAlignItems(PandaUI::Align::Center);
    safeArea->style().setJustifyContent(PandaUI::Justify::Center);

    auto content = std::make_shared<PandaUI::Panel>();
    content->setBackgroundColor(transparent());
    content->style().setFlexDirection(PandaUI::FlexDirection::Column);
    content->style().setAlignItems(PandaUI::Align::Center);
    content->style().setJustifyContent(PandaUI::Justify::Center);
    content->style().setGap(18.f);
    content->style().setPadding(24.f);
    content->style().setFlexGrow(1.f);
    content->style().setWidth(PandaUI::Length::percent(100.f));

    m_headerView = std::make_shared<HeaderView>([this]() { reset(); });
    content->addSubview(m_headerView);

    m_boardView = std::make_shared<BoardView>(
        m_board, [this](Direction direction) { move(direction); }, [this]() { reset(); }
    );
    content->addSubview(m_boardView);

    m_hintLabel =
        makeLabel("Use arrow keys, WASD, or swipe on the board", 16.f, PandaUI::Color(0xEEE4DAFF));
    content->addSubview(m_hintLabel);

    safeArea->addSubview(content);
    addSubview(safeArea);
}

void GameRootView::update(double deltaTime) {
    m_board.update(static_cast<float>(deltaTime));
    applyPendingMoveIfReady();
    if (m_headerView) {
        m_headerView->setScore(m_board.getScore());
    }
    applyResponsiveLayout();
}

void GameRootView::move(Direction direction) {
    if (m_board.move(direction)) {
        m_pendingDirection.reset();
        return;
    }
    if (m_board.isAnimating() && !m_board.isGameOver()) {
        m_pendingDirection = direction;
    }
}

void GameRootView::reset() {
    m_pendingDirection.reset();
    m_board.reset();
}

float GameRootView::calculateBoardSize() const {
    PandaUI::Rect frame = const_cast<GameRootView *>(this)->getFrame();
    if (frame.size.width <= 0.f || frame.size.height <= 0.f) {
        return 0.f;
    }
    const float maxByWidth = frame.size.width - 48.f;
    const float maxByHeight = frame.size.height - 190.f;
    return std::clamp(std::min(maxByWidth, maxByHeight), 280.f, 560.f);
}

void GameRootView::applyResponsiveLayout() {
    const float boardSize = calculateBoardSize();
    if (boardSize <= 0.f || std::abs(boardSize - m_lastContentWidth) < 0.1f) {
        return;
    }
    m_lastContentWidth = boardSize;

    if (m_headerView) {
        m_headerView->setContentWidth(boardSize);
    }
    if (m_boardView) {
        m_boardView->style().setWidth(PandaUI::Length::points(boardSize));
        m_boardView->style().setHeight(PandaUI::Length::points(boardSize));
    }
    if (m_hintLabel) {
        m_hintLabel->style().setWidth(PandaUI::Length::points(boardSize));
    }
}

void GameRootView::applyPendingMoveIfReady() {
    if (!m_pendingDirection || m_board.isAnimating() || m_board.isGameOver()) {
        return;
    }
    const Direction direction = *m_pendingDirection;
    m_pendingDirection.reset();
    m_board.move(direction);
}

} // namespace Game2048
