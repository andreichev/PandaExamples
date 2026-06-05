//
// Created by Bogdan on 28.12.2024.
//

#include <Bamboo/Logger.hpp>

#include <Bamboo/Math.hpp>
#include "Camera.hpp"

void Camera::start() {
    velocity = {};
}

void Camera::lateUpdate(float dt) {
    if (!target.isValid()) {
        LOG_ERROR("Player not valid");
        return;
    }
    Vec3 targetPosition = TransformComponentAPI::getPosition(target);
    Vec3 position = TransformComponentAPI::getPosition(getEntity());
    Vec3 result = Math::smoothDamp(position, targetPosition, velocity, 0.15, dt);
    TransformComponentAPI::setPosition(getEntity(), {result.x, result.y, position.z});
}
