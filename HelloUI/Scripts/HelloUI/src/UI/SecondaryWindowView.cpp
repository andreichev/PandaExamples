#include "UI/SecondaryWindowView.hpp"

#include "UI/HelloUIStyle.hpp"
#include "UI/SecondaryCardsRowView.hpp"

#include <utility>

namespace HelloUI {

SecondaryWindowView::SecondaryWindowView(Action onClose) {
    setBackgroundColor(PandaUI::Color(0x111318FF));
    layout().setFlexDirection(PandaUI::FlexDirection::Column);

    auto safeArea = std::make_shared<PandaUI::SafeAreaView>();
    safeArea->setBackgroundColor(transparent());
    safeArea->layout().setFlexDirection(PandaUI::FlexDirection::Column);

    auto content = std::make_shared<PandaUI::Panel>();
    content->setBackgroundColor(transparent());
    content->layout().setWidth(PandaUI::Length::percent(100.f));
    content->layout().setFlexGrow(1.f);
    content->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    content->layout().setPadding(22.f);
    content->layout().setGap(14.f);

    content->addSubview(makeLabel("Secondary PandaUI Window", 24.f, PandaUI::Color(0xFFFFFFFF)));
    content->addSubview(makeLabel(
        "This window is created from a script and rendered by Panda runtime.",
        14.f,
        PandaUI::Color(0xAEB9CCFF)
    ));
    content->addSubview(std::make_shared<SecondaryCardsRowView>());

    auto closeButton = makeButton("Close", PandaUI::Color(0xE85D75FF));
    closeButton->setOnClick([onClose = std::move(onClose)](PandaUI::Button &) {
        if (onClose) {
            onClose();
        }
    });
    content->addSubview(closeButton);

    safeArea->addSubview(content);
    addSubview(safeArea);
}

} // namespace HelloUI
