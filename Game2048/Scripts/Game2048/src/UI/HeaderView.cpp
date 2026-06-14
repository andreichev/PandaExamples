#include "UI/HeaderView.hpp"

#include "UI/Game2048Style.hpp"

#include <string>
#include <utility>

namespace Game2048 {

HeaderView::HeaderView(Action onRestart) {
    setBackgroundColor(transparent());
    layout().setFlexDirection(PandaUI::FlexDirection::Row);
    layout().setAlignItems(PandaUI::Align::Center);
    layout().setJustifyContent(PandaUI::Justify::SpaceBetween);
    layout().setGap(16.f);

    auto title = makeLabel("2048", 54.f, PandaUI::Color(0xF9F6F2FF));
    title->layout().setFlexGrow(1.f);
    addSubview(title);

    auto scoreCard = std::make_shared<PandaUI::Panel>();
    scoreCard->setBackgroundColor(PandaUI::Color(0xBBADA0FF));
    scoreCard->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    scoreCard->layout().setAlignItems(PandaUI::Align::Center);
    scoreCard->layout().setJustifyContent(PandaUI::Justify::Center);
    scoreCard->layout().setPadding(PandaUI::Edge::Horizontal, 18.f);
    scoreCard->layout().setPadding(PandaUI::Edge::Vertical, 8.f);

    auto scoreTitle = makeLabel("SCORE", 12.f, PandaUI::Color(0xEEE4DAFF));
    m_scoreLabel = makeLabel("0", 22.f, PandaUI::Color(0xFFFFFFFF));
    scoreCard->addSubview(scoreTitle);
    scoreCard->addSubview(m_scoreLabel);
    addSubview(scoreCard);

    auto restartButton = makeButton("Restart", PandaUI::Color(0x8F7A66FF));
    restartButton->layout().setWidth(PandaUI::Length::points(112.f));
    restartButton->layout().setHeight(PandaUI::Length::points(44.f));
    restartButton->setOnClick([onRestart = std::move(onRestart)](PandaUI::Button &) {
        if (onRestart) {
            onRestart();
        }
    });
    addSubview(restartButton);
}

void HeaderView::setScore(int score) {
    if (m_scoreLabel) {
        m_scoreLabel->setText(std::to_string(score));
    }
}

void HeaderView::setContentWidth(float width) {
    layout().setWidth(PandaUI::Length::points(width));
}

} // namespace Game2048
