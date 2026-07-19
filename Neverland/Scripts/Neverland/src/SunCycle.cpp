#include "SunCycle.hpp"

#include "Model/GameContext.hpp"

#include <Bamboo/ApplicationAPI.hpp>
#include <Bamboo/Assets/MaterialAPI.hpp>
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

void SunCycle::syncSky(const glm::vec3 &sunDirection, float dayAmount, float duskAmount) {
    // Дневной фактор гасит солнечный канал воксельного света построек (шейдер blocks_lit)
    // — ночь наступает без ремеша мешей, канал источников (лампы) не тускнеет.
    if (GameContext::s_blocksMaterial.isValid()) {
        MaterialAPI::setFloat(GameContext::s_blocksMaterial, "daylight", dayAmount);
    }
    if (GameContext::s_roofMaterial.isValid()) {
        MaterialAPI::setFloat(GameContext::s_roofMaterial, "daylight", dayAmount);
    }
    // Небо больше не крутит собственный цикл в шейдере — состояние дня пишет скрипт.
    if (!skyMaterial.isValid()) { return; }
    MaterialAPI::setColor(
        skyMaterial, "sunDirectionDay", Color(sunDirection.x, sunDirection.y, sunDirection.z, dayAmount)
    );
    MaterialAPI::setFloat(skyMaterial, "duskAmount", duskAmount);
}

void SunCycle::syncLighting() {
    const EntityHandle sun = getEntity();
    DirectionalLightComponentAPI::setEnabled(sun, true);
    if (enabled == 0) {
        // Цикл выключен — фиксированный день (удобно тестировать; полноценный свет по вокселям — отдельный этап).
        const glm::vec3 sunDirection = glm::normalize(glm::vec3(0.35f, 0.9f, -0.25f));
        TransformComponentAPI::setRotation(sun, toBambooQuat(rotationForLightDirection(sunDirection)));
        DirectionalLightComponentAPI::setColor(sun, Color(1.0f, 0.94f, 0.84f, 1.0f));
        DirectionalLightComponentAPI::setIntensity(sun, std::max(maxSunIntensity, 0.0f));
        WorldLightingAPI::setAmbientColor(Color(0.78f, 0.88f, 1.0f, 1.0f));
        WorldLightingAPI::setAmbientIntensity(std::max(maxAmbientIntensity, 0.0f));
        syncSky(sunDirection, 1.0f, 0.0f);
        return;
    }

    const float safeCycleSeconds = std::max(cycleSeconds, 1.0f);
    const float phase = ApplicationAPI::getTime() / safeCycleSeconds * PI2;
    const float sunAmount = saturate(std::sin(phase) * 0.5f + 0.5f);
    const float dayAmount = smoothstep(0.12f, 0.70f, sunAmount);
    const float duskAmount = std::pow(1.0f - std::abs(sunAmount * 2.0f - 1.0f), 3.0f);

    const glm::vec3 sunDirection = glm::normalize(glm::vec3(std::cos(phase), std::sin(phase), SUN_DISK_Z));
    TransformComponentAPI::setRotation(sun, toBambooQuat(rotationForLightDirection(sunDirection)));
    syncSky(sunDirection, dayAmount, duskAmount);

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
