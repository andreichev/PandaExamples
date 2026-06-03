#include "UI/HelloUIStyle.hpp"

#include <utility>

namespace HelloUI {

std::shared_ptr<PandaUI::Label> makeLabel(std::string text, float fontSize, PandaUI::Color color) {
    auto label = std::make_shared<PandaUI::Label>(std::move(text));
    label->setFontSize(fontSize);
    label->setTextColor(color);
    label->style().setWidth(PandaUI::Length::percent(100.f));
    return label;
}

std::shared_ptr<PandaUI::Button> makeButton(std::string title, PandaUI::Color color) {
    auto button = std::make_shared<PandaUI::Button>(std::move(title));
    button->style().setHeight(PandaUI::Length::points(42.f));
    button->style().setWidth(PandaUI::Length::points(168.f));
    button->setNormalColor(color);
    button->setHoveredColor(PandaUI::Color(0x4A92FFFF));
    button->setPressedColor(PandaUI::Color(0x1C58BDFF));
    button->setTextColor(PandaUI::Color(0xFFFFFFFF));
    return button;
}

std::shared_ptr<PandaUI::Spacer> makeFixedSpacer(float height) {
    auto spacer = std::make_shared<PandaUI::Spacer>();
    spacer->style().setFlexGrow(0.f);
    spacer->style().setHeight(PandaUI::Length::points(height));
    return spacer;
}

} // namespace HelloUI
