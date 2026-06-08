#include "ClawnDashDemo.hpp"

#include <Bamboo/Components/SpriteRendererComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Input.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/WorldAPI.hpp>

#include <algorithm>
#include <cmath>

using namespace Bamboo;

namespace {

constexpr float PLAYER_SIZE = 0.78f;
constexpr float RUN_SPEED = 6.4f;
constexpr float GRAVITY = -27.f;
constexpr float JUMP_VELOCITY = 11.7f;
constexpr float MAX_DELTA_TIME = 1.f / 30.f;
constexpr float CAMERA_OFFSET_X = 4.8f;
constexpr float CAMERA_Y = 0.2f;
constexpr float CAMERA_Z = 10.f;

Color color(uint32_t rgba) {
    const float r = static_cast<float>((rgba >> 24) & 0xff) / 255.f;
    const float g = static_cast<float>((rgba >> 16) & 0xff) / 255.f;
    const float b = static_cast<float>((rgba >> 8) & 0xff) / 255.f;
    const float a = static_cast<float>(rgba & 0xff) / 255.f;
    return Color(r, g, b, a);
}

PandaUI::Color uiColor(uint32_t rgba) {
    return PandaUI::Color(rgba);
}

} // namespace

void ClawnDashDemo::start() {
    m_window = PandaUI::Window::main();
    if (m_window.isValid()) {
        m_hudView = std::make_shared<ClawnDash::DashHudView>();
        m_window.setRootView(m_hudView);
    } else {
        LOG_ERROR("ClawnDash: main PandaUI window is unavailable");
    }

    buildLevel();
    resetGame();
    LOG_INFO("ClawnDash: started");
}

void ClawnDashDemo::update(float deltaTime) {
    const float dt = std::min(std::max(deltaTime, 0.f), MAX_DELTA_TIME);

    if (readRestartPressed()) {
        resetGame();
        return;
    }

    switch (m_state) {
        case State::Playing:
            updatePlaying(dt);
            break;
        case State::Crashed:
        case State::Finished:
            readJumpPressed();
            break;
    }

    updateCamera();
    updateHud();
}

void ClawnDashDemo::shutdown() {
    if (m_window.isValid()) {
        m_window.setRootView(nullptr);
    }
    m_hudView.reset();
}

void ClawnDashDemo::buildLevel() {
    m_obstacles.clear();
    m_player = {};
    loadLevelEntities();
}

void ClawnDashDemo::loadLevelEntities() {
    m_player = WorldAPI::findByTag("Player");
    if (!m_player.isValid()) {
        LOG_ERROR("ClawnDash: Player entity not found");
        return;
    }

    const Vec3 playerPosition = TransformComponentAPI::getPosition(m_player);
    m_startX = playerPosition.x;
    m_startY = playerPosition.y;

    EntityHandle ground = WorldAPI::findByTag("Ground");
    if (ground.isValid()) {
        const Vec3 groundPosition = TransformComponentAPI::getPosition(ground);
        const Vec3 groundScale = TransformComponentAPI::getScale(ground);
        m_groundY = groundPosition.y + std::abs(groundScale.y) * 0.5f;
    } else {
        LOG_ERROR("ClawnDash: Ground entity not found");
    }

    EntityHandle finish = WorldAPI::findByTag("Finish");
    if (!finish.isValid()) {
        LOG_ERROR("ClawnDash: Finish entity not found");
        return;
    }
    m_finishX = TransformComponentAPI::getPosition(finish).x;

    for (EntityHandle hazard : WorldAPI::findAllByTag("Hazard")) {
        m_obstacles.push_back({hazard, readBounds(hazard, 0.78f)});
    }
}

void ClawnDashDemo::resetGame() {
    m_playerX = m_startX;
    m_playerY = m_startY;
    m_playerVelocityY = 0.f;
    m_playerRotation = 0.f;
    m_grounded = true;
    m_jumpWasDown = false;
    m_state = State::Playing;
    applyPlayerTransform();
    updateCamera();
    updateHud();
}

void ClawnDashDemo::updatePlaying(float deltaTime) {
    if (!m_player.isValid()) {
        return;
    }

    if (readJumpPressed() && m_grounded) {
        m_playerVelocityY = JUMP_VELOCITY;
        m_grounded = false;
    }

    m_playerX += RUN_SPEED * deltaTime;
    m_playerVelocityY += GRAVITY * deltaTime;
    m_playerY += m_playerVelocityY * deltaTime;

    const float groundCenterY = m_groundY + PLAYER_SIZE * 0.5f;
    if (m_playerY <= groundCenterY) {
        m_playerY = groundCenterY;
        m_playerVelocityY = 0.f;
        m_grounded = true;
    }

    if (!m_grounded) {
        m_playerRotation -= 520.f * deltaTime;
    } else {
        m_playerRotation = std::round(m_playerRotation / 90.f) * 90.f;
    }

    const ClawnDash::Rect player = playerBounds();
    for (const ClawnDash::Obstacle &obstacle : m_obstacles) {
        if (player.intersects(obstacle.bounds)) {
            crash();
            return;
        }
    }

    if (m_playerX >= m_finishX) {
        finish();
        return;
    }

    applyPlayerTransform();
}

void ClawnDashDemo::updateCamera() {
    if (!m_player.isValid()) {
        return;
    }
    TransformComponentAPI::setPosition(
        getEntity(), Vec3(m_playerX + CAMERA_OFFSET_X, CAMERA_Y, CAMERA_Z)
    );
}

void ClawnDashDemo::updateHud() {
    if (!m_hudView) {
        return;
    }
    const float levelLength = std::max(m_finishX - m_startX, 1.f);
    m_hudView->setProgress(std::clamp((m_playerX - m_startX) / levelLength, 0.f, 1.f));

    switch (m_state) {
        case State::Playing:
            m_hudView->setStatus("Space / Up / tap to jump", uiColor(0xFFD166FF));
            break;
        case State::Crashed:
            m_hudView->setStatus("Crashed. Press R / Space / tap to restart", uiColor(0xFF5C8AFF));
            break;
        case State::Finished:
            m_hudView->setStatus(
                "Level complete. Press R / Space / tap to run again", uiColor(0x7CFFB2FF)
            );
            break;
    }
}

void ClawnDashDemo::crash() {
    m_state = State::Crashed;
    SpriteRendererComponentAPI::setColor(m_player, color(0xFF5C8AFF));
    applyPlayerTransform();
}

void ClawnDashDemo::finish() {
    m_state = State::Finished;
    SpriteRendererComponentAPI::setColor(m_player, color(0x7CFFB2FF));
    applyPlayerTransform();
}

bool ClawnDashDemo::readJumpPressed() {
    const bool jumpDown = Input::isKeyPressed(Key::SPACE) || Input::isKeyPressed(Key::UP) ||
                          Input::isMouseButtonPressed(MouseButton::LEFT) || Input::touchCount() > 0;
    const bool pressed = jumpDown && !m_jumpWasDown;
    m_jumpWasDown = jumpDown;

    if (pressed && m_state != State::Playing) {
        resetGame();
    }
    return pressed;
}

bool ClawnDashDemo::readRestartPressed() const {
    return Input::isKeyJustPressed(Key::R);
}

void ClawnDashDemo::applyPlayerTransform() {
    if (m_player.id == 0) {
        return;
    }
    TransformComponentAPI::setPosition(m_player, Vec3(m_playerX, m_playerY, 0.35f));
    TransformComponentAPI::setRotationEuler(m_player, Vec3(0.f, 0.f, m_playerRotation));
    if (m_state == State::Playing) {
        SpriteRendererComponentAPI::setColor(m_player, color(0x5EEAD4FF));
    }
}

ClawnDash::Rect ClawnDashDemo::readBounds(EntityHandle entity, float inset) const {
    const Vec3 position = TransformComponentAPI::getPosition(entity);
    const Vec3 scale = TransformComponentAPI::getScale(entity);
    return ClawnDash::Rect{
        position.x, position.y, std::abs(scale.x) * inset, std::abs(scale.y) * inset
    };
}

ClawnDash::Rect ClawnDashDemo::playerBounds() const {
    return ClawnDash::Rect{m_playerX, m_playerY, PLAYER_SIZE * 0.72f, PLAYER_SIZE * 0.72f};
}
