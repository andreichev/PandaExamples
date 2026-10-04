#pragma once

#include <Bamboo/Input.hpp>

namespace Vehicle {

// What the driver asks for. Shared by every vehicle model: the keyboard is read in one place, so
// the models are compared under the same controls. The vehicles read it in fixedUpdate: inside a
// physics step a one-shot key (R, T) is seen by exactly one step.
//   W / Up        throttle (brakes while the car rolls backwards)
//   S / Down      brake, then reverse
//   A D / arrows  steering
//   Space         handbrake
//   R             put the car back on its wheels where it stands
//   T             return to the start
// Backspace and Tab are left alone: in the editor the first deletes the entity selected in the
// Hierarchy and the second moves the keyboard focus between the editor controls.
struct DriverInput {
    float throttle = 0.f; // 0..1
    float brake = 0.f;    // 0..1
    float steer = 0.f;    // -1 right .. +1 left
    bool handbrake = false;
    bool flip = false;
    bool respawn = false;
};

inline DriverInput readDriverInput() {
    using Bamboo::Input;
    using Bamboo::Key;
    DriverInput input;
    if (Input::isKeyPressed(Key::W) || Input::isKeyPressed(Key::UP)) { input.throttle = 1.f; }
    if (Input::isKeyPressed(Key::S) || Input::isKeyPressed(Key::DOWN)) { input.brake = 1.f; }
    if (Input::isKeyPressed(Key::A) || Input::isKeyPressed(Key::LEFT)) { input.steer += 1.f; }
    if (Input::isKeyPressed(Key::D) || Input::isKeyPressed(Key::RIGHT)) { input.steer -= 1.f; }
    input.handbrake = Input::isKeyPressed(Key::SPACE);
    input.flip = Input::isKeyJustPressed(Key::R);
    input.respawn = Input::isKeyJustPressed(Key::T);
    return input;
}

} // namespace Vehicle
