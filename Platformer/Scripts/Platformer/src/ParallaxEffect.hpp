//
// Created by Bogdan on 01.01.2025.
//

#pragma once

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>

using namespace Bamboo;

class ParallaxEffect  : public Bamboo::Script {
public:
    void start() override;
    void update(float dt) override;

    EntityHandle target;
    float speed;
    float offsetY;
    float offsetX;

    PANDA_FIELDS_BEGIN(ParallaxEffect)
    PANDA_FIELD(target)
    PANDA_FIELD(speed)
    PANDA_FIELDS_END
};

REGISTER_SCRIPT(ParallaxEffect)
