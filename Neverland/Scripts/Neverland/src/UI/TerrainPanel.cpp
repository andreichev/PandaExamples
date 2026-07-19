#include "TerrainPanel.hpp"
#include "../Model/BlockPalette.hpp"

TerrainPanel::TerrainPanel(const ArchUIContext &context) {
    addSubview(ArchUI::sectionLabel("Sculpting materials"));
    auto row = ArchUI::cardsRow();
    for (const BlockPalette::BlockEntry &entry : BlockPalette::TERRAIN_MATERIALS) {
        auto card = std::make_shared<BlockCardButton>(
            entry.name, context.previewFor(entry.type), nullptr
        );
        // Выбор материала лепки выходит из режима постройки (форма сбрасывается на куб).
        card->setOnClick([context, type = entry.type](PandaUI::Button &) {
            if (!context.blocks) { return; }
            context.blocks->setSelectedElement(ArchObjectType::Block);
            context.blocks->setSelectedBlock(type);
        });
        if (context.blockCards != nullptr) { context.blockCards->emplace_back(entry.type, card); }
        row->addSubview(card);
    }
    addSubview(row);

    addSubview(ArchUI::noteLabel(
        "Terrain is sculpted, not built: brush tool lives in the bottom-left panel. Keys: [ ] — "
        "size, B — brush mode, Alt — cursor."
    ));
}
