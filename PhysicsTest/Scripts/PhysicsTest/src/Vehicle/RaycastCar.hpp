#pragma once

#include "Vehicle/Driving.hpp"

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace Bamboo;

// A car on rays: one body, and under every wheel a ray instead of a wheel. The script is the whole
// model — the suspension, the tire grip, the drive and the brakes are what it applies to the
// chassis where the rays meet the ground. The wheels are only drawn.
//
// The model runs in fixedUpdate, before every physics step. What depends on a position — the
// spring and the hold on a slope — is a force over the step. What depends on a velocity — the
// damper and the tire grip — is an impulse that changes the velocity at once: the impulses are
// found one after another with the mass the chassis shows at each wheel, the way a physics solver
// does it, so the car stays steady whatever the step.
//
// Entity setup:
//   chassis — a root entity: Rigidbody3D (DYNAMIC) + Box Collider 3D + this script, looks along
//             its local -Z;
//   wheel   — a child of the chassis without a body, placed where the wheel hangs with the spring
//             released; its local X is the axle. The script moves and turns it.
// The four wheel fields refer to the wheel entities and are read when Play starts; the other
// fields can be tuned in the inspector while the car drives. The drive, steering and ride fields
// mean the same as in JointCar. The values below are only a reference set — the engine takes
// every field from the world file.
class RaycastCar : public Script {
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
    float wheelRadius = 0.35f;       // m
    float suspensionTravel = 0.18f;  // m: how far a spring compresses before the bump stop
    float gripForward = 1.f;         // tire friction along the rolling direction, on a surface of friction 1
    float gripSide = 1.f;            // tire friction across the rolling direction
    int wheelRays = 3;               // rays per wheel: 1 — the wheel is a point, 3..7 — a round wheel

    PANDA_FIELDS_BEGIN(RaycastCar)
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
    PANDA_FIELD(suspensionTravel)
    PANDA_FIELD(gripForward)
    PANDA_FIELD(gripSide)
    PANDA_FIELD(wheelRays)
    PANDA_FIELDS_END

    void start() override;
    void fixedUpdate(float stepTime) override;
    void shutdown() override;

private:
    struct Wheel {
        EntityHandle entity;
        bool front = false;
        glm::vec3 restOffset{0.f};                  // wheel center in the chassis space, the spring released
        glm::quat restRotation{1.f, 0.f, 0.f, 0.f}; // in the chassis space
        float compression = 0.f;                    // m, as drawn
        float spin = 0.f;                           // rad/s, + rolls forward
        float spinAngle = 0.f;                      // rad
    };

    // The chassis in this frame. The solver changes its velocities with every impulse and sums
    // the impulses up; the sum goes to the body once.
    struct Body {
        EntityHandle entity;
        glm::vec3 position{0.f};
        glm::quat rotation{1.f, 0.f, 0.f, 0.f};
        glm::vec3 up{0.f, 1.f, 0.f};
        float mass = 0.f;
        glm::vec3 center{0.f};         // center of mass, world
        glm::mat3 inverseInertia{0.f}; // world
        glm::vec3 linear{0.f};         // velocity of the center of mass
        glm::vec3 angular{0.f};
        glm::vec3 linearImpulse{0.f};  // the sum of the solver
        glm::vec3 angularImpulse{0.f}; // around the center of mass

        glm::vec3 velocityAt(const glm::vec3 &point) const;
        // The velocity a unit impulse along the direction gives the point in that direction: the
        // inverse of the mass the body shows there.
        float inverseMassAt(const glm::vec3 &point, const glm::vec3 &direction) const;
        void push(const glm::vec3 &point, const glm::vec3 &impulse);
    };

    // Where a wheel stands in this frame and what holds it there.
    struct Contact {
        bool touching = false;
        float steer = 0.f;       // radians, + is to the left
        Vehicle::WheelDrive drive;
        glm::vec3 point{0.f};    // the bottom of the wheel: the chassis is pushed here
        glm::vec3 normal{0.f};   // of the ground
        glm::vec3 roll{0.f};     // the rolling direction on the ground
        glm::vec3 side{0.f};     // the axle on the ground, to the right
        float compression = 0.f; // of the spring, m; above suspensionTravel — on the bump stop
        float lean = 1.f;        // cosine between the ground normal and the suspension, limited from below
        float friction = 0.f;    // of the ground surface
        EntityHandle ground;     // what the wheel stands on
        glm::vec3 groundPoint{0.f};
        glm::vec3 groundVelocity{0.f};
        // The slip of the wheel bottom over the ground before the solver: (roll, side).
        glm::vec2 slide{0.f}; // direction
        float slideSpeed = 0.f;
        float slideInverseMass = 0.f;
        // Inverse masses of the chassis at the point.
        float inverseMassNormal = 0.f;
        float inverseMassRoll = 0.f;
        float inverseMassSide = 0.f;
        // Forces, N: they act over the next physics step.
        float spring = 0.f;   // along the normal
        float holdRoll = 0.f; // the share of the weight along a slope that the tire holds
        float holdSide = 0.f;
        // Impulses of the solver, N·s: summed over its passes.
        float normalImpulse = 0.f; // the damper
        float rollImpulse = 0.f;   // the tire
        float sideImpulse = 0.f;
        bool locked = false; // the brake holds the wheel from turning, and it slides
    };

    void findContact(const Body &body, const Wheel &wheel, Contact &contact) const;
    void solveContact(Body &body, Contact &contact, float stepTime) const;
    void drawWheel(Wheel &wheel, const Contact &contact, float stepTime);
    void placeAt(const glm::vec3 &position, const glm::quat &rotation);

    Wheel m_wheels[4];
    bool m_ready = false;
    float m_wheelbase = 0.f;
    float m_track = 0.f;
    Vehicle::SteeringWheel m_steering;
    glm::vec3 m_spawnPosition{0.f};
    glm::quat m_spawnRotation{1.f, 0.f, 0.f, 0.f};
};

REGISTER_SCRIPT(RaycastCar)
