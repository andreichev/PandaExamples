#pragma once

namespace NeverlandTouchControls {

enum class Button {
    Forward,
    Backward,
    Left,
    Right,
    Jump,
};

struct MoveAxes {
    float x = 0.0f;
    float y = 0.0f;
};

void setPressed(Button button, bool pressed);
MoveAxes getMoveAxes();
bool consumeJumpPressed();
bool isJumpDown();
void ignoreTouch(int id);
void updateIgnoredTouches(float width, float height);
bool isTouchIgnored(int id);
void reset();

} // namespace NeverlandTouchControls
