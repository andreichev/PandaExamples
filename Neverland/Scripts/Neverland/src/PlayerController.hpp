//
// Created by Admin on 12.02.2022.
//

#pragma once

#include "Physics/VoxelCharacterController.hpp"

#include <Bamboo/Script.hpp>
#include <Bamboo/Bamboo.hpp>
#include <glm/glm.hpp>

using namespace Bamboo;

class PlayerController final : public Script {
public:
    float mouseSpeed = 0.2f;
    float moveSpeed = 6.0f;
    float sprintMultiplier = 2.1f;
    float flySpeed = 24.0f; // скорость в режиме полёта (горизонталь и вертикаль)
    float gravity = 28.0f;
    float jumpSpeed = 8.5f;
    float maxFallSpeed = 48.0f;
    float playerRadius = 0.3f;
    float playerHeight = 1.8f;
    float eyeHeight = 1.62f;
    float stepHeight = 1.05f;
    float groundSnapDistance = 0.12f;
    float coyoteTime = 0.08f;
    float stepSmoothingSpeed = 8.0f;
    float touchLookSpeed = 0.12f;
    float touchMoveRadius = 90.0f;
    float doubleJumpFlyWindow = 0.45f;
    int enableTouchControls = 1;
    int flyMode = 0;

    PANDA_FIELDS_BEGIN(PlayerController)
    PANDA_FIELD(mouseSpeed)
    PANDA_FIELD(moveSpeed)
    PANDA_FIELD(sprintMultiplier)
    PANDA_FIELD(flySpeed)
    PANDA_FIELD(gravity)
    PANDA_FIELD(jumpSpeed)
    PANDA_FIELD(maxFallSpeed)
    PANDA_FIELD(playerRadius)
    PANDA_FIELD(playerHeight)
    PANDA_FIELD(eyeHeight)
    PANDA_FIELD(stepHeight)
    PANDA_FIELD(groundSnapDistance)
    PANDA_FIELD(coyoteTime)
    PANDA_FIELD(stepSmoothingSpeed)
    PANDA_FIELD(touchLookSpeed)
    PANDA_FIELD(touchMoveRadius)
    PANDA_FIELD(doubleJumpFlyWindow)
    PANDA_FIELD(enableTouchControls)
    PANDA_FIELD(flyMode)
    PANDA_FIELDS_END

    void start() override;
    void update(float deltaTime) override;
    void shutdown() override;

    Vec3 getFront() {
        return Vec3(m_front.x, m_front.y, m_front.z);
    }
    Vec3 getRayDirectionForScreenPoint(float screenX, float screenY) const;

private:
    void updateVectors();
    void updateTouchControls();
    void updateLook();
    void updateCharacter(float deltaTime);
    void setFlyModeEnabled(bool enabled);
    bool updateJumpModeToggle(bool jumpPressed, float deltaTime);
    void updateStepSmoothing(
        const glm::vec3 &previousPhysicsPosition,
        const glm::vec3 &physicsPosition,
        bool wasGrounded,
        bool jumpPressed,
        float deltaTime
    );
    glm::vec3 getEyePosition();
    void setEyePosition(const glm::vec3 &position);
    glm::vec3 getHorizontalForward() const;
    glm::vec3 getHorizontalRight() const;
    void syncRotationFromAngles();
    void syncAnglesFromRotation(const Quat &rotation);

    struct TouchTracker {
        int id = -1;
        float startX = 0.0f;
        float startY = 0.0f;
        float lastX = 0.0f;
        float lastY = 0.0f;
        bool active = false;
    };

    VoxelCharacterState m_characterState;
    glm::vec3 m_physicsEyePosition = glm::vec3(0.0f);
    float m_visualYOffset = 0.0f;
    bool m_hasPhysicsEyePosition = false;
    TouchTracker m_moveTouch;
    TouchTracker m_lookTouch;
    glm::vec3 m_touchMoveDirection = glm::vec3(0.0f);
    float m_touchLookDeltaX = 0.0f;
    float m_touchLookDeltaY = 0.0f;
    bool m_touchJumpPressed = false;
    float m_doubleJumpTimer = 0.0f;
    bool m_waitingForDoubleJump = false;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    glm::vec3 m_front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 m_up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 m_right = glm::vec3(1.0f, 0.0f, 0.0f);
};

REGISTER_SCRIPT(PlayerController)
