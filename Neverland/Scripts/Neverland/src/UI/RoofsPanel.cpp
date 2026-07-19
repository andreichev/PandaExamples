#include "RoofsPanel.hpp"

RoofsPanel::RoofsPanel(const ArchUIContext &context) {
    auto row = ArchUI::cardsRow();
    row->addSubview(ArchUI::makeElementCard(context, ArchObjectType::Roof, "Roof", "area"));
    addSubview(row);
    addSubview(ArchUI::noteLabel(
        "Paint an area with roof cells: the ridge follows the long side, gables use the wall material."
    ));

    const auto addRow = [this, &context](
                            const char *title, int param, std::vector<PresetRow::Option> options
                        ) {
        auto presets = std::make_shared<PresetRow>(
            context, ArchObjectType::Roof, title, param, std::move(options)
        );
        m_rows.push_back(presets);
        addSubview(presets);
    };
    addRow("Form", 0, {{"Gable", 0}, {"Flat", 1}});
    addRow("Slope", 1, {{"22°", 1}, {"35°", 0}, {"45°", 2}});
    addRow("Tiles", 2, {{"Auto", 0}, {"Red", 1}, {"Brown", 2}, {"Grey", 3}, {"Dark", 4}});
    addRow("Overhang", 3, {{"None", 1}, {"0.25", 0}, {"0.5", 2}});

    ArchUI::addPlannedSection(
        *this, {
                   "Materials: metal, slate",
                   "Ridge finish",
                   "Dormer windows",
                   "Chimneys",
                   "Gutters and downpipes",
               }
    );
}

void RoofsPanel::refresh() {
    for (auto &row : m_rows) { row->syncFromState(); }
}
