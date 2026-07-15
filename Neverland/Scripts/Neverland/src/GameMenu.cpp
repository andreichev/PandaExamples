#include "GameMenu.hpp"

#include <Bamboo/ApplicationAPI.hpp>

GameMenuState GameMenu::s_state = GameMenuState::MainMenu;

GameMenuState GameMenu::state() {
    return s_state;
}

void GameMenu::setState(GameMenuState state) {
    s_state = state;
    Bamboo::ApplicationAPI::setCursorLocked(s_state == GameMenuState::Playing);
}

void GameMenu::reset() {
    setState(GameMenuState::MainMenu);
}

bool GameMenu::isGameplayActive() {
    return s_state == GameMenuState::Playing;
}
