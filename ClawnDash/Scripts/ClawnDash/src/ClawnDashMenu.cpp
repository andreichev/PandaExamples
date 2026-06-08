#include "ClawnDashMenu.hpp"

#include "UI/LevelSelectView.hpp"
#include "UI/MainMenuView.hpp"

#include <Bamboo/Logger.hpp>
#include <Bamboo/WorldAPI.hpp>

#include <array>
#include <cstddef>
#include <memory>

void ClawnDashMenu::start() {
    m_window = PandaUI::Window::main();
    if (!m_window.isValid()) {
        LOG_ERROR("ClawnDashMenu: main PandaUI window is unavailable");
        return;
    }

    m_levels = {
        {
            .title = "First Contact",
            .description = "Single jumps and a first automatic pad.",
            .backgroundColor = 0x090E20FF,
            .groundColor = 0x202B3EFF,
            .groundGlowColor = 0x5EEAD4AA,
            .accentColor = 0xFFD166FF,
        },
        {
            .title = "Orb Switch",
            .description = "Reaction plus air taps on blue orbs.",
            .backgroundColor = 0x120D27FF,
            .groundColor = 0x2B2147FF,
            .groundGlowColor = 0xB56CFFFF,
            .accentColor = 0xB56CFFFF,
        },
        {
            .title = "Memory Line",
            .description = "Pads, orbs, and tighter readable patterns.",
            .backgroundColor = 0x071B22FF,
            .groundColor = 0x153B42FF,
            .groundGlowColor = 0x7CFFB2AA,
            .accentColor = 0x7CFFB2FF,
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
    std::array<Bamboo::WorldHandle, 3> worlds = {level1, level2, level3};
    if (levelIndex >= worlds.size() || !worlds[levelIndex].isValid()) {
        LOG_ERROR("ClawnDashMenu: level world handle is not configured");
        return;
    }
    Bamboo::WorldAPI::load(worlds[levelIndex]);
}
