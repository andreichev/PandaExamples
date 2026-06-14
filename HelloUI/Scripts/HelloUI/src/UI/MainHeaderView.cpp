#include "UI/MainHeaderView.hpp"

#include "UI/HelloUIStyle.hpp"

namespace HelloUI {

MainHeaderView::MainHeaderView() {
    setBackgroundColor(PandaUI::Color(0x151B27FF));
    layout().setFlexDirection(PandaUI::FlexDirection::Column);
    layout().setPadding(18.f);
    layout().setGap(8.f);
    addSubview(makeLabel("HelloUI", 28.f, PandaUI::Color(0xFFFFFFFF)));
    addSubview(makeLabel(
        "Test project for retained-mode PandaUI from game scripting.",
        15.f,
        PandaUI::Color(0x95A3BAFF)
    ));
}

} // namespace HelloUI
