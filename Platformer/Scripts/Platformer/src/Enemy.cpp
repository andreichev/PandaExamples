#include "Enemy.hpp"

void Enemy::start() {
    if (speed <= 0.f) { speed = 2.f; }
    if (activationDistance <= 0.f) { activationDistance = 10.f; }
    target = resolveTarget();
}

void Enemy::update(float dt) {
    (void)dt;
    EntityHandle resolvedTarget = resolveTarget();
    if (!resolvedTarget.isValid()) { return; }

    Vec3 enemyPosition = TransformComponentAPI::getPosition(getEntity());
    Vec3 targetPosition = TransformComponentAPI::getPosition(resolvedTarget);
    float dx = targetPosition.x - enemyPosition.x;

    Vec2 velocity = Rigidbody2DComponentAPI::getLinearVelocity(getEntity());
    if (dx > activationDistance || dx < -activationDistance) {
        velocity.x = 0.f;
    } else {
        velocity.x = dx < 0.f ? -speed : speed;
    }
    Rigidbody2DComponentAPI::setLinearVelocity(getEntity(), velocity);
}

EntityHandle Enemy::resolveTarget() {
    if (target.isValid()) { return target; }
    target = WorldAPI::findByTag("Player");
    return target;
}
