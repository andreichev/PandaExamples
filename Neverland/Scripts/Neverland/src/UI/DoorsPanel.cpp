#include "DoorsPanel.hpp"

DoorsPanel::DoorsPanel(const ArchUIContext &context) {
    auto row = ArchUI::cardsRow();
    row->addSubview(ArchUI::makeElementCard(context, ArchObjectType::Door, "Doorway", "1 x 3"));
    addSubview(row);
    addSubview(ArchUI::noteLabel(
        "Open passage in a wall run, up to the lintel. Lower cells are walkable."
    ));

    auto width = std::make_shared<PresetRow>(
        context, ArchObjectType::Door, "Opening", 0,
        std::vector<PresetRow::Option>{{"Narrow", 1}, {"Standard", 0}, {"Portal", 2}}
    );
    m_rows.push_back(width);
    addSubview(width);

    ArchUI::addPlannedSection(
        *this, {
                   "Door leaf: panel / glazed / arched",
                   "Transom window",
                   "Portal trim",
                   "Canopy",
                   "Entrance steps",
               }
    );
}

void DoorsPanel::refresh() {
    for (auto &row : m_rows) { row->syncFromState(); }
}
