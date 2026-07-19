#pragma once

#include "../Model/GameBrush.hpp"
#include "ArchUICommon.hpp"

// Раздел «Рельеф»: материалы лепки + настройки кисти. Отдельно от строительства (диздок).
class TerrainPanel final : public ArchPanel {
public:
    explicit TerrainPanel(const ArchUIContext &context);
    void refresh() override;

private:
    ArchUIContext m_context;
    std::vector<std::pair<GameBrushMode, std::shared_ptr<SettingsPresetButton>>> m_modeButtons;
    std::vector<std::pair<int, std::shared_ptr<SettingsPresetButton>>> m_sizeButtons;
};
