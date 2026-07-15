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

private:
    static GameMenuState s_state;
};
