#include "UI/MainMenuView.hpp"

#include "UI/DashUIStyle.hpp"

#include <utility>

namespace ClawnDash {

MainMenuView::MainMenuView(Action startLevel, Action selectLevel) {
    setBackgroundColor(PandaUI::Color(0x00000000));
    style().setWidth(PandaUI::Length::percent(100.f));
    style().setHeight(PandaUI::Length::percent(100.f));
    style().setPadding(PandaUI::Edge::All, 28.f);
    style().setFlexDirection(PandaUI::FlexDirection::Column);

    auto card = makeMenuCard();
    card->style().setMargin(PandaUI::Edge::Top, 42.f);
    card->addSubview(makeLabel("CLAWN DASH", 42.f, PandaUI::Color(0xFFFFFFFF), 1));
    card->addSubview(makeLabel(
        "Reaction, timing, and memory. No level locks, no hidden grind.",
        16.f,
        PandaUI::Color(0xBEE7FFFF)
    ));

    auto start = makeButton("Start Level 1", PandaUI::Color(0x5EEAD4FF));
    start->setTextColor(PandaUI::Color(0x09201DFF));
    start->setOnClick([action = std::move(startLevel)](PandaUI::Button &) {
        if (action) {
            action();
        }
    });
    card->addSubview(start);

    auto select = makeButton("Choose Level", PandaUI::Color(0xFFD166FF));
    select->setTextColor(PandaUI::Color(0x111827FF));
    select->setOnClick([action = std::move(selectLevel)](PandaUI::Button &) {
        if (action) {
            action();
        }
    });
    card->addSubview(select);
    addSubview(card);
}

} // namespace ClawnDash
