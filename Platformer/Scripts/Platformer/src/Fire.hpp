//
// Created by Bogdan on 02.01.2025.
//
//
// Created by Bogdan on 01.01.2025.
//

#pragma once

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Components/SpriteRendererComponentAPI.hpp>
#include "Animation.hpp"

using namespace Bamboo;

class Fire  : public Bamboo::Script {
public:
    void start() override;

    void update(float dt) override;

    TextureHandle fire;

    PANDA_FIELDS_BEGIN(Fire)
    PANDA_FIELD(fire)
    PANDA_FIELDS_END
private:
    Animation animation;
};

REGISTER_SCRIPT(Fire)
