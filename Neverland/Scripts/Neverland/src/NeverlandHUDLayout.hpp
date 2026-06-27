#pragma once

namespace NeverlandHUDLayout {

constexpr float CrosshairSize = 34.0f;
constexpr float CrosshairLineLength = 30.0f;
constexpr float CrosshairLineThickness = 2.0f;
constexpr float CrosshairLineInset = 2.0f;
constexpr float CrosshairLineCenter = 16.0f;

constexpr float HotbarSlotWidth = 42.0f;
constexpr float HotbarSlotHeight = 50.0f;
constexpr float HotbarSlotPadding = 4.0f;
constexpr float HotbarSlotGap = 3.0f;
constexpr float HotbarSwatchWidth = 28.0f;
constexpr float HotbarSwatchHeight = 22.0f;
constexpr float HotbarLabelFontSize = 11.0f;

constexpr float HotbarPaddingHorizontal = 8.0f;
constexpr float HotbarPaddingVertical = 7.0f;
constexpr float HotbarGap = 6.0f;
constexpr float HotbarBottomMargin = 24.0f;
constexpr int HotbarSlotCount = 10;

constexpr float HotbarWidth =
    HotbarPaddingHorizontal * 2.0f + HotbarSlotWidth * HotbarSlotCount + HotbarGap * (HotbarSlotCount - 1);
constexpr float HotbarHeight = HotbarPaddingVertical * 2.0f + HotbarSlotHeight;
constexpr float HotbarTouchHeight = HotbarBottomMargin + HotbarHeight + 20.0f;

constexpr float TouchButtonSize = 58.0f;
constexpr float TouchButtonGap = 8.0f;
constexpr float TouchControlsMargin = 24.0f;
constexpr float TouchControlsBottom = HotbarTouchHeight + 16.0f;
constexpr float MovePadSize = TouchButtonSize * 3.0f + TouchButtonGap * 2.0f;
constexpr float JumpButtonWidth = 88.0f;
constexpr float JumpButtonHeight = 58.0f;

inline bool isInsideRect(float x, float y, float left, float top, float width, float height) {
    return x >= left && x <= left + width && y >= top && y <= top + height;
}

inline bool isInsideHotbarTouchArea(float x, float y, float width, float height) {
    const float hotbarWidth = width < HotbarWidth ? width : HotbarWidth;
    const float halfWidth = hotbarWidth * 0.5f;
    const float left = width * 0.5f - halfWidth;
    const float top = height - HotbarTouchHeight;
    return isInsideRect(x, y, left, top, halfWidth * 2.0f, HotbarTouchHeight);
}

inline bool isInsideMovePadTouchArea(float x, float y, float, float height) {
    const float left = TouchControlsMargin;
    const float top = height - TouchControlsBottom - MovePadSize;
    return isInsideRect(x, y, left, top, MovePadSize, MovePadSize);
}

inline bool isInsideJumpButtonTouchArea(float x, float y, float width, float height) {
    const float left = width - TouchControlsMargin - JumpButtonWidth;
    const float top = height - TouchControlsBottom - JumpButtonHeight;
    return isInsideRect(x, y, left, top, JumpButtonWidth, JumpButtonHeight);
}

inline bool isInsideMobileControlsTouchArea(float x, float y, float width, float height) {
    return isInsideMovePadTouchArea(x, y, width, height) || isInsideJumpButtonTouchArea(x, y, width, height);
}

inline bool isInsideInteractiveHUDTouchArea(float x, float y, float width, float height) {
    return isInsideHotbarTouchArea(x, y, width, height) || isInsideMobileControlsTouchArea(x, y, width, height);
}

} // namespace NeverlandHUDLayout
