#include "BlocksPanel.hpp"
#include "../Model/BlockPalette.hpp"

BlocksPanel::BlocksPanel(const ArchUIContext &context) {
    addSubview(ArchUI::noteLabel("Cubes 1 x 1: pick a material, drag to lay lines."));
    constexpr size_t CARDS_PER_ROW = 4;
    std::shared_ptr<PandaUI::Panel> row;
    for (size_t i = 0; i < BlockPalette::BUILDING_BLOCKS.size(); ++i) {
        if (i % CARDS_PER_ROW == 0) {
            row = ArchUI::cardsRow();
            addSubview(row);
        }
        const BlockPalette::BlockEntry &entry = BlockPalette::BUILDING_BLOCKS[i];
        row->addSubview(ArchUI::makeMaterialCard(context, entry.type, entry.name));
    }
}
