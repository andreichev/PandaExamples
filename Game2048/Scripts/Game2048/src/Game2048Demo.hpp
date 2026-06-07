#pragma once

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <PandaUI/PandaUI.hpp>

#include "Model/GameTypes.hpp"

#include <memory>

namespace Game2048 {
class GameRootView;
}

class Game2048Demo final : public Bamboo::Script {
public:
    void start() override;
    void update(float deltaTime) override;
    void shutdown() override;

private:
    void handleKeyboard(float deltaTime);
    bool getPressedDirection(Game2048::Direction &direction) const;
    bool isDirectionPressed(Game2048::Direction direction) const;

    PandaUI::Window m_window;
    std::shared_ptr<Game2048::GameRootView> m_rootView;
    Game2048::Direction m_activeDirection = Game2048::Direction::Up;
    bool m_hasActiveDirection = false;
    bool m_restartWasPressed = false;
    float m_repeatTimer = 0.f;
};

REGISTER_SCRIPT(Game2048Demo)
