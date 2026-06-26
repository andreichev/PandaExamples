#pragma once

#include <glm/glm.hpp>

struct VoxelCharacterConfig {
    float radius = 0.3f;
    float height = 1.8f;
    float eyeHeight = 1.62f;
    float stepHeight = 1.05f;
    float gravity = 28.0f;
    float jumpSpeed = 8.5f;
    float maxFallSpeed = 48.0f;
    float groundSnapDistance = 0.12f;
    float coyoteTime = 0.08f;
    float maxSubstep = 0.02f;
    float maxFrameTime = 0.1f;
};

struct VoxelCharacterInput {
    glm::vec3 horizontalDirection = glm::vec3(0.0f);
    float horizontalSpeed = 0.0f;
    float flyVerticalInput = 0.0f;
    bool jumpPressed = false;
    bool flyMode = false;
};

struct VoxelCharacterState {
    float verticalVelocity = 0.0f;
    float coyoteTimer = 0.0f;
    bool grounded = false;
};

class VoxelCharacterController final {
public:
    static void move(
        glm::vec3 &eyePosition,
        VoxelCharacterState &state,
        const VoxelCharacterConfig &config,
        const VoxelCharacterInput &input,
        float deltaTime
    );

private:
    static bool moveHorizontalWithStep(
        glm::vec3 &eyePosition,
        VoxelCharacterState &state,
        const VoxelCharacterConfig &config,
        const glm::vec3 &translation
    );
    static bool moveAxis(
        glm::vec3 &eyePosition,
        const VoxelCharacterConfig &config,
        int axis,
        float translation,
        bool &hitNegative,
        bool &hitPositive
    );
    static bool snapToGround(
        glm::vec3 &eyePosition, const VoxelCharacterConfig &config, float maxDistance
    );
};
