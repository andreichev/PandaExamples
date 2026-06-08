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
        {
            .title = "Speed Gate",
            .description = "A visible speed portal changes pace and color.",
            .accentColor = 0xFF8A5CFF,
        },
        {
            .title = "Mini Steps",
            .description = "Mini mode squeezes through lower gaps.",
            .accentColor = 0x6EE7F9FF,
        },
        {
            .title = "Gravity Drop",
            .description = "Gravity flips briefly, then returns to normal.",
            .accentColor = 0xF472B6FF,
        },
        {
            .title = "Portal Chain",
            .description = "Speed, mini, pad and orb in one readable run.",
            .accentColor = 0xA3E635FF,
        },
        {
            .title = "False Calm",
            .description = "Quiet gaps hide late orbs and double spikes.",
            .accentColor = 0xFACC15FF,
        },
        {
            .title = "Compression",
            .description = "Short recovery windows with no speed surprise.",
            .accentColor = 0xFB7185FF,
        },
        {
            .title = "Final Study",
            .description = "A longer memory line using all current tools.",
            .accentColor = 0xC084FCFF,
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
    std::array<Bamboo::WorldHandle, 10> worlds = {
        level1, level2, level3, level4, level5, level6, level7, level8, level9, level10
    };
    if (levelIndex >= worlds.size() || !worlds[levelIndex].isValid()) {
        LOG_ERROR("ClawnDashMenu: level world handle is not configured");
        return;
    }
    Bamboo::WorldAPI::load(worlds[levelIndex]);
}
