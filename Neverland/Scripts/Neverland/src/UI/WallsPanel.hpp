#pragma once

#include "ArchUICommon.hpp"

// Раздел «Стены»: тип стены = материал полотна, толщина; примыкания автоматические.
class WallsPanel final : public ArchPanel {
public:
    explicit WallsPanel(const ArchUIContext &context);
    void refresh() override;
    bool hasOwnScrolling() const override {
        return true;
    }

private:
    std::shared_ptr<MaterialGridView> m_grid;
    std::vector<std::shared_ptr<PresetRow>> m_rows;
};
