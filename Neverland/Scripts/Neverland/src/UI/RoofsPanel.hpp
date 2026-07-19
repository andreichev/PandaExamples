#pragma once

#include "ArchUICommon.hpp"

// Раздел «Крыши»: область ячеек Roof — конёк вдоль длинной оси, скаты, фронтоны.
class RoofsPanel final : public ArchPanel {
public:
    explicit RoofsPanel(const ArchUIContext &context);
    void refresh() override;

private:
    std::vector<std::shared_ptr<PresetRow>> m_rows;
};
