//
// Created by Admin on 12.02.2022.
//

#include "CameraMove.hpp"
#include "Model/ChunksStorage.hpp"

#include <Bamboo/Math.hpp>
#include <Bamboo/Input.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/ApplicationAPI.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace {

Quat toBambooQuat(const glm::quat &quat) {
    return Quat(quat.x, quat.y, quat.z, quat.w);
}

} // namespace

void CameraMove::start() {
    TransformComponentAPI::setPosition(
        getEntity(),
        {ChunksStorage::WORLD_SIZE_X / 2,
         ChunksStorage::WORLD_SIZE_Y / 2,
         ChunksStorage::WORLD_SIZE_Z / 2}
    );
    TransformComponentAPI::setRotation(
        getEntity(), toBambooQuat(glm::quat(glm::vec3(glm::radians(25.f), 0.f, 0.f)))
    );
    syncAnglesFromRotation(TransformComponentAPI::getRotation(getEntity()));
}

void CameraMove::update(float deltaTime) {
    float speed = m_moveSpeed * deltaTime;
    if (Input::isKeyPressed(Key::W)) { translate(m_front * speed); }
    if (Input::isKeyPressed(Key::S)) { translate(-m_front * speed); }
    if (Input::isKeyPressed(Key::A)) { translate(-m_right * speed); }
    if (Input::isKeyPressed(Key::D)) { translate(m_right * speed); }
    if (Input::isKeyPressed(Key::SPACE)) { translate(m_up * speed); }
    if (Input::isKeyPressed(Key::LEFT_SHIFT)) {
        translate(-m_up * speed);
    }
    if (ApplicationAPI::isCursorLocked()) {
        double deltaX = Input::getMouseDeltaX();
        double deltaY = Input::getMouseDeltaY();
        // DeltaX - смещение мыши за реальное время, поэтому умножение на deltaTime не требуется.
        // Действия в реальном мире не нужно умножать на deltaTime, умножать нужно только действия в
        // игровом мире.
        m_pitch += glm::radians(static_cast<float>(-deltaY * m_mouseSpeed));
        m_yaw += glm::radians(static_cast<float>(-deltaX * m_mouseSpeed));
        const float maxPitch = glm::radians(89.0f);
        m_pitch = glm::clamp(m_pitch, -maxPitch, maxPitch);
        syncRotationFromAngles();
    }
    if (Input::isKeyJustPressed(Key::TAB)) { ApplicationAPI::toggleCursorLock(); }
}

void CameraMove::updateVectors() {
    Quat rotation = TransformComponentAPI::getRotation(getEntity());
    glm::quat quat = glm::normalize(glm::quat(rotation.w, rotation.x, rotation.y, rotation.z));
    m_front = glm::normalize(quat * glm::vec3(0.f, 0.f, -1.f));
    m_right = glm::normalize(quat * glm::vec3(1.f, 0.f, 0.f));
    m_up = glm::normalize(quat * glm::vec3(0.f, 1.f, 0.f));
}

void CameraMove::translate(glm::vec3 translation) {
    TransformComponentAPI::translate(getEntity(), {translation.x, translation.y, translation.z});
}

void CameraMove::syncRotationFromAngles() {
    TransformComponentAPI::setRotation(getEntity(), toBambooQuat(glm::quat(glm::vec3(m_pitch, m_yaw, 0.0f))));
    updateVectors();
}

void CameraMove::syncAnglesFromRotation(const Quat &rotation) {
    glm::quat quat(rotation.w, rotation.x, rotation.y, rotation.z);
    if (glm::length(quat) < 0.0001f) { quat = glm::quat(glm::vec3(0.0f)); }
    quat = glm::normalize(quat);

    const glm::vec3 forward = glm::normalize(quat * glm::vec3(0.0f, 0.0f, -1.0f));
    m_pitch = asinf(glm::clamp(forward.y, -1.0f, 1.0f));
    m_yaw = atan2f(-forward.x, -forward.z);
    syncRotationFromAngles();
}
