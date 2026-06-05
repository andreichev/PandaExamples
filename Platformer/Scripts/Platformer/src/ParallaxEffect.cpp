//
// Created by Bogdan on 01.01.2025.
//

#include <Bamboo/Math.hpp>
#include <Bamboo/Logger.hpp>

#include "ParallaxEffect.hpp"

void ParallaxEffect::start() {
    Vec3 position = TransformComponentAPI::getPosition(getEntity());
    Vec3 targetPosition = TransformComponentAPI::getPosition(target);
    offsetY = position.y - targetPosition.y;
    offsetX = position.x - targetPosition.x;
}

void ParallaxEffect::lateUpdate(float dt) {
    (void)dt;
    Vec3 targetPosition = TransformComponentAPI::getPosition(target);
    Vec3 position = TransformComponentAPI::getPosition(getEntity());

    position.x = targetPosition.x * speed + offsetX;
    position.y = targetPosition.y * speed + offsetY;

    TransformComponentAPI::setPosition(getEntity(), position);
}
