#pragma once

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <PandaUI/PandaUI.hpp>

#include <memory>
#include <vector>

using namespace Bamboo;

namespace HelloUI {
class MainWindowView;
}

class HelloUIDemo : public Script {
public:
    void start() override;
    void shutdown() override;

private:
    void buildMainWindow();
    void openDemoWindow();
    void incrementClickCount();

    PandaUI::Window m_mainWindow;
    std::shared_ptr<HelloUI::MainWindowView> m_mainView;
    std::vector<PandaUI::Window> m_demoWindows;
    int m_clickCount = 0;
};

REGISTER_SCRIPT(HelloUIDemo)
