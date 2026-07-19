#include "CornicesPanel.hpp"

CornicesPanel::CornicesPanel(const ArchUIContext &context) {
    auto row = ArchUI::cardsRow();
    row->addSubview(ArchUI::makeElementCard(context, ArchObjectType::Cornice, "Cornice", "1 x 1"));
    addSubview(row);
    addSubview(ArchUI::noteLabel(
        "Placed on top of a wall. Neighbours merge into a continuous profile, corners are mitred "
        "automatically; a closed loop wraps the whole building."
    ));

    ArchUI::addPlannedSection(
        *this, {
                   "Profile presets",
                   "Bands (multi-tier)",
                   "Modillions",
                   "Dentils",
                   "Balustrade on top",
               }
    );
}
