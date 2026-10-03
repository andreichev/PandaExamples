#include "Vehicle/RaycastCar.hpp"

#include "Vehicle/Garage.hpp"
#include "Vehicle/VehicleMath.hpp"

#include <Bamboo/Components/Rigidbody3DComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/Physics3DAPI.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <cmath>

using namespace Vehicle;

namespace {

constexpr float TWO_PI = 6.28318530718f;
constexpr float FLIP_LIFT = 1.f; // m: how high the car is lifted to be put back on its wheels
// The physics never steps further than this in a frame: below 30 frames per second the simulation
// slows down. What the script counts per step is counted for the step, not for the frame.
constexpr float MAX_STEP_TIME = 1.f / 30.f;
// Passes of the solver over the wheels. An impulse at one wheel changes the velocities at the
// others, so the answer is approached in a few passes.
constexpr int SOLVER_PASSES = 4;
// Cosines of the angle between the ground normal and the suspension. A wheel does not stand on a
// surface steeper than the first; the push of the ground is amplified no further than the second.
constexpr float MIN_GROUND_ALIGNMENT = 0.3f;
constexpr float MIN_LOAD_ALIGNMENT = 0.5f;
constexpr float RAY_SPREAD = 1.0472f; // radians: how far around the rim the outermost rays look, at most
constexpr int MAX_WHEEL_RAYS = 7;
constexpr float DROOP_SPEED = 3.f; // m/s: how fast a drawn wheel drops when the ground falls away
constexpr float SPIN_DECAY = 0.5f; // 1/s: a wheel in the air slows down

// A zero target with a torque: the wheel is braked (the brake, the handbrake, engine braking).
bool isBraked(const WheelDrive &drive) {
    return drive.torque > 0.f && drive.speed == 0.f;
}

} // namespace

glm::vec3 RaycastCar::Body::velocityAt(const glm::vec3 &point) const {
    return linear + glm::cross(angular, point - center);
}

float RaycastCar::Body::inverseMassAt(const glm::vec3 &point, const glm::vec3 &direction) const {
    const glm::vec3 turn = glm::cross(point - center, direction);
    return 1.f / mass + glm::dot(turn, inverseInertia * turn);
}

void RaycastCar::Body::push(const glm::vec3 &point, const glm::vec3 &impulse) {
    const glm::vec3 turn = glm::cross(point - center, impulse);
    linear += impulse / mass;
    angular += inverseInertia * turn;
    linearImpulse += impulse;
    angularImpulse += turn;
}

void RaycastCar::start() {
    m_wheels[0] = {wheelFrontLeft, true};
    m_wheels[1] = {wheelFrontRight, true};
    m_wheels[2] = {wheelRearLeft, false};
    m_wheels[3] = {wheelRearRight, false};
    for (Wheel &wheel : m_wheels) {
        if (!wheel.entity.isValid()) {
            LOG_ERROR("RaycastCar: all four wheel fields must refer to the wheel entities");
            return;
        }
    }
    const EntityHandle chassis = getEntity();
    m_spawnPosition = toGlm(TransformComponentAPI::getPosition(chassis));
    m_spawnRotation = toGlm(TransformComponentAPI::getRotation(chassis));
    // The wheels are children of the chassis: their transforms are already in its space.
    for (Wheel &wheel : m_wheels) {
        wheel.restOffset = toGlm(TransformComponentAPI::getPosition(wheel.entity));
        wheel.restRotation = toGlm(TransformComponentAPI::getRotation(wheel.entity));
    }
    m_wheelbase = glm::abs(m_wheels[0].restOffset.z - m_wheels[2].restOffset.z);
    m_track = glm::abs(m_wheels[0].restOffset.x - m_wheels[1].restOffset.x);
    Garage::enter(chassis, "Raycast car");
    m_ready = true;
}

void RaycastCar::shutdown() {
    Garage::leave(getEntity());
}

void RaycastCar::update(float deltaTime) {
    if (!m_ready || deltaTime <= 0.f) { return; }
    Body body;
    body.entity = getEntity();
    body.position = toGlm(TransformComponentAPI::getPosition(body.entity));
    body.rotation = toGlm(TransformComponentAPI::getRotation(body.entity));
    body.up = body.rotation * glm::vec3(0.f, 1.f, 0.f);
    const glm::vec3 forward = body.rotation * glm::vec3(0.f, 0.f, -1.f);

    // A car without a driver stands on the handbrake.
    DriverInput input;
    if (Garage::isActive(body.entity)) {
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
        placeAt(body.position + glm::vec3(0.f, FLIP_LIFT, 0.f), glm::quatLookAt(heading, glm::vec3(0.f, 1.f, 0.f)));
        return;
    }

    const Rigidbody3DComponentAPI::MassData massData = Rigidbody3DComponentAPI::getMassData(body.entity);
    if (massData.mass <= 0.f || wheelRadius <= 0.f) { return; }
    body.mass = massData.mass;
    body.center = toGlm(massData.center);
    body.inverseInertia = glm::make_mat3(massData.inverseInertia);
    body.linear = toGlm(Rigidbody3DComponentAPI::getLinearVelocity(body.entity));
    body.angular = toGlm(Rigidbody3DComponentAPI::getAngularVelocity(body.entity));

    const float forwardSpeed = glm::dot(body.linear, forward);
    const SteeringSetup steeringSetup{maxSteerAngle, steerGrip, steerRate, m_wheelbase, m_track};
    const SteeringAngles steering = m_steering.turn(steeringSetup, input.steer, glm::abs(forwardSpeed), deltaTime);
    const DriveSetup driveSetup{
        driveWheels, motorTorque, maxSpeed, reverseSpeed, brakeTorque, handbrakeTorque, coastTorque
    };

    // Where the wheels stand and how hard the springs press them to the ground.
    Contact contacts[4];
    float load = 0.f;       // on all the wheels
    float brakedLoad = 0.f; // on the braked ones
    for (int i = 0; i < 4; i++) {
        const Wheel &wheel = m_wheels[i];
        Contact &contact = contacts[i];
        if (wheel.front) { contact.steer = i == 0 ? steering.left : steering.right; }
        contact.drive = wheelDrive(driveSetup, input, forwardSpeed, wheel.front);
        findContact(body, wheel, contact);
        if (!contact.touching) { continue; }
        load += contact.spring;
        if (isBraked(contact.drive)) { brakedLoad += contact.spring; }
    }

    // The forces: the springs carry the car, and on a slope the tires hold the weight that pulls
    // it along the ground — in shares of their loads; across the wheel every tire holds, along it
    // only a braked one and no harder than its brake.
    const glm::vec3 weight(0.f, -body.mass * GRAVITY, 0.f);
    for (Contact &contact : contacts) {
        if (!contact.touching) { continue; }
        const glm::vec3 pull = weight - contact.normal * glm::dot(weight, contact.normal);
        if (load > 0.f) { contact.holdSide = -glm::dot(pull, contact.side) * contact.spring / load; }
        if (isBraked(contact.drive) && brakedLoad > 0.f) {
            const float brakeForce = contact.drive.torque / wheelRadius;
            contact.holdRoll =
                glm::clamp(-glm::dot(pull, contact.roll) * contact.spring / brakedLoad, -brakeForce, brakeForce);
        }
        const glm::vec3 force =
            contact.normal * contact.spring + contact.roll * contact.holdRoll + contact.side * contact.holdSide;
        Rigidbody3DComponentAPI::applyForceAtPoint(body.entity, toBamboo(force), toBamboo(contact.point));
        // What the wheel stands on gets the opposite: a bridge sags under the car. The impulses
        // below are not passed on: they are sized by the mass of the chassis and would throw a
        // light body away.
        if (contact.ground.id != 0) {
            Rigidbody3DComponentAPI::applyForceAtPoint(contact.ground, toBamboo(-force), toBamboo(contact.groundPoint));
        }
    }

    // The impulses: the dampers and the tires.
    const float stepTime = glm::min(deltaTime, MAX_STEP_TIME);
    for (int pass = 0; pass < SOLVER_PASSES; pass++) {
        for (Contact &contact : contacts) {
            if (contact.touching) { solveContact(body, contact, stepTime); }
        }
    }
    Rigidbody3DComponentAPI::applyLinearImpulse(body.entity, toBamboo(body.linearImpulse));
    Rigidbody3DComponentAPI::applyAngularImpulse(body.entity, toBamboo(body.angularImpulse));

    for (int i = 0; i < 4; i++) {
        drawWheel(m_wheels[i], contacts[i], deltaTime);
    }
}

void RaycastCar::findContact(const Body &body, const Wheel &wheel, Contact &contact) const {
    const glm::quat steering = glm::angleAxis(contact.steer, glm::vec3(0.f, 1.f, 0.f));
    const glm::vec3 heading = body.rotation * (steering * glm::vec3(0.f, 0.f, -1.f));
    const glm::vec3 axle = body.rotation * (steering * glm::vec3(1.f, 0.f, 0.f));
    const float travel = glm::max(suspensionTravel, 0.f);
    // The rays go down the suspension. They start above the wheel at its highest, the spring
    // compressed to the bump stop (a ray that starts inside a shape does not see it), and reach
    // the bottom of the wheel at its lowest, the spring released.
    const glm::vec3 top =
        body.position + body.rotation * (wheel.restOffset + glm::vec3(0.f, travel + wheelRadius, 0.f));
    const float reach = travel + 2.f * wheelRadius;

    // A wheel is round: off its center the rim is above the bottom of the wheel, so the same
    // ground there lifts the wheel less. The hit that lifts it the most is what it stands on.
    const int rays = glm::clamp(wheelRays, 1, MAX_WHEEL_RAYS) | 1; // odd: the middle ray is the center
    const float spread = RAY_SPREAD * rays / (rays + 1);
    float nearest = reach; // to the bottom of the wheel
    Physics3DAPI::RaycastHit ground;
    bool found = false;
    for (int i = 0; i < rays; i++) {
        const float angle = rays > 1 ? spread * (2.f * i / (rays - 1) - 1.f) : 0.f;
        const float rise = wheelRadius * (1.f - glm::cos(angle));
        const glm::vec3 origin = top + heading * (wheelRadius * glm::sin(angle));
        Physics3DAPI::RaycastHit hit;
        if (!Physics3DAPI::raycast(toBamboo(origin), toBamboo(-body.up), reach - rise, hit, body.entity)) {
            continue;
        }
        if (hit.distance + rise < nearest) {
            nearest = hit.distance + rise;
            ground = hit;
            found = true;
        }
    }
    if (!found) { return; }
    const glm::vec3 normal = toGlm(ground.normal);
    const float alignment = glm::dot(normal, body.up);
    if (alignment < MIN_GROUND_ALIGNMENT) { return; } // a wall, or the car lies on its side

    contact.touching = true;
    contact.compression = reach - nearest;
    contact.point = top - body.up * nearest;
    contact.normal = normal;
    contact.side = glm::normalize(axle - normal * glm::dot(axle, normal));
    contact.roll = glm::cross(normal, contact.side);
    contact.lean = glm::max(alignment, MIN_LOAD_ALIGNMENT);
    contact.friction = ground.friction;
    contact.ground = ground.entity;
    contact.groundPoint = toGlm(ground.point);
    if (ground.entity.isValid()) {
        contact.groundVelocity = toGlm(Rigidbody3DComponentAPI::getPointVelocity(ground.entity, ground.point));
    }

    // The spring works along the suspension and carries a quarter of the car; the ground pushes
    // along its normal, and the part of that push along the suspension is the spring force.
    const float angularFrequency = TWO_PI * glm::max(rideFrequency, 0.f);
    const float stiffness = body.mass / 4.f * angularFrequency * angularFrequency; // N/m
    contact.spring = stiffness * contact.compression / contact.lean;

    contact.inverseMassNormal = body.inverseMassAt(contact.point, contact.normal);
    contact.inverseMassRoll = body.inverseMassAt(contact.point, contact.roll);
    contact.inverseMassSide = body.inverseMassAt(contact.point, contact.side);
    const glm::vec3 slip = body.velocityAt(contact.point) - contact.groundVelocity;
    const glm::vec2 slide(glm::dot(slip, contact.roll), glm::dot(slip, contact.side));
    contact.slideSpeed = glm::length(slide);
    if (contact.slideSpeed > 1e-4f) {
        contact.slide = slide / contact.slideSpeed;
        contact.slideInverseMass = contact.slide.x * contact.slide.x * contact.inverseMassRoll +
                                   contact.slide.y * contact.slide.y * contact.inverseMassSide;
    } else {
        contact.slideSpeed = 0.f;
    }
}

// One pass of the solver over a wheel. Every impulse is found from the velocity the chassis has at
// the wheel right now, after all the impulses before it, and is limited as a sum over the passes.
void RaycastCar::solveContact(Body &body, Contact &contact, float stepTime) const {
    // The damper, along the ground normal. Its impulse answers to the velocity it leaves, not to
    // the one it meets, so no step is long enough for it to overshoot. Past the travel the
    // suspension is a stop: it takes the whole closing velocity.
    const float damping =
        2.f * glm::max(rideDamping, 0.f) * (body.mass / 4.f) * TWO_PI * glm::max(rideFrequency, 0.f); // N·s/m
    const float resistance = damping / (contact.lean * contact.lean) * stepTime;
    const float parting = glm::dot(body.velocityAt(contact.point) - contact.groundVelocity, contact.normal);
    float change = -(resistance * parting + contact.normalImpulse) / (1.f + resistance * contact.inverseMassNormal);
    if (contact.compression > glm::max(suspensionTravel, 0.f) && parting + change * contact.inverseMassNormal < 0.f) {
        change = -parting / contact.inverseMassNormal;
    }
    // The ground does not pull the wheel down: the damper takes no more than the spring gives.
    const float normalImpulse = glm::max(contact.normalImpulse + change, -contact.spring * stepTime);
    body.push(contact.point, contact.normal * (normalImpulse - contact.normalImpulse));
    contact.normalImpulse = normalImpulse;

    // What presses the tire to the ground over the step is what it can push along the ground with.
    // The grip is the friction of the tire on a surface of friction 1; another surface scales it
    // the way the physics engine mixes the friction of two colliders: by the square root.
    const float pressing = contact.spring * stepTime + contact.normalImpulse;
    const float surface = glm::sqrt(glm::max(contact.friction, 0.f));
    const float limitRoll = glm::max(gripForward, 0.f) * surface * pressing;
    const float limitSide = glm::max(gripSide, 0.f) * surface * pressing;
    const float motor = contact.drive.torque / wheelRadius * stepTime; // the most the drive or the brake gives
    const float holdRoll = contact.holdRoll * stepTime;
    const float holdSide = contact.holdSide * stepTime;

    // The push of the tire along the ground over the step, (roll, side): the hold and the impulse.
    glm::vec2 tire(0.f);
    // A brake stronger than the grip locks the wheel while the grip cannot stop the slide within
    // the step. A locked tire slides and resists its motion over the ground whichever way it
    // goes, with the friction of that direction.
    contact.locked = false;
    float slideLimit = 0.f;
    if (isBraked(contact.drive) && motor > limitRoll && contact.slideSpeed > 0.f && limitRoll > 0.f &&
        limitSide > 0.f) {
        slideLimit = 1.f / glm::length(glm::vec2(contact.slide.x / limitRoll, contact.slide.y / limitSide));
        contact.locked = contact.slideSpeed / contact.slideInverseMass > slideLimit;
    }
    if (contact.locked) {
        tire = -contact.slide * slideLimit;
    } else {
        // A rolling tire. Along the rolling direction the wheel is a motor: it brings the wheel
        // bottom to the target speed with the drive torque at most. Across — it does not let the
        // bottom slide. The grip that the drive or the brake takes is not left for cornering.
        const glm::vec3 slip = body.velocityAt(contact.point) - contact.groundVelocity;
        const float slipRoll = glm::dot(slip, contact.roll);
        const float slipSide = glm::dot(slip, contact.side);
        tire.x = contact.rollImpulse + (contact.drive.speed - slipRoll) / contact.inverseMassRoll + holdRoll;
        tire.x = glm::clamp(glm::clamp(tire.x, -motor, motor), -limitRoll, limitRoll);
        const float used = limitRoll > 0.f ? tire.x / limitRoll : 0.f;
        const float left = limitSide * glm::sqrt(glm::max(1.f - used * used, 0.f));
        tire.y = glm::clamp(contact.sideImpulse - slipSide / contact.inverseMassSide + holdSide, -left, left);
    }
    const float rollImpulse = tire.x - holdRoll;
    const float sideImpulse = tire.y - holdSide;
    body.push(
        contact.point,
        contact.roll * (rollImpulse - contact.rollImpulse) + contact.side * (sideImpulse - contact.sideImpulse)
    );
    contact.rollImpulse = rollImpulse;
    contact.sideImpulse = sideImpulse;
}

void RaycastCar::drawWheel(Wheel &wheel, const Contact &contact, float deltaTime) {
    // The wheel follows the ground up at once and drops at a rate when the ground falls away.
    const float compression =
        contact.touching ? glm::min(contact.compression, glm::max(suspensionTravel, 0.f)) : 0.f;
    wheel.compression = glm::max(compression, wheel.compression - DROOP_SPEED * deltaTime);
    if (contact.touching) {
        wheel.spin = contact.locked ? 0.f : contact.slide.x * contact.slideSpeed / wheelRadius;
    } else {
        wheel.spin *= glm::max(1.f - SPIN_DECAY * deltaTime, 0.f);
    }
    wheel.spinAngle = std::fmod(wheel.spinAngle + wheel.spin * deltaTime, TWO_PI);

    // The axle is the local X of the wheel: rolling forward, to -Z, turns around it backwards.
    const glm::quat steering = glm::angleAxis(contact.steer, glm::vec3(0.f, 1.f, 0.f));
    const glm::quat spin = glm::angleAxis(-wheel.spinAngle, glm::vec3(1.f, 0.f, 0.f));
    TransformComponentAPI::setPosition(
        wheel.entity, toBamboo(wheel.restOffset + glm::vec3(0.f, wheel.compression, 0.f))
    );
    TransformComponentAPI::setRotation(wheel.entity, toBamboo(steering * wheel.restRotation * spin));
}

void RaycastCar::placeAt(const glm::vec3 &position, const glm::quat &rotation) {
    // Setting a transform teleports the body and stops it; the wheels are its children.
    const EntityHandle chassis = getEntity();
    TransformComponentAPI::setPosition(chassis, toBamboo(position));
    TransformComponentAPI::setRotation(chassis, toBamboo(rotation));
    m_steering.center();
}
