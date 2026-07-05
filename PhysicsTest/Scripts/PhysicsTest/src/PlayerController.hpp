#pragma once

#include <Bamboo/Script.hpp>
#include <Bamboo/Bamboo.hpp>

using namespace Bamboo;

// Kinematic player controller built on CharacterMoverAPI::move (box3d character mover).
// Entity setup: Rigidbody3D (type KINEMATIC) + Capsule Collider 3D + this script.
// WASD — move in world XZ plane, SPACE — jump. Tune speed/jump/gravity in the Inspector.
class PlayerController : public Script {
public:
    float speed = 4.f;     // run speed, m/s
    float jumpSpeed = 6.f; // vertical takeoff speed, m/s
    float gravity = 15.f;  // fall acceleration, m/s^2

    PANDA_FIELDS_BEGIN(PlayerController)
    PANDA_FIELD(speed)
    PANDA_FIELD(jumpSpeed)
    PANDA_FIELD(gravity)
    PANDA_FIELDS_END

    void update(float deltaTime) override;

private:
    Vec3 m_velocity;
    bool m_grounded = false;
};

REGISTER_SCRIPT(PlayerController)
