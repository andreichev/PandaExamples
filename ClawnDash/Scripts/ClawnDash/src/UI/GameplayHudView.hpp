#pragma once

#include <PandaUI/PandaUI.hpp>

#include <functional>
#include <memory>
#include <string>

namespace ClawnDash {

class GameplayHudView final : public PandaUI::Panel {
public:
    using Action = std::function<void()>;

    GameplayHudView(std::string levelTitle, Action openMenu);

    void setProgress(float progress);
    void setStatus(std::string status, PandaUI::Color color);

private:
    std::shared_ptr<PandaUI::Label> m_progressLabel;
    std::shared_ptr<PandaUI::Label> m_statusLabel;
};

} // namespace ClawnDash
