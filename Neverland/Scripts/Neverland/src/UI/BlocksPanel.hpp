#pragma once

#include "ArchUICommon.hpp"

// Раздел «Blocks»: кубы из материалов cocricot. Клик — материал + форма-куб.
class BlocksPanel final : public ArchPanel {
public:
    explicit BlocksPanel(const ArchUIContext &context);
};
