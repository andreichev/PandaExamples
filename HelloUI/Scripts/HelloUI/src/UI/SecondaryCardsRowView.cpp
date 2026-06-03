#include "UI/SecondaryCardsRowView.hpp"

#include "UI/DemoCardView.hpp"
#include "UI/HelloUIStyle.hpp"

namespace HelloUI {

SecondaryCardsRowView::SecondaryCardsRowView() {
    setBackgroundColor(transparent());
    style().setHeight(PandaUI::Length::points(120.f));
    style().setFlexDirection(PandaUI::FlexDirection::Row);
    style().setGap(12.f);

    addSubview(std::make_shared<DemoCardView>("Panel", "Stable View object"));
    addSubview(std::make_shared<DemoCardView>("Label", "Intrinsic text size"));
    addSubview(std::make_shared<DemoCardView>("Button", "Pointer events"));

    for (const auto &child : getSubviews()) {
        child->style().setFlexGrow(1.f);
    }
}

} // namespace HelloUI
