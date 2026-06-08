#include "UI/DashHudView.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace ClawnDash {

namespace {

constexpr float ROOT_PADDING = 28.f;
constexpr float CARD_MAX_WIDTH = 430.f;

} // namespace

DashHudView::DashHudView() {
    setBackgroundColor(PandaUI::Color(0x00000000));
    style().setWidth(PandaUI::Length::percent(100.f));
    style().setHeight(PandaUI::Length::percent(100.f));
    style().setPadding(PandaUI::Edge::Top, ROOT_PADDING);
    style().setPadding(PandaUI::Edge::Left, ROOT_PADDING);
    style().setPadding(PandaUI::Edge::Right, ROOT_PADDING);
    style().setPadding(PandaUI::Edge::Bottom, ROOT_PADDING);
    style().setFlexDirection(PandaUI::FlexDirection::Column);
    style().setGap(12.f);
}

void DashHudView::setActions(
    LevelAction playLevel,
    Action showMainMenu,
    Action showLevelSelect,
    Action restartLevel,
    Action nextLevel
) {
    m_playLevel = std::move(playLevel);
    m_showMainMenu = std::move(showMainMenu);
    m_showLevelSelect = std::move(showLevelSelect);
    m_restartLevel = std::move(restartLevel);
    m_nextLevel = std::move(nextLevel);
}

void DashHudView::showMainMenu(const std::vector<LevelInfo> &levels) {
    removeAllSubviews();
    m_progressLabel.reset();
    m_statusLabel.reset();

    auto card = makeCard();
    card->style().setMargin(PandaUI::Edge::Top, 42.f);
    card->addSubview(makeLabel("CLAWN DASH", 42.f, PandaUI::Color(0xFFFFFFFF)));
    card->addSubview(makeLabel(
        "Reaction-first platforming. Levels are unlocked from the start.", 16.f, PandaUI::Color(0xBEE7FFFF)
    ));

    auto choose = makeButton("Choose Level", PandaUI::Color(0xFFD166FF));
    choose->setTextColor(PandaUI::Color(0x111827FF));
    choose->setOnClick([this](PandaUI::Button &) {
        if (m_showLevelSelect) { m_showLevelSelect(); }
    });
    card->addSubview(choose);

    if (!levels.empty()) {
        auto start = makeButton("Start Level 1", PandaUI::Color(0x5EEAD4FF));
        start->setTextColor(PandaUI::Color(0x09201DFF));
        start->setOnClick([this](PandaUI::Button &) {
            if (m_playLevel) { m_playLevel(0); }
        });
        card->addSubview(start);
    }

    addSubview(card);
}

void DashHudView::showLevelSelect(const std::vector<LevelInfo> &levels) {
    removeAllSubviews();
    m_progressLabel.reset();
    m_statusLabel.reset();

    auto card = makeCard();
    card->style().setMargin(PandaUI::Edge::Top, 18.f);
    card->addSubview(makeLabel("Select Level", 32.f, PandaUI::Color(0xFFFFFFFF)));
    card->addSubview(makeLabel(
        "No locks. Pick any level, learn it, restart quickly.", 15.f, PandaUI::Color(0xBEE7FFFF)
    ));

    for (std::size_t i = 0; i < levels.size(); ++i) {
        const LevelInfo &level = levels[i];
        auto button = makeButton(
            std::to_string(i + 1) + ". " + level.title + " - " + level.description,
            PandaUI::Color(level.accentColor)
        );
        button->setTextColor(PandaUI::Color(0x111827FF));
        button->setNumberOfLines(2);
        button->setOnClick([this, i](PandaUI::Button &) {
            if (m_playLevel) { m_playLevel(i); }
        });
        card->addSubview(button);
    }

    auto back = makeButton("Back", PandaUI::Color(0x22324DFF));
    back->setOnClick([this](PandaUI::Button &) {
        if (m_showMainMenu) { m_showMainMenu(); }
    });
    card->addSubview(back);
    addSubview(card);
}

void DashHudView::showGameplay(std::string levelTitle) {
    removeAllSubviews();

    auto topBar = std::make_shared<PandaUI::Panel>();
    topBar->setBackgroundColor(PandaUI::Color(0x00000000));
    topBar->style().setWidth(PandaUI::Length::percent(100.f));
    topBar->style().setFlexDirection(PandaUI::FlexDirection::Row);
    topBar->style().setGap(12.f);

    auto labels = std::make_shared<PandaUI::Panel>();
    labels->setBackgroundColor(PandaUI::Color(0x00000000));
    labels->style().setFlexGrow(1.f);
    labels->style().setFlexDirection(PandaUI::FlexDirection::Column);
    labels->style().setGap(6.f);
    labels->addSubview(makeLabel(std::move(levelTitle), 24.f, PandaUI::Color(0xFFFFFFFF)));

    m_progressLabel = makeLabel("Progress 0%", 17.f, PandaUI::Color(0xBEE7FFFF));
    labels->addSubview(m_progressLabel);

    m_statusLabel = makeLabel("Space / Up / tap to jump", 15.f, PandaUI::Color(0xFFD166FF));
    labels->addSubview(m_statusLabel);
    topBar->addSubview(labels);

    auto levels = makeButton("Levels", PandaUI::Color(0x22324DFF));
    levels->style().setWidth(PandaUI::Length::points(118.f));
    levels->setOnClick([this](PandaUI::Button &) {
        if (m_showLevelSelect) { m_showLevelSelect(); }
    });
    topBar->addSubview(levels);
    addSubview(topBar);
}

void DashHudView::showCrashed(std::string levelTitle) {
    removeAllSubviews();
    m_progressLabel.reset();
    m_statusLabel.reset();

    auto card = makeCard();
    card->style().setMargin(PandaUI::Edge::Top, 48.f);
    card->addSubview(makeLabel(std::move(levelTitle), 28.f, PandaUI::Color(0xFFFFFFFF)));
    card->addSubview(makeLabel("Crashed. Try the timing again.", 17.f, PandaUI::Color(0xFF8FA3FF)));

    auto restart = makeButton("Restart Level", PandaUI::Color(0xFF5C8AFF));
    restart->setOnClick([this](PandaUI::Button &) {
        if (m_restartLevel) { m_restartLevel(); }
    });
    card->addSubview(restart);

    auto levels = makeButton("Level Select", PandaUI::Color(0x22324DFF));
    levels->setOnClick([this](PandaUI::Button &) {
        if (m_showLevelSelect) { m_showLevelSelect(); }
    });
    card->addSubview(levels);
    addSubview(card);
}

void DashHudView::showFinished(std::string levelTitle, bool hasNextLevel) {
    removeAllSubviews();
    m_progressLabel.reset();
    m_statusLabel.reset();

    auto card = makeCard();
    card->style().setMargin(PandaUI::Edge::Top, 48.f);
    card->addSubview(makeLabel(std::move(levelTitle), 28.f, PandaUI::Color(0xFFFFFFFF)));
    card->addSubview(makeLabel("Level complete.", 18.f, PandaUI::Color(0x7CFFB2FF)));

    auto primary = makeButton(hasNextLevel ? "Next Level" : "Play Again", PandaUI::Color(0x7CFFB2FF));
    primary->setTextColor(PandaUI::Color(0x0C2115FF));
    primary->setOnClick([this, hasNextLevel](PandaUI::Button &) {
        if (hasNextLevel) {
            if (m_nextLevel) { m_nextLevel(); }
        } else if (m_restartLevel) {
            m_restartLevel();
        }
    });
    card->addSubview(primary);

    auto levels = makeButton("Level Select", PandaUI::Color(0x22324DFF));
    levels->setOnClick([this](PandaUI::Button &) {
        if (m_showLevelSelect) { m_showLevelSelect(); }
    });
    card->addSubview(levels);
    addSubview(card);
}

void DashHudView::setProgress(float progress) {
    if (!m_progressLabel) {
        return;
    }
    const int percent = std::clamp(static_cast<int>(progress * 100.f), 0, 100);
    m_progressLabel->setText("Progress " + std::to_string(percent) + "%");
}

void DashHudView::setStatus(std::string status, PandaUI::Color color) {
    if (!m_statusLabel) {
        return;
    }
    m_statusLabel->setText(std::move(status));
    m_statusLabel->setTextColor(color);
}

std::shared_ptr<PandaUI::Label> DashHudView::makeLabel(std::string text, float size, PandaUI::Color color) {
    auto label = std::make_shared<PandaUI::Label>(std::move(text));
    label->setFontSize(size);
    label->setTextColor(color);
    label->setNumberOfLines(2);
    label->style().setWidth(PandaUI::Length::percent(100.f));
    return label;
}

std::shared_ptr<PandaUI::Button> DashHudView::makeButton(std::string text, PandaUI::Color color) {
    auto button = std::make_shared<PandaUI::Button>(std::move(text));
    button->style().setWidth(PandaUI::Length::percent(100.f));
    button->style().setHeight(PandaUI::Length::points(46.f));
    button->setNormalColor(color);
    button->setHoveredColor(PandaUI::Color(0x4A92FFFF));
    button->setPressedColor(PandaUI::Color(0x1C58BDFF));
    button->setTextColor(PandaUI::Color(0xFFFFFFFF));
    button->setFontSize(15.f);
    return button;
}

std::shared_ptr<PandaUI::Panel> DashHudView::makeCard() {
    auto card = std::make_shared<PandaUI::Panel>();
    card->setBackgroundColor(PandaUI::Color(0x101827E8));
    card->style().setWidth(PandaUI::Length::percent(100.f));
    card->style().setMaxWidth(PandaUI::Length::points(CARD_MAX_WIDTH));
    card->style().setPadding(PandaUI::Edge::Horizontal, 20.f);
    card->style().setPadding(PandaUI::Edge::Vertical, 18.f);
    card->style().setGap(12.f);
    card->style().setFlexDirection(PandaUI::FlexDirection::Column);
    return card;
}

} // namespace ClawnDash
