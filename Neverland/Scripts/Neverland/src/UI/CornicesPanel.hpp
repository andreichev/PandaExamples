#pragma once

#include "ArchUICommon.hpp"

// Раздел «Карнизы»: пояс на верху стены, соседние ячейки сливаются в непрерывный профиль.
class CornicesPanel final : public ArchPanel {
public:
    explicit CornicesPanel(const ArchUIContext &context);
};
