#include "NeverlandTouchControls.hpp"
#include "NeverlandHUDLayout.hpp"

#include <Bamboo/Input.hpp>

#include <algorithm>
#include <vector>

namespace NeverlandTouchControls {
namespace {

struct State {
    bool jumpDown = false;
    bool jumpPressed = false;
    MoveStick moveStick;
};

State s_state;
std::vector<int> s_ignoredTouchIds;
float s_safeTop = 0.0f;
float s_safeLeft = 0.0f;
float s_safeRight = 0.0f;
float s_safeBottom = 0.0f;

bool &buttonState(Button) {
    return s_state.jumpDown;
}

bool findTouchById(int id) {
    for (int index = 0; index < Bamboo::Input::touchCount(); index++) {
        Bamboo::Input::Touch touch;
        if (!Bamboo::Input::tryGetTouch(index, touch)) { continue; }
        if (touch.id == id) { return true; }
    }
    return false;
}

bool containsIgnoredTouch(int id) {
    return std::find(s_ignoredTouchIds.begin(), s_ignoredTouchIds.end(), id) != s_ignoredTouchIds.end();
}

} // namespace

void setPressed(Button button, bool pressed) {
    bool &current = buttonState(button);
    if (current == pressed) { return; }
    if (button == Button::Jump && pressed) { s_state.jumpPressed = true; }
    current = pressed;
}

void setMoveStick(const MoveStick &stick) {
    s_state.moveStick = stick;
}

const MoveStick &getMoveStick() {
    return s_state.moveStick;
}

MoveAxes getMoveAxes() {
    MoveAxes axes;
    axes.x = s_state.moveStick.axisX;
    axes.y = s_state.moveStick.axisY;
    return axes;
}

bool consumeJumpPressed() {
    const bool pressed = s_state.jumpPressed;
    s_state.jumpPressed = false;
    return pressed;
}

bool isJumpDown() {
    return s_state.jumpDown;
}

void ignoreTouch(int id) {
    if (containsIgnoredTouch(id)) { return; }
    s_ignoredTouchIds.push_back(id);
}

void setSafeAreaInsets(float top, float left, float right, float bottom) {
    s_safeTop = top;
    s_safeLeft = left;
    s_safeRight = right;
    s_safeBottom = bottom;
}

void updateIgnoredTouches(float width, float height) {
    std::erase_if(s_ignoredTouchIds, [](int id) {
        return !findTouchById(id);
    });

    for (int index = 0; index < Bamboo::Input::touchCount(); index++) {
        Bamboo::Input::Touch touch;
        if (!Bamboo::Input::tryGetTouch(index, touch)) { continue; }
        if (!NeverlandHUDLayout::isInsideInteractiveHUDTouchArea(
                touch.x,
                touch.y,
                width,
                height,
                s_safeTop,
                s_safeLeft,
                s_safeRight,
                s_safeBottom
            )) {
            continue;
        }
        ignoreTouch(touch.id);
    }
}

bool isTouchIgnored(int id) {
    return containsIgnoredTouch(id);
}

void reset() {
    s_state = {};
    s_ignoredTouchIds.clear();
    setSafeAreaInsets(0.0f, 0.0f, 0.0f, 0.0f);
}

} // namespace NeverlandTouchControls
