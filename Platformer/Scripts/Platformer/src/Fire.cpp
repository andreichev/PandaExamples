//
// Created by Bogdan on 02.01.2025.
//

#include "Fire.hpp"

void Fire::start() {
    animation.start(getEntity(), fire, 6, 1, 0.1);
}

void Fire::update(float dt) {
    animation.update(dt);
}
