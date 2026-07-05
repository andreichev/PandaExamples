#include "PlayerController.hpp"

#include <Bamboo/CharacterMoverAPI.hpp>
#include <Bamboo/Input.hpp>

#include <cmath>

void PlayerController::update(float deltaTime) {
    // Horizontal input in world XZ plane (W — forward, towards -Z).
    float inputX = 0.f, inputZ = 0.f;
    if (Input::isKeyPressed(Key::W)) { inputZ -= 1.f; }
    if (Input::isKeyPressed(Key::S)) { inputZ += 1.f; }
    if (Input::isKeyPressed(Key::A)) { inputX -= 1.f; }
    if (Input::isKeyPressed(Key::D)) { inputX += 1.f; }
    const float length = std::sqrt(inputX * inputX + inputZ * inputZ);
    if (length > 0.f) {
        inputX /= length;
        inputZ /= length;
    }
    m_velocity.x = inputX * speed;
    m_velocity.z = inputZ * speed;

    m_velocity.y -= gravity * deltaTime;
    if (m_grounded) {
        if (m_velocity.y < 0.f) {
            m_velocity.y = -0.5f; // small downward bias keeps ground contact on slopes
        }
        if (Input::isKeyJustPressed(Key::SPACE)) { m_velocity.y = jumpSpeed; }
    }

    Vec3 step;
    step.x = m_velocity.x * deltaTime;
    step.y = m_velocity.y * deltaTime;
    step.z = m_velocity.z * deltaTime;
    CharacterMoverAPI::MoveResult result = CharacterMoverAPI::move(getEntity(), step, m_velocity);
    m_velocity = result.velocity; // clipped by walls/ground — no re-accumulation into obstacles
    m_grounded = result.grounded;
}
