#pragma once

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <PandaUI/PandaUI.hpp>

#include <memory>
#include <vector>

using namespace Bamboo;

class HelloUIDemo : public Script {
public:
    void start() override;
    void shutdown() override;

private:
    std::shared_ptr<PandaUI::Panel> makeCard(const char *title, const char *text);
    std::shared_ptr<PandaUI::Button> makeButton(const char *title, PandaUI::Color color);
    void buildMainWindow();
    void openDemoWindow();

    PandaUI::Window m_mainWindow;
    std::shared_ptr<PandaUI::Label> m_statusLabel;
    std::vector<PandaUI::Window> m_demoWindows;
    int m_clickCount = 0;
};

REGISTER_SCRIPT(HelloUIDemo)
