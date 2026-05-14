//
// Created by Bogdan on 31.12.2024.
//

#pragma once

//
// Created by Bogdan on 28.12.2024.
//

#pragma once

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <Bamboo/Components/Rigidbody2DComponentAPI.hpp>

using namespace Bamboo;

class Empty : public Bamboo::Script {
public:
    void start() override {
        Rigidbody2DComponentAPI::getMass(getEntity());
    }
    void update(float dt) override;

private:
};

REGISTER_SCRIPT(Empty)
