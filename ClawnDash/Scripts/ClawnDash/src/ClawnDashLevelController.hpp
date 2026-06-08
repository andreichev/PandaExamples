#pragma once

#include "Model/DashTypes.hpp"
#include "UI/GameplayHudView.hpp"

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <PandaUI/PandaUI.hpp>

#include <cstdint>
#include <memory>
#include <vector>

class ClawnDashLevelController final : public Bamboo::Script {
public:
    void start() override;
    void update(float deltaTime) override;
    void shutdown() override;

    Bamboo::WorldHandle menuWorld;
    Bamboo::WorldHandle nextLevel;
    int levelNumber = 1;

    PANDA_FIELDS_BEGIN(ClawnDashLevelController)
    PANDA_FIELD(menuWorld)
    PANDA_FIELD(nextLevel)
    PANDA_FIELD(levelNumber)
    PANDA_FIELDS_END

private:
    enum class State {
        Playing,
        Crashed,
        Finished,
    };

    Bamboo::EntityHandle m_player;
    Bamboo::EntityHandle m_ground;
    Bamboo::EntityHandle m_groundGlow;
    Bamboo::EntityHandle m_background;
    Bamboo::EntityHandle m_finish;
    PandaUI::Window m_window;
    std::shared_ptr<ClawnDash::GameplayHudView> m_hudView;
    std::vector<ClawnDash::Obstacle> m_obstacles;
    std::vector<ClawnDash::Trigger> m_jumpPads;
    std::vector<ClawnDash::Trigger> m_jumpOrbs;
    float m_startX = 0.f;
    float m_startY = 0.f;
    float m_finishX = 0.f;
    float m_groundY = -2.8f;
    float m_playerX = 0.f;
    float m_playerY = 0.f;
    float m_playerVelocityY = 0.f;
    float m_playerRotation = 0.f;
    bool m_grounded = false;
    bool m_jumpWasDown = false;
    State m_state = State::Playing;

    void loadEntities();
    void resetGame();
    void updatePlaying(float deltaTime);
    void updateTriggers(const ClawnDash::Rect &player, bool jumpPressed);
    void updateCamera();
    void updateHud();
    void crash();
    void finish();
    void openMenu();
    void loadNextLevel();
    const char *levelTitle() const;
    uint32_t accentColor() const;
    bool readJumpPressed();
    bool readJumpDown() const;
    bool readRestartPressed() const;
    bool readMenuPressed() const;
    void applyPlayerTransform();
    void restoreTriggerColors();
    ClawnDash::Rect readBounds(Bamboo::EntityHandle entity, float inset) const;
    ClawnDash::Rect playerBounds() const;
};

REGISTER_SCRIPT(ClawnDashLevelController)
