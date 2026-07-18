#pragma once

#include "Model/TerrainBrush.hpp"

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
        std::function<void(TerrainBrushMode)> onModeChanged, std::function<void(int)> onSizeChanged
    );

    // Синк с внешним стейтом (клавиши B и [ ] тоже меняют кисть — панель отображает).
    void setState(TerrainBrushMode mode, int size, int sizeCount);

private:
    void applySelection();

    std::function<void(TerrainBrushMode)> m_onModeChanged;
    std::function<void(int)> m_onSizeChanged;
    std::array<std::shared_ptr<BrushPanelButton>, 3> m_modeButtons;
    std::shared_ptr<PandaUI::Label> m_sizeLabel;
    TerrainBrushMode m_mode = TerrainBrushMode::Sphere;
    int m_size = 0;
    int m_sizeCount = 1;
};
