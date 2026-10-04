#pragma once

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Components/SpriteRendererComponentAPI.hpp>
#include <Bamboo/Components/Rigidbody2DComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>

#include "Animation.hpp"

using namespace Bamboo;

// The player is a body driven by its velocity, with the sprite on a child entity.
// The body is driven in fixedUpdate, before every physics step. The sprite is animated in update,
// once a frame, and turned to face the way the player runs: it is a child without a body, so
// turning it does not touch the physics, while a transform set on the body entity would teleport
// the body.
class Player : public Bamboo::Script {
public:
    void start() override;
    void fixedUpdate(float stepTime) override;
    void update(float dt) override;

    void beginCollisionTouch(EntityHandle other) override;
    void endCollisionTouch(EntityHandle other) override;
    void beginSensorOverlap(EntityHandle sensor) override;

    WorldHandle nextLevel;
    EntityHandle sprite; // the child entity with the sprite renderer
    float jumpForce;
    float speed;
    TextureHandle run;
    TextureHandle rest;
    TextureHandle jump;

    PANDA_FIELDS_BEGIN(Player)
    PANDA_FIELD(nextLevel)
    PANDA_FIELD(sprite)
    PANDA_FIELD(jumpForce)
    PANDA_FIELD(speed)
    PANDA_FIELD(run)
    PANDA_FIELD(jump)
    PANDA_FIELD(rest)
    PANDA_FIELDS_END
private:
    enum State { REST, RUN, JUMP };
    enum Direction { LEFT, RIGHT };

    struct MobileInput {
        bool moveLeft = false;
        bool moveRight = false;
        bool jumpPressed = false;
    };

    void setState(State newState);
    void setDirection(Direction newDirection);
    MobileInput readMobileInput();
    void resetToStart();

    Vec3 defaultPos;

    Direction direction;
    State state;
    int groundContacts;
    bool mobileJumpWasDown;
    float resetCooldown;

    Animation runAnim;
    Animation jumpAnim;
    Animation restAnim;
    Animation *currentAnimation;

};

REGISTER_SCRIPT(Player)
