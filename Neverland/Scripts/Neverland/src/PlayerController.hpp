//
// Created by Admin on 12.02.2022.
//

#pragma once

#include <Bamboo/Script.hpp>
#include <Bamboo/Bamboo.hpp>
#include <glm/glm.hpp>

using namespace Bamboo;

class PlayerController final : public Script {
public:
    void start() override;
    void update(float deltaTime) override;
    void shutdown() override;

    Vec3 getFront() {
        return Vec3(m_front.x, m_front.y, m_front.z);
    }

private:
    void updateVectors();
    void updateLook();
    void updateCharacter(float deltaTime);
    glm::vec3 getEyePosition();
    void setEyePosition(const glm::vec3 &position);
    glm::vec3 getHorizontalForward() const;
    glm::vec3 getHorizontalRight() const;
    bool moveHorizontalWithStep(glm::vec3 &position, const glm::vec3 &translation);
    bool moveAxis(
        glm::vec3 &position,
        int axis,
        float translation,
        bool &hitNegative,
        bool &hitPositive
    ) const;
    void syncRotationFromAngles();
    void syncAnglesFromRotation(const Quat &rotation);

    float m_mouseSpeed = 0.2f;
    float m_moveSpeed = 6.0f;
    float m_sprintMultiplier = 1.6f;
    float m_gravity = 28.0f;
    float m_jumpSpeed = 8.5f;
    float m_maxFallSpeed = 48.0f;
    float m_playerRadius = 0.3f;
    float m_playerHeight = 1.8f;
    float m_eyeHeight = 1.62f;
    float m_stepHeight = 1.05f;
    float m_verticalVelocity = 0.0f;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    bool m_grounded = false;
    glm::vec3 m_front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 m_up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 m_right = glm::vec3(1.0f, 0.0f, 0.0f);
};

REGISTER_SCRIPT(PlayerController)
