#pragma once

#include "ArchUICommon.hpp"

// Раздел «Двери»: модуль линии стен — открытый проём до перемычки, низ проходим.
class DoorsPanel final : public ArchPanel {
public:
    explicit DoorsPanel(const ArchUIContext &context);
    void refresh() override;

private:
    std::vector<std::shared_ptr<PresetRow>> m_rows;
};
