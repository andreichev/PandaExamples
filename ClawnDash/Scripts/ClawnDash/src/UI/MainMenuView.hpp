#pragma once

#include <PandaUI/PandaUI.hpp>

#include <functional>

namespace ClawnDash {

class MainMenuView final : public PandaUI::Panel {
public:
    using Action = std::function<void()>;

    MainMenuView(Action startLevel, Action selectLevel);
};

} // namespace ClawnDash
