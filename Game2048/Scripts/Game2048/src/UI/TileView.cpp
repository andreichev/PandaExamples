#include "UI/TileView.hpp"

#include "UI/Game2048Style.hpp"

#include <algorithm>
#include <string>

namespace Game2048 {

TileView::TileView() {
    setClipsToBounds(true);
    layoutSetAbsolute();
    layout().setFlexDirection(PandaUI::FlexDirection::Row);
    layout().setAlignItems(PandaUI::Align::Center);
    layout().setJustifyContent(PandaUI::Justify::Center);

    m_label = makeLabel("", 32.f, PandaUI::Color(0x776E65FF));
    m_label->setUserInteractionEnabled(false);
    addSubview(m_label);
}

void TileView::setTile(int value, float cellSize, float scale) {
    if (m_value != value) {
        m_value = value;
        m_label->setText(std::to_string(value));
    }

    const int digits = value >= 1000 ? 4 : value >= 100 ? 3 : value >= 10 ? 2 : 1;
    const float baseFontScale = digits >= 4 ? 0.25f : digits == 3 ? 0.30f : 0.36f;
    m_label->setFontSize(std::max(12.f, cellSize * baseFontScale * scale));
    m_label->setTextColor(tileTextColor(value));
    setBackgroundColor(tileColor(value));
}

void TileView::setVisualFrame(PandaUI::Point origin, float cellSize, float scale) {
    const float visualSize = cellSize * scale;
    const float inset = (cellSize - visualSize) * 0.5f;
    layout().setWidth(PandaUI::Length::points(visualSize));
    layout().setHeight(PandaUI::Length::points(visualSize));
    layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(origin.x + inset));
    layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(origin.y + inset));
}

} // namespace Game2048
