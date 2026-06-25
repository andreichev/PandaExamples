//
// Created by Admin on 12.02.2022.
//

#pragma once

#include <Bamboo/Script.hpp>
#include <Bamboo/Bamboo.hpp>
#include <glm/glm.hpp>

using namespace Bamboo;

class CameraMove final : public Script {
public:
    void start() override;
    void update(float deltaTime) override;
    void shutdown() override;

    Vec3 getFront() {
        return Vec3(m_front.x, m_front.y, m_front.z);
    }

private:
    void updateVectors();
    void translate(glm::vec3 translation);
    void syncRotationFromAngles();
    void syncAnglesFromRotation(const Quat &rotation);

    float m_mouseSpeed = 0.2f;
    float m_moveSpeed = 20.0f;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    glm::vec3 m_front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 m_up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 m_right = glm::vec3(1.0f, 0.0f, 0.0f);
};

REGISTER_SCRIPT(CameraMove)
