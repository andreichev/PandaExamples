//
// Created by Bogdan on 28.12.2024.
//

#pragma once

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>

using namespace Bamboo;

class Camera : public Bamboo::Script {
public:
    void start() override;
    void update(float dt) override;

    EntityHandle target;
    PANDA_FIELDS_BEGIN(Camera)
    PANDA_FIELD(target)
    PANDA_FIELDS_END
};

REGISTER_SCRIPT(Camera)
