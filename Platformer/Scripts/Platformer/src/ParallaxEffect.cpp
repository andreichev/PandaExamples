//
// Created by Bogdan on 01.01.2025.
//

#include "ParallaxEffect.hpp"

void ParallaxEffect::start() {
    if (!target.isValid()) { return; }
    originPosition = TransformComponentAPI::getPosition(getEntity());
    originTargetPosition = TransformComponentAPI::getPosition(target);
}

void ParallaxEffect::lateUpdate(float dt) {
    (void)dt;
    if (!target.isValid()) { return; }
    Vec3 targetPosition = TransformComponentAPI::getPosition(target);
    Vec3 position = originPosition;

    position.x = originPosition.x + (targetPosition.x - originTargetPosition.x) * speed;
    position.y = originPosition.y + (targetPosition.y - originTargetPosition.y) * speed;

    TransformComponentAPI::setPosition(getEntity(), position);
}
