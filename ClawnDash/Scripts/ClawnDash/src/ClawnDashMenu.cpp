#include "ClawnDashMenu.hpp"

#include "UI/LevelSelectView.hpp"
#include "UI/MainMenuView.hpp"

#include <Bamboo/Logger.hpp>
#include <Bamboo/WorldAPI.hpp>

#include <array>
#include <memory>

void ClawnDashMenu::start() {
    m_window = PandaUI::Window::main();
    if (!m_window.isValid()) {
        LOG_ERROR("ClawnDashMenu: main PandaUI window is unavailable");
        return;
    }

    m_levels = {
        {
            .title = "Foundation",
            .description = "Editable terrain, steps, pits, and first walls.",
            .backgroundColor = 0x090E20FF,
            .groundColor = 0x202B3EFF,
            .groundGlowColor = 0x5EEAD4AA,
            .accentColor = 0xFFD166FF,
        },
        {
            .title = "Pressure",
            .description = "A second world kept for chaining and harder tests.",
            .backgroundColor = 0x120D27FF,
            .groundColor = 0x2B2147FF,
            .groundGlowColor = 0xB56CFFFF,
            .accentColor = 0xB56CFFFF,
        },
    };
    showMainMenu();
}

void ClawnDashMenu::shutdown() {
    if (m_window.isValid()) {
        m_window.setRootView(nullptr);
    }
}

void ClawnDashMenu::showMainMenu() {
    if (!m_window.isValid()) {
        return;
    }

    m_window.setRootView(std::make_shared<ClawnDash::MainMenuView>(
        [this]() { loadLevel(0); }, [this]() { showLevelSelect(); }
    ));
}

void ClawnDashMenu::showLevelSelect() {
    if (!m_window.isValid()) {
        return;
    }

    m_window.setRootView(std::make_shared<ClawnDash::LevelSelectView>(
        m_levels,
        [this](std::size_t levelIndex) { loadLevel(levelIndex); },
        [this]() { showMainMenu(); }
    ));
}

void ClawnDashMenu::loadLevel(std::size_t levelIndex) {
    std::array<Bamboo::WorldHandle, 2> worlds = {level1, level2};
    if (levelIndex >= worlds.size() || !worlds[levelIndex].isValid()) {
        LOG_ERROR("ClawnDashMenu: level world handle is not configured");
        return;
    }
    Bamboo::WorldAPI::load(worlds[levelIndex]);
}
