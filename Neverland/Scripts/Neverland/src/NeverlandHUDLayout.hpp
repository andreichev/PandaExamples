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
constexpr float HotbarCornerRadius = 15.0f;

constexpr float HotbarPaddingHorizontal = 8.0f;
constexpr float HotbarPaddingVertical = 7.0f;
constexpr float HotbarGap = 6.0f;
constexpr float HotbarBottomMargin = 0.0f;
constexpr int HotbarSlotCount = 7;

// +1 — кнопка Menu в правом краю хотбара.
constexpr float HotbarWidth = HotbarPaddingHorizontal * 2.0f +
                              HotbarSlotWidth * (HotbarSlotCount + 1) + HotbarGap * HotbarSlotCount;
constexpr float HotbarHeight = HotbarPaddingVertical * 2.0f + HotbarSlotHeight;
constexpr float HotbarTouchHeight = HotbarBottomMargin + HotbarHeight + 20.0f;

constexpr float TouchButtonSize = 58.0f;

// Карточки меню выбора блоков (Tab).
constexpr float MenuCardWidth = 84.0f;
constexpr float MenuCardHeight = 92.0f;
constexpr float MenuCardPreview = 44.0f;

constexpr float MenuButtonWidth = 240.0f;
constexpr float MenuButtonHeight = 52.0f;
constexpr float MenuButtonGap = 14.0f;
constexpr float MenuTitleFontSize = 52.0f;
constexpr float MenuSubtitleFontSize = 20.0f;
constexpr float MenuTitleGap = 42.0f;
constexpr float ActionControlsMargin = 24.0f;
constexpr float ActionControlsBottom = HotbarTouchHeight + 16.0f;
constexpr float JumpButtonWidth = 88.0f;
constexpr float JumpButtonHeight = 58.0f;

// Плавающий джойстик движения (появляется в точке касания левой половины экрана).
constexpr float JoystickRadius = 64.0f;
constexpr float JoystickKnobRadius = 26.0f;

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

inline bool isInsideJumpButtonTouchArea(float x, float y, float width, float height) {
    // Колонка кнопок действий: BLOCKS над JUMP (высота панели — две кнопки + зазор).
    const float columnHeight = JumpButtonHeight * 2.0f + 10.0f;
    const float left = width - ActionControlsMargin - JumpButtonWidth;
    const float top = height - ActionControlsBottom - columnHeight;
    return isInsideRect(x, y, left, top, JumpButtonWidth, columnHeight);
}

inline bool isInsideMobileControlsTouchArea(float x, float y, float width, float height) {
    return isInsideJumpButtonTouchArea(x, y, width, height);
}

inline bool isInsideInteractiveHUDTouchArea(float x, float y, float width, float height) {
    return isInsideHotbarTouchArea(x, y, width, height) || isInsideMobileControlsTouchArea(x, y, width, height);
}

inline bool isInsideInteractiveHUDTouchArea(
    float x,
    float y,
    float width,
    float height,
    float safeTop,
    float safeLeft,
    float safeRight,
    float safeBottom
) {
    const float contentWidth = width - safeLeft - safeRight;
    const float contentHeight = height - safeTop - safeBottom;
    if (contentWidth <= 0.0f || contentHeight <= 0.0f) { return false; }
    return isInsideInteractiveHUDTouchArea(x - safeLeft, y - safeTop, contentWidth, contentHeight);
}

} // namespace NeverlandHUDLayout
