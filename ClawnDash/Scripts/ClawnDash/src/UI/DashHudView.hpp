#pragma once

#include <PandaUI/PandaUI.hpp>

#include <memory>
#include <string>

namespace ClawnDash {

class DashHudView final : public PandaUI::Panel {
public:
    DashHudView();

    void setProgress(float progress);
    void setStatus(std::string status, PandaUI::Color color);

private:
    std::shared_ptr<PandaUI::Label> m_progressLabel;
    std::shared_ptr<PandaUI::Label> m_statusLabel;
};

} // namespace ClawnDash
