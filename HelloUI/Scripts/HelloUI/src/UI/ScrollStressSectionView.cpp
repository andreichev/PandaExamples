#include "UI/ScrollStressSectionView.hpp"

#include "UI/HelloUIStyle.hpp"

#include <array>
#include <memory>
#include <string>

namespace HelloUI {

namespace {

    constexpr std::array<uint32_t, 6> CARD_COLORS = {
        0x22304BFF,
        0x263A31FF,
        0x403049FF,
        0x4A3326FF,
        0x233D45FF,
        0x3F3A28FF,
    };

    std::shared_ptr<PandaUI::Panel> makeRow(int index) {
        auto row = std::make_shared<PandaUI::Panel>();
        row->setBackgroundColor(PandaUI::Color(CARD_COLORS[index % CARD_COLORS.size()]));
        row->setClipsToBounds(true);
        row->layout().setWidth(PandaUI::Length::percent(100.f));
        row->layout().setFlexDirection(PandaUI::FlexDirection::Column);
        row->layout().setPadding(14.f);
        row->layout().setGap(5.f);

        row->addSubview(
            makeLabel("Scroll item " + std::to_string(index + 1), 16.f, PandaUI::Color(0xFFFFFFFF))
        );
        row->addSubview(makeLabel(
            "Extra content for testing smooth vertical scrolling, layout calculation and "
            "overscroll bounce.",
            12.f,
            PandaUI::Color(0xC9D4E8FF)
        ));
        if (index % 3 == 0) {
            row->addSubview(makeLabel(
                "This row has one more line, so its height comes from text and padding instead of "
                "a fixed value.",
                12.f,
                PandaUI::Color(0xAEB9CCFF)
            ));
        }
        return row;
    }

} // namespace

ScrollStressSectionView::ScrollStressSectionView() {
    setBackgroundColor(PandaUI::Color(0x151B27FF));
    layout().setWidth(PandaUI::Length::percent(100.f));
    layout().setFlexDirection(PandaUI::FlexDirection::Column);
    layout().setPadding(16.f);
    layout().setGap(10.f);

    addSubview(makeLabel("Scroll stress test", 20.f, PandaUI::Color(0xF7FAFFFF)));
    addSubview(makeLabel(
        "Many retained views below are intentionally added to make iOS scroll smoothness visible.",
        13.f,
        PandaUI::Color(0xAEB9CCFF)
    ));

    for (int i = 0; i < 36; ++i) {
        addSubview(makeRow(i));
    }
}

} // namespace HelloUI
