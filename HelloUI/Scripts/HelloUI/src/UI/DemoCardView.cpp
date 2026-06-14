#include "UI/DemoCardView.hpp"

#include "UI/HelloUIStyle.hpp"

#include <utility>

namespace HelloUI {

DemoCardView::DemoCardView(std::string title, std::string text) {
    setBackgroundColor(PandaUI::Color(0x202637FF));
    setClipsToBounds(true);
    layout().setWidth(PandaUI::Length::percent(100.f));
    layout().setFlexDirection(PandaUI::FlexDirection::Column);
    layout().setPadding(16.f);
    layout().setGap(6.f);
    addSubview(makeLabel(std::move(title), 18.f, PandaUI::Color(0xF7FAFFFF)));
    addSubview(makeLabel(std::move(text), 13.f, PandaUI::Color(0xAEB9CCFF)));
}

} // namespace HelloUI
