#pragma once

#include "Model/DashTypes.hpp"

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <PandaUI/PandaUI.hpp>

#include <memory>
#include <vector>

class ClawnDashMenu final : public Bamboo::Script {
public:
    void start() override;
    void shutdown() override;

    Bamboo::WorldHandle level1;
    Bamboo::WorldHandle level2;
    Bamboo::WorldHandle level3;
    Bamboo::WorldHandle level4;
    Bamboo::WorldHandle level5;
    Bamboo::WorldHandle level6;
    Bamboo::WorldHandle level7;
    Bamboo::WorldHandle level8;
    Bamboo::WorldHandle level9;
    Bamboo::WorldHandle level10;

    PANDA_FIELDS_BEGIN(ClawnDashMenu)
    PANDA_FIELD(level1)
    PANDA_FIELD(level2)
    PANDA_FIELD(level3)
    PANDA_FIELD(level4)
    PANDA_FIELD(level5)
    PANDA_FIELD(level6)
    PANDA_FIELD(level7)
    PANDA_FIELD(level8)
    PANDA_FIELD(level9)
    PANDA_FIELD(level10)
    PANDA_FIELDS_END

private:
    PandaUI::Window m_window;
    std::vector<ClawnDash::LevelInfo> m_levels;

    void showMainMenu();
    void showLevelSelect();
    void loadLevel(std::size_t levelIndex);
};

REGISTER_SCRIPT(ClawnDashMenu)
