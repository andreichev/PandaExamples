#pragma once

#include <PandaUI/PandaUI.hpp>

#include <functional>
#include <string>

namespace ClawnDash {

class ResultOverlayView final : public PandaUI::Panel {
public:
    using Action = std::function<void()>;

    ResultOverlayView(
        std::string title,
        std::string message,
        PandaUI::Color messageColor,
        std::string primaryTitle,
        Action primaryAction,
        std::string secondaryTitle,
        Action secondaryAction
    );
};

} // namespace ClawnDash
