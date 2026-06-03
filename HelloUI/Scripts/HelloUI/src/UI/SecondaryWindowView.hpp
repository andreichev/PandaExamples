#pragma once

#include <PandaUI/PandaUI.hpp>

#include <functional>

namespace HelloUI {

class SecondaryWindowView final : public PandaUI::Panel {
public:
    using Action = std::function<void()>;

    explicit SecondaryWindowView(Action onClose);
};

} // namespace HelloUI
