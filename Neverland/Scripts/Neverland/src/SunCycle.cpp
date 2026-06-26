#include "SunCycle.hpp"

#include <Bamboo/ApplicationAPI.hpp>
#include <Bamboo/Components/DirectionalLightComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/WorldLightingAPI.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace {

constexpr float PI2 = 6.28318530718f;
constexpr float SUN_DISK_Z = -0.22f;

Quat toBambooQuat(const glm::quat &quat) {
    return Quat(quat.x, quat.y, quat.z, quat.w);
}

float saturate(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float smoothstep(float edge0, float edge1, float value) {
    const float t = saturate((value - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

Color lerp(Color a, Color b, float t) {
    return Color(
        lerp(a.r, b.r, t),
        lerp(a.g, b.g, t),
        lerp(a.b, b.b, t),
        lerp(a.a, b.a, t)
    );
}

glm::quat rotationForLightDirection(const glm::vec3 &sunDirection) {
    const glm::vec3 forward = glm::normalize(-sunDirection);
    const glm::vec3 worldUp =
        std::abs(forward.y) > 0.98f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    return glm::quatLookAt(forward, worldUp);
}

} // namespace

void SunCycle::start() {
    syncLighting();
}

void SunCycle::update(float) {
    syncLighting();
}

void SunCycle::syncLighting() {
    const EntityHandle sun = getEntity();
    if (enabled == 0) {
        DirectionalLightComponentAPI::setEnabled(sun, false);
        return;
    }

    DirectionalLightComponentAPI::setEnabled(sun, true);

    const float safeCycleSeconds = std::max(cycleSeconds, 1.0f);
    const float phase = ApplicationAPI::getTime() / safeCycleSeconds * PI2;
    const float sunAmount = saturate(std::sin(phase) * 0.5f + 0.5f);
    const float dayAmount = smoothstep(0.12f, 0.70f, sunAmount);
    const float duskAmount = std::pow(1.0f - std::abs(sunAmount * 2.0f - 1.0f), 3.0f);

    const glm::vec3 sunDirection = glm::normalize(glm::vec3(std::cos(phase), std::sin(phase), SUN_DISK_Z));
    TransformComponentAPI::setRotation(sun, toBambooQuat(rotationForLightDirection(sunDirection)));

    const Color dayLightColor(1.0f, 0.94f, 0.84f, 1.0f);
    const Color nightLightColor(0.18f, 0.24f, 0.42f, 1.0f);
    const Color duskLightColor(1.0f, 0.52f, 0.24f, 1.0f);
    Color lightColor = lerp(nightLightColor, dayLightColor, dayAmount);
    lightColor = lerp(lightColor, duskLightColor, duskAmount * 0.45f);

    const float sunIntensity = std::clamp(
        lerp(std::max(minSunIntensity, 0.0f), std::max(maxSunIntensity, 0.0f), dayAmount) + duskAmount * 0.08f,
        0.0f,
        std::max(maxSunIntensity, minSunIntensity) + 0.08f
    );
    DirectionalLightComponentAPI::setColor(sun, lightColor);
    DirectionalLightComponentAPI::setIntensity(sun, sunIntensity);

    const Color dayAmbientColor(0.78f, 0.88f, 1.0f, 1.0f);
    const Color nightAmbientColor(0.10f, 0.13f, 0.25f, 1.0f);
    const Color duskAmbientColor(1.0f, 0.48f, 0.24f, 1.0f);
    Color ambientColor = lerp(nightAmbientColor, dayAmbientColor, dayAmount);
    ambientColor = lerp(ambientColor, duskAmbientColor, duskAmount * 0.22f);

    const float ambientIntensity = std::clamp(
        lerp(std::max(minAmbientIntensity, 0.0f), std::max(maxAmbientIntensity, 0.0f), dayAmount),
        0.0f,
        std::max(maxAmbientIntensity, minAmbientIntensity)
    );
    WorldLightingAPI::setAmbientColor(ambientColor);
    WorldLightingAPI::setAmbientIntensity(ambientIntensity);
}
