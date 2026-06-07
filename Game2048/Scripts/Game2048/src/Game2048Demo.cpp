#include "Game2048Demo.hpp"

#include "UI/GameRootView.hpp"

#include <Bamboo/Input.hpp>
#include <Bamboo/Logger.hpp>

namespace {

constexpr float INITIAL_REPEAT_DELAY = 0.18f;
constexpr float REPEAT_INTERVAL = 0.10f;

} // namespace

void Game2048Demo::start() {
    m_window = PandaUI::Window::main();
    if (!m_window.isValid()) {
        LOG_ERROR("Game2048: main PandaUI window is unavailable");
        return;
    }

    m_rootView = std::make_shared<Game2048::GameRootView>();
    m_window.setRootView(m_rootView);
    LOG_INFO("Game2048: started");
}

void Game2048Demo::update(float deltaTime) {
    handleKeyboard(deltaTime);
}

void Game2048Demo::shutdown() {
    if (m_window.isValid()) {
        m_window.setRootView(nullptr);
    }
    m_rootView.reset();
}

void Game2048Demo::handleKeyboard(float deltaTime) {
    if (!m_rootView) {
        return;
    }

    const bool restartPressed = Bamboo::Input::isKeyPressed(Bamboo::Key::R);
    if (restartPressed && !m_restartWasPressed) {
        m_rootView->reset();
        m_hasActiveDirection = false;
        m_repeatTimer = 0.f;
    }
    m_restartWasPressed = restartPressed;

    Game2048::Direction direction;
    if (!getPressedDirection(direction)) {
        m_hasActiveDirection = false;
        m_repeatTimer = 0.f;
        return;
    }

    if (!m_hasActiveDirection || m_activeDirection != direction) {
        m_activeDirection = direction;
        m_hasActiveDirection = true;
        m_repeatTimer = INITIAL_REPEAT_DELAY;
        m_rootView->move(direction);
        return;
    }

    m_repeatTimer -= deltaTime;
    if (m_repeatTimer <= 0.f) {
        m_repeatTimer = REPEAT_INTERVAL;
        m_rootView->move(direction);
    }
}

bool Game2048Demo::getPressedDirection(Game2048::Direction &direction) const {
    if (isDirectionPressed(Game2048::Direction::Up)) {
        direction = Game2048::Direction::Up;
        return true;
    }
    if (isDirectionPressed(Game2048::Direction::Down)) {
        direction = Game2048::Direction::Down;
        return true;
    }
    if (isDirectionPressed(Game2048::Direction::Left)) {
        direction = Game2048::Direction::Left;
        return true;
    }
    if (isDirectionPressed(Game2048::Direction::Right)) {
        direction = Game2048::Direction::Right;
        return true;
    }
    return false;
}

bool Game2048Demo::isDirectionPressed(Game2048::Direction direction) const {
    switch (direction) {
        case Game2048::Direction::Up:
            return Bamboo::Input::isKeyPressed(Bamboo::Key::UP) ||
                   Bamboo::Input::isKeyPressed(Bamboo::Key::W);
        case Game2048::Direction::Down:
            return Bamboo::Input::isKeyPressed(Bamboo::Key::DOWN) ||
                   Bamboo::Input::isKeyPressed(Bamboo::Key::S);
        case Game2048::Direction::Left:
            return Bamboo::Input::isKeyPressed(Bamboo::Key::LEFT) ||
                   Bamboo::Input::isKeyPressed(Bamboo::Key::A);
        case Game2048::Direction::Right:
            return Bamboo::Input::isKeyPressed(Bamboo::Key::RIGHT) ||
                   Bamboo::Input::isKeyPressed(Bamboo::Key::D);
    }
    return false;
}
