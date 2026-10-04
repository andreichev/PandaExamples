#include "Vehicle/JointCar.hpp"

#include "Vehicle/Garage.hpp"
#include "Vehicle/VehicleMath.hpp"

#include <Bamboo/Components/Rigidbody3DComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Components/WheelJoint3DComponentAPI.hpp>
#include <Bamboo/Logger.hpp>

using namespace Vehicle;

namespace {

constexpr float TWO_PI = 6.28318530718f;
constexpr float FLIP_LIFT = 1.f; // m: how high the car is lifted to be put back on its wheels

} // namespace

void JointCar::start() {
    m_wheels[0] = {wheelFrontLeft, true};
    m_wheels[1] = {wheelFrontRight, true};
    m_wheels[2] = {wheelRearLeft, false};
    m_wheels[3] = {wheelRearRight, false};
    for (Wheel &wheel : m_wheels) {
        if (!wheel.entity.isValid()) {
            LOG_ERROR("JointCar: all four wheel fields must refer to the wheel entities");
            return;
        }
    }
    const EntityHandle chassis = getEntity();
    m_spawnPosition = toGlm(TransformComponentAPI::getPosition(chassis));
    m_spawnRotation = toGlm(TransformComponentAPI::getRotation(chassis));
    // The wheelbase and the track are taken from where the wheels stand: they are children of
    // the chassis, so their positions are in its space.
    const glm::vec3 frontLeft = toGlm(TransformComponentAPI::getPosition(m_wheels[0].entity));
    const glm::vec3 frontRight = toGlm(TransformComponentAPI::getPosition(m_wheels[1].entity));
    const glm::vec3 rearLeft = toGlm(TransformComponentAPI::getPosition(m_wheels[2].entity));
    m_wheelbase = glm::abs(frontLeft.z - rearLeft.z);
    m_track = glm::abs(frontLeft.x - frontRight.x);
    applySuspension();
    Garage::enter(chassis, "Joint car");
    m_ready = true;
}

void JointCar::shutdown() {
    Garage::leave(getEntity());
}

// The joint spring is tuned over the reduced mass of the wheel and the chassis, while a car is
// tuned by how its body rides. Converts the ride frequency and damping of the body (a quarter of
// the chassis per wheel) into the joint values. A zero rideFrequency keeps the values of the
// joint components.
void JointCar::applySuspension() {
    m_appliedFrequency = rideFrequency;
    m_appliedDamping = rideDamping;
    const float sprungMass = Rigidbody3DComponentAPI::getMass(getEntity()) / 4.f;
    if (sprungMass <= 0.f || rideFrequency <= 0.f) { return; }
    const float angularFrequency = TWO_PI * rideFrequency;
    const float stiffness = sprungMass * angularFrequency * angularFrequency; // N/m
    for (Wheel &wheel : m_wheels) {
        const float wheelMass = Rigidbody3DComponentAPI::getMass(wheel.entity);
        if (wheelMass <= 0.f) { continue; }
        const float reducedMass = sprungMass * wheelMass / (sprungMass + wheelMass);
        const float hertz = glm::sqrt(stiffness / reducedMass) / TWO_PI;
        const float dampingRatio = rideDamping * glm::sqrt(sprungMass / reducedMass);
        WheelJoint3DComponentAPI::setSuspension(wheel.entity, hertz, dampingRatio);
    }
}

void JointCar::fixedUpdate(float stepTime) {
    if (!m_ready) { return; }
    const EntityHandle chassis = getEntity();
    const glm::vec3 position = toGlm(TransformComponentAPI::getPosition(chassis));
    const glm::quat rotation = toGlm(TransformComponentAPI::getRotation(chassis));
    const glm::vec3 forward = rotation * glm::vec3(0.f, 0.f, -1.f);

    // A car without a driver stands on the handbrake.
    DriverInput input;
    if (Garage::isActive(chassis)) {
        input = readDriverInput();
    } else {
        input.handbrake = true;
    }

    if (input.respawn) {
        placeAt(m_spawnPosition, m_spawnRotation);
        return;
    }
    if (input.flip) {
        const glm::vec3 heading = flatDirection(forward, glm::vec3(0.f, 0.f, -1.f));
        placeAt(position + glm::vec3(0.f, FLIP_LIFT, 0.f), glm::quatLookAt(heading, glm::vec3(0.f, 1.f, 0.f)));
        return;
    }

    // The ride fields were edited in the inspector.
    if (rideFrequency != m_appliedFrequency || rideDamping != m_appliedDamping) { applySuspension(); }

    const float forwardSpeed = glm::dot(toGlm(Rigidbody3DComponentAPI::getLinearVelocity(chassis)), forward);
    const SteeringSetup steeringSetup{maxSteerAngle, steerGrip, steerRate, m_wheelbase, m_track};
    const SteeringAngles steering = m_steering.turn(steeringSetup, input.steer, glm::abs(forwardSpeed), stepTime);
    WheelJoint3DComponentAPI::setSteeringAngle(m_wheels[0].entity, glm::degrees(steering.left));
    WheelJoint3DComponentAPI::setSteeringAngle(m_wheels[1].entity, glm::degrees(steering.right));

    // The spin motor of the joint is the wheel drive as it is: the target is the spin of a wheel
    // that rolls at the target speed (degrees per second), the limit is the torque.
    const DriveSetup driveSetup{
        driveWheels, motorTorque, maxSpeed, reverseSpeed, brakeTorque, handbrakeTorque, coastTorque
    };
    for (Wheel &wheel : m_wheels) {
        const WheelDrive drive = wheelDrive(driveSetup, input, forwardSpeed, wheel.front);
        const float spin = wheelRadius > 0.f ? drive.speed / wheelRadius * DEGREES_PER_RADIAN : 0.f;
        WheelJoint3DComponentAPI::setMotor(wheel.entity, drive.torque > 0.f, spin, drive.torque);
    }
}

void JointCar::placeAt(const glm::vec3 &position, const glm::quat &rotation) {
    // Setting a transform teleports the body and stops it, and the bodies under the entity with
    // it: the wheels are children of the chassis, so all five bodies move together and the
    // joints survive.
    const EntityHandle chassis = getEntity();
    TransformComponentAPI::setPosition(chassis, toBamboo(position));
    TransformComponentAPI::setRotation(chassis, toBamboo(rotation));
    m_steering.center();
}
