#pragma once

#include <PandaUI/PandaUI.hpp>

#include <functional>

namespace HelloUI {

class MainToolbarView final : public PandaUI::Panel {
public:
    using Action = std::function<void()>;

    MainToolbarView(Action onClick, Action onOpenWindow);
};

} // namespace HelloUI
