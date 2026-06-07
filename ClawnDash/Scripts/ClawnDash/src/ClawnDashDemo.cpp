#include "ClawnDashDemo.hpp"

#include <Bamboo/Components/SpriteRendererComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Input.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/WorldAPI.hpp>

#include <algorithm>
#include <array>
#include <cmath>

using namespace Bamboo;

namespace {

constexpr float GROUND_Y = -2.8f;
constexpr float PLAYER_SIZE = 0.78f;
constexpr float START_X = -4.0f;
constexpr float FINISH_X = 112.f;
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
    destroyCreatedEntities();
}

void ClawnDashDemo::buildLevel() {
    destroyCreatedEntities();
    createDecorations();
    createGround();
    createHazards();
    createFinishGate();

    m_player = createSprite(
        "Clawn",
        Vec3(START_X, GROUND_Y + PLAYER_SIZE * 0.5f, 0.25f),
        Vec3(PLAYER_SIZE, PLAYER_SIZE, 1.f),
        color(0x5EEAD4FF)
    );
}

void ClawnDashDemo::resetGame() {
    m_playerX = START_X;
    m_playerY = GROUND_Y + PLAYER_SIZE * 0.5f;
    m_playerVelocityY = 0.f;
    m_playerRotation = 0.f;
    m_grounded = true;
    m_jumpWasDown = false;
    m_state = State::Playing;
    applyPlayerTransform();
    updateCamera();
    updateHud();
}

void ClawnDashDemo::destroyCreatedEntities() {
    for (EntityHandle entity : m_createdEntities) {
        if (entity.id != 0) {
            WorldAPI::destroyEntity(entity);
        }
    }
    m_createdEntities.clear();
    m_obstacles.clear();
    m_player = {};
}

void ClawnDashDemo::updatePlaying(float deltaTime) {
    if (readJumpPressed() && m_grounded) {
        m_playerVelocityY = JUMP_VELOCITY;
        m_grounded = false;
    }

    m_playerX += RUN_SPEED * deltaTime;
    m_playerVelocityY += GRAVITY * deltaTime;
    m_playerY += m_playerVelocityY * deltaTime;

    const float groundCenterY = GROUND_Y + PLAYER_SIZE * 0.5f;
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

    if (m_playerX >= FINISH_X) {
        finish();
        return;
    }

    applyPlayerTransform();
}

void ClawnDashDemo::updateCamera() {
    TransformComponentAPI::setPosition(
        getEntity(), Vec3(m_playerX + CAMERA_OFFSET_X, CAMERA_Y, CAMERA_Z)
    );
}

void ClawnDashDemo::updateHud() {
    if (!m_hudView) {
        return;
    }
    m_hudView->setProgress((m_playerX - START_X) / (FINISH_X - START_X));

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

void ClawnDashDemo::createGround() {
    for (int i = 0; i < 11; ++i) {
        const float x = -6.f + static_cast<float>(i) * 12.f;
        createSprite(
            "Neon Ground",
            Vec3(x, GROUND_Y - 0.2f, 0.05f),
            Vec3(12.2f, 0.4f, 1.f),
            color(0x1F2937FF)
        );
        createSprite(
            "Ground Glow",
            Vec3(x, GROUND_Y + 0.05f, 0.08f),
            Vec3(12.2f, 0.08f, 1.f),
            color(0x22D3EEFF)
        );
    }
}

void ClawnDashDemo::createDecorations() {
    createSprite("Backplate", Vec3(56.f, 0.f, -0.35f), Vec3(132.f, 13.f, 1.f), color(0x0B1020FF));

    for (int i = 0; i < 16; ++i) {
        const float x = -8.f + static_cast<float>(i) * 8.f;
        const float height = 1.4f + static_cast<float>((i * 37) % 5) * 0.65f;
        createSprite(
            "Sky Pulse",
            Vec3(x, -0.4f + height * 0.5f, -0.2f),
            Vec3(1.2f, height, 1.f),
            color(0x172554AA)
        );
    }

    for (int i = 0; i < 18; ++i) {
        const float x = -4.f + static_cast<float>(i) * 6.5f;
        const float y = 2.9f + static_cast<float>((i * 17) % 4) * 0.35f;
        createSprite(
            "Signal Dot", Vec3(x, y, -0.1f), Vec3(0.16f, 0.16f, 1.f), color(0x67E8F9DD), 45.f
        );
    }
}

void ClawnDashDemo::createHazards() {
    const Color spikeColor = color(0xFB7185FF);
    const Color blockColor = color(0xF97316FF);

    const std::array<float, 19> spikes = {
        7.f,  12.5f, 14.0f, 21.f, 28.5f, 30.0f, 37.5f, 43.f, 48.5f,  54.5f,
        61.f, 62.5f, 70.f,  76.f, 82.5f, 84.f,  91.f,  98.f, 104.5f,
    };
    for (float x : spikes) {
        addHazard(x, GROUND_Y + 0.42f, 0.58f, 0.58f, spikeColor, 45.f);
    }

    const std::array<float, 8> blocks = {18.f, 34.f, 52.f, 67.f, 88.f, 95.f, 101.f, 108.f};
    for (float x : blocks) {
        addHazard(x, GROUND_Y + 0.62f, 0.7f, 1.05f, blockColor);
    }
}

void ClawnDashDemo::createFinishGate() {
    createSprite(
        "Finish Gate A",
        Vec3(FINISH_X, GROUND_Y + 1.15f, 0.1f),
        Vec3(0.18f, 2.6f, 1.f),
        color(0x7CFFB2FF)
    );
    createSprite(
        "Finish Gate B",
        Vec3(FINISH_X + 0.55f, GROUND_Y + 1.15f, 0.1f),
        Vec3(0.18f, 2.6f, 1.f),
        color(0x7CFFB2FF)
    );
}

EntityHandle ClawnDashDemo::createSprite(
    const char *tag, Vec3 position, Vec3 scale, Color spriteColor, float rotationZ
) {
    EntityHandle entity = WorldAPI::createEntity(tag);
    EntityAPI::addComponent(entity, ComponentType::SPRITE_RENDERER_COMPONENT);
    TransformComponentAPI::setPosition(entity, position);
    TransformComponentAPI::setScale(entity, scale);
    TransformComponentAPI::setRotationEuler(entity, Vec3(0.f, 0.f, rotationZ));
    SpriteRendererComponentAPI::setColor(entity, spriteColor);
    m_createdEntities.push_back(entity);
    return entity;
}

void ClawnDashDemo::addHazard(
    float x, float y, float width, float height, Color hazardColor, float rotationZ
) {
    EntityHandle entity =
        createSprite("Hazard", Vec3(x, y, 0.2f), Vec3(width, height, 1.f), hazardColor, rotationZ);
    m_obstacles.push_back({entity, ClawnDash::Rect{x, y, width * 0.78f, height * 0.78f}});
}

ClawnDash::Rect ClawnDashDemo::playerBounds() const {
    return ClawnDash::Rect{m_playerX, m_playerY, PLAYER_SIZE * 0.72f, PLAYER_SIZE * 0.72f};
}
