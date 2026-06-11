#include "UI/LevelSelectView.hpp"

#include "UI/DashUIStyle.hpp"

#include <string>
#include <utility>

namespace ClawnDash {

LevelSelectView::LevelSelectView(
    const std::vector<LevelInfo> &levels, LevelAction playLevel, Action back
) {
    setBackgroundColor(PandaUI::Color(0x00000000));
    style().setWidth(PandaUI::Length::percent(100.f));
    style().setHeight(PandaUI::Length::percent(100.f));
    style().setPadding(PandaUI::Edge::All, 28.f);
    style().setFlexDirection(PandaUI::FlexDirection::Column);

    auto card = makeMenuCard();
    card->style().setMargin(PandaUI::Edge::Top, 18.f);
    card->addSubview(makeLabel("Select Level", 32.f, PandaUI::Color(0xFFFFFFFF), 1));
    card->addSubview(makeLabel(
        "Both levels are separate worlds. Open them in the editor and move the terrain directly.",
        15.f,
        PandaUI::Color(0xBEE7FFFF)
    ));

    for (std::size_t i = 0; i < levels.size(); ++i) {
        const LevelInfo &level = levels[i];
        auto button = makeButton(
            std::to_string(i + 1) + ". " + level.title + " - " + level.description,
            PandaUI::Color(level.accentColor)
        );
        button->setTextColor(PandaUI::Color(0x111827FF));
        button->setNumberOfLines(2);
        button->setOnClick([playLevel, i](PandaUI::Button &) {
            if (playLevel) {
                playLevel(i);
            }
        });
        card->addSubview(button);
    }

    auto backButton = makeButton("Back", PandaUI::Color(0x22324DFF));
    backButton->setOnClick([action = std::move(back)](PandaUI::Button &) {
        if (action) {
            action();
        }
    });
    card->addSubview(backButton);
    addSubview(card);
}

} // namespace ClawnDash
