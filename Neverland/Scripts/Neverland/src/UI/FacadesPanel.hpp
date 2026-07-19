#pragma once

#include "ArchUICommon.hpp"

// Раздел «Готовые фасады»: пресеты стилей — игрок берёт фасад и разбирает на элементы.
class FacadesPanel final : public ArchPanel {
public:
    explicit FacadesPanel(const ArchUIContext &context);
};
