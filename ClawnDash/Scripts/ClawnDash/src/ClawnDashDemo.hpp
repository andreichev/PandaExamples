#pragma once

#include "Model/DashTypes.hpp"
#include "UI/DashHudView.hpp"

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <PandaUI/PandaUI.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class ClawnDashDemo final : public Bamboo::Script {
public:
    void start() override;
    void update(float deltaTime) override;
    void shutdown() override;

private:
    enum class State {
        MainMenu,
        LevelSelect,
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
    std::shared_ptr<ClawnDash::DashHudView> m_hudView;
    std::vector<ClawnDash::LevelInfo> m_levels;
    std::vector<ClawnDash::Obstacle> m_obstacles;
    std::size_t m_currentLevelIndex = 0;
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

    void buildLevel();
    void setupHud();
    void createLevels();
    void loadSharedEntities();
    bool loadLevelEntities(std::size_t levelIndex);
    void showMainMenu();
    void showLevelSelect();
    void startLevel(std::size_t levelIndex);
    void restartLevel();
    void startNextLevel();
    void applyLevelTheme();
    void resetGame();
    void updatePlaying(float deltaTime);
    void updateCamera();
    void updateHud();
    void crash();
    void finish();
    bool readJumpPressed();
    bool readJumpDown() const;
    bool readRestartPressed() const;
    bool readMenuPressed() const;
    void applyPlayerTransform();
    ClawnDash::Rect readBounds(Bamboo::EntityHandle entity, float inset) const;
    ClawnDash::Rect playerBounds() const;
};

REGISTER_SCRIPT(ClawnDashDemo)
