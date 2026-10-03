#pragma once

#include "Vehicle/Driving.hpp"

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace Bamboo;

// A car on physical wheels: the chassis and four wheels are separate bodies, every wheel hangs on
// a Wheel Joint 3D (suspension spring, spin motor, steering). The solver does the physics; this
// script only turns the pedals and the steering wheel into motor and steering targets.
//
// Entity setup (all five bodies are root entities):
//   chassis — Rigidbody3D (DYNAMIC) + Box Collider 3D + this script, looks along its local -Z;
//   wheel   — Rigidbody3D (DYNAMIC) + Sphere Collider 3D + Wheel Joint 3D connected to the chassis,
//             spin axis to the LEFT of the car (-X: a positive spin rolls forward), suspension
//             axis up; the front wheels have steering enabled.
// The four wheel fields refer to the wheel entities and are read when Play starts; the other
// fields can be tuned in the inspector while the car drives. The values below are only a
// reference set — the engine takes every field from the world file.
class JointCar : public Script {
public:
    EntityHandle wheelFrontLeft;
    EntityHandle wheelFrontRight;
    EntityHandle wheelRearLeft;
    EntityHandle wheelRearRight;
    int driveWheels = 0;             // 0 rear, 1 front, 2 all four
    float motorTorque = 650.f;       // N·m per driven wheel from a standstill, fades towards maxSpeed
    float maxSpeed = 140.f;          // km/h
    float reverseSpeed = 40.f;       // km/h
    float brakeTorque = 1000.f;      // N·m per front wheel, the rear wheels get less
    float handbrakeTorque = 3000.f;  // N·m per rear wheel: locks it
    float coastTorque = 40.f;        // N·m per driven wheel with the pedals released (engine braking)
    float maxSteerAngle = 32.f;      // degrees: the full lock at low speed
    float steerGrip = 0.95f;         // g: the lateral acceleration the full lock asks for at speed
    float steerRate = 2.5f;          // 1 / steerRate — seconds from the center to the full lock
    float rideFrequency = 2.2f;      // Hz: how fast the body bounces on its springs (1.5 soft .. 3 stiff)
    float rideDamping = 0.6f;        // damping ratio of the body: 0.3 bouncy .. 1 no overshoot
    float wheelRadius = 0.35f;       // m: converts the car speed into the wheel spin

    PANDA_FIELDS_BEGIN(JointCar)
    PANDA_FIELD(wheelFrontLeft)
    PANDA_FIELD(wheelFrontRight)
    PANDA_FIELD(wheelRearLeft)
    PANDA_FIELD(wheelRearRight)
    PANDA_FIELD(driveWheels)
    PANDA_FIELD(motorTorque)
    PANDA_FIELD(maxSpeed)
    PANDA_FIELD(reverseSpeed)
    PANDA_FIELD(brakeTorque)
    PANDA_FIELD(handbrakeTorque)
    PANDA_FIELD(coastTorque)
    PANDA_FIELD(maxSteerAngle)
    PANDA_FIELD(steerGrip)
    PANDA_FIELD(steerRate)
    PANDA_FIELD(rideFrequency)
    PANDA_FIELD(rideDamping)
    PANDA_FIELD(wheelRadius)
    PANDA_FIELDS_END

    void start() override;
    void update(float deltaTime) override;
    void shutdown() override;

private:
    struct Wheel {
        EntityHandle entity;
        bool front = false;
        glm::vec3 restOffset{0.f};                  // in the chassis space, at start
        glm::quat restRotation{1.f, 0.f, 0.f, 0.f}; // relative to the chassis, at start
    };

    void applySuspension();
    // Teleports the whole car: the wheels keep their start places relative to the chassis.
    void placeAt(const glm::vec3 &position, const glm::quat &rotation);

    Wheel m_wheels[4];
    bool m_ready = false;
    float m_wheelbase = 0.f;
    float m_track = 0.f;
    Vehicle::SteeringWheel m_steering;
    // The ride the joints are tuned for: a change of the fields is applied again.
    float m_appliedFrequency = 0.f;
    float m_appliedDamping = 0.f;
    glm::vec3 m_spawnPosition{0.f};
    glm::quat m_spawnRotation{1.f, 0.f, 0.f, 0.f};
};

REGISTER_SCRIPT(JointCar)
