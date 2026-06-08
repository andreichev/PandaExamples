#pragma once

#include <PandaUI/PandaUI.hpp>

#include <memory>
#include <string>

namespace ClawnDash {

std::shared_ptr<PandaUI::Label>
makeLabel(std::string text, float size, PandaUI::Color color, int lines = 2);
std::shared_ptr<PandaUI::Button> makeButton(std::string text, PandaUI::Color color);
std::shared_ptr<PandaUI::Panel> makeMenuCard();

} // namespace ClawnDash
