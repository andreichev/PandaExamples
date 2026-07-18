#pragma once

#include "Model/GameBrush.hpp"

#include <PandaUI/PandaUI.hpp>

#include <array>
#include <functional>
#include <memory>

class BrushPanelButton;

// Контроллер кисти рельефа (PandaUI): выбор режима (Sphere/Raise/Flatten) и размера.
// Виджет с колбеками, без знания о BlocksCreation — кандидат на перенос в редактор Panda
// как инструмент редактирования terrain.
class TerrainBrushPanel final : public PandaUI::Panel {
public:
    TerrainBrushPanel(
        std::function<void(GameBrushMode)> onModeChanged, std::function<void(int)> onSizeChanged
    );

    // Синк с внешним стейтом (клавиши B и [ ] тоже меняют кисть — панель отображает).
    void setState(GameBrushMode mode, int size, int sizeCount);

private:
    void applySelection();

    std::function<void(GameBrushMode)> m_onModeChanged;
    std::function<void(int)> m_onSizeChanged;
    std::array<std::shared_ptr<BrushPanelButton>, 3> m_modeButtons;
    std::shared_ptr<PandaUI::Label> m_sizeLabel;
    GameBrushMode m_mode = GameBrushMode::Sphere;
    int m_size = 0;
    int m_sizeCount = 1;
};
