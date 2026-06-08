#pragma once

#include "Model/DashTypes.hpp"

#include <PandaUI/PandaUI.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ClawnDash {

class DashHudView final : public PandaUI::Panel {
public:
    using LevelAction = std::function<void(std::size_t)>;
    using Action = std::function<void()>;

    DashHudView();

    void setActions(
        LevelAction playLevel,
        Action showMainMenu,
        Action showLevelSelect,
        Action restartLevel,
        Action nextLevel
    );
    void showMainMenu(const std::vector<LevelInfo> &levels);
    void showLevelSelect(const std::vector<LevelInfo> &levels);
    void showGameplay(std::string levelTitle);
    void showCrashed(std::string levelTitle);
    void showFinished(std::string levelTitle, bool hasNextLevel);
    void setProgress(float progress);
    void setStatus(std::string status, PandaUI::Color color);

private:
    std::shared_ptr<PandaUI::Label> makeLabel(std::string text, float size, PandaUI::Color color);
    std::shared_ptr<PandaUI::Button> makeButton(std::string text, PandaUI::Color color);
    std::shared_ptr<PandaUI::Panel> makeCard();

    std::shared_ptr<PandaUI::Label> m_progressLabel;
    std::shared_ptr<PandaUI::Label> m_statusLabel;
    LevelAction m_playLevel;
    Action m_showMainMenu;
    Action m_showLevelSelect;
    Action m_restartLevel;
    Action m_nextLevel;
};

} // namespace ClawnDash
