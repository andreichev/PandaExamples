//
// Created by Admin on 12.02.2022.
//

#include "PlayerController.hpp"
#include "Model/GameContext.hpp"
#include "Model/ChunksStorage.hpp"

#include <Bamboo/Math.hpp>
#include <Bamboo/Input.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/ApplicationAPI.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace {

constexpr float COLLISION_EPSILON = 0.001f;
constexpr float MAX_PHYSICS_SUBSTEP = 0.02f;
constexpr float MAX_FRAME_TIME = 0.1f;

struct Aabb {
    glm::vec3 min;
    glm::vec3 max;
};

Quat toBambooQuat(const glm::quat &quat) {
    return Quat(quat.x, quat.y, quat.z, quat.w);
}

bool isSolidVoxel(int x, int y, int z) {
    if (!ChunksStorage::isWorldCoordInBounds(x, y, z)) { return true; }
    if (GameContext::s_chunkStorage == nullptr) { return false; }

    const Voxel *voxel = GameContext::s_chunkStorage->getVoxel(x, y, z);
    if (voxel == nullptr) {
        // Missing chunk data falls back to deterministic terrain so streaming gaps do not become
        // holes, while unloaded air above terrain stays passable.
        return y <= ChunksStorage::terrainHeight(x, z);
    }
    return !voxel->isAir();
}

template <typename Callback>
bool forEachSolidVoxel(const Aabb &aabb, Callback callback) {
    const int minX = static_cast<int>(std::floor(aabb.min.x + COLLISION_EPSILON));
    const int minY = static_cast<int>(std::floor(aabb.min.y + COLLISION_EPSILON));
    const int minZ = static_cast<int>(std::floor(aabb.min.z + COLLISION_EPSILON));
    const int maxX = static_cast<int>(std::floor(aabb.max.x - COLLISION_EPSILON));
    const int maxY = static_cast<int>(std::floor(aabb.max.y - COLLISION_EPSILON));
    const int maxZ = static_cast<int>(std::floor(aabb.max.z - COLLISION_EPSILON));

    bool found = false;
    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            for (int z = minZ; z <= maxZ; z++) {
                if (!isSolidVoxel(x, y, z)) { continue; }
                found = true;
                callback(x, y, z);
            }
        }
    }
    return found;
}

} // namespace

void PlayerController::start() {
    const int groundHeight = ChunksStorage::terrainHeight(0, 0);
    TransformComponentAPI::setPosition(
        getEntity(), {0.0f, groundHeight + 1.0f + m_eyeHeight + 1.0f, 0.0f}
    );
    TransformComponentAPI::setRotation(
        getEntity(), toBambooQuat(glm::quat(glm::vec3(glm::radians(25.f), 0.f, 0.f)))
    );
    syncAnglesFromRotation(TransformComponentAPI::getRotation(getEntity()));
}

void PlayerController::update(float deltaTime) {
    updateLook();
    updateCharacter(deltaTime);
}

void PlayerController::shutdown() {
    ApplicationAPI::setCursorLocked(false);
}

void PlayerController::updateLook() {
    if (ApplicationAPI::isCursorLocked()) {
        double deltaX = Input::getMouseDeltaX();
        double deltaY = Input::getMouseDeltaY();
        // DeltaX - смещение мыши за реальное время, поэтому умножение на deltaTime не требуется.
        // Действия в реальном мире не нужно умножать на deltaTime, умножать нужно только действия в
        // игровом мире.
        m_pitch += glm::radians(static_cast<float>(-deltaY * m_mouseSpeed));
        m_yaw += glm::radians(static_cast<float>(-deltaX * m_mouseSpeed));
        const float maxPitch = glm::radians(89.0f);
        m_pitch = glm::clamp(m_pitch, -maxPitch, maxPitch);
        syncRotationFromAngles();
    }
    if (Input::isKeyJustPressed(Key::TAB)) {
        ApplicationAPI::setCursorLocked(!ApplicationAPI::isCursorLocked());
    }
}

void PlayerController::updateCharacter(float deltaTime) {
    if (GameContext::s_chunkStorage == nullptr) { return; }

    glm::vec3 inputDirection(0.0f);
    if (Input::isKeyPressed(Key::W)) { inputDirection += getHorizontalForward(); }
    if (Input::isKeyPressed(Key::S)) { inputDirection -= getHorizontalForward(); }
    if (Input::isKeyPressed(Key::A)) { inputDirection -= getHorizontalRight(); }
    if (Input::isKeyPressed(Key::D)) { inputDirection += getHorizontalRight(); }
    if (glm::length(inputDirection) > 0.0001f) { inputDirection = glm::normalize(inputDirection); }

    if (Input::isKeyJustPressed(Key::SPACE) && m_grounded) {
        m_verticalVelocity = m_jumpSpeed;
        m_grounded = false;
    }

    const float clampedDeltaTime = glm::clamp(deltaTime, 0.0f, MAX_FRAME_TIME);
    const int substeps = std::max(1, static_cast<int>(std::ceil(clampedDeltaTime / MAX_PHYSICS_SUBSTEP)));
    const float substepDeltaTime = clampedDeltaTime / static_cast<float>(substeps);
    const float speed = m_moveSpeed *
                        (Input::isKeyPressed(Key::LEFT_SHIFT) ? m_sprintMultiplier : 1.0f);

    glm::vec3 position = getEyePosition();
    for (int step = 0; step < substeps; step++) {
        const glm::vec3 horizontalTranslation = inputDirection * speed * substepDeltaTime;
        moveHorizontalWithStep(position, horizontalTranslation);

        if (m_grounded && m_verticalVelocity < 0.0f) { m_verticalVelocity = 0.0f; }
        m_verticalVelocity -= m_gravity * substepDeltaTime;
        m_verticalVelocity = std::max(m_verticalVelocity, -m_maxFallSpeed);

        bool hitGround = false;
        bool hitCeiling = false;
        const bool verticalHit =
            moveAxis(position, 1, m_verticalVelocity * substepDeltaTime, hitGround, hitCeiling);
        if (verticalHit) {
            if (hitGround) { m_grounded = true; }
            if ((hitGround && m_verticalVelocity < 0.0f) ||
                (hitCeiling && m_verticalVelocity > 0.0f)) {
                m_verticalVelocity = 0.0f;
            }
        } else {
            m_grounded = false;
        }
    }

    setEyePosition(position);
}

void PlayerController::updateVectors() {
    Quat rotation = TransformComponentAPI::getRotation(getEntity());
    glm::quat quat = glm::normalize(glm::quat(rotation.w, rotation.x, rotation.y, rotation.z));
    m_front = glm::normalize(quat * glm::vec3(0.f, 0.f, -1.f));
    m_right = glm::normalize(quat * glm::vec3(1.f, 0.f, 0.f));
    m_up = glm::normalize(quat * glm::vec3(0.f, 1.f, 0.f));
}

glm::vec3 PlayerController::getEyePosition() {
    const Vec3 position = TransformComponentAPI::getPosition(getEntity());
    return glm::vec3(position.x, position.y, position.z);
}

void PlayerController::setEyePosition(const glm::vec3 &position) {
    TransformComponentAPI::setPosition(getEntity(), {position.x, position.y, position.z});
}

glm::vec3 PlayerController::getHorizontalForward() const {
    return glm::normalize(glm::vec3(-std::sin(m_yaw), 0.0f, -std::cos(m_yaw)));
}

glm::vec3 PlayerController::getHorizontalRight() const {
    return glm::normalize(glm::vec3(std::cos(m_yaw), 0.0f, -std::sin(m_yaw)));
}

bool PlayerController::moveHorizontalWithStep(glm::vec3 &position, const glm::vec3 &translation) {
    if (glm::length(translation) < 0.0001f) { return false; }

    const glm::vec3 originalPosition = position;
    bool hitNegative = false;
    bool hitPositive = false;
    bool collided = false;
    collided |= moveAxis(position, 0, translation.x, hitNegative, hitPositive);
    collided |= moveAxis(position, 2, translation.z, hitNegative, hitPositive);
    if (!collided || !m_grounded) { return collided; }

    glm::vec3 steppedPosition = originalPosition;
    bool hitGround = false;
    bool hitCeiling = false;
    if (moveAxis(steppedPosition, 1, m_stepHeight, hitGround, hitCeiling)) { return collided; }

    bool stepCollided = false;
    stepCollided |= moveAxis(steppedPosition, 0, translation.x, hitNegative, hitPositive);
    stepCollided |= moveAxis(steppedPosition, 2, translation.z, hitNegative, hitPositive);
    if (stepCollided) { return collided; }

    hitGround = false;
    hitCeiling = false;
    moveAxis(steppedPosition, 1, -m_stepHeight, hitGround, hitCeiling);
    if (!hitGround) { return collided; }

    position = steppedPosition;
    m_grounded = true;
    m_verticalVelocity = 0.0f;
    return true;
}

bool PlayerController::moveAxis(
    glm::vec3 &position, int axis, float translation, bool &hitNegative, bool &hitPositive
) const {
    hitNegative = false;
    hitPositive = false;
    if (std::abs(translation) < 0.000001f) { return false; }

    position[axis] += translation;
    Aabb aabb;
    aabb.min = glm::vec3(position.x - m_playerRadius, position.y - m_eyeHeight, position.z - m_playerRadius);
    aabb.max = glm::vec3(
        position.x + m_playerRadius,
        position.y - m_eyeHeight + m_playerHeight,
        position.z + m_playerRadius
    );

    const bool collided = forEachSolidVoxel(aabb, [&](int x, int y, int z) {
        if (axis == 0) {
            if (translation > 0.0f) {
                position.x = std::min(position.x, static_cast<float>(x) - m_playerRadius - COLLISION_EPSILON);
                hitPositive = true;
            } else {
                position.x =
                    std::max(position.x, static_cast<float>(x + 1) + m_playerRadius + COLLISION_EPSILON);
                hitNegative = true;
            }
        } else if (axis == 1) {
            if (translation > 0.0f) {
                position.y = std::min(
                    position.y,
                    static_cast<float>(y) - m_playerHeight + m_eyeHeight - COLLISION_EPSILON
                );
                hitPositive = true;
            } else {
                position.y =
                    std::max(position.y, static_cast<float>(y + 1) + m_eyeHeight + COLLISION_EPSILON);
                hitNegative = true;
            }
        } else {
            if (translation > 0.0f) {
                position.z = std::min(position.z, static_cast<float>(z) - m_playerRadius - COLLISION_EPSILON);
                hitPositive = true;
            } else {
                position.z =
                    std::max(position.z, static_cast<float>(z + 1) + m_playerRadius + COLLISION_EPSILON);
                hitNegative = true;
            }
        }
    });
    return collided;
}

void PlayerController::syncRotationFromAngles() {
    TransformComponentAPI::setRotation(
        getEntity(), toBambooQuat(glm::quat(glm::vec3(m_pitch, m_yaw, 0.0f)))
    );
    updateVectors();
}

void PlayerController::syncAnglesFromRotation(const Quat &rotation) {
    glm::quat quat(rotation.w, rotation.x, rotation.y, rotation.z);
    if (glm::length(quat) < 0.0001f) { quat = glm::quat(glm::vec3(0.0f)); }
    quat = glm::normalize(quat);

    const glm::vec3 forward = glm::normalize(quat * glm::vec3(0.0f, 0.0f, -1.0f));
    m_pitch = asinf(glm::clamp(forward.y, -1.0f, 1.0f));
    m_yaw = atan2f(-forward.x, -forward.z);
    syncRotationFromAngles();
}
