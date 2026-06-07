#include "UI/DashHudView.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace ClawnDash {

namespace {

    std::shared_ptr<PandaUI::Label> makeLabel(std::string text, float size, PandaUI::Color color) {
        auto label = std::make_shared<PandaUI::Label>(std::move(text));
        label->setFontSize(size);
        label->setTextColor(color);
        label->setNumberOfLines(1);
        label->style().setWidth(PandaUI::Length::percent(100.f));
        return label;
    }

} // namespace

DashHudView::DashHudView() {
    setBackgroundColor(PandaUI::Color(0x00000000));
    setUserInteractionEnabled(false);
    style().setWidth(PandaUI::Length::percent(100.f));
    style().setHeight(PandaUI::Length::percent(100.f));
    style().setPadding(PandaUI::Edge::Top, 28.f);
    style().setPadding(PandaUI::Edge::Left, 28.f);
    style().setPadding(PandaUI::Edge::Right, 28.f);
    style().setFlexDirection(PandaUI::FlexDirection::Column);
    style().setGap(8.f);

    auto title = makeLabel("CLAWN DASH", 30.f, PandaUI::Color(0xFFFFFFFF));
    addSubview(title);

    m_progressLabel = makeLabel("Progress 0%", 18.f, PandaUI::Color(0xBEE7FFFF));
    addSubview(m_progressLabel);

    m_statusLabel = makeLabel("Space / Up / tap to jump", 16.f, PandaUI::Color(0xFFD166FF));
    addSubview(m_statusLabel);
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

} // namespace ClawnDash
