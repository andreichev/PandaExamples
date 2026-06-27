#include "NeverlandTouchControls.hpp"
#include "NeverlandHUDLayout.hpp"

#include <Bamboo/Input.hpp>

#include <algorithm>
#include <vector>

namespace NeverlandTouchControls {
namespace {

struct State {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool jumpDown = false;
    bool jumpPressed = false;
};

State s_state;
std::vector<int> s_ignoredTouchIds;

bool &buttonState(Button button) {
    switch (button) {
        case Button::Forward: return s_state.forward;
        case Button::Backward: return s_state.backward;
        case Button::Left: return s_state.left;
        case Button::Right: return s_state.right;
        case Button::Jump: return s_state.jumpDown;
    }
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

MoveAxes getMoveAxes() {
    MoveAxes axes;
    axes.x = (s_state.right ? 1.0f : 0.0f) - (s_state.left ? 1.0f : 0.0f);
    axes.y = (s_state.forward ? 1.0f : 0.0f) - (s_state.backward ? 1.0f : 0.0f);
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

void updateIgnoredTouches(float width, float height) {
    std::erase_if(s_ignoredTouchIds, [](int id) {
        return !findTouchById(id);
    });

    for (int index = 0; index < Bamboo::Input::touchCount(); index++) {
        Bamboo::Input::Touch touch;
        if (!Bamboo::Input::tryGetTouch(index, touch)) { continue; }
        if (!NeverlandHUDLayout::isInsideInteractiveHUDTouchArea(touch.x, touch.y, width, height)) { continue; }
        ignoreTouch(touch.id);
    }
}

bool isTouchIgnored(int id) {
    return containsIgnoredTouch(id);
}

void reset() {
    s_state = {};
    s_ignoredTouchIds.clear();
}

} // namespace NeverlandTouchControls
