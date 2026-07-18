#include "GameMenu.hpp"

#include <Bamboo/ApplicationAPI.hpp>

GameMenuState GameMenu::s_state = GameMenuState::MainMenu;
bool GameMenu::s_uiModifierHeld = false;

GameMenuState GameMenu::state() {
    return s_state;
}

void GameMenu::setState(GameMenuState state) {
    s_state = state;
    syncCursor();
}

void GameMenu::reset() {
    s_uiModifierHeld = false;
    setState(GameMenuState::MainMenu);
}

bool GameMenu::isGameplayActive() {
    return s_state == GameMenuState::Playing;
}

void GameMenu::setUIModifierHeld(bool held) {
    if (s_uiModifierHeld == held) { return; }
    s_uiModifierHeld = held;
    syncCursor();
}

bool GameMenu::isUIModifierHeld() {
    return s_uiModifierHeld;
}

void GameMenu::syncCursor() {
    Bamboo::ApplicationAPI::setCursorLocked(s_state == GameMenuState::Playing && !s_uiModifierHeld);
}
