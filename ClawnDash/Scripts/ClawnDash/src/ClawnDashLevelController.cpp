#include "ClawnDashLevelController.hpp"

#include "UI/ResultOverlayView.hpp"

#include <Bamboo/Components/SpriteRendererComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Input.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/WorldAPI.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

using namespace Bamboo;

namespace {

constexpr float PLAYER_SIZE = 1.0f;
constexpr float RUN_SPEED = 12.0f;
constexpr float SPEED_PORTAL_RUN_SPEED = 20.0f;
constexpr float GRAVITY = -80.f;
constexpr float JUMP_VELOCITY = 20.0f;
constexpr float PAD_VELOCITY = 20.0f;
constexpr float ORB_VELOCITY = 20.0f;
constexpr float MAX_DELTA_TIME = 1.f / 30.f;
constexpr float CAMERA_OFFSET_X = 4.8f;
constexpr float CAMERA_Y = 0.2f;
constexpr float CAMERA_Z = 10.f;
constexpr float TERRAIN_COLLISION_SCALE = 0.86f;
constexpr float GROUND_EPSILON = 0.08f;
constexpr float FALL_KILL_DISTANCE = 6.0f;
constexpr float JUMP_BUFFER_TIME = 0.12f;
constexpr float COYOTE_TIME = 0.08f;

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

void ClawnDashLevelController::start() {
    m_window = PandaUI::Window::main();
    if (!m_window.isValid()) {
        LOG_ERROR("ClawnDashLevelController: main PandaUI window is unavailable");
    }

    loadEntities();
    resetGame();
}

void ClawnDashLevelController::update(float deltaTime) {
    const float dt = std::min(std::max(deltaTime, 0.f), MAX_DELTA_TIME);

    if (readMenuPressed()) {
        openMenu();
        return;
    }

    if (readRestartPressed()) {
        resetGame();
        return;
    }

    switch (m_state) {
        case State::Playing:
            updatePlaying(dt);
            break;
        case State::Crashed:
            if (readJumpPressed()) {
                resetGame();
            }
            break;
        case State::Finished:
            if (readJumpPressed()) {
                if (nextLevel.isValid()) {
                    loadNextLevel();
                } else {
                    resetGame();
                }
            }
            break;
    }

    updateCamera();
    updateHud();
}

void ClawnDashLevelController::shutdown() {
    if (m_window.isValid()) {
        m_window.setRootView(nullptr);
    }
    m_hudView.reset();
}

void ClawnDashLevelController::loadEntities() {
    m_player = WorldAPI::findByTag("Player");
    if (!m_player.isValid()) {
        LOG_ERROR("ClawnDashLevelController: Player entity not found");
        return;
    }

    const Vec3 playerPosition = TransformComponentAPI::getPosition(m_player);
    m_startX = playerPosition.x;
    m_startY = playerPosition.y;

    m_ground = WorldAPI::findByTag("Ground");
    m_solids.clear();
    if (m_ground.isValid()) {
        const Vec3 groundPosition = TransformComponentAPI::getPosition(m_ground);
        const Vec3 groundScale = TransformComponentAPI::getScale(m_ground);
        m_groundY = groundPosition.y + std::abs(groundScale.y) * 0.5f;
        m_solids.push_back({m_ground, readBounds(m_ground, 1.f)});
    } else {
        LOG_ERROR("ClawnDashLevelController: Ground entity not found");
    }

    for (EntityHandle solid : WorldAPI::findAllByTag("Solid")) {
        m_solids.push_back({solid, readBounds(solid, 1.f)});
    }

    m_groundGlow = WorldAPI::findByTag("Ground Glow");
    m_background = WorldAPI::findByTag("Level Background");
    m_ceilingY = -m_groundY;

    m_finish = WorldAPI::findByTag("Finish");
    if (!m_finish.isValid()) {
        LOG_ERROR("ClawnDashLevelController: Finish entity not found");
        return;
    }
    m_finishX = TransformComponentAPI::getPosition(m_finish).x;

    m_obstacles.clear();
    for (EntityHandle hazard : WorldAPI::findAllByTag("Hazard")) {
        m_obstacles.push_back({hazard, readBounds(hazard, 0.78f)});
    }

    m_jumpPads.clear();
    for (EntityHandle pad : WorldAPI::findAllByTag("Jump Pad")) {
        m_jumpPads.push_back({pad, readBounds(pad, 0.95f)});
    }

    m_jumpOrbs.clear();
    for (EntityHandle orb : WorldAPI::findAllByTag("Jump Orb")) {
        m_jumpOrbs.push_back({orb, readBounds(orb, 1.05f)});
    }

    m_speedPortals.clear();
    for (EntityHandle portal : WorldAPI::findAllByTag("Speed Portal")) {
        m_speedPortals.push_back({portal, readBounds(portal, 1.05f)});
    }

    m_normalSpeedPortals.clear();
    for (EntityHandle portal : WorldAPI::findAllByTag("Normal Speed Portal")) {
        m_normalSpeedPortals.push_back({portal, readBounds(portal, 1.05f)});
    }

    m_miniPortals.clear();
    for (EntityHandle portal : WorldAPI::findAllByTag("Mini Portal")) {
        m_miniPortals.push_back({portal, readBounds(portal, 1.05f)});
    }

    m_normalPortals.clear();
    for (EntityHandle portal : WorldAPI::findAllByTag("Normal Portal")) {
        m_normalPortals.push_back({portal, readBounds(portal, 1.05f)});
    }

    m_gravityPortals.clear();
    for (EntityHandle portal : WorldAPI::findAllByTag("Gravity Portal")) {
        m_gravityPortals.push_back({portal, readBounds(portal, 1.05f)});
    }

    m_normalGravityPortals.clear();
    for (EntityHandle portal : WorldAPI::findAllByTag("Normal Gravity Portal")) {
        m_normalGravityPortals.push_back({portal, readBounds(portal, 1.05f)});
    }
}

void ClawnDashLevelController::resetGame() {
    m_runSpeed = RUN_SPEED;
    m_gravitySign = 1.f;
    m_playerScale = 1.f;
    m_playerX = m_startX;
    m_playerY = m_startY;
    m_playerVelocityY = 0.f;
    m_playerRotation = 0.f;
    m_jumpBufferTimer = 0.f;
    m_coyoteTimer = COYOTE_TIME;
    m_grounded = true;
    m_jumpWasDown = readJumpDown();
    m_state = State::Playing;
    restoreTriggerColors();
    restorePortalColors();
    setBackgroundColor(baseBackgroundColor());
    applyPlayerTransform();
    updateCamera();

    if (m_window.isValid()) {
        m_hudView =
            std::make_shared<ClawnDash::GameplayHudView>(levelTitle(), [this]() { openMenu(); });
        m_window.setRootView(m_hudView);
    }
    updateHud();
}

void ClawnDashLevelController::updatePlaying(float deltaTime) {
    if (!m_player.isValid()) {
        return;
    }

    const bool jumpPressed = readJumpPressed();
    if (jumpPressed) {
        m_jumpBufferTimer = JUMP_BUFFER_TIME;
    } else {
        m_jumpBufferTimer = std::max(0.f, m_jumpBufferTimer - deltaTime);
    }

    if (m_grounded) {
        m_coyoteTimer = COYOTE_TIME;
    } else {
        m_coyoteTimer = std::max(0.f, m_coyoteTimer - deltaTime);
    }

    if (m_jumpBufferTimer > 0.f && m_coyoteTimer > 0.f) {
        m_playerVelocityY = JUMP_VELOCITY * m_gravitySign;
        m_grounded = false;
        m_jumpBufferTimer = 0.f;
        m_coyoteTimer = 0.f;
    }

    const float previousX = m_playerX;
    const float previousY = m_playerY;
    m_playerX += m_runSpeed * deltaTime;
    m_playerVelocityY += GRAVITY * m_gravitySign * deltaTime;
    m_playerY += m_playerVelocityY * deltaTime;

    m_grounded = false;
    if (!resolveSolidCollisions(previousX, previousY)) {
        crash();
        return;
    }

    if (m_playerY < m_groundY - FALL_KILL_DISTANCE || m_playerY > m_ceilingY + FALL_KILL_DISTANCE) {
        crash();
        return;
    }

    if (!m_grounded) {
        m_playerRotation -= 520.f * deltaTime * m_gravitySign;
    } else {
        m_playerRotation = std::round(m_playerRotation / 90.f) * 90.f;
    }

    const ClawnDash::Rect player = playerBounds();
    updateTriggers(player, m_jumpBufferTimer > 0.f);
    updatePortals(player);

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

void ClawnDashLevelController::updateTriggers(const ClawnDash::Rect &player, bool jumpRequested) {
    for (ClawnDash::Trigger &pad : m_jumpPads) {
        if (!pad.used && player.intersects(pad.bounds)) {
            pad.used = true;
            m_playerVelocityY = PAD_VELOCITY * m_gravitySign;
            m_grounded = false;
            m_coyoteTimer = 0.f;
            SpriteRendererComponentAPI::setColor(pad.entity, color(0xFFE08AFF));
        }
    }

    for (ClawnDash::Trigger &orb : m_jumpOrbs) {
        if (!orb.used && jumpRequested && player.intersects(orb.bounds)) {
            orb.used = true;
            m_playerVelocityY = ORB_VELOCITY * m_gravitySign;
            m_grounded = false;
            m_jumpBufferTimer = 0.f;
            m_coyoteTimer = 0.f;
            SpriteRendererComponentAPI::setColor(orb.entity, color(0x8FB7FFFF));
        }
    }
}

void ClawnDashLevelController::updatePortals(const ClawnDash::Rect &player) {
    for (ClawnDash::Portal &portal : m_speedPortals) {
        if (!portal.used && player.intersects(portal.bounds)) {
            portal.used = true;
            m_runSpeed = SPEED_PORTAL_RUN_SPEED;
            setBackgroundColor(0x22110BFF);
            SpriteRendererComponentAPI::setColor(portal.entity, color(0xFFB15CFF));
        }
    }

    for (ClawnDash::Portal &portal : m_normalSpeedPortals) {
        if (!portal.used && player.intersects(portal.bounds)) {
            portal.used = true;
            m_runSpeed = RUN_SPEED;
            setBackgroundColor(baseBackgroundColor());
            SpriteRendererComponentAPI::setColor(portal.entity, color(0xBFDBFEFF));
        }
    }

    for (ClawnDash::Portal &portal : m_miniPortals) {
        if (!portal.used && player.intersects(portal.bounds)) {
            portal.used = true;
            m_playerScale = 0.62f;
            SpriteRendererComponentAPI::setColor(portal.entity, color(0xA5F3FCFF));
        }
    }

    for (ClawnDash::Portal &portal : m_normalPortals) {
        if (!portal.used && player.intersects(portal.bounds)) {
            portal.used = true;
            m_playerScale = 1.f;
            SpriteRendererComponentAPI::setColor(portal.entity, color(0xE5E7EBFF));
        }
    }

    for (ClawnDash::Portal &portal : m_gravityPortals) {
        if (!portal.used && player.intersects(portal.bounds)) {
            portal.used = true;
            m_gravitySign = -1.f;
            m_playerVelocityY = std::abs(m_playerVelocityY);
            m_grounded = false;
            m_coyoteTimer = 0.f;
            setBackgroundColor(0x230B1DFF);
            SpriteRendererComponentAPI::setColor(portal.entity, color(0xF9A8D4FF));
        }
    }

    for (ClawnDash::Portal &portal : m_normalGravityPortals) {
        if (!portal.used && player.intersects(portal.bounds)) {
            portal.used = true;
            m_gravitySign = 1.f;
            m_playerVelocityY = -std::abs(m_playerVelocityY);
            m_grounded = false;
            m_coyoteTimer = 0.f;
            setBackgroundColor(baseBackgroundColor());
            SpriteRendererComponentAPI::setColor(portal.entity, color(0x86EFACFF));
        }
    }
}

void ClawnDashLevelController::updateCamera() {
    if (!m_player.isValid()) {
        return;
    }
    TransformComponentAPI::setPosition(
        getEntity(), Vec3(m_playerX + CAMERA_OFFSET_X, CAMERA_Y, CAMERA_Z)
    );
}

void ClawnDashLevelController::updateHud() {
    if (!m_hudView || m_state != State::Playing) {
        return;
    }
    const float levelLength = std::max(m_finishX - m_startX, 1.f);
    m_hudView->setProgress(std::clamp((m_playerX - m_startX) / levelLength, 0.f, 1.f));
    m_hudView->setStatus(
        "Space / Up / tap to jump. Press on blue orbs in air.", uiColor(accentColor())
    );
}

void ClawnDashLevelController::crash() {
    m_state = State::Crashed;
    SpriteRendererComponentAPI::setColor(m_player, color(0xFF5C8AFF));
    applyPlayerTransform();
    if (m_window.isValid()) {
        m_window.setRootView(std::make_shared<ClawnDash::ResultOverlayView>(
            "Crashed",
            "Try the timing again.",
            uiColor(0xFF8FA3FF),
            "Restart Level",
            [this]() { resetGame(); },
            "Main Menu",
            [this]() { openMenu(); }
        ));
    }
}

void ClawnDashLevelController::finish() {
    m_state = State::Finished;
    SpriteRendererComponentAPI::setColor(m_player, color(0x7CFFB2FF));
    applyPlayerTransform();
    if (m_window.isValid()) {
        m_window.setRootView(std::make_shared<ClawnDash::ResultOverlayView>(
            "Level Complete",
            nextLevel.isValid() ? "Go to the next world."
                                : "Last level complete. Play again or return to menu.",
            uiColor(0x7CFFB2FF),
            nextLevel.isValid() ? "Next Level" : "Play Again",
            [this]() {
                if (nextLevel.isValid()) {
                    loadNextLevel();
                } else {
                    resetGame();
                }
            },
            "Main Menu",
            [this]() { openMenu(); }
        ));
    }
}

void ClawnDashLevelController::openMenu() {
    if (!menuWorld.isValid()) {
        LOG_ERROR("ClawnDashLevelController: menu world handle is not configured");
        return;
    }
    WorldAPI::load(menuWorld);
}

void ClawnDashLevelController::loadNextLevel() {
    if (!nextLevel.isValid()) {
        resetGame();
        return;
    }
    WorldAPI::load(nextLevel);
}

const char *ClawnDashLevelController::levelTitle() const {
    switch (levelNumber) {
        case 1:
            return "Foundation";
        case 2:
            return "Pressure";
        default:
            return "Clawn Dash";
    }
}

uint32_t ClawnDashLevelController::accentColor() const {
    switch (levelNumber) {
        case 1:
            return 0xFFD166FF;
        case 2:
            return 0xB56CFFFF;
        default:
            return 0xFFD166FF;
    }
}

uint32_t ClawnDashLevelController::baseBackgroundColor() const {
    switch (levelNumber) {
        case 1:
            return 0x090E20FF;
        case 2:
            return 0x120D27FF;
        default:
            return 0x090E20FF;
    }
}

bool ClawnDashLevelController::readJumpPressed() {
    const bool jumpDown = readJumpDown();
    const bool pressed = jumpDown && !m_jumpWasDown;
    m_jumpWasDown = jumpDown;
    return pressed;
}

bool ClawnDashLevelController::readJumpDown() const {
    return Input::isKeyPressed(Key::SPACE) || Input::isKeyPressed(Key::UP) ||
           Input::isMouseButtonPressed(MouseButton::LEFT) || Input::touchCount() > 0;
}

bool ClawnDashLevelController::readRestartPressed() const {
    return Input::isKeyJustPressed(Key::R);
}

bool ClawnDashLevelController::readMenuPressed() const {
    return Input::isKeyJustPressed(Key::ESCAPE);
}

void ClawnDashLevelController::applyPlayerTransform() {
    if (m_player.id == 0) {
        return;
    }
    TransformComponentAPI::setPosition(m_player, Vec3(m_playerX, m_playerY, 0.35f));
    TransformComponentAPI::setRotationEuler(m_player, Vec3(0.f, 0.f, m_playerRotation));
    TransformComponentAPI::setScale(
        m_player, Vec3(PLAYER_SIZE * m_playerScale, PLAYER_SIZE * m_playerScale, 1.f)
    );
    if (m_state == State::Playing) {
        SpriteRendererComponentAPI::setColor(m_player, color(0x5EEAD4FF));
    }
}

void ClawnDashLevelController::restoreTriggerColors() {
    for (ClawnDash::Trigger &pad : m_jumpPads) {
        pad.used = false;
        SpriteRendererComponentAPI::setColor(pad.entity, color(0xFFD166FF));
    }
    for (ClawnDash::Trigger &orb : m_jumpOrbs) {
        orb.used = false;
        SpriteRendererComponentAPI::setColor(orb.entity, color(0x5CA8FFFF));
    }
}

void ClawnDashLevelController::restorePortalColors() {
    for (ClawnDash::Portal &portal : m_speedPortals) {
        portal.used = false;
        SpriteRendererComponentAPI::setColor(portal.entity, color(0xFF8A5CFF));
    }
    for (ClawnDash::Portal &portal : m_normalSpeedPortals) {
        portal.used = false;
        SpriteRendererComponentAPI::setColor(portal.entity, color(0x93C5FDFF));
    }
    for (ClawnDash::Portal &portal : m_miniPortals) {
        portal.used = false;
        SpriteRendererComponentAPI::setColor(portal.entity, color(0x6EE7F9FF));
    }
    for (ClawnDash::Portal &portal : m_normalPortals) {
        portal.used = false;
        SpriteRendererComponentAPI::setColor(portal.entity, color(0xE5E7EBFF));
    }
    for (ClawnDash::Portal &portal : m_gravityPortals) {
        portal.used = false;
        SpriteRendererComponentAPI::setColor(portal.entity, color(0xF472B6FF));
    }
    for (ClawnDash::Portal &portal : m_normalGravityPortals) {
        portal.used = false;
        SpriteRendererComponentAPI::setColor(portal.entity, color(0x86EFACFF));
    }
}

void ClawnDashLevelController::setBackgroundColor(uint32_t rgba) {
    if (m_background.isValid()) {
        SpriteRendererComponentAPI::setColor(m_background, color(rgba));
    }
}

ClawnDash::Rect ClawnDashLevelController::readBounds(EntityHandle entity, float inset) const {
    const Vec3 position = TransformComponentAPI::getPosition(entity);
    const Vec3 scale = TransformComponentAPI::getScale(entity);
    return ClawnDash::Rect{
        position.x, position.y, std::abs(scale.x) * inset, std::abs(scale.y) * inset
    };
}

ClawnDash::Rect ClawnDashLevelController::playerCollisionBounds() const {
    const float size = PLAYER_SIZE * m_playerScale * TERRAIN_COLLISION_SCALE;
    return ClawnDash::Rect{m_playerX, m_playerY, size, size};
}

ClawnDash::Rect ClawnDashLevelController::playerBounds() const {
    const float size = PLAYER_SIZE * m_playerScale * 0.72f;
    return ClawnDash::Rect{m_playerX, m_playerY, size, size};
}

bool ClawnDashLevelController::resolveSolidCollisions(float previousX, float previousY) {
    const float halfSize = PLAYER_SIZE * m_playerScale * TERRAIN_COLLISION_SCALE * 0.5f;
    const ClawnDash::Rect previousBounds{
        previousX, previousY, halfSize * 2.f, halfSize * 2.f
    };

    for (const ClawnDash::Obstacle &solid : m_solids) {
        const ClawnDash::Rect currentBounds = playerCollisionBounds();
        if (!currentBounds.intersects(solid.bounds)) {
            continue;
        }

        if (m_gravitySign > 0.f) {
            const bool wasAbove = previousBounds.bottom() >= solid.bounds.top() - GROUND_EPSILON;
            if (wasAbove && m_playerVelocityY <= 0.f) {
                m_playerY = solid.bounds.top() + halfSize;
                m_playerVelocityY = 0.f;
                m_grounded = true;
                continue;
            }
        } else {
            const bool wasBelow = previousBounds.top() <= solid.bounds.bottom() + GROUND_EPSILON;
            if (wasBelow && m_playerVelocityY >= 0.f) {
                m_playerY = solid.bounds.bottom() - halfSize;
                m_playerVelocityY = 0.f;
                m_grounded = true;
                continue;
            }
        }

        return false;
    }

    return true;
}
