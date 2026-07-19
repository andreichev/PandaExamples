#include "MaterialsPanel.hpp"
#include "../Model/BlockPalette.hpp"

MaterialsPanel::MaterialsPanel(const ArchUIContext &context) {
    addSubview(ArchUI::noteLabel(
        "Material is independent from shape: pick a material here, pick a form in its own section."
    ));

    std::vector<MaterialGridView::Entry> entries;
    for (const BlockPalette::BlockEntry &entry : BlockPalette::BUILDING_BLOCKS) {
        entries.push_back({entry.type, entry.name});
    }
    m_grid = std::make_shared<MaterialGridView>(
        context, std::move(entries), ArchObjectType::Block, false
    );
    m_grid->layout().setFlexGrow(1.f);
    addSubview(m_grid);

    ArchUI::addPlannedSection(
        *this, {
                   "Tint",
                   "Scale, rotation, offset",
                   "Random offset per block",
                   "Dirt",
                   "Aging",
                   "Moss",
                   "Moisture",
                   "Damage",
               }
    );
}

void MaterialsPanel::refresh() {
    if (m_grid) { m_grid->syncFromState(); }
}
