//
// Created by Admin on 12.02.2022.
//

#include "PlayerController.hpp"
#include "Model/ChunksStorage.hpp"

#include <Bamboo/Math.hpp>
#include <Bamboo/Input.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/ApplicationAPI.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

namespace {

Quat toBambooQuat(const glm::quat &quat) {
    return Quat(quat.x, quat.y, quat.z, quat.w);
}

} // namespace

void PlayerController::start() {
    const int groundHeight = ChunksStorage::terrainHeight(0, 0);
    TransformComponentAPI::setPosition(
        getEntity(), {0.0f, groundHeight + 1.0f + eyeHeight + 1.0f, 0.0f}
    );
    TransformComponentAPI::setRotation(
        getEntity(), toBambooQuat(glm::quat(glm::vec3(glm::radians(25.f), 0.f, 0.f)))
    );
    syncAnglesFromRotation(TransformComponentAPI::getRotation(getEntity()));
}

void PlayerController::update(float deltaTime) {
    updateLook();
    updateCharacter(deltaTime);
}

void PlayerController::shutdown() {
    ApplicationAPI::setCursorLocked(false);
}

void PlayerController::updateLook() {
    if (ApplicationAPI::isCursorLocked()) {
        double deltaX = Input::getMouseDeltaX();
        double deltaY = Input::getMouseDeltaY();
        // DeltaX - смещение мыши за реальное время, поэтому умножение на deltaTime не требуется.
        // Действия в реальном мире не нужно умножать на deltaTime, умножать нужно только действия в
        // игровом мире.
        m_pitch += glm::radians(static_cast<float>(-deltaY * mouseSpeed));
        m_yaw += glm::radians(static_cast<float>(-deltaX * mouseSpeed));
        const float maxPitch = glm::radians(89.0f);
        m_pitch = glm::clamp(m_pitch, -maxPitch, maxPitch);
        syncRotationFromAngles();
    }
    if (Input::isKeyJustPressed(Key::TAB)) {
        ApplicationAPI::setCursorLocked(!ApplicationAPI::isCursorLocked());
    }
}

void PlayerController::updateCharacter(float deltaTime) {
    if (Input::isKeyJustPressed(Key::F)) {
        flyMode = flyMode == 0 ? 1 : 0;
        m_characterState = {};
    }

    glm::vec3 inputDirection(0.0f);
    if (Input::isKeyPressed(Key::W)) { inputDirection += getHorizontalForward(); }
    if (Input::isKeyPressed(Key::S)) { inputDirection -= getHorizontalForward(); }
    if (Input::isKeyPressed(Key::A)) { inputDirection -= getHorizontalRight(); }
    if (Input::isKeyPressed(Key::D)) { inputDirection += getHorizontalRight(); }
    if (glm::length(inputDirection) > 0.0001f) { inputDirection = glm::normalize(inputDirection); }

    glm::vec3 position = getEyePosition();
    const bool isFlyMode = flyMode != 0;
    const float speed =
        moveSpeed * (!isFlyMode && Input::isKeyPressed(Key::LEFT_SHIFT) ? sprintMultiplier : 1.0f);

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
    input.jumpPressed = !isFlyMode && Input::isKeyJustPressed(Key::SPACE);
    input.flyMode = isFlyMode;
    if (isFlyMode) {
        if (Input::isKeyPressed(Key::SPACE)) { input.flyVerticalInput += 1.0f; }
        if (Input::isKeyPressed(Key::LEFT_SHIFT)) { input.flyVerticalInput -= 1.0f; }
    }

    VoxelCharacterController::move(position, m_characterState, config, input, deltaTime);

    setEyePosition(position);
}

void PlayerController::updateVectors() {
    Quat rotation = TransformComponentAPI::getRotation(getEntity());
    glm::quat quat = glm::normalize(glm::quat(rotation.w, rotation.x, rotation.y, rotation.z));
    m_front = glm::normalize(quat * glm::vec3(0.f, 0.f, -1.f));
    m_right = glm::normalize(quat * glm::vec3(1.f, 0.f, 0.f));
    m_up = glm::normalize(quat * glm::vec3(0.f, 1.f, 0.f));
}

glm::vec3 PlayerController::getEyePosition() {
    const Vec3 position = TransformComponentAPI::getPosition(getEntity());
    return glm::vec3(position.x, position.y, position.z);
}

void PlayerController::setEyePosition(const glm::vec3 &position) {
    TransformComponentAPI::setPosition(getEntity(), {position.x, position.y, position.z});
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
