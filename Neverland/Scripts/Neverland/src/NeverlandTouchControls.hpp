#pragma once

namespace NeverlandTouchControls {

enum class Button {
    Jump,
};

struct MoveAxes {
    float x = 0.0f;
    float y = 0.0f;
};

// Плавающий джойстик движения: появляется в точке первого касания левой половины экрана.
// Стейт пишет PlayerController (владелец трекинга касаний), HUD читает для отрисовки.
struct MoveStick {
    bool active = false;
    float originX = 0.0f;
    float originY = 0.0f;
    float currentX = 0.0f;
    float currentY = 0.0f;
    float axisX = 0.0f; // нормированные оси (-1..1): x — вправо, y — вперёд
    float axisY = 0.0f;
};

void setPressed(Button button, bool pressed);
void setMoveStick(const MoveStick &stick);
const MoveStick &getMoveStick();
MoveAxes getMoveAxes(); // оси стика (для анимаций/логики вне контроллера)
bool consumeJumpPressed();
bool isJumpDown();
void ignoreTouch(int id);
void setSafeAreaInsets(float top, float left, float right, float bottom);
void updateIgnoredTouches(float width, float height);
bool isTouchIgnored(int id);
void reset();

} // namespace NeverlandTouchControls
