#pragma once

#include <Bamboo/Base.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Math shared by the vehicle scripts: Bamboo <-> glm and a few scalar helpers.
// The world is Y up; a vehicle looks along its local -Z, its right side is +X.
namespace Vehicle {

constexpr float KMH_PER_MS = 3.6f;
constexpr float DEGREES_PER_RADIAN = 57.29577951f;

inline glm::vec3 toGlm(Bamboo::Vec3 value) {
    return glm::vec3(value.x, value.y, value.z);
}

inline Bamboo::Vec3 toBamboo(const glm::vec3 &value) {
    return Bamboo::Vec3(value.x, value.y, value.z);
}

// A handle of a missing entity reads as a zero quaternion: it becomes the identity.
inline glm::quat toGlm(Bamboo::Quat value) {
    const glm::quat rotation(value.w, value.x, value.y, value.z);
    if (glm::dot(rotation, rotation) < 1e-8f) { return glm::quat(1.f, 0.f, 0.f, 0.f); }
    return glm::normalize(rotation);
}

inline Bamboo::Quat toBamboo(const glm::quat &value) {
    return Bamboo::Quat(value.x, value.y, value.z, value.w);
}

inline float moveTowards(float current, float target, float maxDelta) {
    if (current < target) { return glm::min(current + maxDelta, target); }
    return glm::max(current - maxDelta, target);
}

// Direction flattened onto the ground plane; `fallback` when it points straight up or down.
inline glm::vec3 flatDirection(const glm::vec3 &direction, const glm::vec3 &fallback) {
    const glm::vec3 flat(direction.x, 0.f, direction.z);
    if (glm::dot(flat, flat) < 1e-4f) { return fallback; }
    return glm::normalize(flat);
}

} // namespace Vehicle
