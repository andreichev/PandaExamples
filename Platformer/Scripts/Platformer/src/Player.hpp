#pragma once

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Components/SpriteRendererComponentAPI.hpp>
#include <Bamboo/Components/Rigidbody2DComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>

#include "Animation.hpp"

using namespace Bamboo;

class Player : public Bamboo::Script {
public:
    void start() override;
    void update(float dt) override;

    void beginCollisionTouch(EntityHandle other) override;
    void endCollisionTouch(EntityHandle other) override;
    void beginSensorOverlap(EntityHandle sensor) override;

    WorldHandle nextLevel;
    float jumpForce;
    float speed;
    TextureHandle run;
    TextureHandle rest;
    TextureHandle jump;

    PANDA_FIELDS_BEGIN(Player)
    PANDA_FIELD(nextLevel)
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
