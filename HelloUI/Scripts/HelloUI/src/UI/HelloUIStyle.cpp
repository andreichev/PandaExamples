#include "UI/HelloUIStyle.hpp"

#include <utility>

namespace HelloUI {

std::shared_ptr<PandaUI::Label> makeLabel(std::string text, float fontSize, PandaUI::Color color) {
    auto label = std::make_shared<PandaUI::Label>(std::move(text));
    label->setFontSize(fontSize);
    label->setTextColor(color);
    label->layout().setWidth(PandaUI::Length::percent(100.f));
    return label;
}

std::shared_ptr<PandaUI::Button> makeButton(std::string title, PandaUI::Color color) {
    auto button = std::make_shared<PandaUI::Button>(std::move(title));
    button->layout().setWidth(PandaUI::Length::points(168.f));
    return button;
}

} // namespace HelloUI
