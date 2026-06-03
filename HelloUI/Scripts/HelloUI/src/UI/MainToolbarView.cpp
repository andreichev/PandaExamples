#include "UI/MainToolbarView.hpp"

#include "UI/HelloUIStyle.hpp"

#include <utility>

namespace HelloUI {

MainToolbarView::MainToolbarView(Action onClick, Action onOpenWindow) {
    setBackgroundColor(transparent());
    style().setHeight(PandaUI::Length::points(48.f));
    style().setFlexDirection(PandaUI::FlexDirection::Row);
    style().setGap(12.f);

    auto clickButton = makeButton("Click me", PandaUI::Color(0x2E7DFFFF));
    clickButton->setOnClick([onClick = std::move(onClick)](PandaUI::Button &) {
        if (onClick) { onClick(); }
    });
    addSubview(clickButton);

    auto windowButton = makeButton("Open window", PandaUI::Color(0x18A999FF));
    windowButton->setOnClick([onOpenWindow = std::move(onOpenWindow)](PandaUI::Button &) {
        if (onOpenWindow) { onOpenWindow(); }
    });
    addSubview(windowButton);
}

} // namespace HelloUI
