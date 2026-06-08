#include "UI/ResultOverlayView.hpp"

#include "UI/DashUIStyle.hpp"

#include <utility>

namespace ClawnDash {

ResultOverlayView::ResultOverlayView(
    std::string title,
    std::string message,
    PandaUI::Color messageColor,
    std::string primaryTitle,
    Action primaryAction,
    std::string secondaryTitle,
    Action secondaryAction
) {
    setBackgroundColor(PandaUI::Color(0x00000000));
    style().setWidth(PandaUI::Length::percent(100.f));
    style().setHeight(PandaUI::Length::percent(100.f));
    style().setPadding(PandaUI::Edge::All, 28.f);
    style().setFlexDirection(PandaUI::FlexDirection::Column);

    auto card = makeMenuCard();
    card->style().setMargin(PandaUI::Edge::Top, 48.f);
    card->addSubview(makeLabel(std::move(title), 28.f, PandaUI::Color(0xFFFFFFFF), 1));
    card->addSubview(makeLabel(std::move(message), 18.f, messageColor, 2));

    auto primary = makeButton(std::move(primaryTitle), messageColor);
    primary->setTextColor(PandaUI::Color(0x0C2115FF));
    primary->setOnClick([action = std::move(primaryAction)](PandaUI::Button &) {
        if (action) {
            action();
        }
    });
    card->addSubview(primary);

    auto secondary = makeButton(std::move(secondaryTitle), PandaUI::Color(0x22324DFF));
    secondary->setOnClick([action = std::move(secondaryAction)](PandaUI::Button &) {
        if (action) {
            action();
        }
    });
    card->addSubview(secondary);
    addSubview(card);
}

} // namespace ClawnDash
