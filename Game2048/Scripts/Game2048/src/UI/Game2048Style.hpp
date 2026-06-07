#pragma once

#include <PandaUI/PandaUI.hpp>

#include <memory>
#include <string>

namespace Game2048 {

PandaUI::Color transparent();
PandaUI::Color tileColor(int value);
PandaUI::Color tileTextColor(int value);
std::shared_ptr<PandaUI::Label> makeLabel(std::string text, float fontSize, PandaUI::Color color);
std::shared_ptr<PandaUI::Button> makeButton(std::string title, PandaUI::Color color);

} // namespace Game2048
