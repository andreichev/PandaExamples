#pragma once

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>

#include <glm/glm.hpp>

using namespace Bamboo;

// The camera behind the vehicle the player drives (Vehicle::Garage). It stays behind the nose of
// the vehicle, looks at a point above it and steps in front of whatever blocks the view. V passes
// the wheel to the next vehicle of the world. Put it on a root entity with a camera.
class ChaseCamera : public Script {
public:
    EntityHandle target;              // the vehicle the player starts in; in a world without vehicles — what to follow
    float distance = 7.f;             // m behind the vehicle
    float height = 2.6f;              // m above the vehicle
    float lookHeight = 0.8f;          // m above the vehicle center: the point the camera looks at
    float positionSmoothTime = 0.12f; // s: how far the camera lags behind
    float turnSpeed = 4.f;            // 1/s: how fast the camera swings behind a turning vehicle
    int avoidObstacles = 1;           // 1: move in front of whatever stands between the camera and the vehicle

    PANDA_FIELDS_BEGIN(ChaseCamera)
    PANDA_FIELD(target)
    PANDA_FIELD(distance)
    PANDA_FIELD(height)
    PANDA_FIELD(lookHeight)
    PANDA_FIELD(positionSmoothTime)
    PANDA_FIELD(turnSpeed)
    PANDA_FIELD(avoidObstacles)
    PANDA_FIELDS_END

    void start() override;
    void update(float deltaTime) override;
    void lateUpdate(float deltaTime) override;

private:
    glm::vec3 m_heading{0.f, 0.f, -1.f}; // where the followed vehicle looks, on the ground plane
    Vec3 m_velocity;
    uint32_t m_followed = 0; // the vehicle of the previous frame: a new one is approached at once
};

// After the vehicles: the camera reads the poses they end the frame with.
REGISTER_SCRIPT(ChaseCamera, 100)
