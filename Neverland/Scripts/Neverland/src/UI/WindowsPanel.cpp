#include "WindowsPanel.hpp"

WindowsPanel::WindowsPanel(const ArchUIContext &context) {
    auto row = ArchUI::cardsRow();
    row->addSubview(ArchUI::makeElementCard(context, ArchObjectType::Window, "Window", "1 x 3"));
    addSubview(row);
    addSubview(ArchUI::noteLabel(
        "Wall-run module: sill wall, glazed opening, lintel. Material follows the wall."
    ));

    const auto addRow = [this, &context](
                            const char *title, int param, std::vector<PresetRow::Option> options
                        ) {
        auto presets = std::make_shared<PresetRow>(
            context, ArchObjectType::Window, title, param, std::move(options)
        );
        m_rows.push_back(presets);
        addSubview(presets);
    };
    addRow("Opening", 0, {{"Narrow", 1}, {"Standard", 0}, {"Wide", 2}});
    addRow("Sill", 1, {{"0.6", 1}, {"0.9", 0}, {"1.2", 2}});
    addRow("Frame", 2, {{"Cross", 0}, {"None", 1}, {"Bars", 2}});
    // Наличник вокруг проёма; Pediment добавляет сандрик-полочку сверху.
    addRow("Trim", 3, {{"Casing", 0}, {"None", 1}, {"Pediment", 2}});

    ArchUI::addPlannedSection(
        *this, {
                   "Shape: rectangular / arched",
                   "Casements and transom",
                   "Brackets",
                   "Shutters",
                   "Frame material and color",
                   "Glass",
               }
    );
}

void WindowsPanel::refresh() {
    for (auto &row : m_rows) { row->syncFromState(); }
}
