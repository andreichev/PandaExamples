#pragma once

#include <PandaUI/PandaUI.hpp>

#include <memory>

namespace Game2048 {

class TileView final : public PandaUI::Panel {
public:
    TileView();

    void setTile(int value, float cellSize, float scale = 1.f);
    void setVisualFrame(PandaUI::Point origin, float cellSize, float scale = 1.f);

private:
    std::shared_ptr<PandaUI::Label> m_label;
    int m_value = 0;
};

} // namespace Game2048
