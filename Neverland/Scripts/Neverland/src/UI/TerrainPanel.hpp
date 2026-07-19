#pragma once

#include "ArchUICommon.hpp"

// Раздел «Рельеф»: материалы лепки. Отдельно от строительства (диздок).
class TerrainPanel final : public ArchPanel {
public:
    explicit TerrainPanel(const ArchUIContext &context);
};
