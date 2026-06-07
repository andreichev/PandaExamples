#pragma once

#include <PandaUI/PandaUI.hpp>

#include <functional>

namespace Game2048 {

class GameOverView final : public PandaUI::Panel {
public:
    using Action = std::function<void()>;

    explicit GameOverView(Action onRestart);
};

} // namespace Game2048
