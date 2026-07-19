#include "DomesPanel.hpp"

DomesPanel::DomesPanel(const ArchUIContext &) {
    addSubview(ArchUI::noteLabel("Domes over square and round plans — in development."));
    ArchUI::addPlannedSection(
        *this, {
                   "Form: hemisphere / onion / tented",
                   "Drum",
                   "Lucarnes",
                   "Spire",
               }
    );
}
