#include "Vehicle/ChaseCamera.hpp"

#include "Vehicle/Garage.hpp"
#include "Vehicle/VehicleMath.hpp"

#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Input.hpp>
#include <Bamboo/Math.hpp>
#include <Bamboo/Physics3DAPI.hpp>

#include <glm/gtc/quaternion.hpp>

using namespace Vehicle;

namespace {

const glm::vec3 UP(0.f, 1.f, 0.f);
// The camera stops this far in front of an obstacle and never comes closer to the vehicle than
// the minimum.
constexpr float OBSTACLE_MARGIN = 0.3f;
constexpr float MIN_DISTANCE = 1.5f;

} // namespace

void ChaseCamera::start() {
    // The vehicles have entered the garage by now: their scripts start before the camera.
    Garage::activate(target);
}

void ChaseCamera::update(float) {
    if (Input::isKeyJustPressed(Key::V)) { Garage::activateNext(); }
}

void ChaseCamera::lateUpdate(float deltaTime) {
    EntityHandle vehicle = Garage::active();
    if (!vehicle.isValid()) { vehicle = target; }
    if (!vehicle.isValid()) { return; }

    const glm::vec3 position = toGlm(TransformComponentAPI::getPosition(vehicle));
    const glm::quat rotation = toGlm(TransformComponentAPI::getRotation(vehicle));
    // A vehicle that stands on its nose or tail has no heading: the last one is kept.
    const glm::vec3 heading = flatDirection(rotation * glm::vec3(0.f, 0.f, -1.f), m_heading);
    const bool snap = m_followed != vehicle.id;
    m_followed = vehicle.id;
    if (snap) {
        m_heading = heading;
    } else {
        const float blend = 1.f - glm::exp(-turnSpeed * deltaTime);
        m_heading = flatDirection(glm::mix(m_heading, heading, blend), heading);
    }

    const glm::vec3 aim = position + UP * lookHeight;
    glm::vec3 desired = position - m_heading * distance + UP * height;
    if (avoidObstacles != 0) {
        const glm::vec3 toCamera = desired - aim;
        const float length = glm::length(toCamera);
        Physics3DAPI::RaycastHit hit;
        if (length > MIN_DISTANCE && Physics3DAPI::raycast(toBamboo(aim), toBamboo(toCamera), length, hit, vehicle)) {
            desired = aim + toCamera / length * glm::max(hit.distance - OBSTACLE_MARGIN, MIN_DISTANCE);
        }
    }

    const EntityHandle camera = getEntity();
    Vec3 cameraPosition = toBamboo(desired);
    if (snap) {
        m_velocity = Vec3();
    } else {
        cameraPosition = Math::smoothDamp(
            TransformComponentAPI::getPosition(camera), cameraPosition, m_velocity, positionSmoothTime, deltaTime
        );
    }
    TransformComponentAPI::setPosition(camera, cameraPosition);

    const glm::vec3 look = aim - toGlm(cameraPosition);
    if (glm::dot(look, look) > 1e-6f) {
        TransformComponentAPI::setRotation(camera, toBamboo(glm::quatLookAt(glm::normalize(look), UP)));
    }
}
