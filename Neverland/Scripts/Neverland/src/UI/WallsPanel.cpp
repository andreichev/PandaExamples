#include "WallsPanel.hpp"
#include "../Model/BlockPalette.hpp"

WallsPanel::WallsPanel(const ArchUIContext &context) {
    addSubview(ArchUI::noteLabel(
        "Wall panel 1 x 3. Drag to build runs; corners and junctions join automatically."
    ));

    addSubview(ArchUI::sectionLabel("Type / material"));
    std::vector<MaterialGridView::Entry> entries;
    for (const BlockPalette::BlockEntry &entry : BlockPalette::BUILDING_BLOCKS) {
        entries.push_back({entry.type, entry.name});
    }
    m_grid = std::make_shared<MaterialGridView>(
        context, std::move(entries), ArchObjectType::Wall, true
    );
    m_grid->layout().setFlexGrow(1.f);
    addSubview(m_grid);

    auto thickness = std::make_shared<PresetRow>(
        context, ArchObjectType::Wall, "Thickness", 0,
        std::vector<PresetRow::Option>{{"0.2", 1}, {"0.3", 0}, {"0.5", 2}}
    );
    m_rows.push_back(thickness);
    addSubview(thickness);

    ArchUI::addPlannedSection(
        *this, {
                   "Plinth course",
                   "Rustication",
                   "String courses",
                   "Combined type: stone base + plaster top",
               }
    );
}

void WallsPanel::refresh() {
    if (m_grid) { m_grid->syncFromState(); }
    for (auto &row : m_rows) { row->syncFromState(); }
}
