#include "UI/DashUIStyle.hpp"

#include <utility>

namespace ClawnDash {

std::shared_ptr<PandaUI::Label>
makeLabel(std::string text, float size, PandaUI::Color color, int lines) {
    auto label = std::make_shared<PandaUI::Label>(std::move(text));
    label->setFontSize(size);
    label->setTextColor(color);
    label->setNumberOfLines(lines);
    label->style().setWidth(PandaUI::Length::percent(100.f));
    return label;
}

std::shared_ptr<PandaUI::Button> makeButton(std::string text, PandaUI::Color color) {
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

std::shared_ptr<PandaUI::Panel> makeMenuCard() {
    auto card = std::make_shared<PandaUI::Panel>();
    card->setBackgroundColor(PandaUI::Color(0x101827E8));
    card->style().setWidth(PandaUI::Length::percent(100.f));
    card->style().setMaxWidth(PandaUI::Length::points(460.f));
    card->style().setPadding(PandaUI::Edge::Horizontal, 20.f);
    card->style().setPadding(PandaUI::Edge::Vertical, 18.f);
    card->style().setGap(12.f);
    card->style().setFlexDirection(PandaUI::FlexDirection::Column);
    return card;
}

} // namespace ClawnDash
