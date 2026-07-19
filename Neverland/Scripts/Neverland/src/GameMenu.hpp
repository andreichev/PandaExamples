#pragma once

// Глобальное состояние меню игры (как NeverlandTouchControls — статический стейт скриптов).
// Единственный владелец захвата курсора: в геймплее курсор захвачен (вращение камеры),
// в любом меню — свободен (клики по кнопкам).
enum class GameMenuState {
    MainMenu,    // не используется (старт сразу в игре); сентинел «не применено» в HUD
    Playing,     // геймплей: ввод игрока активен, курсор захвачен
    Paused,      // пауза (Esc): Resume / Quit, мир продолжает жить
    BlockPicker, // меню выбора блоков (M): курсор свободен, мир живёт
};

class GameMenu final {
public:
    static GameMenuState state();
    static void setState(GameMenuState state); // синкает захват курсора
    static void reset();                       // → MainMenu (старт мира)

    // Ввод игрока (движение/взгляд/копание) разрешён только в геймплее.
    static bool isGameplayActive();

private:
    static void syncCursor();

    static GameMenuState s_state;
};
