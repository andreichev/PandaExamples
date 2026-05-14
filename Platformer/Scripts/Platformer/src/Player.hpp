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

    float jumpForce;
    float speed;
    TextureHandle run;
    TextureHandle rest;
    TextureHandle jump;

    PANDA_FIELDS_BEGIN(Player)
    PANDA_FIELD(jumpForce)
    PANDA_FIELD(speed)
    PANDA_FIELD(run)
    PANDA_FIELD(jump)
    PANDA_FIELD(rest)
    PANDA_FIELDS_END
private:
    enum State { REST, RUN, JUMP };
    enum Direction { LEFT, RIGHT };

    void setState(State newState);
    void setDirection(Direction newDirection);

    Vec3 defaultPos;

    Direction direction;
    State state;
    bool onGround;

    Animation runAnim;
    Animation jumpAnim;
    Animation restAnim;
    Animation *currentAnimation;

};

REGISTER_SCRIPT(Player)
