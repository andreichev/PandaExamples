#include "ClawnDashDemo.hpp"

#include <Bamboo/Components/SpriteRendererComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Input.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/WorldAPI.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

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
        setupHud();
    } else {
        LOG_ERROR("ClawnDash: main PandaUI window is unavailable");
    }

    buildLevel();
    showMainMenu();
    LOG_INFO("ClawnDash: started");
}

void ClawnDashDemo::update(float deltaTime) {
    const float dt = std::min(std::max(deltaTime, 0.f), MAX_DELTA_TIME);

    if (readMenuPressed()) {
        showLevelSelect();
        return;
    }

    if (m_state != State::MainMenu && m_state != State::LevelSelect && readRestartPressed()) {
        resetGame();
        return;
    }

    switch (m_state) {
        case State::MainMenu:
        case State::LevelSelect:
            readJumpPressed();
            break;
        case State::Playing:
            updatePlaying(dt);
            break;
        case State::Crashed:
            if (readJumpPressed()) {
                restartLevel();
            }
            break;
        case State::Finished:
            if (readJumpPressed()) {
                if (m_currentLevelIndex + 1 < m_levels.size()) {
                    startNextLevel();
                } else {
                    restartLevel();
                }
            }
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
    createLevels();
    loadSharedEntities();
}

void ClawnDashDemo::setupHud() {
    m_hudView = std::make_shared<ClawnDash::DashHudView>();
    m_hudView->setActions(
        [this](std::size_t levelIndex) {
            startLevel(levelIndex);
        },
        [this]() {
            showMainMenu();
        },
        [this]() {
            showLevelSelect();
        },
        [this]() {
            restartLevel();
        },
        [this]() {
            startNextLevel();
        }
    );
    m_window.setRootView(m_hudView);
}

void ClawnDashDemo::createLevels() {
    m_levels = {
        {
            .title = "First Contact",
            .description = "Reaction jumps with long recovery windows.",
            .hazardTag = "Level 1 Hazard",
            .finishTag = "Level 1 Finish",
            .startX = -4.f,
            .startY = -2.41f,
            .backgroundColor = 0x090E20FF,
            .groundColor = 0x202B3EFF,
            .groundGlowColor = 0x5EEAD4AA,
            .accentColor = 0xFFD166FF,
        },
        {
            .title = "Double Beats",
            .description = "Denser pairs and short hold timings.",
            .hazardTag = "Level 2 Hazard",
            .finishTag = "Level 2 Finish",
            .startX = 150.f,
            .startY = -2.41f,
            .backgroundColor = 0x120D27FF,
            .groundColor = 0x2B2147FF,
            .groundGlowColor = 0xB56CFFFF,
            .accentColor = 0xB56CFFFF,
        },
        {
            .title = "Memory Line",
            .description = "Readable patterns, tighter timing, no speed tricks.",
            .hazardTag = "Level 3 Hazard",
            .finishTag = "Level 3 Finish",
            .startX = 300.f,
            .startY = -2.41f,
            .backgroundColor = 0x071B22FF,
            .groundColor = 0x153B42FF,
            .groundGlowColor = 0x7CFFB2AA,
            .accentColor = 0x7CFFB2FF,
        },
    };
}

void ClawnDashDemo::loadSharedEntities() {
    m_player = WorldAPI::findByTag("Player");
    if (!m_player.isValid()) {
        LOG_ERROR("ClawnDash: Player entity not found");
    }

    m_ground = WorldAPI::findByTag("Ground");
    if (m_ground.isValid()) {
        const Vec3 groundPosition = TransformComponentAPI::getPosition(m_ground);
        const Vec3 groundScale = TransformComponentAPI::getScale(m_ground);
        m_groundY = groundPosition.y + std::abs(groundScale.y) * 0.5f;
    } else {
        LOG_ERROR("ClawnDash: Ground entity not found");
    }

    m_groundGlow = WorldAPI::findByTag("Ground Glow");
    m_background = WorldAPI::findByTag("Level Background");
}

bool ClawnDashDemo::loadLevelEntities(std::size_t levelIndex) {
    if (levelIndex >= m_levels.size()) {
        return false;
    }

    const ClawnDash::LevelInfo &level = m_levels[levelIndex];
    m_startX = level.startX;
    m_startY = level.startY;

    m_finish = WorldAPI::findByTag(level.finishTag.c_str());
    if (!m_finish.isValid()) {
        LOG_ERROR("ClawnDash: finish entity not found");
        return false;
    }
    m_finishX = TransformComponentAPI::getPosition(m_finish).x;

    m_obstacles.clear();
    for (EntityHandle hazard : WorldAPI::findAllByTag(level.hazardTag.c_str())) {
        m_obstacles.push_back({hazard, readBounds(hazard, 0.78f)});
    }
    if (m_obstacles.empty()) {
        LOG_ERROR("ClawnDash: level has no hazards");
    }
    return true;
}

void ClawnDashDemo::showMainMenu() {
    m_state = State::MainMenu;
    if (m_hudView) {
        m_hudView->showMainMenu(m_levels);
    }
}

void ClawnDashDemo::showLevelSelect() {
    m_state = State::LevelSelect;
    if (m_hudView) {
        m_hudView->showLevelSelect(m_levels);
    }
}

void ClawnDashDemo::startLevel(std::size_t levelIndex) {
    if (!loadLevelEntities(levelIndex)) {
        return;
    }
    m_currentLevelIndex = levelIndex;
    applyLevelTheme();
    resetGame();
    m_jumpWasDown = readJumpDown();
}

void ClawnDashDemo::restartLevel() {
    startLevel(m_currentLevelIndex);
}

void ClawnDashDemo::startNextLevel() {
    if (m_currentLevelIndex + 1 < m_levels.size()) {
        startLevel(m_currentLevelIndex + 1);
    } else {
        showLevelSelect();
    }
}

void ClawnDashDemo::applyLevelTheme() {
    const ClawnDash::LevelInfo &level = m_levels[m_currentLevelIndex];
    if (m_background.isValid()) {
        SpriteRendererComponentAPI::setColor(m_background, color(level.backgroundColor));
    }
    if (m_ground.isValid()) {
        SpriteRendererComponentAPI::setColor(m_ground, color(level.groundColor));
    }
    if (m_groundGlow.isValid()) {
        SpriteRendererComponentAPI::setColor(m_groundGlow, color(level.groundGlowColor));
    }
    if (m_finish.isValid()) {
        SpriteRendererComponentAPI::setColor(m_finish, color(0x7CFFB2FF));
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
    if (m_hudView) {
        m_hudView->showGameplay(m_levels[m_currentLevelIndex].title);
    }
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
    if (!m_player.isValid() || m_state == State::MainMenu || m_state == State::LevelSelect) {
        return;
    }
    TransformComponentAPI::setPosition(
        getEntity(), Vec3(m_playerX + CAMERA_OFFSET_X, CAMERA_Y, CAMERA_Z)
    );
}

void ClawnDashDemo::updateHud() {
    if (!m_hudView || m_state != State::Playing) {
        return;
    }
    const float levelLength = std::max(m_finishX - m_startX, 1.f);
    m_hudView->setProgress(std::clamp((m_playerX - m_startX) / levelLength, 0.f, 1.f));
    m_hudView->setStatus("Space / Up / tap to jump", uiColor(m_levels[m_currentLevelIndex].accentColor));
}

void ClawnDashDemo::crash() {
    m_state = State::Crashed;
    SpriteRendererComponentAPI::setColor(m_player, color(0xFF5C8AFF));
    applyPlayerTransform();
    if (m_hudView) {
        m_hudView->showCrashed(m_levels[m_currentLevelIndex].title);
    }
}

void ClawnDashDemo::finish() {
    m_state = State::Finished;
    SpriteRendererComponentAPI::setColor(m_player, color(0x7CFFB2FF));
    applyPlayerTransform();
    if (m_hudView) {
        m_hudView->showFinished(
            m_levels[m_currentLevelIndex].title, m_currentLevelIndex + 1 < m_levels.size()
        );
    }
}

bool ClawnDashDemo::readJumpPressed() {
    const bool jumpDown = readJumpDown();
    const bool pressed = jumpDown && !m_jumpWasDown;
    m_jumpWasDown = jumpDown;
    return pressed;
}

bool ClawnDashDemo::readJumpDown() const {
    return Input::isKeyPressed(Key::SPACE) || Input::isKeyPressed(Key::UP) ||
           Input::isMouseButtonPressed(MouseButton::LEFT) || Input::touchCount() > 0;
}

bool ClawnDashDemo::readRestartPressed() const {
    return Input::isKeyJustPressed(Key::R);
}

bool ClawnDashDemo::readMenuPressed() const {
    return Input::isKeyJustPressed(Key::ESCAPE);
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
