#include "UI/Game2048Style.hpp"

#include <utility>

namespace Game2048 {

PandaUI::Color transparent() {
    return PandaUI::Color(0x00000000);
}

PandaUI::Color tileColor(int value) {
    switch (value) {
        case 2:
            return PandaUI::Color(0xEEE4DAFF);
        case 4:
            return PandaUI::Color(0xEDE0C8FF);
        case 8:
            return PandaUI::Color(0xF2B179FF);
        case 16:
            return PandaUI::Color(0xF59563FF);
        case 32:
            return PandaUI::Color(0xF67C5FFF);
        case 64:
            return PandaUI::Color(0xF65E3BFF);
        case 128:
            return PandaUI::Color(0xEDCF72FF);
        case 256:
            return PandaUI::Color(0xEDCC61FF);
        case 512:
            return PandaUI::Color(0xEDC850FF);
        case 1024:
            return PandaUI::Color(0xEDC53FFF);
        case 2048:
            return PandaUI::Color(0xEDC22EFF);
        default:
            return value > 2048 ? PandaUI::Color(0x3C3A32FF) : PandaUI::Color(0xCDC1B4FF);
    }
}

PandaUI::Color tileTextColor(int value) {
    return value <= 4 ? PandaUI::Color(0x776E65FF) : PandaUI::Color(0xF9F6F2FF);
}

std::shared_ptr<PandaUI::Label> makeLabel(std::string text, float fontSize, PandaUI::Color color) {
    auto label = std::make_shared<PandaUI::Label>(std::move(text));
    label->setFontSize(fontSize);
    label->setTextColor(color);
    label->setNumberOfLines(1);
    return label;
}

std::shared_ptr<PandaUI::Button> makeButton(std::string title, PandaUI::Color color) {
    auto button = std::make_shared<PandaUI::Button>(std::move(title));
    button->setNormalColor(color);
    button->setHoveredColor(PandaUI::Color(0x9F8C7DFF));
    button->setPressedColor(PandaUI::Color(0x6F6259FF));
    button->setTextColor(PandaUI::Color(0xFFFFFFFF));
    button->setFontSize(16.f);
    return button;
}

} // namespace Game2048
