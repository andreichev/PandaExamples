#pragma once

// Глобальное состояние меню игры (как NeverlandTouchControls — статический стейт скриптов).
// Единственный владелец захвата курсора: в геймплее курсор захвачен (вращение камеры),
// в любом меню — свободен (клики по кнопкам).
enum class GameMenuState {
    MainMenu, // стартовый экран: Play / Quit
    Playing,  // геймплей: ввод игрока активен, курсор захвачен
    Paused,   // пауза (Esc): Resume / Quit, мир продолжает жить
};

class GameMenu final {
public:
    static GameMenuState state();
    static void setState(GameMenuState state); // синкает захват курсора
    static void reset();                       // → MainMenu (старт мира)

    // Ввод игрока (движение/взгляд/копание) разрешён только в геймплее.
    static bool isGameplayActive();

    // Зажатый UI-модификатор (Alt, desktop): в геймплее временно освобождает курсор
    // для кликов по HUD (панель кисти); взгляд и правки блоков на это время замирают.
    static void setUIModifierHeld(bool held);
    static bool isUIModifierHeld();

private:
    static void syncCursor();

    static GameMenuState s_state;
    static bool s_uiModifierHeld;
};
