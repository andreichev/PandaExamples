#pragma once

#include <PandaUI/PandaUI.hpp>

#include <string>

namespace HelloUI {

class DemoCardView final : public PandaUI::Panel {
public:
    DemoCardView(std::string title, std::string text);
};

} // namespace HelloUI
