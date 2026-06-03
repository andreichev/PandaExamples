#include "HelloUIDemo.hpp"

#include "UI/MainWindowView.hpp"
#include "UI/SecondaryWindowView.hpp"

#include <Bamboo/Logger.hpp>

void HelloUIDemo::start() {
    buildMainWindow();
    LOG_INFO("HelloUI: PandaUI demo started");
}

void HelloUIDemo::shutdown() {
    for (PandaUI::Window &window : m_demoWindows) {
        if (window.isValid()) { window.close(); }
    }
    m_demoWindows.clear();
    if (m_mainWindow.isValid()) { m_mainWindow.setRootView(nullptr); }
    m_mainView.reset();
}

void HelloUIDemo::buildMainWindow() {
    m_mainWindow = PandaUI::Window::main();
    if (!m_mainWindow.isValid()) {
        LOG_ERROR("HelloUI: main PandaUI window is unavailable");
        return;
    }

    m_mainView = std::make_shared<HelloUI::MainWindowView>(
        [this]() { incrementClickCount(); },
        [this]() { openDemoWindow(); }
    );
    m_mainWindow.setRootView(m_mainView);
}

void HelloUIDemo::openDemoWindow() {
    PandaUI::Window window = PandaUI::Window::create({
        "HelloUI Demo Window",
        120.f,
        120.f,
        620.f,
        420.f,
    });
    if (!window.isValid()) {
        LOG_ERROR("HelloUI: failed to create PandaUI demo window");
        return;
    }

    auto root = std::make_shared<HelloUI::SecondaryWindowView>([window]() mutable {
        window.close();
    });
    window.setRootView(root);
    m_demoWindows.push_back(window);
}

void HelloUIDemo::incrementClickCount() {
    ++m_clickCount;
    if (m_mainView) { m_mainView->setClickCount(m_clickCount); }
}
