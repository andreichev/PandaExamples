#pragma once

#include "ArchUICommon.hpp"

// Раздел «Окна»: модуль линии стен (подоконная стенка, проём с рамой, перемычка).
class WindowsPanel final : public ArchPanel {
public:
    explicit WindowsPanel(const ArchUIContext &context);
    void refresh() override;

private:
    std::vector<std::shared_ptr<PresetRow>> m_rows;
};
