#pragma once

#include "ArchUICommon.hpp"

// Раздел «Фундаменты»: сплошные формы — куб и балка, из текущего материала.
class FoundationsPanel final : public ArchPanel {
public:
    explicit FoundationsPanel(const ArchUIContext &context);
};
