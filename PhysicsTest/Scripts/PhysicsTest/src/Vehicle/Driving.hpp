#pragma once

#include "Vehicle/DriverInput.hpp"
#include "Vehicle/VehicleMath.hpp"

#include <glm/glm.hpp>

// What the driver asks of the car: the pedals as a drive of every wheel and the steering wheel as
// the angles of the front wheels. Shared by every vehicle model — the models differ in how a wheel
// hangs on the car and grips the ground, not in how the car is driven, so they are compared under
// the same driver.
namespace Vehicle {

constexpr float GRAVITY = 10.f; // m/s²: the physics world default
// Below this speed (m/s) the car counts as standing: the brake pedal turns into reverse, and the
// throttle no longer brakes a car that rolls backwards.
constexpr float STANDING_SPEED = 1.f;
// The rear wheels are unloaded under braking: with more torque they lock first and the car spins.
constexpr float REAR_BRAKE_SHARE = 0.4f;
// The motor torque fades with speed but not below this share: the car still pulls near its top.
constexpr float MIN_TORQUE_SHARE = 0.2f;

// The drive train and the brakes: the fields of a vehicle script, in their units.
struct DriveSetup {
    int driveWheels = 0;         // 0 rear, 1 front, 2 all four
    float motorTorque = 0.f;     // N·m per driven wheel from a standstill, fades towards maxSpeed
    float maxSpeed = 0.f;        // km/h
    float reverseSpeed = 0.f;    // km/h
    float brakeTorque = 0.f;     // N·m per front wheel, the rear wheels get less
    float handbrakeTorque = 0.f; // N·m per rear wheel: locks it
    float coastTorque = 0.f;     // N·m per driven wheel with the pedals released (engine braking)
};

// A wheel is driven like a motor with a speed target and a torque limit: a target with a torque
// drives, a zero target with a torque brakes, no torque lets the wheel roll.
struct WheelDrive {
    float speed = 0.f;  // m/s over the ground the wheel is turned towards, + is forward
    float torque = 0.f; // N·m: the most the wheel is turned with; 0 — it rolls freely
};

enum class Pedal { Coast, Drive, Reverse, Brake };

inline Pedal pedalOf(const DriverInput &input, float forwardSpeed) {
    if (input.throttle > 0.f && input.brake > 0.f) { return Pedal::Brake; }
    if (input.throttle > 0.f) { return forwardSpeed < -STANDING_SPEED ? Pedal::Brake : Pedal::Drive; }
    if (input.brake > 0.f) { return forwardSpeed > STANDING_SPEED ? Pedal::Brake : Pedal::Reverse; }
    return Pedal::Coast;
}

// forwardSpeed — the speed of the car along its nose, m/s.
inline WheelDrive wheelDrive(const DriveSetup &setup, const DriverInput &input, float forwardSpeed, bool front) {
    if (input.handbrake && !front) { return {0.f, setup.handbrakeTorque}; }
    const bool driven = setup.driveWheels == 2 || (setup.driveWheels == 1) == front;
    const Pedal pedal = pedalOf(input, forwardSpeed);
    switch (pedal) {
        case Pedal::Drive:
        case Pedal::Reverse: {
            // The target is the top speed, the limit is the engine torque; reverse turns backwards.
            const float topSpeed = (pedal == Pedal::Reverse ? setup.reverseSpeed : setup.maxSpeed) / KMH_PER_MS;
            if (!driven || topSpeed <= 0.f) { return {}; }
            const float share = glm::clamp(1.f - glm::abs(forwardSpeed) / topSpeed, MIN_TORQUE_SHARE, 1.f);
            return {pedal == Pedal::Drive ? topSpeed : -topSpeed, setup.motorTorque * share};
        }
        case Pedal::Brake:
            return {0.f, setup.brakeTorque * (front ? 1.f : REAR_BRAKE_SHARE)};
        case Pedal::Coast:
            // A zero target with a small torque is engine braking; a wheel without drive rolls freely.
            return {0.f, driven ? setup.coastTorque : 0.f};
    }
    return {};
}

// The steering: the fields of a vehicle script and the car geometry.
struct SteeringSetup {
    float maxSteerAngle = 0.f; // degrees: the full lock at low speed
    float steerGrip = 0.f;     // g: the lateral acceleration the full lock asks for at speed
    float steerRate = 0.f;     // 1 / steerRate — seconds from the center to the full lock
    float wheelbase = 0.f;     // m
    float track = 0.f;         // m
};

// Angles of the front wheels, radians; + is to the left.
struct SteeringAngles {
    float left = 0.f;
    float right = 0.f;
};

// The steering wheel under a keyboard: the keys only know "full left" and "full right", the wheel
// turns in between at a rate.
class SteeringWheel {
public:
    // input: -1 right .. +1 left; speed — of the car, m/s.
    SteeringAngles turn(const SteeringSetup &setup, float input, float speed, float deltaTime) {
        // The wheel returns to the center, and changes sides, twice as fast as it turns in.
        const bool unwinding = input == 0.f || input * m_position < 0.f;
        m_position = moveTowards(m_position, input, setup.steerRate * (unwinding ? 2.f : 1.f) * deltaTime);

        // At speed the full lock is far more than the tires can hold. The lock shrinks so that it
        // asks for the lateral acceleration steerGrip (in g) at most: the turn radius is
        // wheelbase / tan(angle), the acceleration — speed² / radius.
        float lock = setup.maxSteerAngle;
        if (setup.steerGrip > 0.f && speed > STANDING_SPEED) {
            lock = glm::min(
                lock, glm::degrees(glm::atan(setup.wheelbase * setup.steerGrip * GRAVITY / (speed * speed)))
            );
        }
        const float angle = glm::radians(m_position * lock);

        // Ackermann: the inner wheel follows a tighter circle than the outer one.
        SteeringAngles angles{angle, angle};
        if (glm::abs(angle) > 1e-4f && setup.wheelbase > 0.01f) {
            const float radius = setup.wheelbase / glm::tan(glm::abs(angle));
            const float inner = glm::atan(setup.wheelbase / glm::max(radius - setup.track * 0.5f, 0.01f));
            const float outer = glm::atan(setup.wheelbase / (radius + setup.track * 0.5f));
            angles.left = angle > 0.f ? inner : -outer;
            angles.right = angle > 0.f ? outer : -inner;
        }
        return angles;
    }

    void center() {
        m_position = 0.f;
    }

private:
    float m_position = 0.f; // -1 right .. +1 left
};

} // namespace Vehicle
