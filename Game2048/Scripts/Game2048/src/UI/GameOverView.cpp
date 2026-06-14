#include "UI/GameOverView.hpp"

#include "UI/Game2048Style.hpp"

#include <utility>

namespace Game2048 {

GameOverView::GameOverView(Action onRestart) {
    setBackgroundColor(PandaUI::Color(0xEEE4DACC));
    layoutSetAbsolute();
    layoutSetInsetsFromParent({});
    layout().setFlexDirection(PandaUI::FlexDirection::Column);
    layout().setAlignItems(PandaUI::Align::Center);
    layout().setJustifyContent(PandaUI::Justify::Center);
    layout().setGap(18.f);

    auto title = makeLabel("GAME OVER", 44.f, PandaUI::Color(0x776E65FF));
    addSubview(title);

    auto hint = makeLabel("Press R or tap restart", 18.f, PandaUI::Color(0x776E65FF));
    addSubview(hint);

    auto restartButton = makeButton("Try again", PandaUI::Color(0x8F7A66FF));
    restartButton->layout().setWidth(PandaUI::Length::points(132.f));
    restartButton->layout().setHeight(PandaUI::Length::points(48.f));
    restartButton->setOnClick([onRestart = std::move(onRestart)](PandaUI::Button &) {
        if (onRestart) {
            onRestart();
        }
    });
    addSubview(restartButton);
}

} // namespace Game2048
