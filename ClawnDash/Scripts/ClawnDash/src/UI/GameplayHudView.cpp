#include "UI/GameplayHudView.hpp"

#include "UI/DashUIStyle.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace ClawnDash {

GameplayHudView::GameplayHudView(std::string levelTitle, Action openMenu) {
    setBackgroundColor(PandaUI::Color(0x00000000));
    layout().setWidth(PandaUI::Length::percent(100.f));
    layout().setHeight(PandaUI::Length::percent(100.f));
    layout().setPadding(PandaUI::Edge::Top, 28.f);
    layout().setPadding(PandaUI::Edge::Left, 28.f);
    layout().setPadding(PandaUI::Edge::Right, 28.f);
    layout().setFlexDirection(PandaUI::FlexDirection::Column);

    auto topBar = std::make_shared<PandaUI::Panel>();
    topBar->setBackgroundColor(PandaUI::Color(0x00000000));
    topBar->layout().setWidth(PandaUI::Length::percent(100.f));
    topBar->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    topBar->layout().setGap(12.f);

    auto labels = std::make_shared<PandaUI::Panel>();
    labels->setBackgroundColor(PandaUI::Color(0x00000000));
    labels->layout().setFlexGrow(1.f);
    labels->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    labels->layout().setGap(6.f);
    labels->addSubview(makeLabel(std::move(levelTitle), 24.f, PandaUI::Color(0xFFFFFFFF), 1));

    m_progressLabel = makeLabel("Progress 0%", 17.f, PandaUI::Color(0xBEE7FFFF), 1);
    labels->addSubview(m_progressLabel);

    m_statusLabel = makeLabel(
        "Space / Up / tap to jump. Press on blue orbs in air.", 15.f, PandaUI::Color(0xFFD166FF)
    );
    labels->addSubview(m_statusLabel);
    topBar->addSubview(labels);

    auto menu = makeButton("Menu", PandaUI::Color(0x22324DFF));
    menu->layout().setWidth(PandaUI::Length::points(112.f));
    menu->setOnClick([action = std::move(openMenu)](PandaUI::Button &) {
        if (action) {
            action();
        }
    });
    topBar->addSubview(menu);
    addSubview(topBar);
}

void GameplayHudView::setProgress(float progress) {
    if (!m_progressLabel) {
        return;
    }
    const int percent = std::clamp(static_cast<int>(progress * 100.f), 0, 100);
    m_progressLabel->setText("Progress " + std::to_string(percent) + "%");
}

void GameplayHudView::setStatus(std::string status, PandaUI::Color color) {
    if (!m_statusLabel) {
        return;
    }
    m_statusLabel->setText(std::move(status));
    m_statusLabel->setTextColor(color);
}

} // namespace ClawnDash
