#pragma once

#include <PandaUI/PandaUI.hpp>

#include <memory>
#include <string>

namespace HelloUI {

inline PandaUI::Color transparent() {
    return PandaUI::Color(0x00000000);
}

std::shared_ptr<PandaUI::Label> makeLabel(std::string text, float fontSize, PandaUI::Color color);
std::shared_ptr<PandaUI::Button> makeButton(std::string title, PandaUI::Color color);
std::shared_ptr<PandaUI::Spacer> makeFixedSpacer(float height);

} // namespace HelloUI
