#pragma once

#include "Model/GameBrush.hpp"

#include <PandaUI/PandaUI.hpp>

#include <array>
#include <functional>
#include <memory>

class BrushPanelButton;
class TerrainMaterialButton;

// Инструмент рельефа (PandaUI): выбор материала терраформинга (отдельно от строительных
// блоков — диздок), режима кисти (Sphere/Raise/Flatten) и размера. Виджет с колбеками,
// без знания о BlocksCreation — кандидат на перенос в редактор Panda.
class TerrainBrushPanel final : public PandaUI::Panel {
public:
    TerrainBrushPanel(
        std::function<void(VoxelType)> onMaterialSelected,
        std::function<void(GameBrushMode)> onModeChanged,
        std::function<void(int)> onSizeChanged,
        const std::array<PandaUI::TextureHandle, 4> &materialPreviews
    );

    // Синк с внешним стейтом (клавиши B и [ ] тоже меняют кисть — панель отображает).
    // terraformActive — выбран природный материал (иначе подсветка материалов гаснет).
    void setState(GameBrushMode mode, int size, int sizeCount, VoxelType material, bool terraformActive);

private:
    void applySelection();

    std::function<void(VoxelType)> m_onMaterialSelected;
    std::function<void(GameBrushMode)> m_onModeChanged;
    std::function<void(int)> m_onSizeChanged;
    std::array<std::shared_ptr<TerrainMaterialButton>, 4> m_materialButtons;
    std::array<std::shared_ptr<BrushPanelButton>, 3> m_modeButtons;
    std::shared_ptr<PandaUI::Label> m_sizeLabel;
    GameBrushMode m_mode = GameBrushMode::Sphere;
    int m_size = 0;
    int m_sizeCount = 1;
    VoxelType m_material = VoxelType::NOTHING;
    bool m_terraformActive = false;
};
