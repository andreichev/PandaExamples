#include "UI/DashUIStyle.hpp"

#include <utility>

namespace ClawnDash {

std::shared_ptr<PandaUI::Label>
makeLabel(std::string text, float size, PandaUI::Color color, int lines) {
    auto label = std::make_shared<PandaUI::Label>(std::move(text));
    label->setTextColor(color);
    label->setNumberOfLines(lines);
    label->layout().setWidth(PandaUI::Length::percent(100.f));
    return label;
}

std::shared_ptr<PandaUI::Button> makeButton(std::string text, PandaUI::Color color) {
    auto button = std::make_shared<PandaUI::Button>(std::move(text));
    button->layout().setWidth(PandaUI::Length::percent(100.f));
    button->layout().setHeight(PandaUI::Length::points(46.f));
    return button;
}

std::shared_ptr<PandaUI::Panel> makeMenuCard() {
    auto card = std::make_shared<PandaUI::Panel>();
    card->setBackgroundColor(PandaUI::Color(0x101827E8));
    card->layout().setWidth(PandaUI::Length::percent(100.f));
    card->layout().setMaxWidth(PandaUI::Length::points(460.f));
    card->layout().setPadding(PandaUI::Edge::Horizontal, 20.f);
    card->layout().setPadding(PandaUI::Edge::Vertical, 18.f);
    card->layout().setGap(12.f);
    card->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    return card;
}

} // namespace ClawnDash
