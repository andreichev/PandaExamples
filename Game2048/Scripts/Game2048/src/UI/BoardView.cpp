#include "UI/BoardView.hpp"

#include "UI/Game2048Style.hpp"
#include "UI/GameOverView.hpp"
#include "UI/TileView.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Game2048 {

namespace {

    constexpr float SWIPE_THRESHOLD = 32.f;
    constexpr float PI = 3.14159265358979323846f;

} // namespace

BoardView::BoardView(GameBoard &board, MoveCallback onMove, Action onRestart)
    : m_board(board)
    , m_onMove(std::move(onMove)) {
    setBackgroundColor(PandaUI::Color(0xBBADA0FF));
    setClipsToBounds(true);
    setUserInteractionEnabled(true);

    m_cellLayer = std::make_shared<PandaUI::Panel>();
    m_cellLayer->setBackgroundColor(transparent());
    m_cellLayer->styleSetAbsolute();
    m_cellLayer->styleSetInsetsFromParent({});
    addSubview(m_cellLayer);

    for (auto &cell : m_cells) {
        cell = std::make_shared<PandaUI::Panel>();
        cell->setBackgroundColor(PandaUI::Color(0xCDC1B4FF));
        cell->styleSetAbsolute();
        m_cellLayer->addSubview(cell);
    }

    m_tileLayer = std::make_shared<PandaUI::Panel>();
    m_tileLayer->setBackgroundColor(transparent());
    m_tileLayer->styleSetAbsolute();
    m_tileLayer->styleSetInsetsFromParent({});
    m_tileLayer->setUserInteractionEnabled(false);
    addSubview(m_tileLayer);

    m_gameOverView = std::make_shared<GameOverView>(std::move(onRestart));
    m_gameOverView->setHidden(true);
    addSubview(m_gameOverView);
}

void BoardView::update(double) {
    const Metrics metrics = currentMetrics();
    if (metrics.boardSize <= 0.f) {
        return;
    }
    if (std::abs(metrics.boardSize - m_lastBoardSize) > 0.1f) {
        syncCells(metrics);
        m_lastBoardSize = metrics.boardSize;
    }
    syncTiles(metrics);
    m_gameOverView->setHidden(!m_board.isGameOver());
}

bool BoardView::pointerDown(PandaUI::PointerEvent &event) {
    if (!acceptsPointer(event)) {
        return false;
    }
    m_pointerDownPosition = event.position;
    m_pointerId = event.pointerId;
    m_isPointerDown = true;
    return true;
}

bool BoardView::pointerUp(PandaUI::PointerEvent &event) {
    if (!m_isPointerDown || event.pointerId != m_pointerId) {
        return false;
    }
    m_isPointerDown = false;
    handleSwipe(m_pointerDownPosition, event.position);
    return true;
}

BoardView::Metrics BoardView::currentMetrics() const {
    Metrics metrics;
    PandaUI::Rect frame = const_cast<BoardView *>(this)->getFrame();
    metrics.boardSize = std::min(frame.size.width, frame.size.height);
    metrics.gap = std::max(8.f, metrics.boardSize * 0.026f);
    metrics.cellSize = (metrics.boardSize - metrics.gap * (BOARD_SIZE + 1)) / BOARD_SIZE;
    return metrics;
}

PandaUI::Point BoardView::originFor(GridPoint point, const Metrics &metrics) const {
    return {
        metrics.gap + point.x * (metrics.cellSize + metrics.gap),
        metrics.gap + point.y * (metrics.cellSize + metrics.gap),
    };
}

PandaUI::Point BoardView::originFor(Pos pos, const Metrics &metrics) const {
    return originFor(GridPoint(static_cast<float>(pos.x), static_cast<float>(pos.y)), metrics);
}

void BoardView::syncCells(const Metrics &metrics) {
    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            std::shared_ptr<PandaUI::Panel> cell = m_cells[y * BOARD_SIZE + x];
            PandaUI::Point origin = originFor(Pos(x, y), metrics);
            cell->style().setWidth(PandaUI::Length::points(metrics.cellSize));
            cell->style().setHeight(PandaUI::Length::points(metrics.cellSize));
            cell->style().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(origin.x));
            cell->style().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(origin.y));
        }
    }
}

void BoardView::syncTiles(const Metrics &metrics) {
    int usedTiles = 0;
    const CellGrid &cells = m_board.getCells();
    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            const Cell &cell = cells[x][y];
            if (cell.isZero() || cell.hidden) {
                continue;
            }
            addTile(
                usedTiles,
                cell.value,
                GridPoint(static_cast<float>(x), static_cast<float>(y)),
                1.f,
                metrics
            );
        }
    }

    for (const TileAnimation &animation : m_board.getMoveAnimations()) {
        if (!animation.active) {
            continue;
        }
        addTile(usedTiles, animation.value, animation.current, 1.f, metrics);
    }

    for (const TileAnimation &animation : m_board.getJoinAnimations()) {
        if (!animation.active) {
            continue;
        }
        addTile(usedTiles, animation.value, animation.current, 1.f, metrics);
    }

    for (const AppearAnimation &animation : m_board.getAppearAnimations()) {
        if (!animation.active || animation.delay > 0.f) {
            continue;
        }
        addTile(
            usedTiles,
            animation.value,
            GridPoint(static_cast<float>(animation.pos.x), static_cast<float>(animation.pos.y)),
            appearScale(animation),
            metrics
        );
    }

    for (int i = usedTiles; i < static_cast<int>(m_tilePool.size()); ++i) {
        m_tilePool[i]->setHidden(true);
    }
}

void BoardView::addTile(
    int &usedTiles, int value, GridPoint point, float scale, const Metrics &metrics
) {
    std::shared_ptr<TileView> tile = acquireTile(usedTiles);
    tile->setHidden(false);
    tile->setTile(value, metrics.cellSize, scale);
    tile->setVisualFrame(originFor(point, metrics), metrics.cellSize, scale);
    ++usedTiles;
}

std::shared_ptr<TileView> BoardView::acquireTile(int index) {
    while (index >= static_cast<int>(m_tilePool.size())) {
        auto tile = std::make_shared<TileView>();
        m_tilePool.push_back(tile);
        m_tileLayer->addSubview(tile);
    }
    return m_tilePool[index];
}

float BoardView::appearScale(const AppearAnimation &animation) const {
    if (animation.duration <= 0.f) {
        return 1.f;
    }
    const float progress = animation.timeLeft / animation.duration;
    return 1.f + 0.5f * std::sin(progress * PI);
}

bool BoardView::acceptsPointer(const PandaUI::PointerEvent &event) const {
    if (event.type == PandaUI::PointerType::Touch) {
        return true;
    }
    return event.button == PandaUI::PointerButton::Left;
}

void BoardView::handleSwipe(PandaUI::Point from, PandaUI::Point to) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    if (std::max(std::abs(dx), std::abs(dy)) < SWIPE_THRESHOLD) {
        return;
    }

    if (std::abs(dx) > std::abs(dy)) {
        if (m_onMove) {
            m_onMove(dx > 0.f ? Direction::Right : Direction::Left);
        }
    } else {
        if (m_onMove) {
            m_onMove(dy > 0.f ? Direction::Down : Direction::Up);
        }
    }
}

} // namespace Game2048
