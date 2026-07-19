#include "GameMenu.hpp"

#include <Bamboo/ApplicationAPI.hpp>

GameMenuState GameMenu::s_state = GameMenuState::MainMenu;

GameMenuState GameMenu::state() {
    return s_state;
}

void GameMenu::setState(GameMenuState state) {
    s_state = state;
    syncCursor();
}

void GameMenu::reset() {
    setState(GameMenuState::Playing); // старт мира — сразу игра (без главного меню)
}

bool GameMenu::isGameplayActive() {
    return s_state == GameMenuState::Playing;
}

void GameMenu::syncCursor() {
    Bamboo::ApplicationAPI::setCursorLocked(s_state == GameMenuState::Playing);
}
