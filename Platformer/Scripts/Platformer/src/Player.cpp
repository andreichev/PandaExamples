#include "Player.hpp"

#include <Bamboo/Logger.hpp>
#include <Bamboo/Input.hpp>
#include <Bamboo/Math.hpp>
#include <Bamboo/ApplicationAPI.hpp>
#include <Bamboo/WorldAPI.hpp>

void Player::start() {
    defaultPos = TransformComponentAPI::getPosition(getEntity());
    if (!sprite.isValid()) { LOG_ERROR("Player: the sprite field must refer to the child entity with the sprite"); }

    restAnim.start(sprite, rest, 3, 1, 0.7);
    jumpAnim.start(sprite, jump, 3, 1);
    runAnim.start(sprite, run, 10, 1, 0.1);
    state = State::REST;
    direction = Direction::RIGHT;
    currentAnimation = &restAnim;
    groundContacts = 0;
    mobileJumpWasDown = false;
    resetCooldown = 0.f;
}

void Player::beginCollisionTouch(EntityHandle other) {
    if (strCmp("Ground", EntityAPI::getName(other), 6) == 0) {
        groundContacts++;
    } else if (resetCooldown <= 0.f && strCmp("Enemy", EntityAPI::getName(other), 5) == 0) {
        resetToStart();
    }
}

void Player::endCollisionTouch(EntityHandle other) {
    if (strCmp("Ground", EntityAPI::getName(other), 6) == 0) {
        if (groundContacts > 0) {
            groundContacts--;
        }
    }
}

void Player::beginSensorOverlap(EntityHandle sensor) {
    if (resetCooldown <= 0.f && strCmp("DiedZone", EntityAPI::getName(sensor), 8) == 0) {
        resetToStart();
    } else if (strCmp("LevelExit", EntityAPI::getName(sensor), 9) == 0 && nextLevel.isValid()) {
        WorldAPI::load(nextLevel);
    }
}

void Player::fixedUpdate(float stepTime) {
    if (resetCooldown > 0.f) {
        resetCooldown -= stepTime;
        if (resetCooldown < 0.f) { resetCooldown = 0.f; }
    }

    auto velocity = Rigidbody2DComponentAPI::getLinearVelocity(getEntity());
    MobileInput mobileInput = readMobileInput();

    if (groundContacts > 0 && (Input::isKeyJustPressed(Key::SPACE) || mobileInput.jumpPressed)) {
        velocity.y = jumpForce;
        groundContacts = 0;
    }

    if (Input::isKeyPressed(Key::A) || mobileInput.moveLeft) {
        setState(State::RUN);
        setDirection(Direction::LEFT);
        velocity.x = -speed;
    } else if (Input::isKeyPressed(Key::D) || mobileInput.moveRight) {
        setState(State::RUN);
        setDirection(Direction::RIGHT);
        velocity.x = speed;
    } else {
        float stopSpeed = 30.;
        if (velocity.x > 0.1) {
            velocity.x -= stopSpeed * stepTime;
        } else if (velocity.x < -0.1) {
            velocity.x += stopSpeed * stepTime;
        } else {
            velocity.x = 0;
            setState(State::REST);
        }
    }

    if (velocity.y > 0.1) {
        setState(State::JUMP);
    }

    Rigidbody2DComponentAPI::setLinearVelocity(getEntity(), velocity);
}

void Player::update(float dt) {
    currentAnimation->update(dt);
}

Player::MobileInput Player::readMobileInput() {
    MobileInput result;

    const float width = (float)ApplicationAPI::getWidth();
    if (width <= 0.f) { return result; }

    bool jumpDown = false;
    const int touchesCount = Input::touchCount();
    for (int i = 0; i < touchesCount; i++) {
        Input::Touch touch;
        if (!Input::tryGetTouch(i, touch)) { continue; }

        if (touch.x < width * 0.25f) {
            result.moveLeft = true;
        } else if (touch.x < width * 0.5f) {
            result.moveRight = true;
        } else {
            jumpDown = true;
        }
    }

    result.jumpPressed = jumpDown && !mobileJumpWasDown;
    mobileJumpWasDown = jumpDown;
    return result;
}

void Player::resetToStart() {
    TransformComponentAPI::setPosition(getEntity(), defaultPos);
    Rigidbody2DComponentAPI::setLinearVelocity(getEntity(), {});
    groundContacts = 0;
    resetCooldown = 1.0f;
}

void Player::setDirection(Player::Direction newDirection) {
    if (direction == newDirection) {
        return;
    }
    direction = newDirection;
    TransformComponentAPI::setRotationEuler(sprite, {0, (direction == Direction::LEFT ? 180.f : 0.f), 0});
}

void Player::setState(State newState) {
    if (state == newState) {
        return;
    }
    switch (newState) {
        case State::RUN:
            currentAnimation = &runAnim;
            break;
        case State::REST:
            currentAnimation = &restAnim;
            break;
        case JUMP:
            currentAnimation = &jumpAnim;
            break;
    }
    state = newState;
}
