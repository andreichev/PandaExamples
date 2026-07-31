#pragma once

#include "ArchUICommon.hpp"

// Раздел модельных блоков одной категории (Fences/Windows/Doors/Forms/Decor).
// Ограды/панели (connected) сами соединяются с соседями; прочее повернётся к игроку.
class ModelsPanel final : public ArchPanel {
public:
    ModelsPanel(const ArchUIContext &context, uint8_t category, const char *note);
    void refresh() override;

private:
    ArchUIContext m_context;
    std::vector<std::pair<uint8_t, std::shared_ptr<BlockCardButton>>> m_modelCards;
};
