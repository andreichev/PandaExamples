#pragma once

#include "Model/DashTypes.hpp"
#include "UI/DashHudView.hpp"

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <PandaUI/PandaUI.hpp>

#include <memory>
#include <vector>

class ClawnDashDemo final : public Bamboo::Script {
public:
    void start() override;
    void update(float deltaTime) override;
    void shutdown() override;

private:
    enum class State {
        Playing,
        Crashed,
        Finished,
    };

    Bamboo::EntityHandle m_player;
    PandaUI::Window m_window;
    std::shared_ptr<ClawnDash::DashHudView> m_hudView;
    std::vector<Bamboo::EntityHandle> m_createdEntities;
    std::vector<ClawnDash::Obstacle> m_obstacles;
    float m_playerX = 0.f;
    float m_playerY = 0.f;
    float m_playerVelocityY = 0.f;
    float m_playerRotation = 0.f;
    bool m_grounded = false;
    bool m_jumpWasDown = false;
    State m_state = State::Playing;

    void buildLevel();
    void resetGame();
    void destroyCreatedEntities();
    void updatePlaying(float deltaTime);
    void updateCamera();
    void updateHud();
    void crash();
    void finish();
    bool readJumpPressed();
    bool readRestartPressed() const;
    void applyPlayerTransform();
    void createGround();
    void createDecorations();
    void createHazards();
    void createFinishGate();
    Bamboo::EntityHandle createSprite(
        const char *tag,
        Bamboo::Vec3 position,
        Bamboo::Vec3 scale,
        Bamboo::Color color,
        float rotationZ = 0.f
    );
    void addHazard(
        float x, float y, float width, float height, Bamboo::Color color, float rotationZ = 0.f
    );
    ClawnDash::Rect playerBounds() const;
};

REGISTER_SCRIPT(ClawnDashDemo)
