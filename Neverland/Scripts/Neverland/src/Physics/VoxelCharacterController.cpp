#include "Physics/VoxelCharacterController.hpp"

#include "Model/ChunksStorage.hpp"
#include "Model/GameContext.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float COLLISION_EPSILON = 0.001f;

struct Aabb {
    glm::vec3 min;
    glm::vec3 max;
};

bool isSolidVoxel(int x, int y, int z) {
    if (!ChunksStorage::isWorldCoordInBounds(x, y, z)) { return true; }
    if (GameContext::s_chunkStorage == nullptr) { return false; }

    const Voxel *voxel = GameContext::s_chunkStorage->getVoxel(x, y, z);
    if (voxel == nullptr) {
        // Missing chunk data falls back to deterministic terrain so streaming gaps do not become
        // holes, while unloaded air above terrain stays passable.
        return y <= ChunksStorage::terrainHeight(x, z);
    }
    return voxel->isSolid();
}

Aabb makeBodyAabb(const glm::vec3 &eyePosition, const VoxelCharacterConfig &config) {
    return Aabb{
        glm::vec3(
            eyePosition.x - config.radius,
            eyePosition.y - config.eyeHeight,
            eyePosition.z - config.radius
        ),
        glm::vec3(
            eyePosition.x + config.radius,
            eyePosition.y - config.eyeHeight + config.height,
            eyePosition.z + config.radius
        )
    };
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

float sanitizePositive(float value, float fallback) {
    return value > 0.0f ? value : fallback;
}

} // namespace

void VoxelCharacterController::move(
    glm::vec3 &eyePosition,
    VoxelCharacterState &state,
    const VoxelCharacterConfig &config,
    const VoxelCharacterInput &input,
    float deltaTime
) {
    if (GameContext::s_chunkStorage == nullptr) { return; }

    const float maxFrameTime = sanitizePositive(config.maxFrameTime, 0.1f);
    const float maxSubstep = sanitizePositive(config.maxSubstep, 0.02f);
    const float clampedDeltaTime = std::clamp(deltaTime, 0.0f, maxFrameTime);
    const int substeps =
        std::max(1, static_cast<int>(std::ceil(clampedDeltaTime / maxSubstep)));
    const float substepDeltaTime = clampedDeltaTime / static_cast<float>(substeps);

    if (input.flyMode) {
        state.grounded = false;
        state.coyoteTimer = 0.0f;
        state.verticalVelocity = 0.0f;
        for (int step = 0; step < substeps; step++) {
            eyePosition += input.horizontalDirection * input.horizontalSpeed * substepDeltaTime;
            eyePosition.y += input.flyVerticalInput * input.horizontalSpeed * substepDeltaTime;
        }
        return;
    }

    bool jumpPending = input.jumpPressed;
    for (int step = 0; step < substeps; step++) {
        const glm::vec3 horizontalTranslation =
            input.horizontalDirection * input.horizontalSpeed * substepDeltaTime;
        moveHorizontalWithStep(eyePosition, state, config, horizontalTranslation);

        if (state.grounded) { state.coyoteTimer = std::max(config.coyoteTime, 0.0f); }
        else {
            state.coyoteTimer = std::max(0.0f, state.coyoteTimer - substepDeltaTime);
        }

        if (jumpPending && state.coyoteTimer > 0.0f) {
            state.verticalVelocity = config.jumpSpeed;
            state.grounded = false;
            state.coyoteTimer = 0.0f;
            jumpPending = false;
        }

        if (state.grounded && state.verticalVelocity < 0.0f) { state.verticalVelocity = 0.0f; }
        state.verticalVelocity -= config.gravity * substepDeltaTime;
        state.verticalVelocity = std::max(state.verticalVelocity, -config.maxFallSpeed);

        bool hitGround = false;
        bool hitCeiling = false;
        const bool verticalHit = moveAxis(
            eyePosition,
            config,
            1,
            state.verticalVelocity * substepDeltaTime,
            hitGround,
            hitCeiling
        );
        if (verticalHit) {
            if (hitGround) {
                state.grounded = true;
                state.coyoteTimer = std::max(config.coyoteTime, 0.0f);
            }
            if ((hitGround && state.verticalVelocity < 0.0f) ||
                (hitCeiling && state.verticalVelocity > 0.0f)) {
                state.verticalVelocity = 0.0f;
            }
        } else {
            state.grounded = false;
        }

        if (!state.grounded && state.verticalVelocity <= 0.0f &&
            snapToGround(eyePosition, config, config.groundSnapDistance)) {
            state.grounded = true;
            state.coyoteTimer = std::max(config.coyoteTime, 0.0f);
            state.verticalVelocity = 0.0f;
        }
    }
}

bool VoxelCharacterController::moveHorizontalWithStep(
    glm::vec3 &eyePosition,
    VoxelCharacterState &state,
    const VoxelCharacterConfig &config,
    const glm::vec3 &translation
) {
    if (glm::length(translation) < 0.0001f) { return false; }

    const glm::vec3 originalPosition = eyePosition;
    bool hitNegative = false;
    bool hitPositive = false;
    bool collided = false;
    collided |= moveAxis(eyePosition, config, 0, translation.x, hitNegative, hitPositive);
    collided |= moveAxis(eyePosition, config, 2, translation.z, hitNegative, hitPositive);
    if (!collided || !state.grounded) { return collided; }

    glm::vec3 steppedPosition = originalPosition;
    bool hitGround = false;
    bool hitCeiling = false;
    if (moveAxis(steppedPosition, config, 1, config.stepHeight, hitGround, hitCeiling)) {
        return collided;
    }

    bool stepCollided = false;
    stepCollided |= moveAxis(steppedPosition, config, 0, translation.x, hitNegative, hitPositive);
    stepCollided |= moveAxis(steppedPosition, config, 2, translation.z, hitNegative, hitPositive);
    if (stepCollided) { return collided; }

    hitGround = false;
    hitCeiling = false;
    moveAxis(steppedPosition, config, 1, -config.stepHeight, hitGround, hitCeiling);
    if (!hitGround) { return collided; }

    eyePosition = steppedPosition;
    state.grounded = true;
    state.verticalVelocity = 0.0f;
    return true;
}

bool VoxelCharacterController::moveAxis(
    glm::vec3 &eyePosition,
    const VoxelCharacterConfig &config,
    int axis,
    float translation,
    bool &hitNegative,
    bool &hitPositive
) {
    hitNegative = false;
    hitPositive = false;
    if (std::abs(translation) < 0.000001f) { return false; }

    eyePosition[axis] += translation;
    const bool collided = forEachSolidVoxel(makeBodyAabb(eyePosition, config), [&](int x, int y, int z) {
        if (axis == 0) {
            if (translation > 0.0f) {
                eyePosition.x =
                    std::min(eyePosition.x, static_cast<float>(x) - config.radius - COLLISION_EPSILON);
                hitPositive = true;
            } else {
                eyePosition.x = std::max(
                    eyePosition.x,
                    static_cast<float>(x + 1) + config.radius + COLLISION_EPSILON
                );
                hitNegative = true;
            }
        } else if (axis == 1) {
            if (translation > 0.0f) {
                eyePosition.y = std::min(
                    eyePosition.y,
                    static_cast<float>(y) - config.height + config.eyeHeight - COLLISION_EPSILON
                );
                hitPositive = true;
            } else {
                eyePosition.y = std::max(
                    eyePosition.y,
                    static_cast<float>(y + 1) + config.eyeHeight + COLLISION_EPSILON
                );
                hitNegative = true;
            }
        } else {
            if (translation > 0.0f) {
                eyePosition.z =
                    std::min(eyePosition.z, static_cast<float>(z) - config.radius - COLLISION_EPSILON);
                hitPositive = true;
            } else {
                eyePosition.z = std::max(
                    eyePosition.z,
                    static_cast<float>(z + 1) + config.radius + COLLISION_EPSILON
                );
                hitNegative = true;
            }
        }
    });
    return collided;
}

bool VoxelCharacterController::snapToGround(
    glm::vec3 &eyePosition, const VoxelCharacterConfig &config, float maxDistance
) {
    if (maxDistance <= 0.0f) { return false; }

    const glm::vec3 originalPosition = eyePosition;
    bool hitGround = false;
    bool hitCeiling = false;
    if (moveAxis(eyePosition, config, 1, -maxDistance, hitGround, hitCeiling) && hitGround) {
        return true;
    }

    eyePosition = originalPosition;
    return false;
}
