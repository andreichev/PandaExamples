//
// Created by Admin on 12.02.2022.
//

#include "PlayerController.hpp"
#include "GameMenu.hpp"
#include "Model/ChunksStorage.hpp"
#include "NeverlandTouchControls.hpp"

#include <Bamboo/Math.hpp>
#include <Bamboo/Input.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/ApplicationAPI.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace {

constexpr float CAMERA_VERTICAL_FOV_DEGREES = 70.0f;
constexpr float LEGACY_SPRINT_MULTIPLIER = 1.6f;
constexpr float UPDATED_SPRINT_MULTIPLIER = 2.1f;

Quat toBambooQuat(const glm::quat &quat) {
    return Quat(quat.x, quat.y, quat.z, quat.w);
}

bool findTouchById(int id, Input::Touch &outTouch) {
    for (int index = 0; index < Input::touchCount(); index++) {
        Input::Touch touch;
        if (!Input::tryGetTouch(index, touch)) { continue; }
        if (touch.id != id) { continue; }
        outTouch = touch;
        return true;
    }
    return false;
}

} // namespace

void PlayerController::start() {
    NeverlandTouchControls::reset();
    if (std::abs(sprintMultiplier - LEGACY_SPRINT_MULTIPLIER) < 0.01f) {
        sprintMultiplier = UPDATED_SPRINT_MULTIPLIER;
    }
    const int groundHeight = ChunksStorage::terrainHeight(0, 0);
    m_physicsEyePosition = glm::vec3(0.0f, groundHeight + 1.0f + eyeHeight + 1.0f, 0.0f);
    m_visualYOffset = 0.0f;
    m_hasPhysicsEyePosition = true;
    setEyePosition(m_physicsEyePosition);
    TransformComponentAPI::setRotation(
        getEntity(), toBambooQuat(glm::quat(glm::vec3(glm::radians(25.f), 0.f, 0.f)))
    );
    syncAnglesFromRotation(TransformComponentAPI::getRotation(getEntity()));
}

void PlayerController::update(float deltaTime) {
    if (!GameMenu::isGameplayActive()) { // меню: игрок замирает, тач-трекеры сбрасываются
        NeverlandTouchControls::reset();
        m_moveTouch = {};
        m_lookTouch = {};
        return;
    }
    updateTouchControls();
    updateLook();
    updateCharacter(deltaTime);
}

void PlayerController::shutdown() {
    NeverlandTouchControls::reset();
    ApplicationAPI::setCursorLocked(false);
}

void PlayerController::updateTouchControls() {
    m_touchMoveDirection = glm::vec3(0.0f);
    m_touchLookDeltaX = 0.0f;
    m_touchLookDeltaY = 0.0f;
    m_touchJumpPressed = false;

    if (enableTouchControls == 0) {
        m_moveTouch.active = false;
        m_lookTouch.active = false;
        NeverlandTouchControls::reset();
        return;
    }

    const float width = static_cast<float>(std::max(ApplicationAPI::getWidth(), 1u));
    const float height = static_cast<float>(std::max(ApplicationAPI::getHeight(), 1u));
    NeverlandTouchControls::updateIgnoredTouches(width, height);

    auto updateExistingTouch = [](TouchTracker &tracker, float &deltaX, float &deltaY) {
        deltaX = 0.0f;
        deltaY = 0.0f;
        if (!tracker.active) { return false; }

        Input::Touch touch;
        if (!findTouchById(tracker.id, touch)) {
            tracker.active = false;
            tracker.id = -1;
            return false;
        }

        deltaX = touch.x - tracker.lastX;
        deltaY = touch.y - tracker.lastY;
        tracker.lastX = touch.x;
        tracker.lastY = touch.y;
        return true;
    };

    float moveDeltaX = 0.0f;
    float moveDeltaY = 0.0f;
    (void)updateExistingTouch(m_moveTouch, moveDeltaX, moveDeltaY);

    float lookDeltaX = 0.0f;
    float lookDeltaY = 0.0f;
    if (updateExistingTouch(m_lookTouch, lookDeltaX, lookDeltaY)) {
        m_touchLookDeltaX = lookDeltaX;
        m_touchLookDeltaY = lookDeltaY;
    }

    for (int index = 0; index < Input::touchCount(); index++) {
        Input::Touch touch;
        if (!Input::tryGetTouch(index, touch)) { continue; }
        if (NeverlandTouchControls::isTouchIgnored(touch.id)) { continue; }
        if ((m_moveTouch.active && touch.id == m_moveTouch.id) ||
            (m_lookTouch.active && touch.id == m_lookTouch.id)) {
            continue;
        }

        const bool leftHalf = touch.x < width * 0.5f;
        TouchTracker *tracker = nullptr;
        if (leftHalf && !m_moveTouch.active) {
            tracker = &m_moveTouch;
            if (touch.y < height * 0.45f) { m_touchJumpPressed = true; }
        } else if (!leftHalf && !m_lookTouch.active) {
            tracker = &m_lookTouch;
        }
        if (tracker == nullptr) { continue; }

        tracker->id = touch.id;
        tracker->startX = touch.x;
        tracker->startY = touch.y;
        tracker->lastX = touch.x;
        tracker->lastY = touch.y;
        tracker->active = true;
    }

    if (m_moveTouch.active) {
        const float radius = std::max(touchMoveRadius, 1.0f);
        glm::vec2 move(
            (m_moveTouch.lastX - m_moveTouch.startX) / radius,
            -(m_moveTouch.lastY - m_moveTouch.startY) / radius
        );
        const float length = glm::length(move);
        if (length > 1.0f) { move /= length; }
        if (glm::length(move) > 0.05f) {
            m_touchMoveDirection = getHorizontalRight() * move.x + getHorizontalForward() * move.y;
            if (glm::length(m_touchMoveDirection) > 0.0001f) {
                m_touchMoveDirection = glm::normalize(m_touchMoveDirection);
            }
        }
    }

    const NeverlandTouchControls::MoveAxes buttonMove = NeverlandTouchControls::getMoveAxes();
    if (std::abs(buttonMove.x) > 0.001f || std::abs(buttonMove.y) > 0.001f) {
        glm::vec3 buttonMoveDirection =
            getHorizontalRight() * buttonMove.x + getHorizontalForward() * buttonMove.y;
        if (glm::length(buttonMoveDirection) > 0.0001f) {
            m_touchMoveDirection += glm::normalize(buttonMoveDirection);
            if (glm::length(m_touchMoveDirection) > 0.0001f) {
                m_touchMoveDirection = glm::normalize(m_touchMoveDirection);
            } else {
                m_touchMoveDirection = glm::vec3(0.0f);
            }
        }
    }
    m_touchJumpPressed = m_touchJumpPressed || NeverlandTouchControls::consumeJumpPressed();
}

void PlayerController::updateLook() {
    const bool hasTouchLook =
        std::abs(m_touchLookDeltaX) > 0.001f || std::abs(m_touchLookDeltaY) > 0.001f;
    if (ApplicationAPI::isCursorLocked() || hasTouchLook) {
        double deltaX = Input::getMouseDeltaX();
        double deltaY = Input::getMouseDeltaY();
        if (!ApplicationAPI::isCursorLocked()) {
            deltaX = 0.0;
            deltaY = 0.0;
        }
        // DeltaX - смещение мыши за реальное время, поэтому умножение на deltaTime не требуется.
        // Действия в реальном мире не нужно умножать на deltaTime, умножать нужно только действия в
        // игровом мире.
        m_pitch += glm::radians(static_cast<float>(-deltaY * mouseSpeed));
        m_yaw += glm::radians(static_cast<float>(-deltaX * mouseSpeed));
        m_pitch += glm::radians(-m_touchLookDeltaY * touchLookSpeed);
        m_yaw += glm::radians(-m_touchLookDeltaX * touchLookSpeed);
        const float maxPitch = glm::radians(89.0f);
        m_pitch = glm::clamp(m_pitch, -maxPitch, maxPitch);
        syncRotationFromAngles();
    }
    // Захватом курсора владеет GameMenu (Esc — пауза/возврат), Tab-тоггла больше нет.
}

void PlayerController::updateCharacter(float deltaTime) {
    if (Input::isKeyJustPressed(Key::F)) {
        setFlyModeEnabled(flyMode == 0);
    }

    glm::vec3 inputDirection(0.0f);
    if (Input::isKeyPressed(Key::W)) { inputDirection += getHorizontalForward(); }
    if (Input::isKeyPressed(Key::S)) { inputDirection -= getHorizontalForward(); }
    if (Input::isKeyPressed(Key::A)) { inputDirection -= getHorizontalRight(); }
    if (Input::isKeyPressed(Key::D)) { inputDirection += getHorizontalRight(); }
    inputDirection += m_touchMoveDirection;
    if (glm::length(inputDirection) > 0.0001f) { inputDirection = glm::normalize(inputDirection); }

    glm::vec3 position = getEyePosition();
    const glm::vec3 previousPhysicsPosition = position;
    const bool wasGrounded = m_characterState.grounded;
    const bool jumpPressed = Input::isKeyJustPressed(Key::SPACE) || m_touchJumpPressed;
    const bool gameplayJumpPressed = updateJumpModeToggle(jumpPressed, deltaTime);
    const bool isFlyMode = flyMode != 0;
    const float speed = isFlyMode
                            ? std::max(flySpeed, 0.0f)
                            : moveSpeed * (Input::isKeyPressed(Key::LEFT_SHIFT) ? sprintMultiplier : 1.0f);

    VoxelCharacterConfig config;
    config.radius = playerRadius;
    config.height = playerHeight;
    config.eyeHeight = eyeHeight;
    config.stepHeight = stepHeight;
    config.gravity = gravity;
    config.jumpSpeed = jumpSpeed;
    config.maxFallSpeed = maxFallSpeed;
    config.groundSnapDistance = groundSnapDistance;
    config.coyoteTime = coyoteTime;

    VoxelCharacterInput input;
    input.horizontalDirection = inputDirection;
    input.horizontalSpeed = speed;
    input.jumpPressed = !isFlyMode && gameplayJumpPressed;
    input.flyMode = isFlyMode;
    if (isFlyMode) {
        if (Input::isKeyPressed(Key::SPACE) ||
            (enableTouchControls != 0 && NeverlandTouchControls::isJumpDown())) {
            input.flyVerticalInput += 1.0f;
        }
        if (Input::isKeyPressed(Key::LEFT_SHIFT)) { input.flyVerticalInput -= 1.0f; }
    }

    VoxelCharacterController::move(position, m_characterState, config, input, deltaTime);

    updateStepSmoothing(
        previousPhysicsPosition, position, wasGrounded, gameplayJumpPressed, deltaTime
    );
    m_physicsEyePosition = position;
    setEyePosition(m_physicsEyePosition + glm::vec3(0.0f, m_visualYOffset, 0.0f));
}

void PlayerController::setFlyModeEnabled(bool enabled) {
    const int nextFlyMode = enabled ? 1 : 0;
    if (flyMode == nextFlyMode) { return; }
    flyMode = nextFlyMode;
    m_characterState = {};
    m_visualYOffset = 0.0f;
    m_doubleJumpTimer = 0.0f;
    m_waitingForDoubleJump = false;
}

void PlayerController::updateStepSmoothing(
    const glm::vec3 &previousPhysicsPosition,
    const glm::vec3 &physicsPosition,
    bool wasGrounded,
    bool jumpPressed,
    float deltaTime
) {
    const float smoothingSpeed = std::max(stepSmoothingSpeed, 0.0f);
    if (smoothingSpeed <= 0.0f || flyMode != 0) {
        m_visualYOffset = 0.0f;
        return;
    }

    const float deltaY = physicsPosition.y - previousPhysicsPosition.y;
    const bool steppedUp =
        wasGrounded && m_characterState.grounded && !jumpPressed && deltaY > 0.05f;
    if (steppedUp) {
        // Keep collision exact, but let the camera catch up visually after block step-up.
        m_visualYOffset = std::max(
            m_visualYOffset - deltaY,
            -std::max(stepHeight, 0.0f)
        );
    } else if (!m_characterState.grounded || deltaY < -0.01f) {
        m_visualYOffset = 0.0f;
        return;
    }

    m_visualYOffset = std::min(m_visualYOffset + smoothingSpeed * deltaTime, 0.0f);
}

bool PlayerController::updateJumpModeToggle(bool jumpPressed, float deltaTime) {
    if (flyMode != 0) {
        if (m_doubleJumpTimer > 0.0f) {
            m_doubleJumpTimer = std::max(0.0f, m_doubleJumpTimer - deltaTime);
            if (m_doubleJumpTimer <= 0.0f) { m_waitingForDoubleJump = false; }
        }

        if (!jumpPressed) { return false; }

        if (m_waitingForDoubleJump && m_doubleJumpTimer > 0.0f) {
            setFlyModeEnabled(false);
            return false;
        }

        m_waitingForDoubleJump = true;
        m_doubleJumpTimer = std::max(doubleJumpFlyWindow, 0.35f);
        return false;
    }

    if (m_characterState.grounded && !jumpPressed) {
        m_waitingForDoubleJump = false;
        m_doubleJumpTimer = 0.0f;
    }

    if (!jumpPressed) { return false; }

    const bool canNormalJump = m_characterState.grounded || m_characterState.coyoteTimer > 0.0f;
    if (m_waitingForDoubleJump && !canNormalJump) {
        setFlyModeEnabled(true);
        return false;
    }

    if (canNormalJump) {
        m_waitingForDoubleJump = true;
        m_doubleJumpTimer = 0.0f;
    }
    return true;
}

void PlayerController::updateVectors() {
    Quat rotation = TransformComponentAPI::getRotation(getEntity());
    glm::quat quat = glm::normalize(glm::quat(rotation.w, rotation.x, rotation.y, rotation.z));
    m_front = glm::normalize(quat * glm::vec3(0.f, 0.f, -1.f));
    m_right = glm::normalize(quat * glm::vec3(1.f, 0.f, 0.f));
    m_up = glm::normalize(quat * glm::vec3(0.f, 1.f, 0.f));
}

glm::vec3 PlayerController::getEyePosition() {
    if (m_hasPhysicsEyePosition) { return m_physicsEyePosition; }
    const Vec3 position = TransformComponentAPI::getPosition(getEntity());
    return glm::vec3(position.x, position.y, position.z);
}

void PlayerController::setEyePosition(const glm::vec3 &position) {
    TransformComponentAPI::setPosition(getEntity(), {position.x, position.y, position.z});
}

Vec3 PlayerController::getRayDirectionForScreenPoint(float screenX, float screenY) const {
    const float width = static_cast<float>(std::max(ApplicationAPI::getWidth(), 1u));
    const float height = static_cast<float>(std::max(ApplicationAPI::getHeight(), 1u));
    const float ndcX = (2.0f * screenX / width) - 1.0f;
    const float ndcY = 1.0f - (2.0f * screenY / height);
    const float aspect = width / height;
    const float tanHalfFov = std::tan(glm::radians(CAMERA_VERTICAL_FOV_DEGREES) * 0.5f);

    const glm::vec3 direction = glm::normalize(
        m_front + m_right * (ndcX * tanHalfFov * aspect) + m_up * (ndcY * tanHalfFov)
    );
    return Vec3(direction.x, direction.y, direction.z);
}

glm::vec3 PlayerController::getHorizontalForward() const {
    return glm::normalize(glm::vec3(-std::sin(m_yaw), 0.0f, -std::cos(m_yaw)));
}

glm::vec3 PlayerController::getHorizontalRight() const {
    return glm::normalize(glm::vec3(std::cos(m_yaw), 0.0f, -std::sin(m_yaw)));
}

void PlayerController::syncRotationFromAngles() {
    TransformComponentAPI::setRotation(
        getEntity(), toBambooQuat(glm::quat(glm::vec3(m_pitch, m_yaw, 0.0f)))
    );
    updateVectors();
}

void PlayerController::syncAnglesFromRotation(const Quat &rotation) {
    glm::quat quat(rotation.w, rotation.x, rotation.y, rotation.z);
    if (glm::length(quat) < 0.0001f) { quat = glm::quat(glm::vec3(0.0f)); }
    quat = glm::normalize(quat);

    const glm::vec3 forward = glm::normalize(quat * glm::vec3(0.0f, 0.0f, -1.0f));
    m_pitch = asinf(glm::clamp(forward.y, -1.0f, 1.0f));
    m_yaw = atan2f(-forward.x, -forward.z);
    syncRotationFromAngles();
}
