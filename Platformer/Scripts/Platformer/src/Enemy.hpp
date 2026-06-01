#pragma once

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/WorldAPI.hpp>
#include <Bamboo/Components/Rigidbody2DComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>

using namespace Bamboo;

class Enemy : public Bamboo::Script {
public:
    void start() override;
    void update(float dt) override;

    EntityHandle target;
    float speed;
    float activationDistance;

    PANDA_FIELDS_BEGIN(Enemy)
    PANDA_FIELD(target)
    PANDA_FIELD(speed)
    PANDA_FIELD(activationDistance)
    PANDA_FIELDS_END

private:
    EntityHandle resolveTarget();
};

REGISTER_SCRIPT(Enemy)
